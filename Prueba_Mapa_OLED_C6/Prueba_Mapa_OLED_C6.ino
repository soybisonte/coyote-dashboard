// COYOTE: prueba temporal para identificar la posicion fisica de cada OLED.
// ESP32-C6, TCA9548A a 0x70, SDA IO18, SCL IO19, alimentacion 3V3.
// No inicia BLE ni ESP-NOW. Restaurar XJ_Dashboard_C6 al terminar el mapa.
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

constexpr uint8_t tcaAddress = 0x70;
constexpr uint8_t oledAddresses[] = {0x3C, 0x3D};
Adafruit_SSD1306 oled(128, 64, &Wire, -1, 100000, 100000);
uint32_t scanNumber = 0;

bool responds(uint8_t address) {
  Wire.beginTransmission(address);
  return Wire.endTransmission() == 0;
}

bool selectChannel(uint8_t mask) {
  Wire.beginTransmission(tcaAddress);
  Wire.write(mask);
  return Wire.endTransmission() == 0;
}

void scanAndLabel() {
  ++scanNumber;
  if (!selectChannel(0)) {
    Serial.println("ERROR: TCA sin respuesta en 0x70.");
    return;
  }
  // Un dispositivo en el bus principal produciria etiquetas ambiguas.
  for (uint8_t address : oledAddresses) {
    if (responds(address)) {
      Serial.println("ERROR: OLED en bus principal con canales apagados.");
      return;
    }
  }

  uint8_t channels = 0, devices = 0;
  for (uint8_t channel = 0; channel < 8; ++channel) {
    if (!selectChannel(1U << channel)) {
      Serial.printf("CH%u: ERROR al seleccionar canal.\n", channel);
      break;
    }
    bool found = false;
    for (uint8_t address : oledAddresses) {
      if (!responds(address)) continue;
      found = true;
      if (!oled.begin(SSD1306_SWITCHCAPVCC, address, false, false)) {
        Serial.printf("CH%u 0x%02X: ERROR de memoria.\n", channel, address);
        continue;
      }
      oled.clearDisplay();
      oled.setTextColor(SSD1306_WHITE);
      oled.setTextWrap(false);
      oled.setTextSize(3);
      // La misma etiqueta en ambos sentidos: legible incluso de cabeza.
      for (uint8_t rotation : {0, 2}) {
        oled.setRotation(rotation);
        oled.setCursor(37, 5);
        oled.print("CH");
        oled.print(channel);
        if (scanNumber & 1U) oled.fillCircle(116, 16, 2, SSD1306_WHITE);
      }
      oled.display();
      if (responds(address)) {
        channels |= 1U << channel;
        ++devices;
        Serial.printf("CH%u: OLED 0x%02X, etiqueta enviada.\n", channel, address);
      } else {
        Serial.printf("CH%u 0x%02X: ERROR, perdio respuesta al dibujar.\n", channel, address);
      }
    }
    if (!found) Serial.printf("CH%u: sin respuesta OLED.\n", channel);
    if (!selectChannel(0)) {
      Serial.println("ERROR: no se pudo desactivar el canal.");
      return;
    }
  }
  Serial.printf("MAPA scan=%lu canales=0x%02X dispositivos=%u\n",
                static_cast<unsigned long>(scanNumber), channels, devices);
}

void setup() {
  digitalWrite(22, LOW);
  pinMode(22, OUTPUT);
  Serial.begin(115200);
  Serial.setTxTimeoutMs(0);
  delay(1500);
  Serial.println("COYOTE: identificacion temporal de canales OLED.");
  if (!Wire.begin(18, 19, 100000)) {
    Serial.println("ERROR: no se pudo iniciar I2C.");
    return;
  }
  Wire.setTimeOut(50);
  scanAndLabel();
}

void loop() {
  static uint32_t previous = millis();
  if (millis() - previous < 3000) {
    delay(1);
    return;
  }
  previous = millis();
  scanAndLabel();
}
