// COYOTE: dashboard fijo, OLED y telemetria hacia el mando TFT.
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <math.h>
#include <esp_random.h>
#include "Arranque_XJ.h"
#include "Arranque_Radio.h"
#include "Obd_BLE.h"
#include "Logo_Mal_Camino.h"
#include "Configuracion_OLED.h"
#include "Configuracion_Radio.h"

// ESP32C6 Dev Module, USB CDC On Boot habilitado, flash de 8 MB.
// IO18 a SDA e IO19 a SCL. Una OLED por canal del TCA9548A.
// Alimentar las OLED y el TCA a 3V3 con GND comun.
constexpr uint8_t sdaPin = 18;
constexpr uint8_t sclPin = 19;
constexpr uint8_t lcdBacklightPin = 22;
constexpr uint32_t i2cFrequency = 100000;
constexpr uint32_t oledFrequency = 400000;
constexpr uint8_t oledWidth = ArranqueXJ::panelWidth;
constexpr uint8_t oledHeight = ArranqueXJ::panelHeight;
constexpr uint8_t tcaAddress = 0x70;
constexpr uint8_t tcaChannelCount = 8;
constexpr uint8_t logoChannel = 3;
constexpr uint8_t oledAddresses[] = {0x3C, 0x3D};

Adafruit_SSD1306 oled3C(oledWidth, oledHeight, &Wire, -1,
                       oledFrequency, i2cFrequency);
Adafruit_SSD1306 oled3D(oledWidth, oledHeight, &Wire, -1,
                       oledFrequency, i2cFrequency);

struct PanelPosition {
  uint8_t channel;
  uint8_t column;
  uint8_t row;
};

// Mapa confirmado con etiquetas en IMG_5265, USB a la derecha.
constexpr PanelPosition panelPositions[] = {
  {0, 0, 0}, {3, 1, 0}, {4, 2, 0},
  {5, 0, 1}, {6, 1, 1}, {7, 2, 1},
};

struct GaugeLayout {
  uint8_t channel;
  const char *label;
  const char *unit;
  float minimum;
  float maximum;
};

// Orden de Obd::Gauge; posicion fisica de lectura de izquierda a derecha.
constexpr GaugeLayout gauges[Obd::GaugeCount] = {
  {5, "RPM", "rpm", 0, 6000},
  {6, "VOLTAJE OBD", "V", 9, 16},
  {4, "REFRIGERANTE", "C", -40, 120},
  {7, "GASOLINA", "%", 0, 100},
};
constexpr uint8_t statusChannel = 0;
constexpr uint8_t dashboardScreenCount = Obd::GaugeCount + 2; // Estado + logo.
uint8_t addresses[tcaChannelCount] = {};

constexpr uint8_t espNowChannel = 1;

// Telemetria v3 compatible con el dashboard anterior, 32 bytes en canal 1.
struct __attribute__((packed)) DashboardPacket {
  uint32_t magic;
  uint16_t version;
  uint16_t flags;
  uint32_t sequence;
  uint32_t uptimeMs;
  float rpm, voltage, temperature, fuel;
};
static_assert(sizeof(DashboardPacket) == 32, "Formato de telemetria inesperado");
constexpr auto &portableMac = ConfiguracionRadio::macTft;
bool telemetryReady = false;

void startDashboardRadio() {
  if (!WiFi.mode(WIFI_STA) ||
      !WiFi.setSleep(false) ||
      esp_wifi_set_channel(espNowChannel, WIFI_SECOND_CHAN_NONE) != ESP_OK ||
      esp_now_init() != ESP_OK) {
    Serial.println("ERROR: no se pudo iniciar la telemetria ESP-NOW de COYOTE.");
    return;
  }
  esp_now_peer_info_t peer{};
  memcpy(peer.peer_addr, portableMac, sizeof(portableMac));
  peer.channel = espNowChannel;
  peer.ifidx = WIFI_IF_STA;
  telemetryReady = esp_now_add_peer(&peer) == ESP_OK;
  Serial.println(telemetryReady ? "TFT: telemetria ESP-NOW lista en canal 1."
                               : "ERROR: no se pudo registrar la TFT.");
}

void publishTelemetry() {
  static uint32_t lastSend = 0, lastLog = 0, sequence = 0, sendErrors = 0;
  if (!telemetryReady || millis() - lastSend < 100) return;
  const Obd::State obd = Obd::snapshot();
  const uint32_t now = millis();
  lastSend = now;
  DashboardPacket packet{0x584A4431, 3, 0, ++sequence, now, 0, 0, 0, 0};
  const bool online = obd.link == Obd::Link::Online;
  if (online) packet.flags |= 0x20;
  if (online && obd.fuelUnsupported) packet.flags |= 0x40;
  float values[Obd::GaugeCount] = {};
  for (uint8_t i = 0; i < Obd::GaugeCount; ++i) {
    const Obd::Reading &reading = obd.readings[i];
    const bool unsupported = i == Obd::Fuel && obd.fuelUnsupported;
    if (online && !unsupported && reading.valid && isfinite(reading.value) &&
        now - reading.timestamp < Obd::readingLifetimeMs) {
      values[i] = reading.value;
      packet.flags |= 1U << (i + 1);
    }
  }
  packet.rpm = values[Obd::Rpm];
  packet.voltage = values[Obd::Voltage];
  packet.temperature = values[Obd::Coolant];
  packet.fuel = values[Obd::Fuel];
  if (esp_now_send(portableMac, reinterpret_cast<const uint8_t *>(&packet),
                   sizeof(packet)) != ESP_OK) ++sendErrors;
  if (now - lastLog < 1000) return;
  lastLog = now;
  char line[144];
  const int length = snprintf(line, sizeof(line),
      "TFT TX seq=%lu flags=%02X errores=%lu | OBD %s\n",
      static_cast<unsigned long>(sequence), packet.flags,
      static_cast<unsigned long>(sendErrors), Obd::linkText(obd.link));
  if (length > 0 && length < sizeof(line) && Serial.availableForWrite() >= length)
    Serial.write(reinterpret_cast<const uint8_t *>(line), length);
}

Adafruit_SSD1306 &displayForAddress(uint8_t address) {
  return address == 0x3C ? oled3C : oled3D;
}

Adafruit_SSD1306 &displayForPanel(uint8_t channel, uint8_t address) {
  Adafruit_SSD1306 &oled = displayForAddress(address);
  // Los canales comparten un objeto por direccion I2C: aplicar SIEMPRE el
  // giro del canal antes de dibujar para no heredarlo de la OLED anterior.
  ConfiguracionOled::orientar(oled, channel);
  return oled;
}

bool setTcaChannels(uint8_t mask) {
  Wire.beginTransmission(tcaAddress);
  Wire.write(mask);
  return Wire.endTransmission() == 0;
}

uint8_t findOledAddress() {
  for (const uint8_t address : oledAddresses) {
    Wire.beginTransmission(address);
    if (Wire.endTransmission() == 0) {
      return address;
    }
  }
  return 0;
}

void showOledLabel(uint8_t address, const char *label) {
  Adafruit_SSD1306 &oled = displayForAddress(address);
  oled.setRotation(0);  // Diagnostico de una OLED directa, fuera del TCA.
  oled.clearDisplay();
  oled.drawRect(0, 0, oledWidth, oledHeight, SSD1306_WHITE);
  oled.setTextColor(SSD1306_WHITE);
  oled.setTextSize(2);
  oled.setCursor(8, 5);
  oled.print(label);
  oled.setTextSize(1);
  oled.setCursor(8, 22);
  oled.print("I2C 0x");
  oled.print(address, HEX);
  oled.display();
}

void drawBranding(Adafruit_SSD1306 &oled) {
  oled.drawBitmap((oledWidth - MalCamino::oledWidth) / 2,
      (oledHeight - MalCamino::oledHeight) / 2, MalCamino::oledBitmap,
      MalCamino::oledWidth, MalCamino::oledHeight, SSD1306_WHITE);
}

bool updatePanel(GFXcanvas1 &scene, const uint8_t *previous,
                 const PanelPosition &position, uint8_t address) {
  constexpr int tileStride = oledWidth / 8;
  constexpr int sceneStride = ArranqueXJ::sceneWidth / 8;
  const int sourceByte = position.column * ArranqueXJ::columnPitch / 8;
  const int sourceRow = position.row * ArranqueXJ::rowPitch;
  bool changed = previous == nullptr;
  uint8_t tile[oledHeight * tileStride];
  for (int y = 0; y < oledHeight; ++y) {
    const int offset = (sourceRow + y) * sceneStride + sourceByte;
    const uint8_t *row = scene.getBuffer() + offset;
    memcpy(tile + y * tileStride, row, tileStride);
    if (!changed && memcmp(row, previous + offset, tileStride) != 0) {
      changed = true;
    }
  }
  if (!changed) return true;
  if (!setTcaChannels(1U << position.channel)) return false;

  Adafruit_SSD1306 &oled = displayForPanel(position.channel, address);
  oled.clearDisplay();
  oled.drawBitmap(0, 0, tile, oledWidth, oledHeight, SSD1306_WHITE);
  oled.display();
  return true;
}

void publishStartupFrame(uint16_t frame, uint32_t session) {
  if (!telemetryReady) return;
  const ArranqueRadio::Packet packet{ArranqueRadio::magic, ArranqueRadio::version,
                                    frame, session};
  esp_now_send(portableMac, reinterpret_cast<const uint8_t *>(&packet), sizeof(packet));
}

void playStartup(const uint8_t addresses[]) {
  unsigned panelCount = 0;
  for (const auto &position : panelPositions) {
    if (addresses[position.channel]) ++panelCount;
  }
  if (panelCount == 0) return;

  GFXcanvas1 scene(ArranqueXJ::sceneWidth, ArranqueXJ::sceneHeight);
  GFXcanvas1 previous(ArranqueXJ::sceneWidth, ArranqueXJ::sceneHeight);
  if (!scene.getBuffer() || !previous.getBuffer()) {
    Serial.println("ERROR: sin memoria para el arranque COYOTE.");
    return;
  }

  Serial.printf("Arranque Willys: %u paneles detectados.\n", panelCount);
  const uint32_t session = esp_random() | 1U;
  Serial.printf("Arranque sincronizado sesion=%lu.\n", static_cast<unsigned long>(session));
  const uint32_t startTime = millis();
  // Mal Camino primero, en la OLED central (o la primera disponible).
  uint8_t introChannel = logoChannel;
  if (!addresses[introChannel])
    for (const auto &position : panelPositions)
      if (addresses[position.channel]) { introChannel = position.channel; break; }
  for (const auto &position : panelPositions) {
    if (!addresses[position.channel] || !setTcaChannels(1U << position.channel)) continue;
    Adafruit_SSD1306 &oled = displayForPanel(position.channel, addresses[position.channel]);
    oled.clearDisplay();
    if (position.channel == introChannel) drawBranding(oled);
    oled.display();
  }
  const uint32_t introBegan = millis();
  Serial.println("COYOTE: intro Mal Camino antes del Willys.");
  while (true) {
    const uint32_t elapsed = millis() - introBegan;
    if (elapsed >= ArranqueRadio::introHoldMs) break;
    publishStartupFrame(ArranqueRadio::introFrame, session);
    const uint32_t remaining = ArranqueRadio::introHoldMs - elapsed;
    delay(min(ArranqueRadio::frameMs, remaining));
  }
  constexpr unsigned totalFrames = ArranqueXJ::frameCount + ArranqueXJ::closingFrameCount;
  for (unsigned frame = 0; frame < totalFrames; ++frame) {
    const uint32_t frameStart = millis();
    publishStartupFrame(ArranqueRadio::firstMotionFrame + frame, session);
    if (frame < ArranqueXJ::frameCount) {
      ArranqueXJ::drawFrame(scene, frame);
    } else {
      ArranqueXJ::drawClosingFrame(scene, frame - ArranqueXJ::frameCount + 1);
    }
    for (const auto &position : panelPositions) {
      const uint8_t address = addresses[position.channel];
      if (!address) continue;
      if (!updatePanel(scene, frame == 0 ? nullptr : previous.getBuffer(),
                       position, address)) {
        Serial.printf("ERROR: arranque interrumpido en CH%u.\n", position.channel);
        return;
      }
    }
    constexpr size_t sceneBytes = ArranqueXJ::sceneWidth / 8 * ArranqueXJ::sceneHeight;
    memcpy(previous.getBuffer(), scene.getBuffer(), sceneBytes);
    const uint32_t elapsed = millis() - frameStart;
    if (elapsed < ArranqueRadio::frameMs) delay(ArranqueRadio::frameMs - elapsed);
  }
  Serial.printf("Arranque Willys terminado en %lu ms.\n",
                static_cast<unsigned long>(millis() - startTime));
  if (addresses[logoChannel]) {
    Serial.printf("Logo Willys enviado a CH%u.\n", logoChannel);
  } else {
    Serial.printf("CH%u sin OLED: no se pudo mostrar el logo Willys.\n", logoChannel);
  }
  publishStartupFrame(ArranqueRadio::logoFrame, session);
  delay(ArranqueRadio::logoHoldMs);
  publishStartupFrame(ArranqueRadio::doneFrame, session);
  Serial.printf("Arranque sincronizado completo sesion=%lu.\n",
                static_cast<unsigned long>(session));
}

void drawGauge(Adafruit_SSD1306 &oled, Obd::Gauge gauge, const Obd::State &state,
               bool valid) {
  const GaugeLayout &layout = gauges[gauge];
  const Obd::Reading &reading = state.readings[gauge];
  oled.setTextSize(1);
  oled.setCursor(0, 0);
  oled.print(layout.label);
  oled.setTextSize(2);
  oled.setCursor(0, 14);
  if (valid) oled.print(reading.value, gauge == Obd::Voltage ? 1 : 0);
  else oled.print("--");
  oled.print(" ");
  oled.print(layout.unit);

  oled.drawRect(0, 40, oledWidth, 10, SSD1306_WHITE);
  if (valid) {
    const float ratio = (reading.value - layout.minimum) / (layout.maximum - layout.minimum);
    const int bar = static_cast<int>(constrain(ratio, 0.0f, 1.0f) * (oledWidth - 4));
    oled.fillRect(2, 42, bar, 6, SSD1306_WHITE);
  }
  oled.setTextSize(1);
  oled.setCursor(0, 55);
  if (state.link != Obd::Link::Online) oled.print("SIN OBD");
  else if (gauge == Obd::Fuel && state.fuelUnsupported) oled.print("NO DISP. OBD");
  else if (!valid) oled.print("SIN DATO");
  else oled.print(gauge == Obd::Voltage ? "V ADAPTADOR OBD" : "LECTURA REAL");
}

void drawObdStatus(Adafruit_SSD1306 &oled, Obd::Link link) {
  oled.setTextSize(1);
  oled.setCursor(0, 0);
  oled.print("COYOTE / OBD2");
  oled.setTextSize(link == Obd::Link::WaitingForEcu || link == Obd::Link::Connecting ? 1 : 2);
  oled.setCursor(0, 23);
  oled.print(Obd::linkText(link));
  oled.setTextSize(1);
  oled.setCursor(0, 55);
  oled.print("VEEPEAK / BLE");
}

void setup() {
  digitalWrite(lcdBacklightPin, LOW);
  pinMode(lcdBacklightPin, OUTPUT);

  Serial.begin(115200);
  Serial.setTxTimeoutMs(0);
  delay(1500);

  if (!Wire.begin(sdaPin, sclPin, i2cFrequency)) {
    Serial.println("ERROR: no se pudo iniciar I2C en IO18 e IO19.");
    return;
  }
  Wire.setTimeOut(50);

  if (!setTcaChannels(0)) {
    Serial.println("TCA sin respuesta en 0x70. Probando OLED directa.");
    const uint8_t address = findOledAddress();
    if (address == 0) {
      Serial.println("ERROR: sin respuesta OLED en 0x3C ni en 0x3D.");
    } else if (displayForAddress(address).begin(SSD1306_SWITCHCAPVCC, address,
                                                false, false)) {
      showOledLabel(address, "OLED OK");
      Serial.printf("OLED directa 0x%02X: etiqueta enviada.\n", address);
    } else {
      Serial.println("ERROR: no se pudo inicializar la OLED directa.");
    }
    return;
  }

  Serial.println("TCA responde en 0x70.");
  if (findOledAddress() != 0) {
    Serial.println("ERROR: OLED en el bus principal. Usa SDn y SCn del TCA.");
    return;
  }

  for (unsigned channel = 0; channel < tcaChannelCount; ++channel) {
    if (!setTcaChannels(1U << channel)) {
      Serial.printf("ERROR: no se pudo seleccionar CH%u.\n", channel);
      break;
    }
    const uint8_t address = findOledAddress();
    if (address == 0) {
      Serial.printf("CH%u: sin respuesta OLED.\n", channel);
      continue;
    }

    if (displayForAddress(address).begin(SSD1306_SWITCHCAPVCC, address,
                                         false, false)) {
      addresses[channel] = address;
      Serial.printf("CH%u: OLED detectada en 0x%02X.\n", channel, address);
    } else {
      Serial.printf("CH%u 0x%02X: no se pudo inicializar la OLED.\n", channel, address);
    }
  }

  startDashboardRadio();
  if (!Obd::start()) Serial.println("ERROR: no se pudo iniciar la tarea OBD BLE.");
  playStartup(addresses);
  setTcaChannels(0);
  Serial.println("COYOTE C6: CH0 estado OBD | CH3 Mal Camino | CH4 refrigerante | CH5 RPM | CH6 voltaje | CH7 gasolina.");
}

void loop() {
  publishTelemetry();
  // Dibujar por turno y solo cuando cambie el dato o el estado de la pantalla.
  struct ScreenState {
    bool drawn;
    Obd::Link link;
    bool fuelUnsupported;
    bool valid;
    float value;
  };
  static ScreenState last[dashboardScreenCount] = {};
  static uint32_t lastSlot = 0;
  static uint8_t slot = 0;
  if (millis() - lastSlot < 32) {
    delay(1);
    return;
  }
  lastSlot = millis();
  const uint8_t index = slot;
  slot = (slot + 1) % dashboardScreenCount;
  const bool status = index == Obd::GaugeCount;
  const bool branding = index == Obd::GaugeCount + 1;
  const uint8_t channel = branding ? logoChannel :
                          status ? statusChannel : gauges[index].channel;
  const uint8_t address = addresses[channel];
  if (!address) return;
  const Obd::State obd = Obd::snapshot();
  ScreenState current{true, obd.link, index == Obd::Fuel && obd.fuelUnsupported, false, 0};
  if (!status && !branding) {
    const Obd::Reading &reading = obd.readings[index];
    current.valid = obd.link == Obd::Link::Online && reading.valid &&
                    millis() - reading.timestamp < Obd::readingLifetimeMs;
    if (current.valid) current.value = reading.value;
  }
  const ScreenState &previous = last[index];
  if (previous.drawn && previous.link == current.link && previous.valid == current.valid &&
      previous.fuelUnsupported == current.fuelUnsupported && previous.value == current.value) {
    return;
  }

  if (!setTcaChannels(1U << channel)) return;
  Adafruit_SSD1306 &oled = displayForPanel(channel, address);
  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);
  oled.setTextWrap(false);
  if (status) drawObdStatus(oled, obd.link);
  else if (branding) drawBranding(oled);
  else drawGauge(oled, static_cast<Obd::Gauge>(index), obd, current.valid);
  oled.display();
  setTcaChannels(0);
  last[index] = current;
}
