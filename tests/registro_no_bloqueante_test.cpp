// Regresion del bloqueo de USB: usar el helper real con un puerto saturado.
#include <cassert>
#include <string>
#include "../XJ_Relevadores_C5/Registro_NoBloqueante.h"

struct PuertoSaturable {
  int espacio = 0, escrituras = 0;
  std::string texto;
  int availableForWrite() { return espacio; }
  size_t write(const uint8_t *datos, size_t longitud) {
    ++escrituras;
    // Esta llamada representaria la espera de HWCDC si no hay espacio.
    assert(static_cast<int>(longitud) <= espacio);
    texto.append(reinterpret_cast<const char*>(datos), longitud);
    espacio -= longitud;
    return longitud;
  }
};

int main() {
  PuertoSaturable puerto;
  for (int orden = 0; orden < 10000; ++orden)
    assert(!Registro::escribir(puerto, "Orden %d aplicada\n", orden));
  assert(puerto.escrituras == 0);  // Sin consumidor USB, nunca intentar escribir.
  puerto.espacio = 4;
  assert(!Registro::escribir(puerto, "Mensaje demasiado largo\n"));
  assert(puerto.escrituras == 0);
  puerto.espacio = 64;
  assert(Registro::escribir(puerto, "R%u orden=%lu OFF\n", 2U, 123UL));
  assert(puerto.texto == "R2 orden=123 OFF\n");
  puerto.espacio = 4096;
  const std::string largo(300, 'x');
  assert(!Registro::escribir(puerto, "%s", largo.c_str()));
  assert(puerto.escrituras == 1);  // No emitir mensajes truncados.
  puts("OK: 10000 logs sin lector USB descartados; recuperacion al volver el lector.");
}
