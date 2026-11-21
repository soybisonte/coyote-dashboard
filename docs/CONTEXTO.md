# Contexto vigente · COYOTE

Este archivo describe el estado final de referencia, no un registro de intentos.
Fecha de corte: **21 de noviembre de 2026**. El prototipo final se cargó el día 20.

- C6 de 8 MB, seis OLED SSD1306 por TCA9548A 0x70, SDA18/SCL19.
- Mapa físico: arriba 0/3/4; abajo 5/6/7. Superiores a 180°; inferiores normales.
- Contenido: arriba estado OBD/Mal Camino/refrigerante; abajo RPM/voltaje/gasolina.
- TFT ST7789, dos USB, XPT2046; UI blanca sobre negro sin la antigua prueba LED.
- C5_Relay_X4, cuatro salidas GPIO7/8/9/10, HIGH=ON, confirmadas por clic y LED.
- Radio ESP-NOW 2.4 GHz canal 1; no Wi-Fi de router ni servidor.
- C6 recibe ELM327 por BLE de Veepeak y transmite cuatro valores a la TFT.
- Intro Mal Camino → Willys de cinco rayos → logo Willys → dashboard.
- La TFT usa canvas monocromo y actualiza solo diferencias para evitar saltos.
- El C5 usa logs sin bloqueo y timeout TX USB cero; debe funcionar sin monitor.
- Las MAC se personalizan en esta edición pública; las originales no se incluyen.
- Las salidas se distribuyen bloqueadas hasta declarar el montaje propio.

## Lo que no debes asumir

No hay ocho relevadores implementados ni entradas de un módulo externo
verificadas. No se mide combustible con sensor independiente. El voltaje
es ATRV del adaptador, no una medición directa de batería hecha por el C6.
No hay plano de PCB de seis OLED listo para fabricar ni alimentación 12 V
validada para el conjunto. El LCD integrado del C6 no se usa.

Las rutas XJ son históricas: no cambiar firmas de paquetes ni renombrar las
carpetas Arduino para sustituir texto visible. OBD v3 mide 32 bytes;
relevadores v1, 20 bytes; splash v2, 12 bytes. El GPIO leído no verifica contactos.

## Qué contiene esta publicación

Los cuatro sketches seleccionados, encabezados, herramientas y pruebas
provienen del proyecto construido. La adaptación pública concentra las MAC
en tres headers iguales, añade un configurador, bloquea relés por defecto
y reemplaza rutas personales por instrucciones portables. No se publica
el historial de Git del proyecto privado ni sus capturas de consola/flash.

Para empezar: [README](../README.md), [tutorial](04-tutorial.md) y
[estado de validación](07-validacion.md). El estado de cada persona vive en
`bitacora/estado.local.md`, no en este contexto común.
