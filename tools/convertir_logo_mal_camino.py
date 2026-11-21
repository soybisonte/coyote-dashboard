"""Convierte el logo suministrado a bitmaps nativos de 1 bit para TFT/OLED."""
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
(ROOT / "output/preview").mkdir(parents=True, exist_ok=True)
original = Image.open(ROOT / "assets/branding/Mal_Camino_original.png").convert("L")
# Separar la tinta oscura del papel beige sin redibujar el emblema.
tinta = original.point(lambda valor: 255 if valor < 100 else 0)


def bitmap(ancho):
    alto = round(original.height * ancho / original.width)
    img = tinta.resize((ancho, alto), Image.Resampling.LANCZOS).point(
        lambda valor: 255 if valor >= 110 else 0
    )
    datos = []
    for y in range(alto):
        for byte_x in range((ancho + 7) // 8):
            byte = 0
            for bit in range(8):
                x = byte_x * 8 + bit
                if x < ancho and img.getpixel((x, y)):
                    byte |= 0x80 >> bit
            datos.append(byte)
    return alto, datos, img


lines = ["#pragma once", "#include <stdint.h>", "#include <pgmspace.h>", "",
         "// Logo del usuario: assets/branding/Mal_Camino_original.png.",
         "// Generado por tools/convertir_logo_mal_camino.py; tinta blanca sobre negro.",
         "namespace MalCamino {"]
for nombre, ancho in [("tft", 216), ("oled", 66)]:
    alto, datos, img = bitmap(ancho)
    lines += [f"constexpr int {nombre}Width = {ancho}, {nombre}Height = {alto};",
              f"const uint8_t {nombre}Bitmap[] PROGMEM = {{"]
    for pos in range(0, len(datos), 16):
        lines.append("  " + ", ".join(f"0x{x:02X}" for x in datos[pos:pos + 16]) + ",")
    lines.append("};")
    img.save(ROOT / f"output/preview/Mal_Camino_{nombre}.png")
lines.append("}  // namespace MalCamino\n")
for folder in ["XJ_Mando_TFT", "XJ_Dashboard_C6"]:
    (ROOT / folder / "Logo_Mal_Camino.h").write_text("\n".join(lines))
