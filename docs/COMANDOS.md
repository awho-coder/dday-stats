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

## Juego: contador del airstrike y bonus del Officer (ZeRo)

Rama `officer-improvements` de ddaychile, integrada por `Cambios-Zero` el 2026-10-10:

- **Contador en el HUD** (sin cvar): mientras el equipo espera para volver a pedir un airstrike,
  el Officer ve los segundos que faltan junto al ícono "NEXT ARTY". Aparece después de confirmar
  el airstrike y hasta que se puede pedir otro. Necesita `pics/i_nextarty.pcx` en el server y en
  cada cliente (los jugadores lo bajan solos con `allow_download 1`).
- **Bonus por kill** (`officer_bonus`): cada kill de un Officer le resta 5 segundos a la espera
  del airstrike de su equipo, y el aviso de la muerte agrega "AIRSTRIKE COOLDOWN BONUS -5
  SECONDS". Cuentan todas sus kills, también las del propio airstrike.

| Cvar | Por defecto | Qué hace |
|---|---|---|
| `officer_bonus` | `0` | `1` activa el bonus de -5 segundos por kill del Officer. |

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
| `kamikaze_radius` | `700` | Radio del choque (una bomba normal: 300). Un anillo de explosiones a media distancia muestra el área. |

## Servidor: Medic nuevo (`medic_new`)

Apagado por defecto: con `medic_new 0` el Medic es el clásico (la pistola de su facción y
solo la jeringa). Con `medic_new 1` el Medic de todas las facciones lleva:

- la **Browning Hi-Power** en lugar de la pistola de su facción;
- **2 botiquines lanzables** por vida (`use special` y fuego): curan 25, el sangrado y la cojera al
  compañero que los toca; quedan **3 minutos** en el piso, también si el Medic muere (se borran si se
  desconecta), y cada Medic tiene a lo sumo 6 en el piso (uno nuevo borra el más viejo);
- **jeringas lanzables** (abajo);
- el **dardo de sueño** (ver "Servidor: dardo de sueño del Medic");
- **regeneración**: como ya no se cura con aim + fuego mientras tenga jeringas para lanzar,
  después de 3 segundos sin recibir daño recupera 10 de vida por segundo hasta 100
  (`MEDIC_REGEN_*` en `src/g_local.h`). No cura heridas: cada vez que el sangrado le hace daño,
  los 3 segundos vuelven a empezar.

Es latched, como `ffa`: `set medic_new 1` se aplica en el **próximo mapa**, y todo el mapa
tiene el mismo Medic. Desde el bot: `medic on|off` (sin argumento dice el estado).

### Jeringas lanzables

El Medic con la jeringa en la mano y en modo
apuntado (aim) la **lanza** al apretar fuego, igual que el cuchillo lanzado (misma velocidad
y cadencia, `SYRINGE_THROW_DELAY` en `src/g_local.h`). Al **compañero** que toca lo cura
`syringe_heal` (no cura sangrado ni cojera). Al **enemigo** lo envenena: `syringe_poison` de
daño repartido en `syringe_poison_time` segundos; los venenos de varias jeringas se acumulan, pueden matar y la
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
| `medic_new` | `0` | `1` activa el Medic nuevo (Browning, botiquín, jeringas y dardo de sueño) en el próximo mapa. |
| `syringe_count` | `9` | Jeringas lanzables por vida (sin contar la de la mano). Se reparten al aparecer. |
| `syringe_heal` | `50` | Vida que recupera el compañero (tope `HEALTH_MAX`). |
| `syringe_heal_head` | `100` | Vida que recupera el compañero si la jeringa le pega en la cabeza (tope `HEALTH_MAX`). |
| `syringe_poison` | `50` | Daño total del veneno de una jeringa, repartido en `syringe_poison_time`. |
| `syringe_poison_head` | `100` | Lo mismo cuando la jeringa le pega en la cabeza. |
| `syringe_poison_time` | `4` | Segundos en que se reparte el daño del veneno (el de cada jeringa queda fijo al pegar). |

Lo que se ponga por rcon se pierde al reiniciar el servidor; para dejarlo fijo va en
`dday/server.cfg`.

## Servidor: lanzajeringas del Medic (`medic_bazooka`)

Feature de chiste, apagada por defecto. Con `medic_bazooka 1` el Medic lleva, en lugar de su
pistola, una **bazooka que dispara 15 jeringas de una vez**, como un escopetazo. Cada jeringa es
una jeringa lanzada normal: cura al compañero y envenena al enemigo, con los mismos cvars
`syringe_*` (también el bonus de cabeza). Tiene **5 tiros por vida**, no se recarga, y se dispara
libre: de pie, caminando y sin apuntar, un tiro por segundo. Se ve como la bazooka de USA en
todas las facciones (modelos y sonidos que ya tiene todo el mundo). Funciona con o sin
`medic_new`. Es latched: se aplica en el **próximo mapa**. Desde el bot: `medic bazooka on|off`
(o "admin activa la bazooka de jeringas"). Constantes `SYRINGE_LAUNCHER_*` en `src/g_local.h`.

| Cvar | Por defecto | Qué hace |
|---|---|---|
| `medic_bazooka` | `0` | `1` le da el lanzajeringas al Medic en el próximo mapa. |

## Servidor: dardo de sueño del Medic (con `medic_new`)

Viene con el **Medic nuevo**: con `medic_new 1` (apagado por defecto) el Medic tiene además un
**dardo que duerme a un enemigo**, como el de Ana de Overwatch. Se dispara con el comando **`dardo`** (o `sleepdart`), que cada jugador
asigna a una tecla: `bind q dardo`. Solo se tira **con la jeringa en la mano**: hace la
animación de la jeringa (la de lanzar si está apuntando, la de inyectar si no) y el dardo sale
hacia donde apunta en ese momento. Con la jeringa en la mano ve la **cruz de Quake** en el
centro de la pantalla para apuntar (D-Day deja vacías las miras del cliente, `ch1` a `ch3`). Y
hay **uno cada `sleepdart_cooldown` segundos**: mientras espera, el HUD muestra los segundos que
faltan junto al ícono "NEXT DART" (como el contador del airstrike del Officer); si lo aprieta
antes avisa cuántos segundos faltan, y cuando se recarga llega el mensaje `Sleep dart ready.`. Al reaparecer está listo. Es un
proyectil (la jeringa con el líquido **violeta**, más rápida y **sin caída**: va recto como el de Ana, y deja una
**estela lavanda** chica): se puede esquivar. Quake 2 no tiene violeta en su paleta: quien baja la
textura del server (`.pcx`) y la estela lo ven lavanda; con el `.png` se ve violeta.

El **enemigo** que toca se cae de espaldas al piso (cuerpo a tierra, con una animación de
muerte) y queda dormido `sleepdart_time` segundos. La animación deja el cuerpo lejos de donde
estaba parado (hasta 100 unidades), así que antes de caer el server prueba hacia dónde entra sin
atravesar paredes ni a otros jugadores ni quedar colgando de un borde: hacia donde miraba, a los
costados o hacia atrás (en ángulo recto). Si no entra para ningún lado, duerme boca abajo en el
lugar. La caja que recibe los disparos va **donde está el cuerpo**; un tiro cerca de la cabeza
es headshot y el resto cuenta como torso (igual que cuerpo a tierra). Al despertarse queda
cuerpo a tierra donde estaba el cuerpo. Mientras duerme:

- no se mueve, no salta ni dispara, y la vista le queda fija mirando hacia arriba;
- no puede cambiar de arma, recargar, tirar cosas, cambiar de postura, pedir airstrike ni
  suicidarse (`kill`); el chat, los menús y el TAB siguen andando;
- ve la pantalla azul oscura (se aclara en el último medio segundo);
- le salen **Z que suben** de la cabeza (un sprite que crece) y **ronca**; el ronquido se
  oye solo cerca de él y se corta cuando se despierta;
- **cualquier daño lo despierta** (con `sleepdart_wake 1`), salvo el sangrado de sus propias
  heridas. Se despierta cuerpo a tierra.
- si **lo matan dormido, explota en pedazos** (sangre y gibs, como con una granada) desde donde
  está el cuerpo, para que se note.

A los **compañeros** no les hace nada (el dardo se rompe), tampoco a quien tiene la protección
de spawn, al Medic invulnerable (`invuln_medic`) ni durante la cuenta de un torneo. En FFA
duerme a cualquiera. Los bots también se duermen, pero no usan el dardo. Como `medic_new` es
latched, se prende y se apaga en el **próximo mapa**; los `sleepdart_*` rigen al instante.

**Archivos nuevos** (hay que subirlos al server; los jugadores los bajan solos con
`allow_download 1`): `sprites/zzz.sp2`, `sprites/zzz_0.pcx`, `sprites/zzz_0.png`,
`sound/misc/snore.wav`, `pics/i_nextdart.pcx`, `pics/dartcross.pcx` y
`models/weapons/g_sleepdart/` (`tris.md2`, `skin.pcx` y `skin.png`). Están en el repo y los genera `tools/assets/gen_sleep_assets.py`.
El server los precachea solo con `medic_new 1`, al empezar el mapa.
Desde el bot: `medic on|off` (`dardo on|off` y "admin activa el dardo" hacen lo mismo).
Constantes `SLEEPDART_*` y `SLEEP_*` en `src/g_local.h`.

| Cvar | Por defecto | Qué hace |
|---|---|---|
| `sleepdart_time` | `5` | Segundos que duerme el enemigo. |
| `sleepdart_cooldown` | `15` | Segundos entre dos dardos del mismo Medic. |
| `sleepdart_wake` | `1` | `1` = cualquier daño lo despierta; `0` = duerme todo el tiempo aunque le peguen. |
| `sleepdart_anim` | `1` | `1`, `2` o `3`: cuál de las tres animaciones de muerte de pie usa para caerse. |

Para probarlo con bots: `bot_noshoot 1` (en `0` por defecto) hace que los bots no disparen.

## Servidor: TAB de equipos nuevo (`tab_panel`)

Apagado por defecto: con `tab_panel 0` el TAB de equipos es el de siempre. Con `tab_panel 1` usa el
panel del Free For All (`pics/ffa_score.png`) con los dos equipos uno sobre otro: la bandera de cada
bando (`pics/teams/<teamid>`) junto a su título (nombre, kills y puntos), y hasta 16 jugadores en
total (8 por equipo si hay más; la última fila dice "and N more"). No es latched: rige al instante.

- **Página 1** (primer TAB): `Ping`, `Player` (con `+` si es Medic), `Pts`, `+/-` (kills menos
  muertes de las stats) y `Cura`. Tu fila se ve resaltada.
- **Página 2** (segundo TAB): `Ping`, `Player`, rating (con `stats 1`) o precisión (`Acc%`), `Hit` y `Shot`.
- **Cura:** la vida que el Medic devolvió a sus compañeros en el mapa, con la jeringa en la mano, la
  lanzada y el botiquín (no cuenta la autocuración ni curar a alguien que ya estaba en 100). Va en
  blanco si es 0 y en miles desde 1000 (`1k`, `12k`). Siempre se cuenta, pero solo se ve con `tab_panel 1`.
- Al final del mapa el cartel de victoria va sobre el panel. El marcador de espectadores no cambia.
- Desde el bot: `tab nuevo on|off` (o "admin activa el tab nuevo"); sin argumento dice el estado.

| Cvar | Por defecto | Qué hace |
|---|---|---|
| `tab_panel` | `0` | `1` activa el TAB de equipos nuevo (panel del FFA, banderas, columna Cura). |

## Servidor: estadísticas

Detalle: [`../stats/README.md`](../stats/README.md).

| Cvar | Por defecto | Qué hace |
|---|---|---|
| `stats_log` | `0` | `1` escribe un archivo JSONL por partida (se aplica al cargar el siguiente mapa). |
| `stats_server` | hostname | Identificador del servidor en la base. |
| `stats_log_dir` | `""` | Carpeta de salida absoluta (vacío = `dday/stats/events`). |
| `stats_mode` | `public` | `public` o `duel` (en duelo solo cuenta desde `sv startcount`). |
| `stats_event` | `""` | Nombre del torneo: marca las partidas como duelo oficial. Vaciarlo al terminar. |

## Servidor: MVD de la partida (para bajarlo desde la web)

Graba con el MVD de Q2PRO (`sv_mvd_enable 1` en `server.cfg`) y le pone de nombre
el id de la partida de las stats: `demos/<match_id>.mvd2.gz`. Hace falta `stats_log 1`.
Se corta solo al cambiar de mapa (`sv_mvd_maxmaps 1`). Sin jugadores humanos
Q2PRO no graba nada (los bots no cuentan).

| Cvar / comando | Por defecto | Qué hace |
|---|---|---|
| `mvd_partidas` | `0` | `1` graba un MVD de cada partida, desde la actual. Al instante; un reinicio lo apaga. |
| `sv mvd_partida` | — | Graba solo la partida en curso, desde ese momento. |

Un `mvdrecord` manual con otro nombre sigue funcionando, pero esa grabación no sale en la web.

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
  `ffa <mapa>` `dm <mapa>` `modo normal|control|ffa|dm|duelo [mapa]` `kamikaze on|off` `tab nuevo on|off` `dardo on|off` `grabar on|off|ahora` `evento <nombre>` `cuenta [minutos]`
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
- `dardo on|off` es lo mismo que `medic on|off`: el dardo de sueño viene con el Medic nuevo
  (`medic_new`, próximo mapa). También entiende frases ("admin activa el dardo").
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
