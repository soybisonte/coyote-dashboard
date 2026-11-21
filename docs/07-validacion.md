# 7. Qué está comprobado

## Prototipo original

- Seis OLED detectadas en CH0/3/4/5/6/7, dirección 0x3C.
- Montaje superior invertido, con corrección individual de 180°.
- Firmware del C6 cargado con el orden final pedido: OBD/logo/refrigerante;
  RPM/voltaje/gasolina. Arranque completo y envío de telemetría sin errores
  durante la comprobación final del 20 de noviembre de 2026.
- Cuatro relevadores C5: orden, LED y clic confirmados por la persona.
- Fallo USB corregido: 55 s de prueba con monitor C5 cerrado, 30 órdenes
  confirmadas al primer intento; edad máxima de estado muestreada 440 ms.
- La persona confirmó la fluidez del TFT en ON/OFF y RELES/VOLVER después
  de introducir el canvas y el presentador por diferencias.
- Lecturas OBD reales de RPM y refrigerante en una prueba anterior con ECU;
  gasolina no disponible en el Jeep usado. La última prueba de escritorio
  no tenía Veepeak/ECU disponibles.

## Esta edición pública

Se preservó la lógica del firmware final y se adaptaron únicamente la
configuración de identidades, el bloqueo inicial y el material de reproducción.
Las comprobaciones de publicación se registran en [REVISION.md](../REVISION.md).
El repositorio no instala firmware en ninguna placa al clonarlo o compilarlo.

Las pruebas host verifican paquetes/ACK/reintentos/sesiones, logs sin bloqueo
y la imagen final del presentador TFT. No sustituyen a radio, touch, contactos
ni ECU reales. Cada montaje nuevo necesita completar el tutorial en su hardware.

## Fuera de lo comprobado

No se certificó continuidad/corriente de los contactos, ni ausencia de pulsos
eléctricos durante todas las fases de alimentación/reset. No se han integrado
relés externos, alimentación de automóvil, fusibles/cableado de accesorios,
una PCB de seis OLED ni un sensor independiente de combustible. Ninguna
confirmación de GPIO debe describirse como confirmación física de la carga.
