#!/usr/bin/env python3
"""Ejecuta las regresiones del protocolo, logger y presentador TFT en el host."""
import argparse
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--gfx", type=Path, required=True, help="Carpeta de Adafruit_GFX_Library")
    p.add_argument("--cxx", default="c++")
    a = p.parse_args()
    gfx = a.gfx.resolve()
    if not (gfx / "Adafruit_GFX.cpp").is_file():
        p.error("--gfx no contiene Adafruit_GFX.cpp")
    with tempfile.TemporaryDirectory(prefix="coyote-tests-") as temp:
        for name in ("relevadores", "registro_no_bloqueante", "pantalla_diferencial"):
            out = str(Path(temp) / name)
            cmd = [a.cxx, "-std=c++17"]
            if name == "pantalla_diferencial":
                cmd += ["-DARDUINO=100", "-I" + str(ROOT / "tools/host_gfx"), "-I" + str(gfx)]
            else:
                cmd += ["-Wall", "-Wextra", "-Werror"]
            cmd += [str(ROOT / "tests" / (name + "_test.cpp"))]
            if name == "pantalla_diferencial":
                cmd += [str(gfx / "Adafruit_GFX.cpp")]
            subprocess.run([*cmd, "-o", out], check=True)
            subprocess.run([out], check=True)
    subprocess.run([sys.executable, "-m", "unittest", "discover", "-s", "tests", "-p", "test_*.py"], cwd=ROOT, check=True)


if __name__ == "__main__":
    main()
