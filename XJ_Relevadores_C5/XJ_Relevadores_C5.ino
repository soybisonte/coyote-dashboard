/*
  COYOTE: ESP32-C5_Relay_X4, flash 4 MB. MAC en Configuracion_Radio.h.
  ESP32C5 Dev Module, core 3.3.10, USB CDC On Boot Enabled.
  GPIO 7/8/9/10 recuperados del firmware original. Arranque LOW como fabrica.
  Primera prueba sin cargas: comprobar polaridad y orden fisico de las bornas.
  Ver Configuracion_Placa.h para bloquear todas las salidas en diagnostico.
*/
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_random.h>
#include <esp_system.h>
#include <driver/gpio.h>
#include <freertos/queue.h>
#include "Relevadores_Protocolo.h"
#include "Configuracion_Placa.h"
#include "Registro_NoBloqueante.h"

QueueHandle_t ordenes = nullptr;
uint32_t sesion = 0;
bool radioLista = false, salidasListas = false;
uint32_t recibidas = 0;
uint32_t erroresEnvio = 0, maxPausaLoopMs = 0;

bool mapaValido() {
  if (!configuracionHabilitada) return false;
  for (uint8_t i = 0; i < Relevadores::cantidad; ++i) {
    const int pin = pinesRelevador[i];
    // Conservar USB nativo 13/14, UART 11/12 y SPI flash 16..22.
    if (!GPIO_IS_VALID_OUTPUT_GPIO(pin) || (pin >= 11 && pin <= 14) ||
        (pin >= 16 && pin <= 22)) return false;
    for (uint8_t j = 0; j < i; ++j) if (pin == pinesRelevador[j]) return false;
  }
  return true;
}

uint8_t leerEstados() {
  uint8_t estados = 0;
  if (salidasListas)
    for (uint8_t i = 0; i < Relevadores::cantidad; ++i)
      if (bool(gpio_get_level(static_cast<gpio_num_t>(pinesRelevador[i]))) ==
          activoEnAlto[i]) estados |= 1U << i;
  return estados;
}

void publicar(Relevadores::Tipo tipo, uint32_t id = 0, uint8_t canal = 255) {
  const uint8_t estados = leerEstados();
  const Relevadores::Paquete p{Relevadores::firma, sesion, id, Relevadores::version,
      tipo, canal, static_cast<uint8_t>(canal < 4 && (estados & (1U << canal))),
      estados, static_cast<uint8_t>(salidasListas ? Relevadores::listo : 0), 0};
  if (esp_now_send(Relevadores::macTft, reinterpret_cast<const uint8_t*>(&p), sizeof(p)) != ESP_OK)
    ++erroresEnvio;
}

void recibir(const esp_now_recv_info_t *info, const uint8_t *datos, int longitud) {
  if (!info || longitud != sizeof(Relevadores::Paquete) ||
      memcmp(info->src_addr, Relevadores::macTft, 6)) return;
  Relevadores::Paquete p;
  memcpy(&p, datos, sizeof(p));
  if (Relevadores::valido(p) && p.tipo == Relevadores::Tipo::Orden)
    xQueueSend(ordenes, &p, 0);
}

void setup() {
  Serial.begin(115200);
  // HWCDC del core 3.3.10 puede esperar 20 x 100 ms por escritura si el
  // host deja de leer. El control debe funcionar sin monitor conectado.
  Serial.setTxTimeoutMs(0);
  if (!WiFi.mode(WIFI_STA) || !WiFi.setSleep(false)) return;
  uint8_t mac[6];
  if (esp_wifi_get_mac(WIFI_IF_STA, mac) != ESP_OK ||
      memcmp(mac, Relevadores::macReceptor, 6)) {
    Registro::escribir(Serial, "ERROR: la MAC no coincide con el C5 configurado; salidas bloqueadas.\n");
    return;
  }
  salidasListas = mapaValido();
  if (salidasListas) {
    for (uint8_t i = 0; i < Relevadores::cantidad; ++i) {
      // Precargar OFF antes de habilitar la salida. Sin restauracion desde flash.
      gpio_set_level(static_cast<gpio_num_t>(pinesRelevador[i]), !activoEnAlto[i]);
      gpio_set_direction(static_cast<gpio_num_t>(pinesRelevador[i]), GPIO_MODE_INPUT_OUTPUT);
    }
  }
  do { sesion = esp_random(); } while (!sesion);
  ordenes = xQueueCreate(8, sizeof(Relevadores::Paquete));
  if (!ordenes || esp_wifi_set_band_mode(WIFI_BAND_MODE_2G_ONLY) != ESP_OK ||
      esp_wifi_set_channel(Relevadores::canalRadio, WIFI_SECOND_CHAN_NONE) != ESP_OK ||
      esp_now_init() != ESP_OK) {
    Registro::escribir(Serial, "ERROR iniciando ESP-NOW 2.4 GHz canal 1.\n");
    return;
  }
  esp_now_peer_info_t peer{};
  memcpy(peer.peer_addr, Relevadores::macTft, 6);
  peer.channel = Relevadores::canalRadio;
  peer.ifidx = WIFI_IF_STA;
  radioLista = esp_now_add_peer(&peer) == ESP_OK && esp_now_register_recv_cb(recibir) == ESP_OK;
  Registro::escribir(Serial, "COYOTE C5 MAC %s | radio %s | sesion %lu | salidas %s | reset=%d\n",
      WiFi.macAddress().c_str(), radioLista ? "OK" : "ERROR",
      static_cast<unsigned long>(sesion), salidasListas ? "HABILITADAS / OFF" : "BLOQUEADAS",
      static_cast<int>(esp_reset_reason()));
}

void loop() {
  static uint32_t vueltaAnterior = 0;
  const uint32_t ahora = millis();
  if (vueltaAnterior && ahora - vueltaAnterior > maxPausaLoopMs)
    maxPausaLoopMs = ahora - vueltaAnterior;
  vueltaAnterior = ahora;
  if (radioLista) {
    Relevadores::Paquete p;
    while (xQueueReceive(ordenes, &p, 0) == pdTRUE) {
      ++recibidas;
      if (!salidasListas || p.sesion != sesion) {
        publicar(Relevadores::Tipo::Estado);
        Registro::escribir(Serial, "Orden rechazada: salidas bloqueadas o sesion anterior.\n");
        continue;
      }
      // ON/OFF explicitos: los reintentos nunca alternan el relevador.
      gpio_set_level(static_cast<gpio_num_t>(pinesRelevador[p.canal]),
          p.encendido ? activoEnAlto[p.canal] : !activoEnAlto[p.canal]);
      publicar(Relevadores::Tipo::Confirmacion, p.id, p.canal);
      Registro::escribir(Serial, "R%u orden=%lu %s GPIO=%d\n", p.canal + 1,
          static_cast<unsigned long>(p.id), p.encendido ? "ON" : "OFF",
          pinesRelevador[p.canal]);
    }
    static uint32_t ultimoEstado = 0;
    if (millis() - ultimoEstado >= 500) {
      ultimoEstado = millis();
      publicar(Relevadores::Tipo::Estado);
    }
    static uint32_t ultimoLog = 0;
    if (millis() - ultimoLog >= 3000) {
      ultimoLog = millis();
      Registro::escribir(Serial, "COYOTE C5 radio OK | salidas %s | GPIO mask=%02X | ordenes=%lu | maxLoop=%lu ms | txError=%lu\n",
          salidasListas ? "LISTAS" : "BLOQUEADAS", leerEstados(),
          static_cast<unsigned long>(recibidas), static_cast<unsigned long>(maxPausaLoopMs),
          static_cast<unsigned long>(erroresEnvio));
    }
  }
  delay(5);
}
