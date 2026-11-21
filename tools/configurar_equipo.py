#!/usr/bin/env python3
"""Configura las tres copias de radio; no abre puertos ni carga firmware."""
import argparse
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
FOLDERS = ("XJ_Dashboard_C6", "XJ_Mando_TFT", "XJ_Relevadores_C5")
BOARD = "ESP32-C5_Relay_X4 (303E32C5DC1)"


def mac(value, *, ble=False):
    if not isinstance(value, str) or not re.fullmatch(r"[0-9a-fA-F]{2}(?::[0-9a-fA-F]{2}){5}", value):
        raise ValueError("Usa una MAC de seis bytes, por ejemplo 02:11:22:33:44:55.")
    data = bytes.fromhex(value.replace(":", ""))
    if data in (bytes(6), bytes([255]) * 6) or (not ble and data[0] & 1):
        raise ValueError("La MAC debe ser una direccion unicast real, distinta de cero/broadcast.")
    return data


def render(config):
    addresses = [mac(config.get(key)) for key in ("mac_c6", "mac_tft", "mac_c5")]
    if len(set(addresses)) != 3:
        raise ValueError("C6, TFT y C5 deben tener direcciones MAC distintas.")
    obd = config.get("mac_obd_ble", "")
    if not isinstance(obd, str):
        raise ValueError("mac_obd_ble debe ser texto; usa una cadena vacia si no se conoce.")
    if obd:
        mac(obd, ble=True)
    enabled = config.get("habilitar_relevadores", False)
    if type(enabled) is not bool:
        raise ValueError("habilitar_relevadores debe ser true o false, sin comillas.")
    if enabled and config.get("placa_relevadores") != BOARD:
        raise ValueError("Este mapa de GPIO solo se valido en " + BOARD)
    lines = ["#pragma once", "#include <stdint.h>",
             "// Generado por tools/configurar_equipo.py; usar las MAC del equipo propio.",
             "namespace ConfiguracionRadio {", "constexpr bool configurada = true;"]
    for name, value in zip(("macC6", "macTft", "macC5"), addresses):
        values = ", ".join(f"0x{x:02X}" for x in value)
        lines.append(f"constexpr uint8_t {name}[] = {{{values}}};")
    lines += [f'constexpr char macObdBle[] = "{obd.lower()}";', "}", ""]
    return "\n".join(lines), enabled


def apply(config, root=ROOT, dry_run=False):
    header, enabled = render(config)
    board_file = root / "XJ_Relevadores_C5/Configuracion_Placa.h"
    old = board_file.read_text()
    updated, count = re.subn(r"constexpr bool configuracionHabilitada = (?:true|false);",
                             f"constexpr bool configuracionHabilitada = {str(enabled).lower()};", old)
    if count != 1:
        raise ValueError("No se encontro una unica configuracion de salidas C5; no se escribio nada.")
    destinations = [root / folder / "Configuracion_Radio.h" for folder in FOLDERS]
    if not all(path.is_file() for path in destinations):
        raise ValueError("Falta una copia de Configuracion_Radio.h; no se escribio nada.")
    if not dry_run:
        for path in destinations:
            path.write_text(header)
        board_file.write_text(updated)
    return enabled


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--archivo", type=Path, default=ROOT / "config.local.json")
    parser.add_argument("--comprobar", action="store_true", help="Validar sin escribir archivos")
    args = parser.parse_args()
    try:
        enabled = apply(json.loads(args.archivo.read_text()), dry_run=args.comprobar)
    except (ValueError, OSError) as exc:
        parser.exit(1, f"Configuracion rechazada: {exc}\n")
    print("Configuracion valida." if args.comprobar else "Configuradas las tres copias de radio.")
    print("Relevadores: " + ("HABILITADOS para la placa declarada." if enabled else "BLOQUEADOS; modo de diagnostico."))


if __name__ == "__main__":
    main()
