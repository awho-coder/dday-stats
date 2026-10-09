#!/usr/bin/env python3
# chat-admin.py — admin bot por chat para D-Day (q2pro) — whitelist duro de comandos
import os, re, sys, time, socket, fcntl

LOG    = "/var/data/dday/logs/console.log"
CFG    = "/home/ubuntu/dday/dday/server.cfg"
INBOX  = "/home/ubuntu/dday/chat-inbox"           # chat conversacional -> Hermes
RUNLOG = "/home/ubuntu/dday/chat-admin.actions"   # auditoría de acciones
DEBUG  = "/home/ubuntu/dday/chat-admin.debug"     # calibración de formato
ALLOW  = {"chlsnako", "mrperuano", "mrroman"}          # solo para heurística de debug
ALLOW_RAW = (">ChL<Snako", "[MR]+Peruano+", "[MR]+roman")  # match EXACTO anti-squatting
MAPS   = {"dday2","invade1","invade2","invade6","itadday3","townwar","eurovilla","eurovilla3",
          "inland2","inland3","inland4","inland5","inland6","market1","mp1dday1","mp1dday2",
          "mp1dday3","nav2","nuenen","outpost","townwar1","war3","itadday1","itadday2",
          "dust","dday3","dday4","doomtown","tebessa2"}
POOL   = "doomtown dust eurovilla3 market1 nuenen outpost tebessa2 townwar invade6"
POOL_MAPS = ["doomtown","dust","eurovilla3","market1","nuenen","outpost","tebessa2","townwar","invade6"]
NO_NAV = {"eurovilla3","market1","nuenen","tebessa2","townwar"}   # sin .cmp de bots
RCON = ("127.0.0.1", 27910)
DRY = False
TESTMODE = False

def norm(s): return re.sub(r"[^a-z0-9]", "", (s or "").lower())

def rcon_pw():
    try:
        m = re.search(r'set rcon_password\s+"([^"]+)"', open(CFG).read())
        return m.group(1) if m else ""
    except Exception:
        return ""

def normal_maplist():
    """sv_maplist de server.cfg: la rotacion normal (si nadie vota al final del mapa, sigue esta)."""
    try:
        m = re.search(r'^\s*set\s+sv_maplist\s+"([^"]+)"', open(CFG).read(), re.M)
        return m.group(1) if m else ""
    except Exception:
        return ""

def cfg_value(name):
    """Valor de 'set <name> "..."' (o sin comillas) en server.cfg, '' si no esta."""
    try:
        m = re.search(r'^\s*set\s+%s\s+(?:"([^"]*)"|(\S+))' % re.escape(name), open(CFG).read(), re.M)
        return (m.group(1) if m.group(1) is not None else m.group(2)) if m else ""
    except Exception:
        return ""

def restore_rotation():
    """Modo normal: sin control ni FFA, con la rotacion y la votacion de server.cfg."""
    rcon("set control_mode 0"); rcon("set ffa 0")
    ml = normal_maplist()
    if ml: rcon('set sv_maplist "%s"' % ml)
    mv = cfg_value("mapvoting")
    if mv: rcon("set mapvoting %s" % mv)

def rcon(cmd, wait=1.0):
    if DRY:
        print("DRY-RCON:", cmd); return ""
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM); s.settimeout(wait)
    pkt = b"\xff\xff\xff\xffrcon " + rcon_pw().encode() + b" " + cmd.encode()
    s.sendto(pkt, RCON); data = b""; t0 = time.time()
    while time.time() - t0 < wait:
        try:
            chunk, _ = s.recvfrom(8192); data += chunk
            if time.time() - t0 > 0.3: break
        except socket.timeout:
            break
    s.close()
    txt = data.decode("utf-8", "replace")
    if txt.startswith("\xff\xff\xff\xff"): txt = txt[4:]
    if txt.startswith("print"): txt = txt[5:]
    return txt.strip()

def _cl(s):
    import re as _r
    return _r.sub(r'[\x00-\x08\x0b-\x1f\ufffd]+', '', s).replace('print', '').strip()

def say(msg):
    return rcon("say " + _cl(msg)[:190], wait=0.3)

def log_action(by, text, result):
    try:
        with open(RUNLOG, "a") as f:
            f.write("%s | %s | %s | %s\n" % (time.strftime("%Y-%m-%d %H:%M:%S"), by, text, result[:120]))
    except Exception:
        pass

def players():
    out = rcon("status"); res = []
    for line in out.splitlines():
        # q2pro imprime el nick SIN comillas: 'num score ping name lastmsg ip:port rate pr fps'
        m = (re.match(r'\s*(\d+)\s+-?\d+\s+\d+\s+(.+?)\s+\d+\s+\d{1,3}(?:\.\d{1,3}){3}:\d+', line)
             or re.match(r'\s*(\d+)\s+-?\d+\s+\S+\s+(.+?)\s+\d+\s+\S+:\d+', line)
             or re.match(r'\s*(\d+)\s+\d+\s+"([^"]+)"', line)
             or re.match(r'\s*(\d+)\s+"([^"]+)"', line))
        if m: res.append((int(m.group(1)), m.group(2).strip()))
    return res

FILLER = {"a","al","la","el","los","las","un","una","de","del","en","jugador","jugadores",
          "player","players","por","favor","que","se","lo"}

def clean_arg(arg):
    words = [w for w in re.split(r"\s+", arg) if norm(w) not in FILLER]
    return " ".join(words).strip()

def current_map():
    """Mapa que se esta jugando (de 'status'), '' si no se pudo leer."""
    st = rcon("status")
    m = re.search(r"mapname\\(\S+)|Current map:\s*(\S+)", st)
    return (m.group(1) or m.group(2)) if m else ""

# 'ffa', 'ffa este mapa', 'ffa aca'... = el mapa que se esta jugando
MAPA_ACTUAL = {"", "mapa", "estemapa", "elmapa", "mapaactual", "actual", "aca", "aqui", "este", "esta"}

def find_player(name):
    n = norm(name)
    if n: n = clean_arg(n) or n
    n = norm(name.replace(name, "")) if False else norm(clean_arg(name) or name)
    pl = players()
    for uid, nick in pl:
        if n and norm(nick) == n: return uid, nick
    words = [w for w in re.split(r"\s+", clean_arg(name) or name) if w]
    for uid, nick in pl:
        nn = norm(nick)
        if n and (n in nn or nn in n): return uid, nick
        for w in words:
            nw = norm(w)
            if nw and len(nw) >= 3 and (nw in nn or nn in nw): return uid, nick
    return None, None

HELP = ("[ADMIN] comandos: estado, kick <quien>, kickban <quien>, ban <ip>, unban <ip>, "
        "mapa <nombre>, duelo <mapa>, publico, cuenta, reset, resetscore, tiempo, mapas, bans, "
        "bots on|off, kickbots, screenshot, stuff <cmd>, autostuff <cmd>, autostuffoff, "
        "control <mapa>, ffa <mapa>, dm <mapa>, kamikaze on|off, "
        "lock, unlock, say <texto>, ayuda")

def run_cmd(text, by):
    """Ejecuta un comando de chat. SIEMPRE vía whitelist. Devuelve respuesta para say()."""
    t = text.strip().lstrip("!").strip()
    t = re.sub(r"^(oye\s+)?(admin[,: ]\s*)+", "", t, flags=re.I).strip()  # tolera 'oye admin ...'
    parts = t.split(None, 1)
    if not parts: return HELP, True
    verb = norm(parts[0]); arg = clean_arg(parts[1].strip()) if len(parts) > 1 else ""
    if verb == "modo":
        sub = arg.split(None, 1)
        verb = norm(sub[0]) if sub else "normal"
        arg = sub[1] if len(sub) > 1 else ""
        if verb not in ("control", "ffa", "freeforall", "todoscontratodos", "dm", "deathmatch", "duelo", "duel",
                        "publico", "publica", "normal", "clasico", "restaurar", "default"):
            return "[ADMIN] uso: modo normal | modo control <mapa> | modo ffa <mapa> | modo dm <mapa> | modo duelo <mapa>", True

    # lecturas
    if verb in ("ayuda", "help"): return HELP, True
    if verb in ("estado", "status", "jugadores", "jugador", "players"):
        st = rcon("status")
        m = re.search(r"mapname\\(\S+)|Current map:\s*(\S+)", st)
        mapa = (m.group(1) or m.group(2)) if m else "?"
        pl = players()
        return "[ADMIN] mapa %s · %d conectado(s)" % (mapa, len(pl)), True

    # moderación
    if verb in ("kick", "kickea", "echar", "kik"):
        if not arg: return "[ADMIN] uso: kick <nombre>", True
        uid, nick = find_player(arg)
        if not uid and uid != 0: return "[ADMIN] no encuentro a '%s' conectado" % arg, True
        r = rcon("kick %d" % uid)
        return "[ADMIN] %s kickeado (petición de %s)" % (nick, by), True
    if verb in ("kickban",):
        if not arg: return "[ADMIN] uso: kickban <nombre>", True
        uid, nick = find_player(arg)
        if not uid and uid != 0: return "[ADMIN] no encuentro a '%s'" % arg, True
        rcon("kickban %d" % uid)
        return "[ADMIN] %s echado y baneado (petición de %s)" % (nick, by), True
    if verb in ("ban",):
        if not re.match(r"^\d{1,3}(\.\d{1,3}){3}(/\d{1,2})?$", arg):
            return "[ADMIN] ban necesita una IP válida", True
        rcon('addban %s "chat-admin"' % arg)
        return "[ADMIN] IP %s baneada (petición de %s)" % (arg, by), True
    if verb in ("unban", "delban", "desban"):
        if not re.match(r"^\d{1,3}(\.\d{1,3}){3}(/\d{1,2})?$", arg):
            return "[ADMIN] unban necesita una IP válida", True
        rcon("delban %s" % arg)
        return "[ADMIN] IP %s liberada" % arg, True
    if verb in ("lock", "bloquear"):
        rcon("sv_locked 1"); return "[ADMIN] server bloqueado (solo tú entras vía rcon)", True
    if verb in ("unlock", "abrir"):
        rcon("sv_locked 0"); return "[ADMIN] server abierto", True

    # mapa / modos
    if verb in ("mapa", "map", "cambiamapa", "cambiar"):
        m = norm(arg)
        if m not in {norm(x) for x in MAPS}:
            return "[ADMIN] mapa '%s' no está en la lista" % arg, True
        real = [x for x in MAPS if norm(x) == m][0]
        restore_rotation(); rcon("map %s" % real)
        return "[ADMIN] cambiando a %s en modo normal (petición de %s)" % (real, by), True
    if verb in ("duelo", "duel"):
        real = None
        if arg:
            m = norm(arg)
            if m not in {norm(x) for x in MAPS}:
                return "[ADMIN] mapa '%s' no está en la lista" % arg, True
            real = [x for x in MAPS if norm(x) == m][0]
        rcon('set stats_mode duel'); rcon("set ffa 0")
        if real: rcon("map %s" % real)
        return ("[ADMIN] modo DUELO armado en %s — cuando estén listos: 'cuenta' "
                "para tirar la cuenta atrás (petición de %s)" % (real or "mapa actual", by)), True
    if verb in ("publico", "publica"):
        rcon('set stats_mode public'); restore_rotation()
        return "[ADMIN] modo PUBLICO/NORMAL: stats public, control apagado y rotación normal", True
    if verb in ("cuenta", "startcount", "tiracuenta", "tira", "tirar", "tiro", "lanza", "lanzar",
                "iniciar", "inicia", "arrancar", "arranca", "empezar", "empeza", "empieza",
                "start", "countdown", "count", "cuentaatras"):
        rcon("sv startcount")
        return "[ADMIN] cuenta atrás lanzada — ¡a jugar! (la cuenta la pide %s)" % by, True
    if verb in ("reset", "resetcount", "resetcuenta"):
        rcon("sv resetcount")
        return "[ADMIN] cuenta reseteada (lo registrado se descarta)", True
    if verb in ("control",):
        if not arg: return "[ADMIN] uso: control <mapa>", True
        m = norm(arg)
        if m not in {norm(x) for x in MAPS}: return "[ADMIN] mapa '%s' no está en la lista" % arg, True
        real = [x for x in MAPS if norm(x) == m][0]
        # sin votacion durante la sesion de control: las rondas siguen en este mapa hasta
        # que se pida normal/dm/mapa (la votacion nunca debe llevar al modo control)
        rcon("set mapvoting 0"); rcon("set ffa 0"); rcon("set control_mode 1"); rcon('set sv_maplist "%s"' % real); rcon("map %s" % real)
        return "[ADMIN] modo CONTROL en %s, sin votación hasta volver a normal (petición de %s)" % (real, by), True
    if verb in ("ffa", "freeforall", "todoscontratodos"):
        real = current_map() or "dday2"
        if norm(arg) not in MAPA_ACTUAL:
            m = norm(arg)
            if m not in {norm(x) for x in MAPS}: return "[ADMIN] mapa '%s' no está en la lista" % arg, True
            real = [x for x in MAPS if norm(x) == m][0]
        # ffa es latched: se aplica con el 'map'. Sin votacion para que la sesion siga en
        # este mapa hasta que se pida normal/dm/mapa (como en control)
        rcon("set mapvoting 0"); rcon("set control_mode 0"); rcon("set ffa 1"); rcon('set sv_maplist "%s"' % real); rcon("map %s" % real)
        return "[ADMIN] modo FREE FOR ALL en %s, todos contra todos, sin votación hasta volver a normal (petición de %s)" % (real, by), True
    if verb in ("dm", "deathmatch"):
        real = "dday2"
        if arg:
            m = norm(arg)
            if m not in {norm(x) for x in MAPS}: return "[ADMIN] mapa '%s' no está en la lista" % arg, True
            real = [x for x in MAPS if norm(x) == m][0]
        restore_rotation(); rcon("map %s" % real)
        return "[ADMIN] modo DM en %s" % real, True
    if verb in ("kamikaze", "kamikase", "kamicase"):
        # cvar de la DLL (no latched): rige desde el proximo airstrike; un reinicio del server lo apaga
        a = norm(arg)
        if a in ("on", "1", "si", "activar", "activa", "prender", "prende", "encender", "enciende", "poner", "pon"):
            rcon("set kamikaze_arty 1")
            return ("[ADMIN] arty KAMIKAZE activado: el Officer viaja en su avion y se estrella "
                    "en el punto marcado (desde el proximo airstrike)"), True
        if a in ("off", "0", "no", "desactivar", "desactiva", "apagar", "apaga", "quitar", "quita", "sacar", "saca"):
            rcon("set kamikaze_arty 0")
            return "[ADMIN] arty kamikaze DESACTIVADO: el airstrike vuelve a ser normal", True
        m = re.search(r'is\s+"(\d+)"', rcon("kamikaze_arty"))
        estado = "?" if not m else ("ON" if m.group(1) != "0" else "OFF")
        return "[ADMIN] arty kamikaze: %s (uso: kamikaze on|off)" % estado, True
    if verb in ("normal", "clasico", "restaurar", "default", "resetmodo"):
        restore_rotation(); rcon("set stats_mode public")
        rcon("map dday2")
        return "[ADMIN] modo NORMAL listo: dday2 · DM · public · rotación normal (petición de %s)" % by, True
    if verb in ("pool", "rotacion", "rotar", "mapaspool", "rotacionmapas"):
        restore_rotation()
        if not normal_maplist(): rcon('set sv_maplist "%s"' % POOL)
        sin = ", ".join(sorted(NO_NAV))
        return "[ADMIN] rotacion de 9 restaurada. Sin bots: %s" % sin, True
    if verb in ("evento", "event", "torneoevento"):
        nombre = (arg or "").strip().strip('"')
        if norm(nombre) in ("off", "quitar", "quitarlo", "limpiar", "ninguno", "sacar", "terminar", "fin"):
            rcon('set stats_event ""'); rcon("set tournament 0")
            return "[ADMIN] evento TERMINADO: las partidas vuelven al ladder normal", True
        if not nombre:
            return "[ADMIN] uso: evento <nombre-del-torneo>  o  evento off", True
        if not re.match(r"^[A-Za-z0-9 ._-]{3,40}$", nombre):
            return "[ADMIN] nombre de evento invalido (letras, numeros, . _ -, max 40)", True
        rcon('set stats_event "%s"' % nombre)
        return "[ADMIN] evento '%s' ACTIVO: las partidas van al ladder OFICIAL" % nombre, True
    if verb in ("pass", "password", "clave", "contrasena", "contraseña"):
        if verb in ("passoff",):  # placeholder nunca alcanzado
            pass
        if not arg:  # sin arg -> limpiar password
            rcon('set password ""')
            return "[ADMIN] password de entrada eliminada (server abierto)", True
        if re.search(r"\s", arg) or len(arg) < 4:
            return "[ADMIN] uso: pass <nueva de 4+ caracteres, sin espacios>", True
        rcon('set password "%s"' % arg)
        log_action("test-local" if TESTMODE else by, "pass ****", "[ADMIN] password de entrada cambiada (valor oculto)")
        return ("[ADMIN] password de entrada puesta. OJO: la escribiste en chat — si hay "
                "desconocidos conectados, cambiála después por Telegram", True)
    if verb in ("passoff", "sinpass", "quitaspass"):
        rcon('set password ""')
        return "[ADMIN] password de entrada eliminada (server abierto)", True
    if verb in ("say", "anuncia"):
        if arg:
            rcon("say " + arg[:190])
            return "[ADMIN] dicho", True
        return "[ADMIN] uso: say <texto>", True

    # ---- experto: bots ----
    if verb in ("bots",):
        a = norm(arg)
        if a in ("off", "quitar", "fuera", "cero", "apagar", "disable", "0"):
            rcon("set bots 0")
            return "[ADMIN] bots DESACTIVADOS (no entran más; los que estén se van solos o se kickean)", True
        if a in ("on", "poner", "activar", "enable", "1"):
            rcon("set bots 1")
            return "[ADMIN] bots ACTIVADOS de nuevo", True
        b = rcon("bots")
        return "[ADMIN] cvar bots: %s" % b.replace("print", "")[:50], True
    if verb in ("kickbots", "quitarbots", "kickallbots"):
        # kickear todos los conectados que NO sean de la allowlist (asume que son bots)
        pl = players()
        n = 0
        for uid, nick in pl:
            if nick not in ALLOW_RAW:
                rcon("kick %d" % uid); n += 1
        return "[ADMIN] kickeados %d bots (los humanos allowlist se quedan)" % n, True

    # ---- experto: screenshot / stuff (q2pro) ----
    if verb in ("screenshot", "captura", "pantallazo", "screenshots"):
        rcon("stuffall screenshot")
        return "[ADMIN] screenshot pedido a TODOS (cada cliente lo guarda en SU máquina)", True
    if verb in ("stuff", "stuffall", "enviaratodos"):
        if arg:
            rcon("stuffall " + arg[:150])
            return "[ADMIN] '%s' enviado a todos los clientes" % arg[:70], True
        return "[ADMIN] uso: stuff <comando>", True
    if verb in ("stuffid",):
        p2 = arg.split(None, 1)
        if len(p2) == 2 and p2[0].isdigit():
            rcon("stuff " + p2[0] + " " + p2[1][:120])
            return "[ADMIN] '%s' enviado al cliente %s" % (p2[1][:50], p2[0]), True
        return "[ADMIN] uso: stuffid <id> <comando>", True
    if verb in ("autostuff",):
        if arg:
            rcon("addstuffcmd begin " + arg[:120])
            return "[ADMIN] '%s' se les stuffea a todos al empezar cada mapa" % arg[:60], True
        return "[ADMIN] uso: autostuff <comando>  (para quitar: autostuffoff)", True
    if verb in ("autostuffoff", "delautostuff"):
        rcon("delstuffcmd begin all")
        return "[ADMIN] autostuff eliminado (lista begin vacía)", True
    if verb in ("liststuff", "stufflista"):
        resp = rcon("liststuffcmds begin")
        return "[ADMIN] stuffcmds begin: " + _cl(resp)[:110], True

    # ---- experto: info del server ----
    if verb in ("resetscore", "reseteapuntos", "resetkills"):
        rcon("sv resetscore")
        return "[ADMIN] scores reseteados (puntos de equipo)", True
    if verb in ("tiempo", "timeleft", "tiemporestante"):
        resp = rcon("sv timeleft")
        return "[ADMIN] %s" % resp.replace("print", "").strip()[:80], True
    if verb in ("mapas", "maplist", "listamapas"):
        resp = rcon("sv maplist")
        return "[ADMIN] mapas: " + _cl(resp)[:120], True
    if verb in ("bans", "listabans", "listbans"):
        resp = rcon("listbans")
        return "[ADMIN] bans: " + _cl(resp)[:110], True

    # ---- experto v2: info completa del server ----
    if verb in ("infomapa", "mapainfo", "infodelmapa"):
        resp = _cl(rcon("sv mapinfo"))
        return "[ADMIN] " + _cl(resp).replace(chr(10), " / ")[:150], True
    if verb in ("marcador", "score", "puntaje", "resultados"):
        resp = _cl(rcon("sv teams"))
        return "[ADMIN] " + _cl(resp).replace(chr(10), " / ")[:150], True
    if verb in ("versiones", "clientes", "anticheat"):
        resp = _cl(rcon("status v"))
        return "[ADMIN] versiones: " + _cl(resp).replace(chr(10), " / ")[:140], True
    if verb in ("lag", "pings", "conexion"):
        resp = _cl(rcon("status l"))
        return "[ADMIN] lag: " + _cl(resp).replace(chr(10), " / ")[:140], True
    if verb in ("tiempos", "conexiones"):
        resp = _cl(rcon("status t"))
        return "[ADMIN] tiempos: " + _cl(resp).replace(chr(10), " / ")[:140], True
    if verb in ("zona", "controlzona", "comova"):
        try:
            import subprocess as _sp
            out = _sp.run(["tail", "-200", LOG], capture_output=True, text=True, timeout=5).stdout
            lineas = [l for l in out.splitlines() if "controla la zona" in l]
            if lineas:
                return "[ADMIN] %s" % lineas[-1].split("] ", 1)[-1][:110], True
            return "[ADMIN] no hay zona activa en este mapa (modo control sin zona)", True
        except Exception:
            return "[ADMIN] no pude leer el estado de la zona", True
    if verb in ("proximamapa", "saltarmapa", "siguientemapa"):
        rcon("sv nextmap")
        return "[ADMIN] cambiando al próximo mapa de la rotación", True
    if verb in ("torneo",):
        a = norm(arg)
        if a in ("on", "1", "activar", "si"):
            rcon("set tournament 1")
            return "[ADMIN] tournament ON (habilita freeze mode)", True
        if a in ("off", "0", "desactivar", "no"):
            rcon("set tournament 0")
            return "[ADMIN] tournament OFF", True
        t = rcon("tournament")
        return "[ADMIN] tournament: " + _cl(t)[:40], True
    if verb in ("freeze", "pausar", "congelar", "reanudar", "unfreeze"):
        rcon("sv freeze")
        return "[ADMIN] freeze mode toggulado (requiere tournament ON para actuar)", True

    # ---- experto v2: utilidades ----
    if verb in ("killjugador", "killplayer", "matar"):
        if not arg: return "[ADMIN] uso: killjugador <nombre>", True
        uid, nick = find_player(arg)
        if uid is None: return "[ADMIN] no encuentro a '%s'" % arg, True
        rcon("sv killplayer %d" % uid)
        return "[ADMIN] %s matado (para despegarlo, si estaba trabado)" % nick, True
    if verb in ("reportar", "reporte", "report"):
        if arg:
            log_action(by, "REPORT: " + arg[:120], "reportado a Aldo via cron")
            return "[ADMIN] reporte de %s registrado: '%s' — Aldo lo ve por Telegram" % (by, arg[:60]), True
        return "[ADMIN] uso: reportar <quien> <motivo>", True
    if verb in ("elo", "rating", "ranking", "kda", "stats"):
        # delega al relay (que consulta la BD de elo en la Pi)
        pregunta = "admin, cuál es el elo de %s" % (arg if arg else by)
        reply, action, ok = ask_relay(by, pregunta)
        if ok and (reply or action):
            if action:
                resp, _ = run_cmd(action, by)
                return resp, True
            return reply or "[ADMIN] sin datos de elo", True
        return "[ADMIN] no pude consultar el elo ahora (relay caído?)", True

    # bloqueados explícitamente (aunque lo pidan)
    if verb in ("exec", "set", "seta", "quit", "shutdown", "restart", "reload",
                "restartserver", "addblackhole", "addfiltercmd", "unbindall",
                "rcon_password", "sv_restart", "killserver"):
        log_action(by, text, "BLOQUEADO")
        return "[ADMIN] '%s' no está permitido desde el chat (protección del server)" % parts[0], True

    return HELP, True

RELAY_URL = "http://100.107.202.52:8099/ask"
RELAY_TOKEN_FILE = "/home/ubuntu/dday/.relay-token"

def ask_relay(nick, text):
    """LLM instantáneo. Devuelve (reply, action) o (None, None) si falla -> fallback inbox."""
    import urllib.request, urllib.error
    try:
        token = open(RELAY_TOKEN_FILE).read().strip()
        body = __import__("json").dumps({"token": token, "nick": nick, "text": text}).encode()
        req = urllib.request.Request(RELAY_URL, data=body, method="POST",
                                     headers={"Content-Type": "application/json"})
        with urllib.request.urlopen(req, timeout=10) as r:
            data = __import__("json").loads(r.read().decode())
        reply = str(data.get("reply") or "").strip()
        action = data.get("action") or None
        if action and str(action).lower() in ("null", "none"): action = None
        return (reply, action, True)
    except Exception as e:
        try:
            with open(DEBUG, "a") as f: f.write("relay-fail: %s\n" % e)
        except Exception:
            pass
        return (None, None, False)

PW_WORDS = ("password", "pass", "clave", "contrasena", "contraseña")
PW_STOP = {"password", "pass", "clave", "contrasena", "contraseña", "server", "servidor", "admin",
           "pon", "poner", "ponle", "ponele", "pones", "cambia", "cambiar", "cambiale", "setea",
           "setear", "coloca", "colocar", "activa", "activar", "quita", "quitar", "saca", "sacar",
           "para", "por", "favor", "porfa", "el", "la", "los", "las", "de", "del", "al", "un", "una",
           "que", "es", "y", "o", "a", "con", "sin", "mi", "tu", "su", "nueva", "nuevo", "acceso",
           "entrada", "join", "puedes", "podrias", "podes", "gracias", "server."}

def password_phrase(text):
    """Detecta frases tipo 'ponle password al server, password hola123' -> 'hola123' (o None)."""
    low = text.lower()
    pos = -1
    for kw in PW_WORDS:
        i = low.find(kw)
        if i != -1 and (pos == -1 or i < pos):
            pos = i
    if pos == -1:
        return None
    resto = text[pos:]
    for tok in re.findall(r"[A-Za-z0-9._-]+", resto):
        if norm(tok) in {norm(s) for s in PW_STOP}:
            continue
        if len(tok) >= 4 and re.match(r"^[A-Za-z0-9._-]+$", tok):
            return tok
    return None

FFA_WORDS = ("ffa", "free for all", "freeforall", "todos contra todos", "todoscontratodos")
FFA_OFF = ("saca", "sacar", "quita", "quitar", "apaga", "apagar", "desactiva", "desactivar", "termina",
           "terminar", "para ", "parar", "basta", "volver", "vuelve", "sin ffa", "no mas", "no más")

def ffa_phrase(text):
    """Frases tipo 'admin quiero probar el modo ffa en dust' -> 'ffa dust' ('ffa' = mapa actual).
    None si no piden FFA o piden sacarlo (eso lo resuelve el relay -> normal)."""
    low = " " + text.lower() + " "
    if not any(re.search(r"\b%s\b" % re.escape(w), low) for w in FFA_WORDS):
        return None
    if any(w in low for w in FFA_OFF):
        return None
    for tok in re.findall(r"[a-z0-9_]+", low):
        if tok in MAPS:
            return "ffa " + tok
    return "ffa"

KAMIKAZE_OFF = ("desactiva", "apaga", "saca", "quita", "deshabilita", "termina", "sin ", "off", "no mas",
                "no más", "basta")
KAMIKAZE_ON = ("activa", "prende", "enciende", "pon", "habilita", "mete", "on", "quiero", "probar")

def kamikaze_phrase(text):
    """Frases tipo 'admin activa el arty kamikaze' -> 'kamikaze on' ('apaga...' -> off,
    sin verbo -> estado). None si no hablan del kamikaze."""
    low = " " + text.lower() + " "
    if not re.search(r"\bkamika[sz]e\b|\bkamicase\b", low):
        return None
    if any(w in low for w in KAMIKAZE_OFF):  # primero: 'desactiva' contiene 'activa'
        return "kamikaze off"
    if any(re.search(r"\b%s" % re.escape(w), low) for w in KAMIKAZE_ON):
        return "kamikaze on"
    return "kamikaze"

COOLDOWN = {}

def rate_ok(nick):
    now = time.time()
    q = [t for t in COOLDOWN.get(nick, []) if now - t < 30]
    if len(q) >= 3:
        return False
    q.append(now); COOLDOWN[nick] = q
    return True

CHAT_PATS = [
    r'^\[[^\]]+\]\s*<([^>]+)>\s*(.+)$',
    r'^\[[^\]]+\]\s*([^<\s][^:]{0,40}?):\s+(.+)$',
    r'^\s*([^<>\s][^:]{0,40}?):\s+(.+)$',
]

def parse_chat(line):
    for p in CHAT_PATS:
        m = re.match(p, line)
        if m:
            nick, text = m.group(1).strip(), m.group(2).strip()
            if nick in ALLOW_RAW:
                return nick, text
    n = norm(line)
    for a in ALLOW:
        if a in n:
            m = re.search(r'([^:\s][^:]{0,30}?):\s+(.+)$', line) or re.search(r'<([^>]+)>\s*(.+)', line)
            if m and m.group(1) in ALLOW_RAW:
                return m.group(1), m.group(2)
            try:
                with open(DEBUG, "a") as f: f.write(line + "\n")
            except Exception: pass
    return None

def looks_command(text):
    t = text.strip().lstrip("!").strip()
    if text.strip().startswith("!"): return True
    v = norm(t.split(None, 1)[0] if t else "")
    return v in {"ayuda","help","estado","status","jugadores","players","kick","kickea","echar",
                 "kickban","ban","unban","delban","desban","lock","unlock","bloquear","abrir",
                 "mapa","map","cambiamapa","duelo","duel","publico","publica","cuenta","startcount",
                 "reset","resetcount","normal","modo","clasico","restaurar","default","resetmodo","evento","event","torneoevento","pool","rotacion","rotar","mapaspool","control","ffa","freeforall","todoscontratodos","dm","kamikaze","kamikase","kamicase","say","anuncia","pass","password","clave","contrasena","contraseña","passoff","sinpass","quitaspass","bots","kickbots","quitarbots","kickallbots","screenshot","captura","pantallazo","screenshots","stuff","stuffall","enviaratodos","stuffid","autostuff","autostuffoff","delautostuff","liststuff","stufflista","resetscore","reseteapuntos","resetkills","tiempo","timeleft","tiemporestante","mapas","maplist","listamapas","iniciar","inicia","arrancar","arranca","empezar","empeza","empieza","start","countdown","count","cuentaatras","infomapa","mapainfo","infodelmapa","marcador","score","puntaje","resultados","versiones","clientes","anticheat","lag","pings","conexion","tiempos","conexiones","zona","controlzona","comova","proximamapa","saltarmapa","siguientemapa","torneo","freeze","pausar","congelar","reanudar","unfreeze","killjugador","killplayer","matar","reportar","reporte","report","elo","rating","ranking","kda","stats","bans","listabans","listbans","exec","set","quit","shutdown",
                 "restart","reload"}

def handle_line(line, dry=False):
    cp = parse_chat(line)
    if not cp: return None
    nick, text = cp
    if looks_command(text):
        if not rate_ok(nick):
            if not dry: say("[ADMIN] calma, esperá unos segundos")
            print("RATE-LIMIT %s" % nick)
            return ("rate", nick, text, None)
        aud = re.sub(r"(?i)(pass(word)?\s+)\S+", r"\1****", text)
        resp, ok = run_cmd(text, nick)
        if not dry:
            log_action("test-local" if TESTMODE else nick, aud, resp)
            say(resp)
        print("CMD %s -> %s" % (nick, resp))
        return ("cmd", nick, text, resp)
    else:
        if dry:
            print("CHAT(dry) %s -> %s" % (nick, text))
            return ("chat", nick, text, None)
        pw = password_phrase(text)
        if pw:
            if not rate_ok(nick):
                if not dry: say("[ADMIN] calma, espera unos segundos")
                return ("rate", nick, text, None)
            resp, _ = run_cmd("pass " + pw, nick)
            log_action(nick, "pass ****", resp)
            if not dry: say(resp)
            print("PASS-PHRASE %s -> password cambiada (valor oculto)" % nick)
            return ("passphrase", nick, text, resp)
        if "admin" not in text.lower():
            print("CHAT-IGNORADO (sin 'admin') %s" % nick)
            return ("ignorado", nick, text, None)
        # FFA en frase natural: local, sin LLM (el relay no sabe en que mapa estamos)
        ffa = ffa_phrase(text)
        if ffa:
            if not rate_ok(nick):
                say("[ADMIN] calma, espera unos segundos")
                return ("rate", nick, text, None)
            resp, _ = run_cmd(ffa, nick)
            log_action(nick, "frase:" + ffa, resp)
            say(resp)
            print("FFA-PHRASE %s -> %s" % (nick, resp))
            return ("ffaphrase", nick, text, resp)
        # arty kamikaze en frase natural: local, sin LLM
        kam = kamikaze_phrase(text)
        if kam:
            if not rate_ok(nick):
                say("[ADMIN] calma, espera unos segundos")
                return ("rate", nick, text, None)
            resp, _ = run_cmd(kam, nick)
            log_action(nick, "frase:" + kam, resp)
            say(resp)
            print("KAMIKAZE-PHRASE %s -> %s" % (nick, resp))
            return ("kamikazephrase", nick, text, resp)
        reply, action, ok = ask_relay(nick, text)
        if not ok:
            # fallback: relay caído -> bandeja (Hermes vía cron, ~1 min)
            try:
                with open(INBOX, "a") as f: f.write("%s: %s\n" % (nick, text))
            except Exception: pass
            print("CHAT-FALLBACK %s -> inbox" % nick)
            return ("chat", nick, text, None)
        if action:
            resp, _ = run_cmd(action, nick)
            log_action(nick, "llm:" + action, resp)
            if action.strip().lower().startswith("say"):
                pass          # el anuncio ya salio al chat: no hace falta confirmarlo
            elif resp:
                say(resp)     # UNA sola linea por pedido
            elif reply:
                say(reply)
        elif reply:
            say(reply)
        print("CHAT-LLM %s -> action=%s reply=%s" % (nick, action, reply[:60]))
        return ("llm", nick, text, reply or action)

def daemon():
    if not os.path.exists(LOG):
        print("FATAL: no existe %s" % LOG); sys.exit(1)
    f = open(LOG, encoding="utf-8", errors="replace")
    f.seek(0, 2)
    print("chat-admin escuchando en %s" % LOG); sys.stdout.flush()
    while True:
        line = f.readline()
        if not line: time.sleep(0.3); continue
        try: handle_line(line.rstrip("\n"))
        except Exception as e: print("ERR %s" % e)
        sys.stdout.flush()

def main():
    args = sys.argv[1:]
    global DRY, TESTMODE
    dry = "--dry" in args
    DRY = dry
    TESTMODE = ("--test" in args)
    if "--test" in args:
        i = args.index("--test"); line = args[i+1]
        r = handle_line(line, dry=dry)
        print("resultado:", r)
        return
    if "--cmd" in args:
        i = args.index("--cmd"); by = "hermes"
        if "--by" in args: by = args[args.index("--by")+1]
        resp, _ = run_cmd(args[i+1], by)
        log_action(by, args[i+1], resp)
        say(resp)
        print(resp)
        return
    if any(a in ("-h", "--help") for a in args):
        print("uso: chat-admin.py [--cmd '<comando>' [--by <nick>]]")
        print("Sin argumentos arranca el daemon (una sola instancia permitida).")
        return
    # ── una sola instancia: candado por archivo ──
    global _LOCK
    try:
        _LOCK = open("/home/ubuntu/dday/chat-admin.lock", "w")
        fcntl.flock(_LOCK, fcntl.LOCK_EX | fcntl.LOCK_NB)
        _LOCK.write(str(os.getpid()) + "\n")
        _LOCK.flush()
    except Exception:
        print("ya hay otra instancia de chat-admin corriendo; salgo sin hacer nada")
        return
    daemon()

if __name__ == "__main__":
    main()