#pragma once
#include <stdint.h>
#include <string.h>

// Presentar un canvas monocromo ya terminado, sin borrar la TFT visible.
// La copia anterior permite enviar solo tramos de 8 pixeles que cambiaron.
class PantallaDiferencial {
public:
  static constexpr int ancho = 320, alto = 240, paso = ancho / 8;
  struct Resultado { uint32_t pixeles = 0, ventanas = 0; };

  void invalidar() { conocida = false; }

  template <typename Pantalla>
  Resultado presentar(Pantalla &pantalla, const uint8_t *imagen) {
    Resultado resultado;
    if (!imagen) return resultado;
    uint16_t linea[ancho];
    bool transaccion = false;
    for (int y = 0; y < alto; ++y) {
      const int fila = y * paso;
      int inicio = 0;
      while (inicio < paso) {
        if (conocida && imagen[fila + inicio] == anterior[fila + inicio]) {
          ++inicio;
          continue;
        }
        int fin = inicio + 1;
        while (fin < paso && (!conocida || imagen[fila + fin] != anterior[fila + fin])) ++fin;
        const int longitud = (fin - inicio) * 8;
        for (int x = 0; x < longitud; ++x)
          linea[x] = imagen[fila + inicio + x / 8] & (0x80 >> (x % 8)) ? 0xFFFF : 0x0000;
        if (!transaccion) {
          pantalla.startWrite();
          transaccion = true;
        }
        pantalla.setAddrWindow(inicio * 8, y, longitud, 1);
        pantalla.writePixels(linea, longitud);
        resultado.pixeles += longitud;
        ++resultado.ventanas;
        inicio = fin;
      }
    }
    if (transaccion) pantalla.endWrite();
    memcpy(anterior, imagen, sizeof(anterior));
    conocida = true;
    return resultado;
  }

private:
  uint8_t anterior[paso * alto] = {};
  bool conocida = false;
};
