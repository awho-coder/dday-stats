-- Esquema de estadisticas de D-Day: Normandy.
-- Lo carga stats/ingest.py a partir de los archivos JSONL que escribe la DLL.
-- Es idempotente: se puede ejecutar varias veces (psql -f schema.sql).

BEGIN;

CREATE TABLE IF NOT EXISTS players (
    id          serial PRIMARY KEY,
    name        text        NOT NULL,
    is_bot      boolean     NOT NULL DEFAULT false,
    first_seen  timestamptz NOT NULL,
    last_seen   timestamptz NOT NULL,
    UNIQUE (name, is_bot)
);

CREATE TABLE IF NOT EXISTS matches (
    id           text PRIMARY KEY,           -- <fecha>-<servidor>-<random>
    server       text        NOT NULL,
    map          text        NOT NULL,
    mode         text        NOT NULL,       -- dm, ctb, campaign
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
    kills        integer NOT NULL DEFAULT 0,
    deaths       integer NOT NULL DEFAULT 0,
    suicides     integer NOT NULL DEFAULT 0,
    teamkills    integer NOT NULL DEFAULT 0,
    headshots    integer NOT NULL DEFAULT 0,
    objectives   integer NOT NULL DEFAULT 0,
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

-- Rating Elo por equipos (lo calcula ingest.py)
CREATE TABLE IF NOT EXISTS ratings (
    player_id   integer PRIMARY KEY REFERENCES players (id),
    rating      real        NOT NULL DEFAULT 1500,
    peak        real        NOT NULL DEFAULT 1500,
    games       integer     NOT NULL DEFAULT 0,
    wins        integer     NOT NULL DEFAULT 0,
    losses      integer     NOT NULL DEFAULT 0,
    draws       integer     NOT NULL DEFAULT 0,
    updated_at  timestamptz NOT NULL DEFAULT now()
);

CREATE TABLE IF NOT EXISTS rating_history (
    match_id       text    NOT NULL REFERENCES matches (id) ON DELETE CASCADE,
    player_id      integer NOT NULL REFERENCES players (id),
    rating_before  real    NOT NULL,
    rating_after   real    NOT NULL,
    PRIMARY KEY (match_id, player_id)
);

------------------------------------------------------------------------------
-- Vistas
------------------------------------------------------------------------------

-- Totales por jugador humano (todas las partidas)
CREATE OR REPLACE VIEW v_player_totals AS
SELECT p.id                                   AS player_id,
       p.name,
       count(*)                               AS matches,
       count(*) FILTER (WHERE mp.result = 'W') AS wins,
       count(*) FILTER (WHERE mp.result = 'L') AS losses,
       count(*) FILTER (WHERE mp.result = 'D') AS draws,
       sum(mp.kills)                          AS kills,
       sum(mp.deaths)                         AS deaths,
       round(sum(mp.kills)::numeric / greatest(sum(mp.deaths), 1), 2) AS kd,
       sum(mp.headshots)                      AS headshots,
       round(100.0 * sum(mp.headshots) / greatest(sum(mp.kills), 1), 1) AS hs_pct,
       round(100.0 * sum(mp.hits) / greatest(sum(mp.hits) + sum(mp.misses), 1), 1) AS accuracy_pct,
       sum(mp.teamkills)                      AS teamkills,
       sum(mp.suicides)                       AS suicides,
       sum(mp.objectives)                     AS objectives,
       round(sum(mp.time_team0 + mp.time_team1) / 3600.0, 1) AS hours_played,
       round(sum(mp.kills) * 60.0 / greatest(sum(mp.time_team0 + mp.time_team1), 1), 2) AS kills_per_min,
       round(100.0 * count(*) FILTER (WHERE mp.result = 'W')
             / greatest(count(*) FILTER (WHERE mp.result IS NOT NULL), 1), 1) AS win_pct,
       max(m.started_at)                      AS last_match
FROM match_players mp
JOIN players p ON p.id = mp.player_id
JOIN matches m ON m.id = mp.match_id
WHERE NOT p.is_bot
GROUP BY p.id, p.name;

-- Ladder por K/D (minimo de partidas configurable)
CREATE OR REPLACE FUNCTION ladder_kd(min_matches integer DEFAULT 10)
RETURNS TABLE (pos bigint, name text, matches bigint, kills bigint, deaths bigint,
               kd numeric, kills_per_min numeric, win_pct numeric, hs_pct numeric)
LANGUAGE sql STABLE AS $$
    SELECT rank() OVER (ORDER BY t.kd DESC, t.kills DESC),
           t.name, t.matches, t.kills, t.deaths, t.kd, t.kills_per_min, t.win_pct, t.hs_pct
    FROM v_player_totals t
    WHERE t.matches >= min_matches
    ORDER BY 1, t.name;
$$;

-- Ladder por rating Elo (minimo de partidas rankeadas configurable)
CREATE OR REPLACE FUNCTION ladder_elo(min_games integer DEFAULT 10)
RETURNS TABLE (pos bigint, name text, rating integer, peak integer,
               games integer, wins integer, losses integer, draws integer, win_pct numeric)
LANGUAGE sql STABLE AS $$
    SELECT rank() OVER (ORDER BY r.rating DESC),
           p.name, round(r.rating)::integer, round(r.peak)::integer,
           r.games, r.wins, r.losses, r.draws,
           round(100.0 * r.wins / greatest(r.games, 1), 1)
    FROM ratings r
    JOIN players p ON p.id = r.player_id
    WHERE r.games >= min_games AND NOT p.is_bot
    ORDER BY 1, p.name;
$$;

-- Estadisticas por jugador y mapa
CREATE OR REPLACE VIEW v_player_maps AS
SELECT mp.player_id,
       m.map,
       count(*)                                AS matches,
       count(*) FILTER (WHERE mp.result = 'W') AS wins,
       sum(mp.kills)                           AS kills,
       sum(mp.deaths)                          AS deaths,
       round(sum(mp.kills)::numeric / greatest(sum(mp.deaths), 1), 2) AS kd,
       sum(mp.time_team0 + mp.time_team1)      AS seconds_played
FROM match_players mp
JOIN matches m ON m.id = mp.match_id
GROUP BY mp.player_id, m.map;

-- Mapa mas jugado por jugador
CREATE OR REPLACE VIEW v_player_fav_map AS
SELECT DISTINCT ON (player_id) player_id, map, matches, seconds_played
FROM v_player_maps
ORDER BY player_id, matches DESC, seconds_played DESC, map;

-- Arma con mas kills por jugador (arma en mano o, si no aplica, la causa)
CREATE OR REPLACE VIEW v_player_weapons AS
SELECT k.killer_id                     AS player_id,
       coalesce(k.weapon, k.mod)       AS weapon,
       count(*)                        AS kills,
       count(*) FILTER (WHERE k.headshot) AS headshots,
       round(avg(k.distance))          AS avg_distance
FROM kills k
WHERE k.killer_id IS NOT NULL AND NOT k.friendly_fire
GROUP BY k.killer_id, coalesce(k.weapon, k.mod);

CREATE OR REPLACE VIEW v_player_fav_weapon AS
SELECT DISTINCT ON (player_id) player_id, weapon, kills
FROM v_player_weapons
ORDER BY player_id, kills DESC, weapon;

-- Clase mas jugada (por tiempo)
CREATE OR REPLACE VIEW v_player_classes AS
SELECT mp.player_id, c.key AS class, sum(c.value::integer) AS seconds
FROM match_players mp, jsonb_each_text(mp.classes) c
GROUP BY mp.player_id, c.key;

CREATE OR REPLACE VIEW v_player_fav_class AS
SELECT DISTINCT ON (player_id) player_id, class, seconds
FROM v_player_classes
ORDER BY player_id, seconds DESC, class;

-- Quien mata mas a cada jugador (nemesis) y a quien mata mas (victima favorita)
CREATE OR REPLACE VIEW v_player_duels AS
SELECT k.killer_id, k.victim_id, count(*) AS kills
FROM kills k
WHERE k.killer_id IS NOT NULL AND NOT k.friendly_fire AND NOT k.suicide
GROUP BY k.killer_id, k.victim_id;

CREATE OR REPLACE VIEW v_player_nemesis AS
SELECT DISTINCT ON (d.victim_id) d.victim_id AS player_id, p.name AS nemesis, d.kills
FROM v_player_duels d
JOIN players p ON p.id = d.killer_id
ORDER BY d.victim_id, d.kills DESC, p.name;

CREATE OR REPLACE VIEW v_player_favorite_victim AS
SELECT DISTINCT ON (d.killer_id) d.killer_id AS player_id, p.name AS victim, d.kills
FROM v_player_duels d
JOIN players p ON p.id = d.victim_id
ORDER BY d.killer_id, d.kills DESC, p.name;

-- Mejores marcas por jugador
CREATE OR REPLACE VIEW v_player_records AS
SELECT mp.player_id,
       max(mp.kills)                             AS most_kills_match,
       (SELECT max(k.distance) FROM kills k
         WHERE k.killer_id = mp.player_id AND NOT k.friendly_fire) AS longest_kill
FROM match_players mp
GROUP BY mp.player_id;

-- Perfil completo
CREATE OR REPLACE VIEW v_player_profile AS
SELECT t.*,
       round(r.rating)::integer AS rating,
       r.games                  AS ranked_games,
       fm.map                   AS fav_map,
       fw.weapon                AS fav_weapon,
       fc.class                 AS fav_class,
       ne.nemesis,
       fv.victim                AS favorite_victim,
       rec.most_kills_match,
       rec.longest_kill
FROM v_player_totals t
LEFT JOIN ratings r                 ON r.player_id = t.player_id
LEFT JOIN v_player_fav_map fm       ON fm.player_id = t.player_id
LEFT JOIN v_player_fav_weapon fw    ON fw.player_id = t.player_id
LEFT JOIN v_player_fav_class fc     ON fc.player_id = t.player_id
LEFT JOIN v_player_nemesis ne       ON ne.player_id = t.player_id
LEFT JOIN v_player_favorite_victim fv ON fv.player_id = t.player_id
LEFT JOIN v_player_records rec      ON rec.player_id = t.player_id;

-- Estadisticas por mapa (balance aliados / eje, duracion, letalidad)
CREATE OR REPLACE VIEW v_map_stats AS
SELECT m.map,
       count(*)                                          AS matches,
       count(*) FILTER (WHERE m.winner = 0)              AS allied_wins,
       count(*) FILTER (WHERE m.winner = 1)              AS axis_wins,
       count(*) FILTER (WHERE m.winner = -1)             AS draws,
       round(100.0 * count(*) FILTER (WHERE m.winner = 0)
             / greatest(count(*) FILTER (WHERE m.winner IN (0, 1)), 1), 1) AS allied_win_pct,
       round(avg(m.duration_s)::numeric / 60, 1)              AS avg_minutes,
       round(avg(m.humans), 1)                           AS avg_humans,
       (SELECT count(*) FROM kills k JOIN matches m2 ON m2.id = k.match_id
         WHERE m2.map = m.map AND NOT k.suicide)         AS kills,
       max(m.started_at)                                 AS last_played
FROM matches m
GROUP BY m.map;

-- Armas mas letales por mapa
CREATE OR REPLACE VIEW v_map_weapons AS
SELECT m.map, coalesce(k.weapon, k.mod) AS weapon, count(*) AS kills
FROM kills k
JOIN matches m ON m.id = k.match_id
WHERE NOT k.suicide AND NOT k.friendly_fire
GROUP BY m.map, coalesce(k.weapon, k.mod);

-- Armas globales
CREATE OR REPLACE VIEW v_weapon_stats AS
SELECT coalesce(k.weapon, k.mod)           AS weapon,
       count(*)                            AS kills,
       count(*) FILTER (WHERE k.headshot)  AS headshots,
       round(avg(k.distance))              AS avg_distance,
       max(k.distance)                     AS max_distance
FROM kills k
WHERE NOT k.suicide AND NOT k.friendly_fire
GROUP BY coalesce(k.weapon, k.mod);

-- Actividad diaria
CREATE OR REPLACE VIEW v_daily_activity AS
SELECT date_trunc('day', m.started_at)::date AS day,
       count(DISTINCT m.id)                   AS matches,
       count(DISTINCT mp.player_id) FILTER (WHERE NOT p.is_bot) AS players
FROM matches m
LEFT JOIN match_players mp ON mp.match_id = m.id
LEFT JOIN players p ON p.id = mp.player_id
GROUP BY 1;

COMMIT;
