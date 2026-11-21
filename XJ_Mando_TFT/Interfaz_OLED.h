#pragma once
#include <stdint.h>
#include <string.h>

// El mismo lenguaje de las SSD1306: fuente clasica GFX, blanco sobre negro,
// numeros grandes, etiquetas pequenas y barras sin antialias ni tonos grises.
namespace OledUi {
constexpr uint16_t negro = 0x0000, blanco = 0xFFFF;

template <typename Pantalla>
void texto(Pantalla &p, int x, int y, const char *cadena, uint8_t tamano = 1,
           uint16_t color = blanco) {
  p.setTextSize(tamano);
  p.setTextColor(color);
  p.setCursor(x, y);
  p.print(cadena);
}

template <typename Pantalla>
void navegacion(Pantalla &p, const char *etiqueta) {
  p.drawRect(240, 3, 72, 24, blanco);
  texto(p, 240 + (72 - strlen(etiqueta) * 6) / 2, 11, etiqueta);
}

template <typename Pantalla>
void cabecera(Pantalla &p, const char *titulo, const char *boton) {
  texto(p, 8, 11, titulo);
  navegacion(p, boton);
  p.drawFastHLine(8, 48, 304, blanco);
}

template <typename Pantalla>
void etiquetaMedidor(Pantalla &p, uint8_t indice, const char *etiqueta) {
  texto(p, 8 + (indice % 2) * 160, 54 + (indice / 2) * 92, etiqueta);
}

template <typename Pantalla>
void medidor(Pantalla &p, uint8_t indice, const char *valor, const char *unidad,
             bool valido, float proporcion) {
  const int x = 8 + (indice % 2) * 160, y = 54 + (indice / 2) * 92;
  p.fillRect(x, y + 12, 144, 72, negro);
  const uint8_t tamano = strlen(valor) <= 5 ? 4 : strlen(valor) <= 7 ? 3 : 1;
  texto(p, x, y + 18, valor, tamano);
  texto(p, x + 144 - strlen(unidad) * 6, y + 43, unidad);
  p.drawRect(x, y + 70, 144, 10, blanco);
  if (valido) {
    if (proporcion < 0) proporcion = 0;
    if (proporcion > 1) proporcion = 1;
    p.fillRect(x + 2, y + 72, static_cast<int>(proporcion * 140), 6, blanco);
  }
}

template <typename Pantalla>
void boton(Pantalla &p, int x, int y, int ancho, int alto, const char *etiqueta,
           bool seleccionado, uint8_t tamano = 1) {
  p.fillRect(x, y, ancho, alto, seleccionado ? blanco : negro);
  p.drawRect(x, y, ancho, alto, blanco);
  texto(p, x + (ancho - strlen(etiqueta) * 6 * tamano) / 2,
      y + (alto - 8 * tamano) / 2, etiqueta, tamano,
      seleccionado ? negro : blanco);
}

template <typename Pantalla>
void rele(Pantalla &p, uint8_t indice, const char *estado, bool conocido,
          bool encendido) {
  const int x = 8 + (indice % 2) * 160, y = 58 + (indice / 2) * 88;
  p.drawRect(x, y, 144, 83, blanco);
  char titulo[] = "RELE 1";
  titulo[5] += indice;
  texto(p, x + 6, y + 6, titulo);
  texto(p, x + 6, y + 22, estado, strlen(estado) <= 11 ? 2 : 1);
  boton(p, x + 4, y + 47, 64, 32, "ON", conocido && encendido);
  boton(p, x + 76, y + 47, 64, 32, "OFF", conocido && !encendido);
}
}  // namespace OledUi
