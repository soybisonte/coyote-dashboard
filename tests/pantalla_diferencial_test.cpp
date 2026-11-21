// Usa Adafruit GFX real para comprobar que las transiciones no borran la TFT.
// Compilar con tools/host_gfx y Adafruit_GFX_Library; ver tools/preview_arranque_coyote.cpp.
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <vector>
#include <Adafruit_GFX.h>
#include "../XJ_Mando_TFT/Interfaz_OLED.h"
#include "../XJ_Mando_TFT/Pantalla_Diferencial.h"

struct PantallaSimulada {
  // No ofrece fillScreen: el presentador no puede introducir un borrado visible.
  struct Ventana { int x, y, ancho, alto; } actual{};
  std::vector<Ventana> ventanas;
  std::vector<uint16_t> pixeles = std::vector<uint16_t>(320 * 240, 0x1234);
  unsigned inicios = 0, finales = 0;
  void startWrite() { ++inicios; }
  void endWrite() { ++finales; }
  void setAddrWindow(int x, int y, int ancho, int alto) {
    assert(x >= 0 && x + ancho <= 320 && y >= 0 && y < 240 && alto == 1);
    actual = {x, y, ancho, alto}; ventanas.push_back(actual);
  }
  void writePixels(uint16_t *datos, int longitud) {
    assert(longitud == actual.ancho);
    std::copy(datos, datos + longitud, pixeles.begin() + actual.y * 320 + actual.x);
  }
  void comprobar(GFXcanvas1 &canvas) const {
    for (int y = 0; y < 240; ++y)
      for (int x = 0; x < 320; ++x)
        assert(pixeles[y * 320 + x] == (canvas.getPixel(x, y) ? 0xFFFF : 0));
  }
};

void reles(GFXcanvas1 &canvas, const char *r1, bool conocido, bool encendido) {
  canvas.fillScreen(0);
  OledUi::cabecera(canvas, "COYOTE / RELEVADORES", "VOLVER");
  OledUi::texto(canvas, 8, 34, "C5 EN LINEA / 4 CANALES");
  OledUi::rele(canvas, 0, r1, conocido, encendido);
  for (int i = 1; i < 4; ++i) OledUi::rele(canvas, i, "APAGADO", true, false);
}

int main() {
  GFXcanvas1 canvas(320, 240);
  canvas.setTextWrap(false);
  PantallaDiferencial presentador;
  PantallaSimulada pantalla;
  reles(canvas, "APAGADO", true, false);
  auto resultado = presentador.presentar(pantalla, canvas.getBuffer());
  assert(resultado.pixeles == 320 * 240 && resultado.ventanas == 240);
  pantalla.comprobar(canvas);

  const auto primera = pantalla.pixeles;
  uint32_t totalBoton = 0;
  for (bool confirmacion : {false, true}) {
    pantalla.ventanas.clear();
    reles(canvas, confirmacion ? "ENCENDIDO" : "CONFIRMANDO...", confirmacion, true);
    resultado = presentador.presentar(pantalla, canvas.getBuffer());
    assert(resultado.pixeles > 0 && resultado.pixeles < 10000);
    totalBoton += resultado.pixeles;
    for (const auto &v : pantalla.ventanas)
      assert(v.x >= 8 && v.x + v.ancho <= 152 && v.y >= 58 && v.y < 141);
    pantalla.comprobar(canvas);
  }
  for (int y = 0; y < 240; ++y)
    for (int x = 0; x < 320; ++x)
      if (x < 8 || x >= 152 || y < 58 || y >= 141)
        assert(pantalla.pixeles[y * 320 + x] == primera[y * 320 + x]);

  const auto inicios = pantalla.inicios;
  resultado = presentador.presentar(pantalla, canvas.getBuffer());
  assert(resultado.pixeles == 0 && resultado.ventanas == 0 && pantalla.inicios == inicios);

  // VOLVER: dibujar la pagina nueva completa en RAM, luego presentar diferencias.
  canvas.fillScreen(0);
  OledUi::cabecera(canvas, "COYOTE / TELEMETRIA", "RELES");
  OledUi::texto(canvas, 8, 34, "C6 OK / OBD OK");
  const char *labels[] = {"RPM / MOTOR", "VOLTAJE / OBD", "TEMP. / MOTOR", "GASOLINA"};
  const char *values[] = {"850", "14.1", "92", "68"};
  const char *units[] = {"RPM", "V", "C", "%"};
  for (int i = 0; i < 4; ++i) {
    OledUi::etiquetaMedidor(canvas, i, labels[i]);
    OledUi::medidor(canvas, i, values[i], units[i], true, .5f);
  }
  resultado = presentador.presentar(pantalla, canvas.getBuffer());
  const uint32_t alVolver = resultado.pixeles;
  assert(alVolver > 0 && alVolver < 320 * 240);
  pantalla.comprobar(canvas);
  reles(canvas, "ENCENDIDO", true, true);
  presentador.presentar(pantalla, canvas.getBuffer());
  pantalla.comprobar(canvas);

  presentador.invalidar();  // Reentrada desde un splash escrito directamente.
  resultado = presentador.presentar(pantalla, canvas.getBuffer());
  assert(resultado.pixeles == 320 * 240);
  assert(presentador.presentar(pantalla, nullptr).pixeles == 0);
  assert(pantalla.inicios == pantalla.finales);
  printf("OK: boton+ACK %u pixeles; VOLVER %u; sin borrados ni cambios fuera del rele.\n",
      totalBoton, alVolver);
}
