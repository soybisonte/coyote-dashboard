# Revisión de esta publicación

Fecha: 21 de noviembre de 2026. Edición inicial preparada desde los sketches
finales del prototipo COYOTE. Core ESP32 3.3.10, Arduino CLI 1.5.0 local;
la automatización host usa CLI 1.5.1.

## Compilación local

Los cuatro destinos compilaron con las plantillas públicas y los relés
bloqueados, usando las versiones de `tools/entorno.json`:

| Destino | Programa | RAM global |
| --- | --- | --- |
| C6 | 1,438,366 bytes | 45,168 bytes |
| TFT | 965,924 bytes | 58,176 bytes |
| C5 | 1,008,785 bytes | 47,740 bytes |
| Mapa OLED | 314,128 bytes | 16,532 bytes |

Además se generó una configuración con MAC ficticias distintas en una copia
temporal y se compiló el C5 con salidas habilitadas: 1,009,289 bytes de programa,
47,740 bytes de RAM. No se conectó ni activó ningún relevador en esa prueba.

La compilación se ejecutó contra el entorno instalado que se usó en el
prototipo, no como certificación de todas las variantes del mercado.
Estas plantillas no contienen un equipo configurado: antes de cargar hay
que generar las MAC propias. Las pruebas no flashearon ninguna placa del autor.

## Comprobaciones

- Copias de headers de radio, arranque y logos iguales entre destinos.
- Enlaces internos y JSON de documentación comprobados.
- Regresiones host del protocolo, logger sin bloqueo y presentador TFT.
- Configurador: rechaza MAC inválidas/duplicadas, dirección extendida,
  habilitación no booleana y habilitación de una placa diferente; validación
  previa a escrituras y modo comprobar sin modificar archivos.

El workflow de GitHub ejecuta validaciones y pruebas host, sin acceso a USB
ni activación de GPIO. La compilación completa está disponible con
`python3 tools/compilar.py todos`; no se ejecuta automáticamente en ese workflow.
Los resultados físicos corresponden al prototipo y están diferenciados en
[estado de validación](docs/07-validacion.md).
