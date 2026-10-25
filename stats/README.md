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

La ficha del torneo (fases, inscritos, correcciones) se edita en el panel de
administrador de la app, y desde ahí también se agregan o sacan partidas sin
usar la consola (ver [Torneos](#torneos)).

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
- En cada vuelta (también sin archivos nuevos) aplica los **cambios de torneo
  pedidos desde la app** (agregar o sacar partidas de un torneo, tabla
  `match_event_changes`): cambia `matches.event` y, si algo cambió, recalcula los
  resúmenes y el Elo una sola vez. Con el servicio en `--loop 60` el cambio
  se ve en la app en un minuto como máximo. A mano:
  `python3 stats/ingest.py --apply-event-changes`.
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

### Free For All (`mode = 'ffa'`)

Al llegar al fraglimit la partida termina como `normal`, sin equipo ganador. Cada
jugador que llega al final lleva su puesto (`place`, por puntaje; los empatados
comparten puesto: 1, 1, 3...) y `match_end` trae `ffa_winner` (el único primero,
o `null` si empataron). Quien se fue antes no tiene puesto ni resultado.

- Resultado: `W` al único primero, `D` a los empatados en el primer puesto, `L`
  al resto.
- **Rankeada** (solo para `ladder_ffa` de abajo, no para el Elo): final normal,
  al menos 5 minutos (`--min-duration`) y al menos 3 humanos que terminaron
  (`--ffa-min-humans`, por defecto 3).
- **El FFA no mueve el Elo** (tampoco con `--rebuild-ratings`): solo los modos por
  equipos lo hacen. El FFA tiene su ladder: `ladder_ffa()` (partidas, victorias,
  podios en el top 3, puesto promedio, K/D y mejor racha).
- Las partidas FFA cargadas antes de este cambio (final `other`, sin puestos) las
  completa `--init-schema`: puesto por puntaje, resultado y `end_reason = 'normal'`.

### Modo control: kills en la zona

En mapas con zona, cada `kill` lleva `kzone` y `vzone` (si el que mató / el que
murió estaba dentro) y cada jugador `zone_kills`: kills a enemigos en que alguno
de los dos estaba en la zona (defender o limpiarla; sin fuego amigo ni
suicidios). En la base: `kills.killer_in_zone` / `victim_in_zone` (NULL fuera del
modo control y en partidas anteriores) y `zone_kills`, que muestra `ladder_zone()`.

### Partidas sin humanos

Por defecto se cargan todas las partidas, incluso las que jugaron solo bots.
`--skip-bot-only` (o `DDAY_STATS_SKIP_BOT_ONLY=1`) deja de cargar las que no
tuvieron ningún humano: el archivo se mueve igual a `processed/` y el registro lo
dice. Está apagada por defecto.

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

Una partida de equipos es **rankeada** si terminó normalmente con ganador o empate,
duró lo suficiente y cada equipo tuvo suficientes humanos. Los bots nunca cuentan.

| | público | duelo |
|---|---|---|
| duración mínima | 5 min (`--min-duration`) | 2 min (`--duel-min-duration`) |
| humanos mínimos por equipo | 2 (`--min-humans`) | 1 (`--duel-min-humans`) |

Hay tres categorías con **ratings separados**: `public`, `duel` (casual) y
`official` (cualquier partida con `stats_event`). El Elo es uno solo para todos
los modos de juego (`dm`, `ctb`, `control`...). En el ladder K/D, las
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
--   modo (último parámetro de ladder_kd / ladder_streak / ladder_luck /
--   ladder_unlucky / ladder_streak_tiers, y 4.º de player_totals): 'dm', 'ctb',
--   'campaign', 'control'... o 'all' (por defecto). ladder_elo no lo tiene: el Elo
--   es uno solo para todos los modos.

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

SELECT * FROM ladder_streak();                       -- 20 mejores rachas, temporada actual
SELECT * FROM ladder_streak('duel', 10, 'all');      -- 10 mejores rachas en duelos, histórico

SELECT * FROM ladder_zone();                         -- modo control: capturas, minutos y kills en la zona
SELECT * FROM ladder_zone(1, 'all', 'all');          -- idem, sin mínimo de partidas, histórico

SELECT * FROM ladder_ffa();                          -- Free For All: victorias, podios, puesto promedio (mín. 3)
SELECT * FROM ladder_ffa(1, 'all', 'all');           -- idem, sin mínimo de partidas, histórico

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

## Clanes

Los clanes del Ladder Test viven en la misma base. Se editan desde el panel de
administrador de la app; la carga inicial (hoja `ClanTags` más `=SL=` y `>6S<`)
es `stats/clans_seed.sql`, que se aplica una sola vez.

| Objeto | Qué es |
|---|---|
| `clans` | Un clan: nombre, líder, miembros declarados, mapa favorito, jugador símbolo, frase, rival, logo, `unregistered`, `active`, `sort_order`, quién lo editó. |
| `clan_tags` | Los tags de cada clan: uno principal (`is_primary`) y 0 o más alternativos. Únicos sin distinguir mayúsculas. |
| `v_clans` | Clanes con su tag principal (`tag`) y los alternativos (`aliases`). |
| `v_player_clan` | Clan de cada nick humano (`clan_id` NULL si no tiene). Solo clanes activos. |
| `ladder_clans(categoría, temporada)` | Ranking: suma de los miembros por nick. K/D «sin muestra» (NULL) con menos de 20 muertes; Elo promedio de los miembros con 5 partidas o más; MVPs; mejor jugador. Solo clanes con miembros activos. |
| `clan_duels(categoría, temporada, mínimo_por_lado)` | Duelos de clanes (regla D5, abajo). |
| `clan_preview(id, tags)` | Vista previa del mantenedor: qué nicks quedarían en el clan con esos tags y de qué clan vendrían. |
| `clan_save` / `clan_delete` | Únicas vías de escritura (`SECURITY DEFINER`). |

**Pertenencia de un nick a un clan (`clan_tag_matches`).** Sin distinguir
mayúsculas, el nick pertenece al clan si **contiene** el tag. Un tag formado
**solo por letras y números** (`WR`, `7up`) debe estar **al comienzo** del nick
y **seguido de un carácter que no sea letra ni número** (o ser el nick
completo): `WR|Juan` sí, `Wrath` y `WRmaster` no. Si calzan varios tags gana el
más largo; si empatan, el de menor `sort_order` y luego el de menor `id`. Un
nick pertenece a un solo clan y solo cuentan los clanes activos. La pertenencia
es **por nick**, no por persona: cada partida cuenta para el clan del nick que
se usó.

**Clan No Inscritos (`unregistered`).** Va al final de las listas, fuera de los
podios, y nunca cuenta para los duelos de clanes.

**Duelo de clanes.** Partida por equipos (no FFA), **sin ningún bot**, en la que
**todos los jugadores** del equipo 0 son integrantes de un mismo clan y todos
los del equipo 1, de otro clan distinto; ninguno de los dos `unregistered`. Un
humano sin clan anula el duelo. Mínimo 2 jugadores por lado.

**Permisos.** `dday_ro` lee las tablas y vistas nuevas (privilegios por defecto)
pero no escribe. El rol `dday_clans` (lo usa el backend del mantenedor) hereda
de `dday_ro` y solo puede ejecutar `clan_save` y `clan_delete`; no puede
escribir directamente en `clans` ni en `clan_tags`. `schema.sql` no crea ese
rol (lleva clave): ver [`docs/stats/DESPLIEGUE.md`](../docs/stats/DESPLIEGUE.md).

```sql
SELECT * FROM v_clans ORDER BY sort_order, id;
SELECT * FROM v_player_clan WHERE clan_id IS NOT NULL ORDER BY clan, name;
SELECT * FROM ladder_clans();                         -- temporada actual, todas las categorías
SELECT * FROM ladder_clans('public', 'all');          -- solo público, histórico
SELECT * FROM clan_duels('all', 'all');               -- duelos de clanes (mínimo 2 por lado)
SELECT * FROM clan_preview(NULL, ARRAY['=SL=', 'SL+']);  -- quién quedaría con estos tags
```

## Torneos

Un torneo es un valor de `stats_event` (`matches.event`); la DLL no cambia. La
**ficha** es opcional y se edita en el panel de administrador de la app
(Ladder Test → Torneos), igual que los clanes: datos básicos, **fases**,
**participantes** (clanes inscritos, con grupo), el clan de cada lado, la
fase y la ronda de cada partida, y agregar o sacar partidas. Sin ficha, un
`stats_event` igual aparece como torneo (nombre = clave); una ficha sin partidas
aparece como torneo próximo.

| Objeto | Qué es |
|---|---|
| `tournaments` | La ficha: clave (`event`, única), nombre, estado, fechas, formato, descripción, bases, logo, campeón y subcampeón, `visible`. |
| `tournament_phases` | De 1 a 10 fases por torneo, en orden, cada una con su modalidad (`scoring`) y sus puntos. |
| `tournament_teams` | Clanes inscritos y su grupo (solo se usan en la primera fase). |
| `tournament_match_sides` | Datos manuales de una partida: clan de cada lado, «no cuenta», fase y ronda. |
| `match_event_changes` | Pedidos de agregar o sacar una partida de un torneo; los aplica el ingestor. |
| `v_tournaments` | Fichas más las claves usadas en partidas sin ficha (`registered = false`). |
| `tournament_sides(event)` | Clan, fase y ronda de cada partida, y `counted`. |
| `tournament_series(event)` | Cruces. |
| `tournament_standings(event)` | Tabla de posiciones por fase y grupo. |
| `tournament_save` / `tournament_delete` / `tournament_side_save` / `tournament_side_clear` / `tournament_assign_save` / `tournament_event_request` | Únicas vías de escritura (`SECURITY DEFINER`, las ejecuta `dday_clans`). |
| `apply_event_changes()` | Aplica los pedidos pendientes. Solo la ejecuta el dueño (el ingestor). |

**Fases y modalidades.** Un torneo tiene una o más fases, todas con el mismo
`stats_event`: en el servidor no se cambia nada entre fases. Cada fase elige su
modalidad: **liga** (`league`: tabla de posiciones con puntos y cruces),
**eliminación** (`knockout`: cruces por ronda, sin tabla) o **solo resultados**
(`results`: solo los cruces con su marcador). Una partida cae en la última fase
que ya empezó según su fecha (la primera fase empieza siempre desde el inicio),
o en la que el administrador le fije a mano. Un torneo sin ficha no tiene fases:
sus cruces salen con `phase_id` NULL.

**Clan de cada lado.** Por partida y por lado (equipo 0 y 1) es el clan que
tiene **al menos la mitad de los humanos** de ese lado, sin empate, entre los
clanes inscritos (o entre los clanes activos si no hay inscritos; el clan No
Inscritos nunca). Un mercenario no anula el lado si su clan tiene mayoría; si
ningún clan llega a la mitad, el lado queda **sin asignar**. El administrador
puede fijar los clanes a mano o marcar la partida como «no cuenta».

**La partida «cuenta»** (`tournament_sides.counted`, la única definición; la usan
`tournament_series`, `tournament_standings`, el backend y el mantenedor) solo si
los dos lados tienen clan, son clanes distintos, `winner` es -1, 0 o 1, el
final no fue `aborted` y no está marcada «no cuenta».

**Cruces.** Todas las partidas que cuentan entre los mismos dos clanes, en la
misma fase y la misma ronda (la ronda es una etiqueta libre por partida, p. ej.
«Semifinal»; sin ronda, todas las partidas entre dos clanes son un cruce). El
marcador son los mapas ganados por cada clan; gana quien tiene más mapas, si
empatan quien tiene más frags (`by_frags`) y si siguen empatados el cruce es un
empate (`winner_id` NULL).

**Tabla de posiciones.** Por fase y por grupo. Con `points_per = 'match'` cada
mapa es una partida jugada; con `'series'`, cada cruce. PTS = G × `points_win` +
E × `points_draw` + P × `points_loss`; F+ y F− son los kills del clan y del
rival; se ordena por PTS, diferencia, F+ y tag. Los inscritos sin partidas y los
grupos solo aparecen en la primera fase.

**Agregar o sacar partidas.** El panel pide el cambio (`tournament_event_request`,
tabla `match_event_changes`) y el **ingestor lo aplica** en su siguiente vuelta
(`apply_event_changes`), con el recálculo de resúmenes y Elo de siempre. Una
partida sacada pierde sus datos manuales (clanes, fase y ronda). Distinto de
«no cuenta»: esa sigue en el torneo (estadísticas) pero no suma a cruces ni tabla.

**Permisos.** `dday_ro` lee las tablas y la vista; `dday_clans` solo ejecuta las
funciones de escritura de la tabla de arriba. `tournament_side_prune` y
`apply_event_changes` no se dan a ningún rol.

```sql
SELECT * FROM v_tournaments;
SELECT * FROM tournament_sides('copa-verano');
SELECT * FROM tournament_series('copa-verano');
SELECT * FROM tournament_standings('copa-verano');
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
