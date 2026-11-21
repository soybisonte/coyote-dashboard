#!/usr/bin/env python3
"""Instala core y bibliotecas fijadas usando Arduino CLI; no carga placas."""
import argparse
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--cli", default="arduino-cli")
    args = p.parse_args()
    cfg = json.loads((ROOT / "tools/entorno.json").read_text())
    commands = [
        ["core", "update-index", "--additional-urls", cfg["index"]],
        ["core", "install", cfg["core"], "--additional-urls", cfg["index"]],
        ["lib", "update-index"],
        ["lib", "install", *cfg["libraries"]],
    ]
    for command in commands:
        subprocess.run([args.cli, *command], check=True)


if __name__ == "__main__":
    main()
