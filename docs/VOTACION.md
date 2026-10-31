# Votación de los jugadores, nicks protegidos y alias

Herramientas para que el server se administre solo cuando no hay un admin
conectado: los jugadores votan cambiar de mapa o echar a alguien, los nicks de
los admins y los tags de clan se protegen con clave, y los admins pueden ver
los otros nicks que usó un jugador.

Se evaluó usar **q2admin** (como otros servers de D-Day), pero se hizo en la
DLL: q2admin se pone entre el server y el mod (cambia el deploy, hay que
calibrar `entity_classname_offset` o el server se cae) y su votación no sabe
de torneos, duelos, modo control ni bots. Lo demás que ofrece q2admin ya lo
trae Q2PRO (`addban`, `sv_iplimit`, `addfiltercmd`, `addcvarban`,
`sv_lrcon_password`, `sv_enforcetime`) o el bot del chat.

## Votación de la DLL (`src/g_vote.c`)

Es distinta de la votación del final del mapa (`mapvoting`): la abre un
jugador cuando quiere, con la sintaxis que usaba q2admin.

| Comando (consola) | Qué hace |
|---|---|
| `vote map <mapa>` | Propone cambiar a ese mapa. El que propone vota sí. |
| `vote restart` | Propone reiniciar el mapa actual. |
| `vote kick <nombre o número>` | Propone echar a un jugador. |
| `vote yes` / `vote no` (o `vote si`) | Vota la propuesta en curso. |
| `vote maps <texto>` | Busca entre los mapas del server. |
| `vote players` | Número y nombre de cada jugador (para `vote kick`). |
| `vote` | Muestra la votación en curso o la ayuda. |
| `sv votecancel` | (admin) Corta la votación en curso. |

Reglas:

- Votan solo los **humanos que están en un equipo** (no espectadores ni bots).
  No votar cuenta como no: si se acaba el tiempo sin llegar, se rechaza.
- **Mapa:** se aprueba cuando los sí superan `vote_percent` (50 %). Se puede
  votar cualquier `.bsp` de `dday/maps` que sea de D-Day (tiene
  `info_team_start` o `info_reinforcements_start` en el `.bsp` o en
  `ents/<mapa>.ent`), sin distinguir mayúsculas. Si no está, avisa y sugiere
  parecidos; si no es de D-Day o el archivo está dañado, también avisa.
  Aprobada, termina como `sv nextmap`: la partida queda en las stats como
  cortada (`reason: forced`), no sale la votación de fin de mapa y el mapa
  cambia al terminar la intermisión.
- **Kick:** los sí tienen que superar `vote_kick_percent` (75 %) **sin contar
  al acusado**, que no vota; hacen falta `vote_kick_min` (3) humanos en
  equipo. No se puede echar a un admin (nick protegido) ni a un bot. Sin ban:
  puede volver a entrar. Si el acusado se va, se cancela. Quien propone un
  kick rechazado espera 2 min, y a un mismo jugador no se le vota más de una
  vez cada 2 min. Se anota en el log quién propuso y quién votó sí.
- No se vota en **torneo** (`tournament 1`), **duelo** (`stats_mode duel`),
  campaña ni intermisión. Con un solo humano se aprueba al tiro.

Mensajes: al proponer y al terminar sale un aviso **al medio de la pantalla**
(como q2admin) y una línea en el chat; cada 10 s se recuerda en el chat cómo
va. Solo se usa el centro dos veces por votación porque en Q2PRO un
centerprint borra las kills de arriba.

**Avisos de cómo votar** (con `vote_enable 1` y sin votación en curso): al
entrar al server (solo a ese jugador, una vez), a los 45 s de empezar el mapa,
cada `vote_announce` segundos y al terminar el mapa. Si el bot tiene prendida
la votación por chat, el aviso suma `!votemap` / `!votekick`.

| Cvar | Por defecto | Qué hace |
|---|---|---|
| `vote_enable` | `0` | `1` = los jugadores pueden abrir votaciones. |
| `vote_time` | `30` | Segundos que dura una votación (mínimo 5). |
| `vote_percent` | `50` | % de sí que hay que superar (mapa y restart). |
| `vote_cooldown` | `60` | Segundos de espera tras una votación rechazada. |
| `vote_kick` | `1` | `0` = no se puede votar kick. |
| `vote_kick_percent` | `75` | % de sí que hay que superar en un kick. |
| `vote_kick_min` | `3` | Humanos en equipo necesarios para un kick. |
| `vote_announce` | `600` | Segundos entre avisos de cómo votar (`0` = sin avisos). |
| `vote_chat` | `0` | Lo pone el bot: `1` = el aviso incluye los comandos del chat. |

Ninguno es *latched*: rigen al instante. Lo que se cambia por rcon se pierde al
reiniciar el server; lo permanente va en `server.cfg` (en el server de
pruebas, `set vote_enable 1`).

## Nicks y tags protegidos (`src/g_nicks.c`)

El bot del chat reconoce a los admins por el nick, así que hace falta que
nadie más pueda usarlo. Las reglas van en `dday/nicks.txt` **solo en el
server** (tiene claves; está en `.gitignore`). Formato en
[`../nicks.txt.example`](../nicks.txt.example):

```
nick >ChL<Snako    clave      nadie mas puede usar ese nick
nick *peruano*     clave      ningun nick que contenga "peruano"
tag  [MR]          clave      el tag del clan, en cualquier parte del nick
```

- Los nicks se comparan **normalizados** (sin colores, minúsculas, solo letras
  y números: `chl snako` = `>ChL<Snako`); los tags sin colores, en minúsculas
  y sin espacios.
- El jugador manda la clave en su userinfo, una vez, en su `autoexec.cfg`:
  `set nickpw <clave> u` (nick) y `set tagpw <clave> u` (tag del clan). El
  dueño de un nick protegido no necesita además la del tag.
- **Ponerlo también en `baseq2/autoexec.cfg`:** Q2PRO arranca en `baseq2` y,
  si se conecta así, el `autoexec.cfg` de `dday` no alcanza a correr.
- **Sin la clave no se entra:** la conexión se rechaza con
  `Nick protegido: set nickpw TU_CLAVE u y reconecta` (o el aviso del tag). Los
  valores del userinfo tienen que ser de menos de 64 caracteres, por eso el
  mensaje es corto.
- Si se cambia a un nick protegido **ya dentro**, sigue con el nick que tenía
  (o `Jugador<n>`) y recibe el aviso completo en la consola.
- Las claves se borran del userinfo apenas se revisan; el nick validado se
  recuerda por slot (el mod vuelve a pasar el userinfo guardado, sin claves,
  al aparecer en el mapa).

| Cvar / comando | Por defecto | Qué hace |
|---|---|---|
| `nick_protect` | `1` | Protección activa (es seguridad; sin `nicks.txt` no hace nada). |
| `sv nicks` | — | Lista las reglas (sin claves). |
| `sv nicks reload` | — | Relee `nicks.txt`. También se relee al empezar cada mapa. |

## Bot del chat (`tools/chat-admin`)

Detalle: [`../tools/chat-admin/README.md`](../tools/chat-admin/README.md).

- **Votación por chat**, para cualquier jugador: `!votemap <mapa>`,
  `!votekick <nick>`, `!si`, `!no`, `!voto`. Mismas reglas que la DLL, pero
  cuenta a todos los jugadores de `status` (no distingue espectadores) y
  aprobada hace `map` por rcon. Se prende con el comando de admin
  `votacion on` (queda guardado y sobrevive a reinicios del bot) y se apaga
  con `votacion off`; `votacion cancelar` corta la que está en curso. Tiene su
  propio límite (6 comandos cada 30 s por jugador).
- **Alias** (`alias <nick>`, solo admins): los otros nicks usados desde la
  misma IP exacta. El bot guarda `(ip, nick)` de `status` cada 20 s en una
  base SQLite local del server (180 días); la respuesta va solo a la consola
  del admin (`stuff <id> echo`), nunca al chat.
- Al arrancar avisa en el journal si algún admin de la allowlist no está
  protegido en `nicks.txt`.
- Escribe **sin tildes ni ñ** (Quake 2 las muestra mal).
- Ignora la línea que sigue a `Rcon from ...` en el log: es el comando rcon
  (sus propios `say` y `stuff`), y leerla como chat permitía que un nick
  armado diera órdenes.

## Probar sin jugadores reales

La votación necesita humanos en un equipo. Para probarla en el server local se
usó un cliente mínimo en Python (protocolo 34) que conecta, responde el
*version probe* de Q2PRO (`cmd \x7fc version ...`), manda `begin` al recibir
`precache`, entra a un equipo con `team` + `invuse` y envía comandos y
cambios de userinfo. Con más de 3 clientes desde la misma IP hace falta
`sv_iplimit 0`.
