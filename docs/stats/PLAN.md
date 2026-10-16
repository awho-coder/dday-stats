# Plan: estadísticas en PostgreSQL y ladder

Rama: `feature/stats-db`

## Objetivo

Registrar lo que pasa en cada partida (kills, objetivos, resultado, tiempo
jugado por equipo y clase) en PostgreSQL para construir:

- **Ladder por K/D** (kills, muertes, K/D, KPM, % victorias).
- **Ladder por rating** (Elo por equipos, solo partidas "rankeadas").
- **Estadísticas de jugador**: mapa más jugado, arma y clase favorita,
  headshots, precisión, némesis / víctima favorita, horas jugadas.
- **Estadísticas de mapa**: veces jugado, % victoria aliados vs eje,
  duración promedio, armas más letales.

La identidad del jugador es su **nombre** (sin colores / bit alto). No se
implementa autenticación.

## Restricciones

- Los servidores son livianos y corren Linux + Q2PRO.
- El juego corre en un solo hilo: **la DLL nunca habla con la base de
  datos**. Solo escribe texto a disco, sin bloquear.
- Si falla la escritura, la DLL avisa por consola y sigue (nunca `gi.error`).

## Arquitectura

```
 Q2PRO + gamex86_64.real.so
   └─ g_statslog.c ── escribe ──> dday/stats/events/<match>.jsonl.part
                                  (al terminar la partida se renombra a .jsonl)
                                            │
 stats/ingest.py (cron / systemd / loop) ───┘  lee *.jsonl, inserta en una
   transacción por partida, calcula Elo, mueve el archivo a processed/
                                            │
                                     PostgreSQL (stats/schema.sql)
                                       tablas + vistas del ladder
```

- Un archivo por partida. Mientras la partida corre el archivo termina en
  `.part`; al terminar se renombra a `.jsonl`. El ingestor solo procesa
  archivos cerrados, por lo que nunca lee una partida a medias.
- Si el servidor se cae, queda un `.part` huérfano; el ingestor lo procesa
  como partida abortada (sin rating) pasado un tiempo configurable.
- Idempotente: el id de partida es clave primaria; reprocesar un archivo no
  duplica datos.

## Etapas

1. **Correcciones previas** (DLL)
   - `ClientDisconnect` escribía stats aunque `stats 0`, y `Write_Player_Stats`
     llamaba `gi.error` (botaba el servidor) si no existía `dday/stats`.
   - Bug en `Killed()`: usaba `targ->client->aim` en vez de `targ->ai` y
     decrementaba `stat_bot_minus`.
   - Makefile: `-std=gnu99` (GCC 15 usa C23 por defecto y no compila).
2. **Módulo `g_statslog.c`** (DLL), activado con `set stats_log 1`.
   Eventos JSON por línea: `match_start`, `team`, `kill`, `obj`, `leave`,
   `match_end` (con resumen por jugador).
3. **Esquema PostgreSQL** (`stats/schema.sql`): tablas `players`, `matches`,
   `match_players`, `kills`, `objectives`, `ratings`, `rating_history` y
   vistas para ladder / perfil / mapas.
4. **Ingestor** (`stats/ingest.py`): Python 3 + `psycopg` (única
   dependencia). Calcula Elo incremental al insertar cada partida.
5. **Documentación** (`stats/README.md`): instalación, cvars, ejemplos de
   consultas.
6. **Modo duelo** (`stats_mode duel`): solo cuenta lo jugado tras `sv
   startcount`; ladders y Elo separados de los del público.
7. **Temporadas** manuales (`--new-season`) y **duelos oficiales** por torneo
   (cvar `stats_event`): categorías `public` / `duel` / `official`, cada una
   con su Elo por temporada y uno histórico.
8. (Siguiente iteración) Web / API del ladder sobre las vistas.

## Formato de eventos (v2)

La versión 2 agrega campos (`place`, `ffa_winner`, `zone_kills`, `kzone`/`vzone`);
el ingestor acepta archivos v1 y v2.

Todos llevan `ev` y `t`: segundos de reloj de la partida, que cuenta desde que carga el mapa pero
se detiene mientras no hay nadie (humano ni bot) en un equipo.

| ev | campos |
|---|---|
| `match_start` | `v`, `match`, `server`, `map`, `mode` (`dm`/`ctb`/`campaign`/`coop`/`control`/`ffa`), `kind` (`public`/`duel`), `event` (torneo), `tournament`, `ts` (unix), `teams[]` (`idx`,`army`,`name`) |
| `play` | la primera vez que alguien entra a un equipo: `ts` (unix, hora real del inicio de la partida; el ingestor la usa como `started_at`), `idle` (segundos que el mapa estuvo cargado sin nadie) |
| `live` / `resume` / `live_cancel` | solo duelos: la cuenta llegó a 0 (con `event`) / se reanudó tras una pausa / `sv resetcount` |
| `team` | `slot`, `name`, `bot`, `team` (-1 = observador) |
| `kill` | `killer`/`victim` (`name`,`bot`,`team`,`class`,`pos`), `mod`, `weapon`, `hs`, `ff`, `suicide`, `dist`; en mapas con zona de control también `kzone` / `vzone` (0/1: el que mató / el que murió estaba dentro de la zona) |
| `luck` | `type` (`helmet`/`foot`), `player`, `by` (quien disparó), `mod` |
| `obj` | `type` (`touch`,`area`,`timed`,`timed_held`,`explosive`,`bc_pickup`,`bc_drop`,`bc_capture`,`zone_capture`), `name`, `team`, `player` (en `zone_capture`, un evento por cada jugador del equipo que estaba en la zona) |
| `leave` | resumen del jugador (igual que en `match_end.players[]`) |
| `match_end` | `ts`, `dur`, `winner` (0/1, -1 empate, null = sin ganador), `reason` (`normal`/`forced`), en FFA `ffa_winner` (nombre del único primero, `null` si empataron), `teams[]` (`score`,`kills`,`losses`), `players[]` |

Resumen por jugador: `name`, `bot`, `team` (último), `time` (`[s_aliados, s_eje]`),
`classes` (segundos por clase), `kills`, `deaths`, `suicides`, `tk`, `hs`,
`best_streak`, `helmet_saves`, `foot_saves`, `deflected`, `hits`, `misses`,
`score`, `points`; en mapas con zona de control también `zone_time` (segundos
dentro de la zona) y `zone_kills` (kills a enemigos en que el que mató o el
que murió estaba en la zona); no aparecen en los otros modos. En FFA, quien
termina la partida lleva `place` (puesto por `score`; los empatados comparten
puesto: 1, 1, 3...); quien se fue antes no tiene puesto.

## Reglas del ladder

- **K/D**: suma de partidas terminadas (incluye abortadas), sin bots, mínimo
  configurable de partidas para aparecer (vista con parámetro por defecto 10).
- **Elo por equipos**: partida rankeada si terminó normalmente, duró >= 5 min,
  y cada equipo tuvo al menos 2 humanos (configurable en el ingestor).
  `delta = K * (S - E) * participación`, con `E` calculado con el promedio
  de rating de cada equipo, `K = 40` en las primeras 10 partidas y luego 24,
  y `participación` = fracción del tiempo de la partida jugado en ese equipo.
- **Un solo Elo para todos los modos**: el rating es por categoría (`public`,
  `duel`, `official`) y temporada, sin separar por modo de juego. Los totales
  (`player_stats`, `map_stats`) sí llevan el modo, y los ladders de K/D, rachas
  y suerte reciben un parámetro opcional `match_mode` (por defecto `'all'`).
- **Modo control** (`mode = 'control'`): además del K/D del modo, se
  registran los segundos dentro de la zona (`zone_time`) y las capturas
  (`obj.type = 'zone_capture'`, una por cada jugador del equipo capturador que
  estaba en la zona) y las kills en la zona (`zone_kills`: el que mató o el que
  murió estaba dentro; `kills.killer_in_zone` / `victim_in_zone`). Se consultan
  con `ladder_zone()`.
- **FFA** (`mode = 'ffa'`): al llegar al fraglimit la partida termina como
  `normal` sin equipo ganador; el resultado sale del puesto (`W` al único primero,
  `D` a los empatados en el primer puesto, `L` al resto). Es rankeada con final
  normal, >= 5 min y >= 3 humanos que terminaron, pero **no mueve el Elo**: tiene
  su propio ladder, `ladder_ffa()` (victorias, podios, puesto promedio).
