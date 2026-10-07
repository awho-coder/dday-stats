#!/usr/bin/env python3
# quake-llm-relay — LLM instantáneo para el admin del server Q2.
# La API key NUNCA sale de este archivo de entorno (solo root/pi en esta Pi).
import json, os, re, time, hmac, threading, uuid, urllib.request, urllib.error
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

HERE = os.path.dirname(os.path.abspath(__file__))
ENV = dict(l.strip().split("=", 1) for l in open(os.path.expanduser("~/.hermes/.env"))
           if "=" in l and not l.startswith("#"))
API_KEY = ENV.get("OPENCODE_GO_API_KEY", "")
BASE  = "https://opencode.ai/zen/go/v1"
MODEL = "deepseek-v4.1-flash"
TOKEN = open(os.path.join(HERE, "token")).read().strip()
BIND, PORT = "100.107.202.52", 8099
MAX_PER_MIN = 10
RATE = {}
LOCK = threading.Lock()

SYSTEM = (
"Sos el admin IA del server de Quake 2 D-Day 'D-Day Chile Ladder Test' (mod D-Day Normandy, IP 146.181.54.5:27910). "
"Hablas en el chat del juego con jugadores autorizados (solo 2). Contesta EN ESPANOL, breve (max 180 caracteres), "
"con buena onda, como admin del server. Podes responder con texto o pedir una ACCION (solo estos verbos exactos):\n"
"estado | ayuda | bans | tiempo | mapas | infomapa | marcador | zona | versiones | lag | tiempos | elo <nick> | "
        "kick <nombre> | kickban <nombre> | ban <ip> | unban <ip> | killjugador <nombre> | reportar <quien> <motivo> | "
        "mapa <nombre> | duelo <mapa> | publico | normal | evento <nombre> | dm <mapa> | proximamapa | torneo on|off | freeze | "
        "lock | unlock | say <texto> | pass <nueva> | passoff | bots on|off | kickbots | screenshot | stuff <cmd> | "
        "autostuff <cmd> | autostuffoff | resetscore | "
        " | "
"cuenta (también: iniciar conteo / tira la cuenta) | reset | resetscore | tiempo | mapas | bans | "
        "dm <mapa> | lock | unlock | say <texto> | pass <nueva> | passoff | ayuda | "
        "bots on|off | kickbots | screenshot | stuff <cmd> | autostuff <cmd> | autostuffoff\n"
        "Intenciones: 'sacar los bots'->bots off; 'volver bots'->bots on; 'screenshot a todos'->screenshot; "
        "'iniciar/tirar el conteo'->cuenta; 'resetear puntos'->resetscore; 'qué mapas hay'->mapas; "
        "'quién está baneado'->bans; 'modo normal'/'volver a normal'/'modo clasico'/'dejalo normal'->normal "
        "(deja dday2 en DM public sin control); 'volver a la rotacion'/'rotacion de siempre'->pool; 'modo control': NO se puede por chat, lo activa solo el dueño "
        "del server (reply explicandolo, action=null; ofrece 'dm <mapa>' para jugar ese mapa en DM); "
        "'torneo/evento <nombre>'->evento <nombre>; 'terminar torneo'/'sacar el evento'->evento off.\n"
"Para preguntas de estado ('quien hay', 'en que mapa vamos', 'como estamos') usa action='estado' y reply=''.\n"
"Saludos/dudas/charla: reply con texto y action=null. Rotacion actual (9): doomtown, dust, eurovilla3, "
        "market1, nuenen, outpost, tebessa2, townwar, invade6; hay mas mapas sueltos si piden otro. "
"eurovilla, inland4. NUNCA pidas ni reveles passwords; si tocan la de RCON: reply='Eso no se hace por el juego, "
"mandamelo por Telegram' y action=null. Solo si te insultan o podes ayudar con algo del server, segui ahi; nunca "
"inventes comandos fuera de la lista.\n"
"REGLA DE ORO: si el mensaje NO contiene la palabra admin, responde EXACTAMENTE {\"reply\":\"\",\"action\":null} "
"(silencio total, NO ejecutes ninguna accion). Cuando te digan admin: respuestas SIEMPRE cortas, 1 sola frase, "
"maximo 100 caracteres. NUNCA pidas ni reveles passwords; si tocan la de RCON: reply='Eso no se hace por el juego, "
"mandamelo por Telegram' y action=null. Passwords: NUNCA reveles ni repitas passwords; si piden cambiarla, responde 'usa el comando pass <nueva> en el chat' y action=null. Responde SOLO JSON: {\"reply\":\"texto o vacio\",\"action\":\"verbatim o null\"}")

def sanitize(t):
    import re as _re
    return _re.sub(r'(?i)\b(pass(word)?|contraseña|contrasena|clave)\b(\s+\S+)?',
                   '\1 [REDACTADO]', t)


def elo_db():
    """Top de elo (BD local de la Pi)."""
    import subprocess as _sp
    try:
        sql = ("SELECT p.name || ' | ' || round(r.rating)::text || ' | ' || r.games || 'PJ ' || r.wins "
               "|| 'G ' || r.losses || 'P ' FROM ratings r JOIN players p ON p.id = r.player_id "
               "WHERE r.kind = 'public' AND r.season_id = (SELECT max(season_id) FROM ratings) "
               "ORDER BY r.rating DESC LIMIT 15")
        out = _sp.run(["docker", "exec", "dday-stats-pg", "psql", "-U", "dday", "-d", "dday", "-Atc", sql],
                      capture_output=True, text=True, timeout=8)
        return (out.stdout or "").strip()[:600]
    except Exception:
        return ""

def llm(user_text, nick):
    body = json.dumps({
        "model": MODEL,
        "messages": [{"role": "system", "content": SYSTEM},
                     {"role": "user", "content": "Jugador %s dice: %s" % (nick, user_text)}],
        "max_tokens": 1200,
        "response_format": {"type": "json_object"},
    }).encode()
    req = urllib.request.Request(BASE + "/chat/completions", data=body, method="POST", headers={
        "Content-Type": "application/json",
        "Authorization": "Bearer " + API_KEY,
        "x-opencode-session": "quake-admin-" + uuid.uuid4().hex,
        "User-Agent": "curl/8.5.0",
    })
    with urllib.request.urlopen(req, timeout=9) as r:
        data = json.loads(r.read().decode())
    content = data["choices"][0]["message"].get("content") or ""
    m = re.search(r"\{.*\}", content, re.S)
    if not m: raise ValueError("sin JSON")
    out = json.loads(m.group(0))
    reply = str(out.get("reply") or "").strip()[:190]
    action = str(out.get("action") or "").strip() or None
    if action and action.lower() in ("null", "none", ""): action = None
    return reply, action

class H(BaseHTTPRequestHandler):
    def log_message(self, *a): pass
    def _send(self, code, obj):
        b = json.dumps(obj).encode()
        self.send_response(code); self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(b))); self.end_headers(); self.wfile.write(b)
    def do_GET(self):
        self._send(200, {"ok": True, "svc": "quake-llm-relay"})
    def do_POST(self):
        if self.path != "/ask": return self._send(404, {"error": "no"})
        try:
            n = int(self.headers.get("Content-Length", 0))
            req = json.loads(self.rfile.read(n))
        except Exception:
            return self._send(400, {"error": "json"})
        if not hmac.compare_digest(str(req.get("token", "")), TOKEN):
            return self._send(403, {"error": "token"})
        nick = str(req.get("nick", ""))[:40]; text = str(req.get("text", ""))[:500]
        if not text: return self._send(400, {"error": "vacio"})
        ip = self.client_address[0]
        now = time.time()
        with LOCK:
            q = [t for t in RATE.get(ip, []) if now - t < 60]
            if len(q) >= MAX_PER_MIN:
                return self._send(429, {"error": "rate"})
            q.append(now); RATE[ip] = q
        try:
            prompt_user = sanitize(text)
            if __import__("re").search(r"(?i)\b(elo|rating|ranking|kda|stats)\b", prompt_user):
                db = elo_db()
                if db:
                    prompt_user += "\n\nELO ACTUAL (BD del server):\n" + db
            reply, action = llm(prompt_user, nick)
            return self._send(200, {"reply": reply, "action": action})
        except Exception as e:
            return self._send(503, {"error": "llm", "detail": str(e)[:120]})

if __name__ == "__main__":
    if not API_KEY: raise SystemExit("falta OPENCODE_GO_API_KEY")
    srv = ThreadingHTTPServer((BIND, PORT), H)
    print("quake-llm-relay en %s:%d" % (BIND, PORT))
    srv.serve_forever()