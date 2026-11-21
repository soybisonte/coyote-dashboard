#pragma once
#include <stdint.h>

namespace ConfiguracionOled {
// Un ajuste por canal fisico del TCA9548A, independiente de su posicion.
// 0 = montaje normal; 2 = pantalla montada de cabeza (giro de 180 grados).
// Montaje previsto: dos filas de tres, superiores a 180 grados.
// Mapa confirmado con etiquetas en IMG_5265: arriba CH0, CH3, CH4;
// abajo CH5, CH6, CH7. Vista de frente, USB a la derecha.
constexpr uint8_t rotacionPorCanal[8] = {
  2,  // CH0: izquierda superior / estado OBD
  0,  // CH1
  0,  // CH2
  2,  // CH3: centro superior / Mal Camino
  2,  // CH4: derecha superior / refrigerante
  0,  // CH5: izquierda inferior / RPM
  0,  // CH6: centro inferior / voltaje
  0,  // CH7: derecha inferior / gasolina
};

constexpr bool rotacionesValidas() {
  for (const uint8_t giro : rotacionPorCanal)
    if (giro != 0 && giro != 2) return false;
  return true;
}
static_assert(rotacionesValidas(), "Usar giro 0 o 2 para OLED horizontales de 128x64");

template <typename Pantalla>
void orientar(Pantalla &pantalla, uint8_t canal) {
  pantalla.setRotation(canal < 8 ? rotacionPorCanal[canal] : 0);
}
}  // namespace ConfiguracionOled
