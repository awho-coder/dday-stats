# Estadísticas y ladder

Sistema para guardar las estadísticas de las partidas en PostgreSQL y
construir un ladder (K/D y Elo), perfiles de jugador y estadísticas de mapa.
El diseño está en [`docs/stats/PLAN.md`](../docs/stats/PLAN.md).

```
servidor Q2PRO ──> dday/stats/events/*.jsonl ──> ingest.py ──> PostgreSQL
```

La DLL **no se conecta a la base de datos**: solo escribe un archivo de
texto por partida. Si el disco o la base fallan, el juego sigue normal.

## 1. Servidor de juego

Compilar la DLL (`cd src && make`) y agregar en el `server.cfg`:

```
set stats_log 1                 // activa el registro (se aplica al cargar el siguiente mapa)
set stats_server "chile1"       // opcional: id del servidor (por defecto, hostname)
set stats_log_dir ""            // opcional: carpeta de salida absoluta
```

Por defecto los archivos quedan en `<q2>/dday/stats/events/`. La carpeta se
crea sola. Con contenedores conviene fijar `stats_log_dir` a un volumen
compartido con el ingestor, por ejemplo `/data/events`.

Mientras la partida está en curso el archivo termina en `.jsonl.part`; al
terminar el mapa se renombra a `.jsonl`. Cada partida ocupa del orden de
20–60 KB.

Solo se registran partidas en modo deathmatch (`deathmatch 1`), que es el
modo normal de los servidores de D-Day.

## 2. Base de datos

```sh
createuser dday -P
createdb -O dday dday
pip install -r stats/requirements.txt      # psycopg 3
export DDAY_STATS_DSN="postgresql://dday:clave@localhost/dday"
python3 stats/ingest.py --init-schema       # crea tablas y vistas (idempotente)
```

## 3. Ingestor

```sh
# una pasada (por ejemplo, desde cron cada minuto)
python3 stats/ingest.py --events /srv/q2/dday/stats/events

# como servicio, revisando cada 60 s
python3 stats/ingest.py --events /srv/q2/dday/stats/events --loop 60
```

- Procesa los `.jsonl` en orden cronológico, un archivo por transacción, y
  los mueve a `events/processed/` (con `--delete`, los borra).
- Los archivos con errores de formato se mueven a `events/failed/`.
- Si la base no responde, los archivos quedan donde están y se reintentan.
- Reprocesar un archivo no duplica nada (el id de partida es único).
- Un `.part` sin cambios por más de 6 h (`--stale-hours`) es una partida de
  un servidor caído: se carga como `aborted`, sin rating, y se reconstruyen
  las kills y muertes de cada jugador a partir de los eventos `kill`.

Ejemplo de servicio systemd (`/etc/systemd/system/dday-stats.service`):

```ini
[Unit]
Description=D-Day stats ingestor
After=network-online.target postgresql.service

[Service]
Environment=DDAY_STATS_DSN=postgresql://dday:clave@localhost/dday
ExecStart=/usr/bin/python3 /opt/dday/stats/ingest.py --events /srv/q2/dday/stats/events --loop 60
Restart=always
User=quake2

[Install]
WantedBy=multi-user.target
```

### Reglas del rating

Una partida es **rankeada** si terminó normalmente con ganador o empate,
duró al menos 5 minutos (`--min-duration`) y cada equipo tuvo al menos 2
humanos (`--min-humans`). Los bots nunca cuentan.

Elo por equipos: se compara el rating promedio de cada equipo (ponderado por
el tiempo jugado) y cada jugador gana o pierde
`K × (resultado − esperado) × participación`, con `K = 40` en las primeras
10 partidas y `24` después. Quien jugó menos del 20 % de la partida en su
equipo (`--min-participation`) no se ve afectado.

Si cambian las reglas, `--rebuild-ratings` recalcula el Elo desde cero sobre
las partidas marcadas como rankeadas.

## 4. Consultas

```sql
SELECT * FROM ladder_elo(10);           -- ladder por rating, mínimo 10 partidas rankeadas
SELECT * FROM ladder_kd(10);            -- ladder por K/D, mínimo 10 partidas

SELECT * FROM v_player_profile WHERE name = 'Pato';
-- partidas, victorias, K/D, KPM, % headshots, precisión, horas jugadas,
-- rating, mapa / arma / clase favorita, némesis, víctima favorita, récords

SELECT * FROM v_player_maps WHERE player_id = 1 ORDER BY matches DESC;
SELECT * FROM v_player_weapons WHERE player_id = 1 ORDER BY kills DESC;

SELECT * FROM v_map_stats ORDER BY matches DESC;   -- balance aliados / eje por mapa
SELECT * FROM v_map_weapons WHERE map = 'dday1' ORDER BY kills DESC;
SELECT * FROM v_weapon_stats ORDER BY kills DESC;
SELECT * FROM v_daily_activity ORDER BY day DESC;

-- mapa de calor de muertes en un mapa
SELECT victim_x, victim_y FROM kills k JOIN matches m ON m.id = k.match_id WHERE m.map = 'dday1';
```

## Identidad

La identidad del jugador es su **nombre** (sin colores). Quien cambia de nombre
aparece como otro jugador, y un nombre se puede usar desde otro equipo.
