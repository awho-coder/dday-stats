#!/usr/bin/env python3
"""Genera las placas del killfeed: pics/kf_bg_<equipo>_<ancho>.png.

Cada placa mide <ancho> x 14 px (ancho de 48 a 344 en pasos de 8), con un borde
de 1 px con las esquinas recortadas, una franja de 7 px del color del equipo a la
izquierda y el fondo teñido del mismo color, más claro junto a la franja y más
oscuro hacia la derecha (los nombres van encima, ver src/p_killfeed.c).

Uso: python3 tools/killfeed/kf_plates.py [carpeta pics]
"""
import os
import sys

from PIL import Image

H = 14
STRIPE = 7
WIDTHS = range(48, 344 + 1, 8)

# equipo: (franja, borde, fondo junto a la franja, fondo a la derecha), RGBA
TEAMS = {
    "a": ((150, 205, 70, 245), (110, 150, 50, 230), (62, 92, 30, 215), (34, 48, 20, 200)),   # equipo 0
    "x": ((230, 72, 56, 245), (172, 56, 42, 230), (104, 34, 26, 215), (52, 22, 18, 200)),    # equipo 1
    "n": ((196, 174, 108, 240), (140, 126, 82, 225), (78, 72, 50, 210), (42, 40, 32, 195)),  # sin equipo / FFA
}


def lerp(a, b, t):
    return tuple(round(a[i] + (b[i] - a[i]) * t) for i in range(4))


def plate(team, w):
    stripe, border, left, right = TEAMS[team]
    im = Image.new("RGBA", (w, H), (0, 0, 0, 0))
    px = im.load()
    for x in range(w):
        body = lerp(left, right, min(1.0, (x - STRIPE) / max(1, (w - STRIPE) * 0.6))) if x >= STRIPE else stripe
        for y in range(H):
            corner = (x in (0, w - 1)) and (y in (0, H - 1))
            if corner:
                continue
            if y in (0, H - 1) or x == w - 1:
                px[x, y] = stripe if x < STRIPE else border
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
