#!/usr/bin/env python3
"""Genera las placas del killfeed: pics/kf_bg_<equipo>_<ancho>.png.

Cada placa mide <ancho> x 14 px (ancho de 48 a 344 en pasos de 8): fondo oscuro
semitransparente apenas teñido del color del equipo, esquinas recortadas y una
franja de 3 px del color del equipo a la izquierda.

Uso: python3 tools/killfeed/kf_plates.py [carpeta pics]
"""
import os
import sys

from PIL import Image

H = 14
STRIPE = 3
WIDTHS = range(48, 344 + 1, 8)

# como el kill feed de VisorDemos: fondo negro semitransparente, apenas teñido del color del equipo, con una franja
# de 3 px de ese color a la izquierda (en la placa de la víctima marca el corte con la del killer). Los nombres van
# en el mismo color (KF_COLOR_A y KF_COLOR_X en src/p_killfeed.c)
TEAMS = {
    "a": (120, 175, 255),  # equipo 0, azul (aliados del visor)
    "x": (255, 115, 95),   # equipo 1, rojo anaranjado (eje del visor)
    "n": (200, 190, 150),  # sin equipo / Free For All, caqui
}
TINT = 0.22    # cuánto color del equipo lleva el fondo
BG_ALPHA = 160


def plate(team, w):
    c = TEAMS[team]
    body = tuple(round(v * TINT) for v in c) + (BG_ALPHA,)
    stripe = c + (235,)
    im = Image.new("RGBA", (w, H), (0, 0, 0, 0))
    px = im.load()
    for x in range(w):
        for y in range(H):
            if x in (0, w - 1) and y in (0, H - 1):
                continue  # esquinas recortadas
            px[x, y] = stripe if x < STRIPE else body
    return im


# killfeed_style 1 (como VisorDemos): kf_bg_v_<ancho>.png, un rectángulo negro con alfa 120, sin franja
VISOR = (0, 0, 0, 120)


def plate_visor(w):
    return Image.new("RGBA", (w, H), VISOR)


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), "..", "..", "pics")
    for w in WIDTHS:
        for team in TEAMS:
            plate(team, w).save(os.path.join(out, f"kf_bg_{team}_{w}.png"), optimize=True)
        plate_visor(w).save(os.path.join(out, f"kf_bg_v_{w}.png"), optimize=True)
    print(f"{(len(TEAMS) + 1) * len(WIDTHS)} placas en {os.path.abspath(out)}")


if __name__ == "__main__":
    main()
