#!/usr/bin/env python3
"""Imagenes de los hitmarkers (cvar hitmarkers):

  pics/hitmark.pcx      la X blanca que aparece en el centro al pegarle a un enemigo
  pics/hitkill.pcx      la X roja (un poco mas grande) cuando ese golpe lo mata
                        Las dos son de 320x240 como las miras de francotirador (se dibujan en STAT_CROSSHAIR, el HUD
                        no tiene otro lugar), transparentes (indice 255) salvo la X, con la paleta de Quake 2
  (los sonidos sound/misc/hitmark.wav, el del impacto y la muerte, y sound/misc/hitmark_head.wav, el del headshot,
  no salen de aca: son archivos aparte)

Uso: gen_hitmarker.py <pcx con la paleta de Quake 2> <carpeta de salida>
  Sirve cualquier .pcx de 8 bits con la paleta de Quake 2 (p. ej. pics/dartcross.pcx del repo).
Solo usa la biblioteca estandar de Python.
"""
import os
import struct
import sys

W, H = 320, 240
GAP, LEN = 4, 3          # la X: cada brazo empieza a GAP pixeles del centro y mide LEN
KILL_GAP, KILL_LEN = 4, 4


def nearest(pal, rgb):
    best, bi = None, 0
    for i in range(255):    # el 255 es el transparente
        r, g, b = pal[i * 3:i * 3 + 3]
        d = (r - rgb[0]) ** 2 + (g - rgb[1]) ** 2 + (b - rgb[2]) ** 2
        if best is None or d < best:
            best, bi = d, i
    return bi


def pcx_bytes(w, h, rows, pal):
    head = struct.pack("<BBBBHHHHHH", 10, 5, 1, 8, 0, 0, w - 1, h - 1, 72, 72)
    head += bytes(48) + bytes((0, 1)) + struct.pack("<HH", w, 1) + bytes(58)
    body = bytearray()
    for line in rows:
        i = 0
        while i < w:
            v, run = line[i], 1
            while i + run < w and line[i + run] == v and run < 63:
                run += 1
            if run > 1 or v >= 0xc0:
                body += bytes((0xc0 | run, v))
            else:
                body.append(v)
            i += run
    return head + bytes(body) + b"\x0c" + pal


def write_x(path, pal, fill, edge, gap, length):
    """Los cuatro brazos en diagonal de 1 pixel de ancho, con un borde oscuro (solo arriba, abajo y a los lados del
    trazo) para que se vea sobre el cielo."""
    rows = [[255] * W for _ in range(H)]
    cx, cy = W // 2, H // 2
    body = set()
    for sx in (-1, 1):
        for sy in (-1, 1):
            for k in range(gap, gap + length):
                x = cx + sx * k - (1 if sx < 0 else 0)
                y = cy + sy * k - (1 if sy < 0 else 0)
                body.add((x, y))
    for x, y in body:
        for dx, dy in ((-1, 0), (1, 0), (0, -1), (0, 1)):
            p = (x + dx, y + dy)
            if p not in body:
                rows[p[1]][p[0]] = edge
    for x, y in body:
        rows[y][x] = fill
    open(path, "wb").write(pcx_bytes(W, H, rows, pal))


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    pal = open(sys.argv[1], "rb").read()[-768:]
    out = sys.argv[2]
    os.makedirs(os.path.join(out, "pics"), exist_ok=True)

    edge = nearest(pal, (0, 0, 0))
    write_x(os.path.join(out, "pics", "hitmark.pcx"), pal, nearest(pal, (255, 255, 255)), edge, GAP, LEN)
    write_x(os.path.join(out, "pics", "hitkill.pcx"), pal, nearest(pal, (255, 0, 0)), edge, KILL_GAP, KILL_LEN)


if __name__ == "__main__":
    main()
