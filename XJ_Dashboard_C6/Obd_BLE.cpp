#include "Obd_BLE.h"
#include "Configuracion_Radio.h"

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEClient.h>
#include <BLERemoteService.h>
#include <BLERemoteCharacteristic.h>
#include <freertos/stream_buffer.h>
#include <atomic>
#include <ctype.h>
#include <math.h>

namespace Obd {
namespace {

constexpr auto &veepeakAddress = ConfiguracionRadio::macObdBle;
constexpr uint32_t retryMs = 5000;
constexpr size_t replyCapacity = 513;

State state{};
portMUX_TYPE stateLock = portMUX_INITIALIZER_UNLOCKED;
StreamBufferHandle_t receiveBuffer = nullptr;
std::atomic<bool> overflow{false};

void setLink(Link link, bool fuelUnsupported = false) {
  portENTER_CRITICAL(&stateLock);
  if (link != Link::Online) state = {};
  state.link = link;
  state.fuelUnsupported = fuelUnsupported;
  portEXIT_CRITICAL(&stateLock);
}

void store(Gauge gauge, float value, bool valid) {
  const Reading reading{value, millis(), valid};
  portENTER_CRITICAL(&stateLock);
  state.readings[gauge] = reading;
  portEXIT_CRITICAL(&stateLock);
}

void onReply(BLERemoteCharacteristic *, uint8_t *data, size_t size, bool) {
  if (xStreamBufferSend(receiveBuffer, data, size, 0) != size) overflow = true;
}

bool findVeepeak(BLEScan &scan, BLEAdvertisedDevice &adapter) {
  BLEScanResults *results = scan.start(4, false);
  int bestMatch = 0;
  bool ambiguous = false;
  if (results) {
    for (int i = 0; i < results->getCount(); ++i) {
      BLEAdvertisedDevice device = results->getDevice(i);
      const bool knownAddress = device.getAddress().toString().equalsIgnoreCase(veepeakAddress);
      const bool knownName = device.getName().equalsIgnoreCase("VEEPEAK");
      const int match = knownAddress ? 2 : knownName ? 1 : 0;
      if (match > bestMatch) {
        adapter = device;
        bestMatch = match;
        ambiguous = false;
      } else if (match && match == bestMatch) {
        ambiguous = true;
      }
    }
  }
  scan.clearResults();
  return bestMatch && !ambiguous;
}

bool writable(BLERemoteCharacteristic *characteristic) {
  return characteristic && (characteristic->canWrite() || characteristic->canWriteNoResponse());
}

bool readableByNotification(BLERemoteCharacteristic *characteristic) {
  return characteristic && (characteristic->canNotify() || characteristic->canIndicate());
}

BLERemoteCharacteristic *subscribeToElm(BLEClient &client) {
  // UUIDs de transporte serie BLE. Confirmar cada par por sus propiedades GATT.
  struct Profile { const char *service; const char *rx; const char *tx; };
  constexpr Profile profiles[] = {
    {"fff0", "fff1", "fff2"},
    {"ffe0", "ffe1", "ffe1"},
    {"6e400001-b5a3-f393-e0a9-e50e24dcca9e",
     "6e400003-b5a3-f393-e0a9-e50e24dcca9e",
     "6e400002-b5a3-f393-e0a9-e50e24dcca9e"},
  };
  for (const Profile &profile : profiles) {
    BLERemoteService *service = client.getService(BLEUUID(profile.service));
    if (!service) continue;
    BLERemoteCharacteristic *rx = service->getCharacteristic(BLEUUID(profile.rx));
    BLERemoteCharacteristic *tx = service->getCharacteristic(BLEUUID(profile.tx));
    if (readableByNotification(tx) && writable(rx)) {
      BLERemoteCharacteristic *swap = rx;
      rx = tx;
      tx = swap;
    }
    if (readableByNotification(rx) && writable(tx)) {
      rx->registerForNotify(onReply, rx->canNotify());
      Serial.printf("OBD BLE GATT %s | RX %s TX %s\n", profile.service,
                    rx->getUUID().toString().c_str(), tx->getUUID().toString().c_str());
      return tx;
    }
  }
  const auto *services = client.getServices();
  if (services) {
    for (const auto &entry : *services) Serial.printf("OBD BLE servicio %s\n", entry.first.c_str());
  }
  return nullptr;
}

// Un comando a la vez. El prompt '>' delimita la respuesta aunque BLE la fragmente.
class ElmSession {
public:
  ElmSession(BLEClient &client, BLERemoteCharacteristic &tx)
    : client(client), tx(tx) {}

  bool healthy() { return synchronized && client.isConnected(); }
  const char *reply() const { return response; }

  bool command(const char *text, uint32_t timeoutMs = 4000) {
    if (!healthy()) return false;
    response[0] = '\0';
    uint8_t byte;
    while (xStreamBufferReceive(receiveBuffer, &byte, 1, 0)) {}
    overflow = false;

    char request[12];
    const int length = snprintf(request, sizeof(request), "%s\r", text);
    if (length <= 0 || length >= static_cast<int>(sizeof(request)) ||
        !tx.writeValue(reinterpret_cast<uint8_t *>(request), length, !tx.canWriteNoResponse())) {
      synchronized = false;
      return false;
    }

    const uint32_t sent = millis();
    size_t used = 0;
    while (client.isConnected() && !overflow && millis() - sent < timeoutMs) {
      if (!xStreamBufferReceive(receiveBuffer, &byte, 1, pdMS_TO_TICKS(25))) continue;
      if (byte == '>') {
        response[used] = '\0';
        if (!overflow) return true;
        break;
      }
      if (used == sizeof(response) - 1) break;
      response[used++] = static_cast<char>(toupper(byte));
    }
    response[used] = '\0';
    synchronized = false;
    Serial.printf("OBD respuesta incompleta a %s\n", text);
    return false;
  }

private:
  BLEClient &client;
  BLERemoteCharacteristic &tx;
  bool synchronized = true;
  char response[replyCapacity]{};
};

bool hexReply(const char *response, uint8_t pid, unsigned bytes, uint32_t &value) {
  char prefix[5];
  snprintf(prefix, sizeof(prefix), "41%02X", pid);
  for (const char *p = response; *p;) {
    char line[13] = {};
    unsigned length = 0;
    while (*p && *p != '\r' && *p != '\n') {
      if (*p != ' ' && *p != '\t' && length < sizeof(line) - 1) line[length++] = *p;
      ++p;
    }
    if (*p) ++p;
    const unsigned end = 4 + bytes * 2;
    if (length < end || strncmp(line, prefix, 4) != 0) continue;
    uint32_t parsed = 0;
    for (unsigned i = 4; i < end; ++i) {
      if (!isxdigit(static_cast<unsigned char>(line[i]))) return false;
      parsed = parsed * 16 + (line[i] <= '9' ? line[i] - '0' : line[i] - 'A' + 10);
    }
    value = parsed;
    return true;
  }
  return false;
}

bool supports(uint32_t mask, unsigned page, unsigned pid) {
  return (mask & (1UL << (32 + page - pid))) != 0;
}

void readPid(ElmSession &elm, Gauge gauge, uint8_t pid, unsigned bytes,
             float scale, float offset = 0) {
  char request[5];
  snprintf(request, sizeof(request), "01%02X", pid);
  uint32_t raw = 0;
  const bool valid = elm.command(request) && hexReply(elm.reply(), pid, bytes, raw);
  store(gauge, raw * scale + offset, valid);
}

void readVoltage(ElmSession &elm) {
  float value = 0;
  bool valid = false;
  if (elm.command("ATRV")) {
    char *end;
    const char *reply = elm.reply();
    value = strtof(reply, &end);
    if (end != reply && *end == 'V') {
      do { ++end; } while (isspace(static_cast<unsigned char>(*end)));
      valid = !*end && isfinite(value) && value >= 0 && value <= 32;
    }
  }
  store(Voltage, value, valid);
}

void pollVehicle(ElmSession &elm) {
  setLink(Link::WaitingForEcu);
  if (!elm.command("ATI") || !strstr(elm.reply(), "ELM")) {
    Serial.println("OBD BLE: sin identificacion ELM327.");
    return;
  }
  for (const char *command : {"ATE0", "ATL0", "ATS0", "ATH0", "ATSP0"}) {
    if (!elm.command(command) || !strstr(elm.reply(), "OK")) return;
  }
  uint32_t mask = 0;
  if (!elm.command("0100", 30000) || !hexReply(elm.reply(), 0, 4, mask)) {
    Serial.println("OBD BLE: el adaptador responde, la ECU no envio su mapa.");
    return;
  }

  uint32_t fuelPage = 0;
  const bool fuelKnown = !supports(mask, 0, 0x20) ||
                         (elm.command("0120") && hexReply(elm.reply(), 0x20, 4, fuelPage));
  const bool fuelSupported = fuelKnown && supports(fuelPage, 0x20, 0x2F);
  if (!elm.healthy()) return;
  Serial.printf("OBD ECU mapa %08lX | gasolina %s\n", static_cast<unsigned long>(mask),
                fuelSupported ? "PID 2F" : fuelKnown ? "no disponible" : "sin confirmar");
  setLink(Link::Online, fuelKnown && !fuelSupported);

  uint32_t lastTemp = millis() - 1000;
  uint32_t lastVolt = millis() - 2000;
  uint32_t lastFuel = lastVolt;
  while (elm.healthy()) {
    if (supports(mask, 0, 0x0C)) readPid(elm, Rpm, 0x0C, 2, 0.25f);
    const uint32_t now = millis();
    if (elm.healthy() && supports(mask, 0, 0x05) && now - lastTemp >= 1000) {
      lastTemp = now;
      readPid(elm, Coolant, 0x05, 1, 1.0f, -40.0f);
    }
    if (elm.healthy() && now - lastVolt >= 2000) {
      lastVolt = now;
      readVoltage(elm);
    }
    if (elm.healthy() && fuelSupported && now - lastFuel >= 2000) {
      lastFuel = now;
      readPid(elm, Fuel, 0x2F, 1, 100.0f / 255.0f);
    }
    delay(100);
  }
}

void obdTask(void *) {
  if (!BLEDevice::init("COYOTE-Dashboard-C6")) {
    setLink(Link::Error);
    Serial.println("OBD: error iniciando BLE.");
    vTaskDelete(nullptr);
    return;
  }
  BLEScan *scan = BLEDevice::getScan();
  scan->setActiveScan(true);
  for (;;) {
    setLink(Link::Searching);
    BLEAdvertisedDevice adapter;
    if (!findVeepeak(*scan, adapter)) {
      Serial.println("OBD BLE: Veepeak no encontrado o ambiguo; reintentando.");
      delay(retryMs);
      continue;
    }

    setLink(Link::Connecting);
    Serial.printf("OBD BLE: conectando %s\n", adapter.getAddress().toString().c_str());
    BLEClient *client = BLEDevice::createClient();
    if (client && client->connectTimeout(&adapter, 8000)) {
      if (BLERemoteCharacteristic *tx = subscribeToElm(*client)) {
        ElmSession elm(*client, *tx);
        pollVehicle(elm);
      } else {
        Serial.println("OBD BLE: sin perfil serie compatible.");
      }
    } else {
      Serial.println("OBD BLE: no se pudo conectar al Veepeak.");
    }
    setLink(Link::Searching);
    delete client;  // En NimBLE, espera el evento de desconexion antes de liberar el cliente.
    delay(retryMs);
  }
}

}  // namespace

bool start() {
  receiveBuffer = xStreamBufferCreate(1024, 1);
  if (receiveBuffer && xTaskCreate(obdTask, "obd-ble", 8192, nullptr, 1, nullptr) == pdPASS) {
    return true;
  }
  if (receiveBuffer) {
    vStreamBufferDelete(receiveBuffer);
    receiveBuffer = nullptr;
  }
  setLink(Link::Error);
  return false;
}

State snapshot() {
  portENTER_CRITICAL(&stateLock);
  const State copy = state;
  portEXIT_CRITICAL(&stateLock);
  return copy;
}

const char *linkText(Link link) {
  switch (link) {
    case Link::Searching: return "BUSCANDO";
    case Link::Connecting: return "CONECTANDO";
    case Link::WaitingForEcu: return "ESPERANDO ECU";
    case Link::Online: return "EN LINEA";
    case Link::Error: return "ERROR BLE";
  }
  return "SIN OBD";
}

}  // namespace Obd
