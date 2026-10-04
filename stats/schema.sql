-- Esquema de estadisticas de D-Day: Normandy.
-- Lo carga stats/ingest.py a partir de los archivos JSONL que escribe la DLL.
-- Es idempotente: se puede ejecutar varias veces (psql -f schema.sql).

BEGIN;

SET LOCAL client_min_messages = warning;

CREATE TABLE IF NOT EXISTS players (
    id          serial PRIMARY KEY,
    name        text        NOT NULL,
    is_bot      boolean     NOT NULL DEFAULT false,
    first_seen  timestamptz NOT NULL,
    last_seen   timestamptz NOT NULL,
    UNIQUE (name, is_bot)
);

-- Temporadas: cada una empieza en starts_at y dura hasta que empieza la
-- siguiente. Se crean con "ingest.py --new-season NOMBRE".
CREATE TABLE IF NOT EXISTS seasons (
    id          serial PRIMARY KEY,
    name        text        NOT NULL UNIQUE,
    starts_at   timestamptz NOT NULL UNIQUE
);
INSERT INTO seasons (name, starts_at)
SELECT 'Temporada 1', '-infinity' WHERE NOT EXISTS (SELECT 1 FROM seasons);

CREATE TABLE IF NOT EXISTS matches (
    id           text PRIMARY KEY,           -- <fecha>-<servidor>-<random>
    server       text        NOT NULL,
    map          text        NOT NULL,
    mode         text        NOT NULL,       -- dm, ctb, campaign
    kind         text        NOT NULL DEFAULT 'public',  -- public o duel (cvar stats_mode)
    event        text        NOT NULL DEFAULT '',        -- torneo (cvar stats_event); '' = casual
    season_id    integer     REFERENCES seasons (id),
    tournament   boolean     NOT NULL DEFAULT false,
    started_at   timestamptz NOT NULL,
    ended_at     timestamptz,
    duration_s   real        NOT NULL DEFAULT 0,
    winner       smallint,                   -- 0 aliados, 1 eje, -1 empate, NULL sin ganador
    end_reason   text        NOT NULL,       -- normal, forced, other, aborted
    team0_army   text,
    team1_army   text,
    team0_score  integer,
    team1_score  integer,
    team0_kills  integer,
    team1_kills  integer,
    humans       integer     NOT NULL DEFAULT 0,
    ranked       boolean     NOT NULL DEFAULT false,
    ingested_at  timestamptz NOT NULL DEFAULT now()
);
CREATE INDEX IF NOT EXISTS matches_started_idx ON matches (started_at);
CREATE INDEX IF NOT EXISTS matches_map_idx ON matches (map);

-- Una fila por jugador y partida (suma de todos sus tramos: reconexiones, etc.)
CREATE TABLE IF NOT EXISTS match_players (
    match_id     text    NOT NULL REFERENCES matches (id) ON DELETE CASCADE,
    player_id    integer NOT NULL REFERENCES players (id),
    team         smallint,                   -- equipo donde jugo mas tiempo
    time_team0   integer NOT NULL DEFAULT 0, -- segundos
    time_team1   integer NOT NULL DEFAULT 0,
    kills        integer NOT NULL DEFAULT 0, -- a enemigos (humanos o bots)
    deaths       integer NOT NULL DEFAULT 0, -- todas
    human_kills  integer NOT NULL DEFAULT 0, -- a enemigos humanos (ladder)
    human_deaths integer NOT NULL DEFAULT 0, -- por humanos, suicidio o entorno (ladder)
    suicides     integer NOT NULL DEFAULT 0,
    teamkills    integer NOT NULL DEFAULT 0,
    headshots    integer NOT NULL DEFAULT 0,
    objectives   integer NOT NULL DEFAULT 0,
    best_streak  integer NOT NULL DEFAULT 0, -- mejor racha de kills en la partida
    hits         integer NOT NULL DEFAULT 0,
    misses       integer NOT NULL DEFAULT 0,
    score        integer NOT NULL DEFAULT 0,
    points       integer NOT NULL DEFAULT 0,
    classes      jsonb   NOT NULL DEFAULT '{}'::jsonb,  -- segundos por clase
    main_class   text,
    result       char(1),                    -- W, L, D o NULL (sin ganador)
    PRIMARY KEY (match_id, player_id)
);
CREATE INDEX IF NOT EXISTS match_players_player_idx ON match_players (player_id);


CREATE TABLE IF NOT EXISTS kills (
    id            bigserial PRIMARY KEY,
    match_id      text     NOT NULL REFERENCES matches (id) ON DELETE CASCADE,
    t             real     NOT NULL,         -- segundos desde el inicio del mapa
    killer_id     integer  REFERENCES players (id),  -- NULL = suicidio / entorno
    victim_id     integer  NOT NULL REFERENCES players (id),
    killer_team   smallint,
    victim_team   smallint,
    killer_class  text,
    victim_class  text,
    mod           text     NOT NULL,         -- causa (rifle, sniper, grenade, ...)
    weapon        text,                      -- arma en mano (solo armas de fuego / cuchillo)
    headshot      boolean  NOT NULL DEFAULT false,
    friendly_fire boolean  NOT NULL DEFAULT false,
    suicide       boolean  NOT NULL DEFAULT false,
    distance      integer,
    killer_x integer, killer_y integer, killer_z integer,
    victim_x integer, victim_y integer, victim_z integer
);
CREATE INDEX IF NOT EXISTS kills_match_idx ON kills (match_id);
CREATE INDEX IF NOT EXISTS kills_killer_idx ON kills (killer_id);
CREATE INDEX IF NOT EXISTS kills_victim_idx ON kills (victim_id);

CREATE TABLE IF NOT EXISTS objectives (
    id         bigserial PRIMARY KEY,
    match_id   text     NOT NULL REFERENCES matches (id) ON DELETE CASCADE,
    t          real     NOT NULL,
    type       text     NOT NULL,            -- touch, area, timed, timed_held, explosive, bc_*
    name       text,
    team       smallint,
    player_id  integer  REFERENCES players (id)
);
CREATE INDEX IF NOT EXISTS objectives_match_idx ON objectives (match_id);
CREATE INDEX IF NOT EXISTS objectives_player_idx ON objectives (player_id);

------------------------------------------------------------------------------
-- Migraciones de bases creadas con versiones anteriores del esquema
-- (van despues de crear todas las tablas; en una base nueva no hacen nada)
------------------------------------------------------------------------------

ALTER TABLE matches ADD COLUMN IF NOT EXISTS kind text NOT NULL DEFAULT 'public';
ALTER TABLE matches ADD COLUMN IF NOT EXISTS event text NOT NULL DEFAULT '';
ALTER TABLE matches ADD COLUMN IF NOT EXISTS season_id integer REFERENCES seasons (id);
UPDATE matches m SET season_id = (SELECT s.id FROM seasons s WHERE s.starts_at <= m.started_at
                                  ORDER BY s.starts_at DESC LIMIT 1)
WHERE m.season_id IS NULL;
CREATE INDEX IF NOT EXISTS matches_season_idx ON matches (season_id);
ALTER TABLE match_players ADD COLUMN IF NOT EXISTS human_kills  integer NOT NULL DEFAULT 0;
ALTER TABLE match_players ADD COLUMN IF NOT EXISTS human_deaths integer NOT NULL DEFAULT 0;
ALTER TABLE match_players ADD COLUMN IF NOT EXISTS best_streak  integer NOT NULL DEFAULT 0;

-- rellena las columnas nuevas en filas cargadas antes de que existieran
UPDATE match_players mp SET
    human_kills = (SELECT count(*) FROM kills k JOIN players v ON v.id = k.victim_id
                   WHERE k.match_id = mp.match_id AND k.killer_id = mp.player_id
                     AND NOT k.friendly_fire AND NOT v.is_bot),
    human_deaths = (SELECT count(*) FROM kills k LEFT JOIN players kp ON kp.id = k.killer_id
                    WHERE k.match_id = mp.match_id AND k.victim_id = mp.player_id
                      AND (k.killer_id IS NULL OR NOT kp.is_bot))
WHERE mp.human_kills = 0 AND mp.human_deaths = 0 AND (mp.kills > 0 OR mp.deaths > 0);

-- Rating Elo por equipos (lo calcula ingest.py), separado por categoria
-- (public, duel, official) y por temporada. season_id = 0 es el historico de
-- todas las temporadas. Es un dato derivado: si la tabla es de una version
-- anterior se recrea y ingest.py --init-schema la recalcula.
DO $$
BEGIN
    IF EXISTS (SELECT 1 FROM information_schema.tables WHERE table_name = 'ratings')
       AND NOT EXISTS (SELECT 1 FROM information_schema.columns
                       WHERE table_name = 'ratings' AND column_name = 'season_id') THEN
        DROP TABLE rating_history CASCADE;
        DROP TABLE ratings CASCADE;  -- las vistas se recrean mas abajo
        RAISE NOTICE 'ratings recreada: ejecute ingest.py --rebuild-ratings';
    END IF;
END $$;

CREATE TABLE IF NOT EXISTS ratings (
    player_id   integer     NOT NULL REFERENCES players (id),
    kind        text        NOT NULL,       -- public, duel u official
    season_id   integer     NOT NULL,       -- 0 = historico
    rating      real        NOT NULL DEFAULT 1500,
    peak        real        NOT NULL DEFAULT 1500,
    games       integer     NOT NULL DEFAULT 0,
    wins        integer     NOT NULL DEFAULT 0,
    losses      integer     NOT NULL DEFAULT 0,
    draws       integer     NOT NULL DEFAULT 0,
    updated_at  timestamptz NOT NULL DEFAULT now(),
    PRIMARY KEY (player_id, kind, season_id)
);

CREATE TABLE IF NOT EXISTS rating_history (
    match_id       text    NOT NULL REFERENCES matches (id) ON DELETE CASCADE,
    player_id      integer NOT NULL REFERENCES players (id),
    season_id      integer NOT NULL,        -- 0 = historico
    rating_before  real    NOT NULL,
    rating_after   real    NOT NULL,
    PRIMARY KEY (match_id, player_id, season_id)
);

------------------------------------------------------------------------------
-- Tablas de resumen
--
-- Las mantiene ingest.py al cargar cada partida (apply_rollups) para que las
-- consultas del ladder lean tablas chicas en vez de recorrer millones de kills.
-- Si se borran o corrigen partidas a mano: SELECT rebuild_rollups();
------------------------------------------------------------------------------

-- version anterior sin temporadas: se recrea (rebuild_rollups la vuelve a llenar)
DO $$
BEGIN
    IF EXISTS (SELECT 1 FROM information_schema.tables WHERE table_name = 'player_stats')
       AND NOT EXISTS (SELECT 1 FROM information_schema.columns
                       WHERE table_name = 'player_stats' AND column_name = 'season_id') THEN
        DROP TABLE player_stats CASCADE;
    END IF;
END $$;

-- por jugador humano, temporada, categoria (public / duel / official) y torneo
CREATE TABLE IF NOT EXISTS player_stats (
    player_id     integer NOT NULL REFERENCES players (id),
    season_id     integer NOT NULL REFERENCES seasons (id),
    kind          text    NOT NULL,             -- public, duel u official
    event         text    NOT NULL DEFAULT '',  -- torneo; '' = casual
    matches       integer NOT NULL DEFAULT 0,
    wins          integer NOT NULL DEFAULT 0,
    losses        integer NOT NULL DEFAULT 0,
    draws         integer NOT NULL DEFAULT 0,
    kills         integer NOT NULL DEFAULT 0,  -- a enemigos (humanos o bots)
    deaths        integer NOT NULL DEFAULT 0,
    human_kills   integer NOT NULL DEFAULT 0,
    human_deaths  integer NOT NULL DEFAULT 0,
    headshots     integer NOT NULL DEFAULT 0,
    hits          integer NOT NULL DEFAULT 0,
    misses        integer NOT NULL DEFAULT 0,
    teamkills     integer NOT NULL DEFAULT 0,
    suicides      integer NOT NULL DEFAULT 0,
    objectives    integer NOT NULL DEFAULT 0,
    seconds       integer NOT NULL DEFAULT 0,
    best_streak   integer NOT NULL DEFAULT 0,
    max_kills     integer NOT NULL DEFAULT 0,  -- mas kills en una partida
    longest_kill  integer,
    last_match    timestamptz,
    PRIMARY KEY (player_id, season_id, kind, event)
);

CREATE TABLE IF NOT EXISTS player_map_stats (
    player_id     integer NOT NULL REFERENCES players (id),
    map           text    NOT NULL,
    kind          text    NOT NULL,
    matches       integer NOT NULL DEFAULT 0,
    wins          integer NOT NULL DEFAULT 0,
    kills         integer NOT NULL DEFAULT 0,
    deaths        integer NOT NULL DEFAULT 0,
    seconds       integer NOT NULL DEFAULT 0,
    best_streak   integer NOT NULL DEFAULT 0,
    PRIMARY KEY (player_id, map, kind)
);

CREATE TABLE IF NOT EXISTS player_class_stats (
    player_id     integer NOT NULL REFERENCES players (id),
    class         text    NOT NULL,
    seconds       integer NOT NULL DEFAULT 0,
    PRIMARY KEY (player_id, class)
);

CREATE TABLE IF NOT EXISTS player_weapon_stats (
    player_id     integer NOT NULL REFERENCES players (id),
    weapon        text    NOT NULL,             -- arma en mano o, si no aplica, la causa
    kills         integer NOT NULL DEFAULT 0,
    headshots     integer NOT NULL DEFAULT 0,
    distance_sum  bigint  NOT NULL DEFAULT 0,
    distance_n    integer NOT NULL DEFAULT 0,
    PRIMARY KEY (player_id, weapon)
);

-- kills entre humanos (nemesis / victima favorita)
CREATE TABLE IF NOT EXISTS player_duel_stats (
    killer_id     integer NOT NULL REFERENCES players (id),
    victim_id     integer NOT NULL REFERENCES players (id),
    kills         integer NOT NULL DEFAULT 0,
    PRIMARY KEY (killer_id, victim_id)
);
CREATE INDEX IF NOT EXISTS player_duel_stats_victim_idx ON player_duel_stats (victim_id);

CREATE TABLE IF NOT EXISTS map_stats (
    map           text    NOT NULL,
    kind          text    NOT NULL,
    matches       integer NOT NULL DEFAULT 0,
    allied_wins   integer NOT NULL DEFAULT 0,
    axis_wins     integer NOT NULL DEFAULT 0,
    draws         integer NOT NULL DEFAULT 0,
    seconds       bigint  NOT NULL DEFAULT 0,
    humans        integer NOT NULL DEFAULT 0,
    kills         integer NOT NULL DEFAULT 0,
    best_streak   integer NOT NULL DEFAULT 0,
    last_played   timestamptz,
    PRIMARY KEY (map, kind)
);

CREATE TABLE IF NOT EXISTS map_weapon_stats (
    map           text    NOT NULL,
    kind          text    NOT NULL,
    weapon        text    NOT NULL,
    kills         integer NOT NULL DEFAULT 0,
    headshots     integer NOT NULL DEFAULT 0,
    distance_sum  bigint  NOT NULL DEFAULT 0,
    distance_n    integer NOT NULL DEFAULT 0,
    max_distance  integer,
    PRIMARY KEY (map, kind, weapon)
);

-- actividad por dia (hora de Chile)
CREATE TABLE IF NOT EXISTS daily_stats (
    day           date    PRIMARY KEY,
    matches       integer NOT NULL DEFAULT 0,
    players       integer NOT NULL DEFAULT 0   -- jugadores humanos distintos
);

CREATE TABLE IF NOT EXISTS daily_players (
    day           date    NOT NULL,
    player_id     integer NOT NULL REFERENCES players (id),
    PRIMARY KEY (day, player_id)
);

-- para ladder_streak (top N sin recorrer toda la tabla)
CREATE INDEX IF NOT EXISTS match_players_streak_idx ON match_players (best_streak DESC);

-- Categoria de ladder de una partida: los torneos (event) son 'official',
-- el resto segun el modo del servidor ('public' o 'duel')
CREATE OR REPLACE FUNCTION match_category(kind text, event text) RETURNS text
LANGUAGE sql IMMUTABLE AS $$
    SELECT CASE WHEN event <> '' THEN 'official' ELSE kind END;
$$;

-- Temporada por nombre: 'current' = la vigente, 'all' = NULL (todas)
CREATE OR REPLACE FUNCTION season_lookup(season text) RETURNS integer
LANGUAGE sql STABLE AS $$
    SELECT CASE
        WHEN season = 'all' THEN NULL
        WHEN season = 'current' THEN (SELECT id FROM seasons WHERE starts_at <= now()
                                      ORDER BY starts_at DESC LIMIT 1)
        ELSE coalesce((SELECT id FROM seasons WHERE name = season), -1)
    END;
$$;

-- Suma las partidas indicadas a las tablas de resumen. Cada partida debe
-- sumarse una sola vez (ingest.py lo hace en la misma transaccion del insert).
CREATE OR REPLACE FUNCTION apply_rollups(ids text[]) RETURNS void
LANGUAGE plpgsql AS $$
BEGIN
    INSERT INTO player_stats AS s (player_id, season_id, kind, event, matches, wins, losses, draws,
        kills, deaths, human_kills, human_deaths, headshots, hits, misses, teamkills, suicides,
        objectives, seconds, best_streak, max_kills, longest_kill, last_match)
    SELECT mp.player_id, m.season_id, match_category(m.kind, m.event), m.event, count(*),
           count(*) FILTER (WHERE mp.result = 'W'), count(*) FILTER (WHERE mp.result = 'L'),
           count(*) FILTER (WHERE mp.result = 'D'),
           sum(mp.kills), sum(mp.deaths), sum(mp.human_kills), sum(mp.human_deaths),
           sum(mp.headshots), sum(mp.hits), sum(mp.misses), sum(mp.teamkills), sum(mp.suicides),
           sum(mp.objectives), sum(mp.time_team0 + mp.time_team1), max(mp.best_streak),
           max(mp.kills), max(lk.longest), max(m.started_at)
    FROM match_players mp
    JOIN matches m ON m.id = mp.match_id
    JOIN players p ON p.id = mp.player_id
    LEFT JOIN (SELECT k.match_id, k.killer_id, max(k.distance) AS longest
               FROM kills k
               WHERE k.match_id = ANY (ids) AND NOT k.friendly_fire AND k.killer_id IS NOT NULL
               GROUP BY k.match_id, k.killer_id) lk
           ON lk.match_id = mp.match_id AND lk.killer_id = mp.player_id
    WHERE mp.match_id = ANY (ids) AND NOT p.is_bot
    GROUP BY mp.player_id, m.season_id, match_category(m.kind, m.event), m.event
    ON CONFLICT (player_id, season_id, kind, event) DO UPDATE SET
        matches = s.matches + EXCLUDED.matches, wins = s.wins + EXCLUDED.wins,
        losses = s.losses + EXCLUDED.losses, draws = s.draws + EXCLUDED.draws,
        kills = s.kills + EXCLUDED.kills, deaths = s.deaths + EXCLUDED.deaths,
        human_kills = s.human_kills + EXCLUDED.human_kills,
        human_deaths = s.human_deaths + EXCLUDED.human_deaths,
        headshots = s.headshots + EXCLUDED.headshots, hits = s.hits + EXCLUDED.hits,
        misses = s.misses + EXCLUDED.misses, teamkills = s.teamkills + EXCLUDED.teamkills,
        suicides = s.suicides + EXCLUDED.suicides, objectives = s.objectives + EXCLUDED.objectives,
        seconds = s.seconds + EXCLUDED.seconds,
        best_streak = greatest(s.best_streak, EXCLUDED.best_streak),
        max_kills = greatest(s.max_kills, EXCLUDED.max_kills),
        longest_kill = greatest(s.longest_kill, EXCLUDED.longest_kill),
        last_match = greatest(s.last_match, EXCLUDED.last_match);

    INSERT INTO player_map_stats AS s (player_id, map, kind, matches, wins, kills, deaths, seconds, best_streak)
    SELECT mp.player_id, m.map, m.kind, count(*), count(*) FILTER (WHERE mp.result = 'W'),
           sum(mp.kills), sum(mp.deaths), sum(mp.time_team0 + mp.time_team1), max(mp.best_streak)
    FROM match_players mp
    JOIN matches m ON m.id = mp.match_id
    JOIN players p ON p.id = mp.player_id
    WHERE mp.match_id = ANY (ids) AND NOT p.is_bot
    GROUP BY mp.player_id, m.map, m.kind
    ON CONFLICT (player_id, map, kind) DO UPDATE SET
        matches = s.matches + EXCLUDED.matches, wins = s.wins + EXCLUDED.wins,
        kills = s.kills + EXCLUDED.kills, deaths = s.deaths + EXCLUDED.deaths,
        seconds = s.seconds + EXCLUDED.seconds,
        best_streak = greatest(s.best_streak, EXCLUDED.best_streak);

    INSERT INTO player_class_stats AS s (player_id, class, seconds)
    SELECT mp.player_id, c.key, sum(c.value::integer)
    FROM match_players mp
    JOIN players p ON p.id = mp.player_id
    CROSS JOIN LATERAL jsonb_each_text(mp.classes) c
    WHERE mp.match_id = ANY (ids) AND NOT p.is_bot
    GROUP BY mp.player_id, c.key
    ON CONFLICT (player_id, class) DO UPDATE SET seconds = s.seconds + EXCLUDED.seconds;

    INSERT INTO player_weapon_stats AS s (player_id, weapon, kills, headshots, distance_sum, distance_n)
    SELECT k.killer_id, coalesce(k.weapon, k.mod), count(*), count(*) FILTER (WHERE k.headshot),
           coalesce(sum(k.distance), 0), count(k.distance)
    FROM kills k
    JOIN players p ON p.id = k.killer_id
    WHERE k.match_id = ANY (ids) AND NOT k.friendly_fire AND NOT p.is_bot
    GROUP BY k.killer_id, coalesce(k.weapon, k.mod)
    ON CONFLICT (player_id, weapon) DO UPDATE SET
        kills = s.kills + EXCLUDED.kills, headshots = s.headshots + EXCLUDED.headshots,
        distance_sum = s.distance_sum + EXCLUDED.distance_sum,
        distance_n = s.distance_n + EXCLUDED.distance_n;

    INSERT INTO player_duel_stats AS s (killer_id, victim_id, kills)
    SELECT k.killer_id, k.victim_id, count(*)
    FROM kills k
    JOIN players kp ON kp.id = k.killer_id
    JOIN players vp ON vp.id = k.victim_id
    WHERE k.match_id = ANY (ids) AND NOT k.friendly_fire AND NOT k.suicide
      AND k.killer_id <> k.victim_id AND NOT kp.is_bot AND NOT vp.is_bot
    GROUP BY k.killer_id, k.victim_id
    ON CONFLICT (killer_id, victim_id) DO UPDATE SET kills = s.kills + EXCLUDED.kills;

    INSERT INTO map_stats AS s (map, kind, matches, allied_wins, axis_wins, draws, seconds, humans,
        kills, best_streak, last_played)
    SELECT m.map, m.kind, count(*), count(*) FILTER (WHERE m.winner = 0),
           count(*) FILTER (WHERE m.winner = 1), count(*) FILTER (WHERE m.winner = -1),
           sum(m.duration_s)::bigint, sum(m.humans),
           coalesce(sum(mk.kills), 0), coalesce(max(ms.best_streak), 0), max(m.started_at)
    FROM matches m
    LEFT JOIN (SELECT match_id, count(*) AS kills FROM kills
               WHERE match_id = ANY (ids) AND NOT suicide GROUP BY match_id) mk ON mk.match_id = m.id
    LEFT JOIN (SELECT mp.match_id, max(mp.best_streak) AS best_streak
               FROM match_players mp JOIN players p ON p.id = mp.player_id
               WHERE mp.match_id = ANY (ids) AND NOT p.is_bot GROUP BY mp.match_id) ms ON ms.match_id = m.id
    WHERE m.id = ANY (ids)
    GROUP BY m.map, m.kind
    ON CONFLICT (map, kind) DO UPDATE SET
        matches = s.matches + EXCLUDED.matches, allied_wins = s.allied_wins + EXCLUDED.allied_wins,
        axis_wins = s.axis_wins + EXCLUDED.axis_wins, draws = s.draws + EXCLUDED.draws,
        seconds = s.seconds + EXCLUDED.seconds, humans = s.humans + EXCLUDED.humans,
        kills = s.kills + EXCLUDED.kills,
        best_streak = greatest(s.best_streak, EXCLUDED.best_streak),
        last_played = greatest(s.last_played, EXCLUDED.last_played);

    INSERT INTO map_weapon_stats AS s (map, kind, weapon, kills, headshots, distance_sum, distance_n, max_distance)
    SELECT m.map, m.kind, coalesce(k.weapon, k.mod), count(*), count(*) FILTER (WHERE k.headshot),
           coalesce(sum(k.distance), 0), count(k.distance), max(k.distance)
    FROM kills k
    JOIN matches m ON m.id = k.match_id
    WHERE k.match_id = ANY (ids) AND NOT k.suicide AND NOT k.friendly_fire
    GROUP BY m.map, m.kind, coalesce(k.weapon, k.mod)
    ON CONFLICT (map, kind, weapon) DO UPDATE SET
        kills = s.kills + EXCLUDED.kills, headshots = s.headshots + EXCLUDED.headshots,
        distance_sum = s.distance_sum + EXCLUDED.distance_sum,
        distance_n = s.distance_n + EXCLUDED.distance_n,
        max_distance = greatest(s.max_distance, EXCLUDED.max_distance);

    INSERT INTO daily_stats AS s (day, matches)
    SELECT (m.started_at AT TIME ZONE 'America/Santiago')::date, count(*)
    FROM matches m
    WHERE m.id = ANY (ids)
    GROUP BY 1
    ON CONFLICT (day) DO UPDATE SET matches = s.matches + EXCLUDED.matches;

    -- solo los jugadores nuevos de cada dia suman al conteo
    WITH new_players AS (
        INSERT INTO daily_players (day, player_id)
        SELECT DISTINCT (m.started_at AT TIME ZONE 'America/Santiago')::date, mp.player_id
        FROM match_players mp
        JOIN matches m ON m.id = mp.match_id
        JOIN players p ON p.id = mp.player_id
        WHERE mp.match_id = ANY (ids) AND NOT p.is_bot
        ON CONFLICT DO NOTHING
        RETURNING day
    )
    UPDATE daily_stats d SET players = d.players + c.n
    FROM (SELECT day, count(*) AS n FROM new_players GROUP BY day) c
    WHERE d.day = c.day;
END;
$$;

-- Recalcula todas las tablas de resumen desde las tablas de detalle
CREATE OR REPLACE FUNCTION rebuild_rollups() RETURNS void
LANGUAGE plpgsql AS $$
BEGIN
    TRUNCATE player_stats, player_map_stats, player_class_stats, player_weapon_stats,
             player_duel_stats, map_stats, map_weapon_stats, daily_stats, daily_players;
    PERFORM apply_rollups(ARRAY(SELECT id FROM matches));
END;
$$;

-- base con datos de partidas pero resumenes vacios (version anterior del esquema)
DO $$
BEGIN
    IF EXISTS (SELECT 1 FROM matches)
       AND (NOT EXISTS (SELECT 1 FROM player_stats) OR NOT EXISTS (SELECT 1 FROM map_stats)
            OR NOT EXISTS (SELECT 1 FROM daily_stats)) THEN
        PERFORM rebuild_rollups();
    END IF;
END $$;

------------------------------------------------------------------------------
-- Vistas (se recrean siempre; no guardan datos)
------------------------------------------------------------------------------

DROP VIEW IF EXISTS v_player_profile, v_player_totals, v_player_totals_by_kind,
    player_totals_base, v_player_maps, v_player_fav_map, v_player_weapons,
    v_player_fav_weapon, v_player_classes, v_player_fav_class, v_player_duels,
    v_player_nemesis, v_player_favorite_victim, v_player_records, v_map_stats,
    v_map_weapons, v_weapon_stats, v_daily_activity, v_seasons, v_events CASCADE;
DROP FUNCTION IF EXISTS ladder_kd(integer);
DROP FUNCTION IF EXISTS ladder_elo(integer);
DROP FUNCTION IF EXISTS ladder_kd(integer, text);
DROP FUNCTION IF EXISTS ladder_elo(integer, text);
DROP FUNCTION IF EXISTS ladder_streak(text, integer);
DROP FUNCTION IF EXISTS ladder_kd(integer, text, text, text);
DROP FUNCTION IF EXISTS ladder_elo(integer, text, text);
DROP FUNCTION IF EXISTS ladder_streak(text, integer, text);
DROP FUNCTION IF EXISTS player_totals(text, text, text) CASCADE;

-- Totales por jugador para una temporada, categoria y torneo:
--   season: 'current' (la vigente), 'all' (historico) o el nombre de una temporada
--   kind:   'public', 'duel', 'official' o 'all'
--   event:  nombre de un torneo, o NULL para no filtrar
-- kills / deaths / kd / kills_per_min cuentan solo enfrentamientos con humanos;
-- las kills a bots se muestran aparte en bot_kills.
CREATE FUNCTION player_totals(season text DEFAULT 'all', match_kind text DEFAULT 'all',
                              match_event text DEFAULT NULL)
RETURNS TABLE (player_id integer, name text, matches bigint, wins bigint, losses bigint,
               draws bigint, kills bigint, deaths bigint, kd numeric, bot_kills bigint,
               headshots bigint, hs_pct numeric, accuracy_pct numeric, teamkills bigint,
               suicides bigint, objectives bigint, best_streak integer, hours_played numeric,
               kills_per_min numeric, win_pct numeric, most_kills_match integer,
               longest_kill integer, last_match timestamptz)
LANGUAGE sql STABLE AS $$
    WITH sid AS (SELECT season_lookup(season) AS id),
    s AS (
        SELECT ps.player_id, sum(ps.matches) AS matches, sum(ps.wins) AS wins,
               sum(ps.losses) AS losses, sum(ps.draws) AS draws, sum(ps.kills) AS kills,
               sum(ps.human_kills) AS hk, sum(ps.human_deaths) AS hd, sum(ps.headshots) AS hs,
               sum(ps.hits) AS hits, sum(ps.misses) AS misses, sum(ps.teamkills) AS tk,
               sum(ps.suicides) AS suicides, sum(ps.objectives) AS objectives,
               sum(ps.seconds) AS seconds, max(ps.best_streak) AS best_streak,
               max(ps.max_kills) AS max_kills, max(ps.longest_kill) AS longest_kill,
               max(ps.last_match) AS last_match
        FROM player_stats ps, sid
        WHERE (season = 'all' OR ps.season_id = sid.id)
          AND (match_kind = 'all' OR ps.kind = match_kind)
          AND (match_event IS NULL OR ps.event = match_event)
        GROUP BY ps.player_id
    )
    SELECT s.player_id, p.name, s.matches, s.wins, s.losses, s.draws,
           s.hk, s.hd, round(s.hk::numeric / greatest(s.hd, 1), 2), s.kills - s.hk,
           s.hs, round(100.0 * s.hs / greatest(s.kills, 1), 1),
           round(100.0 * s.hits / greatest(s.hits + s.misses, 1), 1),
           s.tk, s.suicides, s.objectives, s.best_streak,
           round(s.seconds / 3600.0, 1), round(s.hk * 60.0 / greatest(s.seconds, 1), 2),
           round(100.0 * s.wins / greatest(s.wins + s.losses + s.draws, 1), 1),
           s.max_kills, s.longest_kill, s.last_match
    FROM s
    JOIN players p ON p.id = s.player_id;
$$;

-- Totales historicos por jugador: una fila por jugador (todas las partidas)
CREATE VIEW v_player_totals AS
SELECT * FROM player_totals('all', 'all');

-- Totales historicos por jugador y categoria (public, duel, official)
CREATE VIEW v_player_totals_by_kind AS
SELECT k.kind, t.*
FROM (VALUES ('public'), ('duel'), ('official')) k(kind),
     LATERAL player_totals('all', k.kind) t;

-- Ladder por K/D (por defecto: temporada actual, todas las categorias)
CREATE FUNCTION ladder_kd(min_matches integer DEFAULT 10, match_kind text DEFAULT 'all',
                          season text DEFAULT 'current', match_event text DEFAULT NULL)
RETURNS TABLE (pos bigint, name text, matches bigint, kills bigint, deaths bigint,
               kd numeric, kills_per_min numeric, win_pct numeric, hs_pct numeric)
LANGUAGE sql STABLE AS $$
    SELECT rank() OVER (ORDER BY t.kd DESC, t.kills DESC),
           t.name, t.matches, t.kills, t.deaths, t.kd, t.kills_per_min, t.win_pct, t.hs_pct
    FROM player_totals(season, match_kind, match_event) t
    WHERE t.matches >= min_matches
    ORDER BY 1, t.name;
$$;

-- Ladder por rating Elo. kind: 'public', 'duel' u 'official' (cada uno con su
-- rating). season: 'current', el nombre de una temporada o 'all' (historico).
CREATE FUNCTION ladder_elo(min_games integer DEFAULT 10, match_kind text DEFAULT 'public',
                           season text DEFAULT 'current')
RETURNS TABLE (pos bigint, name text, rating integer, peak integer,
               games integer, wins integer, losses integer, draws integer, win_pct numeric)
LANGUAGE sql STABLE AS $$
    SELECT rank() OVER (ORDER BY r.rating DESC),
           p.name, round(r.rating)::integer, round(r.peak)::integer,
           r.games, r.wins, r.losses, r.draws,
           round(100.0 * r.wins / greatest(r.games, 1), 1)
    FROM ratings r
    JOIN players p ON p.id = r.player_id
    WHERE r.games >= min_games AND r.kind = match_kind AND NOT p.is_bot
      AND r.season_id = coalesce(season_lookup(season), 0)
    ORDER BY 1, p.name;
$$;

-- Mejores rachas de kills (una fila por partida)
CREATE FUNCTION ladder_streak(match_kind text DEFAULT 'all', max_rows integer DEFAULT 20,
                              season text DEFAULT 'current')
RETURNS TABLE (pos bigint, name text, best_streak integer, map text, kind text,
               event text, played_at timestamptz)
LANGUAGE sql STABLE AS $$
    SELECT rank() OVER (ORDER BY t.best_streak DESC), t.name, t.best_streak, t.map, t.kind,
           t.event, t.started_at
    FROM (SELECT p.name, mp.best_streak, m.map, match_category(m.kind, m.event) AS kind,
                 m.event, m.started_at
          FROM match_players mp
          JOIN players p ON p.id = mp.player_id
          JOIN matches m ON m.id = mp.match_id
          WHERE NOT p.is_bot AND mp.best_streak > 0
            AND (match_kind = 'all' OR match_category(m.kind, m.event) = match_kind)
            AND (season = 'all' OR m.season_id = (SELECT season_lookup(season)))
          ORDER BY mp.best_streak DESC, m.started_at
          LIMIT max_rows) t
    ORDER BY t.best_streak DESC, t.started_at;
$$;

-- Temporadas y torneos
CREATE VIEW v_seasons AS
SELECT s.id, s.name, s.starts_at,
       lead(s.starts_at) OVER (ORDER BY s.starts_at) AS ends_at,
       (SELECT count(*) FROM matches m WHERE m.season_id = s.id) AS matches
FROM seasons s;

CREATE VIEW v_events AS
SELECT m.event, count(*) AS matches, min(m.started_at) AS first_match,
       max(m.started_at) AS last_match, array_agg(DISTINCT m.map) AS maps
FROM matches m
WHERE m.event <> ''
GROUP BY m.event;

-- Estadisticas por jugador y mapa (todas las partidas)
CREATE VIEW v_player_maps AS
SELECT player_id,
       map,
       sum(matches)                                   AS matches,
       sum(wins)                                      AS wins,
       sum(kills)                                     AS kills,
       sum(deaths)                                    AS deaths,
       round(sum(kills)::numeric / greatest(sum(deaths), 1), 2) AS kd,
       sum(seconds)                                   AS seconds_played,
       max(best_streak)                               AS best_streak
FROM player_map_stats
GROUP BY player_id, map;

-- Mapa mas jugado por jugador
CREATE VIEW v_player_fav_map AS
SELECT DISTINCT ON (player_id) player_id, map, matches, seconds_played
FROM v_player_maps
ORDER BY player_id, matches DESC, seconds_played DESC, map;

-- Armas por jugador (arma en mano o, si no aplica, la causa)
CREATE VIEW v_player_weapons AS
SELECT player_id, weapon, kills, headshots,
       round(distance_sum::numeric / nullif(distance_n, 0)) AS avg_distance
FROM player_weapon_stats;

CREATE VIEW v_player_fav_weapon AS
SELECT DISTINCT ON (player_id) player_id, weapon, kills
FROM player_weapon_stats
ORDER BY player_id, kills DESC, weapon;

-- Clases por jugador (segundos jugados)
CREATE VIEW v_player_classes AS
SELECT player_id, class, seconds FROM player_class_stats;

CREATE VIEW v_player_fav_class AS
SELECT DISTINCT ON (player_id) player_id, class, seconds
FROM player_class_stats
ORDER BY player_id, seconds DESC, class;

-- Quien mata mas a cada jugador (nemesis) y a quien mata mas (victima favorita)
CREATE VIEW v_player_duels AS
SELECT killer_id, victim_id, kills FROM player_duel_stats;

CREATE VIEW v_player_nemesis AS
SELECT DISTINCT ON (d.victim_id) d.victim_id AS player_id, p.name AS nemesis, d.kills
FROM player_duel_stats d
JOIN players p ON p.id = d.killer_id
ORDER BY d.victim_id, d.kills DESC, p.name;

CREATE VIEW v_player_favorite_victim AS
SELECT DISTINCT ON (d.killer_id) d.killer_id AS player_id, p.name AS victim, d.kills
FROM player_duel_stats d
JOIN players p ON p.id = d.victim_id
ORDER BY d.killer_id, d.kills DESC, p.name;

-- Perfil completo (una fila por jugador)
CREATE VIEW v_player_profile AS
SELECT t.*,
       round(r.rating)::integer  AS rating,          -- Elo de la temporada actual
       r.games                   AS ranked_games,
       round(rd.rating)::integer AS duel_rating,
       rd.games                  AS duel_games,
       round(ro.rating)::integer AS official_rating,
       ro.games                  AS official_games,
       fm.map                    AS fav_map,
       fw.weapon                 AS fav_weapon,
       fc.class                  AS fav_class,
       ne.nemesis,
       fv.victim                 AS favorite_victim
FROM v_player_totals t
LEFT JOIN ratings r                   ON r.player_id = t.player_id AND r.kind = 'public'
                                     AND r.season_id = season_lookup('current')
LEFT JOIN ratings rd                  ON rd.player_id = t.player_id AND rd.kind = 'duel'
                                     AND rd.season_id = season_lookup('current')
LEFT JOIN ratings ro                  ON ro.player_id = t.player_id AND ro.kind = 'official'
                                     AND ro.season_id = season_lookup('current')
LEFT JOIN v_player_fav_map fm         ON fm.player_id = t.player_id
LEFT JOIN v_player_fav_weapon fw      ON fw.player_id = t.player_id
LEFT JOIN v_player_fav_class fc       ON fc.player_id = t.player_id
LEFT JOIN v_player_nemesis ne         ON ne.player_id = t.player_id
LEFT JOIN v_player_favorite_victim fv ON fv.player_id = t.player_id;

-- Estadisticas por mapa y tipo (balance aliados / eje, duracion, letalidad)
CREATE VIEW v_map_stats AS
SELECT map,
       kind,
       matches,
       allied_wins,
       axis_wins,
       draws,
       round(100.0 * allied_wins / greatest(allied_wins + axis_wins, 1), 1) AS allied_win_pct,
       round(seconds / 60.0 / greatest(matches, 1), 1)  AS avg_minutes,
       round(humans::numeric / greatest(matches, 1), 1) AS avg_humans,
       best_streak,
       kills,
       last_played
FROM map_stats;

-- Armas mas letales por mapa (todas las partidas)
CREATE VIEW v_map_weapons AS
SELECT map, weapon, sum(kills) AS kills
FROM map_weapon_stats
GROUP BY map, weapon;

-- Armas globales
CREATE VIEW v_weapon_stats AS
SELECT weapon,
       sum(kills)                                            AS kills,
       sum(headshots)                                        AS headshots,
       round(sum(distance_sum)::numeric / nullif(sum(distance_n), 0)) AS avg_distance,
       max(max_distance)                                     AS max_distance
FROM map_weapon_stats
GROUP BY weapon;

-- Actividad diaria (dias en hora de Chile)
CREATE VIEW v_daily_activity AS
SELECT day, matches, players FROM daily_stats;

COMMIT;
