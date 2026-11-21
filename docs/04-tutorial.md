# 4. Tutorial de puesta en marcha

Avanza una etapa cada vez. Anota el resultado observable antes de seguir;
si falla, usa el [diagnóstico](06-diagnostico.md). Mantén vacíos los contactos
de los relevadores durante todo este tutorial.

## A. Identifica las seis OLED

Conecta el conjunto C6/TCA/OLED por USB. Compila y carga la prueba temporal:

```sh
python3 tools/compilar.py mapa
arduino-cli upload --port PUERTO_C6 --fqbn esp32:esp32:esp32c6:CDCOnBoot=cdc,FlashSize=8M,PartitionScheme=default_8MB --input-dir build/mapa Prueba_Mapa_OLED_C6
arduino-cli monitor --port PUERTO_C6 --config baudrate=115200
```

Cada OLED muestra su canal dos veces, uno en cada sentido. Espera ver arriba
CH0–CH3–CH4 y abajo CH5–CH6–CH7. El registro debe detectar seis dispositivos,
`canales=0xF9`. El punto cambia en cada escaneo de tres segundos.

Si no coincide, puedes recablear con USB desconectado o adaptar
`panelPositions` y `Configuracion_OLED.h` a tu montaje. Los giros deben
aplicarse antes de dibujar **cada canal**: comparten objetos SSD1306.
No deduzcas el canal por el indicador de un firmware viejo.

## B. Carga el dashboard C6

Cierra el monitor y carga el firmware completo:

```sh
python3 tools/compilar.py c6
arduino-cli upload --port PUERTO_C6 --fqbn esp32:esp32:esp32c6:CDCOnBoot=cdc,FlashSize=8M,PartitionScheme=default_8MB --input-dir build/c6 XJ_Dashboard_C6
```

Debe aparecer Mal Camino arriba al centro, luego el recorrido del Willys,
el logo Willys y finalmente el dashboard en el orden de la tabla del README.
Las tres superiores se leen derechas aunque sus módulos estén invertidos.
Sin Veepeak/ECU se esperan `BUSCANDO`, `--` y `SIN OBD`, no datos simulados.

## C. Prueba el mando y los relevadores

Primero carga el C5 con las salidas bloqueadas y tus MAC configuradas:

```sh
python3 tools/compilar.py c5
arduino-cli upload --port PUERTO_C5 --fqbn esp32:esp32:esp32c5:CDCOnBoot=cdc --input-dir build/c5 XJ_Relevadores_C5
python3 tools/compilar.py tft
arduino-cli upload --port PUERTO_TFT --fqbn esp32:esp32:esp32:FlashSize=4M,PartitionScheme=default,PSRAM=disabled --upload-property upload.speed=115200 --input-dir build/tft XJ_Mando_TFT
```

La TFT presenta las cuatro mediciones. **RELES** muestra la página de cuatro
canales; una indicación de bloqueo en esta fase es esperada. Comprueba que
los toques correspondan con los botones. La calibración de referencia es
200–3800 en ambos ejes; ajústala solo si tu unidad lo necesita.

Verificada la placa C5 exacta, cambia `habilitar_relevadores` a `true`,
ejecuta otra vez el configurador y recompila/carga **el C5**. Si cambiaste
MAC, recompila/carga las tres placas. Prueba R1 ON/OFF y repite en R2–R4;
LED y clic deben coincidir con su número. Reinicia el C5 y verifica OFF.

Cierra el monitor serie del C5 y repite varias pulsaciones y RELES/VOLVER.
No debe aparecer `sin señal` ni un borrado completo de la TFT. Esta prueba
es necesaria porque hubo un fallo que solo aparecía con la consola cerrada.

## D. Conecta el OBD2

Con el vehículo estacionado y en condiciones apropiadas para leer su ECU,
conecta el Veepeak al OBD. Evita otra app conectada al mismo adaptador BLE.
El C6 pasa por `BUSCANDO → CONECTANDO → ESPERANDO ECU → EN LINEA`.
No se envían órdenes de actuadores ni borrado de códigos por OBD.

RPM, refrigerante y gasolina provienen de sus PID si la ECU los soporta.
Voltaje usa **ATRV del adaptador OBD**, no un ADC del ESP32. En el Jeep
probado no estaba disponible el PID de gasolina: `NO DISP. OBD` es correcto
y no debe reemplazarse por un porcentaje inventado.

## E. Cierra la instalación de prueba

Revisa las seis posiciones, el arranque y la recepción en el TFT. En los
relevadores, el ACK confirma el nivel GPIO, no continuidad ni estado físico
de los contactos. Si el TFT pierde señal, el C5 **mantiene el último estado**;
al reiniciar arranca OFF. Cablear cargas requiere un diseño específico
fuera del alcance de estas pruebas por USB.

Guarda en tu bitácora qué comprobaste en hardware, qué solo compiló y qué
quedó pendiente. Para las próximas sesiones usa la [guía de agentes](05-guia-agentes.md).
