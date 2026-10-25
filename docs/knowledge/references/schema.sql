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
    mode         text        NOT NULL,       -- dm, ctb, campaign, control, ffa
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
    helmet_saves integer NOT NULL DEFAULT 0, -- el casco le desvio un tiro a la cabeza
    foot_saves   integer NOT NULL DEFAULT 0, -- sobrevivio con "almost lost a foot"
    deflected    integer NOT NULL DEFAULT 0, -- tiros suyos desviados por un casco
    hits         integer NOT NULL DEFAULT 0,
    misses       integer NOT NULL DEFAULT 0,
    score        integer NOT NULL DEFAULT 0,
    points       integer NOT NULL DEFAULT 0,
    classes      jsonb   NOT NULL DEFAULT '{}'::jsonb,  -- segundos por clase
    main_class   text,
    result       char(1),                    -- W, L, D o NULL (sin ganador)
    -- rachas de la partida: veces que llego a cada anuncio de KillingSpree()
    -- (umbrales en streak_base()) y rachas ajenas que corto. Las calcula
    -- apply_rollups a partir de la tabla kills.
    sprees       integer NOT NULL DEFAULT 0, -- KILLING SPREE
    rampages     integer NOT NULL DEFAULT 0, -- RAMPAGE
    dominatings  integer NOT NULL DEFAULT 0, -- DOMINATING
    unstoppables integer NOT NULL DEFAULT 0, -- UNSTOPPABLE
    godlikes     integer NOT NULL DEFAULT 0, -- GODLIKE
    streaks_ended integer NOT NULL DEFAULT 0, -- "has ENDED <x> streak"
    zone_seconds integer NOT NULL DEFAULT 0, -- modo control: segundos dentro de la zona
    zone_kills   integer NOT NULL DEFAULT 0, -- modo control: kills con el que mata o el que muere en la zona
    place        smallint,                   -- FFA: puesto al terminar (NULL en otros modos o si se fue antes)
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
    victim_x integer, victim_y integer, victim_z integer,
    killer_in_zone boolean,                  -- modo control: el que mato estaba en la zona (NULL fuera del modo)
    victim_in_zone boolean                   -- modo control: el que murio estaba en la zona
);
CREATE INDEX IF NOT EXISTS kills_match_idx ON kills (match_id);
CREATE INDEX IF NOT EXISTS kills_killer_idx ON kills (killer_id);
CREATE INDEX IF NOT EXISTS kills_victim_idx ON kills (victim_id);

CREATE TABLE IF NOT EXISTS objectives (
    id         bigserial PRIMARY KEY,
    match_id   text     NOT NULL REFERENCES matches (id) ON DELETE CASCADE,
    t          real     NOT NULL,
    type       text     NOT NULL,            -- touch, area, timed, timed_held, explosive, bc_*, zone_capture
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
ALTER TABLE match_players ADD COLUMN IF NOT EXISTS helmet_saves integer NOT NULL DEFAULT 0;
ALTER TABLE match_players ADD COLUMN IF NOT EXISTS foot_saves   integer NOT NULL DEFAULT 0;
ALTER TABLE match_players ADD COLUMN IF NOT EXISTS deflected    integer NOT NULL DEFAULT 0;

-- conteo de rachas por nivel: si la base no lo tenia, se agregan las columnas y
-- al final se recalculan los resumenes (rebuild_rollups) para rellenar el historial
DO $$
BEGIN
    IF NOT EXISTS (SELECT 1 FROM information_schema.columns
                   WHERE table_name = 'match_players' AND column_name = 'godlikes') THEN
        CREATE TEMP TABLE streak_backfill_needed () ON COMMIT DROP;
    END IF;
END $$;
ALTER TABLE match_players ADD COLUMN IF NOT EXISTS sprees        integer NOT NULL DEFAULT 0;
ALTER TABLE match_players ADD COLUMN IF NOT EXISTS rampages      integer NOT NULL DEFAULT 0;
ALTER TABLE match_players ADD COLUMN IF NOT EXISTS dominatings   integer NOT NULL DEFAULT 0;
ALTER TABLE match_players ADD COLUMN IF NOT EXISTS unstoppables  integer NOT NULL DEFAULT 0;
ALTER TABLE match_players ADD COLUMN IF NOT EXISTS godlikes      integer NOT NULL DEFAULT 0;
ALTER TABLE match_players ADD COLUMN IF NOT EXISTS streaks_ended integer NOT NULL DEFAULT 0;
ALTER TABLE match_players ADD COLUMN IF NOT EXISTS zone_seconds  integer NOT NULL DEFAULT 0;
ALTER TABLE match_players ADD COLUMN IF NOT EXISTS zone_kills    integer NOT NULL DEFAULT 0;
ALTER TABLE match_players ADD COLUMN IF NOT EXISTS place         smallint;
ALTER TABLE kills ADD COLUMN IF NOT EXISTS killer_in_zone boolean;
ALTER TABLE kills ADD COLUMN IF NOT EXISTS victim_in_zone boolean;

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
-- (public, duel, official) y por temporada; un solo Elo para todos los modos de
-- juego. season_id = 0 es el historico de todas las temporadas. Es un dato
-- derivado: si la tabla es de una version anterior (sin temporadas, o con la
-- columna mode de una prueba de Elo por modo) se recrea y ingest.py
-- --init-schema la recalcula.
DO $$
BEGIN
    IF EXISTS (SELECT 1 FROM information_schema.tables WHERE table_name = 'ratings')
       AND (NOT EXISTS (SELECT 1 FROM information_schema.columns
                        WHERE table_name = 'ratings' AND column_name = 'season_id')
            OR EXISTS (SELECT 1 FROM information_schema.columns
                       WHERE table_name = 'ratings' AND column_name = 'mode')) THEN
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

-- version anterior sin temporadas, modo o stats de zona: se recrea (rebuild_rollups la
-- vuelve a llenar)
DO $$
BEGIN
    IF EXISTS (SELECT 1 FROM information_schema.tables WHERE table_name = 'player_stats')
       AND NOT EXISTS (SELECT 1 FROM information_schema.columns
                       WHERE table_name = 'player_stats' AND column_name = 'season_id') THEN
        DROP TABLE player_stats CASCADE;
    END IF;
    IF EXISTS (SELECT 1 FROM information_schema.tables WHERE table_name = 'player_stats')
       AND NOT EXISTS (SELECT 1 FROM information_schema.columns
                       WHERE table_name = 'player_stats' AND column_name = 'mode') THEN
        DROP TABLE player_stats CASCADE;
    END IF;
    IF EXISTS (SELECT 1 FROM information_schema.tables WHERE table_name = 'player_stats')
       AND NOT EXISTS (SELECT 1 FROM information_schema.columns
                       WHERE table_name = 'player_stats' AND column_name = 'zone_captures') THEN
        DROP TABLE player_stats CASCADE;  -- las capturas por jugador salen de objectives
    END IF;
    IF EXISTS (SELECT 1 FROM information_schema.tables WHERE table_name = 'map_stats')
       AND NOT EXISTS (SELECT 1 FROM information_schema.columns
                       WHERE table_name = 'map_stats' AND column_name = 'mode') THEN
        DROP TABLE map_stats CASCADE;
    END IF;
END $$;

-- por jugador humano, temporada, categoria (public / duel / official), modo
-- de juego y torneo
CREATE TABLE IF NOT EXISTS player_stats (
    player_id     integer NOT NULL REFERENCES players (id),
    season_id     integer NOT NULL REFERENCES seasons (id),
    kind          text    NOT NULL,             -- public, duel u official
    mode          text    NOT NULL,             -- dm, ctb, campaign, control...
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
    helmet_saves  integer NOT NULL DEFAULT 0,
    foot_saves    integer NOT NULL DEFAULT 0,
    deflected     integer NOT NULL DEFAULT 0,
    sprees        integer NOT NULL DEFAULT 0,
    rampages      integer NOT NULL DEFAULT 0,
    dominatings   integer NOT NULL DEFAULT 0,
    unstoppables  integer NOT NULL DEFAULT 0,
    godlikes      integer NOT NULL DEFAULT 0,
    streaks_ended integer NOT NULL DEFAULT 0,
    longest_kill  integer,
    last_match    timestamptz,
    zone_seconds  integer NOT NULL DEFAULT 0,  -- modo control: segundos dentro de la zona
    zone_captures integer NOT NULL DEFAULT 0,  -- modo control: capturas en las que participo
    zone_kills    integer NOT NULL DEFAULT 0,  -- modo control: kills en la zona (defender o limpiar)
    ffa_podiums   integer NOT NULL DEFAULT 0,  -- FFA: veces en el top 3
    ffa_placed    integer NOT NULL DEFAULT 0,  -- FFA: partidas terminadas con puesto
    ffa_place_sum integer NOT NULL DEFAULT 0,  -- FFA: suma de puestos (puesto promedio = suma / ffa_placed)
    PRIMARY KEY (player_id, season_id, kind, mode, event)
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
    mode          text    NOT NULL,
    matches       integer NOT NULL DEFAULT 0,
    allied_wins   integer NOT NULL DEFAULT 0,
    axis_wins     integer NOT NULL DEFAULT 0,
    draws         integer NOT NULL DEFAULT 0,
    seconds       bigint  NOT NULL DEFAULT 0,
    humans        integer NOT NULL DEFAULT 0,
    kills         integer NOT NULL DEFAULT 0,
    best_streak   integer NOT NULL DEFAULT 0,
    last_played   timestamptz,
    PRIMARY KEY (map, kind, mode)
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

-- columnas de FFA y kills en zona: si player_stats no las tenia, al final se
-- recalculan los resumenes (rebuild_rollups) para que cuadren con el historial
DO $$
BEGIN
    IF EXISTS (SELECT 1 FROM information_schema.tables WHERE table_name = 'player_stats')
       AND NOT EXISTS (SELECT 1 FROM information_schema.columns
                       WHERE table_name = 'player_stats' AND column_name = 'ffa_placed')
       AND to_regclass('pg_temp.streak_backfill_needed') IS NULL THEN
        CREATE TEMP TABLE streak_backfill_needed () ON COMMIT DROP;
    END IF;
END $$;

ALTER TABLE player_stats ADD COLUMN IF NOT EXISTS helmet_saves integer NOT NULL DEFAULT 0;
ALTER TABLE player_stats ADD COLUMN IF NOT EXISTS foot_saves   integer NOT NULL DEFAULT 0;
ALTER TABLE player_stats ADD COLUMN IF NOT EXISTS deflected    integer NOT NULL DEFAULT 0;
ALTER TABLE player_stats ADD COLUMN IF NOT EXISTS sprees        integer NOT NULL DEFAULT 0;
ALTER TABLE player_stats ADD COLUMN IF NOT EXISTS rampages      integer NOT NULL DEFAULT 0;
ALTER TABLE player_stats ADD COLUMN IF NOT EXISTS dominatings   integer NOT NULL DEFAULT 0;
ALTER TABLE player_stats ADD COLUMN IF NOT EXISTS unstoppables  integer NOT NULL DEFAULT 0;
ALTER TABLE player_stats ADD COLUMN IF NOT EXISTS godlikes      integer NOT NULL DEFAULT 0;
ALTER TABLE player_stats ADD COLUMN IF NOT EXISTS streaks_ended integer NOT NULL DEFAULT 0;
ALTER TABLE player_stats ADD COLUMN IF NOT EXISTS zone_seconds  integer NOT NULL DEFAULT 0;
ALTER TABLE player_stats ADD COLUMN IF NOT EXISTS zone_captures integer NOT NULL DEFAULT 0;
ALTER TABLE player_stats ADD COLUMN IF NOT EXISTS zone_kills    integer NOT NULL DEFAULT 0;
ALTER TABLE player_stats ADD COLUMN IF NOT EXISTS ffa_podiums   integer NOT NULL DEFAULT 0;
ALTER TABLE player_stats ADD COLUMN IF NOT EXISTS ffa_placed    integer NOT NULL DEFAULT 0;
ALTER TABLE player_stats ADD COLUMN IF NOT EXISTS ffa_place_sum integer NOT NULL DEFAULT 0;

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

-- Clanes del Ladder Test. Se editan desde el panel de administrador de la app
-- (funciones clan_save / clan_delete); la carga inicial es stats/clans_seed.sql.
CREATE TABLE IF NOT EXISTS clans (
    id               serial PRIMARY KEY,
    name             text NOT NULL DEFAULT '',
    leader           text NOT NULL DEFAULT '',
    declared_members integer CHECK (declared_members IS NULL OR declared_members BETWEEN 0 AND 999),
    favorite_map     text NOT NULL DEFAULT '',
    symbol_player    text NOT NULL DEFAULT '',
    phrase           text NOT NULL DEFAULT '',
    rival            text NOT NULL DEFAULT '',
    logo_url         text NOT NULL DEFAULT '',
    unregistered     boolean NOT NULL DEFAULT false,
    active           boolean NOT NULL DEFAULT true,
    sort_order       integer NOT NULL DEFAULT 0,
    created_at       timestamptz NOT NULL DEFAULT now(),
    updated_at       timestamptz NOT NULL DEFAULT now(),
    updated_by       text
);

-- Todos los tags de cada clan: el principal (is_primary) y los alternativos.
CREATE TABLE IF NOT EXISTS clan_tags (
    clan_id    integer NOT NULL REFERENCES clans (id) ON DELETE CASCADE,
    tag        text NOT NULL CHECK (length(tag) BETWEEN 1 AND 24 AND tag !~ '\s'),
    is_primary boolean NOT NULL DEFAULT false
);
CREATE UNIQUE INDEX IF NOT EXISTS clan_tags_key_idx ON clan_tags (lower(tag));
CREATE UNIQUE INDEX IF NOT EXISTS clan_tags_primary_idx ON clan_tags (clan_id) WHERE is_primary;
CREATE INDEX IF NOT EXISTS clan_tags_clan_idx ON clan_tags (clan_id);

-- Fichas de torneo del Ladder Test. La clave es el valor del cvar stats_event
-- (matches.event). Se editan desde el panel admin (tournament_save / tournament_delete).
-- La modalidad y los puntos van en cada fase (tournament_phases).
CREATE TABLE IF NOT EXISTS tournaments (
    id           serial PRIMARY KEY,
    event        text NOT NULL CHECK (length(event) BETWEEN 1 AND 64),
    name         text NOT NULL DEFAULT '',
    status       text NOT NULL DEFAULT 'upcoming'
                 CHECK (status IN ('upcoming', 'live', 'finished')),
    starts_on    date,
    ends_on      date,
    format       text NOT NULL DEFAULT '',
    description  text NOT NULL DEFAULT '',
    rules_url    text NOT NULL DEFAULT '',
    logo_url     text NOT NULL DEFAULT '',
    champion_id  integer REFERENCES clans (id) ON DELETE SET NULL,
    runner_up_id integer REFERENCES clans (id) ON DELETE SET NULL,
    visible      boolean NOT NULL DEFAULT true,
    created_at   timestamptz NOT NULL DEFAULT now(),
    updated_at   timestamptz NOT NULL DEFAULT now(),
    updated_by   text,
    CHECK (ends_on IS NULL OR starts_on IS NULL OR ends_on >= starts_on)
);
CREATE UNIQUE INDEX IF NOT EXISTS tournaments_event_idx ON tournaments (event);

-- Fases de un torneo (D22), cada una con su modalidad. Una partida cae en la fase con el
-- starts_at mas reciente que no sea posterior a la partida; la primera fase (menor
-- sort_order) siempre tiene starts_at NULL = desde el inicio.
CREATE TABLE IF NOT EXISTS tournament_phases (
    id            serial PRIMARY KEY,
    tournament_id integer NOT NULL REFERENCES tournaments (id) ON DELETE CASCADE,
    name          text NOT NULL CHECK (length(name) BETWEEN 1 AND 40),
    scoring       text NOT NULL DEFAULT 'league'
                  CHECK (scoring IN ('league', 'knockout', 'results')),
    points_per    text NOT NULL DEFAULT 'match' CHECK (points_per IN ('match', 'series')),
    points_win    integer NOT NULL DEFAULT 3 CHECK (points_win BETWEEN 0 AND 100),
    points_draw   integer NOT NULL DEFAULT 1 CHECK (points_draw BETWEEN 0 AND 100),
    points_loss   integer NOT NULL DEFAULT 0 CHECK (points_loss BETWEEN 0 AND 100),
    starts_at     timestamptz,
    sort_order    integer NOT NULL DEFAULT 0
);
CREATE INDEX IF NOT EXISTS tournament_phases_tournament_idx ON tournament_phases (tournament_id);

-- Clanes inscritos en un torneo, con su grupo ('' = sin grupo). Los grupos y los
-- inscritos en cero solo se usan en la PRIMERA fase.
CREATE TABLE IF NOT EXISTS tournament_teams (
    tournament_id integer NOT NULL REFERENCES tournaments (id) ON DELETE CASCADE,
    clan_id       integer NOT NULL REFERENCES clans (id) ON DELETE CASCADE,
    group_name    text NOT NULL DEFAULT '' CHECK (length(group_name) <= 24),
    PRIMARY KEY (tournament_id, clan_id)
);

-- Datos manuales de una partida de torneo: clan de cada lado (NULL = automatico),
-- "no cuenta", fase (NULL = automatica por fecha) y ronda ('' = sin ronda).
CREATE TABLE IF NOT EXISTS tournament_match_sides (
    match_id   text PRIMARY KEY REFERENCES matches (id) ON DELETE CASCADE,
    clan0_id   integer REFERENCES clans (id) ON DELETE SET NULL,
    clan1_id   integer REFERENCES clans (id) ON DELETE SET NULL,
    excluded   boolean NOT NULL DEFAULT false,
    phase_id   integer REFERENCES tournament_phases (id) ON DELETE SET NULL,
    round      text NOT NULL DEFAULT '' CHECK (length(round) <= 32),
    updated_at timestamptz NOT NULL DEFAULT now(),
    updated_by text
);

-- Pedidos de cambio de torneo de una partida (D23). Los crea el panel admin
-- (tournament_event_request) y los aplica el ingestor (apply_event_changes + recalculo).
-- new_event '' = sacar del torneo (partida normal).
CREATE TABLE IF NOT EXISTS match_event_changes (
    id           serial PRIMARY KEY,
    match_id     text NOT NULL REFERENCES matches (id) ON DELETE CASCADE,
    new_event    text NOT NULL CHECK (length(new_event) <= 64),
    requested_by text,
    requested_at timestamptz NOT NULL DEFAULT now(),
    applied_at   timestamptz
);
CREATE INDEX IF NOT EXISTS match_event_changes_pending_idx
    ON match_event_changes (match_id) WHERE applied_at IS NULL;

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

-- Kills seguidas para el primer anuncio de racha: igual al cvar exbattleinfo
-- del servidor (server.cfg: set exbattleinfo 3). Los niveles siguen cada 3:
-- KILLING SPREE = base, RAMPAGE = base+3, DOMINATING = base+6,
-- UNSTOPPABLE = base+9, GODLIKE = base+12. Si se cambia, ejecutar
-- SELECT rebuild_rollups(); para recalcular el historial.
CREATE OR REPLACE FUNCTION streak_base() RETURNS integer
LANGUAGE sql IMMUTABLE AS $$ SELECT 3 $$;

-- Suma las partidas indicadas a las tablas de resumen. Cada partida debe
-- sumarse una sola vez (ingest.py lo hace en la misma transaccion del insert).
CREATE OR REPLACE FUNCTION apply_rollups(ids text[]) RETURNS void
LANGUAGE plpgsql AS $$
BEGIN
    -- Rachas por nivel, reconstruidas desde kills con las reglas de
    -- KillingSpree() / StatsLog_Streak(): suma una kill a un enemigo hecha por
    -- el atacante directo (el desangrado no cuenta), el fuego amigo la corta y
    -- morir la reinicia. Cada racha cuenta en todos los niveles que alcanzo.
    -- streaks_ended: rachas ajenas (>= base) que el jugador corto matando.
    WITH ev AS (
        SELECT k.match_id, k.killer_id AS player_id, k.t, k.id, 0 AS ord,
               k.friendly_fire::int AS reset,
               (NOT k.friendly_fire AND NOT k.suicide AND k.killer_id <> k.victim_id
                AND k.mod <> 'wound')::int AS kill,
               NULL::integer AS ender
        FROM kills k
        WHERE k.match_id = ANY (ids) AND k.killer_id IS NOT NULL
        UNION ALL
        SELECT k.match_id, k.victim_id, k.t, k.id, 1, 1, 0,
               CASE WHEN k.killer_id <> k.victim_id AND k.mod <> 'wound' THEN k.killer_id END
        FROM kills k
        WHERE k.match_id = ANY (ids)
    ),
    runs AS (
        SELECT ev.*, sum(ev.reset) OVER (PARTITION BY ev.match_id, ev.player_id
                                         ORDER BY ev.t, ev.id, ev.ord
                                         ROWS UNBOUNDED PRECEDING) AS run
        FROM ev
    ),
    lens AS (
        SELECT match_id, player_id, run, sum(kill) AS len
        FROM runs GROUP BY match_id, player_id, run
    ),
    tiers AS (
        SELECT l.match_id, l.player_id,
               count(*) FILTER (WHERE l.len >= b.v)      AS sprees,
               count(*) FILTER (WHERE l.len >= b.v + 3)  AS rampages,
               count(*) FILTER (WHERE l.len >= b.v + 6)  AS dominatings,
               count(*) FILTER (WHERE l.len >= b.v + 9)  AS unstoppables,
               count(*) FILTER (WHERE l.len >= b.v + 12) AS godlikes
        FROM lens l, (SELECT streak_base() AS v) b
        GROUP BY l.match_id, l.player_id
    ),
    ended AS (
        -- la muerte que abre la racha n cierra la racha n-1 de la victima
        SELECT r.match_id, r.ender AS player_id, count(*) AS n
        FROM runs r
        JOIN lens l ON l.match_id = r.match_id AND l.player_id = r.player_id AND l.run = r.run - 1
        WHERE r.ord = 1 AND r.ender IS NOT NULL AND l.len >= streak_base()
        GROUP BY r.match_id, r.ender
    )
    UPDATE match_players mp SET
        sprees = coalesce(t.sprees, 0), rampages = coalesce(t.rampages, 0),
        dominatings = coalesce(t.dominatings, 0), unstoppables = coalesce(t.unstoppables, 0),
        godlikes = coalesce(t.godlikes, 0), streaks_ended = coalesce(e.n, 0)
    FROM match_players m2
    LEFT JOIN tiers t ON t.match_id = m2.match_id AND t.player_id = m2.player_id
    LEFT JOIN ended e ON e.match_id = m2.match_id AND e.player_id = m2.player_id
    WHERE m2.match_id = mp.match_id AND m2.player_id = mp.player_id AND mp.match_id = ANY (ids);

    INSERT INTO player_stats AS s (player_id, season_id, kind, mode, event, matches, wins, losses, draws,
        kills, deaths, human_kills, human_deaths, headshots, hits, misses, teamkills, suicides,
        objectives, seconds, best_streak, max_kills, longest_kill, last_match,
        helmet_saves, foot_saves, deflected,
        sprees, rampages, dominatings, unstoppables, godlikes, streaks_ended,
        zone_seconds, zone_captures, zone_kills, ffa_podiums, ffa_placed, ffa_place_sum)
    SELECT mp.player_id, m.season_id, match_category(m.kind, m.event), m.mode, m.event, count(*),
           count(*) FILTER (WHERE mp.result = 'W'), count(*) FILTER (WHERE mp.result = 'L'),
           count(*) FILTER (WHERE mp.result = 'D'),
           sum(mp.kills), sum(mp.deaths), sum(mp.human_kills), sum(mp.human_deaths),
           sum(mp.headshots), sum(mp.hits), sum(mp.misses), sum(mp.teamkills), sum(mp.suicides),
           sum(mp.objectives), sum(mp.time_team0 + mp.time_team1), max(mp.best_streak),
           max(mp.kills), max(lk.longest), max(m.started_at),
           sum(mp.helmet_saves), sum(mp.foot_saves), sum(mp.deflected),
           sum(mp.sprees), sum(mp.rampages), sum(mp.dominatings), sum(mp.unstoppables),
           sum(mp.godlikes), sum(mp.streaks_ended),
           sum(mp.zone_seconds), coalesce(sum(zc.n), 0), sum(mp.zone_kills),
           -- puestos del FFA (solo m.mode = 'ffa'; quien se fue antes no tiene puesto)
           count(*) FILTER (WHERE m.mode = 'ffa' AND mp.place <= 3),
           count(*) FILTER (WHERE m.mode = 'ffa' AND mp.place IS NOT NULL),
           coalesce(sum(mp.place) FILTER (WHERE m.mode = 'ffa'), 0)
    FROM match_players mp
    JOIN matches m ON m.id = mp.match_id
    JOIN players p ON p.id = mp.player_id
    LEFT JOIN (SELECT k.match_id, k.killer_id, max(k.distance) AS longest
               FROM kills k
               WHERE k.match_id = ANY (ids) AND NOT k.friendly_fire AND k.killer_id IS NOT NULL
               GROUP BY k.match_id, k.killer_id) lk
           ON lk.match_id = mp.match_id AND lk.killer_id = mp.player_id
    LEFT JOIN (SELECT o.match_id, o.player_id, count(*) AS n
               FROM objectives o
               WHERE o.match_id = ANY (ids) AND o.type = 'zone_capture' AND o.player_id IS NOT NULL
               GROUP BY o.match_id, o.player_id) zc
           ON zc.match_id = mp.match_id AND zc.player_id = mp.player_id
    WHERE mp.match_id = ANY (ids) AND NOT p.is_bot
    GROUP BY mp.player_id, m.season_id, match_category(m.kind, m.event), m.mode, m.event
    ON CONFLICT (player_id, season_id, kind, mode, event) DO UPDATE SET
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
        last_match = greatest(s.last_match, EXCLUDED.last_match),
        helmet_saves = s.helmet_saves + EXCLUDED.helmet_saves,
        foot_saves = s.foot_saves + EXCLUDED.foot_saves,
        deflected = s.deflected + EXCLUDED.deflected,
        sprees = s.sprees + EXCLUDED.sprees, rampages = s.rampages + EXCLUDED.rampages,
        dominatings = s.dominatings + EXCLUDED.dominatings,
        unstoppables = s.unstoppables + EXCLUDED.unstoppables,
        godlikes = s.godlikes + EXCLUDED.godlikes,
        streaks_ended = s.streaks_ended + EXCLUDED.streaks_ended,
        zone_seconds = s.zone_seconds + EXCLUDED.zone_seconds,
        zone_captures = s.zone_captures + EXCLUDED.zone_captures,
        zone_kills = s.zone_kills + EXCLUDED.zone_kills,
        ffa_podiums = s.ffa_podiums + EXCLUDED.ffa_podiums,
        ffa_placed = s.ffa_placed + EXCLUDED.ffa_placed,
        ffa_place_sum = s.ffa_place_sum + EXCLUDED.ffa_place_sum;

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

    INSERT INTO map_stats AS s (map, kind, mode, matches, allied_wins, axis_wins, draws, seconds, humans,
        kills, best_streak, last_played)
    SELECT m.map, m.kind, m.mode, count(*), count(*) FILTER (WHERE m.winner = 0),
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
    GROUP BY m.map, m.kind, m.mode
    ON CONFLICT (map, kind, mode) DO UPDATE SET
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

-- Partidas FFA viejas (cargadas antes de que la DLL registrara el final del
-- FFA): terminaron con end_reason 'other', sin puestos ni resultado. Se
-- completan con el puesto por score (los empatados comparten puesto), el
-- resultado W/D/L como en las nuevas y end_reason 'normal', y se recalculan los
-- resumenes. Es idempotente: solo toca FFA con end_reason = 'other' y la
-- primera vez las deja en 'normal'. 'ranked' sigue la misma regla que ingest.py
-- (al menos 5 minutos y 3 humanos; aqui todos tienen puesto).
DO $$
DECLARE
    ids text[];
BEGIN
    SELECT array_agg(id) INTO ids FROM matches WHERE mode = 'ffa' AND end_reason = 'other';
    IF ids IS NULL THEN
        RETURN;
    END IF;

    UPDATE match_players mp SET place = r.place
    FROM (SELECT match_id, player_id,
                 rank() OVER (PARTITION BY match_id ORDER BY score DESC)::smallint AS place
          FROM match_players WHERE match_id = ANY (ids)) r
    WHERE mp.match_id = r.match_id AND mp.player_id = r.player_id;

    UPDATE match_players mp SET result = CASE
        WHEN mp.place <> 1 THEN 'L'
        WHEN (SELECT count(*) FROM match_players x
              WHERE x.match_id = mp.match_id AND x.place = 1) = 1 THEN 'W'
        ELSE 'D' END
    WHERE mp.match_id = ANY (ids);

    UPDATE matches m SET end_reason = 'normal',
        ranked = (m.duration_s >= 300 AND
                  (SELECT count(*) FROM match_players mp JOIN players p ON p.id = mp.player_id
                   WHERE mp.match_id = m.id AND NOT p.is_bot AND mp.place IS NOT NULL
                     AND mp.time_team0 + mp.time_team1 > 0) >= 3)
    WHERE m.id = ANY (ids);

    IF to_regclass('pg_temp.streak_backfill_needed') IS NULL THEN
        CREATE TEMP TABLE streak_backfill_needed () ON COMMIT DROP;
    END IF;
    RAISE NOTICE 'FFA viejas completadas: %', array_length(ids, 1);
END $$;

-- base con datos de partidas pero resumenes vacios (version anterior del esquema)
DO $$
BEGIN
    IF EXISTS (SELECT 1 FROM matches)
       AND (NOT EXISTS (SELECT 1 FROM player_stats) OR NOT EXISTS (SELECT 1 FROM map_stats)
            OR NOT EXISTS (SELECT 1 FROM daily_stats)
            OR to_regclass('pg_temp.streak_backfill_needed') IS NOT NULL) THEN
        PERFORM rebuild_rollups();
    END IF;
END $$;

------------------------------------------------------------------------------
-- Vistas (se recrean siempre; no guardan datos)
------------------------------------------------------------------------------

DROP VIEW IF EXISTS v_player_profile, v_player_totals, v_player_totals_by_kind,
    v_player_totals_by_mode, player_totals_base, v_player_maps, v_player_fav_map, v_player_weapons,
    v_player_fav_weapon, v_player_classes, v_player_fav_class, v_player_duels,
    v_player_nemesis, v_player_favorite_victim, v_player_records, v_map_stats, v_map_stats_by_mode,
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
DROP FUNCTION IF EXISTS ladder_luck(integer, text, text);
DROP FUNCTION IF EXISTS ladder_unlucky(integer, text, text);
DROP FUNCTION IF EXISTS ladder_streak_tiers(integer, text, text);
-- firmas actuales (se recrean en cada ejecucion; CREATE FUNCTION no reemplaza)
DROP FUNCTION IF EXISTS player_totals(text, text, text, text) CASCADE;
DROP FUNCTION IF EXISTS ladder_kd(integer, text, text, text, text);
DROP FUNCTION IF EXISTS ladder_elo(integer, text, text, text);
DROP FUNCTION IF EXISTS ladder_streak(text, integer, text, text);
DROP FUNCTION IF EXISTS ladder_luck(integer, text, text, text);
DROP FUNCTION IF EXISTS ladder_unlucky(integer, text, text, text);
DROP FUNCTION IF EXISTS ladder_streak_tiers(integer, text, text, text);
DROP FUNCTION IF EXISTS ladder_zone(integer, text, text);
DROP FUNCTION IF EXISTS ladder_ffa(integer, text, text);

-- Totales por jugador para una temporada, categoria y torneo:
--   season: 'current' (la vigente), 'all' (historico) o el nombre de una temporada
--   kind:   'public', 'duel', 'official' o 'all'
--   event:  nombre de un torneo, o NULL para no filtrar
--   mode:   modo de juego ('dm', 'ctb', 'campaign', 'control'...) o 'all'
-- kills / deaths / kd / kills_per_min cuentan solo enfrentamientos con humanos;
-- las kills a bots se muestran aparte en bot_kills.
CREATE FUNCTION player_totals(season text DEFAULT 'all', match_kind text DEFAULT 'all',
                              match_event text DEFAULT NULL, match_mode text DEFAULT 'all')
RETURNS TABLE (player_id integer, name text, matches bigint, wins bigint, losses bigint,
               draws bigint, kills bigint, deaths bigint, kd numeric, bot_kills bigint,
               headshots bigint, hs_pct numeric, accuracy_pct numeric, teamkills bigint,
               suicides bigint, objectives bigint, best_streak integer, hours_played numeric,
               kills_per_min numeric, win_pct numeric, most_kills_match integer,
               longest_kill integer, last_match timestamptz,
               helmet_saves bigint, foot_saves bigint, luck bigint, deflected bigint,
               sprees bigint, rampages bigint, dominatings bigint, unstoppables bigint,
               godlikes bigint, streaks_ended bigint,
               zone_captures bigint, zone_minutes numeric, zone_kills bigint)
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
               max(ps.last_match) AS last_match, sum(ps.helmet_saves) AS helmet_saves,
               sum(ps.foot_saves) AS foot_saves, sum(ps.deflected) AS deflected,
               sum(ps.sprees) AS sprees, sum(ps.rampages) AS rampages,
               sum(ps.dominatings) AS dominatings, sum(ps.unstoppables) AS unstoppables,
               sum(ps.godlikes) AS godlikes, sum(ps.streaks_ended) AS streaks_ended,
               sum(ps.zone_captures) AS zone_captures, sum(ps.zone_seconds) AS zone_seconds,
               sum(ps.zone_kills) AS zone_kills
        FROM player_stats ps, sid
        WHERE (season = 'all' OR ps.season_id = sid.id)
          AND (match_kind = 'all' OR ps.kind = match_kind)
          AND (match_event IS NULL OR ps.event = match_event)
          AND (match_mode = 'all' OR ps.mode = match_mode)
        GROUP BY ps.player_id
    )
    SELECT s.player_id, p.name, s.matches, s.wins, s.losses, s.draws,
           s.hk, s.hd, round(s.hk::numeric / greatest(s.hd, 1), 2), s.kills - s.hk,
           s.hs, round(100.0 * s.hs / greatest(s.kills, 1), 1),
           round(100.0 * s.hits / greatest(s.hits + s.misses, 1), 1),
           s.tk, s.suicides, s.objectives, s.best_streak,
           round(s.seconds / 3600.0, 1), round(s.hk * 60.0 / greatest(s.seconds, 1), 2),
           round(100.0 * s.wins / greatest(s.wins + s.losses + s.draws, 1), 1),
           s.max_kills, s.longest_kill, s.last_match,
           s.helmet_saves, s.foot_saves, s.helmet_saves + s.foot_saves, s.deflected,
           s.sprees, s.rampages, s.dominatings, s.unstoppables, s.godlikes, s.streaks_ended,
           s.zone_captures, round(s.zone_seconds / 60.0, 1), s.zone_kills
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

-- Totales historicos por jugador y modo de juego (dm, ctb, control...)
CREATE VIEW v_player_totals_by_mode AS
SELECT m.mode, t.*
FROM (SELECT DISTINCT mode FROM player_stats) m,
     LATERAL player_totals('all', 'all', NULL, m.mode) t;

-- Ladder por K/D (por defecto: temporada actual, todas las categorias y modos)
CREATE FUNCTION ladder_kd(min_matches integer DEFAULT 10, match_kind text DEFAULT 'all',
                          season text DEFAULT 'current', match_event text DEFAULT NULL,
                          match_mode text DEFAULT 'all')
RETURNS TABLE (pos bigint, name text, matches bigint, kills bigint, deaths bigint,
               kd numeric, kills_per_min numeric, win_pct numeric, hs_pct numeric)
LANGUAGE sql STABLE AS $$
    SELECT rank() OVER (ORDER BY t.kd DESC, t.kills DESC),
           t.name, t.matches, t.kills, t.deaths, t.kd, t.kills_per_min, t.win_pct, t.hs_pct
    FROM player_totals(season, match_kind, match_event, match_mode) t
    WHERE t.matches >= min_matches
    ORDER BY 1, t.name;
$$;

-- Ladder por rating Elo. kind: 'public', 'duel' u 'official' (cada uno con su
-- rating). season: 'current', el nombre de una temporada o 'all' (historico).
-- El Elo es uno solo para todos los modos de juego.
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
                              season text DEFAULT 'current', match_mode text DEFAULT 'all')
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
            AND (match_mode = 'all' OR m.mode = match_mode)
            AND (season = 'all' OR m.season_id = (SELECT season_lookup(season)))
          ORDER BY mp.best_streak DESC, m.started_at
          LIMIT max_rows) t
    ORDER BY t.best_streak DESC, t.started_at;
$$;

-- Los mas suertudos: casco que desvia un tiro a la cabeza + sobrevivir con
-- "almost lost a foot". luck_per_match permite comparar a quien juega poco.
CREATE FUNCTION ladder_luck(min_matches integer DEFAULT 5, match_kind text DEFAULT 'all',
                            season text DEFAULT 'current', match_mode text DEFAULT 'all')
RETURNS TABLE (pos bigint, name text, luck bigint, helmet_saves bigint, foot_saves bigint,
               matches bigint, luck_per_match numeric)
LANGUAGE sql STABLE AS $$
    SELECT rank() OVER (ORDER BY t.luck DESC), t.name, t.luck, t.helmet_saves, t.foot_saves,
           t.matches, round(t.luck::numeric / greatest(t.matches, 1), 2)
    FROM player_totals(season, match_kind, NULL, match_mode) t
    WHERE t.matches >= min_matches AND t.luck > 0
    ORDER BY t.luck DESC, t.name;
$$;

-- Los tiradores mas desafortunados: mas tiros desviados por cascos ajenos
CREATE FUNCTION ladder_unlucky(min_matches integer DEFAULT 5, match_kind text DEFAULT 'all',
                               season text DEFAULT 'current', match_mode text DEFAULT 'all')
RETURNS TABLE (pos bigint, name text, deflected bigint, matches bigint, deflected_per_match numeric)
LANGUAGE sql STABLE AS $$
    SELECT rank() OVER (ORDER BY t.deflected DESC), t.name, t.deflected, t.matches,
           round(t.deflected::numeric / greatest(t.matches, 1), 2)
    FROM player_totals(season, match_kind, NULL, match_mode) t
    WHERE t.matches >= min_matches AND t.deflected > 0
    ORDER BY t.deflected DESC, t.name;
$$;

-- Rachas por nivel: quien mas veces llego a GODLIKE, UNSTOPPABLE, etc.
CREATE FUNCTION ladder_streak_tiers(min_matches integer DEFAULT 1, match_kind text DEFAULT 'all',
                                    season text DEFAULT 'current', match_mode text DEFAULT 'all')
RETURNS TABLE (pos bigint, name text, matches bigint, godlikes bigint, unstoppables bigint,
               dominatings bigint, rampages bigint, sprees bigint, streaks_ended bigint,
               best_streak integer)
LANGUAGE sql STABLE AS $$
    SELECT rank() OVER (ORDER BY t.godlikes DESC, t.unstoppables DESC, t.dominatings DESC,
                                 t.rampages DESC, t.sprees DESC),
           t.name, t.matches, t.godlikes, t.unstoppables, t.dominatings, t.rampages, t.sprees,
           t.streaks_ended, t.best_streak
    FROM player_totals(season, match_kind, NULL, match_mode) t
    WHERE t.matches >= min_matches AND (t.sprees > 0 OR t.streaks_ended > 0)
    ORDER BY 1, t.name;
$$;

-- Modo control de zona: quien mas capturas hizo (cuenta cada jugador que estaba
-- en la zona cuando su equipo la capturo), cuanto tiempo paso dentro de ella y
-- cuantas kills hizo defendiendola o limpiandola (zone_kills)
CREATE FUNCTION ladder_zone(min_matches integer DEFAULT 5, match_kind text DEFAULT 'all',
                            season text DEFAULT 'current')
RETURNS TABLE (pos bigint, name text, matches bigint, wins bigint, win_pct numeric,
               zone_captures bigint, zone_minutes numeric, captures_per_match numeric,
               zone_kills bigint)
LANGUAGE sql STABLE AS $$
    SELECT rank() OVER (ORDER BY t.zone_captures DESC, t.zone_minutes DESC),
           t.name, t.matches, t.wins, t.win_pct, t.zone_captures, t.zone_minutes,
           round(t.zone_captures::numeric / greatest(t.matches, 1), 2), t.zone_kills
    FROM player_totals(season, match_kind, NULL, 'control') t
    WHERE t.matches >= min_matches
    ORDER BY 1, t.name;
$$;

-- Ladder del Free For All (modo ffa): victorias (1.er puesto), podios (top 3),
-- puesto promedio (solo partidas que termino), K/D y mejor racha. No usa el Elo.
CREATE FUNCTION ladder_ffa(min_matches integer DEFAULT 3, match_kind text DEFAULT 'all',
                           season text DEFAULT 'current')
RETURNS TABLE (pos bigint, name text, matches bigint, wins bigint, win_pct numeric,
               podiums bigint, avg_place numeric, kills bigint, deaths bigint,
               kd numeric, best_streak integer)
LANGUAGE sql STABLE AS $$
    WITH sid AS (SELECT season_lookup(season) AS id),
    s AS (
        SELECT ps.player_id, sum(ps.matches) AS matches, sum(ps.wins) AS wins,
               sum(ps.ffa_podiums) AS podiums, sum(ps.ffa_placed) AS placed,
               sum(ps.ffa_place_sum) AS place_sum, sum(ps.kills) AS kills,
               sum(ps.deaths) AS deaths, max(ps.best_streak) AS best_streak
        FROM player_stats ps, sid
        WHERE ps.mode = 'ffa'
          AND (season = 'all' OR ps.season_id = sid.id)
          AND (match_kind = 'all' OR ps.kind = match_kind)
        GROUP BY ps.player_id
    )
    SELECT rank() OVER (ORDER BY s.wins DESC, s.podiums DESC,
                        s.place_sum::numeric / greatest(s.placed, 1)),
           p.name, s.placed, s.wins,
           round(100.0 * s.wins / greatest(s.placed, 1), 1), s.podiums,
           CASE WHEN s.placed > 0 THEN round(s.place_sum::numeric / s.placed, 2) END,
           s.kills, s.deaths, round(s.kills::numeric / greatest(s.deaths, 1), 2), s.best_streak
    FROM s
    JOIN players p ON p.id = s.player_id
    WHERE s.placed >= min_matches
    ORDER BY 1, p.name;
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
       round(r.rating)::integer  AS rating,          -- Elo general de la temporada actual
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

-- Estadisticas por mapa y tipo, todos los modos juntos (balance aliados / eje,
-- duracion, letalidad)
CREATE VIEW v_map_stats AS
SELECT map,
       kind,
       sum(matches)                                   AS matches,
       sum(allied_wins)                               AS allied_wins,
       sum(axis_wins)                                 AS axis_wins,
       sum(draws)                                     AS draws,
       round(100.0 * sum(allied_wins) / greatest(sum(allied_wins) + sum(axis_wins), 1), 1) AS allied_win_pct,
       round(sum(seconds) / 60.0 / greatest(sum(matches), 1), 1)  AS avg_minutes,
       round(sum(humans)::numeric / greatest(sum(matches), 1), 1) AS avg_humans,
       max(best_streak)                               AS best_streak,
       sum(kills)                                     AS kills,
       max(last_played)                               AS last_played
FROM map_stats
GROUP BY map, kind;

-- Lo mismo separado por modo de juego (dm, ctb, control...)
CREATE VIEW v_map_stats_by_mode AS
SELECT map,
       kind,
       mode,
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

------------------------------------------------------------------------------
-- Clanes: regla de pertenencia, vistas y funciones
------------------------------------------------------------------------------

-- Regla de pertenencia: un tag formado solo por letras y numeros (WR, 7up) debe
-- estar al comienzo del nick y seguido de un caracter que no sea letra ni numero
-- (o ser el nick completo): "WR|Juan" si, "Wrath" y "WRmaster" no. Cualquier otro
-- tag (con simbolos) basta con que el nick lo contenga. Sin distinguir mayusculas.
CREATE OR REPLACE FUNCTION clan_tag_matches(nick text, tag text) RETURNS boolean
LANGUAGE sql IMMUTABLE AS $$
    SELECT CASE WHEN tag ~ '^[A-Za-z0-9]+$'
                THEN left(lower(nick), length(tag)) = lower(tag)
                     AND substr(nick, length(tag) + 1, 1) !~ '[A-Za-z0-9]'
                ELSE strpos(lower(nick), lower(tag)) > 0 END
$$;

-- Clanes con su tag principal y la lista de alternativos.
CREATE OR REPLACE VIEW v_clans AS
SELECT c.*, pt.tag,
       coalesce(array_agg(al.tag ORDER BY al.tag) FILTER (WHERE al.tag IS NOT NULL), '{}') AS aliases
  FROM clans c
  JOIN clan_tags pt ON pt.clan_id = c.id AND pt.is_primary
  LEFT JOIN clan_tags al ON al.clan_id = c.id AND NOT al.is_primary
 GROUP BY c.id, pt.tag;

-- Clan de cada nick humano (clan_id NULL si no tiene). Solo clanes activos; si
-- calzan varios gana el tag mas largo, luego menor sort_order y menor id.
CREATE OR REPLACE VIEW v_player_clan AS
SELECT p.id AS player_id, p.name, m.clan_id, m.clan, m.unregistered
  FROM players p
  LEFT JOIN LATERAL (
       SELECT c.id AS clan_id, pt.tag AS clan, c.unregistered
         FROM clan_tags ct
         JOIN clans c ON c.id = ct.clan_id AND c.active
         JOIN clan_tags pt ON pt.clan_id = c.id AND pt.is_primary
        WHERE clan_tag_matches(p.name, ct.tag)
        ORDER BY length(ct.tag) DESC, c.sort_order, c.id
        LIMIT 1) m ON true
 WHERE NOT p.is_bot;

DROP FUNCTION IF EXISTS ladder_clans(text, text);
DROP FUNCTION IF EXISTS clan_duels(text, text, integer);
DROP FUNCTION IF EXISTS clan_preview(integer, text[]);

-- Ranking de clanes: suma de los miembros (por nick) en la temporada y categoria.
-- kd es NULL si los miembros suman menos de 20 muertes (sin muestra).
-- elo_avg: promedio del Elo de los miembros con 5 partidas o mas (categoria public
-- si match_kind es 'all'). mvps: partidas en que un miembro hizo mas kills que
-- nadie en su partida (bots incluidos; los empates cuentan).
CREATE FUNCTION ladder_clans(match_kind text DEFAULT 'all', season text DEFAULT 'current')
RETURNS TABLE (clan_id integer, clan text, unregistered boolean, members_active bigint,
               matches bigint, kills bigint, deaths bigint, kd numeric, bot_kills bigint,
               hours numeric, wins bigint, losses bigint, win_pct numeric,
               best_streak integer, godlikes bigint, headshots bigint, hs_pct numeric,
               elo_avg numeric, mvps bigint, top_player text)
LANGUAGE sql STABLE AS $$
    WITH t AS (
        SELECT pc.clan_id, pc.clan, pc.unregistered, pt.*
          FROM player_totals(season, match_kind) pt
          JOIN v_player_clan pc ON pc.player_id = pt.player_id
         WHERE pc.clan_id IS NOT NULL
    ),
    agg AS (
        SELECT t.clan_id, t.clan, t.unregistered,
               count(*) FILTER (WHERE t.matches > 0) AS members_active,
               sum(t.matches) AS matches, sum(t.kills) AS kills, sum(t.deaths) AS deaths,
               sum(t.bot_kills) AS bot_kills, sum(t.hours_played) AS hours,
               sum(t.wins) AS wins, sum(t.losses) AS losses, sum(t.draws) AS draws,
               max(t.best_streak) AS best_streak, sum(t.godlikes) AS godlikes,
               sum(t.headshots) AS headshots,
               (array_agg(t.name ORDER BY t.kills DESC, t.name))[1] AS top_player
          FROM t
         GROUP BY t.clan_id, t.clan, t.unregistered
    ),
    elo AS (
        SELECT pc.clan_id, round(avg(r.rating)::numeric) AS elo_avg
          FROM ratings r
          JOIN v_player_clan pc ON pc.player_id = r.player_id
         WHERE pc.clan_id IS NOT NULL AND r.games >= 5
           AND r.kind = CASE WHEN match_kind = 'all' THEN 'public' ELSE match_kind END
           AND r.season_id = coalesce(season_lookup(season), 0)
         GROUP BY pc.clan_id
    ),
    f AS (
        SELECT mp.match_id, mp.player_id, mp.kills
          FROM match_players mp
          JOIN matches m ON m.id = mp.match_id
         WHERE (season = 'all' OR m.season_id = season_lookup(season))
           AND (match_kind = 'all' OR match_category(m.kind, m.event) = match_kind)
    ),
    mx AS (SELECT match_id, max(kills) AS top FROM f GROUP BY match_id),
    mv AS (
        SELECT pc.clan_id, count(*) AS mvps
          FROM f
          JOIN mx ON mx.match_id = f.match_id AND f.kills = mx.top AND mx.top > 0
          JOIN v_player_clan pc ON pc.player_id = f.player_id
         WHERE pc.clan_id IS NOT NULL
         GROUP BY pc.clan_id
    )
    SELECT a.clan_id, a.clan, a.unregistered, a.members_active, a.matches, a.kills, a.deaths,
           CASE WHEN a.deaths >= 20 THEN round(a.kills::numeric / a.deaths, 2) END,
           a.bot_kills, a.hours, a.wins, a.losses,
           round(100.0 * a.wins / greatest(a.wins + a.losses + a.draws, 1), 1),
           a.best_streak, a.godlikes, a.headshots,
           round(100.0 * a.headshots / greatest(a.kills + a.bot_kills, 1), 1),
           e.elo_avg, coalesce(mv.mvps, 0), a.top_player
      FROM agg a
      LEFT JOIN elo e ON e.clan_id = a.clan_id
      LEFT JOIN mv ON mv.clan_id = a.clan_id
     WHERE a.members_active > 0
     ORDER BY a.unregistered, a.kills DESC, a.clan;
$$;

-- Duelos de clanes: partida por equipos, sin ningun bot, en que todos los jugadores
-- de cada equipo son integrantes de un mismo clan (equipo 0 = solo clan A, equipo 1 =
-- solo clan B, A <> B, ninguno sin inscribir). Un humano sin clan anula el duelo.
CREATE FUNCTION clan_duels(match_kind text DEFAULT 'all', season text DEFAULT 'current',
                           min_per_side integer DEFAULT 2)
RETURNS TABLE (match_id text, clan0_id integer, clan0 text, clan0_players bigint,
               clan1_id integer, clan1 text, clan1_players bigint)
LANGUAGE sql STABLE AS $$
    WITH v AS (SELECT * FROM v_player_clan),
    team_comp AS (
        SELECT mp.match_id, mp.team,
               count(*) AS players,
               count(*) FILTER (WHERE p.is_bot) AS bots,
               count(*) FILTER (WHERE NOT p.is_bot AND (v.clan_id IS NULL OR v.unregistered)) AS outsiders,
               count(DISTINCT v.clan_id) AS clans,
               min(v.clan_id) AS clan_id
          FROM match_players mp
          JOIN players p ON p.id = mp.player_id
          LEFT JOIN v ON v.player_id = mp.player_id
          JOIN matches m ON m.id = mp.match_id
         WHERE (season = 'all' OR m.season_id = season_lookup(season))
           AND (match_kind = 'all' OR match_category(m.kind, m.event) = match_kind)
           AND m.mode <> 'ffa' AND mp.team IN (0, 1)
         GROUP BY mp.match_id, mp.team
    ),
    pure AS (SELECT * FROM team_comp
              WHERE bots = 0 AND outsiders = 0 AND clans = 1 AND players >= min_per_side)
    SELECT a.match_id, a.clan_id, ca.tag, a.players, b.clan_id, cb.tag, b.players
      FROM pure a
      JOIN pure b ON b.match_id = a.match_id AND a.team = 0 AND b.team = 1
      JOIN clan_tags ca ON ca.clan_id = a.clan_id AND ca.is_primary
      JOIN clan_tags cb ON cb.clan_id = b.clan_id AND cb.is_primary
     WHERE a.clan_id <> b.clan_id
       AND NOT EXISTS (SELECT 1 FROM match_players x JOIN players px ON px.id = x.player_id
                        WHERE x.match_id = a.match_id AND px.is_bot);
$$;

-- Vista previa del mantenedor: que nicks humanos quedarian en el clan p_clan_id (o en
-- uno nuevo si es NULL) si sus tags fueran p_tags, compitiendo con los tags de todos
-- los demas clanes activos (gana el tag mas largo). from_clan: clan actual del nick
-- si es otro; NULL si hoy no tiene clan.
CREATE FUNCTION clan_preview(p_clan_id integer, p_tags text[])
RETURNS TABLE (name text, matches bigint, last_match timestamptz, from_clan text)
LANGUAGE sql STABLE AS $$
    WITH cand AS (
        SELECT ct.tag, c.id AS clan_id, c.sort_order, false AS target
          FROM clan_tags ct
          JOIN clans c ON c.id = ct.clan_id AND c.active
         WHERE c.id IS DISTINCT FROM p_clan_id
        UNION ALL
        SELECT t.tag, coalesce(p_clan_id, 2147483647),
               coalesce((SELECT sort_order FROM clans WHERE id = p_clan_id), 2147483647), true
          FROM unnest(p_tags) AS t(tag)
         WHERE t.tag <> ''
    ),
    hit AS (
        SELECT p.id AS player_id, p.name, b.target
          FROM players p
          CROSS JOIN LATERAL (
               SELECT cand.target FROM cand
                WHERE clan_tag_matches(p.name, cand.tag)
                ORDER BY length(cand.tag) DESC, cand.sort_order, cand.clan_id
                LIMIT 1) b
         WHERE NOT p.is_bot
    )
    SELECT h.name, count(mp.match_id), max(m.started_at),
           (SELECT pc.clan FROM v_player_clan pc
             WHERE pc.player_id = h.player_id AND pc.clan_id IS DISTINCT FROM p_clan_id)
      FROM hit h
      LEFT JOIN match_players mp ON mp.player_id = h.player_id
      LEFT JOIN matches m ON m.id = mp.match_id
     WHERE h.target
     GROUP BY h.player_id, h.name
     ORDER BY count(mp.match_id) DESC, h.name;
$$;

-- Escritura de clanes: unica via para el rol dday_clans (SECURITY DEFINER).
-- p_tags[1] es el tag principal; el resto, alternativos. p_id NULL crea el clan.
-- Un tag repetido lo rechaza el indice clan_tags_key_idx (SQLSTATE 23505).
CREATE OR REPLACE FUNCTION clan_save(p_id integer, p_tags text[], p_data jsonb, p_admin text)
RETURNS integer
LANGUAGE plpgsql SECURITY DEFINER SET search_path = public AS $$
DECLARE
    v_id integer;
BEGIN
    IF p_tags IS NULL OR coalesce(array_length(p_tags, 1), 0) = 0 THEN
        RAISE EXCEPTION 'el clan necesita al menos un tag' USING ERRCODE = '22023';
    END IF;
    IF p_id IS NULL THEN
        INSERT INTO clans (name, leader, declared_members, favorite_map, symbol_player,
                           phrase, rival, logo_url, unregistered, active, sort_order,
                           updated_by)
        VALUES (coalesce(p_data->>'name', ''), coalesce(p_data->>'leader', ''),
                nullif(p_data->>'declared_members', '')::integer,
                coalesce(p_data->>'favorite_map', ''), coalesce(p_data->>'symbol_player', ''),
                coalesce(p_data->>'phrase', ''), coalesce(p_data->>'rival', ''),
                coalesce(p_data->>'logo_url', ''),
                coalesce((p_data->>'unregistered')::boolean, false),
                coalesce((p_data->>'active')::boolean, true),
                coalesce((p_data->>'sort_order')::integer, 0), p_admin)
        RETURNING id INTO v_id;
    ELSE
        UPDATE clans SET
            name = coalesce(p_data->>'name', ''), leader = coalesce(p_data->>'leader', ''),
            declared_members = nullif(p_data->>'declared_members', '')::integer,
            favorite_map = coalesce(p_data->>'favorite_map', ''),
            symbol_player = coalesce(p_data->>'symbol_player', ''),
            phrase = coalesce(p_data->>'phrase', ''), rival = coalesce(p_data->>'rival', ''),
            logo_url = coalesce(p_data->>'logo_url', ''),
            unregistered = coalesce((p_data->>'unregistered')::boolean, false),
            active = coalesce((p_data->>'active')::boolean, true),
            sort_order = coalesce((p_data->>'sort_order')::integer, 0),
            updated_at = now(), updated_by = p_admin
         WHERE id = p_id
        RETURNING id INTO v_id;
        IF v_id IS NULL THEN
            RAISE EXCEPTION 'el clan % no existe', p_id USING ERRCODE = 'no_data_found';
        END IF;
        DELETE FROM clan_tags WHERE clan_id = v_id;
    END IF;
    INSERT INTO clan_tags (clan_id, tag, is_primary)
    SELECT v_id, t.tag, t.ord = 1 FROM unnest(p_tags) WITH ORDINALITY AS t(tag, ord);
    RETURN v_id;
END $$;

-- Borra un clan (sus tags se van en cascada). true si existia.
CREATE OR REPLACE FUNCTION clan_delete(p_id integer) RETURNS boolean
LANGUAGE plpgsql SECURITY DEFINER SET search_path = public AS $$
BEGIN
    DELETE FROM clans WHERE id = p_id;
    RETURN FOUND;
END $$;

-- Permisos: las funciones de escritura solo las ejecuta el dueno y dday_clans. El rol
-- dday_clans (lleva clave) no se crea aqui: ver docs/stats/DESPLIEGUE.md.
REVOKE ALL ON FUNCTION clan_save(integer, text[], jsonb, text) FROM PUBLIC;
REVOKE ALL ON FUNCTION clan_delete(integer) FROM PUBLIC;
DO $$
BEGIN
    IF EXISTS (SELECT 1 FROM pg_roles WHERE rolname = 'dday_clans') THEN
        GRANT EXECUTE ON FUNCTION clan_save(integer, text[], jsonb, text) TO dday_clans;
        GRANT EXECUTE ON FUNCTION clan_delete(integer) TO dday_clans;
    END IF;
END $$;

DROP VIEW IF EXISTS v_tournaments;
DROP FUNCTION IF EXISTS tournament_standings(text);
DROP FUNCTION IF EXISTS tournament_series(text);
DROP FUNCTION IF EXISTS tournament_sides(text);

-- Torneos: fichas + claves usadas en partidas sin ficha (D4, D18).
CREATE VIEW v_tournaments AS
SELECT t.id,
       coalesce(t.event, e.event)                          AS event,
       coalesce(nullif(t.name, ''), t.event, e.event)      AS name,
       CASE WHEN t.id IS NOT NULL THEN t.status
            WHEN e.last_match >= now() - interval '7 days' THEN 'live'
            ELSE 'finished' END                            AS status,
       t.starts_on, t.ends_on,
       coalesce(t.format, '')      AS format,
       coalesce(t.description, '') AS description,
       coalesce(t.rules_url, '')   AS rules_url,
       coalesce(t.logo_url, '')    AS logo_url,
       t.champion_id, t.runner_up_id,
       coalesce(t.visible, true)   AS visible,
       t.id IS NOT NULL            AS registered,
       (SELECT count(*) FROM tournament_phases p WHERE p.tournament_id = t.id) AS phases,
       coalesce(e.matches, 0)      AS matches,
       e.first_match, e.last_match,
       coalesce(e.maps, '{}')      AS maps,
       t.updated_at, t.updated_by
  FROM tournaments t
  FULL JOIN v_events e ON e.event = t.event;

-- Clan, fase y ronda de cada partida de un torneo (D7 automatico, D8 correccion, D22 fase).
-- counted: la partida suma a cruces y tabla (D9). Es la UNICA definicion de "cuenta":
-- la usan tournament_series, tournament_standings, el backend y el mantenedor.
-- overridden: el admin fijo un clan o la marco "no cuenta" (fase o ronda no cuentan).
-- phase_id NULL solo si el torneo no tiene ficha.
CREATE FUNCTION tournament_sides(p_event text)
RETURNS TABLE (match_id text, detected0 integer, detected1 integer,
               clan0_id integer, clan1_id integer, excluded boolean,
               overridden boolean, counted boolean, phase_id integer,
               phase_manual boolean, round text)
LANGUAGE sql STABLE AS $$
    WITH t AS (SELECT id FROM tournaments WHERE event = p_event),
    ph AS (
        SELECT p.id, p.sort_order,
               CASE WHEN row_number() OVER (ORDER BY p.sort_order, p.id) = 1 THEN '-infinity'::timestamptz
                    ELSE coalesce(p.starts_at, '-infinity') END AS since
          FROM tournament_phases p JOIN t ON t.id = p.tournament_id
    ),
    pool AS (
        SELECT tt.clan_id FROM tournament_teams tt JOIN t ON t.id = tt.tournament_id
        UNION
        SELECT c.id FROM clans c
         WHERE c.active AND NOT c.unregistered
           AND NOT EXISTS (SELECT 1 FROM tournament_teams tt JOIN t ON t.id = tt.tournament_id)
    ),
    side AS (
        SELECT mp.match_id, mp.team, count(*) AS humans
          FROM matches m
          JOIN match_players mp ON mp.match_id = m.id
          JOIN players p ON p.id = mp.player_id AND NOT p.is_bot
         WHERE m.event = p_event AND mp.team IN (0, 1)
         GROUP BY mp.match_id, mp.team
    ),
    votes AS (
        SELECT mp.match_id, mp.team, pc.clan_id, count(*) AS n
          FROM matches m
          JOIN match_players mp ON mp.match_id = m.id
          JOIN v_player_clan pc ON pc.player_id = mp.player_id
          JOIN pool ON pool.clan_id = pc.clan_id
         WHERE m.event = p_event AND mp.team IN (0, 1)
         GROUP BY mp.match_id, mp.team, pc.clan_id
    ),
    best AS (
        SELECT v.match_id, v.team, v.clan_id
          FROM votes v
          JOIN side s ON s.match_id = v.match_id AND s.team = v.team
         WHERE 2 * v.n >= s.humans
           AND NOT EXISTS (SELECT 1 FROM votes o
                            WHERE o.match_id = v.match_id AND o.team = v.team
                              AND o.clan_id <> v.clan_id AND o.n >= v.n)
    ),
    r AS (
        SELECT m.id, m.winner, m.end_reason, b0.clan_id AS d0, b1.clan_id AS d1,
               coalesce(o.clan0_id, b0.clan_id) AS c0, coalesce(o.clan1_id, b1.clan_id) AS c1,
               coalesce(o.excluded, false) AS ex,
               (o.clan0_id IS NOT NULL OR o.clan1_id IS NOT NULL OR coalesce(o.excluded, false)) AS ov,
               coalesce(o.phase_id,
                        (SELECT ph.id FROM ph WHERE ph.since <= m.started_at
                          ORDER BY ph.since DESC, ph.sort_order DESC, ph.id DESC LIMIT 1)) AS pid,
               o.phase_id IS NOT NULL AS pm,
               coalesce(o.round, '') AS rd
          FROM matches m
          LEFT JOIN best b0 ON b0.match_id = m.id AND b0.team = 0
          LEFT JOIN best b1 ON b1.match_id = m.id AND b1.team = 1
          LEFT JOIN tournament_match_sides o ON o.match_id = m.id
         WHERE m.event = p_event
    )
    SELECT r.id, r.d0, r.d1, r.c0, r.c1, r.ex, r.ov,
           coalesce(NOT r.ex AND r.c0 IS NOT NULL AND r.c1 IS NOT NULL AND r.c0 <> r.c1
                    AND r.winner IN (-1, 0, 1) AND r.end_reason IS DISTINCT FROM 'aborted',
                    false),
           r.pid, r.pm, r.rd
      FROM r;
$$;

-- Cruces de un torneo (D19, D20): partidas que cuentan entre el mismo par de clanes, en
-- la misma fase y la misma ronda. clan_a = el id menor del par. winner_id NULL = empate.
-- by_frags: el ganador salio por frags porque empataron en mapas.
CREATE FUNCTION tournament_series(p_event text)
RETURNS TABLE (phase_id integer, round text, clan_a integer, clan_b integer, matches bigint,
               maps_a bigint, maps_b bigint, map_draws bigint,
               frags_a bigint, frags_b bigint, winner_id integer, by_frags boolean,
               first_match timestamptz, last_match timestamptz)
LANGUAGE sql STABLE AS $$
    WITH c AS (
        SELECT s.phase_id AS pid, s.round AS rd, m.started_at,
               least(s.clan0_id, s.clan1_id) AS a, greatest(s.clan0_id, s.clan1_id) AS b,
               CASE WHEN m.winner = -1 THEN NULL
                    WHEN m.winner = 0 THEN s.clan0_id ELSE s.clan1_id END AS won_by,
               CASE WHEN s.clan0_id < s.clan1_id THEN coalesce(m.team0_kills, 0)
                    ELSE coalesce(m.team1_kills, 0) END AS fa,
               CASE WHEN s.clan0_id < s.clan1_id THEN coalesce(m.team1_kills, 0)
                    ELSE coalesce(m.team0_kills, 0) END AS fb
          FROM tournament_sides(p_event) s
          JOIN matches m ON m.id = s.match_id
         WHERE s.counted
    ),
    g AS (
        SELECT pid, rd, a, b, count(*) AS n,
               count(*) FILTER (WHERE won_by = a) AS ma,
               count(*) FILTER (WHERE won_by = b) AS mb,
               count(*) FILTER (WHERE won_by IS NULL) AS md,
               sum(fa)::bigint AS fa, sum(fb)::bigint AS fb,
               min(started_at) AS f, max(started_at) AS l
          FROM c
         GROUP BY pid, rd, a, b
    )
    SELECT pid, rd, a, b, n, ma, mb, md, fa, fb,
           CASE WHEN ma > mb THEN a WHEN mb > ma THEN b
                WHEN fa > fb THEN a WHEN fb > fa THEN b END,
           (ma = mb AND fa <> fb),
           f, l
      FROM g;
$$;

-- Tabla de posiciones por fase (D21). Se calcula para todas las fases de la ficha; la app
-- solo la muestra en las de liga. En la PRIMERA fase incluye a los inscritos que aun no
-- juegan y sus grupos; en las demas, solo a los clanes que jugaron (sin grupo).
-- points_per = 'match': una fila por mapa; 'series': una fila por cruce.
CREATE FUNCTION tournament_standings(p_event text)
RETURNS TABLE (phase_id integer, group_name text, clan_id integer, played bigint,
               wins bigint, draws bigint, losses bigint, frags_for bigint,
               frags_against bigint, points bigint)
LANGUAGE sql STABLE AS $$
    WITH t AS (SELECT * FROM tournaments WHERE event = p_event),
    ph AS (
        SELECT p.*, row_number() OVER (ORDER BY p.sort_order, p.id) = 1 AS is_first
          FROM tournament_phases p JOIN t ON t.id = p.tournament_id
    ),
    c AS (
        SELECT s.phase_id AS pid, s.clan0_id, s.clan1_id, m.winner,
               coalesce(m.team0_kills, 0) AS k0, coalesce(m.team1_kills, 0) AS k1
          FROM tournament_sides(p_event) s
          JOIN matches m ON m.id = s.match_id
          JOIN ph ON ph.id = s.phase_id AND ph.points_per = 'match'
         WHERE s.counted
    ),
    sr AS (
        SELECT x.* FROM tournament_series(p_event) x
          JOIN ph ON ph.id = x.phase_id AND ph.points_per = 'series'
    ),
    per AS (
        SELECT pid, clan0_id AS clan_id,
               CASE winner WHEN 0 THEN 'W' WHEN 1 THEN 'L' ELSE 'D' END AS r, k0 AS ff, k1 AS fa
          FROM c
        UNION ALL
        SELECT pid, clan1_id, CASE winner WHEN 1 THEN 'W' WHEN 0 THEN 'L' ELSE 'D' END, k1, k0
          FROM c
        UNION ALL
        SELECT sr.phase_id, clan_a,
               CASE WHEN winner_id IS NULL THEN 'D' WHEN winner_id = clan_a THEN 'W' ELSE 'L' END,
               frags_a, frags_b
          FROM sr
        UNION ALL
        SELECT sr.phase_id, clan_b,
               CASE WHEN winner_id IS NULL THEN 'D' WHEN winner_id = clan_b THEN 'W' ELSE 'L' END,
               frags_b, frags_a
          FROM sr
    ),
    teams AS (
        SELECT ph.id AS pid, tt.clan_id, tt.group_name
          FROM ph JOIN tournament_teams tt ON tt.tournament_id = ph.tournament_id
         WHERE ph.is_first
        UNION
        SELECT DISTINCT per.pid, per.clan_id, coalesce(g.group_name, '')
          FROM per
          JOIN ph ON ph.id = per.pid
          LEFT JOIN tournament_teams g
                 ON ph.is_first AND g.tournament_id = ph.tournament_id AND g.clan_id = per.clan_id
    )
    SELECT tm.pid, tm.group_name, tm.clan_id,
           count(per.r),
           count(*) FILTER (WHERE per.r = 'W'),
           count(*) FILTER (WHERE per.r = 'D'),
           count(*) FILTER (WHERE per.r = 'L'),
           coalesce(sum(per.ff), 0)::bigint,
           coalesce(sum(per.fa), 0)::bigint,
           (count(*) FILTER (WHERE per.r = 'W') * ph.points_win
            + count(*) FILTER (WHERE per.r = 'D') * ph.points_draw
            + count(*) FILTER (WHERE per.r = 'L') * ph.points_loss)::bigint
      FROM teams tm
      JOIN ph ON ph.id = tm.pid
      LEFT JOIN per ON per.pid = tm.pid AND per.clan_id = tm.clan_id
     GROUP BY tm.pid, tm.group_name, tm.clan_id, ph.points_win, ph.points_draw, ph.points_loss;
$$;

-- Escritura de torneos: unica via para el rol dday_clans (SECURITY DEFINER), mismo
-- patron que clan_save / clan_delete. p_id NULL crea la ficha. p_phases: arreglo en orden
-- (1 a 10); un elemento con id actualiza esa fase, sin id (null) crea una nueva, y las
-- fases que no vienen se borran. La primera fase siempre queda con starts_at NULL.
-- p_teams: [{clan_id, group}]. Clave repetida: 23505. Clan inexistente: 23503.
CREATE OR REPLACE FUNCTION tournament_save(p_id integer, p_data jsonb, p_phases jsonb,
                                           p_teams jsonb, p_admin text)
RETURNS integer
LANGUAGE plpgsql SECURITY DEFINER SET search_path = public AS $$
DECLARE
    v_id    integer;
    v_old   text;
    v_new   text := p_data->>'event';
    v_keep  integer[];
    v_ph    record;
BEGIN
    IF p_phases IS NULL OR jsonb_typeof(p_phases) <> 'array'
       OR jsonb_array_length(p_phases) NOT BETWEEN 1 AND 10 THEN
        RAISE EXCEPTION 'el torneo necesita de 1 a 10 fases' USING ERRCODE = '22023';
    END IF;
    IF p_id IS NULL THEN
        INSERT INTO tournaments (event, name, status, starts_on, ends_on, format, description,
                                 rules_url, logo_url, champion_id, runner_up_id, visible,
                                 updated_by)
        VALUES (v_new, coalesce(p_data->>'name', ''),
                coalesce(p_data->>'status', 'upcoming'),
                nullif(p_data->>'starts_on', '')::date, nullif(p_data->>'ends_on', '')::date,
                coalesce(p_data->>'format', ''), coalesce(p_data->>'description', ''),
                coalesce(p_data->>'rules_url', ''), coalesce(p_data->>'logo_url', ''),
                nullif(p_data->>'champion_id', '')::integer,
                nullif(p_data->>'runner_up_id', '')::integer,
                coalesce((p_data->>'visible')::boolean, true), p_admin)
        RETURNING id INTO v_id;
    ELSE
        SELECT event INTO v_old FROM tournaments WHERE id = p_id;
        IF NOT FOUND THEN
            RAISE EXCEPTION 'el torneo % no existe', p_id USING ERRCODE = 'no_data_found';
        END IF;
        IF v_new IS DISTINCT FROM v_old AND (
               EXISTS (SELECT 1 FROM matches WHERE event = v_old)
            OR EXISTS (SELECT 1 FROM match_event_changes c
                        WHERE c.applied_at IS NULL AND c.new_event = v_old)) THEN
            RAISE EXCEPTION 'el torneo ya tiene partidas' USING ERRCODE = '55006';
        END IF;
        UPDATE tournaments SET
            event = v_new, name = coalesce(p_data->>'name', ''),
            status = coalesce(p_data->>'status', 'upcoming'),
            starts_on = nullif(p_data->>'starts_on', '')::date,
            ends_on = nullif(p_data->>'ends_on', '')::date,
            format = coalesce(p_data->>'format', ''),
            description = coalesce(p_data->>'description', ''),
            rules_url = coalesce(p_data->>'rules_url', ''),
            logo_url = coalesce(p_data->>'logo_url', ''),
            champion_id = nullif(p_data->>'champion_id', '')::integer,
            runner_up_id = nullif(p_data->>'runner_up_id', '')::integer,
            visible = coalesce((p_data->>'visible')::boolean, true),
            updated_at = now(), updated_by = p_admin
         WHERE id = p_id
        RETURNING id INTO v_id;
        IF v_id IS NULL THEN
            RAISE EXCEPTION 'el torneo % no existe', p_id USING ERRCODE = 'no_data_found';
        END IF;
    END IF;

    -- Fases: todo id que venga debe ser de este torneo.
    SELECT coalesce(array_agg(nullif(x->>'id', '')::integer)
                    FILTER (WHERE nullif(x->>'id', '') IS NOT NULL), '{}')
      INTO v_keep FROM jsonb_array_elements(p_phases) x;
    IF EXISTS (SELECT 1 FROM unnest(v_keep) k
                WHERE NOT EXISTS (SELECT 1 FROM tournament_phases f
                                   WHERE f.id = k AND f.tournament_id = v_id)) THEN
        RAISE EXCEPTION 'una fase no pertenece a este torneo' USING ERRCODE = '22023';
    END IF;
    DELETE FROM tournament_phases f
     WHERE f.tournament_id = v_id AND f.id <> ALL (v_keep);
    FOR v_ph IN
        SELECT x, ord FROM jsonb_array_elements(p_phases) WITH ORDINALITY AS t(x, ord)
    LOOP
        IF nullif(v_ph.x->>'id', '') IS NOT NULL THEN
            UPDATE tournament_phases SET
                name = coalesce(v_ph.x->>'name', ''),
                scoring = coalesce(v_ph.x->>'scoring', 'league'),
                points_per = coalesce(v_ph.x->>'points_per', 'match'),
                points_win = coalesce((v_ph.x->>'points_win')::integer, 3),
                points_draw = coalesce((v_ph.x->>'points_draw')::integer, 1),
                points_loss = coalesce((v_ph.x->>'points_loss')::integer, 0),
                starts_at = CASE WHEN v_ph.ord = 1 THEN NULL
                                 ELSE nullif(v_ph.x->>'starts_at', '')::timestamptz END,
                sort_order = v_ph.ord
             WHERE id = (v_ph.x->>'id')::integer AND tournament_id = v_id;
        ELSE
            INSERT INTO tournament_phases (tournament_id, name, scoring, points_per,
                                           points_win, points_draw, points_loss,
                                           starts_at, sort_order)
            VALUES (v_id, coalesce(v_ph.x->>'name', ''),
                    coalesce(v_ph.x->>'scoring', 'league'),
                    coalesce(v_ph.x->>'points_per', 'match'),
                    coalesce((v_ph.x->>'points_win')::integer, 3),
                    coalesce((v_ph.x->>'points_draw')::integer, 1),
                    coalesce((v_ph.x->>'points_loss')::integer, 0),
                    CASE WHEN v_ph.ord = 1 THEN NULL
                         ELSE nullif(v_ph.x->>'starts_at', '')::timestamptz END,
                    v_ph.ord);
        END IF;
    END LOOP;

    DELETE FROM tournament_teams WHERE tournament_id = v_id;
    INSERT INTO tournament_teams (tournament_id, clan_id, group_name)
    SELECT v_id, (x->>'clan_id')::integer, coalesce(x->>'group', '')
      FROM jsonb_array_elements(coalesce(p_teams, '[]')) x;
    RETURN v_id;
END $$;

-- Borra una ficha (fases e inscritos se van en cascada) y los datos manuales de sus
-- partidas. No borra partidas. true si existia.
CREATE OR REPLACE FUNCTION tournament_delete(p_id integer) RETURNS boolean
LANGUAGE plpgsql SECURITY DEFINER SET search_path = public AS $$
DECLARE
    v_event text;
BEGIN
    SELECT event INTO v_event FROM tournaments WHERE id = p_id;
    IF NOT FOUND THEN
        RETURN false;
    END IF;
    DELETE FROM tournament_match_sides
     WHERE match_id IN (SELECT id FROM matches WHERE event = v_event);
    DELETE FROM tournaments WHERE id = p_id;
    RETURN FOUND;
END $$;

-- Borra las filas de tournament_match_sides que quedaron vacias. Solo la llaman las
-- otras funciones de torneos (sin GRANT a nadie). Devuelve cuantas borro.
CREATE OR REPLACE FUNCTION tournament_side_prune(p_match_ids text[]) RETURNS integer
LANGUAGE plpgsql AS $$
DECLARE
    v_n integer;
BEGIN
    DELETE FROM tournament_match_sides
     WHERE match_id = ANY (p_match_ids)
       AND clan0_id IS NULL AND clan1_id IS NULL AND NOT excluded
       AND phase_id IS NULL AND round = '';
    GET DIAGNOSTICS v_n = ROW_COUNT;
    RETURN v_n;
END $$;

-- Fija el clan de cada lado de una partida de torneo y/o la marca "no cuenta". NULL en un
-- clan = automatico. No toca la fase ni la ronda.
CREATE OR REPLACE FUNCTION tournament_side_save(p_match_id text, p_clan0 integer,
                                                p_clan1 integer, p_excluded boolean,
                                                p_admin text)
RETURNS boolean
LANGUAGE plpgsql SECURITY DEFINER SET search_path = public AS $$
BEGIN
    IF NOT EXISTS (SELECT 1 FROM matches WHERE id = p_match_id AND event <> '') THEN
        RAISE EXCEPTION 'la partida % no existe o no es de un torneo', p_match_id
            USING ERRCODE = 'no_data_found';
    END IF;
    INSERT INTO tournament_match_sides (match_id, clan0_id, clan1_id, excluded, updated_by)
    VALUES (p_match_id, p_clan0, p_clan1, coalesce(p_excluded, false), p_admin)
    ON CONFLICT (match_id) DO UPDATE SET
        clan0_id = EXCLUDED.clan0_id, clan1_id = EXCLUDED.clan1_id,
        excluded = EXCLUDED.excluded, updated_at = now(), updated_by = EXCLUDED.updated_by;
    RETURN true;
END $$;

-- Vuelve a lo automatico los clanes y el "no cuenta" de una partida, sin perder su fase ni
-- su ronda. true si habia fila.
CREATE OR REPLACE FUNCTION tournament_side_clear(p_match_id text) RETURNS boolean
LANGUAGE plpgsql SECURITY DEFINER SET search_path = public AS $$
DECLARE
    v_found boolean;
BEGIN
    UPDATE tournament_match_sides
       SET clan0_id = NULL, clan1_id = NULL, excluded = false, updated_at = now()
     WHERE match_id = p_match_id;
    v_found := FOUND;
    PERFORM tournament_side_prune(ARRAY[p_match_id]);
    RETURN v_found;
END $$;

-- Pone fase (NULL = automatica por fecha) y ronda ('' = sin ronda) a varias partidas (las
-- de un cruce). Todas deben existir y ser del mismo torneo. No toca clanes ni "no cuenta".
-- Devuelve cuantas partidas actualizo.
CREATE OR REPLACE FUNCTION tournament_assign_save(p_match_ids text[], p_phase_id integer,
                                                  p_round text, p_admin text)
RETURNS integer
LANGUAGE plpgsql SECURITY DEFINER SET search_path = public AS $$
DECLARE
    v_ids    text[];
    v_event  text;
    v_events integer;
    v_found  integer;
    v_round  text := trim(coalesce(p_round, ''));
    v_n      integer;
BEGIN
    SELECT coalesce(array_agg(DISTINCT i), '{}') INTO v_ids FROM unnest(p_match_ids) i;
    IF cardinality(v_ids) = 0 THEN
        RAISE EXCEPTION 'faltan partidas' USING ERRCODE = 'no_data_found';
    END IF;
    SELECT count(*), count(DISTINCT m.event), min(m.event)
      INTO v_found, v_events, v_event
      FROM matches m WHERE m.id = ANY (v_ids);
    IF v_found <> cardinality(v_ids) OR v_events <> 1 OR v_event = '' THEN
        RAISE EXCEPTION 'las partidas deben existir y ser del mismo torneo'
            USING ERRCODE = 'no_data_found';
    END IF;
    IF p_phase_id IS NOT NULL AND NOT EXISTS (
           SELECT 1 FROM tournament_phases f JOIN tournaments t ON t.id = f.tournament_id
            WHERE f.id = p_phase_id AND t.event = v_event) THEN
        RAISE EXCEPTION 'la fase no es de este torneo' USING ERRCODE = '22023';
    END IF;
    INSERT INTO tournament_match_sides (match_id, phase_id, round, updated_by)
    SELECT i, p_phase_id, v_round, p_admin FROM unnest(v_ids) i
    ON CONFLICT (match_id) DO UPDATE SET
        phase_id = EXCLUDED.phase_id, round = EXCLUDED.round,
        updated_at = now(), updated_by = EXCLUDED.updated_by;
    GET DIAGNOSTICS v_n = ROW_COUNT;
    PERFORM tournament_side_prune(v_ids);
    RETURN v_n;
END $$;

-- Pedido de cambio de torneo (D23) para 1 a 100 partidas: p_event '' = sacar del torneo, o
-- el event de una ficha existente. No cambia matches: lo aplica el ingestor
-- (apply_event_changes). Devuelve cuantos pedidos creo; las partidas que ya son de p_event
-- no crean pedido (y cancelan uno pendiente que hubiera).
CREATE OR REPLACE FUNCTION tournament_event_request(p_match_ids text[], p_event text,
                                                    p_admin text)
RETURNS integer
LANGUAGE plpgsql SECURITY DEFINER SET search_path = public AS $$
DECLARE
    v_event text := trim(coalesce(p_event, ''));
    v_ids   text[];
    v_n     integer;
BEGIN
    IF v_event <> '' AND NOT EXISTS (SELECT 1 FROM tournaments WHERE event = v_event) THEN
        RAISE EXCEPTION 'el torneo no tiene ficha' USING ERRCODE = '22023';
    END IF;
    SELECT coalesce(array_agg(DISTINCT i), '{}') INTO v_ids FROM unnest(p_match_ids) i;
    IF cardinality(v_ids) NOT BETWEEN 1 AND 100 THEN
        RAISE EXCEPTION 'se piden de 1 a 100 partidas' USING ERRCODE = '22023';
    END IF;
    IF (SELECT count(*) FROM matches WHERE id = ANY (v_ids)) <> cardinality(v_ids) THEN
        RAISE EXCEPTION 'alguna partida no existe' USING ERRCODE = 'no_data_found';
    END IF;
    DELETE FROM match_event_changes c
     WHERE c.applied_at IS NULL AND c.match_id = ANY (v_ids);
    INSERT INTO match_event_changes (match_id, new_event, requested_by)
    SELECT m.id, v_event, p_admin FROM matches m
     WHERE m.id = ANY (v_ids) AND m.event <> v_event;
    GET DIAGNOSTICS v_n = ROW_COUNT;
    RETURN v_n;
END $$;

-- Aplica los pedidos pendientes de cambio de torneo (el mas reciente de cada partida):
-- cambia matches.event, borra los datos manuales de las partidas que cambiaron y marca los
-- pedidos como aplicados. Devuelve cuantas partidas cambiaron. Sin SECURITY DEFINER y sin
-- GRANT: la ejecuta el ingestor (dueno), que despues recalcula resumenes y Elo.
CREATE OR REPLACE FUNCTION apply_event_changes() RETURNS integer
LANGUAGE plpgsql AS $$
DECLARE
    v_n integer;
BEGIN
    WITH pend AS (
        SELECT DISTINCT ON (match_id) match_id, new_event
          FROM match_event_changes
         WHERE applied_at IS NULL
         ORDER BY match_id, requested_at DESC, id DESC
    ), chg AS (
        UPDATE matches m SET event = p.new_event
          FROM pend p
         WHERE m.id = p.match_id AND m.event <> p.new_event
        RETURNING m.id
    ), del AS (
        DELETE FROM tournament_match_sides WHERE match_id IN (SELECT id FROM chg)
    ), mark AS (
        UPDATE match_event_changes SET applied_at = now()
         WHERE applied_at IS NULL AND match_id IN (SELECT match_id FROM pend)
    )
    SELECT count(*) INTO v_n FROM chg;
    RETURN v_n;
END $$;

-- Permisos: igual que clanes. tournament_side_prune y apply_event_changes no se dan a nadie.
REVOKE ALL ON FUNCTION tournament_save(integer, jsonb, jsonb, jsonb, text) FROM PUBLIC;
REVOKE ALL ON FUNCTION tournament_delete(integer) FROM PUBLIC;
REVOKE ALL ON FUNCTION tournament_side_save(text, integer, integer, boolean, text) FROM PUBLIC;
REVOKE ALL ON FUNCTION tournament_side_clear(text) FROM PUBLIC;
REVOKE ALL ON FUNCTION tournament_assign_save(text[], integer, text, text) FROM PUBLIC;
REVOKE ALL ON FUNCTION tournament_side_prune(text[]) FROM PUBLIC;
REVOKE ALL ON FUNCTION tournament_event_request(text[], text, text) FROM PUBLIC;
REVOKE ALL ON FUNCTION apply_event_changes() FROM PUBLIC;
DO $$
BEGIN
    IF EXISTS (SELECT 1 FROM pg_roles WHERE rolname = 'dday_clans') THEN
        GRANT EXECUTE ON FUNCTION tournament_save(integer, jsonb, jsonb, jsonb, text) TO dday_clans;
        GRANT EXECUTE ON FUNCTION tournament_delete(integer) TO dday_clans;
        GRANT EXECUTE ON FUNCTION tournament_side_save(text, integer, integer, boolean, text) TO dday_clans;
        GRANT EXECUTE ON FUNCTION tournament_side_clear(text) TO dday_clans;
        GRANT EXECUTE ON FUNCTION tournament_assign_save(text[], integer, text, text) TO dday_clans;
        GRANT EXECUTE ON FUNCTION tournament_event_request(text[], text, text) TO dday_clans;
    END IF;
END $$;

------------------------------------------------------------------------------
-- Comentarios del esquema (se ven en psql con \d+ y en clientes como
-- Beekeeper/DBeaver/pgAdmin). Idempotente: se pueden reaplicar.
------------------------------------------------------------------------------

COMMENT ON TABLE players IS 'Jugador o bot. La identidad es el nombre (sin colores).';
COMMENT ON COLUMN players.id IS 'Clave interna del jugador.';
COMMENT ON COLUMN players.name IS 'Nombre del jugador (sin colores ni sufijos).';
COMMENT ON COLUMN players.is_bot IS 'true si es un bot.';
COMMENT ON COLUMN players.first_seen IS 'Primera vez visto.';
COMMENT ON COLUMN players.last_seen IS 'Ultima vez visto.';

COMMENT ON TABLE seasons IS 'Temporadas del ladder (se crean con "ingest.py --new-season NOMBRE").';
COMMENT ON COLUMN seasons.id IS 'Clave interna de la temporada.';
COMMENT ON COLUMN seasons.name IS 'Nombre de la temporada (unico).';
COMMENT ON COLUMN seasons.starts_at IS 'Cuando empieza; dura hasta que empieza la siguiente.';

COMMENT ON TABLE matches IS 'Una partida (un mapa jugado en un servidor).';
COMMENT ON COLUMN matches.id IS 'Id de partida: <fecha>-<servidor>-<random>. Clave primaria.';
COMMENT ON COLUMN matches.server IS 'Id del servidor (cvar stats_server).';
COMMENT ON COLUMN matches.map IS 'Mapa jugado.';
COMMENT ON COLUMN matches.mode IS 'Modo del juego: dm, ctb, campaign, control, ffa (o el que mande la DLL).';
COMMENT ON COLUMN matches.kind IS 'Tipo de partida: public o duel (cvar stats_mode).';
COMMENT ON COLUMN matches.event IS 'Torneo (cvar stats_event); cadena vacia = casual.';
COMMENT ON COLUMN matches.season_id IS 'Temporada a la que pertenece la partida.';
COMMENT ON COLUMN matches.tournament IS 'true si el servidor estaba en modo torneo.';
COMMENT ON COLUMN matches.started_at IS 'Inicio de la partida.';
COMMENT ON COLUMN matches.ended_at IS 'Fin de la partida (NULL si no se cerro).';
COMMENT ON COLUMN matches.duration_s IS 'Duracion de la partida en segundos.';
COMMENT ON COLUMN matches.winner IS 'Ganador: 0 aliados, 1 eje, -1 empate, NULL sin ganador.';
COMMENT ON COLUMN matches.end_reason IS 'Como termino: normal, forced, other o aborted.';
COMMENT ON COLUMN matches.team0_army IS 'Ejercito del equipo 0 (aliados).';
COMMENT ON COLUMN matches.team1_army IS 'Ejercito del equipo 1 (eje).';
COMMENT ON COLUMN matches.team0_score IS 'Puntaje final del equipo 0.';
COMMENT ON COLUMN matches.team1_score IS 'Puntaje final del equipo 1.';
COMMENT ON COLUMN matches.team0_kills IS 'Kills del equipo 0.';
COMMENT ON COLUMN matches.team1_kills IS 'Kills del equipo 1.';
COMMENT ON COLUMN matches.humans IS 'Cantidad de jugadores humanos en la partida.';
COMMENT ON COLUMN matches.ranked IS 'true si la partida es rankeada. Modos por equipos: cuenta para el Elo. FFA: solo marca la partida para ladder_ffa (final normal, 5 minutos y 3 humanos que terminaron); el FFA no mueve el Elo.';
COMMENT ON COLUMN matches.ingested_at IS 'Cuando se cargo en la base.';

COMMENT ON TABLE match_players IS 'Resumen de un jugador en una partida (suma de sus tramos/reconexiones).';
COMMENT ON COLUMN match_players.match_id IS 'Partida.';
COMMENT ON COLUMN match_players.player_id IS 'Jugador.';
COMMENT ON COLUMN match_players.team IS 'Equipo donde jugo mas tiempo: 0 aliados, 1 eje, -1 observador.';
COMMENT ON COLUMN match_players.time_team0 IS 'Segundos jugados en el equipo 0.';
COMMENT ON COLUMN match_players.time_team1 IS 'Segundos jugados en el equipo 1.';
COMMENT ON COLUMN match_players.kills IS 'Kills a enemigos (humanos o bots).';
COMMENT ON COLUMN match_players.deaths IS 'Todas las muertes.';
COMMENT ON COLUMN match_players.human_kills IS 'Kills a enemigos humanos (lo que cuenta el ladder).';
COMMENT ON COLUMN match_players.human_deaths IS 'Muertes por humanos, suicidio o entorno (lo que cuenta el ladder).';
COMMENT ON COLUMN match_players.suicides IS 'Suicidios.';
COMMENT ON COLUMN match_players.teamkills IS 'Fuego amigo.';
COMMENT ON COLUMN match_players.headshots IS 'Kills con tiro a la cabeza.';
COMMENT ON COLUMN match_players.objectives IS 'Objetivos completados (banderas, maletin, etc.).';
COMMENT ON COLUMN match_players.best_streak IS 'Mejor racha de kills en la partida.';
COMMENT ON COLUMN match_players.helmet_saves IS 'El casco le desvio un tiro a la cabeza.';
COMMENT ON COLUMN match_players.foot_saves IS 'Sobrevivio con "almost lost a foot".';
COMMENT ON COLUMN match_players.deflected IS 'Tiros suyos desviados por el casco de otro.';
COMMENT ON COLUMN match_players.hits IS 'Disparos acertados.';
COMMENT ON COLUMN match_players.misses IS 'Disparos fallados.';
COMMENT ON COLUMN match_players.score IS 'Puntaje del juego.';
COMMENT ON COLUMN match_players.points IS 'Puntos del juego.';
COMMENT ON COLUMN match_players.classes IS 'Segundos jugados por clase (JSON: {"clase": segundos}).';
COMMENT ON COLUMN match_players.main_class IS 'Clase en la que jugo mas tiempo.';
COMMENT ON COLUMN match_players.result IS 'Resultado: W (gano), L (perdio), D (empate) o NULL (sin ganador).';
COMMENT ON COLUMN match_players.zone_kills IS 'Modo control: kills a enemigos en que el que mato o el que murio estaba en la zona (defender o limpiar).';
COMMENT ON COLUMN match_players.place IS 'FFA: puesto al terminar por score (los empatados comparten puesto); NULL en otros modos o si se fue antes.';
COMMENT ON COLUMN match_players.sprees IS 'Veces que alcanzo KILLING SPREE.';
COMMENT ON COLUMN match_players.rampages IS 'Veces que alcanzo RAMPAGE.';
COMMENT ON COLUMN match_players.dominatings IS 'Veces que alcanzo DOMINATING.';
COMMENT ON COLUMN match_players.unstoppables IS 'Veces que alcanzo UNSTOPPABLE.';
COMMENT ON COLUMN match_players.godlikes IS 'Veces que alcanzo GODLIKE.';
COMMENT ON COLUMN match_players.streaks_ended IS 'Rachas ajenas (>= base) que corto matando.';
COMMENT ON COLUMN match_players.zone_seconds IS 'Modo control: segundos que paso dentro de la zona (vivo y con la zona abierta).';

COMMENT ON TABLE kills IS 'Una kill (o suicidio/muerte por entorno). Tabla de detalle, la mas grande.';
COMMENT ON COLUMN kills.id IS 'Clave interna.';
COMMENT ON COLUMN kills.match_id IS 'Partida.';
COMMENT ON COLUMN kills.t IS 'Segundos desde el inicio del mapa.';
COMMENT ON COLUMN kills.killer_id IS 'Quien mato; NULL en suicidio o muerte por entorno.';
COMMENT ON COLUMN kills.victim_id IS 'Quien murio.';
COMMENT ON COLUMN kills.killer_team IS 'Equipo del killer (0 aliados, 1 eje).';
COMMENT ON COLUMN kills.victim_team IS 'Equipo de la victima.';
COMMENT ON COLUMN kills.killer_class IS 'Clase del killer.';
COMMENT ON COLUMN kills.victim_class IS 'Clase de la victima.';
COMMENT ON COLUMN kills.mod IS 'Causa de la muerte: rifle, sniper, grenade, wound, ...';
COMMENT ON COLUMN kills.weapon IS 'Arma en mano (solo armas de fuego o cuchillo).';
COMMENT ON COLUMN kills.headshot IS 'true si fue tiro a la cabeza.';
COMMENT ON COLUMN kills.friendly_fire IS 'true si fue fuego amigo.';
COMMENT ON COLUMN kills.suicide IS 'true si fue suicidio.';
COMMENT ON COLUMN kills.distance IS 'Distancia killer-victima en unidades de Quake 2 (~3 cm).';
COMMENT ON COLUMN kills.killer_x IS 'Posicion X del killer (unidades de Quake 2).';
COMMENT ON COLUMN kills.killer_y IS 'Posicion Y del killer.';
COMMENT ON COLUMN kills.killer_z IS 'Posicion Z del killer (altura).';
COMMENT ON COLUMN kills.victim_x IS 'Posicion X de la victima.';
COMMENT ON COLUMN kills.victim_y IS 'Posicion Y de la victima.';
COMMENT ON COLUMN kills.victim_z IS 'Posicion Z de la victima.';
COMMENT ON COLUMN kills.killer_in_zone IS 'Modo control: true si el que mato estaba dentro de la zona; NULL fuera del modo control (o en partidas anteriores a este dato).';
COMMENT ON COLUMN kills.victim_in_zone IS 'Modo control: true si el que murio estaba dentro de la zona; NULL fuera del modo control.';

COMMENT ON TABLE objectives IS 'Eventos de objetivo de una partida (banderas, maletin, zonas, etc.).';
COMMENT ON COLUMN objectives.id IS 'Clave interna.';
COMMENT ON COLUMN objectives.match_id IS 'Partida.';
COMMENT ON COLUMN objectives.t IS 'Segundos desde el inicio del mapa.';
COMMENT ON COLUMN objectives.type IS 'Tipo: touch, area, timed, timed_held, explosive, bc_pickup/bc_drop/bc_capture, zone_capture (modo control: un evento por jugador del equipo que estaba en la zona).';
COMMENT ON COLUMN objectives.name IS 'Nombre del objetivo.';
COMMENT ON COLUMN objectives.team IS 'Equipo que lo completo.';
COMMENT ON COLUMN objectives.player_id IS 'Jugador que lo hizo (NULL si no aplica).';

COMMENT ON TABLE ratings IS 'Elo por equipos, por jugador, categoria (public/duel/official) y temporada (todos los modos juntos).';
COMMENT ON COLUMN ratings.player_id IS 'Jugador.';
COMMENT ON COLUMN ratings.kind IS 'Categoria: public, duel u official.';
COMMENT ON COLUMN ratings.season_id IS 'Temporada; 0 = historico (todas).';
COMMENT ON COLUMN ratings.rating IS 'Elo actual (arranca en 1500).';
COMMENT ON COLUMN ratings.peak IS 'Elo maximo alcanzado.';
COMMENT ON COLUMN ratings.games IS 'Partidas rankeadas jugadas.';
COMMENT ON COLUMN ratings.wins IS 'Partidas ganadas.';
COMMENT ON COLUMN ratings.losses IS 'Partidas perdidas.';
COMMENT ON COLUMN ratings.draws IS 'Partidas empatadas.';
COMMENT ON COLUMN ratings.updated_at IS 'Ultima actualizacion.';

COMMENT ON TABLE rating_history IS 'Elo antes/despues de cada partida (para graficos de evolucion).';
COMMENT ON COLUMN rating_history.match_id IS 'Partida.';
COMMENT ON COLUMN rating_history.player_id IS 'Jugador.';
COMMENT ON COLUMN rating_history.season_id IS 'Temporada; 0 = historico.';
COMMENT ON COLUMN rating_history.rating_before IS 'Elo antes de la partida.';
COMMENT ON COLUMN rating_history.rating_after IS 'Elo despues de la partida.';

COMMENT ON TABLE player_stats IS 'Resumen por jugador, temporada, categoria, modo de juego y torneo (lo mantiene apply_rollups).';
COMMENT ON COLUMN player_stats.player_id IS 'Jugador.';
COMMENT ON COLUMN player_stats.season_id IS 'Temporada.';
COMMENT ON COLUMN player_stats.kind IS 'Categoria: public, duel u official.';
COMMENT ON COLUMN player_stats.mode IS 'Modo de juego de las partidas: dm, ctb, campaign, control...';
COMMENT ON COLUMN player_stats.event IS 'Torneo; cadena vacia = casual.';
COMMENT ON COLUMN player_stats.matches IS 'Partidas jugadas.';
COMMENT ON COLUMN player_stats.wins IS 'Partidas ganadas.';
COMMENT ON COLUMN player_stats.losses IS 'Partidas perdidas.';
COMMENT ON COLUMN player_stats.draws IS 'Partidas empatadas.';
COMMENT ON COLUMN player_stats.kills IS 'Kills a enemigos (humanos o bots).';
COMMENT ON COLUMN player_stats.deaths IS 'Todas las muertes.';
COMMENT ON COLUMN player_stats.human_kills IS 'Kills a humanos (lo que cuenta el ladder).';
COMMENT ON COLUMN player_stats.human_deaths IS 'Muertes por humanos/suicidio/entorno.';
COMMENT ON COLUMN player_stats.headshots IS 'Kills con tiro a la cabeza.';
COMMENT ON COLUMN player_stats.hits IS 'Disparos acertados.';
COMMENT ON COLUMN player_stats.misses IS 'Disparos fallados.';
COMMENT ON COLUMN player_stats.teamkills IS 'Fuego amigo.';
COMMENT ON COLUMN player_stats.suicides IS 'Suicidios.';
COMMENT ON COLUMN player_stats.objectives IS 'Objetivos completados.';
COMMENT ON COLUMN player_stats.seconds IS 'Segundos jugados.';
COMMENT ON COLUMN player_stats.best_streak IS 'Mejor racha de kills.';
COMMENT ON COLUMN player_stats.max_kills IS 'Mas kills en una sola partida.';
COMMENT ON COLUMN player_stats.helmet_saves IS 'Cascos que desviaron un tiro a la cabeza.';
COMMENT ON COLUMN player_stats.foot_saves IS 'Veces que sobrevivio con el pie herido.';
COMMENT ON COLUMN player_stats.deflected IS 'Tiros desviados por cascos ajenos.';
COMMENT ON COLUMN player_stats.sprees IS 'Veces que alcanzo KILLING SPREE.';
COMMENT ON COLUMN player_stats.rampages IS 'Veces que alcanzo RAMPAGE.';
COMMENT ON COLUMN player_stats.dominatings IS 'Veces que alcanzo DOMINATING.';
COMMENT ON COLUMN player_stats.unstoppables IS 'Veces que alcanzo UNSTOPPABLE.';
COMMENT ON COLUMN player_stats.godlikes IS 'Veces que alcanzo GODLIKE.';
COMMENT ON COLUMN player_stats.streaks_ended IS 'Rachas ajenas que corto.';
COMMENT ON COLUMN player_stats.longest_kill IS 'Kill mas larga (distancia en unidades de Quake 2).';
COMMENT ON COLUMN player_stats.last_match IS 'Fecha de la ultima partida.';
COMMENT ON COLUMN player_stats.zone_seconds IS 'Modo control: segundos dentro de la zona.';
COMMENT ON COLUMN player_stats.zone_captures IS 'Modo control: capturas de la zona en las que participo (estaba dentro).';
COMMENT ON COLUMN player_stats.zone_kills IS 'Modo control: kills en la zona (el que mato o el que murio estaba dentro).';
COMMENT ON COLUMN player_stats.ffa_podiums IS 'FFA: veces que termino en el top 3.';
COMMENT ON COLUMN player_stats.ffa_placed IS 'FFA: partidas que termino con puesto (no se fue antes).';
COMMENT ON COLUMN player_stats.ffa_place_sum IS 'FFA: suma de los puestos; el puesto promedio es ffa_place_sum / ffa_placed.';

COMMENT ON TABLE player_map_stats IS 'Resumen por jugador y mapa (todas las temporadas).';
COMMENT ON COLUMN player_map_stats.player_id IS 'Jugador.';
COMMENT ON COLUMN player_map_stats.map IS 'Mapa.';
COMMENT ON COLUMN player_map_stats.kind IS 'Categoria: public o duel.';
COMMENT ON COLUMN player_map_stats.matches IS 'Partidas jugadas en ese mapa.';
COMMENT ON COLUMN player_map_stats.wins IS 'Partidas ganadas.';
COMMENT ON COLUMN player_map_stats.kills IS 'Kills.';
COMMENT ON COLUMN player_map_stats.deaths IS 'Muertes.';
COMMENT ON COLUMN player_map_stats.seconds IS 'Segundos jugados.';
COMMENT ON COLUMN player_map_stats.best_streak IS 'Mejor racha en ese mapa.';

COMMENT ON TABLE player_class_stats IS 'Segundos jugados por clase y jugador.';
COMMENT ON COLUMN player_class_stats.player_id IS 'Jugador.';
COMMENT ON COLUMN player_class_stats.class IS 'Clase (sniper, medic, engineer, ...).';
COMMENT ON COLUMN player_class_stats.seconds IS 'Segundos jugados con esa clase.';

COMMENT ON TABLE player_weapon_stats IS 'Kills por arma y jugador (arma en mano o, si no aplica, la causa).';
COMMENT ON COLUMN player_weapon_stats.player_id IS 'Jugador.';
COMMENT ON COLUMN player_weapon_stats.weapon IS 'Arma o causa (rifle, sniper, grenade, ...).';
COMMENT ON COLUMN player_weapon_stats.kills IS 'Kills con esa arma.';
COMMENT ON COLUMN player_weapon_stats.headshots IS 'Kills a la cabeza con esa arma.';
COMMENT ON COLUMN player_weapon_stats.distance_sum IS 'Suma de distancias (para el promedio).';
COMMENT ON COLUMN player_weapon_stats.distance_n IS 'Cantidad de kills con distancia conocida.';

COMMENT ON TABLE player_duel_stats IS 'Kills entre humanos (nemesis / victima favorita).';
COMMENT ON COLUMN player_duel_stats.killer_id IS 'Quien mata.';
COMMENT ON COLUMN player_duel_stats.victim_id IS 'A quien mata.';
COMMENT ON COLUMN player_duel_stats.kills IS 'Veces que lo mato.';

COMMENT ON TABLE map_stats IS 'Resumen por mapa, categoria y modo de juego (balance, duracion, letalidad).';
COMMENT ON COLUMN map_stats.map IS 'Mapa.';
COMMENT ON COLUMN map_stats.kind IS 'Categoria: public o duel.';
COMMENT ON COLUMN map_stats.mode IS 'Modo de juego: dm, ctb, campaign, control...';
COMMENT ON COLUMN map_stats.matches IS 'Partidas jugadas.';
COMMENT ON COLUMN map_stats.allied_wins IS 'Victorias de aliados.';
COMMENT ON COLUMN map_stats.axis_wins IS 'Victorias del eje.';
COMMENT ON COLUMN map_stats.draws IS 'Empates.';
COMMENT ON COLUMN map_stats.seconds IS 'Segundos totales jugados.';
COMMENT ON COLUMN map_stats.humans IS 'Suma de humanos por partida.';
COMMENT ON COLUMN map_stats.kills IS 'Kills totales.';
COMMENT ON COLUMN map_stats.best_streak IS 'Mejor racha en ese mapa.';
COMMENT ON COLUMN map_stats.last_played IS 'Ultima vez jugado.';

COMMENT ON TABLE map_weapon_stats IS 'Kills por mapa, categoria y arma.';
COMMENT ON COLUMN map_weapon_stats.map IS 'Mapa.';
COMMENT ON COLUMN map_weapon_stats.kind IS 'Categoria: public o duel.';
COMMENT ON COLUMN map_weapon_stats.weapon IS 'Arma o causa.';
COMMENT ON COLUMN map_weapon_stats.kills IS 'Kills.';
COMMENT ON COLUMN map_weapon_stats.headshots IS 'Kills a la cabeza.';
COMMENT ON COLUMN map_weapon_stats.distance_sum IS 'Suma de distancias.';
COMMENT ON COLUMN map_weapon_stats.distance_n IS 'Cantidad de kills con distancia conocida.';
COMMENT ON COLUMN map_weapon_stats.max_distance IS 'Kill mas larga con esa arma.';

COMMENT ON TABLE daily_stats IS 'Actividad por dia (hora de Chile): partidas y jugadores nuevos.';
COMMENT ON COLUMN daily_stats.day IS 'Dia.';
COMMENT ON COLUMN daily_stats.matches IS 'Partidas jugadas ese dia.';
COMMENT ON COLUMN daily_stats.players IS 'Jugadores humanos distintos ese dia.';

COMMENT ON TABLE daily_players IS 'Que jugador jugo cada dia (para no contarlo dos veces en daily_stats.players).';
COMMENT ON COLUMN daily_players.day IS 'Dia.';
COMMENT ON COLUMN daily_players.player_id IS 'Jugador.';

COMMENT ON VIEW v_player_totals IS 'Totales historicos por jugador (una fila por jugador).';
COMMENT ON VIEW v_player_totals_by_kind IS 'Totales historicos por jugador y categoria (public/duel/official).';
COMMENT ON VIEW v_player_totals_by_mode IS 'Totales historicos por jugador y modo de juego (dm, ctb, control...).';
COMMENT ON VIEW v_player_profile IS 'Perfil completo de un jugador (totales + Elo + favoritos + nemesis).';
COMMENT ON VIEW v_player_maps IS 'Estadisticas de un jugador por mapa.';
COMMENT ON VIEW v_player_fav_map IS 'Mapa favorito de cada jugador (el mas jugado).';
COMMENT ON VIEW v_player_weapons IS 'Kills de un jugador por arma.';
COMMENT ON VIEW v_player_fav_weapon IS 'Arma favorita de cada jugador.';
COMMENT ON VIEW v_player_classes IS 'Segundos por clase y jugador.';
COMMENT ON VIEW v_player_fav_class IS 'Clase favorita de cada jugador.';
COMMENT ON VIEW v_player_duels IS 'Kills entre pares de jugadores.';
COMMENT ON VIEW v_player_nemesis IS 'Quien mas mata a cada jugador (su nemesis).';
COMMENT ON VIEW v_player_favorite_victim IS 'A quien mas mata cada jugador (su victima favorita).';
COMMENT ON VIEW v_map_stats IS 'Estadisticas por mapa: balance aliados/eje, duracion, kills (todos los modos).';
COMMENT ON VIEW v_map_stats_by_mode IS 'Estadisticas por mapa, tipo y modo de juego.';
COMMENT ON VIEW v_map_weapons IS 'Armas mas letales por mapa.';
COMMENT ON VIEW v_weapon_stats IS 'Armas globales: kills, headshots y distancia media.';
COMMENT ON VIEW v_daily_activity IS 'Actividad diaria (partidas y jugadores por dia).';
COMMENT ON VIEW v_seasons IS 'Temporadas con su fecha de fin y cantidad de partidas.';
COMMENT ON VIEW v_events IS 'Torneos jugados (partidas y mapas).';

COMMENT ON FUNCTION match_category(text, text) IS 'Categoria de ladder de una partida: official si tiene torneo, si no el modo (public/duel).';
COMMENT ON FUNCTION season_lookup(text) IS 'Resuelve una temporada: current (la vigente), all (NULL) o el nombre.';
COMMENT ON FUNCTION streak_base() IS 'Kills seguidas para el primer anuncio de racha (igual al cvar exbattleinfo).';
COMMENT ON FUNCTION apply_rollups(text[]) IS 'Suma las partidas indicadas a las tablas de resumen (una sola vez por partida).';
COMMENT ON FUNCTION rebuild_rollups() IS 'Recalcula todas las tablas de resumen desde las tablas de detalle.';
COMMENT ON FUNCTION player_totals(text, text, text, text) IS 'Totales por jugador para una temporada, categoria, torneo y modo (base de los ladders).';
COMMENT ON FUNCTION ladder_kd(integer, text, text, text, text) IS 'Ladder por K/D (humano vs humano), opcionalmente de un modo.';
COMMENT ON FUNCTION ladder_elo(integer, text, text) IS 'Ladder por rating Elo, por categoria.';
COMMENT ON FUNCTION ladder_streak(text, integer, text, text) IS 'Mejores rachas de kills (una fila por partida), opcionalmente de un modo.';
COMMENT ON FUNCTION ladder_luck(integer, text, text, text) IS 'Los mas suertudos (casco que desvia + pie salvado).';
COMMENT ON FUNCTION ladder_unlucky(integer, text, text, text) IS 'Los tiradores mas desafortunados (tiros desviados por cascos).';
COMMENT ON FUNCTION ladder_streak_tiers(integer, text, text, text) IS 'Rachas por nivel: quien mas llego a GODLIKE, UNSTOPPABLE, etc.';
COMMENT ON FUNCTION ladder_zone(integer, text, text) IS 'Modo control: capturas, minutos y kills en la zona por jugador.';
COMMENT ON FUNCTION ladder_ffa(integer, text, text) IS 'Free For All: victorias, podios, puesto promedio, K/D y mejor racha por jugador (sin Elo). Partidas y % de victorias cuentan solo las FFA terminadas (con puesto); K/D y racha, todas.';

COMMENT ON TABLE clans IS 'Clanes del Ladder Test (se editan desde el panel de administrador; escritura solo por clan_save / clan_delete).';
COMMENT ON COLUMN clans.id IS 'Clave interna del clan.';
COMMENT ON COLUMN clans.name IS 'Nombre del clan (puede estar vacio).';
COMMENT ON COLUMN clans.leader IS 'Lider del clan.';
COMMENT ON COLUMN clans.declared_members IS 'Cantidad de miembros declarada (0 a 999); NULL si no se sabe.';
COMMENT ON COLUMN clans.favorite_map IS 'Mapa favorito declarado.';
COMMENT ON COLUMN clans.symbol_player IS 'Jugador simbolo del clan.';
COMMENT ON COLUMN clans.phrase IS 'Frase tipica del clan.';
COMMENT ON COLUMN clans.rival IS 'Rival historico declarado.';
COMMENT ON COLUMN clans.logo_url IS 'URL https del logo (puede ser un enlace de Drive); vacio = sin logo.';
COMMENT ON COLUMN clans.unregistered IS 'true para el clan No Inscritos: va al final de las listas, fuera de los podios y nunca cuenta para duelos de clanes.';
COMMENT ON COLUMN clans.active IS 'Solo los clanes activos reclaman nicks (v_player_clan).';
COMMENT ON COLUMN clans.sort_order IS 'Orden manual; desempata cuando dos clanes calzan con un tag del mismo largo.';
COMMENT ON COLUMN clans.created_at IS 'Cuando se creo el clan.';
COMMENT ON COLUMN clans.updated_at IS 'Ultima edicion.';
COMMENT ON COLUMN clans.updated_by IS 'Quien hizo la ultima edicion (administrador, o "carga inicial").';

COMMENT ON TABLE clan_tags IS 'Tags de cada clan: uno principal y 0 o mas alternativos, todos cuentan para la pertenencia (regla en clan_tag_matches).';
COMMENT ON COLUMN clan_tags.clan_id IS 'Clan al que pertenece el tag (se borra en cascada con el clan).';
COMMENT ON COLUMN clan_tags.tag IS 'Tag, de 1 a 24 caracteres sin espacios; unico sin distinguir mayusculas.';
COMMENT ON COLUMN clan_tags.is_primary IS 'true en el tag principal (uno por clan); es el que se muestra.';

COMMENT ON VIEW v_clans IS 'Clanes con su tag principal (tag) y la lista de tags alternativos (aliases).';
COMMENT ON VIEW v_player_clan IS 'Clan de cada nick humano segun clan_tag_matches (solo clanes activos; gana el tag mas largo). clan_id NULL = sin clan.';

COMMENT ON FUNCTION clan_tag_matches(text, text) IS 'Regla de pertenencia: un tag solo de letras y numeros debe estar al comienzo del nick y seguido de un caracter que no sea letra ni numero; otros tags basta con que el nick los contenga. Sin distinguir mayusculas.';
COMMENT ON FUNCTION ladder_clans(text, text) IS 'Ranking de clanes: suma de sus miembros activos en la temporada y categoria (K/D sin muestra con menos de 20 muertes, Elo promedio, MVPs).';
COMMENT ON FUNCTION clan_duels(text, text, integer) IS 'Duelos de clanes: partidas por equipos sin bots donde cada equipo esta formado solo por integrantes de un mismo clan (distintos entre si, ninguno sin inscribir), con un minimo por lado.';
COMMENT ON FUNCTION clan_preview(integer, text[]) IS 'Vista previa del mantenedor: nicks humanos que quedarian en un clan (o en uno nuevo si el id es NULL) con los tags indicados, y el clan del que vendrian.';
COMMENT ON FUNCTION clan_save(integer, text[], jsonb, text) IS 'Crea (id NULL) o edita un clan y reemplaza sus tags (el primero es el principal). SECURITY DEFINER: es la unica via de escritura para dday_clans. Un tag repetido da el error 23505.';
COMMENT ON FUNCTION clan_delete(integer) IS 'Borra un clan y sus tags; true si existia. SECURITY DEFINER.';

COMMENT ON TABLE tournaments IS 'Fichas de torneo del Ladder Test (se editan desde el panel de administrador; escritura solo por tournament_save / tournament_delete). La clave es el valor de stats_event (matches.event).';
COMMENT ON COLUMN tournaments.id IS 'Clave interna de la ficha.';
COMMENT ON COLUMN tournaments.event IS 'Clave del torneo = cvar stats_event = matches.event (unica, 1 a 64 caracteres).';
COMMENT ON COLUMN tournaments.name IS 'Nombre para mostrar (vacio = se usa la clave).';
COMMENT ON COLUMN tournaments.status IS 'Estado puesto por el administrador: upcoming, live o finished.';
COMMENT ON COLUMN tournaments.starts_on IS 'Fecha de inicio (opcional).';
COMMENT ON COLUMN tournaments.ends_on IS 'Fecha de fin (opcional; no anterior al inicio).';
COMMENT ON COLUMN tournaments.format IS 'Formato en texto libre corto, p. ej. Liga ida y vuelta.';
COMMENT ON COLUMN tournaments.description IS 'Descripcion del torneo.';
COMMENT ON COLUMN tournaments.rules_url IS 'URL https de las bases.';
COMMENT ON COLUMN tournaments.logo_url IS 'URL https del logo (puede ser un enlace de Drive); vacio = sin logo.';
COMMENT ON COLUMN tournaments.champion_id IS 'Clan campeon (opcional; NULL si el clan se borra).';
COMMENT ON COLUMN tournaments.runner_up_id IS 'Clan subcampeon (opcional; NULL si el clan se borra).';
COMMENT ON COLUMN tournaments.visible IS 'false = oculto en la app publica.';
COMMENT ON COLUMN tournaments.created_at IS 'Cuando se creo la ficha.';
COMMENT ON COLUMN tournaments.updated_at IS 'Ultima edicion.';
COMMENT ON COLUMN tournaments.updated_by IS 'Quien hizo la ultima edicion (administrador).';

COMMENT ON TABLE tournament_phases IS 'Fases de un torneo (1 a 10, en orden), cada una con su modalidad. Una partida cae en la fase con el starts_at mas reciente que no sea posterior a ella; la primera fase siempre empieza desde el inicio.';
COMMENT ON COLUMN tournament_phases.id IS 'Clave interna de la fase.';
COMMENT ON COLUMN tournament_phases.tournament_id IS 'Torneo al que pertenece (se borra en cascada con la ficha).';
COMMENT ON COLUMN tournament_phases.name IS 'Nombre de la fase (1 a 40 caracteres).';
COMMENT ON COLUMN tournament_phases.scoring IS 'Modalidad: league (tabla con puntos y cruces), knockout (eliminacion: cruces por ronda) o results (solo resultados).';
COMMENT ON COLUMN tournament_phases.points_per IS 'Puntos de la liga por mapa (match) o por cruce (series).';
COMMENT ON COLUMN tournament_phases.points_win IS 'Puntos por victoria (0 a 100).';
COMMENT ON COLUMN tournament_phases.points_draw IS 'Puntos por empate (0 a 100).';
COMMENT ON COLUMN tournament_phases.points_loss IS 'Puntos por derrota (0 a 100).';
COMMENT ON COLUMN tournament_phases.starts_at IS 'Desde cuando rige la fase; NULL en la primera (desde el inicio).';
COMMENT ON COLUMN tournament_phases.sort_order IS 'Posicion de la fase dentro del torneo (1 = primera).';

COMMENT ON TABLE tournament_teams IS 'Clanes inscritos en un torneo, con su grupo. Los grupos y los inscritos sin partidas solo se usan en la primera fase.';
COMMENT ON COLUMN tournament_teams.tournament_id IS 'Torneo (se borra en cascada con la ficha).';
COMMENT ON COLUMN tournament_teams.clan_id IS 'Clan inscrito (se borra en cascada con el clan).';
COMMENT ON COLUMN tournament_teams.group_name IS 'Grupo del clan (hasta 24 caracteres); vacio = sin grupo.';

COMMENT ON TABLE tournament_match_sides IS 'Datos manuales de una partida de torneo: clan de cada lado, "no cuenta", fase y ronda. Se escribe con tournament_side_save / tournament_assign_save.';
COMMENT ON COLUMN tournament_match_sides.match_id IS 'Partida (se borra en cascada con ella).';
COMMENT ON COLUMN tournament_match_sides.clan0_id IS 'Clan fijado a mano para el equipo 0; NULL = automatico.';
COMMENT ON COLUMN tournament_match_sides.clan1_id IS 'Clan fijado a mano para el equipo 1; NULL = automatico.';
COMMENT ON COLUMN tournament_match_sides.excluded IS 'true = "no cuenta": sigue en el torneo (estadisticas) pero no suma a cruces ni tabla.';
COMMENT ON COLUMN tournament_match_sides.phase_id IS 'Fase fijada a mano; NULL = automatica por fecha.';
COMMENT ON COLUMN tournament_match_sides.round IS 'Ronda (etiqueta libre de hasta 32 caracteres, p. ej. Semifinal); vacio = sin ronda.';
COMMENT ON COLUMN tournament_match_sides.updated_at IS 'Ultima edicion.';
COMMENT ON COLUMN tournament_match_sides.updated_by IS 'Quien hizo la ultima edicion (administrador).';

COMMENT ON TABLE match_event_changes IS 'Pedidos de cambio de torneo de una partida, creados por el panel admin (tournament_event_request) y aplicados por el ingestor (apply_event_changes).';
COMMENT ON COLUMN match_event_changes.id IS 'Clave interna del pedido.';
COMMENT ON COLUMN match_event_changes.match_id IS 'Partida (se borra en cascada con ella).';
COMMENT ON COLUMN match_event_changes.new_event IS 'Torneo al que pasa la partida; vacio = sacarla del torneo (partida normal).';
COMMENT ON COLUMN match_event_changes.requested_by IS 'Administrador que lo pidio.';
COMMENT ON COLUMN match_event_changes.requested_at IS 'Cuando se pidio.';
COMMENT ON COLUMN match_event_changes.applied_at IS 'Cuando lo aplico el ingestor; NULL = pendiente.';

COMMENT ON VIEW v_tournaments IS 'Torneos: las fichas mas las claves usadas en partidas sin ficha (registered = false, estado deducido: live si la ultima partida fue hace 7 dias o menos).';
COMMENT ON FUNCTION tournament_sides(text) IS 'Clan, fase y ronda de cada partida de un torneo: clan automatico por mayoria de humanos del lado (al menos la mitad, sin empate, entre los inscritos o los clanes activos), correcciones manuales y fase por fecha. counted es la unica definicion de "la partida cuenta" para cruces y tabla.';
COMMENT ON FUNCTION tournament_series(text) IS 'Cruces de un torneo: partidas que cuentan entre el mismo par de clanes en la misma fase y ronda. Ganador por mapas, luego por frags (by_frags); winner_id NULL = empate.';
COMMENT ON FUNCTION tournament_standings(text) IS 'Tabla de posiciones por fase y grupo (PJ, G, E, P, frags, puntos), por mapa o por cruce segun la fase. La app solo la muestra en las fases de liga.';
COMMENT ON FUNCTION tournament_save(integer, jsonb, jsonb, jsonb, text) IS 'Crea (id NULL) o edita una ficha de torneo con sus fases y su lista de inscritos. SECURITY DEFINER: unica via de escritura para dday_clans. Clave repetida 23505; cambiar la clave con partidas 55006; fases invalidas 22023.';
COMMENT ON FUNCTION tournament_delete(integer) IS 'Borra una ficha (fases e inscritos en cascada) y los datos manuales de sus partidas; no borra partidas. true si existia. SECURITY DEFINER.';
COMMENT ON FUNCTION tournament_side_save(text, integer, integer, boolean, text) IS 'Fija el clan de cada lado de una partida de torneo y/o la marca "no cuenta" (no toca fase ni ronda). SECURITY DEFINER.';
COMMENT ON FUNCTION tournament_side_clear(text) IS 'Vuelve a automatico los clanes y el "no cuenta" de una partida, sin perder fase ni ronda. SECURITY DEFINER.';
COMMENT ON FUNCTION tournament_side_prune(text[]) IS 'Auxiliar: borra las filas vacias de tournament_match_sides. Sin permisos para nadie mas que el dueno.';
COMMENT ON FUNCTION tournament_assign_save(text[], integer, text, text) IS 'Pone fase y ronda a varias partidas del mismo torneo (las de un cruce). SECURITY DEFINER.';
COMMENT ON FUNCTION tournament_event_request(text[], text, text) IS 'Pide agregar partidas a un torneo (event de una ficha) o sacarlas (event vacio); lo aplica el ingestor. Devuelve cuantos pedidos creo. SECURITY DEFINER.';
COMMENT ON FUNCTION apply_event_changes() IS 'Aplica los pedidos pendientes de match_event_changes (cambia matches.event y borra los datos manuales). Solo el dueno (ingestor); despues hay que recalcular resumenes y Elo.';

COMMIT;
