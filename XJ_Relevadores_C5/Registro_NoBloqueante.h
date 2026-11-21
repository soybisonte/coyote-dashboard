#pragma once
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>

namespace Registro {
// La consola es opcional. Nunca esperar a que una Mac lea el USB para
// atender ordenes o publicar estados. Configurar ademas TX timeout=0.
template <typename Puerto>
bool escribir(Puerto &puerto, const char *formato, ...) {
  char linea[192];
  va_list argumentos;
  va_start(argumentos, formato);
  const int longitud = vsnprintf(linea, sizeof(linea), formato, argumentos);
  va_end(argumentos);
  if (longitud <= 0 || longitud >= static_cast<int>(sizeof(linea)) ||
      puerto.availableForWrite() < longitud) return false;
  return puerto.write(reinterpret_cast<const uint8_t*>(linea), longitud) ==
      static_cast<size_t>(longitud);
}
}  // namespace Registro
