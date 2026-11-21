#pragma once
#include <stdint.h>
#include "Configuracion_Radio.h"

namespace Relevadores {
constexpr uint32_t firma = 0x584A5231;
constexpr uint8_t version = 1, cantidad = 4, todos = 0x0F;
constexpr uint8_t listo = 1;
constexpr auto &macReceptor = ConfiguracionRadio::macC5;
constexpr auto &macTft = ConfiguracionRadio::macTft;
constexpr uint8_t canalRadio = 1;
enum class Tipo : uint8_t { Orden = 1, Confirmacion = 2, Estado = 3 };
struct __attribute__((packed)) Paquete {
  uint32_t firma;
  uint32_t sesion;  // Cambia al reiniciar el receptor; rechaza ordenes de otro arranque.
  uint32_t id;
  uint8_t version;
  Tipo tipo;
  uint8_t canal;    // 0..3; 255 para estado general.
  uint8_t encendido;
  uint8_t estados;  // Mascara de niveles GPIO aplicados, NO de contactos fisicos.
  uint8_t flags;    // Bit 0: configuracion de salidas habilitada; no verifica contactos.
  uint16_t reservado;
};
static_assert(sizeof(Paquete) == 20, "Formato de relevadores inesperado");

inline bool valido(const Paquete &p) {
  if (p.firma != firma || p.version != version || !p.sesion || p.reservado ||
      p.encendido > 1 || (p.estados & ~todos) || (p.flags & ~listo)) return false;
  if (p.tipo == Tipo::Estado) return p.canal == 255 && p.id == 0;
  return (p.tipo == Tipo::Orden || p.tipo == Tipo::Confirmacion) && p.canal < cantidad;
}
inline bool coincide(const Paquete &respuesta, const Paquete &orden) {
  return valido(respuesta) && respuesta.tipo == Tipo::Confirmacion &&
      (respuesta.flags & listo) && respuesta.sesion == orden.sesion &&
      respuesta.id == orden.id && respuesta.canal == orden.canal &&
      respuesta.encendido == orden.encendido &&
      bool(respuesta.estados & (1U << orden.canal)) == bool(orden.encendido);
}
}  // namespace Relevadores
