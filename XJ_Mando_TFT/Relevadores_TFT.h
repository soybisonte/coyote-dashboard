#pragma once
#include "Relevadores_Protocolo.h"
#include "Interfaz_OLED.h"

// Incluir despues de los objetos y colores de la TFT.
namespace RelayUi {
struct Event { Relevadores::Paquete packet; uint32_t receivedAt; };
QueueHandle_t queue = nullptr;
Relevadores::Paquete state{}, pending{};
uint32_t seen = 0, lastAttempt = 0;
uint32_t receivedCount = 0;
uint8_t attempts = 0, failedMask = 0;
bool known = false, dirty = true;
constexpr uint32_t timeoutMs = 3000;

bool begin() {
  queue = xQueueCreate(8, sizeof(Event));
  if (!queue) return false;
  esp_now_peer_info_t peer{};
  memcpy(peer.peer_addr, Relevadores::macReceptor, 6);
  peer.ifidx = WIFI_IF_STA;
  peer.channel = Relevadores::canalRadio;
  return esp_now_add_peer(&peer) == ESP_OK;
}

void receive(const uint8_t *data, int length) {
  if (!queue || length != sizeof(Relevadores::Paquete)) return;
  Event event{};
  memcpy(&event.packet, data, sizeof(event.packet));
  if (!Relevadores::valido(event.packet) ||
      event.packet.tipo == Relevadores::Tipo::Orden) return;
  event.receivedAt = millis();
  xQueueSend(queue, &event, 0);
}

bool fresh() { return known && millis() - seen < timeoutMs; }
bool ready() { return fresh() && (state.flags & Relevadores::listo); }

void sendPending() {
  ++attempts;
  lastAttempt = millis();
  const esp_err_t result = esp_now_send(Relevadores::macReceptor,
      reinterpret_cast<const uint8_t*>(&pending), sizeof(pending));
  Serial.printf("R%u orden=%lu %s intento=%u envio=%s\n", pending.canal + 1,
      static_cast<unsigned long>(pending.id), pending.encendido ? "ON" : "OFF",
      attempts, esp_err_to_name(result));
}

void service() {
  Event event;
  while (xQueueReceive(queue, &event, 0) == pdTRUE) {
    const auto &p = event.packet;
    if (p.tipo == Relevadores::Tipo::Confirmacion &&
        (!attempts || !Relevadores::coincide(p, pending))) continue;
    ++receivedCount;
    if (!known || state.sesion != p.sesion || state.flags != p.flags ||
        state.estados != p.estados)
      Serial.printf("C5 RX sesion=%lu salidas=%s mascaraGPIO=%02X\n",
          static_cast<unsigned long>(p.sesion),
          (p.flags & Relevadores::listo) ? "LISTAS" : "BLOQUEADAS", p.estados);
    if (known && state.sesion != p.sesion) {
      attempts = 0;  // No repetir una orden tras reiniciar el receptor.
      failedMask = 0;
    }
    if (attempts && p.tipo == Relevadores::Tipo::Confirmacion) {
      attempts = 0;
      failedMask &= ~(1U << p.canal);
      Serial.printf("R%u confirmado %s (GPIO)\n", p.canal + 1, p.encendido ? "ON" : "OFF");
    }
    if (attempts && !(p.flags & Relevadores::listo)) {
      failedMask |= 1U << pending.canal;
      attempts = 0;
    }
    state = p;
    known = true;
    seen = event.receivedAt;
  }
  if (attempts && millis() - lastAttempt >= 200) {
    if (attempts < 3 && ready()) sendPending();
    else {
      failedMask |= 1U << pending.canal;
      attempts = 0;
      Serial.println("Relevador sin confirmacion; no se asume el cambio.");
    }
  }
}

void touch(int x, int y) {
  if (!radioReady || !ready() || attempts) return;
  for (uint8_t i = 0; i < 4; ++i) {
    const int left = 8 + (i % 2) * 160, top = 58 + (i / 2) * 88;
    if (y < top + 47 || y >= top + 79) continue;
    const bool on = x >= left + 4 && x < left + 68;
    const bool off = x >= left + 76 && x < left + 140;
    if (!on && !off) continue;
    pending = {Relevadores::firma, state.sesion, ++nextId, Relevadores::version,
        Relevadores::Tipo::Orden, i, static_cast<uint8_t>(on), 0, 0, 0};
    failedMask &= ~(1U << i);
    sendPending();
    return;
  }
}

void navigation(const char *label) {
  OledUi::navegacion(screen, label);
}

void draw() {
  static uint32_t previousKey = UINT32_MAX;
  const bool online = fresh(), enabled = ready();
  const uint32_t key = online | (enabled << 1) | (state.estados << 2) |
      (failedMask << 6) | (uint32_t(attempts != 0) << 10) |
      (uint32_t(pending.canal) << 11) | (uint32_t(radioReady) << 13);
  if (!dirty && key == previousKey) return;
  dirty = false;
  previousKey = key;
  screen.fillScreen(background);
  OledUi::cabecera(screen, "COYOTE / RELEVADORES", "VOLVER");
  OledUi::texto(screen, 8, 34, !radioReady ? "ERROR RADIO" : !online ? "C5 SIN SENAL" :
      !enabled ? "C5 OK / GPIO POR VERIFICAR" : "C5 EN LINEA / 4 CANALES");
  for (uint8_t i = 0; i < 4; ++i) {
    const bool waiting = attempts && pending.canal == i;
    const bool failed = failedMask & (1U << i), on = state.estados & (1U << i);
    OledUi::rele(screen, i, !online ? "SIN SENAL" : !enabled ? "POR CONFIGURAR" : waiting ?
        "CONFIRMANDO..." : failed ? "SIN CONFIRMACION" : on ? "ENCENDIDO" : "APAGADO",
        enabled && !waiting && !failed, on);
  }
}
}  // namespace RelayUi
