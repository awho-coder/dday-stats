#!/usr/bin/env python3
"""RCON al server D-Day en Oracle (q2pro).

El password NO se guarda en el skill: se lee por SSH desde server.cfg en cada uso.

Uso:
  rcon.py                 -> "status"
  rcon.py "cvarlist control"
  rcon.py set control_mode 1
"""
import socket, subprocess, sys, re

KEY = "/home/pi/.ssh/id_ed25519_oracle"
SSH_TARGET = "ubuntu@100.94.135.124"      # por la tailnet (pidash-vnic)
RCON_ADDR = "100.94.135.124"              # 146.181.54.5 = IP publica
PORT = 27910

def get_pw():
    out = subprocess.run(
        ["ssh", "-i", KEY, "-o", "BatchMode=yes", "-o", "ConnectTimeout=10",
         SSH_TARGET,
         "grep -E '^set rcon_password' /home/ubuntu/dday/dday/server.cfg"],
        capture_output=True, text=True, timeout=30).stdout
    m = re.search(r'rcon_password\s+"([^"]+)"', out)
    if not m:
        sys.exit("no se pudo leer rcon_password desde server.cfg")
    return m.group(1)

def rcon(cmd, pw, timeout=6):
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.settimeout(timeout)
    pkt = b"\xff\xff\xff\xffrcon " + pw.encode() + b" " + cmd.encode()
    s.sendto(pkt, (RCON_ADDR, PORT))
    chunks = []
    try:
        while len(chunks) < 40:
            data, _ = s.recvfrom(65535)
            chunks.append(data)
    except socket.timeout:
        pass
    finally:
        s.close()
    return b"".join(chunks).decode("utf-8", "replace").replace("\x00", " ")

if __name__ == "__main__":
    cmd = " ".join(sys.argv[1:]).strip() or "status"
    print(rcon(cmd, get_pw()).strip() or "(sin respuesta)")