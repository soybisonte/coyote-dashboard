# 2. Conexiones

Haz el montaje sin alimentación y conecta por **etiqueta**, no por la posición
izquierda/derecha de los pines: la fila superior quedará físicamente de cabeza.
Este tutorial se limita a la prueba sobre mesa por USB, sin cargas en los relés.

## C6 → TCA9548A

| C6 | TCA9548A |
| --- | --- |
| 3V3 | VIN/VCC de un módulo compatible con 3.3 V |
| GND | GND |
| IO18 | SDA de entrada |
| IO19 | SCL de entrada |

El TCA debe responder en **0x70**. Verifica la configuración A0/A1/A2 de tu módulo.
Todas las OLED comparten 3V3 y GND con el TCA y el C6. La alimentación de las
OLED no pasa por SDn/SCn: esos son datos y reloj de cada canal.

## TCA → cada OLED

Conecta SDA de cada OLED a **SDn** y SCL a **SCn** del mismo canal n.

| Posición de frente, USB a la derecha | Canal | Contenido | Giro de software |
| --- | --- | --- | --- |
| Superior izquierda | CH0 | Estado de conexión OBD2 | 180° (`2`) |
| Superior centro | CH3 | Mal Camino | 180° (`2`) |
| Superior derecha | CH4 | Refrigerante | 180° (`2`) |
| Inferior izquierda | CH5 | RPM | Normal (`0`) |
| Inferior centro | CH6 | Voltaje OBD | Normal (`0`) |
| Inferior derecha | CH7 | Gasolina | Normal (`0`) |

CH1 y CH2 quedan libres. Seis OLED a 0x3C en esos canales producen la máscara
**0xF9** en la prueba de identificación. No pongas dos OLED con la misma
dirección en un único canal ni conectes una OLED al bus principal del TCA.

```text
             VISTA FRONTAL
  ┌────────────┬────────────┬────────────┐
  │ CH0 / OBD  │ CH3 / logo │ CH4 / temp │  giro 180°
  ├────────────┼────────────┼────────────┤     USB →
  │ CH5 / RPM  │ CH6 / volt │ CH7 / gas  │  giro normal
  └────────────┴────────────┴────────────┘
```

## TFT y C5

No llevan cables de señal hacia el C6. Se alimentan por su propio USB.
La TFT y el touch ya están integrados en la placa compatible; los siguientes
son **pines internos usados por el sketch**, no puentes que debas añadir:

| Función TFT | Pines |
| --- | --- |
| ST7789 | SCLK14, MISO12, MOSI13, CS15, DC2, backlight21 |
| XPT2046 | SCLK25, MISO39, MOSI32, CS33 |

En la placa C5 validada, R1/R2/R3/R4 corresponden a GPIO7/8/9/10, HIGH=ON.
Deja sus contactos COM/NO/NC vacíos para las pruebas. No conectes 12 V a los
GPIO ni supongas el mismo mapa en otra tarjeta. La expansión externa queda
pendiente de verificar entradas, alimentación y adaptación de niveles.

Siguiente: [preparación](03-preparacion.md).
