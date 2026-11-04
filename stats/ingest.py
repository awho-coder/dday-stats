#!/usr/bin/env python3
"""
Ingestor de estadisticas de D-Day: Normandy.

Lee los archivos JSONL que escribe la DLL (uno por partida, ver
docs/stats/PLAN.md), los inserta en PostgreSQL y actualiza el rating Elo.

Uso:
    ingest.py --dsn postgresql://dday@localhost/dday --events /srv/q2/dday/stats/events
    ingest.py ... --loop 60          # revisar cada 60 s (servicio)
    ingest.py ... --rebuild-ratings  # recalcular todo el Elo desde cero
    ingest.py ... --init-schema      # crear / actualizar tablas y vistas
    ingest.py ... --skip-bot-only    # no cargar partidas sin ningun humano
    ingest.py ... --reingest A.jsonl # volver a cargar partidas ya cargadas (borra y recarga)

Cada archivo se procesa en una transaccion. Si la partida ya existe en la
base, el archivo se considera procesado (idempotente). Los archivos
procesados se mueven a <events>/processed/ (o se borran con --delete).

Desde el panel admin: con --mvd-inbox y --demos (o DDAY_STATS_MVD_INBOX y
DDAY_STATS_DEMOS) cada vuelta convierte primero los MVD que el panel dejo
pedidos (tabla mvd_imports, estados pending -> converted / duplicate / error):
deja el .jsonl en --events, la grabacion en <demos>/<match_id>.mvd2.gz y esa
misma vuelta carga la partida. Sin esas carpetas no importa nada.

Dependencia: psycopg 3 (pip install "psycopg[binary]").
"""

import argparse
import gzip
import json
import logging
import os
import shutil
import sys
import time
from dataclasses import dataclass, field
from datetime import datetime, timezone
from pathlib import Path

import psycopg

log = logging.getLogger("dday-ingest")

SCHEMA_FILE = Path(__file__).with_name("schema.sql")

# Elo
ELO_START = 1500.0
ELO_K_NEW = 40.0       # primeras partidas
ELO_K = 24.0
ELO_NEW_GAMES = 10

# modos sin equipos: tienen puestos (place) y su propio ladder; no mueven el Elo
FFA_MODES = ("ffa", "gungame")


@dataclass
class Config:
    min_humans: int = 2          # humanos minimos por equipo para rankear (publico)
    min_duration: float = 300.0  # segundos minimos para rankear (publico)
    duel_min_humans: int = 1     # idem para duelos (stats_mode duel)
    duel_min_duration: float = 120.0
    min_participation: float = 0.2  # fraccion minima del tiempo para contar en el Elo
    bots_as_humans: bool = False    # solo para pruebas: los bots cuentan como humanos
    ffa_min_humans: int = 3         # FFA: humanos minimos que terminaron la partida para rankear
    gg_min_duration: float = 120.0  # Gun Game: segundos minimos para rankear (termina cuando alguien gana)
    skip_bot_only: bool = False     # no cargar partidas sin ningun humano que haya jugado


@dataclass
class PlayerAgg:
    name: str
    bot: bool
    time: list = field(default_factory=lambda: [0, 0])
    classes: dict = field(default_factory=dict)
    kills: int = 0
    deaths: int = 0
    suicides: int = 0
    tk: int = 0
    hs: int = 0
    objs: int = 0
    hits: int = 0
    misses: int = 0
    score: int = 0
    points: int = 0
    human_kills: int = 0     # kills a enemigos humanos
    best_streak: int = 0     # la mejor de todos los tramos (no se suma)
    helmet_saves: int = 0    # el casco le desvio un tiro a la cabeza
    foot_saves: int = 0      # sobrevivio con "almost lost a foot"
    deflected: int = 0       # tiros suyos desviados por un casco
    human_deaths: int = 0    # muertes por humanos, suicidio o entorno
    zone_time: int = 0       # modo control: segundos dentro de la zona
    zone_kills: int = 0      # modo control: kills con el que mata o el que muere en la zona
    place: object = None     # FFA: puesto al terminar (None = se fue antes / otro modo)
    gg_level: object = None  # Gun Game: escalon al terminar o al irse (1 = el primero)

    def add(self, ev):
        t = ev.get("time") or [0, 0]
        self.time[0] += int(t[0])
        self.time[1] += int(t[1])
        for k, v in (ev.get("classes") or {}).items():
            self.classes[k] = self.classes.get(k, 0) + int(v)
        for attr, key in (("kills", "kills"), ("deaths", "deaths"), ("suicides", "suicides"),
                          ("tk", "tk"), ("hs", "hs"), ("objs", "objs"), ("hits", "hits"),
                          ("misses", "misses"), ("score", "score"), ("points", "points"),
                          ("helmet_saves", "helmet_saves"), ("foot_saves", "foot_saves"),
                          ("deflected", "deflected"), ("zone_time", "zone_time"),
                          ("zone_kills", "zone_kills")):
            setattr(self, attr, getattr(self, attr) + int(ev.get(key) or 0))
        self.best_streak = max(self.best_streak, int(ev.get("best_streak") or 0))
        if ev.get("place"):
            self.place = int(ev["place"])
        if ev.get("gg_level") is not None:
            self.gg_level = int(ev["gg_level"])  # el ultimo tramo manda

    @property
    def total_time(self):
        return self.time[0] + self.time[1]

    @property
    def team(self):
        if not self.total_time:
            return None
        return 0 if self.time[0] >= self.time[1] else 1

    @property
    def main_class(self):
        if not self.classes:
            return None
        return max(sorted(self.classes), key=lambda c: self.classes[c])


def ts(value):
    return datetime.fromtimestamp(int(value), tz=timezone.utc)


def read_events(path):
    """Devuelve la lista de eventos validos. Ignora lineas corruptas
    (por ejemplo la ultima linea de un .part de un servidor caido)."""
    events = []
    with open(path, "r", encoding="utf-8", errors="replace") as f:
        for n, line in enumerate(f, 1):
            line = line.strip()
            if not line:
                continue
            try:
                ev = json.loads(line)
            except json.JSONDecodeError:
                log.warning("%s:%d: linea invalida, se ignora", path.name, n)
                continue
            if isinstance(ev, dict) and "ev" in ev:
                events.append(ev)
    return events


@dataclass
class Match:
    start: dict
    end: dict
    players: dict          # (name, bot) -> PlayerAgg
    kills: list
    objectives: list
    event: str = ""        # torneo; '' = casual
    gg: list = field(default_factory=list)  # Gun Game: eventos "gg" (cambios de escalon)

    @property
    def id(self):
        return self.start["match"]

    @property
    def mode(self):
        # la DLL manda el Gun Game como "ffa" + variant "gungame" (un ingestor viejo lo carga como FFA)
        mode = self.start.get("mode", "dm")
        if mode == "ffa" and self.start.get("variant") == "gungame":
            return "gungame"
        return mode

    @property
    def gg_steps(self):
        """Escalones de la escalera. Partidas de antes de que la DLL lo mandara: el escalon
        del ganador (gana en el ultimo, el cuchillo)."""
        if self.mode != "gungame":
            return None
        if self.start.get("gg_steps"):
            return int(self.start["gg_steps"])
        winner = self.end.get("ffa_winner")
        levels = [p.gg_level for p in self.players.values() if p.gg_level and p.name == winner]
        return max(levels) if levels else None

    @property
    def kind(self):
        return "duel" if self.start.get("kind") == "duel" else "public"

    @property
    def duration(self):
        return float(self.end.get("dur") or 0)

    @property
    def winner(self):
        w = self.end.get("winner")
        return w if w in (0, 1, -1) else None


class SkipMatch(Exception):
    """Archivo valido que no se debe cargar."""


def strip_bot_flags(events):
    """Modo prueba: marca a todos los jugadores como humanos."""
    for e in events:
        if "bot" in e:
            e["bot"] = 0
        for role in ("killer", "victim", "player", "by"):
            if isinstance(e.get(role), dict):
                e[role]["bot"] = 0


def build_match(events, path):
    start = next((e for e in events if e["ev"] == "match_start"), None)
    if not start or not start.get("match"):
        raise ValueError("sin match_start")
    if start.get("v") not in (1, 2):
        raise ValueError(f"version de formato no soportada: {start.get('v')}")

    # duelo: solo cuenta lo que viene despues de la ultima cuenta regresiva
    # (lo anterior es calentamiento o un duelo cancelado con sv resetcount)
    first_t = 0.0
    event = start.get("event") or ""
    if start.get("kind") == "duel":
        lives = [i for i, e in enumerate(events) if e["ev"] == "live"]
        if not lives:
            raise SkipMatch("duelo sin cuenta regresiva")
        live = events[lives[-1]]
        first_t = float(live.get("t") or 0)
        # el torneo vale el que tenia la cvar al empezar el duelo
        event = live.get("event", event) or ""
        events = [start] + events[lives[-1] + 1:]

    end = next((e for e in events if e["ev"] == "match_end"), None)
    if end is None:
        # partida cortada (servidor caido): se cierra como abortada
        last_t = max((float(e.get("t") or 0) for e in events), default=0.0)
        end = {"ev": "match_end", "t": last_t, "dur": max(0.0, last_t - first_t), "winner": None,
               "reason": "aborted", "teams": [],
               "ts": int(path.stat().st_mtime)}

    players = {}
    for e in events:
        if e["ev"] == "player":
            key = (e["name"], bool(e.get("bot")))
            players.setdefault(key, PlayerAgg(e["name"], bool(e.get("bot")))).add(e)

    kills = [e for e in events if e["ev"] == "kill"]
    objectives = [e for e in events if e["ev"] == "obj"]
    gg = [e for e in events if e["ev"] == "gg" and isinstance(e.get("player"), dict)]

    # si el servidor se cayo no hay eventos "player": se reconstruye lo
    # posible (kills / muertes) a partir de las kills
    summarized = set(players)
    streak = {}
    for k in events:
        if k["ev"] == "team":
            # el juego reinicia la racha al cambiar de equipo u observar
            streak[(k.get("name"), bool(k.get("bot")))] = 0
            continue
        if k["ev"] == "luck":
            p_key = (k["player"]["name"], bool(k["player"].get("bot")))
            if p_key not in summarized:
                p = players.setdefault(p_key, PlayerAgg(*p_key))
                if k.get("type") == "helmet":
                    p.helmet_saves += 1
                else:
                    p.foot_saves += 1
            by = k.get("by")
            if by and k.get("type") == "helmet":
                b_key = (by["name"], bool(by.get("bot")))
                if b_key not in summarized:
                    players.setdefault(b_key, PlayerAgg(*b_key)).deflected += 1
            continue
        if k["ev"] != "kill":
            continue
        victim, killer = k.get("victim"), k.get("killer")
        actors = [("victim", victim)] + ([("killer", killer)] if killer else [])
        for role, actor in actors:
            key = (actor["name"], bool(actor.get("bot")))
            if key in summarized:
                continue
            p = players.setdefault(key, PlayerAgg(key[0], key[1]))
            if role == "victim":
                p.deaths += 1
                p.suicides += 1 if k.get("suicide") else 0
                streak[key] = 0
            elif k.get("ff"):
                p.tk += 1
                streak[key] = 0
            else:
                p.kills += 1
                p.hs += 1 if k.get("hs") else 0
                p.zone_kills += 1 if (k.get("kzone") or k.get("vzone")) else 0
                # como en el juego, la kill por desangrado no suma a la racha
                if k.get("mod") != "wound":
                    streak[key] = streak.get(key, 0) + 1
                    p.best_streak = max(p.best_streak, streak[key])

    # enfrentamientos entre humanos (para el ladder): no cuentan las kills a
    # bots ni las muertes causadas por bots
    for k in kills:
        victim, killer = k["victim"], k.get("killer")
        vkey = (victim["name"], bool(victim.get("bot")))
        if killer is None or not killer.get("bot"):
            players[vkey].human_deaths += 1
        if killer and not k.get("ff") and not victim.get("bot"):
            players[(killer["name"], bool(killer.get("bot")))].human_kills += 1
    return Match(start, end, players, kills, objectives, event.strip()[:64], gg)


def team_humans(match, team):
    return sum(1 for p in match.players.values()
               if not p.bot and p.team == team and p.total_time > 0)


def ffa_finishers(match):
    """Humanos que terminaron una partida FFA (tienen puesto)."""
    return sum(1 for p in match.players.values()
               if not p.bot and p.place is not None and p.total_time > 0)


def ffa_result(match, p):
    """W al unico primero, D a los empatados en el primer puesto, L al resto;
    None a quien se fue antes (sin puesto)."""
    if p.place is None:
        return None
    if p.place != 1:
        return "L"
    firsts = sum(1 for q in match.players.values() if q.place == 1)
    return "W" if firsts == 1 else "D"


def is_ranked(match, cfg):
    if match.mode == "gungame":
        # rankeada solo para el ladder Gun Game: gana quien mata con el cuchillo en el
        # ultimo escalon, asi que una partida corta igual tiene un ganador real
        return (match.end.get("reason") == "normal"
                and bool(match.end.get("ffa_winner"))
                and match.duration >= cfg.gg_min_duration
                and ffa_finishers(match) >= cfg.ffa_min_humans)
    if match.mode == "ffa":
        # rankeada solo para el ladder FFA: no mueve el Elo
        return (match.end.get("reason") == "normal"
                and match.duration >= cfg.min_duration
                and ffa_finishers(match) >= cfg.ffa_min_humans)
    if match.kind == "duel":
        min_humans, min_duration = cfg.duel_min_humans, cfg.duel_min_duration
    else:
        min_humans, min_duration = cfg.min_humans, cfg.min_duration
    return (match.end.get("reason") == "normal"
            and match.winner is not None
            and match.duration >= min_duration
            and team_humans(match, 0) >= min_humans
            and team_humans(match, 1) >= min_humans)


class Ingestor:
    def __init__(self, conn, cfg):
        self.conn = conn
        self.cfg = cfg

    @staticmethod
    def player_id(cur, name, bot, seen):
        cur.execute(
            """INSERT INTO players (name, is_bot, first_seen, last_seen)
               VALUES (%s, %s, %s, %s)
               ON CONFLICT (name, is_bot) DO UPDATE
                 SET last_seen = greatest(players.last_seen, EXCLUDED.last_seen),
                     first_seen = least(players.first_seen, EXCLUDED.first_seen)
               RETURNING id""",
            (name, bot, seen, seen))
        return cur.fetchone()[0]

    def ingest_file(self, path):
        """Inserta la partida de un archivo. No hace nada si ya existe."""
        events = read_events(path)
        if self.cfg.bots_as_humans:
            strip_bot_flags(events)
        match = build_match(events, path)
        humans = sum(1 for p in match.players.values() if not p.bot and p.total_time > 0)
        if self.cfg.skip_bot_only and humans == 0:
            raise SkipMatch("partida sin humanos (--skip-bot-only)")
        ranked = is_ranked(match, self.cfg)
        # DLL nuevas: el reloj (t, dur) se detiene sin nadie en un equipo y "play"
        # marca la primera entrada. El mapa puede quedar cargado horas vacio (o
        # alguien entrar un momento y salir), asi que el inicio es el fin menos el
        # tiempo jugado, y nunca antes de "play". Archivos viejos: match_start.
        play = next((e for e in events if e["ev"] == "play" and e.get("ts")), None)
        if play is None:
            started = ts(match.start["ts"])
        elif match.end.get("ts"):
            started = ts(max(float(play["ts"]), float(match.end["ts"]) - match.duration))
        else:
            started = ts(play["ts"])
        ended = ts(match.end["ts"]) if match.end.get("ts") else None
        teams_start = {t["idx"]: t for t in match.start.get("teams", [])}
        teams_end = {t["idx"]: t for t in match.end.get("teams", [])}
        winner = match.winner

        with self.conn.transaction(), self.conn.cursor() as cur:
            cur.execute(
                """INSERT INTO matches (id, server, map, mode, kind, event, season_id, tournament,
                       started_at, ended_at, duration_s, winner, end_reason, team0_army, team1_army,
                       team0_score, team1_score, team0_kills, team1_kills, humans, ranked,
                       gg_steps, gg_ladder, ffa_weapon)
                   VALUES (%s,%s,%s,%s,%s,%s,
                           (SELECT id FROM seasons WHERE starts_at <= %s ORDER BY starts_at DESC LIMIT 1),
                           %s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s)
                   ON CONFLICT (id) DO NOTHING""",
                (match.id, match.start.get("server", ""), match.start.get("map", ""),
                 match.mode, match.kind, match.event, started,
                 bool(match.start.get("tournament")),
                 started, ended, match.duration, winner, match.end.get("reason", "other"),
                 teams_start.get(0, {}).get("army"), teams_start.get(1, {}).get("army"),
                 teams_end.get(0, {}).get("score"), teams_end.get(1, {}).get("score"),
                 teams_end.get(0, {}).get("kills"), teams_end.get(1, {}).get("kills"),
                 humans, ranked, match.gg_steps,
                 json.dumps(match.start["gg_ladder"]) if match.mode == "gungame" and match.start.get("gg_ladder") else None,
                 match.start.get("ffa_weapon") if match.mode == "ffa" else None))
            if cur.rowcount == 0:
                log.info("%s: ya existe, se omite", match.id)
                return

            seen = ended or started
            pids = {}
            for key, p in match.players.items():
                pid = self.player_id(cur, p.name, p.bot, seen)
                pids[key] = pid
                if match.mode in FFA_MODES:
                    result = ffa_result(match, p)
                elif winner is None or p.team is None:
                    result = None
                elif winner == -1:
                    result = "D"
                else:
                    result = "W" if p.team == winner else "L"
                cur.execute(
                    """INSERT INTO match_players (match_id, player_id, team, time_team0, time_team1,
                           kills, deaths, human_kills, human_deaths, suicides, teamkills, headshots,
                           objectives, best_streak, helmet_saves, foot_saves, deflected,
                           hits, misses, score, points, classes, main_class, result, zone_seconds,
                           zone_kills, place, gg_level)
                       VALUES (%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s)""",
                    (match.id, pid, p.team, p.time[0], p.time[1], p.kills, p.deaths,
                     p.human_kills, p.human_deaths,
                     p.suicides, p.tk, p.hs, p.objs, p.best_streak, p.helmet_saves, p.foot_saves,
                     p.deflected, p.hits, p.misses, p.score, p.points,
                     json.dumps(p.classes), p.main_class, result, p.zone_time, p.zone_kills,
                     p.place, p.gg_level if match.mode == "gungame" else None))

            def actor_id(name, bot):
                key = (name, bot)
                if key not in pids:
                    pids[key] = self.player_id(cur, name, bot, seen)
                return pids[key]

            def objective_player_id(name):
                # el evento obj no indica si es bot: se usa el que jugo la partida
                if (name, False) not in pids and (name, True) in pids:
                    return pids[(name, True)]
                return actor_id(name, False)

            rows = []
            for k in match.kills:
                v, kl = k["victim"], k.get("killer")
                vpos = v.get("pos") or [None] * 3
                kpos = (kl or {}).get("pos") or [None] * 3
                rows.append((match.id, float(k.get("t") or 0),
                             actor_id(kl["name"], bool(kl.get("bot"))) if kl else None,
                             actor_id(v["name"], bool(v.get("bot"))),
                             kl.get("team") if kl else None, v.get("team"),
                             kl.get("class") if kl else None, v.get("class"),
                             k.get("mod") or "unknown", k.get("weapon"),
                             bool(k.get("hs")), bool(k.get("ff")), bool(k.get("suicide")),
                             k.get("dist"), kpos[0], kpos[1], kpos[2], vpos[0], vpos[1], vpos[2],
                             bool(k["kzone"]) if "kzone" in k else None,
                             bool(k["vzone"]) if "vzone" in k else None))
            if rows:
                cur.executemany(
                    """INSERT INTO kills (match_id, t, killer_id, victim_id, killer_team, victim_team,
                           killer_class, victim_class, mod, weapon, headshot, friendly_fire, suicide,
                           distance, killer_x, killer_y, killer_z, victim_x, victim_y, victim_z,
                           killer_in_zone, victim_in_zone)
                       VALUES (%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s)""",
                    rows)

            rows = []
            for o in match.objectives:
                player = o.get("player")
                team = o.get("team")
                rows.append((match.id, float(o.get("t") or 0), o.get("type") or "unknown",
                             o.get("name"), team if team in (0, 1) else None,
                             objective_player_id(player) if player else None))
            if rows:
                cur.executemany(
                    "INSERT INTO objectives (match_id, t, type, name, team, player_id)"
                    " VALUES (%s,%s,%s,%s,%s,%s)", rows)

            # Gun Game: cada cambio de escalon (la carrera de niveles de la partida)
            rows = [(match.id, float(g.get("t") or 0),
                     actor_id(g["player"]["name"], bool(g["player"].get("bot"))),
                     int(g.get("level") or 0), g.get("weapon"), g.get("why") or "kill")
                    for g in match.gg] if match.mode == "gungame" else []
            if rows:
                cur.executemany(
                    "INSERT INTO gg_levels (match_id, t, player_id, level, weapon, why)"
                    " VALUES (%s,%s,%s,%s,%s,%s)", rows)

            # tablas de resumen del ladder (en la misma transaccion)
            cur.execute("SELECT apply_rollups(%s)", ([match.id],))

            # el FFA y el Gun Game tienen su propio ladder: solo los modos por equipos mueven el Elo
            if ranked and match.mode not in FFA_MODES:
                apply_elo(cur, match.id, self.cfg.min_participation)

        log.info("%s: %s (%s, %s%s), %d jugadores, %d kills%s", match.id, match.start.get("map"),
                 match.mode, match.kind,
                 f", torneo {match.event}" if match.event else "",
                 len(match.players), len(match.kills), ", rankeada" if ranked else "")


def apply_elo(cur, match_id, min_participation):
    """Elo por equipos. Usa match_players de la partida ya insertada.

    Se actualizan dos ratings: el de la temporada de la partida y el historico
    (season_id = 0), ambos de su categoria (public, duel u official).
    """
    cur.execute("SELECT duration_s, winner, match_category(kind, event), season_id"
                " FROM matches WHERE id = %s", (match_id,))
    duration, winner, kind, season_id = cur.fetchone()
    if winner not in (0, 1, -1) or not duration:
        return
    for scope in (season_id, 0):
        _apply_elo_scope(cur, match_id, duration, winner, kind, scope, min_participation)


def _apply_elo_scope(cur, match_id, duration, winner, kind, season_id, min_participation):
    cur.execute(
        """SELECT mp.player_id, mp.team, mp.time_team0, mp.time_team1,
                  coalesce(r.rating, %s), coalesce(r.games, 0)
           FROM match_players mp
           JOIN players p ON p.id = mp.player_id
           LEFT JOIN ratings r ON r.player_id = mp.player_id AND r.kind = %s
                              AND r.season_id = %s
           WHERE mp.match_id = %s AND NOT p.is_bot AND mp.team IS NOT NULL
           ORDER BY mp.player_id""",
        (ELO_START, kind, season_id, match_id))
    rows = cur.fetchall()

    parts = []
    for pid, team, t0, t1, rating, games in rows:
        participation = min(1.0, (t0 if team == 0 else t1) / duration)
        if participation < min_participation:
            continue
        parts.append((pid, team, participation, float(rating), games))

    team_rating = {}
    for team in (0, 1):
        members = [p for p in parts if p[1] == team]
        weight = sum(p[2] for p in members)
        if not members or weight <= 0:
            return
        team_rating[team] = sum(p[3] * p[2] for p in members) / weight

    expected0 = 1.0 / (1.0 + 10 ** ((team_rating[1] - team_rating[0]) / 400.0))
    score0 = 0.5 if winner == -1 else (1.0 if winner == 0 else 0.0)

    for pid, team, participation, rating, games in parts:
        expected = expected0 if team == 0 else 1.0 - expected0
        score = score0 if team == 0 else 1.0 - score0
        k = ELO_K_NEW if games < ELO_NEW_GAMES else ELO_K
        new_rating = rating + k * (score - expected) * participation
        win = 1 if score == 1.0 else 0
        loss = 1 if score == 0.0 else 0
        draw = 1 if score == 0.5 else 0
        cur.execute(
            """INSERT INTO ratings (player_id, kind, season_id, rating, peak, games, wins, losses,
                                   draws, updated_at)
               VALUES (%s, %s, %s, %s, greatest(%s, %s), 1, %s, %s, %s, now())
               ON CONFLICT (player_id, kind, season_id) DO UPDATE SET
                 rating = EXCLUDED.rating,
                 peak = greatest(ratings.peak, EXCLUDED.rating),
                 games = ratings.games + 1,
                 wins = ratings.wins + EXCLUDED.wins,
                 losses = ratings.losses + EXCLUDED.losses,
                 draws = ratings.draws + EXCLUDED.draws,
                 updated_at = now()""",
            (pid, kind, season_id, new_rating, ELO_START, new_rating, win, loss, draw))
        cur.execute(
            """INSERT INTO rating_history (match_id, player_id, season_id, rating_before, rating_after)
               VALUES (%s, %s, %s, %s, %s)""",
            (match_id, pid, season_id, rating, new_rating))


def rebuild_ratings(conn, cfg):
    with conn.transaction(), conn.cursor() as cur:
        cur.execute("DELETE FROM rating_history")
        cur.execute("DELETE FROM ratings")
        cur.execute("SELECT id FROM matches WHERE ranked AND mode NOT IN ('ffa', 'gungame')"
                    " ORDER BY started_at, id")
        ids = [r[0] for r in cur.fetchall()]
        for mid in ids:
            apply_elo(cur, mid, cfg.min_participation)
    log.info("rating recalculado con %d partidas rankeadas", len(ids))


def new_season(conn, name, start):
    """Las partidas que empiecen desde 'start' (por defecto, ahora) cuentan para
    la temporada nueva. Si ya hay partidas cargadas despues de esa fecha, se
    reasignan y se recalculan resumenes y ratings."""
    with conn.transaction():
        row = conn.execute(
            "INSERT INTO seasons (name, starts_at) VALUES (%s, coalesce(%s::timestamptz, now()))"
            " RETURNING id, starts_at", (name, start)).fetchone()
        moved = conn.execute(
            """UPDATE matches m SET season_id = (SELECT s.id FROM seasons s
                   WHERE s.starts_at <= m.started_at ORDER BY s.starts_at DESC LIMIT 1)
               WHERE m.started_at >= %s""", (row[1],)).rowcount
    log.info("temporada '%s' iniciada el %s", name, row[1])
    return moved


def set_event(conn, match_id, event):
    cur = conn.execute("UPDATE matches SET event = %s WHERE id = %s", (event.strip()[:64], match_id))
    if cur.rowcount == 0:
        raise SystemExit(f"no existe la partida {match_id}")
    log.info("partida %s marcada como %s", match_id, f"torneo '{event}'" if event else "casual")


def apply_event_changes(conn, cfg):
    """Aplica los cambios de torneo que el panel admin dejo pedidos (tabla
    match_event_changes): agregar o sacar partidas de un torneo. Si alguna
    partida cambio, recalcula resumenes y Elo (una sola vez por tanda).
    Devuelve cuantas partidas cambiaron de torneo."""
    pending = conn.execute(
        "SELECT EXISTS (SELECT 1 FROM match_event_changes WHERE applied_at IS NULL)").fetchone()[0]
    if not pending:
        return 0
    # todo en una transaccion: si el recalculo falla, los pedidos siguen pendientes
    # y se reintentan en la proxima vuelta (nunca quedan aplicados a medias)
    with conn.transaction():
        changed = conn.execute("SELECT apply_event_changes()").fetchone()[0]
        if changed:
            conn.execute("SELECT rebuild_rollups()")
            rebuild_ratings(conn, cfg)
    if changed:
        log.info("cambios de torneo aplicados: %d partidas; resumenes y Elo recalculados", changed)
    return changed


def save_demo(src, dest):
    """Deja la grabacion en dest (<match_id>.mvd2.gz). Si ya existe no hace nada; si src ya
    es gzip lo copia tal cual y si no, lo comprime. Escribe a dest.tmp y renombra."""
    if dest.exists():
        return
    dest.parent.mkdir(parents=True, exist_ok=True)
    tmp = dest.with_name(dest.name + ".tmp")
    try:
        with open(src, "rb") as f:
            gz = f.read(2) == b"\x1f\x8b"
        if gz:
            shutil.copyfile(src, tmp)
        else:
            with open(src, "rb") as fin, gzip.open(tmp, "wb") as fout:
                shutil.copyfileobj(fin, fout, 1024 * 1024)
        tmp.replace(dest)
    finally:
        tmp.unlink(missing_ok=True)


def finish_import_file(src, ok):
    """Archivo subido ya procesado: si salio bien se borra; si no, va a <inbox>/failed/
    para poder revisarlo (y alli se borran los de mas de 14 dias)."""
    if ok:
        src.unlink(missing_ok=True)
        return
    failed = src.parent / "failed"
    failed.mkdir(exist_ok=True)
    if src.exists():
        src.replace(failed / src.name)
    limit = time.time() - 14 * 86400
    for old in failed.iterdir():
        try:
            if old.is_file() and old.stat().st_mtime < limit:
                old.unlink()
        except OSError:
            pass


def process_mvd_imports(conn, inbox, demos, events_dir):
    """Convierte los MVD que el panel admin dejo pedidos (tabla mvd_imports).
    Cada fila en su transaccion, con SKIP LOCKED (el cron y el servicio pueden correr a la vez).
    Un error de un archivo se anota en su fila y no corta la vuelta. Devuelve cuantas proceso."""
    import mvd2jsonl  # mismo directorio que ingest.py
    done = 0
    while True:
        with conn.transaction():
            row = conn.execute(
                "SELECT id, file_key, server, kind, event, end_at FROM mvd_imports"
                " WHERE status = 'pending' ORDER BY id LIMIT 1 FOR UPDATE SKIP LOCKED").fetchone()
            if row is None:
                return done
            imp_id, key, server, kind, event, end_at = row
            src = inbox / f"{key}.mvd2"
            status, match_id, message, report = "error", None, "", None
            dest = None
            try:
                if not src.is_file():
                    raise SystemExit("no esta el archivo subido")
                match_id, dest, report = mvd2jsonl.convert_to_file(
                    src, server, kind, event, end_at.timestamp(), events_dir)
                exists = conn.execute("SELECT 1 FROM matches WHERE id = %s", (match_id,)).fetchone()
                status = "duplicate" if exists else "converted"
                if exists:
                    dest.unlink(missing_ok=True)  # ya cargada: no volver a dejar el .jsonl
                save_demo(src, demos / f"{match_id}.mvd2.gz")
            except psycopg.Error:
                raise  # la transaccion quedo abortada: la fila sigue pendiente y se reintenta
            except SystemExit as e:
                message = str(e).replace(f"{inbox}{os.sep}", "")[:300]  # sin la ruta del servidor
            except Exception as e:  # noqa: BLE001 - un MVD raro no debe tumbar el ingestor
                log.exception("importacion %s", imp_id)
                message = f"error inesperado: {type(e).__name__}"
            if status == "error":
                match_id = None
                if dest is not None:
                    dest.unlink(missing_ok=True)  # no dejar un .jsonl de una importacion fallida
            conn.execute(
                "UPDATE mvd_imports SET status = %s, match_id = %s, message = %s, report = %s,"
                " processed_at = now() WHERE id = %s",
                (status, match_id, message, json.dumps(report) if report and status != "error" else None, imp_id))
        finish_import_file(src, ok=status != "error")
        log.info("importacion MVD %s: %s %s %s", imp_id, status, match_id or "", message)
        done += 1


def register_imported_clans(conn):
    """Partidas de torneo importadas desde el panel y ya cargadas: inscribe en el torneo los
    clanes de cada lado que falten (mvd_import_register_clans). Cada importacion se revisa
    una sola vez (clans_added deja de ser NULL). Devuelve cuantas reviso."""
    rows = conn.execute(
        "SELECT i.id FROM mvd_imports i"
        " WHERE i.status IN ('converted', 'duplicate') AND i.event <> '' AND i.clans_added IS NULL"
        " AND EXISTS (SELECT 1 FROM matches m WHERE m.id = i.match_id) ORDER BY i.id").fetchall()
    for (imp_id,) in rows:
        tags = conn.execute("SELECT mvd_import_register_clans(%s)", (imp_id,)).fetchone()[0]
        if tags:
            log.info("importacion MVD %s: clanes inscritos en el torneo: %s", imp_id, ", ".join(tags))
    return len(rows)


def reingest(conn, cfg, paths):
    """Vuelve a cargar partidas ya cargadas (por ejemplo, para que una version nueva del
    ingestor las clasifique bien): borra cada partida (sus filas se borran en cascada), la
    carga otra vez desde su archivo y despues recalcula los resumenes y, si alguna partida
    por equipos cambio, el Elo. Todo en una transaccion. Los archivos no se mueven."""
    ingestor = Ingestor(conn, cfg)
    team_modes = False
    with conn.transaction():
        for path in paths:
            events = read_events(path)
            start = next((e for e in events if e["ev"] == "match_start"), None)
            if not start or not start.get("match"):
                raise SystemExit(f"{path}: sin match_start")
            old = conn.execute("SELECT mode, ranked FROM matches WHERE id = %s", (start["match"],)).fetchone()
            conn.execute("DELETE FROM matches WHERE id = %s", (start["match"],))
            ingestor.ingest_file(path)
            new = conn.execute("SELECT mode, ranked FROM matches WHERE id = %s", (start["match"],)).fetchone()
            log.info("%s: %s -> %s", start["match"], old[0] if old else "(no estaba)", new[0] if new else "(omitida)")
            for row in (old, new):
                if row and row[0] not in FFA_MODES and row[1]:
                    team_modes = True
        conn.execute("SELECT rebuild_rollups()")
        if team_modes:
            rebuild_ratings(conn, cfg)
    log.info("recargadas %d partidas; resumenes%s recalculados", len(paths), " y Elo" if team_modes else "")


def pending_files(events_dir, stale_hours):
    files = sorted(events_dir.glob("*.jsonl"))
    if stale_hours > 0:
        limit = time.time() - stale_hours * 3600
        files += sorted(p for p in events_dir.glob("*.jsonl.part") if p.stat().st_mtime < limit)
    # orden cronologico (el nombre empieza con la fecha UTC)
    return sorted(files, key=lambda p: p.name)


def finish_file(path, delete):
    if delete:
        path.unlink()
        return
    dest_dir = path.parent / "processed"
    dest_dir.mkdir(exist_ok=True)
    name = path.name[:-5] if path.name.endswith(".part") else path.name
    path.replace(dest_dir / name)  # replace: en Windows rename falla si el destino existe


def run_once(conn, args, cfg, files):
    ingestor = Ingestor(conn, cfg)
    ok = failed = 0
    for path in files:
        try:
            ingestor.ingest_file(path)
            finish_file(path, args.delete)
            ok += 1
        except SkipMatch as e:
            log.info("%s: %s, se omite", path.name, e)
            finish_file(path, args.delete)
        except (ValueError, KeyError, TypeError) as e:
            # archivo malformado: se aparta para revisarlo a mano
            log.error("%s: %s", path.name, e)
            bad = path.parent / "failed"
            bad.mkdir(exist_ok=True)
            path.replace(bad / path.name)
            failed += 1
    if ok or failed:
        log.info("procesados: %d, con error: %d", ok, failed)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--dsn", default=os.environ.get("DDAY_STATS_DSN", ""),
                    help="cadena de conexion PostgreSQL (o variable DDAY_STATS_DSN)")
    ap.add_argument("--events", type=Path, default=os.environ.get("DDAY_STATS_EVENTS"),
                    help="carpeta de eventos (dday/stats/events)")
    ap.add_argument("--mvd-inbox", type=Path, default=os.environ.get("DDAY_STATS_MVD_INBOX") or None,
                    help="carpeta donde el panel admin deja los MVD a importar (vacio = no importa)")
    ap.add_argument("--demos", type=Path, default=os.environ.get("DDAY_STATS_DEMOS") or None,
                    help="carpeta de grabaciones del backend (<match_id>.mvd2.gz)")
    ap.add_argument("--loop", type=int, default=0, metavar="SEG",
                    help="repetir cada SEG segundos en vez de terminar")
    ap.add_argument("--delete", action="store_true", help="borrar archivos procesados en vez de moverlos")
    ap.add_argument("--stale-hours", type=float, default=6,
                    help="procesar .part sin cambios hace mas de N horas como partida abortada (0 = nunca)")
    ap.add_argument("--min-humans", type=int, default=Config.min_humans)
    ap.add_argument("--min-duration", type=float, default=Config.min_duration)
    ap.add_argument("--duel-min-humans", type=int, default=Config.duel_min_humans)
    ap.add_argument("--duel-min-duration", type=float, default=Config.duel_min_duration)
    ap.add_argument("--min-participation", type=float, default=Config.min_participation)
    ap.add_argument("--bots-as-humans", action="store_true",
                    help="SOLO PRUEBAS: cargar a los bots como si fueran humanos")
    ap.add_argument("--ffa-min-humans", type=int, default=Config.ffa_min_humans,
                    help="FFA: humanos minimos que terminaron la partida para que sea rankeada")
    ap.add_argument("--gg-min-duration", type=float, default=Config.gg_min_duration,
                    help="Gun Game: segundos minimos para que la partida sea rankeada")
    ap.add_argument("--reingest", nargs="+", type=Path, metavar="ARCHIVO",
                    help="volver a cargar estas partidas (.jsonl ya procesados): las borra y las recarga")
    ap.add_argument("--skip-bot-only", action="store_true",
                    default=os.environ.get("DDAY_STATS_SKIP_BOT_ONLY", "").lower() in ("1", "true", "yes", "on"),
                    help="no cargar partidas sin ningun humano que haya jugado (o DDAY_STATS_SKIP_BOT_ONLY=1)")
    ap.add_argument("--init-schema", action="store_true", help="crear / actualizar tablas y vistas")
    ap.add_argument("--rebuild-ratings", action="store_true", help="recalcular el Elo desde cero")
    ap.add_argument("--rebuild-stats", action="store_true",
                    help="recalcular las tablas de resumen del ladder desde cero")
    ap.add_argument("--new-season", metavar="NOMBRE",
                    help="iniciar una temporada nueva (desde ahora, o desde --season-start)")
    ap.add_argument("--season-start", metavar="FECHA",
                    help="inicio de la temporada nueva, ej. '2026-11-01 00:00-03'")
    ap.add_argument("--set-event", nargs=2, metavar=("PARTIDA", "TORNEO"),
                    help="marcar una partida como de un torneo ('' para casual) y recalcular")
    ap.add_argument("--apply-event-changes", action="store_true",
                    help="aplicar ahora los cambios de torneo pedidos desde la app y recalcular")
    ap.add_argument("--log-file", metavar="ARCHIVO",
                    help="escribir el registro en un archivo (util en servicios sin consola)")
    ap.add_argument("-v", "--verbose", action="store_true")
    args = ap.parse_args()

    logging.basicConfig(level=logging.DEBUG if args.verbose else logging.INFO,
                        format="%(asctime)s %(levelname)s %(message)s",
                        filename=args.log_file, encoding="utf-8")

    if not args.dsn:
        ap.error("falta --dsn o DDAY_STATS_DSN")

    cfg = Config(min_humans=args.min_humans, min_duration=args.min_duration,
                 duel_min_humans=args.duel_min_humans, duel_min_duration=args.duel_min_duration,
                 min_participation=args.min_participation, bots_as_humans=args.bots_as_humans,
                 ffa_min_humans=args.ffa_min_humans, gg_min_duration=args.gg_min_duration,
                 skip_bot_only=args.skip_bot_only)

    maintenance = (args.init_schema or args.rebuild_ratings or args.rebuild_stats
                   or args.new_season or args.set_event or args.apply_event_changes or args.reingest)
    if args.events is None and not maintenance:
        ap.error("falta --events o DDAY_STATS_EVENTS")
    if args.events is not None and not args.events.is_dir():
        ap.error(f"no existe la carpeta {args.events}")

    if maintenance:
        with psycopg.connect(args.dsn, autocommit=True) as conn:
            rebuild_elo = args.rebuild_ratings
            if args.init_schema:
                conn.execute(SCHEMA_FILE.read_text(encoding="utf-8"))
                log.info("esquema actualizado")
                # ratings recreada por una migracion: se recalcula sola
                if conn.execute("SELECT EXISTS (SELECT 1 FROM matches WHERE ranked AND mode NOT IN ('ffa', 'gungame'))"
                                " AND NOT EXISTS (SELECT 1 FROM ratings)").fetchone()[0]:
                    rebuild_elo = True
            if args.new_season:
                if new_season(conn, args.new_season, args.season_start):
                    args.rebuild_stats = rebuild_elo = True
            if args.set_event:
                set_event(conn, *args.set_event)
                args.rebuild_stats = rebuild_elo = True
            if args.apply_event_changes:
                # si hubo cambios ya recalcula resumenes y Elo por su cuenta
                apply_event_changes(conn, cfg)
            if args.reingest:
                # recalcula resumenes (y Elo si hace falta) por su cuenta
                reingest(conn, cfg, args.reingest)
            if args.rebuild_stats:
                conn.execute("SELECT rebuild_rollups()")
                log.info("tablas de resumen recalculadas")
            if rebuild_elo:
                rebuild_ratings(conn, cfg)

    if args.events is None:
        return 0

    while True:
        if args.mvd_inbox and args.demos:
            try:
                with psycopg.connect(args.dsn, autocommit=True) as conn:
                    process_mvd_imports(conn, args.mvd_inbox, args.demos, args.events)
            except psycopg.Error as e:
                log.error("importaciones MVD: error de base de datos: %s", e)
        files = pending_files(args.events, args.stale_hours)
        try:
            # conexion nueva en cada vuelta (sobrevive a reinicios de PostgreSQL):
            # primero carga los archivos nuevos, si hay, y despues aplica los
            # cambios de torneo pedidos desde la app
            with psycopg.connect(args.dsn, autocommit=True) as conn:
                if files:
                    run_once(conn, args, cfg, files)
                if args.mvd_inbox and args.demos:
                    register_imported_clans(conn)
                apply_event_changes(conn, cfg)
        except psycopg.Error as e:
            # el archivo queda donde estaba y se reintenta en la proxima vuelta
            log.error("error de base de datos: %s", e)
            if not args.loop:
                return 1
        if not args.loop:
            return 0
        time.sleep(args.loop)


if __name__ == "__main__":
    sys.exit(main())
