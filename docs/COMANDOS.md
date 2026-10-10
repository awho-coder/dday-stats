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

## Servidor: Free For All (todos contra todos)

Rama `free-for-all-mod` de ddaychile. Apagado por defecto; con `ffa 0` el juego es el D-Day de siempre.

| Cvar | Por defecto | Qué hace |
|---|---|---|
| `ffa` | `0` | `1` activa el modo (latched: se aplica con el próximo `map`). Requiere `deathmatch 1` y no se mezcla con el modo control ni con CTB. |

Todos entran como Sniper (rifle + cuchillo), sin equipos ni fuego amigo, con spawns al azar
lejos de los demás, puntaje individual, marcador propio y anuncios del líder. Termina por
`fraglimit` (30 si el server usa 0) o `timelimit`. No funciona junto con `tournament` y las
stats persistentes (`stats 1`) no lo distinguen. Desde el bot: `ffa <mapa>`.

## Juego: cambios de clases y armas

Ramas de ddaychile integradas el 2026-10-09. Las dos del Medic van detrás de `medic_new`
(ver [Medic nuevo](#servidor-medic-nuevo-medic_new)):

- **Medic: Browning Hi-Power** (`browning-medic`): reemplaza a la pistola del Medic en todas
  las facciones. Automática, 10 balas, 5 disparos por segundo, 17 de daño; el spread crece
  con cada disparo de la ráfaga. Constantes `BROWNING_*` en `src/g_local.h`.
- **Medic: botiquín lanzable** (`medic-healthpack`): 2 por vida, se usa con `use special`
  y fuego; cura 25 al compañero que lo toma. Necesita los modelos y texturas de
  [`../models/README.md`](../models/README.md) en el server y en cada cliente.
- **BAR y MP43** (sin cvar) (`lmg-recoil-spread`): el retroceso y el spread siguen subiendo mientras se
  mantiene el gatillo (`LMG_BLOOM_*` en `src/g_local.h`) y la BAR lleva 7 cargadores.

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

## Servidor: Medic nuevo (`medic_new`)

Apagado por defecto: con `medic_new 0` el Medic es el clásico (la pistola de su facción y
solo la jeringa). Con `medic_new 1` el Medic de todas las facciones lleva:

- la **Browning Hi-Power** en lugar de la pistola de su facción;
- **2 botiquines lanzables** por vida (`use special` y fuego);
- **jeringas lanzables** (abajo).

Es latched, como `ffa`: `set medic_new 1` se aplica en el **próximo mapa**, y todo el mapa
tiene el mismo Medic. Desde el bot: `medic on|off` (sin argumento dice el estado).

### Jeringas lanzables

El Medic con la jeringa en la mano y en modo
apuntado (aim) la **lanza** al apretar fuego, igual que el cuchillo lanzado (misma velocidad
y cadencia, `SYRINGE_THROW_DELAY` en `src/g_local.h`). Al **compañero** que toca lo cura
`syringe_heal` (no cura sangrado ni cojera). Al **enemigo** lo envenena: `syringe_poison` de
daño repartido en 2 segundos; los venenos de varias jeringas se acumulan, pueden matar y la
muerte cuenta como kill del Medic (`was poisoned by`, `syringe` en el log de stats). Contra
una pared, un objetivo o un cadáver la jeringa se rompe. Lleva `syringe_count` jeringas
lanzables por vida (el HUD muestra cuántas le quedan), además de la de la mano, que es
infinita y sigue sirviendo para inyectar de cerca. El Medic se puede mover mientras lanza
(aim + cuerpo a tierra lo deja quieto, como con el cuchillo).

Saltar no lo saca del apuntado mientras le queden jeringas, igual que con el cuchillo.

El modelo de la jeringa en vuelo es `models/weapons/g_syringe/tris.md2` (ver
[`../models/README.md`](../models/README.md)): hay que subirlo al server, y los jugadores lo
bajan solos con `allow_download 1`. Sin jeringas lanzables, con un bot o con
`invuln_medic` distinto de 0, aim + fuego sigue autocurando como siempre.

| Cvar | Por defecto | Qué hace |
|---|---|---|
| `medic_new` | `0` | `1` activa el Medic nuevo (Browning, botiquín y jeringas) en el próximo mapa. |
| `syringe_count` | `9` | Jeringas lanzables por vida (sin contar la de la mano). Se reparten al aparecer. |
| `syringe_heal` | `33` | Vida que recupera el compañero (tope `HEALTH_MAX`). |
| `syringe_poison` | `33` | Daño total del veneno de una jeringa, en 2 segundos. |

Lo que se ponga por rcon se pierde al reiniciar el servidor; para dejarlo fijo va en
`dday/server.cfg`.

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
  `ffa <mapa>` `dm <mapa>` `modo normal|control|ffa|dm|duelo [mapa]` `kamikaze on|off` `evento <nombre>` `cuenta`
  `reset` `resetscore` `torneo on|off` `freeze` `proximamapa` `lock` `unlock`
  `say <texto>`
- `control <mapa>` (o `modo control <mapa>`) deja solo ese mapa y **apaga la
  votación** mientras dure la sesión: las rondas se repiten en ese mapa.
- `ffa <mapa>` (o `ffa` solo = el mapa actual; también en frase: "admin quiero probar
  el ffa en dust") arranca Free For All en ese mapa y, como `control`, apaga la votación.
- `mapa`, `dm`, `normal` y `publico` apagan el modo control y el FFA, y restauran la
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
