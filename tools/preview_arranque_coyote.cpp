#include <cassert>
#include <cstdio>
#include <fstream>
#include <string>
#include <algorithm>
#include <Adafruit_GFX.h>
#include "../XJ_Mando_TFT/Arranque_Radio.h"
#include "../XJ_Mando_TFT/Logo_Mal_Camino.h"

void guardar(GFXcanvas1 &canvas, const std::string &ruta) {
  std::ofstream out(ruta, std::ios::binary);
  out << "P5\n" << canvas.width() << " " << canvas.height() << "\n255\n";
  for (int y = 0; y < canvas.height(); ++y)
    for (int x = 0; x < canvas.width(); ++x)
      out.put(static_cast<char>(canvas.getPixel(x, y) ? 255 : 0));
  assert(out.good());
}

int main(int argc, char **argv) {
  if (argc != 2) return 1;
  using namespace ArranqueRadio;
  // Limites de la secuencia y continuidad al pasar de remoto a local.
  assert(frameAt(0) == introFrame && frameAt(introHoldMs - 1) == introFrame);
  assert(frameAt(introHoldMs) == firstMotionFrame);
  assert(frameAt(logoOffsetMs - 1) == logoFrame - 1);
  assert(frameAt(logoOffsetMs) == logoFrame);
  assert(frameAt(durationMs - 1) == logoFrame && frameAt(durationMs) == doneFrame);
  for (uint16_t f = introFrame; f <= doneFrame; ++f) assert(frameAt(offsetFor(f)) == f);

  GFXcanvas1 scene(ArranqueXJ::sceneWidth, ArranqueXJ::sceneHeight);
  GFXcanvas1 tft(320, 240), oled(128, 64), wheel(25, 25);
  ArranqueXJ::drawWheel(wheel, {12, 12}, 0);
  guardar(wheel, std::string(argv[1]) + "/rin.pgm");
  oled.drawBitmap((128 - MalCamino::oledWidth) / 2, (64 - MalCamino::oledHeight) / 2,
      MalCamino::oledBitmap, MalCamino::oledWidth, MalCamino::oledHeight, 1);
  guardar(oled, std::string(argv[1]) + "/intro_oled.pgm");

  for (uint16_t f = introFrame; f < doneFrame; ++f) {
    tft.fillScreen(0);
    if (f == introFrame) {
      tft.drawBitmap((320 - MalCamino::tftWidth) / 2, (240 - MalCamino::tftHeight) / 2,
          MalCamino::tftBitmap, MalCamino::tftWidth, MalCamino::tftHeight, 1);
    } else {
      const unsigned sourceFrame = f - firstMotionFrame;
      const unsigned motion = std::min<unsigned>(sourceFrame, ArranqueXJ::frameCount - 1);
      ArranqueXJ::drawFrame(scene, motion);
      const float progress = float(motion) / (ArranqueXJ::frameCount - 1);
      const float eased = .78f * progress + .22f * progress * progress * (3 - 2 * progress);
      const int vehicleX = lroundf(41 + 353 * eased);
      const int viewportX = std::max(0, std::min(ArranqueXJ::sceneWidth - 224, vehicleX - 112));
      for (int y = 0; y < 240; ++y)
        for (int x = 0; x < 320; ++x)
          tft.drawPixel(x, y, scene.getPixel(viewportX + x * 224 / 320,
              y * ArranqueXJ::sceneHeight / 240));
      if (sourceFrame >= ArranqueXJ::frameCount) {
        const int edge = f == logoFrame ? 320 :
            320 * (sourceFrame - ArranqueXJ::frameCount + 1) / ArranqueXJ::closingFrameCount;
        tft.fillRect(0, 0, edge, 240, 0);
        constexpr int lx = (320 - ArranqueXJ::logoWidth * 2) / 2;
        constexpr int ly = (240 - ArranqueXJ::logoHeight * 2) / 2;
        constexpr int stride = (ArranqueXJ::logoWidth + 7) / 8;
        for (int y = 0; y < ArranqueXJ::logoHeight * 2; ++y)
          for (int x = 0; x < ArranqueXJ::logoWidth * 2 && lx + x < edge; ++x)
            if (ArranqueXJ::logoWillys[(y / 2) * stride + (x / 2) / 8] & (0x80 >> ((x / 2) % 8)))
              tft.drawPixel(lx + x, ly + y, 1);
      }
    }
    char nombre[48]; snprintf(nombre, sizeof(nombre), "/frame_%02u.pgm", f);
    guardar(tft, std::string(argv[1]) + nombre);
  }
  puts("OK: intro, 50 cuadros Willys, logo, cierre y offsets de recuperacion.");
}
