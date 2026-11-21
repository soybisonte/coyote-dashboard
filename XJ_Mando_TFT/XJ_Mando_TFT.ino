/*
  Mando portatil COYOTE, ESP32-2432S028 con dos USB, core ESP32 3.3.10.
  Board ESP32 Dev Module, flash 4 MB, particion Default, PSRAM Disabled.
  Bibliotecas Adafruit GFX, Adafruit ST7735 and ST7789, XPT2046_Touchscreen.
  TFT ST7789 240x320 en horizontal. SPI LCD 14/12/13, CS15, DC2, BL21.
  Touch XPT2046 SPI 25/39/32, CS33. SD sin usar, CS5 en HIGH.
  C6 envia OBD v3 de 32 bytes. Pantalla principal dedicada a las mediciones.
  Control de cuatro relevadores C5 con confirmacion, sin la prueba LED P25.
  Splash Willys coordinado por C6 o local al arrancar el mando solo.
  Conservar Arranque_XJ.h, Logo_Willys.h y Arranque_Radio.h junto al sketch.
  Ajustar touchMin/Max si las zonas tactiles no coinciden con los botones.
  Monitor serie 115200. No envia ordenes al arrancar.
  Pagina RELES: cuatro canales del C5 configurado, con protocolo propio.
  Interfaz en canvas monocromo; presentar diferencias sin borrar la TFT visible.
*/
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_random.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <XPT2046_Touchscreen.h>
#include <freertos/queue.h>
#include <math.h>
#include "Arranque_Radio.h"
#include "Interfaz_OLED.h"
#include "Logo_Mal_Camino.h"
#include "Pantalla_Diferencial.h"
#include "Configuracion_Radio.h"

constexpr auto &dashboardMac = ConfiguracionRadio::macC6;
constexpr uint8_t radioChannel = 1;
constexpr uint32_t radioTimeoutMs = 1500;
constexpr int touchMinX = 200, touchMaxX = 3800;
constexpr int touchMinY = 200, touchMaxY = 3800;

struct __attribute__((packed)) DashboardPacket {
  uint32_t magic;
  uint16_t version;
  uint16_t flags;
  uint32_t sequence;
  uint32_t uptimeMs;
  float rpm, voltage, temperature, fuel;
};
static_assert(sizeof(DashboardPacket) == 32, "Formato de telemetria inesperado");

struct Telemetry {
  DashboardPacket packet;
  uint32_t receivedAt;
  uint32_t count;
};
struct StartupEvent {
  ArranqueRadio::Packet packet;
  uint32_t receivedAt;
};
QueueHandle_t telemetryQueue = nullptr;
QueueHandle_t startupQueue = nullptr;
SPIClass displaySpi(HSPI), touchSpi(VSPI);
Adafruit_ST7789 display(&displaySpi, 15, 2, -1);
GFXcanvas1 screen(320, 240);
PantallaDiferencial presentador;
XPT2046_Touchscreen touch(33);
bool radioReady = false;
uint32_t nextId = 0;
bool touchHeld = true;
bool dashboardNeedsRedraw = true;
bool startupActive = true, startupRemote = false;
uint32_t startupBeganAt = 0, startupLastRemoteAt = 0, startupSession = 0;
uint16_t startupFrame = UINT16_MAX, startupRemoteFrames = 0;
GFXcanvas1 startupScene(ArranqueXJ::sceneWidth, ArranqueXJ::sceneHeight);
constexpr uint16_t background = OledUi::negro, foreground = OledUi::blanco;
bool showRelays = false;
#include "Relevadores_TFT.h"

void receivePacket(const esp_now_recv_info_t *info, const uint8_t *data, int length) {
  if (!info) return;
  if (memcmp(info->src_addr, Relevadores::macReceptor, 6) == 0) {
    RelayUi::receive(data, length);
    return;
  }
  if (memcmp(info->src_addr, dashboardMac, 6) == 0 && length == sizeof(ArranqueRadio::Packet)) {
    ArranqueRadio::Packet packet;
    memcpy(&packet, data, sizeof(packet));
    if (packet.magic != ArranqueRadio::magic || packet.version != ArranqueRadio::version ||
        packet.frame > ArranqueRadio::doneFrame || packet.session == 0) return;
    const StartupEvent event{packet, millis()};
    xQueueOverwrite(startupQueue, &event);
    return;
  }
  if (memcmp(info->src_addr, dashboardMac, 6) == 0 && length == sizeof(DashboardPacket)) {
    DashboardPacket packet;
    memcpy(&packet, data, sizeof(packet));
    if (packet.magic != 0x584A4431 || packet.version != 3 ||
        (packet.flags & ~0x7E) != 0 ||
        ((packet.flags & 0x40) && (packet.flags & 0x10)) ||
        !isfinite(packet.rpm) || !isfinite(packet.voltage) ||
        !isfinite(packet.temperature) || !isfinite(packet.fuel)) return;
    static uint32_t count = 0;
    const Telemetry telemetry{packet, millis(), ++count};
    xQueueOverwrite(telemetryQueue, &telemetry);
    return;
  }
}

void readTouch() {
  static uint32_t lastTouch = 0;
  if (millis() - lastTouch < 25) return;
  lastTouch = millis();
  if (!touch.touched()) {
    touchHeld = false;
    return;
  }
  const TS_Point point = touch.getPoint();
  if (touchHeld || point.z < 300) return;
  touchHeld = true;
  const int x = constrain(map(point.x, touchMinX, touchMaxX, 319, 0), 0, 319);
  const int y = constrain(map(point.y, touchMinY, touchMaxY, 239, 0), 0, 239);
  Serial.printf("Touch raw=%d,%d z=%d pantalla=%d,%d\n", point.x, point.y, point.z, x, y);
  if (x >= 240 && x < 312 && y >= 3 && y < 27) {
    showRelays = !showRelays;
    dashboardNeedsRedraw = true;
    RelayUi::dirty = true;
    return;
  }
  if (showRelays) RelayUi::touch(x, y);
}

void drawText(int x, int y, int width, const char *text, uint16_t color, uint8_t size) {
  screen.fillRect(x, y, width, 8 * size, background);
  screen.setCursor(x, y);
  screen.setTextSize(size);
  screen.setTextColor(color);
  screen.print(text);
}

void drawStartupFrame(uint16_t frame) {
  if (frame == ArranqueRadio::introFrame) {
    display.fillScreen(0);
    display.drawBitmap((320 - MalCamino::tftWidth) / 2, (240 - MalCamino::tftHeight) / 2,
        MalCamino::tftBitmap, MalCamino::tftWidth, MalCamino::tftHeight, foreground);
    return;
  }
  const unsigned sceneFrame = frame - ArranqueRadio::firstMotionFrame;
  static uint16_t scanline[320];
  constexpr int sceneStride = (ArranqueXJ::sceneWidth + 7) / 8;
  constexpr int viewportWidth = 224;  // 224x168 conserva el formato 4:3 de la TFT.
  const unsigned motionFrame = min<unsigned>(sceneFrame, ArranqueXJ::frameCount - 1);
  ArranqueXJ::drawFrame(startupScene, motionFrame);
  const float progress = static_cast<float>(motionFrame) / (ArranqueXJ::frameCount - 1);
  const float eased = 0.78f * progress +
                      0.22f * progress * progress * (3.0f - 2.0f * progress);
  const int vehicleX = lroundf(41.0f + 353.0f * eased);
  const int viewportX = constrain(vehicleX - viewportWidth / 2,
                                  0, ArranqueXJ::sceneWidth - viewportWidth);
  const bool closing = sceneFrame >= ArranqueXJ::frameCount;
  const int edge = frame == ArranqueRadio::logoFrame ? 320 :
      320 * (static_cast<int>(sceneFrame) - ArranqueXJ::frameCount + 1) /
      ArranqueXJ::closingFrameCount;
  constexpr int logoScale = 2;
  constexpr int logoX = (320 - ArranqueXJ::logoWidth * logoScale) / 2;
  constexpr int logoY = (240 - ArranqueXJ::logoHeight * logoScale) / 2;
  constexpr int logoStride = (ArranqueXJ::logoWidth + 7) / 8;
  display.startWrite();
  display.setAddrWindow(0, 0, 320, 240);
  for (int y = 0; y < 240; ++y) {
    const int sourceY = y * ArranqueXJ::sceneHeight / 240;
    for (int x = 0; x < 320; ++x) {
      bool white;
      if (closing && x < edge) {
        white = false;
        if (x >= logoX && x < logoX + ArranqueXJ::logoWidth * logoScale &&
            y >= logoY && y < logoY + ArranqueXJ::logoHeight * logoScale) {
          const int logoPixelX = (x - logoX) / logoScale;
          const int logoPixelY = (y - logoY) / logoScale;
          const uint8_t bits = pgm_read_byte(ArranqueXJ::logoWillys +
              logoPixelY * logoStride + logoPixelX / 8);
          white = bits & (0x80 >> (logoPixelX % 8));
        }
      } else {
        const int sourceX = viewportX + x * viewportWidth / 320;
        const uint8_t bits = startupScene.getBuffer()[sourceY * sceneStride + sourceX / 8];
        white = bits & (0x80 >> (sourceX % 8));
      }
      scanline[x] = white ? foreground : 0;
    }
    display.writePixels(scanline, 320);
  }
  display.endWrite();
}

void finishStartup() {
  startupActive = false;
  // El splash escribio directamente en la TFT: renovar la primera vista completa.
  presentador.invalidar();
  dashboardNeedsRedraw = true;
  RelayUi::dirty = true;
  touchHeld = true;
  Serial.printf("Arranque COYOTE completo modo=%s sesion=%lu fotogramas_rx=%u\n",
      startupRemote ? "C6" : "local", static_cast<unsigned long>(startupSession),
      startupRemoteFrames);
}

bool updateStartup() {
  if (!startupScene.getBuffer()) {
    if (startupActive) {
      Serial.println("ERROR: sin memoria para splash Willys.");
      finishStartup();
    }
    return false;
  }
  StartupEvent event;
  if (startupQueue && xQueueReceive(startupQueue, &event, 0) == pdTRUE) {
    const bool newSession = event.packet.session != startupSession;
    if (newSession || (startupActive && event.packet.frame >= startupFrame)) {
      const bool advanced = newSession || event.packet.frame != startupFrame;
      if (newSession) {
        startupRemoteFrames = 0;
        startupFrame = UINT16_MAX;
        Serial.printf("Arranque COYOTE sincronizando C6 sesion=%lu desde frame=%u\n",
            static_cast<unsigned long>(event.packet.session), event.packet.frame);
      }
      startupSession = event.packet.session;
      startupLastRemoteAt = event.receivedAt;
      startupRemote = true;
      startupActive = true;
      touchHeld = true;
      ++startupRemoteFrames;
      if (event.packet.frame == ArranqueRadio::doneFrame) {
        startupFrame = event.packet.frame;
        finishStartup();
      } else {
        startupFrame = event.packet.frame;
        if (advanced) drawStartupFrame(startupFrame);
      }
    }
  }
  if (!startupActive) return false;
  const uint32_t now = millis();
  if (startupRemote) {
    if (startupFrame == ArranqueRadio::logoFrame &&
        now - startupLastRemoteAt >= ArranqueRadio::logoHoldMs) {
      finishStartup();
      return false;
    }
    if (now - startupLastRemoteAt < 1500) return true;
    startupRemote = false;
    startupBeganAt = now - ArranqueRadio::offsetFor(startupFrame);
    Serial.println("Arranque COYOTE continua local por perdida de enlace C6.");
  }
  if (static_cast<int32_t>(now - startupBeganAt) < 0) return true;
  const uint32_t elapsed = now - startupBeganAt;
  const uint16_t frame = ArranqueRadio::frameAt(elapsed);
  if (frame == ArranqueRadio::doneFrame) {
    finishStartup();
    return false;
  }
  if (frame != startupFrame) {
    startupFrame = frame;
    drawStartupFrame(frame);
  }
  return true;
}

void drawDashboard(const Telemetry &telemetry, bool haveTelemetry) {
  static char previousStatus[32] = "", previousValues[4][24] = {};
  if (dashboardNeedsRedraw) {
    screen.fillScreen(background);
    previousStatus[0] = '\0';
    for (auto &value : previousValues) value[0] = '\0';
    OledUi::cabecera(screen, "COYOTE / TELEMETRIA", "RELES");
    const char *labels[] = {"RPM / MOTOR", "VOLTAJE / OBD", "TEMP. / MOTOR", "GASOLINA"};
    for (uint8_t i = 0; i < 4; ++i)
      OledUi::etiquetaMedidor(screen, i, labels[i]);
    dashboardNeedsRedraw = false;
  }
  const bool online = haveTelemetry && millis() - telemetry.receivedAt < radioTimeoutMs;
  const DashboardPacket &packet = telemetry.packet;
  const bool obdOnline = online && (packet.flags & 0x20);
  const char *status = !radioReady ? "ERROR RADIO" : !online ? "C6 SIN SENAL" :
                       obdOnline ? "C6 OK / OBD OK" : "C6 OK / SIN OBD";
  if (strcmp(status, previousStatus) != 0) {
    drawText(8, 34, 304, status, foreground, 1);
    snprintf(previousStatus, sizeof(previousStatus), "%s", status);
  }
  const float values[] = {packet.rpm, packet.voltage, packet.temperature, packet.fuel};
  const char *units[] = {"RPM", "V", "C", "%"};
  const float minimum[] = {0, 9, -40, 0}, maximum[] = {6000, 16, 120, 100};
  for (uint8_t i = 0; i < 4; ++i) {
    char text[24];
    if (i == 3 && obdOnline && (packet.flags & 0x40)) {
      snprintf(text, sizeof(text), "NO DISP. OBD");
    } else if (obdOnline && (packet.flags & (1U << (i + 1)))) {
      snprintf(text, sizeof(text), i == 1 ? "%.1f" : "%.0f", values[i]);
    } else {
      snprintf(text, sizeof(text), "--");
    }
    if (strcmp(text, previousValues[i]) == 0) continue;
    OledUi::medidor(screen, i, text, units[i], obdOnline && (packet.flags & (1U << (i + 1))),
        (values[i] - minimum[i]) / (maximum[i] - minimum[i]));
    snprintf(previousValues[i], sizeof(previousValues[i]), "%s", text);
  }
}

void setup() {
  Serial.begin(115200);
  nextId = esp_random();
  digitalWrite(21, LOW);
  pinMode(21, OUTPUT);
  for (uint8_t pin : {uint8_t(5), uint8_t(33), uint8_t(4), uint8_t(16), uint8_t(17)}) {
    digitalWrite(pin, HIGH);
    pinMode(pin, OUTPUT);
  }
  displaySpi.begin(14, 12, 13, 15);
  display.init(240, 320, SPI_MODE0);
  display.setSPISpeed(24000000);
  display.setRotation(1);
  uint8_t madctl = ST77XX_MADCTL_MY | ST77XX_MADCTL_MV | 0x08;  // BGR.
  display.sendCommand(ST77XX_MADCTL, &madctl, 1);
  display.invertDisplay(false);
  screen.setTextWrap(false);
  display.fillScreen(0);
  digitalWrite(21, HIGH);
  if (!screen.getBuffer()) {
    display.setTextColor(foreground);
    display.setTextSize(2);
    display.setCursor(8, 8);
    display.print("ERROR MEMORIA UI");
    Serial.println("ERROR: no hay memoria para el canvas de la interfaz.");
    return;
  }
  startupBeganAt = millis() + 2000;
  touchSpi.begin(25, 39, 32, 33);
  touch.begin(touchSpi);
  touch.setRotation(1);
  startupFrame = ArranqueRadio::introFrame;
  drawStartupFrame(startupFrame);

  telemetryQueue = xQueueCreate(1, sizeof(Telemetry));
  startupQueue = xQueueCreate(1, sizeof(StartupEvent));
  if (!telemetryQueue || !startupQueue || !WiFi.mode(WIFI_STA) ||
      !WiFi.setSleep(false) ||
      esp_wifi_set_channel(radioChannel, WIFI_SECOND_CHAN_NONE) != ESP_OK ||
      esp_now_init() != ESP_OK) {
    Serial.println("ERROR iniciando radio o colas.");
    return;
  }
  radioReady = RelayUi::begin() &&
               esp_now_register_recv_cb(receivePacket) == ESP_OK;
  Serial.printf("COYOTE TFT MAC %s | ST7789 320x240 | radio %s\n",
                WiFi.macAddress().c_str(), radioReady ? "OK" : "ERROR");
}

void loop() {
  if (!screen.getBuffer()) { delay(10); return; }
  static Telemetry telemetry{};
  static bool haveTelemetry = false;
  static uint32_t lastDraw = 0, lastLog = 0;
  if (radioReady) {
    if (xQueueReceive(telemetryQueue, &telemetry, 0) == pdTRUE) haveTelemetry = true;
    RelayUi::service();
  }
  const bool splash = updateStartup();
  if (radioReady && !splash) readTouch();
  if (!splash && millis() - lastDraw >= 100) {
    lastDraw = millis();
    if (showRelays) RelayUi::draw();
    else drawDashboard(telemetry, haveTelemetry);
    presentador.presentar(display, screen.getBuffer());
  }
  if (millis() - lastLog >= 1000) {
    lastLog = millis();
    Serial.printf("TFT RX paquetes=%lu seq=%lu flags=%02X edad=%lu ms heap=%u | C5 RX=%lu edad=%lu ms\n",
        static_cast<unsigned long>(telemetry.count),
        static_cast<unsigned long>(telemetry.packet.sequence), telemetry.packet.flags,
        static_cast<unsigned long>(haveTelemetry ? millis() - telemetry.receivedAt : 0),
        ESP.getFreeHeap(), static_cast<unsigned long>(RelayUi::receivedCount),
        static_cast<unsigned long>(RelayUi::known ? millis() - RelayUi::seen : 0));
  }
  delay(1);
}
