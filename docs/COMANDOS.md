# Comandos y cvars agregados

Referencia rápida de todo lo que este fork agregó o cambió respecto del
original (`pcarmona79`, hasta abril de 2026). El detalle de cada tema está en
el documento enlazado en cada sección.

> Al agregar un cvar, comando u opción nueva, sumarlo aquí.

## Servidor: modos especiales, rotación y nombre del server

Modos que activa un admin (bot del chat o rcon): **control de zona** (`control_mode 1`),
**Free For All** (`ffa 1`) y **Gun Game** (`ffa 1` + `ffa_gungame 1`). Cada uno se juega en los
mapas **preparados** para él:

| Modo | Mapa preparado si existe | Mapas hoy |
|---|---|---|
| Control | `ents/<mapa>.ctl` | eurovilla, inland4, invade2, invade6, itadday3, townwar |
| FFA / Gun Game | `ents/<mapa>_ffa.ent` | dday2, gg_rats, inferno, tebessa2 |

- **Rotación:** al terminar un mapa, la DLL pasa al siguiente mapa preparado (orden alfabético,
  vuelve al primero) y no abre la votación de fin de mapa. Si el modo arrancó en un mapa sin
  preparar, sigue con el primero preparado. Sin ningún mapa preparado se usa `sv_maplist`.
- **Votación de los jugadores:** `vote map <mapa>` (igual que `vote normal <mapa>`) **siempre** juega el
  mapa en modo normal, aunque sea un mapa del modo (el aviso dice "(vuelve al modo normal)"; al
  aprobarse vuelven también `mapvoting` y `sv_maplist` del `server.cfg`, como `normal` del bot).
  Para jugar un modo se vota con su comando: `vote gungame <mapa>`, `vote control <mapa>` o
  `vote ffa <mapa>`, solo con mapas preparados para ese modo y si el modo está en `vote_modes`
  (por defecto ninguno; S1 tiene `"gungame"`). Al aprobarse corta el mapa al tiro y después todo
  sigue como siempre. `vote maps gungame` lista los mapas de ese modo.
- **Agregar un mapa a un modo:** subir su `.ctl` o `_ffa.ent` a `ents/` del server (sin reinicio).
- **Nombre del server** (`hostname_mode 1`, 0 por defecto; en Oracle va en `server.cfg`): al cargar
  cada mapa la palabra "Ladder" del `hostname` cambia por el modo: `D-Day Chile FFA Test`,
  `D-Day Chile Gungame Test` o `D-Day Chile Control Test` (control solo en mapas con `.ctl`). En el
  juego normal vuelve al original (`hostname_base`). Si se cambia `hostname` a mano, ese pasa a ser
  el original.
- **Volver al juego normal:** `normal`, `mapa <x>` o `dm` en el bot (restauran `sv_maplist` y
  `mapvoting` de `server.cfg`).
- **Votación de fin de mapa por secciones** (`mapvoting_modes 1`, 0 por defecto): el menú trae
  3 mapas normales, 2 de control y 2 de Gun Game, y el ganador define el modo del mapa siguiente.
  Cada sección se apaga con su cvar en 0 (`mapvoting_normal`, `mapvoting_control`,
  `mapvoting_gungame`, `mapvoting_ffa`). Detalle en [`VOTACION.md`](VOTACION.md).

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

Todos entran como Sniper (rifle + cuchillo), sin equipos ni fuego amigo, con spawns al azar lejos de los demás, puntaje individual,
marcador propio y anuncios del líder. Termina por `fraglimit` (30 si el server usa 0) o
`timelimit`. No funciona junto con `tournament` y las stats persistentes (`stats 1`) no lo
distinguen. Desde el bot: `ffa <mapa>`. Mapas preparados: dday2, tebessa2 (spawns extra) y gg_rats (mapa
hecho para Gun Game, con jump pad y sin daño de caída), ver
[`ents/README.md`](../ents/README.md).

**Ruleta de armas** (`ffa_roulette 1`, apagada por defecto; rama `ffa-gungame` de ddaychile, 2026-10-31): en vez del Sniper, al empezar cada mapa se sortea
una sola arma para todos entre las de Infantry, Officer, Light/Heavy Gunner, Sniper y Special de
los dos ejércitos del mapa, más la pistola de cada uno (partida de pistolas, con 6 cargadores).
Durante la pausa inicial (10 s) el HUD muestra una ruleta (`pics/ffa_w_*.png`). No repite el arma
del mapa anterior. En el mapa solo quedan esa arma, su munición y el cuchillo.

### Gun Game (`ffa_gungame`)

Con `ffa 1` y `ffa_gungame 1`, cada kill te da la siguiente arma de una escalera; gana el
primero que mata en el último escalón, con lo que sea: cuchillo, puños, casco o
desangrado (no hay fraglimit). No cuentan las kills con un arma de un escalón anterior: una
granada que seguía en el aire al subir, o el desangrado de una herida hecha antes de llegar
al último escalón. Cada kill que te sube de arma te cura el sangrado y la cojera
(no la vida). La escalera se arma al
azar en cada mapa con armas de las 8 facciones (sin lanzallamas ni molotov), sin repetir el tipo
de arma ni el ejército de un escalón al siguiente. Bajas un escalón si te suicidas, si te matan
con cuchillo o tras varias muertes seguidas sin matar. Quien entra tarde parte en el escalón
más bajo de los que juegan. No hay armas en el piso ni se pueden tirar. El HUD muestra LEADER
y LEVEL, el TAB una columna `Lvl` y al final el banner del ganador.

| Cvar | Por defecto | Qué hace |
|---|---|---|
| `ffa_gungame` | `0` | `1` activa el Gun Game (latched, con `ffa 1`). |
| `ffa_roulette` | `0` | FFA sin Gun Game: `1` = un arma sorteada por mapa con ruleta; `0` = todos Sniper (latched). |
| `ffa_gg_steps` | `29` | Armas antes del cuchillo (30 escalones con él; `0` = todas las que haya). |
| `ffa_gg_factions` | `1` | `1` = armas de las 8 facciones; `0` = solo las de los dos ejércitos del mapa. |
| `ffa_gg_deaths` | `0` | Muertes seguidas sin matar para bajar un escalón (`0` = un quinto de los escalones, mínimo 2; negativo = nunca). |
| `ffa_gg_snd_up` | `ffa/gg_up2.wav` | Sonido al subir (ruta bajo `sound/`). |
| `ffa_gg_snd_down` | `misc/menu3.wav` | Sonido al bajar. |
| `ffa_gg_victory_vol` | 0.4 | Volumen del himno de victoria (0-1; suena 3 veces encima, 0 = sin himno). |
| `ffa_gg_leader_level` | 0 | Desde este escalón el líder (o los empatados arriba) brilla con un aura dorada que parpadea lento y una luz amarilla, visibles solo al mirarlo. 0 = apagado. En Oracle está en 25. |

Si en un mapa no caben todos los modelos o sonidos (Quake 2 admite 256 de cada uno), la escalera
queda más corta y el log del server lo dice (`Gun Game: N weapons left out`). En las stats la
partida sigue siendo `ffa`, con `"variant":"gungame"` en `match_start`, el ganador y los puestos
por escalón y `gg_level` en cada jugador. Desde el bot: `gungame <mapa>` (o en frase: "admin pon
el gun game en dday2"). Archivos para los clientes: `pics/ffa_gungame.png`,
`pics/ffa_gungame_win.png`, `pics/ffa_w_*.png` y `sound/ffa/gg_up*.wav` (se descargan del server).

### Entidades para mapas: jump pad y sin daño de caída

Dos agregados que se usan desde un `.ent` (por ejemplo un `_ffa.ent`), sin tocar el `.bsp`:

- **`"nofalldamage" "1"` en el `worldspawn`:** en ese mapa la caída no hace daño, ni al que cae
  ni al que está abajo (`level.nofalldamage`, `P_FallingDamage`). Los demás mapas no cambian.
- **`trigger_jumppad`** (entidad de punto, `g_trigger.c`): una plataforma sólida de 256x256 y 10
  de alto (se sube caminando) que lanza hacia arriba al que la pisa. Mantiene la velocidad
  horizontal: el que entra corriendo sale en esa dirección. El anillo del centro pulsa, salen
  partículas y suena `sound/misc/jumppad.wav`.

  ```
  {
  "classname" "trigger_jumppad"
  "origin" "48 -224 -640"   // centro, a ras de piso
  "speed" "1300"            // velocidad hacia arriba (default 1000); sube unas speed²/1600 unidades
  }
  ```

  `"spawnflags"` cambia el tipo: `1` = plataforma chica (96x96, `models/objects/jumppad_small/`), `2` = **diagonal**:
  una flecha plana no sólida (`models/objects/jumppad_arrow/`, franjas amarillas y negras) que apunta hacia
  `"angle"` y lanza a 45° en esa dirección. `speed` es la velocidad total (default 800; el alcance en plano
  es `speed²/800`, 894 = ~1000 unidades) y reemplaza la velocidad horizontal con la que se entra. El ángulo y la
  posición salen de un `spot` marcado en el juego (`origin` = la z del `spot` menos 24).

  `"angle"` gira el modelo (la caja de colisión no gira: usar 0 o múltiplos de 90). Antes de
  ponerlo, revisar que el techo quede más arriba que el salto. Archivos para los clientes:
  `models/objects/jumppad/` (generado con `tools/assets/gen_jumppad.py`) y
  `sound/misc/jumppad.wav`.

Los usa **gg_rats** ("Rats", una cocina gigante), que tiene `nofalldamage` y un jump pad al centro
del piso (speed 1300, ~1050 unidades). El mapa trae su ascensor (`func_plat`), que usa
`sound/plats/pt1_*.wav`: en el cliente y en Oracle esos archivos eran una página HTML guardada como
`.wav`. Ahora el repo trae unos sintetizados con ffmpeg.

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

## Servidor: tornado (`tornado`)

Otra feature de chiste, apagada por defecto. Un remolino de arena (nubes de polvo y escombros
girando en embudo, con viento) recorre el mapa: va de un destino lejano a otro por los nodos de
los bots (`navigation/<mapa>.nav`; a veces el destino es donde hay un jugador). En mapas sin
nodos va de un punto de aparición a otro bordeando las paredes. A quien agarra lo sube girando,
arriba le da `tornado_spins` vueltas y lo lanza hacia afuera. **La caída no hace daño**: el seguro
dura hasta que el jugador vuelve a tocar el piso. No agarra a muertos, espectadores, pilotos kamikaze ni a los que están en una torreta.
Usa archivos que ya trae el pak0 (`sprites/null.sp2`, `debris1/2`) y `sound/wind2x.wav`, así que
no hay que descargar nada.

- `sv tornado` hace aparecer uno ahora, en cualquier mapa, en un lugar al azar al aire libre (sobre los nodos de los bots; en mapas sin nodos, cerca de un jugador al azar), lejos de los jugadores;
  `sv tornado <id>` lo pone cerca de ese jugador; `sv tornado off` saca los que haya. Máximo 2.

| Cvar | Por defecto | Qué hace |
|---|---|---|
| `tornado` | `0` | `1` = aparece solo cada `tornado_interval` segundos en los mapas de `tornado_maps`. |
| `tornado_gungame` | `0` | `1` = lo mismo, pero solo cuando hay Gun Game (`ffa_gungame 1`). En Oracle va en 1 en `server.cfg`. |
| `tornado_maps` | `tebessa tebessa2` | Mapas con tornado automático, separados por espacio (`*` = todos). |
| `tornado_interval` | `150` | Máximo de segundos entre tornados; se sortea entre la mitad y ese valor (90 = cada 45–90 s, lo que usa Oracle). |
| `tornado_time` | `40` | Segundos que dura cada tornado. |
| `tornado_radius` | `300` | Radio en que agarra a los jugadores (y ancho del embudo). |
| `tornado_height` | `1000` | Alto del embudo (baja solo si hay cielo o techo más abajo). |
| `tornado_speed` | `160` | Velocidad con que anda. |
| `tornado_lift` | `400` | Velocidad de subida del jugador. |
| `tornado_spins` | `2` | Vueltas que le da arriba antes de soltarlo. |
| `tornado_fling` | `900` | Fuerza con que lo lanza al soltarlo. |

`navigation/tebessa2.nav` es una copia de `tebessa.nav`: el mapa tiene la misma estructura (de 269
nodos, solo 1 quedó dentro de una pared y 5 de 1021 enlaces chocan con algo nuevo). Además de
guiar al tornado, deja entrar bots en tebessa2.

## Servidor: Medic nuevo (`medic_new`)

Apagado por defecto: con `medic_new 0` el Medic es el clásico (la pistola de su facción y
solo la jeringa). Con `medic_new 1` el Medic de todas las facciones lleva:

- la **Browning Hi-Power** en lugar de la pistola de su facción;
- **2 botiquines lanzables** por vida (`use special` y fuego): curan 25, el sangrado, la cojera y el
  veneno al compañero que los toca, y también al mismo Medic (pasado 1 segundo desde que lo lanza); quedan **3 minutos** en el piso, también si el Medic muere (se borran si se
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
muerte cuenta como kill del Medic (`was poisoned by`, `syringe` en el log de stats). Un
**Medic de su equipo le quita todo el veneno de una vez**, con una jeringa lanzada o
inyectándolo de cerca (el daño ya hecho no se devuelve). Contra
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
lugar. Si el dardo le pega **en el aire** (saltando o cayendo), la animación parte ahí mismo y
sigue cayendo con la velocidad que traía; al tocar el piso se revisa si el cuerpo entra donde
cayó (si no, gira hacia otro lado o queda boca abajo), y no se despierta antes de aterrizar.
La caja que recibe los disparos va **donde está el cuerpo**; un tiro cerca de la cabeza
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

## Servidor: hitmarkers (`hitmarkers`)

Apagado por defecto. Con `hitmarkers 0` no hay hitmarkers para nadie. Con cualquier otro valor, **cada jugador
elige** lo que quiere con el comando `hitmarker <numero>` (también `hitmarkers`); quien no elige no recibe nada.
Cuando le pegas a un enemigo, el server te manda un sonido solo a ti (y a los espectadores que te siguen, cada uno
con lo que eligió). No se marcan los golpes a compañeros, ni el sangrado de las heridas, ni los golpes a cuerpos.
Tampoco los ataques de los bots.

| `hitmarker` (jugador) | Qué recibe |
|---|---|
| `0` | Nada (por defecto). |
| `1` | Todo: sonido al pegar o matar (`sound/misc/hitmark.wav`), otro en un headshot (`hitmark_head.wav`) y una X en el centro: blanca al pegar (`pics/hitmark.pcx`, 0,2 s) y roja al matar (`pics/hitkill.pcx`, 0,4 s). |
| `2` | Solo los sonidos. |
| `3` | Solo la X. |
| `4` | Solo al matar: el sonido y la X roja en las kills y los headshots; los golpes que no matan no marcan nada. |

`hitmarker` sin número muestra la lista y cuál tiene. La elección se guarda en el juego del jugador (`seta hm`,
userinfo `hmu`) y en el server por nombre, en `dday/hitmarkers.txt` (el `0` no se escribe). Ver "Elecciones de
cada jugador" más abajo.

En un headshot, `hitmark_head.wav` reemplaza la voz "HEADSHOT" del juego (`misc/hithead.wav`) para quienes eligieron
sonido (`1`, `2` o `4`): los que están cerca lo oyen en la cabeza del muerto, como se oía la voz, y quien disparó (y
quien lo sigue) lo oye además a todo volumen sin importar la distancia (una sola vez, aunque esté cerca). Los demás
oyen la voz de siempre. Los headshots de los bots o entre compañeros también van así, solo en la cabeza del muerto.

No hace falta cambiar de mapa: el server los puede prender y apagar en cualquier momento (los archivos se
precargan siempre). La X usa el mismo lugar del HUD que las miras (`STAT_CROSSHAIR`), así que con la mira telescópica, los binoculares,
la torreta o la mira del dardo solo suena. Suena una vez por cuadro (los perdigones de la escopeta o una granada
que pega a varios suenan una vez; si en ese cuadro un perdigón pega y otro mata, quien eligió `4` oye la kill). La LMG, la SMG y la Browning no hacen headshot mientras el enemigo tenga más
de 30 de vida (cuenta como golpe en el pecho).

`pics/hitmark.pcx` y `pics/hitkill.pcx` salen de `tools/assets/gen_hitmarker.py`. `hitmark.wav` y
`hitmark_head.wav` son archivos aparte, pasados a 22050 Hz mono (al del headshot también se le quitó el silencio del
final y se comprimió para que suene fuerte). Constantes `HITMARK_*` y `HITHEAD_SOUND` en `src/g_local.h`.

## Jugador: killfeed (`killfeed`)

Cada jugador elige cómo ve las muertes con `killfeed <0-6>` (`killfeed` sin número muestra la lista). `0` = en
texto, como siempre; `1` a `6` = un feed gráfico arriba a la derecha (imágenes del pak `killfeed2.pkz`, que Q2PRO baja
por HTTP). El cvar `killfeed_style` del server es el estilo de quien no eligió (`0` por defecto). Se guarda en el juego
del jugador (`seta kf`, userinfo `kfu`) y en el server por nombre, en `dday/killfeed.txt` (aquí el `0` sí se escribe:
es una elección aunque el server tenga otro estilo por defecto). Sigue leyendo `kf_style` del userinfo de antes.

## Elecciones de cada jugador (hitmarkers y killfeed)

Lo que cada jugador elige con `hitmarker` y `killfeed` se guarda en dos lugares (`PlayerPref_*` en `src/g_utils.c`):

- **En su juego**: el server le manda `set <u> x<n> u` y `seta <cvar> <n>` (`hmu`/`hm` y `kfu`/`kf`). Con Q2PRO, el
  cvar queda en su `config.cfg` al cerrar el juego. Al entrar a un server, este le pide `set <u> x$<cvar> u`: el
  cliente pone su valor en lugar de `$<cvar>` y lo manda en el userinfo. Así la elección pasa de S1 a S2 y sigue
  aunque cambie de nombre.
- **En el server, por nombre** (sin distinguir mayúsculas), en `dday/hitmarkers.txt` y `dday/killfeed.txt`: una línea
  `<n> <nombre>` por jugador. Sirve para clientes que no guardan el cvar (r1q2, 3.20) o si juega desde otro PC.

Manda lo del cliente (y se copia al archivo de ese server); si el cliente no trae nada, se usa el archivo. La "x" es
para que el valor nunca llegue vacío (Q2PRO no reemplaza `$cvar` entre comillas y deja vacío lo que no existe).

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
| `tab_final` | `1` | Pantalla final (ver abajo). `0` = al terminar sale el TAB de siempre. |

### Pantalla final (`tab_final`)

Al terminar una partida por equipos (no FFA ni Gun Game ni campaña), en vez del TAB sale una pantalla propia
con el panel de Snako, para jugadores y espectadores, tenga `tab_panel` lo que tenga:

- Arriba el cartel de victoria; en la placa de bronce la duración (sin el calentamiento ni las pausas del duelo,
  sin los ratos sin nadie; necesita `stats_log 1`).
- Línea del resultado: "Gana USA 25-18 por kills" (también por puntos, por objetivos, por zona o por tiempo;
  en control y CTB el marcador es de puntos), "Empate 18-18" o "Fin de la partida" (mapa votado).
- Por equipo la bandera y su título, y por jugador K, M, +/-, precisión y puntos (Pts solo con
  `player_scores 1`); tu fila en naranjo, `+` = Medic. Nombres cortados a 13 letras.
- Abajo los destacados: MVP (más kills; a igualdad, menos muertes), mejor puntería (con 20 tiros o más) y más
  curación (si algún Medic curó).
- Caben unos 12 jugadores en total; con más, la última fila de cada equipo dice "y N mas" (se ven los de más puntos).
- En duelo, las kills, muertes, precisión y curación del calentamiento no cuentan (se borran al empezar).
- La segunda página del TAB (segunda pulsación) sigue igual.

## Servidor: estadísticas

Detalle: [`../stats/README.md`](../stats/README.md).

| Cvar | Por defecto | Qué hace |
|---|---|---|
| `stats_log` | `0` | `1` escribe un archivo JSONL por partida (se aplica al cargar el siguiente mapa). |
| `stats_server` | hostname | Identificador del servidor en la base. |
| `stats_log_dir` | `""` | Carpeta de salida absoluta (vacío = `dday/stats/events`). |
| `stats_mode` | `public` | `public` o `duel` (en duelo solo cuenta desde que termina la cuenta: `sv startcount` o el ready de los capitanes). |
| `stats_event` | `""` | Clave del torneo: marca las partidas como oficiales. Lo pone `sv torneo` (ver abajo). |
| `stats_event_name` | `""` | Nombre largo del torneo, solo para mostrar (mensaje de espera, inicio, pantalla final). Lo pone `sv torneo`. |

## Servidor: duelo con ready de los capitanes (`ready_mode`)

En duelo (`stats_mode duel`) o torneo (`tournament 1`) la partida empieza sola cuando el capitán de
cada equipo pone `ready`, sin admin. Mientras se espera, cada 10 s el centro de la pantalla explica qué
hacer y muestra el estado de cada equipo. Cuando los dos están listos sale la cuenta de siempre
("Preparados! 3 2 1 Que comience el juego!"). Al empezar un duelo se borran las kills del
calentamiento (antes contaban para el ganador).

| Cvar / comando | Por defecto | Qué hace |
|---|---|---|
| `ready_mode` | `0` | `1` = en duelo o torneo la partida empieza con el ready de los dos capitanes. |
| `ready_time` | `0` | Minutos de la partida. `0` = dura hasta que gane un equipo (también en torneo: el daño se habilita cuando empieza la partida, aunque no tenga tiempo límite). |
| `capitan` / `captain` | — | (Jugador) queda de capitán de su equipo, uno por equipo. También en el menú de equipos: "<equipo> (capitan)". |
| `ready` / `listo` | — | (Capitán) su equipo está listo. Si el equipo no tiene capitán, quien lo escribe queda de capitán. |
| `notready` / `unready` / `nolisto` | — | (Capitán) lo desmarca. |
| `ready_remind` | `60` | Si un equipo está listo y el otro no, cada tantos segundos se le recuerda al otro (chat y centro). `0` = sin aviso. |
| `pausa` / `pause` / `timeout` | — | (Capitán, partida en juego) pausa: nadie se mueve ni recibe daño y el reloj se detiene. |
| `seguir` / `continuar` / `unpause` | — | (Los dos capitanes) sigue antes de que se acabe la pausa, con una cuenta de 5 s. |
| `pause_max` | `2` | Pausas de cada equipo por partida. `0` = sin pausas de capitanes. |
| `pause_time` | `120` | Segundos que dura la pausa de un capitán como máximo (después sigue sola). La pausa automática no tiene límite. |
| `pause_auto` | `1` | `1` = si se desconecta un jugador de un equipo, pausa hasta que vuelva, sin límite de tiempo (si no vuelve, `seguir` de los dos capitanes). No cuenta en `pause_max`. |
| `cancelar` / `cancel` | — | (Los dos capitanes, en menos de 30 s) cancelan la partida, como `sv resetcount`: vuelve el calentamiento y el ready. |
| `sv ready` / `sv ready reset` / `sv ready seguir` | — | Estado (capitanes, ready, humanos, kills, pausas) / borrar capitanes y ready / sacar la pausa de los capitanes. |

Los comandos sirven en la consola y en el chat (el mensaje exacto, con o sin `!`, también en el chat de
equipo; el mensaje se ve igual). En la consola **no** usar `pause`: es un comando del cliente de Quake 2 y no
llega al server; usar `pausa` o `timeout` (en el chat `pause` sí sirve). Si el capitán se va
del equipo o del server, la capitanía pasa altiro a otro jugador del equipo (se le avisa en el centro de la
pantalla; antes de empezar, el ready del equipo se borra y el nuevo capitán lo vuelve a poner). Si no queda nadie,
el próximo que escriba un comando de capitán lo será.
`sv startcount` del admin sigue funcionando y se salta el ready; `sv resetcount` vuelve a pedirlo.

Ya empezada la partida, el jugador de un equipo que se desconecta entra solo a su mismo equipo al volver
(con el mismo nombre) y con la clase que tenía (si hay cupo; el menú de clase se abre por si quiere otra). Con `pause_auto 1` la partida queda en pausa
hasta que vuelva: al entrar a su equipo el server avisa "Volvio el que se habia caido" y la partida sigue sola
tras una cuenta de 5 s (nadie pone ready; la cuenta empieza al entrar al equipo, así que tiene esos 5 s para
elegir clase). La pausa automática no tiene límite de tiempo: si no vuelve, los dos capitanes escriben `seguir` (si su equipo
quedó vacío, basta el otro capitán) o el admin `sv ready seguir`. Si
vuelve con otro nombre no se le reconoce. Un echado con `vote kick` no pausa ni vuelve solo.
Durante la pausa nadie se mueve; el tiempo límite, los objetivos y la zona de
control no corren y las stats del duelo no cuentan ese rato.

Los espectadores usan el chat de equipo (`messagemode2`) entre ellos: solo lo ven otros espectadores
(se muestra con `(Espectadores)`). El chat general queda igual.

## Servidor: torneo en un paso (`sv torneo`) y menú Duelo / Torneo

Las fichas de torneo se crean en la web (panel admin → Torneos): **clave** corta (la que se escribe en el server)
y **nombre** largo. El bot del chat baja cada 5 min las fichas abiertas (*Próximo* / *En curso*) de la API
pública y las deja en `dday/torneos.txt` (`clave|minutos|nombre`); la DLL las lee de ahí.

| Comando | Qué hace |
|---|---|
| `sv torneo` | Torneo activo y lista de torneos abiertos. |
| `sv torneo <clave> [mapa] [minutos]` | Activa el torneo. Vale la clave exacta o un pedazo de la clave o del nombre si calza con un solo torneo (si no, lista los abiertos). Pone `stats_event` + `stats_event_name`, `tournament 1`, `stats_mode duel`, `ready_mode 1`, `ready_time` (los minutos del comando o de la ficha; `0` = sin tiempo), `mapvoting 0`, `bots 0` y `sv_maplist` = el mapa. Recarga el mapa si se dio uno o si el server no estaba en duelo. Con una partida en juego, sin mapa, no lo activa. `sv torneo clave 25` = 25 min sin cambiar de mapa. |
| `sv torneo fin` | Lo termina y deja las cvars como estaban antes de activarlo. Con una partida en juego, termina al acabar esa partida. |

- **Reinicios:** el torneo activo se guarda en `dday/torneo_activo.txt` (con su mapa y los valores previos de las
  cvars). Si el server se reinicia, al cargar el mapa se vuelve a poner solo (y vuelve a su mapa).
- **Fin automático:** si la ficha deja de estar abierta en la web (pasa a *Terminado*), el bot hace `sv torneo fin`.
- Si alguien cambia `stats_event` a mano, el torneo se da por terminado (sin tocar las demás cvars).
- Desde el bot: `torneo` (lista), `torneo <clave> [mapa] [min]`, `torneo fin`; `evento ...` es lo mismo.

**Menú Duelo / Torneo** (solo con un torneo activo): lo abre cualquiera con `menuduelo` (o `menutorneo`) o
desde el menú principal ("Duelo / Torneo"). Muestra el torneo, el estado de cada equipo y de la partida, y
las acciones que sirven en ese momento para quien puede actuar como capitán (el capitán, o cualquiera de un equipo
sin capitán, que queda de capitán):

| Acción | Cuándo |
|---|---|
| Ser capitán / Listos / No estamos listos | Antes de empezar |
| Cambiar mapa... | Antes de empezar: lo propone un capitán (lista = `sv_maplist` del `server.cfg`) y el otro lo acepta en su menú ("Aceptar mapa X", 60 s). Si el otro equipo está vacío, cambia directo |
| Pedir pausa / Seguir | En la partida (las mismas reglas que `pausa` / `seguir`) |
| Cancelar partida | Partida empezada o en la cuenta (los dos capitanes, como `cancelar`) |

Echar a alguien no va en el menú: queda `vote kick`. Terminar el torneo: `sv torneo fin`, el bot, la web o el
menú del árbitro.

**Árbitro** (cualquiera que tenga la clave rcon del server, sin importar el nick):

| Comando | Qué hace |
|---|---|
| `arbitro <clave rcon>` | **En la consola** (tecla `~`), no en el chat. Queda de árbitro mientras siga conectado (se avisa a todos: "X es arbitro") y se abre su menú. 3 claves malas lo bloquean hasta que se reconecte; cada intento malo queda en el log con nick e IP. |
| `arbitro` | Abre el menú del árbitro (también "Arbitro..." en el menú Duelo / Torneo; sin torneo activo, "Arbitro" en el menú principal y en el de equipos). |
| `arbitro salir` | Deja de ser árbitro. |

Un mensaje de chat que empiece con `arbitro` o que contenga la clave rcon no se manda (se avisa que va en la consola).

| Menú del árbitro | Qué hace |
|---|---|
| Activar torneo... / Cambiar torneo... | Torneo de la lista (fichas abiertas) → mapa (este u otro de la rotación) → minutos (de la ficha, 10-30 o sin tiempo); como `sv torneo <clave> [mapa] [min]`. Cambiar torneo solo sin partida en juego |
| Empezar ya (sin ready) | Salta el ready de los capitanes y parte la cuenta (como `sv startcount` con los minutos del torneo) |
| Pausar / Sacar la pausa | Su pausa no tiene tiempo ni gasta pausas de los equipos; solo él (o `sv ready seguir`) la saca, los capitanes no. "Sacar la pausa" también saca la de un capitán o la automática |
| Reiniciar partida | Con confirmación: como `sv resetcount` (vuelve el calentamiento y el ready) |
| Cambiar mapa... | Directo, sin que lo acepte el otro capitán; con una partida en juego pide confirmación (se corta) |
| Minutos... | Cambia los minutos del torneo activo (desde la próxima partida) |
| Terminar torneo | Con confirmación: como `sv torneo fin` (con una partida en juego, al acabarla) |
| Menu de jugadores | El menú Duelo / Torneo normal |

Duelo en modo control (`control_mode 1` + `stats_mode duel`): durante el calentamiento la zona se ve pero queda
cerrada (nadie la gana antes del duelo); el bloqueo (`control_lock`) cuenta desde "Que comience el juego!". Rota y se
vota solo entre los mapas con `.ctl`.

## Servidor: MVD de la partida (para bajarlo desde la web)

Graba con el MVD de Q2PRO (`sv_mvd_enable 1` en `server.cfg`) y le pone de nombre
el id de la partida de las stats: `demos/<match_id>.mvd2.gz`. Hace falta `stats_log 1`.
Se corta solo al cambiar de mapa (`sv_mvd_maxmaps 1`). Sin jugadores humanos
Q2PRO no graba nada (los bots no cuentan).

| Cvar / comando | Por defecto | Qué hace |
|---|---|---|
| `mvd_partidas` | `0` | `1` graba un MVD de cada partida, desde la actual. Al instante; un reinicio lo apaga. |
| `sv mvd_partida` | — | Graba solo la partida en curso, desde ese momento. |

En duelo (`stats_mode duel`) `mvd_partidas` graba solo el duelo: empieza con "Que comience el juego!" (sin el
calentamiento) y, si no se juega, no graba nada. Con `sv resetcount` se corta y el próximo duelo lo vuelve a grabar
con el mismo nombre.

Un `mvdrecord` manual con otro nombre sigue funcionando, pero esa grabación no sale en la web.

## Servidor: votación de los jugadores y nicks protegidos

Detalle: [`VOTACION.md`](VOTACION.md).

| Cvar / comando | Por defecto | Qué hace |
|---|---|---|
| `vote_enable` | `0` | `1` = los jugadores pueden votar en la partida (`vote map`, `vote restart`, `vote kick`, `vote yes/no`). |
| `vote_time` | `30` | Segundos que dura una votación. |
| `vote_percent` | `50` | % de sí que hay que superar (mapa y restart). |
| `vote_cooldown` | `60` | Espera tras una votación rechazada. |
| `vote_kick` | `1` | `0` = sin votación de kick. |
| `vote_kick_percent` | `75` | % de sí en un kick (sin contar al acusado). |
| `vote_kick_min` | `3` | Humanos en equipo necesarios para un kick. |
| `vote_announce` | `600` | Segundos entre avisos de cómo votar (`0` = sin avisos). |
| `vote_modes` | `""` | Modos que se votan con `vote <modo> <mapa>` además del normal: `gungame`, `control`, `ffa` (separados por espacio). En S1: `"gungame"`. |
| `sv votecancel` | — | Corta la votación en curso. |
| `nick_protect` | `1` | Nicks y tags de `dday/nicks.txt` solo con clave (`set nickpw <clave> u`, `set tagpw <clave> u`). |
| `sv nicks [reload]` | — | Lista las reglas (sin claves) / relee el archivo. |

Comandos de los jugadores (consola): `vote map <mapa>` (= `vote normal <mapa>`, siempre modo normal),
`vote gungame <mapa>` / `vote control <mapa>` / `vote ffa <mapa>` (según `vote_modes`, no en duelo
ni torneo), `vote restart`, `vote kick <nombre|número>`, `vote yes`, `vote no`,
`vote maps <texto>` (`vote maps gungame`: los mapas de ese modo), `vote players`.

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
- **Juego** (los mapas: cualquier mapa de D-Day del server): `mapa <x>` `duelo <mapa>` `publico` `normal` `control <mapa>`
  `ffa <mapa>` `ffa ruleta <mapa>` `gungame <mapa>` `dm <mapa>` `modo normal|control|ffa|gungame|dm|duelo [mapa]` `kamikaze on|off` `tornado [on|off]` `tab nuevo on|off` `dardo on|off` `grabar on|off|ahora` `evento <nombre>` `cuenta [minutos]`
  `reset` `resetscore` `torneo on|off` `freeze` `proximamapa` `lock` `unlock`
  `say <texto>`
- `control <mapa>` (o `modo control <mapa>`) arranca el modo control en ese mapa y **apaga la
  votación de fin de mapa** mientras dure la sesión; después **rota entre los mapas con `.ctl`**
  (la DLL elige el siguiente). Ahí `vote map` vuelve al modo normal y `vote control <mapa>` (si
  `vote_modes` lo permite) sigue en control.
- `ffa <mapa>` (o `ffa` solo = el mapa actual; también en frase: "admin quiero probar
  el ffa en dust") arranca Free For All en ese mapa; como `control`, rota entre los mapas con
  `ents/<mapa>_ffa.ent` (dday2, gg_rats, inferno y tebessa2). La votación de fin de mapa queda como en
  `server.cfg` (en S1 se vota entre Normal y Gun Game). `vote map` vuelve al modo normal y
  `vote gungame <mapa>` sigue en Gun Game.
- `gungame <mapa>` igual que `ffa`, pero con el Gun Game (`ffa_gungame 1`); `ffa` lo apaga.
- `ffa ruleta <mapa>` FFA con la ruleta de armas (`ffa_roulette 1`) en vez de todos Sniper.
- `mapa`, `dm`, `normal` y `publico` apagan el modo control y el FFA, y restauran la
  rotación (`sv_maplist`) y la votación (`mapvoting`) de `server.cfg`.
- `kamikaze on|off` prende o apaga el airstrike kamikaze del Officer (`kamikaze_arty`);
  sin argumento dice el estado. También entiende frases ("admin activa el kamikaze").
  Rige desde el próximo airstrike y un reinicio del server lo apaga.
- `tornado` hace aparecer un tornado ahora (`sv tornado`); `tornado on` prende el automático de
  tebessa y tebessa2 (`tornado 1`); `tornado off` lo apaga también en el Gun Game
  (`tornado_gungame 0`, vuelve a 1 al reiniciar) y saca el que haya. También entiende frases:
  "admin tira un tornado", "admin apaga el tornado", "admin activa el tornado automático".
- `dardo on|off` es lo mismo que `medic on|off`: el dardo de sueño viene con el Medic nuevo
  (`medic_new`, próximo mapa). También entiende frases ("admin activa el dardo").
- La votación de fin de mapa **nunca** lleva al modo control: el mapa votado
  siempre arranca en deathmatch (la DLL pone `control_mode 0`).
- **Moderación:** `kick <nick>` `kickban <nick>` `ban <ip>` `unban <ip>`
  `killjugador <nick>` `kickbots` `bots on|off` `reportar <quien> <motivo>`
- **q2pro:** `screenshot` `stuff <cmd>` `stuffid <id> <cmd>` `autostuff <cmd>`
  `autostuffoff` `liststuff`
- **Password:** `pass <nueva>` `passoff`
- **Alias:** `alias <nick>` muestra los otros nicks de la misma IP, solo en la consola del admin.
  La votación por chat (`!votemap`, `!votekick`, `!si`, `!no`) se sacó el 2026-11-20: se vota con
  `vote` en la consola. Ver [`VOTACION.md`](VOTACION.md).
- **Con LLM:** `elo <nick>`

## Comandos del juego que ya existían y usamos

| Comando | Para qué |
|---|---|
| `spot` | Imprime la posición y el ángulo del jugador (marcar zonas de control). |
| `sv startcount` / `sv resetcount` | Cuenta regresiva de inicio / descartarla (duelos). |
| `sv freeze` | Pausar la partida. |
| `sv resetscore` | Poner en cero el puntaje de los equipos. |
