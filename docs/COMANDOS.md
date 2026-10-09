# Comandos y cvars agregados

Referencia rápida de todo lo que este fork agregó o cambió respecto del
original (`pcarmona79`, hasta abril de 2026). El detalle de cada tema está en
el documento enlazado en cada sección.

> Al agregar un cvar, comando u opción nueva, sumarlo aquí.

## Servidor: modo control de zona

Detalle: [`modos/CONTROL.md`](modos/CONTROL.md).

| Cvar | Por defecto | Qué hace |
|---|---|---|
| `control_mode` | `0` | `1` activa el modo en los mapas con `ents/<mapa>.ctl` (requiere `deathmatch 1`; se aplica al cambiar de mapa). |
| `control_lock` | `30` | Segundos que la zona está bloqueada al empezar. |
| `control_captime` | `15` | Segundos que tarda un jugador solo en capturar la zona. |
| `control_holdtime` | `120` | Segundos de control para llegar al 100%. |
| `control_emptyrate` | `0.25` | Con la zona vacía, el control sube a esta fracción (`0` = se congela, máx. `1`). |
| `control_grenades` | `0` | Granadas por jugador (`0` = ninguna, `-1` = sin límite). |
| `control_engineer` | `0` | `1` permite el ingeniero desde el inicio, con su equipo normal. |
| `control_engineer_at` | `85` | % de control de un equipo que desbloquea el ingeniero al rival hasta el fin de la ronda (`0` = nunca). |
| `control_engineer_rockets` | `3` | Cohetes en total del ingeniero desbloqueado, contando el cargado. |
| `control_engineer_grenades` | `1` | Granadas del ingeniero desbloqueado. |

Mapas con zona: `invade6`, `invade2`, `inland4`, `eurovilla`, `itadday3`,
`townwar`. Para marcar una zona nueva se usa el comando `spot` en consola
(ya existía) y la skill `agregar-mapa-control`.

## Servidor: airstrike kamikaze del Officer

Feature de chiste, apagada por defecto. Con `kamikaze_arty 1` (y `airstrikes 1`), el
Officer que llama el airstrike se sube al avión: la cámara viaja con él, el avión no
bombardea y pica hasta clavarse en el punto marcado con los binoculares. En la picada la
vista queda fija por la trompa. El Officer muere (cuenta como suicidio) y el choque hace daño
de airstrike a su nombre. Los aviones de los mapas (`misc_airstrike`) no cambian.

| Cvar | Por defecto | Qué hace |
|---|---|---|
| `kamikaze_arty` | `0` | `1` activa el kamikaze en el próximo airstrike (un vuelo ya en curso no cambia). |
| `kamikaze_dmg` | `1000` | Daño del choque (una bomba normal hace 700). |
| `kamikaze_radius` | `420` | Radio del choque (una bomba normal: 300). |

## Servidor: estadísticas

Detalle: [`../stats/README.md`](../stats/README.md).

| Cvar | Por defecto | Qué hace |
|---|---|---|
| `stats_log` | `0` | `1` escribe un archivo JSONL por partida (se aplica al cargar el siguiente mapa). |
| `stats_server` | hostname | Identificador del servidor en la base. |
| `stats_log_dir` | `""` | Carpeta de salida absoluta (vacío = `dday/stats/events`). |
| `stats_mode` | `public` | `public` o `duel` (en duelo solo cuenta desde `sv startcount`). |
| `stats_event` | `""` | Nombre del torneo: marca las partidas como duelo oficial. Vaciarlo al terminar. |

## Servidor: valores por defecto cambiados

| Cvar / constante | Antes | Ahora | Por qué |
|---|---|---|---|
| `fast_sniper` | `0` | `1` | Cambio de un colaborador (octubre de 2026). |
| `SWAY_START` / `SWAY_BREAK` / `SWAY_MULTI` | 5 / 5 / 4 | 2 / 2 / 1 | Menos sacudida de pantalla por explosiones. |
| Intensidad del efecto de daño | `/10` | `/15` | Más suave. |

## Ingestor de estadísticas (`stats/ingest.py`)

Detalle: [`../stats/README.md`](../stats/README.md) e
[`stats/INSTALACION.md`](stats/INSTALACION.md).

| Opción | Qué hace |
|---|---|
| `--dsn` / `DDAY_STATS_DSN` | Conexión a PostgreSQL. |
| `--events` / `DDAY_STATS_EVENTS` | Carpeta de los JSONL que escribe la DLL. |
| `--loop SEG` | Quedarse corriendo y revisar cada SEG segundos. |
| `--delete` | Borrar los archivos procesados en vez de moverlos. |
| `--stale-hours` | Horas tras las que un `.part` sin cerrar se considera abandonado (6). |
| `--init-schema` | Crear o actualizar tablas y vistas. |
| `--rebuild-ratings` | Recalcular todo el Elo desde cero. |
| `--rebuild-stats` | Recalcular las tablas de resumen. |
| `--new-season NOMBRE` [`--season-start FECHA`] | Empezar una temporada nueva. |
| `--set-event PARTIDA TORNEO` | Marcar una partida como de torneo (`''` = casual) y recalcular. |
| `--min-humans` / `--min-duration` | Mínimos para que una partida pública sea rankeada (2 por equipo, 5 min). |
| `--duel-min-humans` / `--duel-min-duration` | Lo mismo para duelos (1, 2 min). |
| `--min-participation` | Fracción mínima del tiempo para que el Elo afecte a un jugador. |
| `--bots-as-humans` | Contar bots como humanos (solo pruebas). |
| `--log-file ARCHIVO` / `-v` | Registro a archivo / detallado. |

## Bot admin por chat (`tools/chat-admin`)

Detalle: [`../tools/chat-admin/README.md`](../tools/chat-admin/README.md).
Solo responde a los nicks autorizados; la primera palabra del mensaje es el
comando.

- **Info:** `estado` `ayuda` `bans` `tiempo` `mapas` `infomapa` `marcador`
  `zona` `versiones` `lag` `tiempos`
- **Juego:** `mapa <x>` `duelo <mapa>` `publico` `normal` `control <mapa>`
  `dm <mapa>` `modo normal|control|dm|duelo [mapa]` `kamikaze on|off` `evento <nombre>` `cuenta`
  `reset` `resetscore` `torneo on|off` `freeze` `proximamapa` `lock` `unlock`
  `say <texto>`
- `control <mapa>` (o `modo control <mapa>`) deja solo ese mapa y **apaga la
  votación** mientras dure la sesión: las rondas se repiten en ese mapa.
- `mapa`, `dm`, `normal` y `publico` apagan el modo control y restauran la
  rotación (`sv_maplist`) y la votación (`mapvoting`) de `server.cfg`.
- `kamikaze on|off` prende o apaga el airstrike kamikaze del Officer (`kamikaze_arty`);
  sin argumento dice el estado. También entiende frases ("admin activa el kamikaze").
  Rige desde el próximo airstrike y un reinicio del server lo apaga.
- La votación de fin de mapa **nunca** lleva al modo control: el mapa votado
  siempre arranca en deathmatch (la DLL pone `control_mode 0`).
- **Moderación:** `kick <nick>` `kickban <nick>` `ban <ip>` `unban <ip>`
  `killjugador <nick>` `kickbots` `bots on|off` `reportar <quien> <motivo>`
- **q2pro:** `screenshot` `stuff <cmd>` `stuffid <id> <cmd>` `autostuff <cmd>`
  `autostuffoff` `liststuff`
- **Password:** `pass <nueva>` `passoff`
- **Con LLM:** `elo <nick>`

## Comandos del juego que ya existían y usamos

| Comando | Para qué |
|---|---|
| `spot` | Imprime la posición y el ángulo del jugador (marcar zonas de control). |
| `sv startcount` / `sv resetcount` | Cuenta regresiva de inicio / descartarla (duelos). |
| `sv freeze` | Pausar la partida. |
| `sv resetscore` | Poner en cero el puntaje de los equipos. |
