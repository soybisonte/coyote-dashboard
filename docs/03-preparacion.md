# 3. Preparar herramientas y configurar tu equipo

Los comandos siguientes se ejecutan desde la raíz del repositorio.
En Windows usa `py` si tu instalación no ofrece `python3`; los puertos serán
`COMn` en lugar de `/dev/cu.*` o `/dev/tty*`.

## Herramientas y versiones

Instala [Arduino IDE](https://www.arduino.cc/en/software/) o
[Arduino CLI](https://docs.arduino.cc/arduino-cli/installation/), Python 3.10+
y Git si vas a clonar. Arduino IDE permite abrir cada `.ino` directamente;
no saques el archivo de su carpeta porque los headers forman parte del sketch.

La referencia es **esp32 by Espressif Systems 3.3.10**. No hace falta instalar
NimBLE-Arduino por separado: se usa BLE del core. Biblioteca y versión:

| Biblioteca | Versión |
| --- | --- |
| Adafruit BusIO | 1.17.4 |
| Adafruit GFX Library | 1.13.0 |
| Adafruit SSD1306 | 2.5.17 |
| Adafruit ST7735 and ST7789 Library | 1.12.0 |
| XPT2046_Touchscreen | 1.4 |

En IDE añade el [índice estable de Espressif](https://espressif.github.io/arduino-esp32/package_esp32_index.json)
al gestor de placas e instala la versión indicada. Es el método descrito
por [Espressif](https://docs.espressif.com/projects/arduino-esp32/en/latest/installing.html).
Con CLI, este script instala las mismas versiones; descarga archivos e
instala dependencias, pero **no carga ninguna placa**:

```sh
python3 tools/preparar_entorno.py
```

Si `arduino-cli` no está en PATH, añade `--cli RUTA_AL_EJECUTABLE`.
Las versiones y FQBN están en [tools/entorno.json](../tools/entorno.json).

## Identificar puertos y MAC

Conecta una placa cada vez y compara la lista antes/después:

```sh
arduino-cli board list
```

Para leer su identidad sin reemplazar la aplicación, crea un entorno Python
y usa esptool. La lectura reinicia la placa mediante el cargador ROM, pero
no escribe su flash:

```sh
python3 -m venv .venv
# macOS/Linux:
source .venv/bin/activate
# PowerShell, en lugar de la línea anterior: .venv\Scripts\Activate.ps1
python3 -m pip install -r requirements-herramientas.txt
python3 -m esptool --port PUERTO read-mac
```

Registra **BASE MAC / Wi-Fi STA de seis bytes** para C6, TFT y C5. C6/C5
pueden mostrar otra dirección extendida de ocho bytes con FF:FE: no la copies.
Cierra el monitor serie antes de leer o cargar. No elijas una placa solo por
su nombre de puerto: verifica también chip y tamaño de flash.

## Configurar las tres placas

Copia `config/equipo.ejemplo.json` como `config.local.json`. Llena
`mac_c6`, `mac_tft` y `mac_c5` con tus direcciones. `mac_obd_ble` puede quedar
vacía cuando solo haya un VEEPEAK compatible visible; si conoces su dirección
BLE puedes fijarla para preferirlo. No inventes una MAC del adaptador.

Deja inicialmente `habilitar_relevadores: false`. Después de verificar la
placa exacta, y con contactos sin cargas, ponlo en `true` para probar los clics.

```sh
python3 tools/configurar_equipo.py --comprobar
python3 tools/configurar_equipo.py
python3 tools/verificar.py
```

El configurador valida las MAC, exige que sean distintas y escribe las tres
copias de `Configuracion_Radio.h`. También actualiza el bloqueo de las salidas
C5. El firmware verifica que la MAC local del C5 coincida con la configurada;
si no coincide, no habilita salidas. No desactives ese control para “hacerlo funcionar”.
`config.local.json` y la bitácora personal están excluidos de Git.

## Selección de placa

| Equipo | IDE | Opciones |
| --- | --- | --- |
| C6 | ESP32C6 Dev Module | Flash 8 MB, Default 8MB, USB CDC On Boot Enabled |
| TFT | ESP32 Dev Module | Flash 4 MB, Default, PSRAM Disabled, carga 115200 |
| C5 | ESP32C5 Dev Module | Flash 4 MB, Default, USB CDC On Boot Enabled |

Monitor: **115200** en los tres. Compilar no equivale a cargar:

```sh
python3 tools/compilar.py todos
```

Siguiente: [puesta en marcha](04-tutorial.md).
