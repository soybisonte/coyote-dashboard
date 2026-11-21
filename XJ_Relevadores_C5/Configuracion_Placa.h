#pragma once

// ESP32-C5_Relay_X4 (303E32C5DC1), MAC propia en Configuracion_Radio.h.
// GPIO recuperados del firmware de fabrica del prototipo validado.
// Su setup configura 7/8/9/10 como salidas y los deja LOW; loop alterna HIGH/LOW.
// HIGH=ON y correspondencia 1..4 confirmados por el usuario: clic y LED de
// los cuatro relevadores responden a ON/OFF del TFT (19 de noviembre de 2026).
// No se ha medido continuidad en los contactos ni conectado cargas.
// false vuelve a diagnostico sin configurar ningun GPIO de relevador.
constexpr bool configuracionHabilitada = false;
constexpr int pinesRelevador[4] = {7, 8, 9, 10};
constexpr bool activoEnAlto[4] = {true, true, true, true};
