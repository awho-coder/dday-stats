# Bot Admin del server Quake 2 (D-Day) — "Hermes en Quake2"

Sistema que lee el **chat del server**, ejecuta comandos por **rcon** y conversa
en lenguaje natural. Responde **solo a los nicks de la allowlist** y **solo si
dicen "admin"** (o si escriben un comando directo).

## Arquitectura (3 piezas)

```
   [SERVER Q2 en la VM]  <-- logfile 2 escribe el chat a console.log
            | lee el log cada 0.3 s
            v
   [chat-admin.py en la VM]  --comando?--> ejecuta rcon local --> responde con `say`
            |                          <1 s, sin IA
            | --charla con "admin"?
            v
   [relay.py en la PI :8099]  (solo por Tailscale, token HMAC)
            | consulta BD (elo) y LLM (OpenCode Go)
            v
   responde en el juego (2-5 s)  +  auditoría  +  reporte a Telegram (cron)
```

- **chat-admin.py** (VM): lee `/var/data/dday/logs/console.log` (lo genera el
  server con `logfile 2` + `logfile_flush 1`), filtra por allowlist exacta,
  ejecuta comandos por rcon local (password leído de `server.cfg`) y responde
  con `say`. **Nunca** expone el valor de la password al LLM ni al chat.
- **relay.py** (Pi): recibe preguntas por HTTP (bind SOLO en la IP Tailscale),
  autentica con token (HMAC compare), inyecta el ELO real desde Postgres y
  consulta el LLM (OpenCode Go). Rate limit 10/min.
- **cron Hermes** (Pi, opcional): revisa la auditoría y manda novedades a
  Telegram; también es el **fallback** si el relay se cae (responde en ~1 min).

## Requisitos

- Server Q2 con **q2pro** y el mod cargado, con `logfile 2` + `logfile_flush 1`
  (sin eso el chat no llega a la consola y el bot es ciego).
- Python 3 (stdlib) en la VM y en la Pi.
- Tailscale entre VM y Pi (el relay se bindea a la IP Tailscale).
- API key de OpenCode Go en el entorno de la Pi (`OPENCODE_GO_API_KEY`).

## Instalación — VM (chat-admin)

```bash
cp chat-admin.py /home/ubuntu/dday/chat-admin.py
cp dday-chat-admin.service /etc/systemd/system/
# token compartido con el relay (el MISMO valor en ambos lados):
uuidgen | sudo tee /home/ubuntu/dday/.relay-token
chmod 600 /home/ubuntu/dday/.relay-token
sudo systemctl daemon-reload && sudo systemctl enable --now dday-chat-admin
journalctl -u dday-chat-admin -f     # logs en vivo
```

## Instalación — Pi (relay)

```bash
mkdir -p ~/quake-llm-relay && cp relay/relay.py ~/quake-llm-relay/
cp relay/token.example ~/quake-llm-relay/token     # pegar el MISMO uuid
chmod 600 ~/quake-llm-relay/token
export OPENCODE_GO_API_KEY=...                     # o en ~/.hermes/.env
cp relay/quake-llm-relay.service ~/.config/systemd/user/
systemctl --user daemon-reload && systemctl --user enable --now quake-llm-relay
loginctl enable-linger $USER                       # sobrevive al logout
```

## Configuración (constantes al inicio de cada archivo)

| Qué | Dónde | Por defecto |
|---|---|---|
| Nicks autorizados | `chat-admin.py` → `ALLOW_RAW` | `>ChL<Snako`, `[MR]+Peruano+`, `[MR]+roman` |
| Log del server | `chat-admin.py` → `LOG` | `/var/data/dday/logs/console.log` |
| URL del relay | `chat-admin.py` → `RELAY_URL` | `http://<ip-tailscale>:8099/ask` |
| Token (VM) | archivo `.relay-token` | — |
| Token + bind (Pi) | `relay.py` → `BIND/PORT` y archivo `token` | `<ip-tailscale>:8099` |
| Modelo LLM | `relay.py` → `MODEL` | `deepseek-v4.1-flash` |
| Máquina del elo | `relay.py` → `PG_*` | Postgres del pipeline |

## Comandos (primer palabra; no requieren "admin")

- **Info**: `estado` `ayuda` `bans` `tiempo` `mapas` `infomapa` `marcador` `zona`
  `versiones` `lag` `tiempos`
- **Juego**: `mapa <x>` `duelo <mapa>` `publico` `control <mapa>` `ffa <mapa>` `dm <mapa>`
  `kamikaze on|off` (sin argumento dice el estado; también en frase natural:
  "admin activa/apaga el kamikaze", detector local, sin LLM)
  `cuenta` (17 alias: iniciar/start/tira…) `reset` `resetscore` `torneo on|off`
  `freeze` `proximamapa` `lock` `unlock` `say <texto>`
- **Moderación**: `kick <nick>` `kickban <nick>` `ban <ip>` `unban <ip>`
  `killjugador <nick>` `kickbots` `bots on|off` `reportar <quien> <motivo>`
- **q2pro**: `screenshot` (= `stuffall screenshot`) `stuff <cmd>` `stuffid <id> <cmd>`
  `autostuff <cmd>` `autostuffoff` `liststuff`
- **Password**: `pass <nueva>` `passoff` — y en frase natural
  ("admin ponle password al server, password X", detector local, sin LLM)
- **Único que usa LLM**: `elo <nick>` (consulta la BD y redacta)
- **Bloqueados para siempre**: `exec` `set` `restart` `rcon_password`
  archivos/BD/servicios (whitelist dura en código, no en prompt)

## Seguridad (capas)

1. **Allowlist exacta** de nicks (case-sensitive; variantes tipo `[mr]+roman` se ignoran).
2. **Whitelist dura** de verbos en código: nada de sistema de archivos, BD ni servicios.
3. **Rate limit** 3 comandos/30 s por nick.
4. **Redacción**: el valor de una password **nunca** viaja al LLM ni se repite en
   el chat; la auditoría guarda `pass ****`.
5. **rcon local** en la VM (el password de rcon no sale del server);
   el relay solo escucha en la interfaz Tailscale y exige token.

## Operación

```bash
sudo systemctl restart dday-chat-admin      # VM (después de editar el .py)
# corre una sola instancia (candado /home/ubuntu/dday/chat-admin.lock): una segunda sale sin hacer nada
systemctl --user restart quake-llm-relay    # Pi
tail -f /home/ubuntu/dday/chat-admin.actions   # auditoría (quién pidió qué)
```

**Al parchar `chat-admin.py`: reiniciar el servicio** (si no, corre el código viejo).

## Portabilidad

- El bot es agnóstico: sirve para **cualquier server q2pro** que loguee el chat
  (cambiar IP/rutas en las constantes).
- Los comandos `sv ...` (`switch`, `teams`, `killplayer`, `startcount`, `freeze`,
  `nextmap`, `mapinfo`, `maplist`), el modo control/duelo, los bots y las stats
  vienen del **mod D-Day** (la DLL/.so), no del motor: sin ese mod cargado no existen.
- Comandos de motor (`kick`, `ban`, `map`, `status`, `stuffall`…) funcionan en
  cualquier q2pro.
- Para otro juego (Source/Minecraft/Quake3) el patrón es el mismo pero hay que
  adaptar el lector del log y el cliente rcon.
