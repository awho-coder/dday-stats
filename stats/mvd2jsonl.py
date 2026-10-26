#!/usr/bin/env python3
"""
Convierte un MVD2 de Q2PRO (grabado con mvdrecord / GTV en un servidor de D-Day
que NO tiene stats_log) en un archivo de eventos .jsonl como los que escribe la
DLL (g_statslog.c), para cargarlo con ingest.py como cualquier otra partida.

    python3 stats/mvd2jsonl.py PARTIDA.mvd2[.gz] --server dday.cl-duelos1 \
        --kind duel --event "Torneo Clanes sin Gloria" --end "2026-10-26 00:40-03" \
        --out /ruta/de/eventos/

Qué se saca del MVD:
  - mapa, jugadores (nombre y modelo -> equipo y clase), tiempo de cada uno en
    cada equipo y en cada clase (por frames, 10 por segundo)
  - cada muerte a partir del mensaje que el mod manda a todos (víctima, frase,
    asesino, arma, ** HEADSHOT **), con la posición de los dos y la distancia
  - el arma exacta en armas "en mano": el modelo que tiene el que mata (gunindex)
  - el casco que desvía un tiro y el pie salvado (mensajes del mod)
  - en duelos, el inicio real ("Que comience el juego!") y el ganador
    ("Team X is victorious ...")

Qué NO trae un MVD: tiros acertados/fallados (puntería), objetivos, puntaje del
mod. Quedan en cero. El reloj es el de los frames (no hay hora real): la hora
de la partida se da con --end (fin) o se toma la fecha del archivo.

Solo lee: el archivo es un dato, no se ejecuta nada de él.
"""
import argparse
import gzip
import hashlib
import json
import math
import re
import struct
import sys
from datetime import datetime, timezone
from pathlib import Path

FRAME_S = 0.1  # los frames de un MVD son de 1/10 s

# --- protocolo MVD (inc/common/protocol.h de Q2PRO) ---------------------------
SVCMD_BITS = 5
SVCMD_MASK = (1 << SVCMD_BITS) - 1
MVD_NOP, MVD_SERVERDATA, MVD_CONFIGSTRING, MVD_FRAME = 1, 4, 5, 6
MVD_UNICAST, MVD_UNICAST_R = 8, 9
MVD_MULTICAST_FIRST, MVD_MULTICAST_LAST = 10, 15
MVD_SOUND, MVD_PRINT = 16, 17
SVC_LAYOUT, SVC_PRINT, SVC_STUFFTEXT, SVC_CONFIGSTRING, SVC_CENTERPRINT = 4, 10, 11, 13, 15
CLIENTNUM_NONE = 255

# configstrings, esquema clásico (cs_remap_old); las versiones 2011+ con límites
# extendidos usan otro y no se soportan (se avisa)
CS_MODELS, CS_PLAYERSKINS, CS_END = 32, 1312, 2080
MVD_VERSIONS = (2009, 2010)

# bits del estado de jugador en MVD (PPS_*)
PPS_M_TYPE, PPS_M_ORIGIN, PPS_M_ORIGIN2 = 1 << 0, 1 << 1, 1 << 2
PPS_VIEWOFFSET, PPS_VIEWANGLES, PPS_VIEWANGLE2 = 1 << 3, 1 << 4, 1 << 5
PPS_KICKANGLES, PPS_BLEND, PPS_FOV = 1 << 6, 1 << 7, 1 << 8
PPS_WEAPONINDEX, PPS_WEAPONFRAME, PPS_GUNOFFSET = 1 << 9, 1 << 10, 1 << 11
PPS_GUNANGLES, PPS_RDFLAGS, PPS_STATS, PPS_MOREBITS = 1 << 12, 1 << 13, 1 << 14, 1 << 15
PM_SPECTATOR = 1

# --- D-Day -------------------------------------------------------------------
ALLIES = {"usa", "gbr", "rus", "pol", "aus", "fra", "usm"}
AXIS = {"grm", "ita", "jpn"}
# modelo "usa/class_lmg" -> clase de la DLL (class_names[] de g_statslog.c)
MODEL_CLASS = {
    "class_infantry": "infantry", "class_officer": "officer", "class_lmg": "lgunner",
    "class_hmg": "hgunner", "class_sniper": "sniper", "class_special": "special",
    "class_engineer": "engineer", "class_medic": "medic", "class_flamer": "flamer",
}

# frase del obituario (ClientObituary, p_client.c) -> mod de la DLL
# (StatsLog_ModName). El resto de la frase ("'s rifle - ** HEADSHOT **") va aparte.
KILL_PHRASES = [
    ("was capped by", "pistol"), ("was shotgunned by", "shotgun"),
    ("was shot down by", "rifle"), ("was machinegunned by", "lmg"),
    ("was popped by", "grenade"), ("was shredded by", "grenade"),
    ("did not survive", "rocket"), ("was browned by", "hmg"),
    ("was poisoned by", "syringe"), ("was sniped by", "sniper"),
    ("died of severe wounds inflicted by", "wound"), ("tried to invade", "telefrag"),
    ("was scorched by", "fire"), ("was cremated by", "fire"), ("got flamed by", "fire"),
    ("was sliced in half by", "knife"), ("was castrated by", "knife"),
    ("was knocked out by", "fists"), ("was helmeted by", "helmet"),
    ("was bayoneted by", "bayonet"), ("was flattened by", "plonk"),
    ("was molotoved by", "molotov"), ("feels", "held_grenade"),
    # frases que dependen de lo que sigue al nombre del asesino
    ("was gunned down by", None), ("was killed by", None), ("ate", None),
    ("caught", None), ("didn't see", None),
]
TAIL_MOD = [  # "'s submachinegun" -> smg, para las frases ambiguas
    ("submachinegun", "smg"), ("heavy machine gun", "hmg"), ("light machine gun", "lmg"),
    ("airstrike", "airstrike"), ("rocket", "rocket"), ("explosive attack", "rocket"),
    ("handgrenade", "handgrenade"), ("TNT", "tnt"), ("grenade", "grenade"),
    ("shrapnel", "grenade"), ("rifle", "rifle"), ("pain", "held_grenade"),
]
# muertes sin asesino (o con "(after being wounded by X)", que le da el crédito a X)
ENV_PHRASES = [
    ("killed for being in a spawn area", "spawncamp"), ("penalized by execution", "penalty"),
    ("was squished", "crush"), ("in the wrong place", "trigger_hurt"),
    ("cratered", "falling"), ("fell to", "falling"), ("sank like a rock", "water"),
    ("drown", "water"), ("melted", "slime"), ("does a back flip into the lava", "lava"),
    ("tripped on his own grenade", "handgrenade"), ("tripped on her own grenade", "handgrenade"),
    ("tripped on his own TNT", "tnt"), ("tripped on her own TNT", "tnt"),
    ("blew himself up", "rocket"), ("blew herself up", "rocket"),
    ("killed himself", "suicide"), ("killed herself", "suicide"), ("died", "suicide"),
]
# mods cuyo "weapon" es el arma que tiene en la mano el que mata
# (StatsLog_ModUsesHeldWeapon, g_statslog.c)
HELD_WEAPON_MODS = {"pistol", "shotgun", "rifle", "lmg", "hmg", "smg", "sniper", "rocket"}


class Buf:
    def __init__(self, data):
        self.d = data
        self.i = 0

    def byte(self):
        v = self.d[self.i]
        self.i += 1
        return v

    def char(self):
        v = struct.unpack_from("<b", self.d, self.i)[0]
        self.i += 1
        return v

    def word(self):
        v = struct.unpack_from("<H", self.d, self.i)[0]
        self.i += 2
        return v

    def short(self):
        v = struct.unpack_from("<h", self.d, self.i)[0]
        self.i += 2
        return v

    def long(self):
        v = struct.unpack_from("<i", self.d, self.i)[0]
        self.i += 4
        return v

    def string(self):
        j = self.d.index(0, self.i)
        s = self.d[self.i:j]
        self.i = j + 1
        return s.decode("latin-1")

    def skip(self, n):
        self.i += n

    def left(self):
        return len(self.d) - self.i


class Player:
    __slots__ = ("inuse", "pm_type", "origin", "gunindex")

    def __init__(self):
        self.inuse = False
        self.pm_type = 0
        self.origin = [0, 0, 0]   # en 1/8 de unidad
        self.gunindex = 0


def read_players(b, players):
    """Jugadores de un frame (MVD_ParsePacketPlayers + delta del estado)."""
    while True:
        num = b.byte()
        if num == CLIENTNUM_NONE:
            return
        p = players.setdefault(num, Player())
        bits = b.word()
        remove = bool(bits & PPS_MOREBITS)  # versiones viejas: MOREBITS = REMOVE
        if bits & PPS_M_TYPE:
            p.pm_type = b.byte()
        if bits & PPS_M_ORIGIN:
            p.origin[0] = b.short()
            p.origin[1] = b.short()
        if bits & PPS_M_ORIGIN2:
            p.origin[2] = b.short()
        if bits & PPS_VIEWOFFSET:
            b.skip(3)
        if bits & PPS_VIEWANGLES:
            b.skip(4)
        if bits & PPS_VIEWANGLE2:
            b.skip(2)
        if bits & PPS_KICKANGLES:
            b.skip(3)
        if bits & PPS_WEAPONINDEX:
            p.gunindex = b.byte()
        if bits & PPS_WEAPONFRAME:
            b.skip(1)
        if bits & PPS_GUNOFFSET:
            b.skip(3)
        if bits & PPS_GUNANGLES:
            b.skip(3)
        if bits & PPS_BLEND:
            b.skip(4)
        if bits & PPS_FOV:
            b.skip(1)
        if bits & PPS_RDFLAGS:
            b.skip(1)
        if bits & PPS_STATS:
            statbits = b.long() & 0xFFFFFFFF
            for i in range(32):
                if statbits & (1 << i):
                    b.skip(2)
        p.inuse = not remove


def read_mvd(path):
    """Recorre el MVD. Devuelve configstrings iniciales, la línea de tiempo por
    frame (configstrings cambiados y estado de los jugadores) y los prints."""
    raw = Path(path).read_bytes()
    if raw[:2] == b"\x1f\x8b":
        raw = gzip.decompress(raw)
    if raw[:4] != b"MVD2":
        raise SystemExit(f"{path}: no es un MVD2")
    cs = {}
    players = {}
    frames = []        # por frame: [(slot, inuse, pm_type, (x, y, z), gunindex), ...]
    cs_changes = []    # (frame, index, string)
    prints = []        # (frame, destino, nivel, texto); destino -1 = todos
    frame = 0
    i = 4
    while i + 2 <= len(raw):
        n = struct.unpack_from("<H", raw, i)[0]
        i += 2
        if n == 0:
            break  # fin del mapa (un MVD con varios mapas: solo el primero)
        b = Buf(raw[i:i + n])
        i += n
        while b.left() > 0:
            op = b.byte()
            cmd, extra = op & SVCMD_MASK, op >> SVCMD_BITS
            if cmd == MVD_SERVERDATA:
                if b.long() != 37:
                    raise SystemExit("protocolo MVD no soportado")
                version = b.word()
                if version not in MVD_VERSIONS:
                    raise SystemExit(f"versión de MVD {version} no soportada (solo {MVD_VERSIONS})")
                b.long()      # servercount
                b.string()    # gamedir
                b.short()     # clientnum
                while True:
                    idx = b.word()
                    if idx == CS_END:
                        break
                    cs[idx] = b.string()
                # frame base: portalbits + jugadores (las entidades no hacen falta)
                b.skip(b.byte())
                read_players(b, players)
                break
            if cmd == MVD_CONFIGSTRING:
                idx = b.word()
                cs_changes.append((frame, idx, b.string()))
            elif cmd == MVD_FRAME:
                b.skip(b.byte())
                read_players(b, players)
                frames.append([(s, p.inuse, p.pm_type, tuple(p.origin), p.gunindex)
                               for s, p in players.items()])
                frame += 1
                break  # entidades y datagrama: no hacen falta
            elif cmd == MVD_PRINT:
                level = b.byte()
                prints.append((frame, -1, level, b.string()))
            elif cmd in (MVD_UNICAST, MVD_UNICAST_R):
                length = b.byte() | (extra << 8)
                dest = b.byte()
                end = b.i + length
                while b.i < end:
                    sc = b.byte()
                    if sc == SVC_PRINT:
                        level = b.byte()
                        prints.append((frame, dest, level, b.string()))
                    elif sc == SVC_CENTERPRINT:
                        prints.append((frame, dest, "center", b.string()))
                    elif sc in (SVC_LAYOUT, SVC_STUFFTEXT):
                        b.string()
                    elif sc == SVC_CONFIGSTRING:
                        b.word()
                        b.string()
                    else:
                        b.i = end
                b.i = end
            elif MVD_MULTICAST_FIRST <= cmd <= MVD_MULTICAST_LAST:
                length = b.byte() | (extra << 8)
                if (cmd - MVD_MULTICAST_FIRST) % 3:
                    b.word()
                b.skip(length)
            elif cmd == MVD_NOP:
                pass
            else:
                break  # sonido u otro dato no confiable: lo que queda del bloque no importa
    return cs, cs_changes, frames, prints


def clean(s):
    """Texto legible: sin bit alto ni caracteres de control (D-Day pone \\x02)."""
    out = []
    for ch in s:
        c = ord(ch) & 127
        if c in (10, 13, 9):
            c = 32
        if c < 32:
            continue
        out.append(chr(c))
    return "".join(out).strip()


def lines_per_frame(prints):
    """Cada jugador recibe su copia de un obituario, en pedazos. Se arman las
    líneas por destinatario y se cuenta cada una una vez por frame (la cantidad
    de veces = la mayor que recibió un mismo destinatario)."""
    acc = {}
    out = {}
    for frame, dest, level, text in prints:
        if level == "center":
            out.setdefault(frame, {}).setdefault(("center", clean(text)), {})[dest] = 1
            continue
        buf = acc.setdefault(dest, ["", frame])
        if not buf[0]:
            buf[1] = frame
        buf[0] += text
        while "\n" in buf[0]:
            line, buf[0] = buf[0].split("\n", 1)
            c = clean(line)
            if c:
                per = out.setdefault(buf[1], {}).setdefault(("print", c), {})
                per[dest] = per.get(dest, 0) + 1
            buf[1] = frame
    result = []
    for frame in sorted(out):
        for (kind, text), per in out[frame].items():
            for _ in range(max(per.values())):
                result.append((frame, kind, text))
    return result


class Timeline:
    """Nombres y modelos por slot en cada frame (configstrings que cambian)."""

    def __init__(self, cs, cs_changes):
        self.base = {k: v for k, v in cs.items() if CS_PLAYERSKINS <= k < CS_PLAYERSKINS + 256}
        self.models = {k - CS_MODELS: v for k, v in cs.items() if CS_MODELS <= k < CS_MODELS + 256}
        self.changes = [(f, k, v) for f, k, v in cs_changes if CS_PLAYERSKINS <= k < CS_PLAYERSKINS + 256]
        for f, k, v in cs_changes:
            if CS_MODELS <= k < CS_MODELS + 256:
                self.models[k - CS_MODELS] = v
        self.cur = dict(self.base)
        self.pos = 0

    def advance(self, frame):
        while self.pos < len(self.changes) and self.changes[self.pos][0] <= frame:
            _, k, v = self.changes[self.pos]
            self.cur[k] = v
            self.pos += 1

    def slots(self):
        """slot -> (nombre, ejército, clase)"""
        out = {}
        for k, v in self.cur.items():
            if not v:
                continue
            name, _, model = v.partition("\\")
            army, _, cls = model.partition("/")
            out[k - CS_PLAYERSKINS] = (name, army.lower(), MODEL_CLASS.get(cls.lower()))
        return out


def team_of(army):
    if army in ALLIES:
        return 0
    if army in AXIS:
        return 1
    return None


def parse_obituary(text, names):
    """(víctima, asesino o None, mod, resto) o None si no es un obituario."""
    by_len = sorted(names, key=len, reverse=True)
    victim = next((n for n in by_len if text.startswith(n + " ") or text.startswith(n + "'")), None)
    if not victim:
        return None
    rest = text[len(victim):].strip()
    m = re.search(r"\(after being wounded by (.+?)\)\.?$", rest)
    if m:
        killer = m.group(1).strip()
        what = rest[:m.start()].strip()
        mod = next((md for ph, md in ENV_PHRASES if ph in what), "wound")
        return victim, (killer if killer in names else None), mod, ""
    for phrase, mod in KILL_PHRASES:
        if not rest.startswith(phrase + " "):
            continue
        after = rest[len(phrase) + 1:]
        killer = next((n for n in by_len if after.startswith(n)), None)
        if not killer:
            continue
        tail = after[len(killer):]
        if mod is None:
            mod = next((md for key, md in TAIL_MOD if key in tail), None)
            if mod is None:
                mod = "smg" if phrase == "was gunned down by" else "explosive"
                if phrase == "was gunned down by" and "'s " not in tail:
                    mod = "shotgun"  # MOD_SHOTGUN: "was gunned down by" sin arma
        return victim, killer, mod, tail
    env = next((md for ph, md in ENV_PHRASES if ph in rest), None)
    if env:
        return victim, None, env, ""
    return None


def convert(path, server, kind, event, end_ts, weapons):
    cs, cs_changes, frames, prints = read_mvd(path)
    mapname = re.sub(r"^maps/|\.bsp$", "", cs.get(CS_MODELS + 1, ""))
    tl = Timeline(cs, cs_changes)
    lines = lines_per_frame(prints)

    # inicio (duelo: la última cuenta regresiva) y fin
    live_frame = None
    end_frame, winner, end_kills = None, None, None
    for frame, k, text in lines:
        if k == "center" and "comience el juego" in text.lower():
            live_frame = frame
        m = re.match(r"Team (Allies|Axis) is victorious(?: \((\d+) / (\d+) kills\))?", text)
        if k == "print" and m:
            end_frame, winner = frame, (0 if m.group(1) == "Allies" else 1)
            end_kills = int(m.group(2)) if m.group(2) else None
    if kind == "duel" and live_frame is None:
        raise SystemExit(f"{path}: duelo sin 'Que comience el juego!' (sin cuenta regresiva)")
    start_frame = live_frame if live_frame is not None else 0
    reason = "normal" if end_frame is not None else "aborted"
    if end_frame is None:
        end_frame = len(frames)
    dur = round((end_frame - start_frame) * FRAME_S, 1)

    # tiempo por jugador, equipo y clase (desde el inicio hasta el fin)
    stats = {}
    team_changes = []   # (frame, nombre, equipo) al cambiar
    last_team = {}
    armies = {0: {}, 1: {}}
    for f in range(len(frames)):
        tl.advance(f)
        slots = tl.slots()
        for slot, inuse, pm_type, _, _ in frames[f]:
            if not inuse or slot not in slots:
                continue
            name, army, cls = slots[slot]
            team = team_of(army)
            if pm_type == PM_SPECTATOR or team is None:
                team = None
            if last_team.get(name, "x") != team:
                last_team[name] = team
                team_changes.append((f, name, team))
            if team is None or not (start_frame <= f < end_frame):
                continue
            s = stats.setdefault(name, {"time": [0, 0], "classes": {}})
            s["time"][team] += 1
            if cls:
                s["classes"][cls] = s["classes"].get(cls, 0) + 1
            armies[team][army] = armies[team].get(army, 0) + 1

    # muertes
    def state_at(f, name):
        tl2.advance(f)
        slots = tl2.slots()
        for slot, inuse, pm_type, origin, gun in frames[min(f, len(frames) - 1)]:
            if slot in slots and slots[slot][0] == name:
                n, army, cls = slots[slot]
                return {"team": team_of(army), "army": army, "class": cls,
                        "pos": [round(origin[0] / 8), round(origin[1] / 8), round(origin[2] / 8)],
                        "gun": tl2.models.get(gun, "")}
        return None

    tl2 = Timeline(cs, cs_changes)
    kills, lucks, unknown = [], [], []
    for frame, k, text in lines:
        if k != "print":
            continue
        tl.pos, tl.cur = 0, dict(tl.base)
        tl.advance(frame)
        names = {v[0] for v in tl.slots().values()} | set(stats)
        m = re.match(r"\*\* (.+?) is a lucky bastard! The helmet deflected the shot", text)
        if m:
            lucks.append((frame, "helmet", m.group(1)))
            continue
        m = re.match(r"\*\* (.+?) almost lost a foot!", text)
        if m:
            lucks.append((frame, "foot", m.group(1)))
            continue
        ob = parse_obituary(text, names)
        if ob is None:
            if any(w in text for w in (" was ", " by ", "himself")):
                unknown.append(text)
            continue
        victim, killer, mod, tail = ob
        vs = state_at(frame, victim) or {"team": None, "class": None, "pos": None, "army": ""}
        ks = state_at(frame, killer) if killer else None
        hs = "HEADSHOT" in tail
        ff = bool(killer and ks and ks["team"] is not None and ks["team"] == vs["team"])
        weapon = None
        if killer and ks and mod in HELD_WEAPON_MODS:
            weapon = weapons.get(f"{ks['army']}|{ks['gun']}")
        ev = {"ev": "kill", "t": round(frame * FRAME_S, 1),
              "victim": {"name": victim, "bot": 0, "team": vs["team"], "class": vs["class"],
                         "pos": vs["pos"]},
              "killer": ({"name": killer, "bot": 0, "team": ks["team"] if ks else None,
                          "class": ks["class"] if ks else None, "pos": ks["pos"] if ks else None}
                         if killer else None),
              "mod": mod, "weapon": weapon, "hs": int(hs), "ff": int(ff),
              "suicide": int(killer is None)}
        if killer and ks and vs["pos"] and ks["pos"]:
            ev["dist"] = int(math.dist(ks["pos"], vs["pos"]))
        kills.append((frame, ev))

    # resumen por jugador (kills, muertes, racha...) desde el inicio
    def agg(name):
        return stats.setdefault(name, {"time": [0, 0], "classes": {}})

    streak = {}
    summary = {}
    for frame, ev in kills:
        if not (start_frame <= frame <= end_frame):
            continue
        v = ev["victim"]["name"]
        sv = summary.setdefault(v, dict(kills=0, deaths=0, suicides=0, tk=0, hs=0, best_streak=0,
                                        helmet_saves=0, foot_saves=0))
        sv["deaths"] += 1
        sv["suicides"] += ev["suicide"]
        streak[v] = 0
        kl = ev["killer"]
        if kl:
            sk = summary.setdefault(kl["name"], dict(kills=0, deaths=0, suicides=0, tk=0, hs=0,
                                                     best_streak=0, helmet_saves=0, foot_saves=0))
            if ev["ff"]:
                sk["tk"] += 1
                streak[kl["name"]] = 0
            else:
                sk["kills"] += 1
                sk["hs"] += ev["hs"]
                if ev["mod"] != "wound":
                    streak[kl["name"]] = streak.get(kl["name"], 0) + 1
                    sk["best_streak"] = max(sk["best_streak"], streak[kl["name"]])
    for frame, typ, name in lucks:
        if start_frame <= frame <= end_frame:
            s = summary.setdefault(name, dict(kills=0, deaths=0, suicides=0, tk=0, hs=0,
                                              best_streak=0, helmet_saves=0, foot_saves=0))
            s["helmet_saves" if typ == "helmet" else "foot_saves"] += 1

    # eventos en el formato de la DLL (version 2)
    base_ts = end_ts - dur
    def ts_of(frame):
        return int(round(base_ts + (frame - start_frame) * FRAME_S))

    digest = hashlib.sha1(Path(path).read_bytes()).hexdigest()[:8]
    stamp = datetime.fromtimestamp(base_ts, tz=timezone.utc).strftime("%Y%m%d-%H%M%S")
    match_id = f"{stamp}-{re.sub(r'[^A-Za-z0-9-]', '', server)[:24]}-{digest}"
    army0 = max(armies[0], key=armies[0].get) if armies[0] else "usa"
    army1 = max(armies[1], key=armies[1].get) if armies[1] else "grm"
    out = [{"ev": "match_start", "t": 0.0, "v": 2, "match": match_id, "server": server,
            "map": mapname, "mode": "dm", "kind": kind, "event": event,
            "tournament": 1 if kind == "duel" else 0, "ts": ts_of(0),
            "teams": [{"idx": 0, "army": army0, "name": "Allies"},
                      {"idx": 1, "army": army1, "name": "Axis"}],
            "source": "mvd", "mvd": Path(path).name}]
    events = []
    for f, name, team in team_changes:
        events.append((f, 0, {"ev": "team", "t": round(f * FRAME_S, 1), "name": name, "bot": 0,
                              "team": team if team is not None else -1}))
    if live_frame is not None:
        events.append((live_frame, 1, {"ev": "live", "t": round(live_frame * FRAME_S, 1),
                                       "ts": ts_of(live_frame), "event": event}))
        events.append((live_frame, 1, {"ev": "play", "t": round(live_frame * FRAME_S, 1),
                                       "ts": ts_of(live_frame), "idle": 0}))
    for f, ev in kills:
        events.append((f, 2, ev))
    for f, typ, name in lucks:
        ps = state_at(f, name) or {}
        events.append((f, 2, {"ev": "luck", "t": round(f * FRAME_S, 1), "type": typ,
                              "player": {"name": name, "bot": 0, "team": ps.get("team"),
                                         "class": ps.get("class"), "pos": ps.get("pos")}}))
    events.sort(key=lambda e: (e[0], e[1]))
    out += [e for _, _, e in events]

    # contador de kills del equipo como en el juego: kills a enemigos más los
    # suicidios del rival (así da el "75 / 75 kills" que anuncia el servidor)
    team_kills = {0: 0, 1: 0}
    for f, ev in kills:
        if not (start_frame <= f <= end_frame):
            continue
        kl, vteam = ev["killer"], ev["victim"]["team"]
        if kl and not ev["ff"] and kl["team"] in (0, 1):
            team_kills[kl["team"]] += 1
        elif ev["suicide"] and vteam in (0, 1):
            team_kills[1 - vteam] += 1
    t_end = round(end_frame * FRAME_S, 1)
    for name in sorted(set(stats) | set(summary)):
        s = stats.get(name, {"time": [0, 0], "classes": {}})
        su = summary.get(name, {})
        secs = [round(x * FRAME_S) for x in s["time"]]
        if not sum(secs) and not su:
            continue
        out.append({"ev": "player", "t": t_end, "name": name, "bot": 0,
                    "team": 0 if secs[0] >= secs[1] else 1, "time": secs,
                    "classes": {c: round(n * FRAME_S) for c, n in s["classes"].items()},
                    "kills": su.get("kills", 0), "deaths": su.get("deaths", 0),
                    "suicides": su.get("suicides", 0), "tk": su.get("tk", 0), "hs": su.get("hs", 0),
                    "objs": 0, "best_streak": su.get("best_streak", 0),
                    "helmet_saves": su.get("helmet_saves", 0), "foot_saves": su.get("foot_saves", 0),
                    "hits": 0, "misses": 0, "score": 0, "points": 0})
    out.append({"ev": "match_end", "t": t_end, "ts": int(round(end_ts)), "dur": dur,
                "winner": winner, "reason": reason,
                "teams": [{"idx": 0, "score": 0, "kills": team_kills[0], "losses": team_kills[1]},
                          {"idx": 1, "score": 0, "kills": team_kills[1], "losses": team_kills[0]}]})
    report = {"match": match_id, "map": mapname, "dur": dur, "winner": winner,
              "reason": reason, "end_kills_msg": end_kills, "team_kills": team_kills,
              "kills": sum(1 for f, _ in kills if start_frame <= f <= end_frame),
              "players": len([e for e in out if e["ev"] == "player"]),
              "unparsed": unknown[:10]}
    return match_id, out, report


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("mvd", type=Path)
    ap.add_argument("--server", required=True, help="id del servidor de origen (p. ej. dday.cl-duelos1)")
    ap.add_argument("--kind", choices=("public", "duel"), default="duel")
    ap.add_argument("--event", default="", help="torneo (stats_event); vacío = casual")
    ap.add_argument("--end", help="hora de fin de la partida, ej. '2026-10-26 00:40-03' "
                                  "(por defecto, la fecha del archivo)")
    ap.add_argument("--weapons", type=Path, default=Path(__file__).with_name("mvd_weapons.json"),
                    help="(ejército|modelo del arma) -> nombre del arma, sacado de src/*/*_item.c")
    ap.add_argument("--out", type=Path, default=Path("."), help="carpeta de salida")
    args = ap.parse_args()

    if args.end:
        end_ts = datetime.fromisoformat(args.end.replace(" ", "T")).timestamp()
    else:
        end_ts = args.mvd.stat().st_mtime
    weapons = json.loads(args.weapons.read_text()) if args.weapons.exists() else {}
    match_id, events, report = convert(args.mvd, args.server, args.kind, args.event.strip()[:64],
                                       end_ts, weapons)
    args.out.mkdir(parents=True, exist_ok=True)
    dest = args.out / f"{match_id}.jsonl"
    with open(dest, "w", encoding="utf-8") as f:
        for e in events:
            f.write(json.dumps(e, ensure_ascii=False) + "\n")
    print(json.dumps({"file": str(dest), **report}, ensure_ascii=False, indent=1))
    return 0


if __name__ == "__main__":
    sys.exit(main())
