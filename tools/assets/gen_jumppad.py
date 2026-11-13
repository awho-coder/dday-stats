#!/usr/bin/env python3
"""Genera el modelo del jump pad (trigger_jumppad, g_trigger.c):

  models/objects/jumppad/tris.md2   plataforma cuadrada de SIDE x SIDE unidades y HEIGHT de alto, con el origen al
                                    centro del piso (la caja de colision del juego es la misma)
  models/objects/jumppad/skin.pcx   la tapa: plancha de acero con borde de franjas amarillas y negras y un anillo
                                    naranja al centro; los costados usan las franjas del borde
  models/objects/jumppad_arrow/      flecha plana (tris.md2 + skin.pcx + skin2.pcx) para el diagonal, apunta a +x y gira con el yaw
  models/objects/jumppad_small/tris.md2  igual pero de SIDE_SMALL de lado, con las mismas texturas
  models/objects/jumppad/skin2.pcx  igual con el anillo y el centro encendidos: el juego alterna las dos (pulso)

Los .pcx son de 8 bits con la paleta de Quake 2, que es la que usa el cliente para los .pcx de modelos.

Uso: gen_jumppad.py <paleta> <carpeta de salida>
  <paleta> es la de pics/colormap.pcx (sus ultimos 768 bytes) o un .bin de 768 bytes.
Solo usa la biblioteca estandar de Python. SIDE y HEIGHT deben coincidir con JUMPPAD_SIDE/JUMPPAD_HEIGHT de
g_trigger.c.
"""
import math
import os
import random
import struct
import sys

SIDE = 256      # lado de la plataforma en unidades (= pixeles de la textura)
HEIGHT = 10     # alto; menos que el escalon del jugador (18) para subirse caminando
TEX = 256       # lado de la textura
BORDER = 24     # ancho del borde de franjas

SKIN_DIR = "models/objects/jumppad"
SIDE_SMALL = 96                 # lado del jump pad chico ("small" "1"), debe coincidir con JUMPPAD_SIDE_SMALL de g_trigger.c
SMALL_DIR = "models/objects/jumppad_small"

STEEL = (70, 72, 76)
STEEL_DARK = (40, 42, 46)
STEEL_LIGHT = (150, 150, 155)
YELLOW = (235, 190, 40)
BLACK = (22, 22, 22)
RING = (200, 90, 10)
RING_ON = (255, 230, 120)
CORE = (120, 60, 10)
CORE_ON = (255, 255, 200)


def nearest(pal, rgb):
    best, bi = None, 0
    for i in range(255):
        r, g, b = pal[i * 3:i * 3 + 3]
        d = (r - rgb[0]) ** 2 + (g - rgb[1]) ** 2 + (b - rgb[2]) ** 2
        if best is None or d < best:
            best, bi = d, i
    return bi


def shade(c, k):
    return tuple(max(0, min(255, int(v * k))) for v in c)


def skin_rgb(lit):
    """La tapa como filas de colores RGB. lit = el anillo y el centro encendidos."""
    rnd = random.Random(7)      # el mismo ruido en las dos texturas, para que solo cambie lo que pulsa
    c = TEX / 2 - 0.5
    rows = []
    for y in range(TEX):
        row = []
        for x in range(TEX):
            edge = min(x, y, TEX - 1 - x, TEX - 1 - y)
            r = math.hypot(x - c, y - c)
            if edge < BORDER:
                col = YELLOW if ((x + y) // 16) % 2 == 0 else BLACK
                if edge < 2:
                    col = shade(col, 0.6)
            elif edge < BORDER + 3:
                col = STEEL_LIGHT
            elif 84 <= r < 98:          # anillo
                col = RING_ON if lit else RING
                if r < 86 or r >= 96:
                    col = shade(col, 0.75)
            elif 98 <= r < 101 or 81 <= r < 84:
                col = STEEL_DARK
            elif r < 20:                # centro
                col = CORE_ON if lit else CORE
            elif r < 81:                # rejilla de anillos alrededor del centro
                if int(r) % 12 < 3:
                    col = shade(RING_ON if lit else RING, 0.55 if lit else 0.45)
                else:
                    col = STEEL_DARK
            else:
                col = STEEL
                if (x - BORDER) % 64 == 0 or (y - BORDER) % 64 == 0:
                    col = STEEL_DARK    # juntas de las planchas
                col = shade(col, 0.92 + rnd.random() * 0.16)
            # pernos en las esquinas de adentro
            for bx, by in ((40, 40), (TEX - 41, 40), (40, TEX - 41), (TEX - 41, TEX - 41)):
                d = math.hypot(x - bx, y - by)
                if d < 5:
                    col = STEEL_LIGHT if d < 3.5 else STEEL_DARK
            row.append(col)
        rows.append(row)
    return rows


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


def write_skin(path, lit, pal):
    cache = {}
    rows = []
    for row in skin_rgb(lit):
        out = []
        for col in row:
            if col not in cache:
                cache[col] = nearest(pal, col)
            out.append(cache[col])
        rows.append(out)
    open(path, "wb").write(pcx_bytes(TEX, TEX, rows, pal))


def write_md2(path, side=SIDE):
    h = side / 2
    # vertices: 0-3 abajo, 4-7 arriba (esquinas en el mismo orden)
    corners = ((-h, -h), (h, -h), (h, h), (-h, h))
    xyz = [(x, y, 0) for x, y in corners] + [(x, y, HEIGHT) for x, y in corners]

    st = []
    tris = []

    def st_index(s, t):
        st.append((int(s), int(t)))
        return len(st) - 1

    def quad(v, uv):
        """Cara de 4 vertices (en orden alrededor) con sus coordenadas de textura; se agrega de los dos lados
        para no depender del sentido de las caras de cada cliente."""
        si = [st_index(*p) for p in uv]
        for a, b, c in ((0, 1, 2), (0, 2, 3)):
            tris.append(((v[a], v[b], v[c]), (si[a], si[b], si[c])))
            tris.append(((v[a], v[c], v[b]), (si[a], si[c], si[b])))

    # tapa: la textura entera (y de la textura hacia -y del mundo, como se ve desde arriba)
    quad((4, 5, 6, 7), ((0, TEX), (TEX, TEX), (TEX, 0), (0, 0)))
    # costados: una tira de las franjas del borde, de HEIGHT pixeles
    band = ((0, 4 + HEIGHT), (TEX, 4 + HEIGHT), (TEX, 4), (0, 4))
    for i in range(4):
        j = (i + 1) % 4
        quad((i, j, j + 4, i + 4), band)

    scale = (side / 255.0, side / 255.0, HEIGHT / 255.0)
    translate = (-h, -h, 0.0)
    frame = struct.pack("<3f3f16s", *scale, *translate, b"jumppad")
    for v in xyz:
        b = [int(round((v[k] - translate[k]) / scale[k])) for k in range(3)]
        frame += struct.pack("<4B", *b, 5)     # normal 5 de la tabla de Quake 2 = (0, 0, 1)

    # glcmds (los usa el renderer GL original): cada triangulo como una tira de 3
    glcmds = b""
    for vi, si in tris:
        glcmds += struct.pack("<i", 3)
        for k in range(3):
            s, t = st[si[k]]
            glcmds += struct.pack("<ffi", s / TEX, t / TEX, vi[k])
    glcmds += struct.pack("<i", 0)

    skins = [f"{SKIN_DIR}/skin.pcx", f"{SKIN_DIR}/skin2.pcx"]
    ofs_skins = 68
    ofs_st = ofs_skins + 64 * len(skins)
    ofs_tris = ofs_st + 4 * len(st)
    ofs_frames = ofs_tris + 12 * len(tris)
    ofs_glcmds = ofs_frames + len(frame)
    ofs_end = ofs_glcmds + len(glcmds)
    head = struct.pack("<4s16i", b"IDP2", 8, TEX, TEX, len(frame), len(skins), len(xyz), len(st), len(tris),
                       len(glcmds) // 4, 1, ofs_skins, ofs_st, ofs_tris, ofs_frames, ofs_glcmds, ofs_end)
    data = head
    data += b"".join(s.encode().ljust(64, b"\0") for s in skins)
    data += b"".join(struct.pack("<hh", s, t) for s, t in st)
    data += b"".join(struct.pack("<6h", *vi, *si) for vi, si in tris)
    data += frame + glcmds
    open(path, "wb").write(data)


# --- flecha del jump pad diagonal (spawnflags 2): apunta hacia "angle" ---
ARROW_TEX = 128         # lado de la textura de la flecha
ARROW_DIR = "models/objects/jumppad_arrow"
# contorno visto desde arriba, apuntando a +x (el modelo gira con el yaw): cola y punta de flecha
ARROW_TAIL = ((-48, -16), (8, -16), (8, 16), (-48, 16))
ARROW_HEAD = ((8, -40), (56, 0), (8, 40))


def arrow_inside(x, y):
    if -48 <= x <= 8 and -16 <= y <= 16:
        return True
    # triangulo de la punta: base en x=8 (y de -40 a 40), vertice en x=56
    return 8 <= x <= 56 and abs(y) <= 40 * (56 - x) / 48.0


def arrow_skin_rgb(lit):
    """Flecha de franjas amarillas y negras en diagonal (peligro), borde de acero; lit = franjas mas vivas."""
    yel = (255, 225, 70) if lit else YELLOW
    blk = (40, 30, 10) if lit else BLACK
    rows = []
    for py in range(ARROW_TEX):
        row = []
        for px in range(ARROW_TEX):
            # pixel -> mundo (x hacia la derecha, y hacia arriba en la textura)
            x = px * 128.0 / ARROW_TEX - 64
            y = 64 - py * 128.0 / ARROW_TEX
            if not arrow_inside(x, y):
                row.append(STEEL_DARK)
                continue
            # borde de 4 unidades
            edge = not (arrow_inside(x - 4, y) and arrow_inside(x + 4, y) and
                        arrow_inside(x, y - 4) and arrow_inside(x, y + 4))
            if edge:
                row.append(STEEL_LIGHT if lit else STEEL)
            else:
                row.append(yel if int((x + y) // 12) % 2 == 0 else blk)
        rows.append(row)
    return rows


def write_arrow_skin(path, lit, pal):
    cache = {}
    rows = []
    for row in arrow_skin_rgb(lit):
        out = []
        for col in row:
            if col not in cache:
                cache[col] = nearest(pal, col)
            out.append(cache[col])
        rows.append(out)
    open(path, "wb").write(pcx_bytes(ARROW_TEX, ARROW_TEX, rows, pal))


def write_arrow_md2(path):
    xyz = []
    st = []
    tris = []

    def vert(x, y, z):
        xyz.append((x, y, z))
        return len(xyz) - 1

    def st_index(x, y):
        # mundo (x, y) -> pixel de la textura
        st.append((int(round((x + 64) * ARROW_TEX / 128.0)), int(round((64 - y) * ARROW_TEX / 128.0))))
        return len(st) - 1

    def poly(pts, z, uvpts):
        """Poligono convexo plano (de pts), en ambos sentidos para no depender de cada cliente."""
        v = [vert(x, y, z) for x, y in pts]
        si = [st_index(*p) for p in uvpts]
        for k in range(1, len(pts) - 1):
            tris.append(((v[0], v[k], v[k + 1]), (si[0], si[k], si[k + 1])))
            tris.append(((v[0], v[k + 1], v[k]), (si[0], si[k + 1], si[k])))

    for part in (ARROW_TAIL, ARROW_HEAD):
        poly(part, HEIGHT, part)
    # costados: cada arista del contorno, con un texel de acero oscuro (esquina de la textura, fuera de la flecha)
    outline = ((-48, -16), (8, -16), (8, -40), (56, 0), (8, 40), (8, 16), (-48, 16))
    for i in range(len(outline)):
        a, b = outline[i], outline[(i + 1) % len(outline)]
        v = [vert(a[0], a[1], 0), vert(b[0], b[1], 0), vert(b[0], b[1], HEIGHT), vert(a[0], a[1], HEIGHT)]
        corner = (-62, 62)
        si = [st_index(*corner) for _ in v]
        for k in (1, 2):
            tris.append(((v[0], v[k], v[k + 1]), (si[0], si[k], si[k + 1])))
            tris.append(((v[0], v[k + 1], v[k]), (si[0], si[k + 1], si[k])))

    scale = (128 / 255.0, 128 / 255.0, HEIGHT / 255.0)
    translate = (-64.0, -64.0, 0.0)
    frame = struct.pack("<3f3f16s", *scale, *translate, b"arrow")
    for x, y, z in xyz:
        b = [int(round((c - translate[k]) / scale[k])) for k, c in enumerate((x, y, z))]
        frame += struct.pack("<4B", *b, 5)

    glcmds = b""
    for vi, si in tris:
        glcmds += struct.pack("<i", 3)
        for k in range(3):
            s_, t_ = st[si[k]]
            glcmds += struct.pack("<ffi", s_ / ARROW_TEX, t_ / ARROW_TEX, vi[k])
    glcmds += struct.pack("<i", 0)

    skins = [f"{ARROW_DIR}/skin.pcx", f"{ARROW_DIR}/skin2.pcx"]
    ofs_skins = 68
    ofs_st = ofs_skins + 64 * len(skins)
    ofs_tris = ofs_st + 4 * len(st)
    ofs_frames = ofs_tris + 12 * len(tris)
    ofs_glcmds = ofs_frames + len(frame)
    ofs_end = ofs_glcmds + len(glcmds)
    head = struct.pack("<4s16i", b"IDP2", 8, ARROW_TEX, ARROW_TEX, len(frame), len(skins), len(xyz), len(st),
                       len(tris), len(glcmds) // 4, 1, ofs_skins, ofs_st, ofs_tris, ofs_frames, ofs_glcmds, ofs_end)
    data = head
    data += b"".join(sk.encode().ljust(64, b"\0") for sk in skins)
    data += b"".join(struct.pack("<hh", a, b) for a, b in st)
    data += b"".join(struct.pack("<6h", *vi, *si) for vi, si in tris)
    data += frame + glcmds
    open(path, "wb").write(data)


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    pal = open(sys.argv[1], "rb").read()[-768:]
    out = os.path.join(sys.argv[2], SKIN_DIR)
    os.makedirs(out, exist_ok=True)
    write_md2(os.path.join(out, "tris.md2"))
    write_skin(os.path.join(out, "skin.pcx"), False, pal)
    write_skin(os.path.join(out, "skin2.pcx"), True, pal)
    # el chico usa las mismas texturas (las del modelo grande), solo cambia el tamano
    small = os.path.join(sys.argv[2], SMALL_DIR)
    os.makedirs(small, exist_ok=True)
    write_md2(os.path.join(small, "tris.md2"), SIDE_SMALL)
    # la flecha del diagonal
    arrow = os.path.join(sys.argv[2], ARROW_DIR)
    os.makedirs(arrow, exist_ok=True)
    write_arrow_md2(os.path.join(arrow, "tris.md2"))
    write_arrow_skin(os.path.join(arrow, "skin.pcx"), False, pal)
    write_arrow_skin(os.path.join(arrow, "skin2.pcx"), True, pal)


if __name__ == "__main__":
    main()
