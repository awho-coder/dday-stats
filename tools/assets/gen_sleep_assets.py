#!/usr/bin/env python3
"""Genera los archivos del dardo de sueno del Medic (con medic_new):

  sprites/zzz.sp2     sprite de la "Z" que sube sobre el dormido (3 cuadros: chica, mediana, grande)
  sprites/zzz_0.pcx   la Z en 8 bits con la paleta de Quake 2 (indice 255 = transparente): la que se baja del server
  sprites/zzz_0.png   la misma Z en RGBA con bordes suaves: Q2PRO la prefiere si la tiene
  sound/misc/snore.wav  el ronquido (22050 Hz, mono, 16 bits), sintetizado con ffmpeg
  pics/i_nextdart.pcx   icono "NEXT DART" del contador del HUD, armado con las letras de pics/i_nextarty.pcx
  pics/dartcross.pcx    mira del Medic con la jeringa: 320x240 como las miras de francotirador (el HUD no tiene
                        otro lugar), transparente, con la cruz de pics/crosshair.pcx (la de Quake) en el centro

  models/weapons/g_sleepdart/  el dardo en vuelo: el tris.md2 de la jeringa lanzada (models/weapons/g_syringe) con
                        su propia textura, el liquido verde pintado de violeta (skin.png) o del lavanda mas
                        parecido de la paleta de Quake 2, que no tiene violetas (skin.pcx, el que se baja)

Uso: gen_sleep_assets.py <paleta> <carpeta de salida> [<i_nextarty.pcx> [<crosshair.pcx>]]
  <paleta> es la de pics/colormap.pcx (sus ultimos 768 bytes) o un .bin de 768 bytes.
  Sin <i_nextarty.pcx> no se hace el icono; sin <crosshair.pcx> (esta en pak0.pak del juego) no se hace la mira.
  El dardo se hace si encuentra models/weapons/g_syringe/ (del repo) desde la carpeta actual.
Solo usa la biblioteca estandar de Python y ffmpeg.
"""
import os
import struct
import subprocess
import sys
import zlib

SIZE = 32           # lado del .pcx
PNG_SIZE = 64       # lado del .png
SS = 4              # submuestras por pixel y por eje
FRAMES = (8, 11, 14)  # lado en unidades del juego de cada cuadro del sprite

FILL = (225, 238, 255)    # blanco azulado
EDGE = (25, 40, 110)      # azul oscuro


def z_shape(x, y, grow):
    """True si el punto (x, y) en [0,1] cae dentro de la Z (inclinada, de trazo grueso), engordada en grow."""
    x = (x - 0.5) / 0.8 + 0.5    # al 80 %, con margen para el borde y la cursiva
    y = (y - 0.5) / 0.8 + 0.5
    x -= (0.5 - y) * 0.18   # cursiva
    top = 0.14 - grow <= y <= 0.32 + grow and 0.16 - grow <= x <= 0.86 + grow
    bottom = 0.68 - grow <= y <= 0.86 + grow and 0.14 - grow <= x <= 0.84 + grow
    # diagonal de arriba a la derecha hasta abajo a la izquierda
    diag = False
    if 0.22 - grow <= y <= 0.78 + grow:
        cx = 0.84 - (y - 0.22) / 0.56 * 0.68
        diag = abs(x - cx) <= 0.13 + grow and 0.14 - grow <= x <= 0.86 + grow   # sin puntas fuera de las barras
    return top or bottom or diag


def coverage(n):
    """Por pixel de una imagen n x n: (cobertura del relleno, cobertura del borde), entre 0 y 1."""
    out = []
    for py in range(n):
        row = []
        for px in range(n):
            f = e = 0
            for sy in range(SS):
                for sx in range(SS):
                    x = (px + (sx + 0.5) / SS) / n
                    y = (py + (sy + 0.5) / SS) / n
                    if z_shape(x, y, 0):
                        f += 1
                    elif z_shape(x, y, 0.055):
                        e += 1
            row.append((f / SS / SS, e / SS / SS))
        out.append(row)
    return out


def write_png(path, n):
    cov = coverage(n)
    raw = bytearray()
    for row in cov:
        raw.append(0)
        for f, e in row:
            a = f + e
            if a <= 0:
                raw += bytes((0, 0, 0, 0))
                continue
            c = [int((FILL[i] * f + EDGE[i] * e) / a) for i in range(3)]
            raw += bytes(c + [min(255, int(a * 255 + 0.5))])

    def chunk(tag, data):
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xffffffff)

    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", n, n, 8, 6, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(bytes(raw), 9)) + chunk(b"IEND", b"")
    open(path, "wb").write(png)


def nearest(pal, rgb):
    best, bi = None, 0
    for i in range(255):    # el 255 queda para lo transparente
        r, g, b = pal[i * 3:i * 3 + 3]
        d = (r - rgb[0]) ** 2 + (g - rgb[1]) ** 2 + (b - rgb[2]) ** 2
        if best is None or d < best:
            best, bi = d, i
    return bi


def write_pcx(path, n, pal):
    cov = coverage(n)
    fill, edge = nearest(pal, FILL), nearest(pal, EDGE)
    pixels = []
    for row in cov:
        line = []
        for f, e in row:
            if f + e < 0.5:
                line.append(255)
            else:
                line.append(fill if f >= e else edge)
        pixels.append(line)

    head = struct.pack("<BBBBHHHHHH", 10, 5, 1, 8, 0, 0, n - 1, n - 1, 72, 72)
    head += bytes(48) + bytes((0, 1)) + struct.pack("<HH", n, 1) + bytes(58)
    body = bytearray()
    for line in pixels:     # RLE de PCX
        i = 0
        while i < n:
            v, run = line[i], 1
            while i + run < n and line[i + run] == v and run < 63:
                run += 1
            if run > 1 or v >= 0xc0:
                body += bytes((0xc0 | run, v))
            else:
                body.append(v)
            i += run
    open(path, "wb").write(head + bytes(body) + b"\x0c" + pal)


def write_sp2(path):
    data = struct.pack("<4sii", b"IDS2", 2, len(FRAMES))
    for s in FRAMES:
        name = b"sprites/zzz_0.pcx"
        data += struct.pack("<iiii", s, s, s // 2, s // 2) + name + bytes(64 - len(name))
    open(path, "wb").write(data)


def write_snore(path):
    """Un ronquido: inspiracion ronca (ruido grave con el aleteo del paladar) y una exhalacion suave."""
    sr = 22050
    flt = (
        # inspiracion: ruido marron muy grave + un zumbido de 70 Hz con armonicos, los dos con aleteo de ~30 Hz
        "anoisesrc=d=1.35:c=brown:r=%d:a=0.9,lowpass=f=420,lowpass=f=420[n];"
        "aevalsrc='0.35*sin(2*PI*68*t)+0.2*sin(2*PI*136*t)+0.12*sin(2*PI*204*t)':d=1.35:s=%d[z];"
        "[n][z]amix=inputs=2:normalize=0,tremolo=f=29:d=0.85,"
        "afade=t=in:d=0.55,afade=t=out:st=0.95:d=0.4[in];"
        "anullsrc=r=%d:cl=mono:d=0.3[gap];"
        # exhalacion: ruido rosa entre 350 y 1600 Hz, bajo
        "anoisesrc=d=0.85:c=pink:r=%d:a=0.5,highpass=f=350,lowpass=f=1600,volume=0.3,"
        "afade=t=in:d=0.25,afade=t=out:st=0.4:d=0.45[out];"
        "[in][gap][out]concat=n=3:v=0:a=1,volume=2.2,alimiter=limit=0.9[s]"
    ) % (sr, sr, sr, sr)
    subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-filter_complex", flt, "-map", "[s]",
                    "-ac", "1", "-ar", str(sr), "-c:a", "pcm_s16le", path], check=True)


def read_pcx(path):
    """(ancho, alto, filas de indices, paleta) de un .pcx de 8 bits."""
    d = open(path, "rb").read()
    xmin, ymin, xmax, ymax = struct.unpack("<HHHH", d[4:12])
    w, h = xmax - xmin + 1, ymax - ymin + 1
    bpl = struct.unpack("<H", d[66:68])[0]
    rows, i = [], 128
    for _ in range(h):
        row = []
        while len(row) < bpl:
            b = d[i]
            i += 1
            if b >= 0xc0:
                row += [d[i]] * (b & 0x3f)
                i += 1
            else:
                row.append(b)
        rows.append(row[:w])
    return w, h, rows, d[-768:]


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


# "NEXT DART": la fila de abajo de i_nextarty ("ARTY", letras en las filas 14 a 20, borde en 13 y 21) se rearma
# como D + A + R + T, con las mismas letras (la D se dibuja con el mismo trazo de 3 pixeles)
LETTER, OUTLINE = 9, 0
D_GLYPH = ("XXXXXXX..",
           "XXX..XXX.",
           "XXX...XXX",
           "XXX...XXX",
           "XXX...XXX",
           "XXX..XXX.",
           "XXXXXXX..")


def write_nextdart(src, path):
    w, h, rows, pal = read_pcx(src)
    out = [r[:] for r in rows]
    top, bottom = 13, 21
    # el fondo de la fila de abajo: el de la fila 22, columna por columna
    for y in range(top, bottom + 1):
        for x in range(2, w - 2):
            out[y][x] = rows[22][x]
    # A, R y T de "ARTY" (columnas en el original)
    glyphs = {"A": (4, 13), "R": (15, 23), "T": (25, 33)}
    x = 4
    mask = {}   # (x, y) -> True donde va letra
    for y, line in enumerate(D_GLYPH):
        for i, c in enumerate(line):
            if c == "X":
                mask[(x + i, 14 + y)] = True
    x += len(D_GLYPH[0]) + 1
    for g in "ART":
        a, b = glyphs[g]
        for sx in range(a, b + 1):
            for y in range(14, 21):
                if rows[y][sx] == LETTER:
                    mask[(x + sx - a, y)] = True
        x += b - a + 2
    for (mx, my) in mask:
        out[my][mx] = LETTER
    # borde negro alrededor de las letras
    for y in range(top, bottom + 1):
        for xx in range(2, w - 2):
            if (xx, y) in mask:
                continue
            if any((xx + dx, y + dy) in mask for dx in (-1, 0, 1) for dy in (-1, 0, 1)):
                out[y][xx] = OUTLINE
    open(path, "wb").write(pcx_bytes(w, h, out, pal))


def write_dartcross(src, path, pal):
    cw, ch, cross, _ = read_pcx(src)
    w, h = 320, 240
    rows = [[255] * w for _ in range(h)]
    x0, y0 = w // 2 - cw // 2, h // 2 - ch // 2   # su centro en el centro de la pantalla
    for y in range(ch):
        for x in range(cw):
            rows[y0 + y][x0 + x] = cross[y][x]
    open(path, "wb").write(pcx_bytes(w, h, rows, pal))


DART_LIQUID_PNG = (150, 70, 215)   # violeta
DART_LIQUID_PCX = 0xb0              # (119, 123, 207): lo mas parecido en la paleta de Quake 2


def write_sleepdart(src_dir, out_dir, pal):
    """El tris.md2 de la jeringa con el nombre de su textura cambiado, y la textura con el liquido (el cuadro verde)
    de otro color."""
    os.makedirs(out_dir, exist_ok=True)
    d = bytearray(open(os.path.join(src_dir, "tris.md2"), "rb").read())
    hdr = struct.unpack("<4s16i", bytes(d[:68]))
    nskins, oskins = hdr[5], hdr[11]
    name = b"models/weapons/g_sleepdart/skin.pcx"
    for i in range(nskins):
        d[oskins + i * 64:oskins + (i + 1) * 64] = name + bytes(64 - len(name))
    open(os.path.join(out_dir, "tris.md2"), "wb").write(bytes(d))

    # el .pcx: el indice del verde pasa a ser el lavanda
    w, h, rows, _ = read_pcx(os.path.join(src_dir, "skin.pcx"))
    green = max(set(v for r in rows for v in r), key=lambda v: pal[v * 3 + 1] - (pal[v * 3] + pal[v * 3 + 2]) / 2)
    rows = [[DART_LIQUID_PCX if v == green else v for v in r] for r in rows]
    open(os.path.join(out_dir, "skin.pcx"), "wb").write(pcx_bytes(w, h, rows, pal))

    # el .png: el mismo cuadro, violeta (los pixeles que en el .pcx eran el verde)
    _, _, src_rows, _ = read_pcx(os.path.join(src_dir, "skin.pcx"))
    raw = bytearray()
    for r in src_rows:
        raw.append(0)
        for v in r:
            raw += bytes(DART_LIQUID_PNG if v == green else tuple(pal[v * 3:v * 3 + 3]))

    def chunk(tag, data):
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xffffffff)

    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(bytes(raw), 9)) + chunk(b"IEND", b"")
    open(os.path.join(out_dir, "skin.png"), "wb").write(png)


def main():
    if len(sys.argv) not in (3, 4, 5):
        sys.exit(__doc__)
    data = open(sys.argv[1], "rb").read()
    pal = data[-768:]
    out = sys.argv[2]
    os.makedirs(os.path.join(out, "sprites"), exist_ok=True)
    os.makedirs(os.path.join(out, "sound", "misc"), exist_ok=True)
    write_sp2(os.path.join(out, "sprites", "zzz.sp2"))
    write_pcx(os.path.join(out, "sprites", "zzz_0.pcx"), SIZE, pal)
    write_png(os.path.join(out, "sprites", "zzz_0.png"), PNG_SIZE)
    write_snore(os.path.join(out, "sound", "misc", "snore.wav"))
    if len(sys.argv) == 4:
        os.makedirs(os.path.join(out, "pics"), exist_ok=True)
        write_nextdart(sys.argv[3], os.path.join(out, "pics", "i_nextdart.pcx"))
    if len(sys.argv) == 5:
        write_dartcross(sys.argv[4], os.path.join(out, "pics", "dartcross.pcx"), pal)
    if os.path.isdir(os.path.join("models", "weapons", "g_syringe")):
        write_sleepdart(os.path.join("models", "weapons", "g_syringe"),
                        os.path.join(out, "models", "weapons", "g_sleepdart"), pal)


if __name__ == "__main__":
    main()
