// Pruebas de protocolo y logica real del mando, sin radio ni salidas fisicas.
// c++ -std=c++17 tests/relevadores_test.cpp -o /tmp/xj-reles-test && /tmp/xj-reles-test
#include <cassert>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <string>
#include <vector>

struct Queue { size_t size; std::deque<std::vector<uint8_t>> items; };
using QueueHandle_t = Queue*;
constexpr bool pdTRUE = true;
QueueHandle_t xQueueCreate(int, size_t size) { return new Queue{size, {}}; }
bool xQueueSend(QueueHandle_t q, const void *data, int) {
  const auto *bytes = static_cast<const uint8_t*>(data);
  q->items.emplace_back(bytes, bytes + q->size);
  return true;
}
bool xQueueReceive(QueueHandle_t q, void *data, int) {
  if (q->items.empty()) return false;
  memcpy(data, q->items.front().data(), q->size);
  q->items.pop_front();
  return true;
}
uint32_t clockMs = 100;
uint32_t millis() { return clockMs; }
struct esp_now_peer_info_t { uint8_t peer_addr[6]; int ifidx; uint8_t channel; };
using esp_err_t = int;
constexpr int WIFI_IF_STA = 0, ESP_OK = 0;
int sends = 0;
std::vector<uint8_t> lastSent;
int esp_now_add_peer(esp_now_peer_info_t*) { return ESP_OK; }
int esp_now_send(const uint8_t*, const uint8_t *data, size_t size) {
  ++sends;
  lastSent.assign(data, data + size);
  return ESP_OK;
}
const char *esp_err_to_name(int) { return "ESP_OK"; }
struct FakeSerial {
  void printf(const char*, ...) {}
  void println(const char*) {}
} Serial;
struct FakeScreen {
  std::string text;
  void fillScreen(uint16_t) { text.clear(); }
  void fillRect(int, int, int, int, uint16_t) {}
  void drawRect(int, int, int, int, uint16_t) {}
  void drawFastHLine(int, int, int, uint16_t) {}
  void setTextSize(int) {}
  void setTextColor(uint16_t) {}
  void setCursor(int, int) {}
  void print(const char *s) { text += s; text += '\n'; }
  void printf(const char *fmt, ...) {
    char buffer[64]; va_list args; va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args); va_end(args); print(buffer);
  }
} screen;
bool radioReady = true;
uint32_t nextId = 100;
constexpr uint16_t background = 0;
#include "../XJ_Mando_TFT/Relevadores_TFT.h"

void deliver(const Relevadores::Paquete &p) {
  RelayUi::receive(reinterpret_cast<const uint8_t*>(&p), sizeof(p));
  RelayUi::service();
}
Relevadores::Paquete status(uint32_t session, bool ready, uint8_t states = 0) {
  return {Relevadores::firma, session, 0, Relevadores::version,
      Relevadores::Tipo::Estado, 255, 0, states, uint8_t(ready), 0};
}
int main() {
  using namespace Relevadores;
  assert(RelayUi::begin());
  auto p = status(123, false);
  assert(valido(p));
  auto bad = p; bad.version++; assert(!valido(bad));
  bad = p; bad.firma++; assert(!valido(bad));
  bad = p; bad.sesion = 0; assert(!valido(bad));
  bad = p; bad.estados = 16; assert(!valido(bad));
  bad = p; bad.reservado = 1; assert(!valido(bad));
  bad = p; bad.flags = 2; assert(!valido(bad));
  bad = p; bad.canal = 4; assert(!valido(bad));

  RelayUi::touch(12, 105); assert(sends == 0);  // Sin telemetria, no adivinar estado.
  deliver(p); RelayUi::touch(12, 105); assert(sends == 0);  // GPIO sin verificar.
  RelayUi::draw(); assert(screen.text.find("POR CONFIGURAR") != std::string::npos);
  deliver(status(123, true));
  RelayUi::touch(80, 105); assert(sends == 0);  // Separacion entre botones.
  RelayUi::touch(12, 104); assert(sends == 0); // Fuera de la zona vertical.

  for (uint8_t channel = 0; channel < 4; ++channel) {
    for (bool on : {true, false}) {
      const int before = sends;
      RelayUi::touch(8 + (channel % 2) * 160 + (on ? 5 : 77),
          58 + (channel / 2) * 88 + 48);
      assert(sends == before + 1 && RelayUi::attempts == 1);
      const auto order = RelayUi::pending;
      assert(valido(order) && order.canal == channel && bool(order.encendido) == on);
      RelayUi::touch(12, 105); assert(sends == before + 1); // Una orden en vuelo.
      auto ack = order; ack.tipo = Tipo::Confirmacion; ack.flags = listo;
      ack.estados = on ? (1U << channel) : 0;
      bad = ack; bad.id++; deliver(bad); assert(RelayUi::attempts);
      bad = ack; bad.sesion++; deliver(bad); assert(RelayUi::attempts);
      bad = ack; bad.estados ^= (1U << channel); deliver(bad); assert(RelayUi::attempts);
      deliver(ack); assert(!RelayUi::attempts);
      assert(bool(RelayUi::state.estados & (1U << channel)) == on);
    }
  }
  RelayUi::touch(12, 105);
  const auto firstBytes = lastSent;
  for (int retry = 0; retry < 2; ++retry) {
    clockMs += 200; RelayUi::service(); assert(lastSent == firstBytes);
  }
  clockMs += 200; RelayUi::service();
  assert(!RelayUi::attempts && (RelayUi::failedMask & 1));
  RelayUi::draw(); assert(screen.text.find("SIN CONFIRMACION") != std::string::npos);
  clockMs += 3000; RelayUi::service();
  const int beforeOffline = sends;
  RelayUi::touch(12, 105); assert(sends == beforeOffline);
  RelayUi::draw(); assert(screen.text.find("SIN SENAL") != std::string::npos);
  deliver(status(123, true)); RelayUi::touch(12, 105);
  const auto oldOrder = RelayUi::pending;
  deliver(status(456, true)); assert(!RelayUi::attempts); // Reinicio cancela reintentos.
  clockMs += 200; RelayUi::service();
  auto late = oldOrder; late.tipo = Tipo::Confirmacion; late.flags = listo; late.estados = 1;
  deliver(late); assert(RelayUi::state.sesion == 456);
  RelayUi::touch(12, 105); assert(RelayUi::pending.sesion == 456);
  deliver(status(456, false)); assert(!RelayUi::attempts && !RelayUi::ready());
  puts("OK: protocolo, cuatro canales ON/OFF, bloqueos, ACK, reintentos, perdida y reinicio.");
  delete RelayUi::queue;
}
