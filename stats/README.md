# Estadísticas y ladder

Sistema para guardar las estadísticas de las partidas en PostgreSQL y
construir un ladder (K/D y Elo), perfiles de jugador y estadísticas de mapa.
El diseño está en [`docs/stats/PLAN.md`](../docs/stats/PLAN.md) y la guía de
instalación paso a paso para **Windows y Linux** en
[`docs/stats/INSTALACION.md`](../docs/stats/INSTALACION.md).

El **catálogo del esquema** (una página por tabla, vista y función, más las
reglas del ladder) está en [`docs/knowledge/`](../docs/knowledge/), y el índice
de toda la documentación en [`docs/README.md`](../docs/README.md).

```
servidor Q2PRO ──> dday/stats/events/*.jsonl ──> ingest.py ──> PostgreSQL
```

La DLL **no se conecta a la base de datos**: solo escribe un archivo de
texto por partida. Si el disco o la base fallan, el juego sigue normal.

## 1. Servidor de juego

Compilar la DLL (`cd src && make`) y agregar en el `server.cfg`:

```
set stats_log 1                 // activa el registro (se aplica al cargar el siguiente mapa)
set stats_server "chile1"       // opcional: id del servidor (por defecto, hostname)
set stats_log_dir ""            // opcional: carpeta de salida absoluta
set stats_mode "public"         // "public" o "duel" (ver abajo)
set stats_event ""              // nombre del torneo; vacío = casual (ver abajo)
set sv_allow_map 1              // Q2PRO: sin esto el mod no puede cambiar de mapa
```

### Público o duelo (`stats_mode`)

| | `public` (por defecto) | `duel` |
|---|---|---|
| Qué se registra | el mapa completo | solo desde que la cuenta de `sv startcount` llega a 0 |
| Calentamiento | cuenta | se descarta |
| `sv freeze` (pausa) | — | el tiempo en pausa no suma; al reanudar se sigue |
| `sv resetcount` | — | descarta el duelo en curso |
| Mapa sin cuenta | se guarda | **no se guarda** |
| Ladder y Elo | públicos | de duelo, separados |

Ejemplo para un servidor de duelos de 20 personas:

```
set stats_log 1
set stats_server "duelos1"
set stats_mode "duel"
set maxclients 20
```

### Duelos oficiales de torneo (`stats_event`)

Antes de lanzar `sv startcount` de un duelo oficial, el admin pone el nombre
del torneo (en el `server.cfg` o por rcon):

```
set stats_event "copa-verano"
sv startcount 20
```

Ese duelo cuenta en la categoría **`official`** (con su propio ladder y Elo,
separados de los duelos casuales) y queda agrupado bajo "copa-verano". En
duelos vale el valor que tenga la cvar cuando la cuenta llega a 0.

> **Importante:** la cvar se mantiene entre mapas (cómodo para torneos de
> varios mapas). Al terminar el torneo hay que borrarla con
> `set stats_event ""`; si no, los duelos casuales siguientes contarán como
> oficiales.

`sv freeze` (pausa) solo funciona con `tournament 1`, que es lo normal en los
servidores de duelo.

Si se olvidó ponerla, se corrige después:

```sh
python3 stats/ingest.py --set-event <id_partida> "copa-verano"   # '' para volver a casual
```

Y para el público:

```
set stats_log 1
set stats_server "publico1"
set stats_mode "public"
set maxclients 20
```

Por defecto los archivos quedan en `<q2>/dday/stats/events/`. La carpeta se
crea sola. Con contenedores conviene fijar `stats_log_dir` a un volumen
compartido con el ingestor, por ejemplo `/data/events`.

Mientras la partida está en curso el archivo termina en `.jsonl.part`; al
terminar el mapa se renombra a `.jsonl`. Cada partida ocupa del orden de
20–60 KB.

Solo se registran partidas en modo deathmatch (`deathmatch 1`), que es el
modo normal de los servidores de D-Day.

## 2. Base de datos

```sh
createuser dday -P
createdb -O dday dday
pip install -r stats/requirements.txt      # psycopg 3
export DDAY_STATS_DSN="postgresql://dday:clave@localhost/dday"
python3 stats/ingest.py --init-schema       # crea tablas y vistas (idempotente)
```

`createuser` y `createdb` dejan la base **vacía**. Las tablas, vistas y
funciones las crea `ingest.py --init-schema` a partir de `stats/schema.sql`
(idempotente: se puede repetir sin perder datos y también sirve para migrar).

## 3. Ingestor

```sh
# una pasada (por ejemplo, desde cron cada minuto)
python3 stats/ingest.py --events /srv/q2/dday/stats/events

# como servicio, revisando cada 60 s
python3 stats/ingest.py --events /srv/q2/dday/stats/events --loop 60
```

- Procesa los `.jsonl` en orden cronológico, un archivo por transacción, y
  los mueve a `events/processed/` (con `--delete`, los borra).
- Los archivos con errores de formato se mueven a `events/failed/`.
- Si la base no responde, los archivos quedan donde están y se reintentan.
- Reprocesar un archivo no duplica nada (el id de partida es único).
- Un `.part` sin cambios por más de 6 h (`--stale-hours`) es una partida de
  un servidor caído: se carga como `aborted`, sin rating, y se reconstruyen
  las kills y muertes de cada jugador a partir de los eventos `kill`.

Ejemplo de servicio systemd (`/etc/systemd/system/dday-stats.service`):

```ini
[Unit]
Description=D-Day stats ingestor
After=network-online.target postgresql.service

[Service]
Environment=DDAY_STATS_DSN=postgresql://dday:clave@localhost/dday
ExecStart=/usr/bin/python3 /opt/dday/stats/ingest.py --events /srv/q2/dday/stats/events --loop 60
Restart=always
User=quake2

[Install]
WantedBy=multi-user.target
```

### Rachas de kills

El registro lleva su propio contador de rachas con las mismas reglas que el
anuncio del juego (`exbattleinfo`): cada kill a un enemigo suma 1, morir,
cambiar de equipo o el fuego amigo la reinician, y la kill por desangrado no
suma. Funciona aunque `exbattleinfo` esté apagado. Se guarda la mejor racha
de cada jugador en cada partida.

### Golpes de suerte

Se registran dos momentos de suerte del juego:

- **casco** (`helmet_saves`): el casco desvió un tiro a la cabeza
  ("X is a lucky bastard! The helmet deflected the shot").
- **pie** (`foot_saves`): un tiro de rifle de cerrojo lo dejó desangrándose en
  vez de matarlo ("X almost lost a foot! He needs a medic").

`luck` es la suma de ambos. Al revés, `deflected` cuenta los tiros de un
jugador que desvió el casco de otro: el tirador más desafortunado.

### Reglas del rating

Una partida es **rankeada** si terminó normalmente con ganador o empate,
duró lo suficiente y cada equipo tuvo suficientes humanos. Los bots nunca cuentan.

| | público | duelo |
|---|---|---|
| duración mínima | 5 min (`--min-duration`) | 2 min (`--duel-min-duration`) |
| humanos mínimos por equipo | 2 (`--min-humans`) | 1 (`--duel-min-humans`) |

Hay tres categorías con **ratings separados**: `public`, `duel` (casual) y
`official` (cualquier partida con `stats_event`). Cada categoría tiene además un
Elo **general** (`mode = 'all'`, todas las partidas) y un Elo **por modo** (`dm`,
`ctb`, `campaign`, `control`...): cada partida rankeada actualiza los dos, así
ganar una partida de control no mueve el Elo de kills de `dm`. En el ladder K/D, las
kills a bots y las muertes causadas por bots no cuentan (se ven aparte en
`bot_kills`).

Elo por equipos: se compara el rating promedio de cada equipo (ponderado por
el tiempo jugado) y cada jugador gana o pierde
`K × (resultado − esperado) × participación`, con `K = 40` en las primeras
10 partidas y `24` después. Quien jugó menos del 20 % de la partida en su
equipo (`--min-participation`) no se ve afectado.

Si cambian las reglas, `--rebuild-ratings` recalcula el Elo desde cero sobre
las partidas marcadas como rankeadas.

### Temporadas

Las temporadas se inician a mano. Cada partida pertenece a la temporada
vigente cuando empezó; los ladders muestran por defecto la temporada actual y
el Elo parte de 1500 en cada una. El histórico (todas las temporadas) se
conserva siempre.

```sh
python3 stats/ingest.py --new-season "Temporada 2"                 # desde ahora
python3 stats/ingest.py --new-season "Copa Invierno" --season-start "2026-07-01 00:00-04"
```

Con `--season-start` en el pasado, las partidas posteriores a esa fecha se
reasignan y se recalcula todo solo.

## 4. Consultas

```sql
-- Parámetros comunes:
--   categoría: 'public', 'duel', 'official' o 'all'
--   temporada: 'current' (por defecto), 'all' (histórico) o el nombre de una temporada
--   modo (último parámetro de ladder_kd / ladder_elo / ladder_streak / ladder_luck /
--   ladder_unlucky / ladder_streak_tiers, y 4.º de player_totals): 'dm', 'ctb',
--   'campaign', 'control'... o 'all' (por defecto). En ladder_elo 'all' es el Elo general.

SELECT * FROM ladder_kd(10);                         -- K/D, temporada actual, mínimo 10 partidas
SELECT * FROM ladder_kd(10, 'public');               -- solo público
SELECT * FROM ladder_kd(10, 'duel');                 -- solo duelos casuales
SELECT * FROM ladder_kd(1, 'official');              -- solo duelos oficiales
SELECT * FROM ladder_kd(1, 'official', 'all', 'copa-verano');  -- un torneo, histórico
SELECT * FROM ladder_kd(10, 'all', 'Temporada 1');   -- una temporada anterior
SELECT * FROM ladder_kd(10, 'all', 'all');           -- histórico
SELECT * FROM ladder_kd(10, 'all', 'current', NULL, 'dm');  -- K/D solo en deathmatch

SELECT * FROM ladder_elo(10);                        -- Elo público, temporada actual
SELECT * FROM ladder_elo(5, 'official');             -- Elo de duelos oficiales
SELECT * FROM ladder_elo(10, 'public', 'all');       -- Elo histórico
SELECT * FROM ladder_elo(10, 'public', 'current', 'control');  -- Elo solo del modo control
SELECT * FROM ladder_elo(10, 'public', 'current', 'dm');       -- Elo solo de deathmatch

SELECT * FROM ladder_streak();                       -- 20 mejores rachas, temporada actual
SELECT * FROM ladder_streak('duel', 10, 'all');      -- 10 mejores rachas en duelos, histórico

SELECT * FROM ladder_zone();                         -- modo control: capturas y minutos en la zona
SELECT * FROM ladder_zone(1, 'all', 'all');          -- idem, sin mínimo de partidas, histórico

SELECT * FROM ladder_luck();                         -- los más suertudos (casco + pie), mín. 5 partidas
SELECT * FROM ladder_unlucky();                      -- tiradores con más tiros desviados por cascos

SELECT * FROM v_seasons;                             -- temporadas, fechas y partidas
SELECT * FROM v_events;                              -- torneos jugados
SELECT * FROM player_totals('current', 'official');  -- totales completos con cualquier filtro

SELECT * FROM v_player_totals;                    -- una fila por jugador (histórico)
SELECT * FROM v_player_totals_by_kind;            -- una fila por jugador y categoría (histórico)
SELECT * FROM v_player_totals_by_mode;            -- una fila por jugador y modo de juego (histórico)

SELECT * FROM v_player_profile WHERE name = 'Pato';
-- partidas, victorias, K/D, KPM, % headshots, precisión, horas jugadas,
-- rating, mejor racha, mapa / arma / clase favorita, némesis, víctima favorita, récords

SELECT * FROM v_player_maps WHERE player_id = 1 ORDER BY matches DESC;
SELECT * FROM v_player_weapons WHERE player_id = 1 ORDER BY kills DESC;

SELECT * FROM v_map_stats ORDER BY matches DESC;   -- balance aliados / eje por mapa, tipo y modo
SELECT * FROM v_map_weapons WHERE map = 'dday1' ORDER BY kills DESC;
SELECT * FROM v_weapon_stats ORDER BY kills DESC;
SELECT * FROM v_daily_activity ORDER BY day DESC;

-- mapa de calor de muertes en un mapa
SELECT victim_x, victim_y FROM kills k JOIN matches m ON m.id = k.match_id WHERE m.map = 'dday1';
```

## Rendimiento

Las vistas del ladder leen **tablas de resumen** (`player_stats`,
`map_stats`, `player_weapon_stats`, etc.) que el ingestor actualiza en la misma
transacción en que carga cada partida (unos milisegundos por partida). Así las
consultas no recorren la tabla de kills, que es la que más crece.

Medido con un año de un servidor de 20 jugadores muy activo (15.000 partidas,
3 millones de kills, ~570 MB):

| consulta | sin resúmenes | con resúmenes |
|---|---|---|
| perfil de un jugador | 3 s | 35 ms |
| todos los perfiles | 11 s | 36 ms |
| estadísticas de mapas | 11 s | 2 ms |
| ladder K/D | 200 ms | 8 ms |
| armas globales | 210 ms | 2 ms |
| actividad diaria | 290 ms | 2 ms |

- Las tablas de resumen se crean y llenan solas con `--init-schema`.
- Si se borran o corrigen partidas a mano en la base:
  `python3 stats/ingest.py --rebuild-stats` (o `SELECT rebuild_rollups();`).
- El ingestor en modo `--loop` solo se conecta a la base cuando hay archivos
  nuevos.
- La actividad diaria usa el día en hora de Chile (`America/Santiago`).

## Identidad

La identidad del jugador es su **nombre** (sin colores). Quien cambia de nombre
aparece como otro jugador, y un nombre se puede usar desde otro equipo.
