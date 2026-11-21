# 6. Diagnóstico con una prueba concreta

| Síntoma | Comprobación y siguiente paso |
| --- | --- |
| Ninguna OLED enciende | Desconecta USB y revisa GND y 3V3 comunes. Hubo un GND suelto real durante el montaje; no asumir daño ni culpar al sketch |
| Una OLED apagada | Carga la prueba de etiquetas y revisa qué canal/dirección responde. Comprueba SDn/SCn del mismo canal y sus cuatro etiquetas |
| CH0–CH7 no coinciden con la disposición | Lee los números de las seis pantallas. Ajusta posición y giro por canal; no hace falta usar canales consecutivos |
| TCA no responde en 0x70 | Verifica SDA18/SCL19, dirección A0/A1/A2 y alimentación; no cambiar GPIO a ciegas |
| Texto invertido | En `Configuracion_OLED.h`, `0` es normal y `2` gira 180°. Reaplicar por canal evita heredar el giro del panel anterior |
| Ruido o bandas | Repite identificación, anota si responde estable y revisa conexiones. No es un efecto normal del giro. La prueba de mapa usa 100 kHz; el dashboard transfiere a 400 kHz |
| C6 está BUSCANDO | Con Veepeak ausente es esperado. Verifica BLE, nombre/dirección configurada y que otra app no lo ocupe |
| ESPERANDO ECU | El enlace BLE puede estar conectado sin que la ECU responda. El perfil ELM y el estado del vehículo deben permitir lectura |
| Gasolina NO DISP. OBD | La ECU no anuncia PID 2F. Es lo observado en el Jeep original; conservar esa indicación |
| C5 sin señal en TFT | Comparar MAC, canal 1 y revisión de protocolo en ambas placas. La plantilla sin configurar no sirve para enlazar equipos |
| C5 comunica pero salidas bloqueadas | Revisar configuración local y MAC propia del C5. El bloqueo inicial es intencional |
| Se pierde señal al cerrar monitor C5 | Confirmar `Serial.setTxTimeoutMs(0)` y `Registro_NoBloqueante.h`; no aumentar el tiempo de vigencia para ocultarlo |
| RELES/VOLVER parpadea | Verificar el firmware con `GFXcanvas1` y `Pantalla_Diferencial.h`; no volver a dibujar borrando la TFT física |
| Touch no corresponde | Leer coordenadas raw y ajustar mínimos/máximos para la variante probada; no cambiar controlador de pantalla sin identificarlo |
| Carga TFT falla a velocidad alta | Usar 115200 y cable de datos. Comprobar puerto/chip y cerrar el monitor |

La TFT vence la telemetría C6 a 1.5 s y el estado C5 a 3 s. El C5 publica
cada 500 ms; las órdenes tienen hasta tres intentos a intervalos de 200 ms.
Reiniciar el C5 cambia la sesión y cancela órdenes anteriores.

Al modificar algo, prueba primero el caso que fallaba y registra el resultado.
No conectes el módulo externo de 12 V para solucionar un problema del C5:
no es parte de esta versión y su entrada no se ha verificado a 3.3 V.
