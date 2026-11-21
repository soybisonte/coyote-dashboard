#!/usr/bin/env python3
"""Comprueba enlaces locales, copias de headers y configuracion; no usa USB."""
from pathlib import Path
import json
import re
from urllib.parse import unquote

ROOT = Path(__file__).resolve().parents[1]


def main():
    problems = []
    for header in ("Arranque_XJ.h", "Arranque_Radio.h", "Logo_Willys.h", "Logo_Mal_Camino.h"):
        if (ROOT / "XJ_Dashboard_C6" / header).read_bytes() != (ROOT / "XJ_Mando_TFT" / header).read_bytes():
            problems.append("Difieren las copias de " + header)
    if (ROOT / "XJ_Relevadores_C5/Relevadores_Protocolo.h").read_bytes() != (ROOT / "XJ_Mando_TFT/Relevadores_Protocolo.h").read_bytes():
        problems.append("Difieren las copias del protocolo de relevadores")
    configs = [(ROOT / folder / "Configuracion_Radio.h").read_bytes() for folder in
               ("XJ_Dashboard_C6", "XJ_Mando_TFT", "XJ_Relevadores_C5")]
    if len(set(configs)) != 1:
        problems.append("Difieren las tres configuraciones de radio")
    for path in ROOT.rglob("*.md"):
        if any(x in path.relative_to(ROOT).parts for x in (".git", "build", ".venv", "output")):
            continue
        for target in re.findall(r"!?\[[^\]]*\]\(([^\s)]+)\)", path.read_text()):
            if "://" in target or target.startswith(("#", "mailto:")):
                continue
            local = unquote(target.split("#", 1)[0].strip("<>"))
            if local and not (path.parent / local).exists():
                problems.append(f"{path.relative_to(ROOT)}: enlace local roto: {local}")
    for path in (ROOT / "config/equipo.ejemplo.json", ROOT / "tools/entorno.json"):
        json.loads(path.read_text())
    if (ROOT / "CLAUDE.md").read_text().strip() != "@AGENTS.md":
        problems.append("CLAUDE.md no importa la fuente comun AGENTS.md")
    if problems:
        raise SystemExit("\n".join(problems))
    print("OK: enlaces locales, JSON, headers compartidos e instrucciones de agentes.")


if __name__ == "__main__":
    main()
