# 1. Componentes

| Cantidad | Componente | Qué se probó / qué debes verificar |
| --- | --- | --- |
| 1 | ESP32-C6, flash 8 MB | Prototipo ESP32-C6-LCD-1.47, chip C6FH8; usa IO18/IO19. Su LCD integrada no se usa |
| 6 | OLED SSD1306 I2C 128×64 | Módulos de cuatro pines a 3.3 V; prototipo 0x3C. El código contempla 0x3D |
| 1 | Multiplexor TCA9548A | Dirección 0x70; seis canales usados |
| 1 | ESP32-2432S028 con TFT y touch | **Variante de dos USB con ST7789**, 4 MB, sin PSRAM; touch XPT2046 |
| 1 | ESP32-C5_Relay_X4 (303E32C5DC1) | Chip C5, 4 MB; GPIO 7/8/9/10, activación HIGH confirmada en esa placa |
| 1 | Adaptador Veepeak OBD2 BLE | Transporte ELM327 por BLE; no confundir con uno exclusivo de Bluetooth Classic |
| Según montaje | Protoboard, jumpers y cables USB de datos | Cableado por etiqueta, GND común en el conjunto C6/TCA/OLED |
| Opcional | Separadores y enclosure | El frente final es una matriz de 3 columnas × 2 filas |

La familia de nombres ESP32-2432S028 tiene variantes: que una publicación diga
“CYD” no confirma el controlador ni el pinout. Comprueba ST7789 y los pines
antes de cargar este sketch. Un módulo SH1106 tampoco equivale a SSD1306.

La placa C5 de referencia fue adquirida con [ASIN B0HK35LFR1](https://www.amazon.com.mx/dp/B0HK35LFR1).
El enlace identifica la compra, sin garantizar disponibilidad ni que otro lote
conserve el circuito. El mapa se recuperó del programa de fábrica y se comprobó
observando clic y LED en los cuatro relevadores, sin cargas.

No necesitas router ni cuenta de nube para operar el dashboard. Las tres
placas se comunican por ESP-NOW en 2.4 GHz, canal 1. El Veepeak se comunica
con el C6 por BLE y se alimenta del puerto OBD del vehículo.

**No forma parte de esta versión:** módulo externo de relevadores sin ESP32,
PCB integrada de fabricación, lectura directa del aforador, sensores nuevos
o alimentación automotriz del conjunto. No se han validado esas ampliaciones.

Siguiente: [conexiones](02-conexiones.md).
