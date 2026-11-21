// Renderiza los mismos componentes que el firmware, con su fuente GFX real.
// Compilar con -I a la carpeta de Adafruit_GFX_Library para glcdfont.c.
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>
#include <glcdfont.c>
#include "../XJ_Mando_TFT/Interfaz_OLED.h"

class BitmapScreen {
  std::vector<uint8_t> pixels = std::vector<uint8_t>(320 * 240, 0);
  int cursorX = 0, cursorY = 0, size = 1;
  uint16_t color = 0xFFFF;
public:
  void fillRect(int x, int y, int width, int height, uint16_t value) {
    assert(value == 0 || value == 0xFFFF);
    for (int yy = std::max(0, y); yy < std::min(240, y + height); ++yy)
      for (int xx = std::max(0, x); xx < std::min(320, x + width); ++xx)
        pixels[yy * 320 + xx] = value ? 255 : 0;
  }
  void drawFastHLine(int x, int y, int width, uint16_t value) {
    fillRect(x, y, width, 1, value);
  }
  void drawRect(int x, int y, int width, int height, uint16_t value) {
    fillRect(x, y, width, 1, value); fillRect(x, y + height - 1, width, 1, value);
    fillRect(x, y, 1, height, value); fillRect(x + width - 1, y, 1, height, value);
  }
  void setTextSize(int value) { size = value; }
  void setTextColor(uint16_t value) { color = value; }
  void setCursor(int x, int y) { cursorX = x; cursorY = y; }
  void print(const char *value) {
    for (unsigned char c : std::string(value)) {
      for (int x = 0; x < 5; ++x)
        for (int y = 0; y < 8; ++y)
          if (font[c * 5 + x] & (1U << y))
            fillRect(cursorX + x * size, cursorY + y * size, size, size, color);
      cursorX += 6 * size;
    }
  }
  void save(const char *path) {
    std::ofstream stream(path, std::ios::binary);
    stream << "P5\n320 240\n255\n";
    stream.write(reinterpret_cast<const char*>(pixels.data()), pixels.size());
    assert(stream.good());
  }
};

int main(int argc, char **argv) {
  if (argc != 2) return 1;
  const std::string base = argv[1];
  BitmapScreen dashboard;
  OledUi::cabecera(dashboard, "COYOTE / TELEMETRIA", "RELES");
  OledUi::texto(dashboard, 8, 34, "C6 OK / OBD OK");
  const char *labels[] = {"RPM / MOTOR", "VOLTAJE / OBD", "TEMP. / MOTOR", "GASOLINA"};
  const char *values[] = {"850", "14.1", "92", "68"};
  const char *units[] = {"RPM", "V", "C", "%"};
  const float fractions[] = {850.f / 6000, (14.1f - 9) / 7, (92.f + 40) / 160, .68f};
  for (uint8_t i = 0; i < 4; ++i) {
    OledUi::etiquetaMedidor(dashboard, i, labels[i]);
    OledUi::medidor(dashboard, i, values[i], units[i], true, fractions[i]);
  }
  dashboard.save((base + "/TFT_OLED_dashboard.pgm").c_str());

  BitmapScreen reles;
  OledUi::cabecera(reles, "COYOTE / RELEVADORES", "VOLVER");
  OledUi::texto(reles, 8, 34, "C5 EN LINEA / 4 CANALES");
  for (uint8_t i = 0; i < 4; ++i)
    OledUi::rele(reles, i, i == 0 || i == 2 ? "ENCENDIDO" : "APAGADO", true, i == 0 || i == 2);
  reles.save((base + "/TFT_OLED_reles.pgm").c_str());

  BitmapScreen sinSenal;
  OledUi::cabecera(sinSenal, "COYOTE / RELEVADORES", "VOLVER");
  OledUi::texto(sinSenal, 8, 34, "C5 SIN SENAL");
  for (uint8_t i = 0; i < 4; ++i) OledUi::rele(sinSenal, i, "SIN SENAL", false, false);
  sinSenal.save((base + "/TFT_OLED_sin_senal.pgm").c_str());
}
