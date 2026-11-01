#!/usr/bin/env python3
"""Revisa un ents/<mapa>_ffa.ent contra su .bsp antes de subirlo al server.

Uso: python3 tools/ffa/spawncheck.py maps/<mapa>.bsp ents/<mapa>_ffa.ent [ents/<mapa>.ent]

1. Compara con las entidades del mapa: las del .ent normal si se pasa (el juego lo usa en vez
   de las del .bsp) o las del .bsp. Avisa si falta o cambio alguna (se perderian en FFA) y
   lista las nuevas.
2. Para cada spawn (info_reinforcements_start, que es donde aparece el FFA, e
   info_player_start) cuenta cuantas esquinas de la caja del jugador (donde lo pone el
   juego: 10 unidades sobre el punto) quedan dentro de una pared (debe dar 0) y cuanto cae
   hasta el piso.
"""
import re
import struct
import sys

SOLID = 1 | 2  # CONTENTS_SOLID | CONTENTS_WINDOW


def load_bsp(path):
    data = open(path, "rb").read()
    if data[:4] != b"IBSP":
        sys.exit(f"{path}: no es un BSP de Quake 2")

    def lump(i):
        return struct.unpack_from("<ii", data, 8 + 8 * i)

    off, ln = lump(0)
    entities = data[off:off + ln].rstrip(b"\0").decode("latin-1")
    po, pl = lump(1)
    planes = [struct.unpack_from("<4fi", data, po + k * 20) for k in range(pl // 20)]
    no, nl = lump(4)
    nodes = [struct.unpack_from("<3i", data, no + k * 28) for k in range(nl // 28)]
    lo, ll = lump(8)
    leafs = [struct.unpack_from("<i", data, lo + k * 28)[0] for k in range(ll // 28)]
    mo, _ = lump(13)
    head = struct.unpack_from("<i", data, mo + 36)[0]

    def contents(p):
        n = head
        while n >= 0:
            pn, front, back = nodes[n]
            nx, ny, nz, dist, _ = planes[pn]
            n = front if nx * p[0] + ny * p[1] + nz * p[2] - dist >= 0 else back
        return leafs[-1 - n]

    return entities, contents


def parse_entities(text):
    text = text.replace("\r", "")
    return [dict(re.findall(r'"([^"]*)"\s+"([^"]*)"', b)) for b in re.findall(r"\{(.*?)\}", text, re.S)]


def main():
    if len(sys.argv) not in (3, 4):
        sys.exit(__doc__)
    bsp_text, contents = load_bsp(sys.argv[1])
    ref = "el .ent normal" if len(sys.argv) == 4 else ".bsp"
    original = parse_entities(open(sys.argv[3], encoding="latin-1").read() if len(sys.argv) == 4 else bsp_text)
    ffa = parse_entities(open(sys.argv[2], encoding="latin-1").read())

    print(f"entidades: {ref} {len(original)}, _ffa.ent {len(ffa)}")
    missing = 0
    for e in original:
        if e in ffa:
            continue
        same = [x for x in ffa if x.get("classname") == e.get("classname") and x.get("origin") == e.get("origin")]
        print("  FALTA o cambia:", e.get("classname"), e.get("origin", ""), "->", "cambiada" if same else "no esta")
        missing += 0 if same else 1
    for e in ffa:
        if e not in original:
            print("  nueva:", e.get("classname"), e.get("origin", ""))

    stuck_total = 0
    print("spawns:")
    for e in ffa:
        if e.get("classname") not in ("info_reinforcements_start", "info_player_start") or "origin" not in e:
            continue
        x, y, z = (float(v) for v in e["origin"].split())
        z += 10  # el FFA pone al jugador 9 unidades sobre el punto y PutClientInServer 1 mas (p_client.c)
        # caja del jugador de pie: -16..16, -24..32 (un poco hacia adentro)
        corners = [(x + dx, y + dy, z + dz) for dx in (-15, 15) for dy in (-15, 15) for dz in (-23, 31)]
        stuck = sum(1 for p in corners + [(x, y, z)] if contents(p) & SOLID)
        floor = next((d for d in range(0, 600, 2) if contents((x, y, z - 24 - d)) & SOLID), None)
        stuck_total += stuck
        print(f"  {e['classname']:26s} {e['origin']:24s} esquinas_en_solido={stuck} caida_al_piso={floor}")

    ok = missing == 0 and stuck_total == 0
    print("OK" if ok else "REVISAR: faltan entidades o hay spawns dentro de paredes")
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
