#!/usr/bin/env python3
# chat-admin.py — admin bot por chat para D-Day (q2pro) — whitelist duro de comandos
import os, re, sys, time, socket, fcntl

LOG    = "/var/data/dday/logs/console.log"
CFG    = "/home/ubuntu/dday/dday/server.cfg"
INBOX  = "/home/ubuntu/dday/chat-inbox"           # chat conversacional -> Hermes
RUNLOG = "/home/ubuntu/dday/chat-admin.actions"   # auditoría de acciones
DEBUG  = "/home/ubuntu/dday/chat-admin.debug"     # calibración de formato
ALLOW  = {"chlsnako", "peruano", "mrroman"}            # solo para heurística de debug
ALLOW_RAW = (">ChL<Snako", "[MR]+Peruano+", "[MR]+roman")  # match EXACTO anti-squatting
ALLOW_SUB = ("peruano",)   # pedido del dueño: cualquier nick que contenga esto (cambia de clan/tag)
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

def is_allowed(nick):
    return nick in ALLOW_RAW or any(a in norm(nick) for a in ALLOW_SUB)

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
        "control <mapa>, ffa <mapa>, dm <mapa>, kamikaze on|off, medic on|off, medic bazooka on|off, dardo on|off, officer bonus on|off, tab nuevo on|off, "
        "grabar on|off|ahora, votacion on|off|cancelar, alias <nick>, lock, unlock, say <texto>, ayuda")

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
        # el DLL exige 'sv startcount <minutos>': los del argumento, o el timelimit, o 20
        m = re.match(r"^\d{1,3}$", arg.strip())
        minutos = int(m.group(0)) if m else 0
        if minutos <= 0:
            tl = re.search(r'is\s+"(\d+)', rcon("timelimit"))
            minutos = int(tl.group(1)) if tl else 0
        if minutos <= 0:
            minutos = 20
        r = _cl(rcon("sv startcount %d" % minutos))
        if re.search(r"Uso:|mayor a 0|remaining time", r):   # errores del DLL (con pausa pendiente: 'freeze')
            return "[ADMIN] el server no lanzó la cuenta: %s" % r.replace(chr(10), " ")[:90], True
        return "[ADMIN] cuenta atrás de %d min lanzada — ¡a jugar! (la cuenta la pide %s)" % (minutos, by), True
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
    if verb in ("alias", "aliases"):
        # otros nicks desde la misma IP: la respuesta va solo a la consola del admin, nunca al chat
        txt = alias_lookup(arg)
        if by != "hermes" and tell_private(by, txt):
            return None, True
        return txt, True
    if verb in ("votacion", "votaciones"):
        # votacion de mapa por chat abierta a todos (!votemap <mapa>, !si, !no); se guarda en VOTEFLAG
        a = norm(arg)
        if a in ("on", "1", "si", "activar", "activa", "prender", "prende", "encender", "enciende"):
            vote_set(True)
            return ("[ADMIN] votacion por chat ON: cualquiera puede escribir !votemap <mapa> o !votekick <nick> "
                    "y votar con !si / !no"), True
        if a in ("off", "0", "no", "desactivar", "desactiva", "apagar", "apaga"):
            vote_set(False)
            return "[ADMIN] votacion por chat OFF", True
        if a in ("cancelar", "cancela", "parar", "para"):
            if not VOTE["active"]: return "[ADMIN] no hay ninguna votacion en curso", True
            VOTE["active"] = False
            return "[ADMIN] votacion cancelada", True
        return "[ADMIN] votacion por chat: %s (uso: votacion on|off|cancelar)" % ("ON" if vote_enabled() else "OFF"), True
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
    if verb in ("grabar", "graba", "grabacion", "grabacin", "mvd"):
        # MVD de la partida con nombre = id de la partida, para bajarlo desde la web.
        # mvd_partidas es cvar de la DLL (no latched); un reinicio del server lo apaga
        a = norm(arg)
        if a in ("on", "1", "si", "siempre", "todas", "activar", "activa", "prender", "prende", "encender", "enciende"):
            rcon("set mvd_partidas 1")
            return ("[ADMIN] grabacion ON: se graba un MVD de cada partida, desde esta "
                    "(se baja desde la web, en la partida)"), True
        if a in ("off", "0", "no", "desactivar", "desactiva", "apagar", "apaga", "parar", "para"):
            rcon("set mvd_partidas 0")
            return "[ADMIN] grabacion OFF: no se graban las proximas partidas (la actual se termina de grabar)", True
        if a in ("ahora", "esta", "partida", "actual", "ya"):
            out = rcon("sv mvd_partida")
            if "grabando" in out and "ya se" not in out:
                return "[ADMIN] grabando esta partida (solo esta; se baja desde la web al terminar)", True
            if "ya se esta grabando" in out:
                return "[ADMIN] esta partida ya se esta grabando", True
            if "stats_log" in out:
                return "[ADMIN] no se puede grabar: las stats estan apagadas (stats_log 0)", True
            return "[ADMIN] no se pudo grabar: no hay partida en curso", True
        m = re.search(r'is\s+"(\d+)"', rcon("mvd_partidas"))
        estado = "?" if not m else ("ON" if m.group(1) != "0" else "OFF")
        return "[ADMIN] grabar todas las partidas: %s (uso: grabar on|off|ahora)" % estado, True
    if verb in ("officer", "oficial", "officers", "oficiales"):
        # cvar de la DLL (no latched): rige al instante; un reinicio del server lo apaga
        a = norm(arg)
        if not a.startswith("bonus"):
            return "[ADMIN] uso: officer bonus on|off", True
        b = a[len("bonus"):]
        if b in ("on", "1", "si", "activar", "activa", "prender", "prende", "encender", "enciende", "poner", "pon"):
            rcon("set officer_bonus 1")
            return ("[ADMIN] BONUS del Officer activado: cada kill de un Officer le resta 5 segundos "
                    "a la espera del airstrike de su equipo"), True
        if b in ("off", "0", "no", "desactivar", "desactiva", "apagar", "apaga", "quitar", "quita", "sacar", "saca"):
            rcon("set officer_bonus 0")
            return "[ADMIN] bonus del Officer DESACTIVADO: la espera del airstrike vuelve a ser normal", True
        m = re.search(r'is\s+"(\d+)"', rcon("officer_bonus"))
        estado = "?" if not m else ("ON" if m.group(1) != "0" else "OFF")
        return "[ADMIN] bonus del Officer: %s (uso: officer bonus on|off)" % estado, True
    if verb in ("tab", "scoreboard"):
        # cvar de la DLL (no latched): el TAB de equipos nuevo (panel del FFA, banderas, columna Cura del Medic)
        # rige al instante; un reinicio del server lo apaga
        a = norm(arg)
        if not (a.startswith("nuevo") or a.startswith("panel")):
            return "[ADMIN] uso: tab nuevo on|off", True
        b = a[len("nuevo"):] if a.startswith("nuevo") else a[len("panel"):]
        if b in ("on", "1", "si", "activar", "activa", "prender", "prende", "encender", "enciende", "poner", "pon"):
            rcon("set tab_panel 1")
            return "[ADMIN] TAB nuevo activado: panel con las banderas, +/- y la curacion del Medic (TAB dos veces: rating y punteria)", True
        if b in ("off", "0", "no", "desactivar", "desactiva", "apagar", "apaga", "quitar", "quita", "sacar", "saca"):
            rcon("set tab_panel 0")
            return "[ADMIN] TAB nuevo DESACTIVADO: vuelve el TAB clasico", True
        m = re.search(r'is\s+"(\d+)"', rcon("tab_panel"))
        estado = "?" if not m else ("ON" if m.group(1) != "0" else "OFF")
        return "[ADMIN] TAB nuevo: %s (uso: tab nuevo on|off)" % estado, True
    if verb in ("dardo", "sleepdart"):
        # el dardo de sueno viene con el Medic nuevo (medic_new): 'dardo on|off' es 'medic on|off'
        return run_cmd(("medic " + arg).strip(), by)
    if verb in ("medic", "medico", "mdico"):  # norm() borra los acentos: médico -> mdico
        # cvar latched de la DLL: el Medic nuevo (Browning, botiquin y jeringas lanzables) rige desde el
        # proximo mapa; un reinicio del server lo deja como diga server.cfg
        a = norm(arg)
        if a.startswith(("bazooka", "bazuca", "lanzajeringa")):
            # 'medic bazooka on|off': el lanzajeringas (medic_bazooka, tambien latched)
            b = re.sub(r"^(bazooka|bazuca|lanzajeringas?)", "", a)
            if b in ("on", "1", "si", "activar", "activa", "prender", "prende", "poner", "pon"):
                rcon("set medic_bazooka 1")
                return ("[ADMIN] LANZAJERINGAS desde el proximo mapa: el Medic cambia la pistola por una "
                        "bazooka de 15 jeringas, 5 tiros"), True
            if b in ("off", "0", "no", "desactivar", "desactiva", "apagar", "apaga", "quitar", "quita", "sacar", "saca"):
                rcon("set medic_bazooka 0")
                return "[ADMIN] lanzajeringas DESACTIVADO desde el proximo mapa: el Medic vuelve a su pistola", True
            r = rcon("medic_bazooka")
            m = re.search(r'is\s+"(\d+)"', r)
            estado = "?" if not m else ("ON" if m.group(1) != "0" else "OFF")
            lat = re.search(r'latched:?\s+"(\d+)"', r)
            if lat and m and lat.group(1) != m.group(1):
                estado += " (en el proximo mapa: %s)" % ("ON" if lat.group(1) != "0" else "OFF")
            return "[ADMIN] lanzajeringas: %s (uso: medic bazooka on|off)" % estado, True
        if a in ("on", "1", "si", "nuevo", "activar", "activa", "prender", "prende", "encender", "enciende", "poner", "pon"):
            rcon("set medic_new 1")
            return ("[ADMIN] Medic NUEVO desde el proximo mapa: Browning, botiquin lanzable, jeringas "
                    "lanzables (aim + fuego) y dardo de sueno ('bind <tecla> dardo' con la jeringa)"), True
        if a in ("off", "0", "no", "clasico", "clsico", "desactivar", "desactiva", "apagar", "apaga", "quitar", "quita", "sacar", "saca"):
            rcon("set medic_new 0")
            return "[ADMIN] Medic CLASICO desde el proximo mapa: la pistola de su faccion y solo la jeringa", True
        r = rcon("medic_new")
        m = re.search(r'is\s+"(\d+)"', r)
        estado = "?" if not m else ("NUEVO" if m.group(1) != "0" else "CLASICO")
        lat = re.search(r'latched:?\s+"(\d+)"', r)
        if lat and m and lat.group(1) != m.group(1):
            estado += " (en el proximo mapa: %s)" % ("NUEVO" if lat.group(1) != "0" else "CLASICO")
        return "[ADMIN] Medic: %s (uso: medic on|off)" % estado, True
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
            if not is_allowed(nick):
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

MEDIC_ON_VERBS = ("activa", "prende", "enciende", "habilita", "pon ", "mete")
MEDIC_CLASSIC = ("clasico", "clásico", "normal", "viejo", "original", "antiguo")

def officer_phrase(text):
    """Frases tipo 'admin activa el bonus del officer' / 'apaga el bonus de arty' -> 'officer bonus on|off'
    (sin verbo -> estado). Hace falta 'bonus' y nombrar al officer, el arty o el airstrike.
    None si no hablan del bonus."""
    low = " " + text.lower() + " "
    if not re.search(r"\bbonus\b", low) or not re.search(r"officer|oficial|\barty\b|airstrike|cooldown|bombardeo", low):
        return None
    low = re.sub(r"officers?|oficial(es)?", " ", low)  # 'officer' contiene 'off', que es un verbo de apagar
    if any(w in low for w in KAMIKAZE_OFF):
        return "officer bonus off"
    if any(re.search(r"\b%s" % re.escape(w), low) for w in KAMIKAZE_ON + ("dale", "da ")):
        return "officer bonus on"
    return "officer bonus"

def tab_phrase(text):
    """Frases tipo 'admin activa el tab nuevo' / 'pon el marcador nuevo' -> 'tab nuevo on' ('apaga...' -> off,
    sin verbo -> estado). Hace falta 'tab'/'marcador'/'scoreboard' Y 'nuevo'/'panel': 'admin marcador' y
    'admin score' siguen siendo los comandos que muestran el resultado. None si no hablan del TAB nuevo."""
    low = " " + text.lower() + " "
    if not re.search(r"\b(tab|marcador|scoreboard)\b", low) or not re.search(r"\b(nuevo|panel)\b", low):
        return None
    if any(w in low for w in KAMIKAZE_OFF):  # primero: 'desactiva' contiene 'activa'
        return "tab nuevo off"
    if any(re.search(r"\b%s" % re.escape(w), low) for w in KAMIKAZE_ON + ("dale", "da ")):
        return "tab nuevo on"
    return "tab nuevo"

def medic_phrase(text):
    """Frases tipo 'admin activa el medic nuevo' -> 'medic on'; 'apaga el medic nuevo' o
    'deja el medic clasico' -> 'medic off'; si solo nombran al medic -> estado. Mas estricto que el
    kamikaze: 'quiero un medico' no prende nada (hace falta 'nuevo' o un verbo de activar).
    None si no hablan del medic."""
    low = " " + text.lower() + " "
    # el dardo de sueno: 'activa el dardo', 'saca el dardo del medic' (antes que el Medic: nombran a los dos)
    if re.search(r"\bdardos?\b|\bsleep ?dart", low):
        if any(w in low for w in KAMIKAZE_OFF):
            return "dardo off"
        if any(re.search(r"\b%s" % re.escape(w), low) for w in KAMIKAZE_ON + ("dale", "da ")):
            return "dardo on"
        return "dardo"
    # el lanzajeringas: 'activa la bazooka de jeringas', 'saca el lanzajeringas'
    if re.search(r"lanzajeringa|\bbazooka\b.*jeringa|jeringa.*\bbazooka\b|\bbazuca\b.*jeringa|\bm[eé]dic[oa]?\b.*\bbazooka\b|\bbazooka\b.*\bm[eé]dic", low):
        if any(w in low for w in KAMIKAZE_OFF):
            return "medic bazooka off"
        if any(re.search(r"\b%s" % re.escape(w), low) for w in KAMIKAZE_ON + ("dale", "da ", "entrega")):
            return "medic bazooka on"
        return "medic bazooka"
    if not re.search(r"\bm[eé]dic[oa]?s?\b", low):
        return None
    if any(w in low for w in MEDIC_CLASSIC) or any(w in low for w in KAMIKAZE_OFF):
        return "medic off"
    if re.search(r"\bnuevo\b|\bnew\b", low) or any(re.search(r"\b%s" % re.escape(w), low) for w in MEDIC_ON_VERBS):
        return "medic on"
    return "medic"

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

# ── votacion por chat (para probar antes de la de la DLL) ──
# Cualquier jugador: "!votemap <mapa>" (o "!votar"), "!votekick <nick>", "!si" / "!no", "!voto" (estado).
# Se prende con el comando de admin "votacion on" (queda guardado en VOTEFLAG); apagada por defecto.
VOTEFLAG      = "/home/ubuntu/dday/chat-admin.vote"
GAMEDIR       = "/home/ubuntu/dday/dday"
VOTE_TIME     = 30      # segundos que dura
VOTE_PERCENT  = 50      # mapa: hay que superar este % de los jugadores del server
VOTE_KICK_PERCENT = 75  # kick: % de los jugadores sin contar al acusado
VOTE_KICK_MIN = 3       # jugadores necesarios para votar un kick
VOTE_COOLDOWN = 60      # espera tras una votacion rechazada
KICK_WAIT     = 120     # tras un kick rechazado (quien propuso) y entre kicks al mismo jugador
VOTE_REMIND   = 10
VOTE = {"active": False, "kind": "map", "map": "", "target": None, "target_nick": "", "caller": "",
        "caller_nick": "", "end": 0, "remind": 0, "next": 0, "votes": {}}
KICK_WAIT_CALLER = {}   # nick normalizado -> time.time() hasta el que no puede proponer kick
KICK_WAIT_TARGET = {}   # nick normalizado -> hasta cuando no se le puede votar kick
VOTE_START = {"votemap", "votar", "votamapa", "votemapa", "vmap"}
VOTE_KICK  = {"votekick", "vkick", "votarkick"}
VOTE_YES   = {"si", "yes", "s", "y", "f1"}
VOTE_NO    = {"no", "n", "f2"}
VOTE_INFO  = {"voto", "votacion", "vote"}
_MAPS_CACHE = {"t": 0, "maps": {}}

def vote_enabled():
    try: return open(VOTEFLAG).read().strip() == "1"
    except Exception: return False

def vote_set(on):
    try:
        with open(VOTEFLAG, "w") as f: f.write("1\n" if on else "0\n")
        return True
    except Exception:
        return False

def server_maps():
    """{nombre en minusculas: nombre real} de los .bsp del server (cache de 60 s)."""
    if time.time() - _MAPS_CACHE["t"] > 60:
        maps = {}
        try:
            for fn in os.listdir(os.path.join(GAMEDIR, "maps")):
                if fn.lower().endswith(".bsp"):
                    maps.setdefault(fn[:-4].lower(), fn[:-4])
        except Exception:
            pass
        _MAPS_CACHE.update(t=time.time(), maps=maps)
    return _MAPS_CACHE["maps"]

def map_kind(real):
    """'ok' si es de D-Day (spawns de equipo en ents/<mapa>.ent o en el .bsp), 'nodday', o 'broken'."""
    want = (b"info_team_start", b"info_reinforcements_start")
    try:
        with open(os.path.join(GAMEDIR, "ents", real + ".ent"), "rb") as f:
            e = f.read().lower()
        if any(w in e for w in want): return "ok"
    except Exception:
        pass
    try:
        with open(os.path.join(GAMEDIR, "maps", real + ".bsp"), "rb") as f:
            h = f.read(16)
            if len(h) < 16 or h[:4] != b"IBSP": return "broken"
            ofs, ln = int.from_bytes(h[8:12], "little"), int.from_bytes(h[12:16], "little")
            if ofs <= 0 or not 0 < ln < 8 << 20: return "broken"
            f.seek(ofs); e = f.read(ln).lower()
        return "ok" if any(w in e for w in want) else "nodday"
    except Exception:
        return "broken"

def map_suggest(q, names):
    """Hasta 5 mapas parecidos: primero los que contienen el texto, despues por parecido."""
    import difflib
    q = q.lower()
    out = [n for n in sorted(names) if q in n or (len(n) >= 3 and n in q)][:5]
    for n in difflib.get_close_matches(q, sorted(names), n=5, cutoff=0.6):
        if n not in out and len(out) < 5: out.append(n)
    return out

def vote_parse_chat(line):
    """(nick, texto) de cualquier jugador; ignora al server ('console'), los avisos del bot y el chat de equipo."""
    for p in CHAT_PATS:
        m = re.match(p, line)
        if m:
            nick, text = m.group(1).strip(), m.group(2).strip()
            if norm(nick) == "console" or nick.startswith("(") or text.startswith("["):
                return None
            return nick, text
    return None

def vote_needed(total):
    pct = VOTE_KICK_PERCENT if VOTE["kind"] == "kick" else VOTE_PERCENT
    return total * pct // 100 + 1

def vote_count():
    """(si, no, total) con los jugadores conectados ahora; en un kick el acusado no cuenta."""
    pl = dict(players())
    VOTE["votes"] = {uid: v for uid, v in VOTE["votes"].items() if uid in pl}
    if VOTE["kind"] == "kick":
        pl.pop(VOTE["target"], None)
        VOTE["votes"].pop(VOTE["target"], None)
    yes = sum(1 for v in VOTE["votes"].values() if v)
    no = sum(1 for v in VOTE["votes"].values() if not v)
    return yes, no, len(pl)

def vote_blocked():
    if re.search(r'is\s+"1', rcon("tournament")): return "no se puede votar durante un torneo"
    if re.search(r'is\s+"duel', rcon("stats_mode")): return "no se puede votar durante un duelo"
    return None

def vote_what():
    return "echar a %s" % VOTE["target_nick"] if VOTE["kind"] == "kick" else "cambiar a %s" % VOTE["map"]

def vote_target_present():
    """En un kick: el acusado sigue en su slot (mismo nick)."""
    return dict(players()).get(VOTE["target"]) == VOTE["target_nick"]

def vote_finish(passed, why=""):
    VOTE["active"] = False
    kick = VOTE["kind"] == "kick"
    if kick:
        KICK_WAIT_TARGET[norm(VOTE["target_nick"])] = time.time() + KICK_WAIT
    if not passed:
        VOTE["next"] = time.time() + VOTE_COOLDOWN
        if kick:
            KICK_WAIT_CALLER[norm(VOTE["caller_nick"])] = time.time() + KICK_WAIT
        say("[VOTO] rechazada (%s): %s" % (why, vote_what()))
        log_action(VOTE["caller"], "vote " + vote_what(), "rechazada: " + why)
        return
    say("[VOTO] aprobada: %s" % vote_what())
    log_action(VOTE["caller"], "vote " + vote_what(), "aprobada")
    if kick:
        if vote_target_present():
            rcon("kick %d" % VOTE["target"])
    else:
        rcon("map %s" % VOTE["map"])

def vote_check():
    """Decide la votacion si ya alcanza (o ya no puede alcanzar). True si termino."""
    if VOTE["kind"] == "kick" and not vote_target_present():
        VOTE["active"] = False
        say("[VOTO] cancelada: %s se fue del servidor" % VOTE["target_nick"]); return True
    yes, no, total = vote_count()
    if not total:
        vote_finish(False, "no quedan jugadores"); return True
    need = vote_needed(total)
    if yes >= need:
        vote_finish(True); return True
    if total - no < need:
        vote_finish(False, "no alcanzan los votos"); return True
    return False

def vote_status():
    yes, no, total = vote_count()
    return "[VOTO] %s propone %s: si %d, no %d, faltan %d si (%d s). Escribe !si o !no" % (
        VOTE["caller"], vote_what(), yes, no, max(vote_needed(total) - yes, 0),
        max(int(VOTE["end"] - time.time() + 0.9), 0))

def find_target(who, pl):
    """(uid, nick) por numero de slot o nick (exacto o parte unica). (None, motivo) si no."""
    w = who.strip().lstrip("#")
    if w.isdigit():
        d = dict(pl)
        return (int(w), d[int(w)]) if int(w) in d else (None, "no hay ningun jugador con el numero %s" % w)
    n = norm(who)
    if not n: return None, "uso: !votekick <nick o numero>"
    exact = [(u, k) for u, k in pl if norm(k) == n]
    if exact: return exact[0]
    part = [(u, k) for u, k in pl if n in norm(k)]
    if len(part) == 1: return part[0]
    return None, ("hay varios jugadores con '%s', usa el numero" % who[:20]) if part else \
                 ("no hay ningun jugador '%s'" % who[:20])

def vote_open(kind, nick, uid, **kw):
    now = time.time()
    VOTE.update(active=True, kind=kind, caller=nick, caller_nick=nick, end=now + VOTE_TIME,
                remind=now + VOTE_REMIND, votes={uid: True}, **kw)

def vote_line(line, dry=False):
    """Atiende los comandos de votacion de cualquier jugador. None si la linea no es de votacion."""
    cp = vote_parse_chat(line)
    if not cp: return None
    nick, text = cp
    t = text.strip()
    if not t.startswith("!"): return None
    words = t[1:].split()
    if not words: return None
    verb = norm(words[0])
    if verb not in VOTE_START | VOTE_KICK | VOTE_YES | VOTE_NO | VOTE_INFO: return None
    res = ("vote", nick, text, None)
    out = lambda msg: (say(msg) if not dry else None, print("VOTE %s -> %s" % (nick, msg)))
    if not vote_enabled():
        if verb in VOTE_START | VOTE_KICK: out("[VOTO] la votacion por chat esta apagada")
        return res
    if not rate_ok(nick):
        return ("rate", nick, text, None)
    pl = players()
    # el slot del que vota: un voto por slot aunque se cambie el nick
    uid = next((u for u, n in pl if n == nick), None)
    if uid is None:
        print("VOTE %s no aparece en status, se ignora" % nick)
        return res

    if verb in VOTE_INFO:
        out(vote_status() if VOTE["active"] else "[VOTO] uso: !votemap <mapa>, !votekick <nick>, !si, !no")
        return res

    if verb in VOTE_YES | VOTE_NO:
        if not VOTE["active"]:
            out("[VOTO] no hay ninguna votacion en curso"); return res
        if VOTE["kind"] == "kick" and uid == VOTE["target"]:
            out("[VOTO] no puedes votar en tu propio kick"); return res
        VOTE["votes"][uid] = verb in VOTE_YES
        if not vote_check():
            out(vote_status())
        return res

    # abrir una votacion
    if VOTE["active"]:
        out(vote_status()); return res
    if time.time() < VOTE["next"]:
        out("[VOTO] espera %d s para proponer otra" % int(VOTE["next"] - time.time() + 0.9)); return res
    if len(words) < 2:
        out("[VOTO] uso: !votemap <mapa> o !votekick <nick>"); return res
    why = vote_blocked()
    if why:
        out("[VOTO] " + why); return res

    if verb in VOTE_KICK:
        if len(pl) < VOTE_KICK_MIN:
            out("[VOTO] para votar un kick hacen falta %d jugadores (hay %d)" % (VOTE_KICK_MIN, len(pl))); return res
        wait = KICK_WAIT_CALLER.get(norm(nick), 0) - time.time()
        if wait > 0:
            out("[VOTO] espera %d s para proponer otro kick" % int(wait + 0.9)); return res
        tuid, tnick = find_target(" ".join(words[1:]), pl)
        if tuid is None:
            out("[VOTO] " + tnick); return res
        if tuid == uid:
            out("[VOTO] no puedes votar para echarte a ti mismo"); return res
        if is_allowed(tnick):
            out("[VOTO] no se puede votar para echar a un admin"); return res
        wait = KICK_WAIT_TARGET.get(norm(tnick), 0) - time.time()
        if wait > 0:
            out("[VOTO] a %s ya lo votaron hace poco, espera %d s" % (tnick, int(wait + 0.9))); return res
        vote_open("kick", nick, uid, target=tuid, target_nick=tnick)
    else:
        maps = server_maps()
        real = maps.get(words[1].lower())
        if not real:
            sug = map_suggest(words[1], list(maps))
            out("[VOTO] el mapa '%s' no esta en el servidor%s" % (words[1][:20],
                ". Quisiste decir: " + ", ".join(maps[s] for s in sug) + "?" if sug else "")); return res
        kind = map_kind(real)
        if kind != "ok":
            out("[VOTO] '%s' %s" % (real, "esta en el servidor pero no es un mapa de D-Day" if kind == "nodday"
                                    else "tiene el archivo danado, no se puede votar")); return res
        vote_open("map", nick, uid, map=real, target=None, target_nick="")
    if dry:
        print("VOTE %s abre votacion: %s" % (nick, vote_what())); return res
    if not vote_check():
        out(vote_status())
    return res

def vote_tick():
    """Desde el bucle del daemon: vence la votacion y recuerda cada VOTE_REMIND segundos."""
    if not VOTE["active"]: return
    now = time.time()
    if not vote_enabled():
        vote_finish(False, "votacion apagada"); return
    if now >= VOTE["end"]:
        if not vote_check():
            vote_finish(False, "se acabo el tiempo")
        return
    if now >= VOTE["remind"]:
        VOTE["remind"] = now + VOTE_REMIND
        if not vote_check():
            say(vote_status())

# ── nicks protegidos (los revisa la DLL con dday/nicks.txt; aca solo se avisa) ──
NICKS_FILE = os.path.join(GAMEDIR, "nicks.txt")

def nick_rules():
    """[(tipo, substring?, clave normalizada)] de nicks.txt (sin las claves)."""
    rules = []
    try:
        for line in open(NICKS_FILE, encoding="latin1"):
            p = line.strip().split()
            if len(p) < 3 or p[0].startswith("#") or p[0].lower() not in ("nick", "tag"): continue
            name = " ".join(p[1:-1]); sub = len(name) > 2 and name[0] == name[-1] == "*"
            if sub: name = name[1:-1]
            rules.append((p[0].lower(), sub, norm(name)))
    except Exception:
        pass
    return rules

def check_admin_protection():
    """Avisa en el journal si algun admin de la allowlist no esta protegido en nicks.txt."""
    rules = [r for r in nick_rules() if r[0] == "nick"]
    if not rules:
        print("AVISO: no hay %s (o no tiene nicks): cualquiera puede usar el nick de un admin" % NICKS_FILE)
        return
    for nick in ALLOW_RAW:
        n = norm(nick)
        if not any((sub and key in n) or key == n for _, sub, key in rules):
            print("AVISO: el admin %s no esta protegido en nicks.txt" % nick)
    for s in ALLOW_SUB:
        if not any(sub and key in norm(s) for _, sub, key in rules):
            print("AVISO: los nicks con '%s' no estan protegidos en nicks.txt (falta 'nick *%s* clave')" % (s, s))

# ── alias: nicks usados desde la misma IP (base local en Oracle, nunca sale de aca) ──
ALIASDB = "/home/ubuntu/dday/aliases.db"
ALIAS_POLL = 20          # cada cuantos segundos se mira status
ALIAS_KEEP_DAYS = 180
ALIAS_SESSION = 1800     # mas de 30 min sin verlo = otra vez que entro
_ALIAS = {"next": 0, "purge": 0, "db": None}

def alias_db():
    if _ALIAS["db"] is None:
        import sqlite3
        db = sqlite3.connect(ALIASDB)
        db.execute("CREATE TABLE IF NOT EXISTS alias (ip TEXT, nick TEXT, nnick TEXT, first REAL, last REAL,"
                   " times INTEGER, PRIMARY KEY (ip, nick))")
        db.execute("CREATE INDEX IF NOT EXISTS alias_nnick ON alias (nnick)")
        _ALIAS["db"] = db
    return _ALIAS["db"]

def players_ip():
    """[(uid, nick, ip)] de status."""
    res = []
    for line in rcon("status").splitlines():
        m = re.match(r'\s*(\d+)\s+-?\d+\s+\S+\s+(.+?)\s+\d+\s+(\d{1,3}(?:\.\d{1,3}){3}):\d+', line)
        if m: res.append((int(m.group(1)), m.group(2).strip(), m.group(3)))
    return res

def alias_record(rows, now=None):
    now = now or time.time()
    db = alias_db()
    for _, nick, ip in rows:
        if ip.startswith("127."): continue
        cur = db.execute("SELECT last FROM alias WHERE ip=? AND nick=?", (ip, nick)).fetchone()
        if cur is None:
            db.execute("INSERT INTO alias VALUES (?,?,?,?,?,1)", (ip, nick, norm(nick), now, now))
        else:
            db.execute("UPDATE alias SET last=?, times=times+? WHERE ip=? AND nick=?",
                       (now, 1 if now - cur[0] > ALIAS_SESSION else 0, ip, nick))
    if now > _ALIAS["purge"]:
        db.execute("DELETE FROM alias WHERE last < ?", (now - ALIAS_KEEP_DAYS * 86400,))
        _ALIAS["purge"] = now + 86400
    db.commit()

def alias_tick():
    if time.time() < _ALIAS["next"]: return
    _ALIAS["next"] = time.time() + ALIAS_POLL
    rows = players_ip()
    if rows: alias_record(rows)

def alias_lookup(who):
    """Texto con los otros nicks usados desde las mismas IPs que 'who' (solo IP exacta)."""
    db = alias_db(); n = norm(who)
    if not n: return "uso: alias <nick>"
    nicks = [r[0] for r in db.execute("SELECT DISTINCT nick FROM alias WHERE nnick=?", (n,))]
    if not nicks:
        nicks = [r[0] for r in db.execute("SELECT DISTINCT nick FROM alias WHERE nnick LIKE ?", ("%" + n + "%",))]
        if len(nicks) > 1:
            return "alias: hay varios nicks con '%s': %s" % (who, ", ".join(nicks[:8]))
    if not nicks:
        return "alias: no hay datos de '%s'" % who
    ips = [r[0] for r in db.execute("SELECT DISTINCT ip FROM alias WHERE nick=?", (nicks[0],))]
    q = "SELECT nick, SUM(times), MAX(last) FROM alias WHERE ip IN (%s) AND nick<>? GROUP BY nick ORDER BY MAX(last) DESC" \
        % ",".join("?" * len(ips))
    others = db.execute(q, ips + [nicks[0]]).fetchall()
    if not others:
        return "alias %s: sin otros nicks (visto desde %d IP%s)" % (nicks[0], len(ips), "" if len(ips) == 1 else "s")
    return "alias %s: %s" % (nicks[0], ", ".join("%s (%d %s, %s)" % (k, t, "vez" if t == 1 else "veces",
                             time.strftime("%Y-%m-%d", time.localtime(l))) for k, t, l in others[:12]))

def tell_private(nick, text):
    """Manda texto solo a la consola de ese jugador (stuff echo). False si no esta conectado."""
    uid = next((u for u, k in players() if k == nick), None)
    if uid is None: return False
    clean = re.sub(r'[;"$\\\n\r]', "", text)
    for i in range(0, len(clean), 180):
        rcon("stuff %d echo %s" % (uid, clean[i:i + 180]), wait=0.3)
    return True

def parse_chat(line):
    for p in CHAT_PATS:
        m = re.match(p, line)
        if m:
            nick, text = m.group(1).strip(), m.group(2).strip()
            if is_allowed(nick):
                return nick, text
    n = norm(line)
    for a in ALLOW:
        if a in n:
            m = re.search(r'([^:\s][^:]{0,30}?):\s+(.+)$', line) or re.search(r'<([^>]+)>\s*(.+)', line)
            if m and is_allowed(m.group(1)):
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
                 "kickban","ban","votacion","votaciones","alias","aliases","unban","delban","desban","lock","unlock","bloquear","abrir",
                 "mapa","map","cambiamapa","duelo","duel","publico","publica","cuenta","startcount",
                 "reset","resetcount","normal","modo","clasico","restaurar","default","resetmodo","evento","event","torneoevento","pool","rotacion","rotar","mapaspool","control","ffa","freeforall","todoscontratodos","dm","kamikaze","kamikase","kamicase","grabar","graba","grabacion","grabacin","mvd","medic","medico","mdico","dardo","sleepdart","officer","oficial","officers","oficiales","tab","scoreboard","say","anuncia","pass","password","clave","contrasena","contraseña","passoff","sinpass","quitaspass","bots","kickbots","quitarbots","kickallbots","screenshot","captura","pantallazo","screenshots","stuff","stuffall","enviaratodos","stuffid","autostuff","autostuffoff","delautostuff","liststuff","stufflista","resetscore","reseteapuntos","resetkills","tiempo","timeleft","tiemporestante","mapas","maplist","listamapas","iniciar","inicia","arrancar","arranca","empezar","empeza","empieza","start","countdown","count","cuentaatras","infomapa","mapainfo","infodelmapa","marcador","score","puntaje","resultados","versiones","clientes","anticheat","lag","pings","conexion","tiempos","conexiones","zona","controlzona","comova","proximamapa","saltarmapa","siguientemapa","torneo","freeze","pausar","congelar","reanudar","unfreeze","killjugador","killplayer","matar","reportar","reporte","report","elo","rating","ranking","kda","stats","bans","listabans","listbans","exec","set","quit","shutdown",
                 "restart","reload"}

def handle_line(line, dry=False):
    v = vote_line(line, dry)
    if v: return v
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
            log_action("test-local" if TESTMODE else nick, aud, resp or "(respuesta privada)")
            if resp: say(resp)
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
        # bonus del Officer en frase natural: local, sin LLM
        off = officer_phrase(text)
        if off:
            if not rate_ok(nick):
                say("[ADMIN] calma, espera unos segundos")
                return ("rate", nick, text, None)
            resp, _ = run_cmd(off, nick)
            log_action(nick, "frase:" + off, resp)
            say(resp)
            print("OFFICER-PHRASE %s -> %s" % (nick, resp))
            return ("officerphrase", nick, text, resp)
        # TAB de equipos nuevo en frase natural: local, sin LLM
        tabp = tab_phrase(text)
        if tabp:
            if not rate_ok(nick):
                say("[ADMIN] calma, espera unos segundos")
                return ("rate", nick, text, None)
            resp, _ = run_cmd(tabp, nick)
            log_action(nick, "frase:" + tabp, resp)
            say(resp)
            print("TAB-PHRASE %s -> %s" % (nick, resp))
            return ("tabphrase", nick, text, resp)
        # Medic nuevo/clasico en frase natural: local, sin LLM
        med = medic_phrase(text)
        if med:
            if not rate_ok(nick):
                say("[ADMIN] calma, espera unos segundos")
                return ("rate", nick, text, None)
            resp, _ = run_cmd(med, nick)
            log_action(nick, "frase:" + med, resp)
            say(resp)
            print("MEDIC-PHRASE %s -> %s" % (nick, resp))
            return ("medicphrase", nick, text, resp)
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
    print("chat-admin escuchando en %s" % LOG)
    check_admin_protection(); sys.stdout.flush()
    while True:
        try: vote_tick()
        except Exception as e: print("ERR vote %s" % e)
        try: alias_tick()
        except Exception as e: print("ERR alias %s" % e)
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
        log_action(by, args[i+1], resp or "(respuesta privada)")
        if resp and norm(args[i+1].split()[0] if args[i+1].split() else "") not in ("alias", "aliases"):
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