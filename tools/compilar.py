#!/usr/bin/env python3
"""Compila sin abrir USB ni cargar firmware. Salida: build/<placa>."""
import argparse
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("placa", choices=("c6", "tft", "c5", "mapa", "todos"))
    p.add_argument("--cli", default="arduino-cli")
    p.add_argument("--jobs", type=int, default=4)
    args = p.parse_args()
    boards = json.loads((ROOT / "tools/entorno.json").read_text())["boards"]
    targets = boards if args.placa == "todos" else [args.placa]
    for name in targets:
        board = boards[name]
        subprocess.run([args.cli, "compile", "--jobs", str(args.jobs), "--fqbn", board["fqbn"],
                        "--build-path", str(ROOT / "build" / name), str(ROOT / board["sketch"])], check=True)


if __name__ == "__main__":
    main()
