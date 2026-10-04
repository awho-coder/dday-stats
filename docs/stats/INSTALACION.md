# Instalación del sistema de estadísticas (Windows y Linux)

Guía paso a paso para dejar funcionando las estadísticas y el ladder de
D-Day: Normandy 5.068 o superior. Para el detalle de consultas y reglas, ver
[`stats/README.md`](../../stats/README.md); para el diseño,
[`PLAN.md`](PLAN.md).

## Las tres piezas

```
 1. Servidor de juego          2. Ingestor               3. PostgreSQL
    (DLL de D-Day)                (stats/ingest.py)
    escribe un archivo   ───>     carga los archivos  ───>  guarda y calcula
    por partida                   cada minuto               ladders y rankings
```

- La DLL **nunca** se conecta a la base: solo escribe archivos de texto en
  `dday/stats/events/`. Si la base o el ingestor fallan, el juego sigue
  normal y los archivos esperan.
- Las tres piezas pueden estar en la misma máquina (lo más simple) o en
  máquinas distintas. Si el ingestor está en otra máquina, necesita acceso a
  la carpeta de eventos (carpeta compartida o copia periódica).
- Recursos: el ingestor usa unos pocos MB de RAM; PostgreSQL funciona bien con
  su configuración por defecto. Un año de un servidor de 20 jugadores muy
  activo ocupa unos 600 MB en la base.

---

## 1. Servidor de juego

### Linux (Q2PRO)

Requisitos: `gcc` y `make` (Fedora: `sudo dnf install gcc make`;
Debian/Ubuntu: `sudo apt install build-essential`).

```sh
cd src
make                      # genera gamex86_64.real.so
```

Instalar (respaldando la DLL actual):

```sh
cd /ruta/al/quake2/dday
cp gamex86_64.so gamex86_64.so.respaldo          # si existe
cp /ruta/al/repo/src/gamex86_64.real.so gamex86_64.so
```

Q2PRO de 64 bits busca `gamex86_64.so` dentro de la carpeta `dday`. Si su
servidor usa otro nombre o carga la librería desde otra ruta, use ese.
Para ARM64, descomente las líneas `ARCH=aarch64` / `CC=...` del Makefile.

### Windows

Requisitos: **Visual Studio 2022** con la carga de trabajo
"Desarrollo para el escritorio con C++".

1. Abrir `dday.sln`.
2. Elegir la configuración **Release | Win32** y compilar (Compilar →
   Compilar solución).
3. El proyecto deja la DLL en `C:\Quake2\dday\gamex86.dll`. Si su Quake 2 está
   en otra carpeta, cambie la ruta en Proyecto → Propiedades → General
   ("Directorio de salida") y en Vinculador → General ("Archivo de salida"),
   o copie la DLL a mano. Respalde antes la `gamex86.dll` que esté usando.

> El proyecto solo tiene configuración de **32 bits**, por lo que en Windows
> se necesita un servidor de Quake 2 de 32 bits (por ejemplo Q2PRO o R1Q2 de
> 32 bits).

### Configuración del servidor (ambos sistemas)

Agregar al `server.cfg`:

```
set stats_log 1              // activa el registro (se aplica desde el siguiente mapa)
set stats_server "publico1"  // nombre corto y distinto para cada servidor
set stats_mode "public"      // "public" o "duel"
set sv_allow_map 1           // Q2PRO: necesario para que el mod pueda cambiar de mapa
```

Servidor de **duelos**:

```
set stats_mode "duel"        // solo cuenta lo jugado después de sv startcount
set tournament 1             // necesario para sv freeze (pausa)
```

Los archivos quedan en `dday/stats/events/` (en Windows,
`C:\Quake2\dday\stats\events\`); la carpeta se crea sola. Para usar otra
carpeta (por ejemplo una compartida con el ingestor):
`set stats_log_dir "/srv/dday-events"` o `set stats_log_dir "D:/dday-events"`.

Reinicie el servidor. Al terminar el primer mapa debería aparecer un archivo
`.jsonl` en esa carpeta.

---

## 2. PostgreSQL

Se necesita una base `dday` con un usuario `dday`. Versión 13 o superior
(probado con 16).

### Linux

Fedora:

```sh
sudo dnf install postgresql-server
sudo postgresql-setup --initdb
sudo systemctl enable --now postgresql
```

Debian / Ubuntu:

```sh
sudo apt install postgresql
```

Crear usuario y base (pide una contraseña):

```sh
sudo -u postgres createuser dday -P
sudo -u postgres createdb -O dday dday
```

En **Fedora**, las conexiones por `127.0.0.1` vienen configuradas con `ident`
y la contraseña no funciona. Edite `/var/lib/pgsql/data/pg_hba.conf` y cambie
`ident` por `scram-sha-256` en las líneas `host ... 127.0.0.1/32` y
`host ... ::1/128`; luego `sudo systemctl reload postgresql`.

### Windows

```bat
winget install PostgreSQL.PostgreSQL.16
```

(o el instalador de https://www.postgresql.org/download/windows/). El
instalador pide la contraseña del usuario `postgres`. Luego, en una consola:

```bat
cd "C:\Program Files\PostgreSQL\16\bin"
createuser -U postgres -P dday
createdb -U postgres -O dday dday
```

### Alternativa con contenedor (Linux o Windows con Docker/Podman)

```sh
podman run -d --name dday-stats-pg --restart unless-stopped \
  -v dday-stats-pgdata:/var/lib/postgresql/data \
  -e POSTGRES_USER=dday -e POSTGRES_PASSWORD=CAMBIAR -e POSTGRES_DB=dday \
  -p 127.0.0.1:5432:5432 docker.io/library/postgres:16-alpine
```

El volumen `dday-stats-pgdata` conserva los datos aunque se borre el
contenedor. Sin `-v`, **los datos se pierden** al detenerlo.

> **Las tablas no se crean aquí.** La base queda **vacía** (solo el usuario y
> la base). Las tablas, vistas y funciones las crea el ingestor con
> `ingest.py --init-schema` al final de la sección 3; no hace falta ejecutar
> SQL a mano.

---

## 3. Ingestor

Requisitos: Python 3.9 o superior.

La cadena de conexión tiene esta forma (cambie la contraseña):

```
postgresql://dday:CONTRASEÑA@localhost/dday
```

### Linux

```sh
sudo mkdir -p /opt/dday-stats
sudo cp -r stats /opt/dday-stats/
python3 -m venv /opt/dday-stats/venv
/opt/dday-stats/venv/bin/pip install -r /opt/dday-stats/stats/requirements.txt

export DDAY_STATS_DSN="postgresql://dday:CONTRASEÑA@localhost/dday"
/opt/dday-stats/venv/bin/python /opt/dday-stats/stats/ingest.py --init-schema
```

Servicio `systemd` (`/etc/systemd/system/dday-stats.service`):

```ini
[Unit]
Description=D-Day stats ingestor
After=network-online.target postgresql.service

[Service]
Environment=DDAY_STATS_DSN=postgresql://dday:CONTRASEÑA@localhost/dday
ExecStart=/opt/dday-stats/venv/bin/python /opt/dday-stats/stats/ingest.py --events /ruta/al/quake2/dday/stats/events --loop 60
Restart=always
User=quake2

[Install]
WantedBy=multi-user.target
```

`User` debe poder leer y escribir en la carpeta de eventos (normalmente el
mismo usuario que corre el servidor de juego), porque el ingestor mueve los
archivos cargados a `processed/`.

```sh
sudo systemctl daemon-reload
sudo systemctl enable --now dday-stats
journalctl -u dday-stats -f          # ver lo que va cargando
```

### Windows

```bat
winget install Python.Python.3.12
```

En una consola (ajuste las rutas):

```bat
mkdir C:\dday-stats
xcopy /E /I stats C:\dday-stats\stats
py -m venv C:\dday-stats\venv
C:\dday-stats\venv\Scripts\pip install -r C:\dday-stats\stats\requirements.txt

C:\dday-stats\venv\Scripts\python C:\dday-stats\stats\ingest.py ^
  --dsn "postgresql://dday:CONTRASEÑA@localhost/dday" --init-schema
```

Para que corra solo al iniciar Windows, cree una tarea programada (consola
como administrador; todo en una línea):

```bat
schtasks /create /tn "DDay Stats" /sc onstart /ru SYSTEM /f /tr "C:\dday-stats\venv\Scripts\pythonw.exe C:\dday-stats\stats\ingest.py --dsn postgresql://dday:CONTRASEÑA@localhost/dday --events C:\Quake2\dday\stats\events --loop 60 --log-file C:\dday-stats\ingest.log"
schtasks /run /tn "DDay Stats"
```

`pythonw.exe` corre sin ventana; el registro queda en
`C:\dday-stats\ingest.log`. Para detenerla: `schtasks /end /tn "DDay Stats"`.
La contraseña de la base queda visible en la definición de la tarea, así que
use un usuario de base dedicado solo a esto (como `dday`).

---

## 4. Comprobar que funciona

1. Jugar un mapa completo en el servidor.
2. Debe aparecer un `.jsonl` en la carpeta de eventos y, al minuto, pasar a
   `processed/`.
3. Consultar la base:

```sh
psql -U dday -h localhost -d dday -c "SELECT * FROM v_seasons;"
psql -U dday -h localhost -d dday -c "SELECT * FROM ladder_kd(1);"
```

(En Windows, `psql` está en `C:\Program Files\PostgreSQL\16\bin`.) También
sirve cualquier cliente gráfico (DBeaver, pgAdmin, Beekeeper Studio) con
host `localhost`, puerto `5432`, base `dday`, usuario `dday`.

Para que una partida sea **rankeada** (sume al Elo) hacen falta al menos
2 humanos por equipo en público y 1 en duelos, y una duración mínima; las
demás igual aparecen en el K/D. Los bots nunca cuentan. Ver las reglas en
`stats/README.md`.

---

## 5. Operación diaria

| Tarea | Comando |
|---|---|
| Nueva temporada (reinicia el ladder) | `ingest.py --new-season "Temporada 2"` |
| Duelo oficial de torneo | en el servidor: `set stats_event "copa-verano"` antes de `sv startcount` |
| Fin del torneo | en el servidor: `set stats_event ""` (si no, los duelos siguientes cuentan como oficiales) |
| Marcar un duelo como oficial después | `ingest.py --set-event <id_partida> "copa-verano"` |
| Recalcular resúmenes / Elo | `ingest.py --rebuild-stats` / `ingest.py --rebuild-ratings` |
| Actualizar el esquema tras una versión nueva | `ingest.py --init-schema` (migra solo, sin perder datos) |

Los comandos de `ingest.py` necesitan la conexión: variable `DDAY_STATS_DSN`
o el parámetro `--dsn`.

**Respaldo de la base** (recomendado, por ejemplo diario):

```sh
pg_dump -U dday -h localhost -Fc -f dday-$(date +%F).dump dday        # Linux
```
```bat
pg_dump -U dday -h localhost -Fc -f C:\respaldos\dday.dump dday       :: Windows
```

Para restaurar: `pg_restore -U dday -h localhost -d dday --clean <archivo>`.

**Archivos procesados:** quedan en `events/processed/` (unos 50 KB por
partida). Se pueden borrar o comprimir cada cierto tiempo, o correr el
ingestor con `--delete` para que los borre al cargarlos. Conviene conservarlos
unas semanas por si hay que recargar algo.

---

## 6. Actualizar a una versión nueva

1. Compilar e instalar la DLL nueva (sección 1) y reiniciar el servidor.
2. Copiar la carpeta `stats/` nueva sobre la instalada.
3. Ejecutar `ingest.py --init-schema`: aplica las migraciones y recalcula lo
   necesario sin perder datos.
4. Reiniciar el ingestor (`systemctl restart dday-stats` o
   `schtasks /end` + `schtasks /run`).

---

## 7. Problemas comunes

| Síntoma | Causa / solución |
|---|---|
| No aparece ningún archivo en `stats/events` | `stats_log` no está en 1, falta `deathmatch 1`, o el servidor no tiene permiso de escritura en la carpeta. La consola del servidor muestra `StatsLog: no se pudo crear...` |
| El mapa no cambia al terminar (Q2PRO) | Falta `set sv_allow_map 1` |
| Un duelo no se guardó | En modo duelo, si no se usó `sv startcount` (o se canceló con `sv resetcount`), la partida no se guarda |
| `sv freeze` dice "only works in tournament mode" | Falta `set tournament 1` |
| Duelos casuales aparecen como oficiales | Quedó puesta `stats_event`; corregir con `set stats_event ""` y `ingest.py --set-event <id> ""` |
| El ingestor dice `connection refused` / `password authentication failed` | PostgreSQL no está corriendo, el puerto es otro, o en Fedora falta cambiar `ident` por `scram-sha-256` (sección 2) |
| Archivos `.part` antiguos en la carpeta | Partidas de un servidor que se cayó. El ingestor los carga solos como "abortadas" pasadas 6 h (`--stale-hours`) |
| Archivos en `events/failed/` | Archivos dañados; el registro del ingestor dice por qué. No afectan al resto |
