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

Cada archivo se procesa en una transaccion. Si la partida ya existe en la
base, el archivo se considera procesado (idempotente). Los archivos
procesados se mueven a <events>/processed/ (o se borran con --delete).

Dependencia: psycopg 3 (pip install "psycopg[binary]").
"""

import argparse
import json
import logging
import os
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


@dataclass
class Config:
    min_humans: int = 2          # humanos minimos por equipo para rankear (publico)
    min_duration: float = 300.0  # segundos minimos para rankear (publico)
    duel_min_humans: int = 1     # idem para duelos (stats_mode duel)
    duel_min_duration: float = 120.0
    min_participation: float = 0.2  # fraccion minima del tiempo para contar en el Elo
    bots_as_humans: bool = False    # solo para pruebas: los bots cuentan como humanos


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
                          ("deflected", "deflected")):
            setattr(self, attr, getattr(self, attr) + int(ev.get(key) or 0))
        self.best_streak = max(self.best_streak, int(ev.get("best_streak") or 0))

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

    @property
    def id(self):
        return self.start["match"]

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
    if start.get("v") != 1:
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
    return Match(start, end, players, kills, objectives, event.strip()[:64])


def team_humans(match, team):
    return sum(1 for p in match.players.values()
               if not p.bot and p.team == team and p.total_time > 0)


def is_ranked(match, cfg):
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
        ranked = is_ranked(match, self.cfg)
        started = ts(match.start["ts"])
        ended = ts(match.end["ts"]) if match.end.get("ts") else None
        teams_start = {t["idx"]: t for t in match.start.get("teams", [])}
        teams_end = {t["idx"]: t for t in match.end.get("teams", [])}
        winner = match.winner
        humans = sum(1 for p in match.players.values() if not p.bot and p.total_time > 0)

        with self.conn.transaction(), self.conn.cursor() as cur:
            cur.execute(
                """INSERT INTO matches (id, server, map, mode, kind, event, season_id, tournament,
                       started_at, ended_at, duration_s, winner, end_reason, team0_army, team1_army,
                       team0_score, team1_score, team0_kills, team1_kills, humans, ranked)
                   VALUES (%s,%s,%s,%s,%s,%s,
                           (SELECT id FROM seasons WHERE starts_at <= %s ORDER BY starts_at DESC LIMIT 1),
                           %s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s)
                   ON CONFLICT (id) DO NOTHING""",
                (match.id, match.start.get("server", ""), match.start.get("map", ""),
                 match.start.get("mode", "dm"), match.kind, match.event, started,
                 bool(match.start.get("tournament")),
                 started, ended, match.duration, winner, match.end.get("reason", "other"),
                 teams_start.get(0, {}).get("army"), teams_start.get(1, {}).get("army"),
                 teams_end.get(0, {}).get("score"), teams_end.get(1, {}).get("score"),
                 teams_end.get(0, {}).get("kills"), teams_end.get(1, {}).get("kills"),
                 humans, ranked))
            if cur.rowcount == 0:
                log.info("%s: ya existe, se omite", match.id)
                return

            seen = ended or started
            pids = {}
            for key, p in match.players.items():
                pid = self.player_id(cur, p.name, p.bot, seen)
                pids[key] = pid
                if winner is None or p.team is None:
                    result = None
                elif winner == -1:
                    result = "D"
                else:
                    result = "W" if p.team == winner else "L"
                cur.execute(
                    """INSERT INTO match_players (match_id, player_id, team, time_team0, time_team1,
                           kills, deaths, human_kills, human_deaths, suicides, teamkills, headshots,
                           objectives, best_streak, helmet_saves, foot_saves, deflected,
                           hits, misses, score, points, classes, main_class, result)
                       VALUES (%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s)""",
                    (match.id, pid, p.team, p.time[0], p.time[1], p.kills, p.deaths,
                     p.human_kills, p.human_deaths,
                     p.suicides, p.tk, p.hs, p.objs, p.best_streak, p.helmet_saves, p.foot_saves,
                     p.deflected, p.hits, p.misses, p.score, p.points,
                     json.dumps(p.classes), p.main_class, result))

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
                             k.get("dist"), kpos[0], kpos[1], kpos[2], vpos[0], vpos[1], vpos[2]))
            if rows:
                cur.executemany(
                    """INSERT INTO kills (match_id, t, killer_id, victim_id, killer_team, victim_team,
                           killer_class, victim_class, mod, weapon, headshot, friendly_fire, suicide,
                           distance, killer_x, killer_y, killer_z, victim_x, victim_y, victim_z)
                       VALUES (%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s)""",
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

            # tablas de resumen del ladder (en la misma transaccion)
            cur.execute("SELECT apply_rollups(%s)", ([match.id],))

            if ranked:
                apply_elo(cur, match.id, self.cfg.min_participation)

        log.info("%s: %s (%s, %s%s), %d jugadores, %d kills%s", match.id, match.start.get("map"),
                 match.start.get("mode", "dm"), match.kind,
                 f", torneo {match.event}" if match.event else "",
                 len(match.players), len(match.kills), ", rankeada" if ranked else "")


def apply_elo(cur, match_id, min_participation):
    """Elo por equipos. Usa match_players de la partida ya insertada.

    Se actualizan dos ratings de la categoria de la partida (public, duel u
    official): el de la temporada de la partida y el historico (season_id = 0).
    Cada uno en dos versiones: el general (mode = 'all', todos los modos) y el
    del modo de la partida (dm, ctb, control...).
    """
    cur.execute("SELECT duration_s, winner, match_category(kind, event), season_id, mode"
                " FROM matches WHERE id = %s", (match_id,))
    duration, winner, kind, season_id, match_mode = cur.fetchone()
    if winner not in (0, 1, -1) or not duration:
        return
    modes = ["all"]
    if match_mode and match_mode != "all":
        modes.append(match_mode)
    for scope in (season_id, 0):
        for mode in modes:
            _apply_elo_scope(cur, match_id, duration, winner, kind, mode, scope, min_participation)


def _apply_elo_scope(cur, match_id, duration, winner, kind, mode, season_id, min_participation):
    cur.execute(
        """SELECT mp.player_id, mp.team, mp.time_team0, mp.time_team1,
                  coalesce(r.rating, %s), coalesce(r.games, 0)
           FROM match_players mp
           JOIN players p ON p.id = mp.player_id
           LEFT JOIN ratings r ON r.player_id = mp.player_id AND r.kind = %s
                              AND r.mode = %s AND r.season_id = %s
           WHERE mp.match_id = %s AND NOT p.is_bot AND mp.team IS NOT NULL
           ORDER BY mp.player_id""",
        (ELO_START, kind, mode, season_id, match_id))
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
            """INSERT INTO ratings (player_id, kind, mode, season_id, rating, peak, games, wins,
                                   losses, draws, updated_at)
               VALUES (%s, %s, %s, %s, %s, greatest(%s, %s), 1, %s, %s, %s, now())
               ON CONFLICT (player_id, kind, mode, season_id) DO UPDATE SET
                 rating = EXCLUDED.rating,
                 peak = greatest(ratings.peak, EXCLUDED.rating),
                 games = ratings.games + 1,
                 wins = ratings.wins + EXCLUDED.wins,
                 losses = ratings.losses + EXCLUDED.losses,
                 draws = ratings.draws + EXCLUDED.draws,
                 updated_at = now()""",
            (pid, kind, mode, season_id, new_rating, ELO_START, new_rating, win, loss, draw))
        cur.execute(
            """INSERT INTO rating_history (match_id, player_id, mode, season_id, rating_before,
                                          rating_after)
               VALUES (%s, %s, %s, %s, %s, %s)""",
            (match_id, pid, mode, season_id, rating, new_rating))


def rebuild_ratings(conn, cfg):
    with conn.transaction(), conn.cursor() as cur:
        cur.execute("DELETE FROM rating_history")
        cur.execute("DELETE FROM ratings")
        cur.execute("SELECT id FROM matches WHERE ranked ORDER BY started_at, id")
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
                 min_participation=args.min_participation, bots_as_humans=args.bots_as_humans)

    maintenance = (args.init_schema or args.rebuild_ratings or args.rebuild_stats
                   or args.new_season or args.set_event)
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
                # ratings recreada por una migracion (o sin Elo por modo): se recalcula sola
                if conn.execute("SELECT EXISTS (SELECT 1 FROM matches WHERE ranked)"
                                " AND (NOT EXISTS (SELECT 1 FROM ratings)"
                                "      OR NOT EXISTS (SELECT 1 FROM ratings WHERE mode <> 'all'))"
                                ).fetchone()[0]:
                    rebuild_elo = True
            if args.new_season:
                if new_season(conn, args.new_season, args.season_start):
                    args.rebuild_stats = rebuild_elo = True
            if args.set_event:
                set_event(conn, *args.set_event)
                args.rebuild_stats = rebuild_elo = True
            if args.rebuild_stats:
                conn.execute("SELECT rebuild_rollups()")
                log.info("tablas de resumen recalculadas")
            if rebuild_elo:
                rebuild_ratings(conn, cfg)

    if args.events is None:
        return 0

    while True:
        files = pending_files(args.events, args.stale_hours)
        try:
            # solo se conecta si hay algo que cargar; conexion nueva en cada
            # vuelta para sobrevivir a reinicios de PostgreSQL
            if files:
                with psycopg.connect(args.dsn, autocommit=True) as conn:
                    run_once(conn, args, cfg, files)
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
