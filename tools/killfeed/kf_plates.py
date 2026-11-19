#!/usr/bin/env python3
"""Genera las placas del killfeed: pics/kf_bg_<equipo>_<ancho>.png.

Cada placa mide <ancho> x 14 px (ancho de 48 a 344 en pasos de 8), del color del
equipo, con un borde de 1 px con las esquinas recortadas y una franja clara de
3 px a la izquierda (en la placa de la víctima marca el corte con la del killer).

Uso: python3 tools/killfeed/kf_plates.py [carpeta pics]
"""
import os
import sys

from PIL import Image

H = 14
STRIPE = 3
WIDTHS = range(48, 344 + 1, 8)

# equipo: (franja, fondo, borde), RGBA. Colores vivos y casi opacos: la fila se arma con la placa del equipo del
# killer y encima, desde el arma, la del equipo de la victima (ver KF_Line en src/p_killfeed.c)
TEAMS = {
    "a": ((170, 230, 80, 250), (70, 120, 32, 235), (120, 180, 60, 240)),     # equipo 0, verde
    "x": ((250, 90, 70, 250), (160, 40, 32, 235), (220, 80, 60, 240)),       # equipo 1, rojo
    "n": ((220, 200, 130, 250), (110, 100, 66, 230), (170, 155, 100, 240)),  # sin equipo / Free For All, caqui
}


def plate(team, w):
    stripe, body, border = TEAMS[team]
    im = Image.new("RGBA", (w, H), (0, 0, 0, 0))
    px = im.load()
    for x in range(w):
        for y in range(H):
            if x in (0, w - 1) and y in (0, H - 1):
                continue  # esquinas recortadas
            if x < STRIPE:
                px[x, y] = stripe
            elif y in (0, H - 1) or x == w - 1:
                px[x, y] = border
            else:
                px[x, y] = body
    return im


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), "..", "..", "pics")
    for team in TEAMS:
        for w in WIDTHS:
            plate(team, w).save(os.path.join(out, f"kf_bg_{team}_{w}.png"), optimize=True)
    print(f"{len(TEAMS) * len(WIDTHS)} placas en {os.path.abspath(out)}")


if __name__ == "__main__":
    main()
