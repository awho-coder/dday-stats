# Documentación — D-Day: Normandy (juego y estadísticas)

Índice de la documentación del repositorio.

## Onboarding (leer primero)

Si es tu primera vez en el repo, seguí este orden:

1. [`../README.md`](../README.md) — qué es el repositorio y cómo se compila.
2. [`../stats/README.md`](../stats/README.md) — **cómo funciona el sistema de estadísticas**: cvars del servidor, reglas del ladder y ejemplos de consultas.
3. [`knowledge/`](knowledge/) — **catálogo del esquema** (formato OKF): una página por tabla, vista y función, más las reglas del ladder. Es la forma más rápida de entender la base.
4. [`stats/INSTALACION.md`](stats/INSTALACION.md) — cómo montar PostgreSQL + ingestor + servidor de juego.
5. [`stats/PLAN.md`](stats/PLAN.md) — diseño del sistema y formato de eventos (JSONL).

## Referencia rápida

- [`COMANDOS.md`](COMANDOS.md) — **todos los cvars, comandos y opciones que agregó este fork** (modo control, estadísticas, ingestor, bot admin por chat).

## Cómo contribuir

- **Ramas (GitFlow):** ver [`GITFLOW.md`](GITFLOW.md). Todo el trabajo sale de `develop`; `main` es producción.
- **Si cambiás el esquema** (`stats/schema.sql`): aplicalo con `ingest.py --init-schema` y **regenerá el catálogo**:
  ```sh
  python3 stats/okf_export.py --dsn "$DDAY_STATS_DSN" --out docs/knowledge
  ```
- **Si cambiás el código del ingestor** (`stats/ingest.py`): corré las pruebas con `python3 -m pytest` (si aplica) o al menos una carga de prueba.
- **Commits en español**, cortos y descriptivos (estilo `feat(...)`, `fix(...)`, `docs(...)`).
- **No subir** credenciales, rutas personales, datos de servidores propios ni archivos `*.jsonl` de partidas.
- Las guías del juego (compilación de la DLL) están en el [`../README.md`](../README.md) y [`../readme.linux.txt`](../readme.linux.txt).

## Estadísticas y ladder

| Documento | Para qué |
|---|---|
| [`../stats/README.md`](../stats/README.md) | Reglas del ladder, cvars del servidor, consultas y rendimiento. |
| [`stats/PLAN.md`](stats/PLAN.md) | Diseño: arquitectura, formato de eventos v1 (JSONL) y etapas. |
| [`stats/INSTALACION.md`](stats/INSTALACION.md) | Instalación genérica paso a paso (Windows y Linux, todo en una máquina). |
| [`../stats/`](../stats/) | Código del sistema: `schema.sql`, `ingest.py`, `okf_export.py`, `requirements.txt`. |

## Base de conocimiento (OKF)

[`knowledge/`](knowledge/) es un bundle [Open Knowledge Format](https://github.com/GoogleCloudPlatform/open-knowledge-format)
del sistema: una página por **tabla, vista y función** (generadas desde el
catálogo de PostgreSQL con `stats/okf_export.py`, reusando los comentarios del
esquema) más las **reglas del ladder**. Es una forma navegable y versionada de
recorrer el esquema, legible por humanos y por agentes de IA. Incluye un grafo
autocontenido en `knowledge/viz.html`.

## El juego

| Documento | Para qué |
|---|---|
| [`../README.md`](../README.md) | Qué es el repositorio (código base del juego) y compilación. |
| [`modos/CONTROL.md`](modos/CONTROL.md) | Modo control de zona (estilo Overwatch): reglas, cvars y cómo agregar la zona a un mapa. |
| [`../readme.linux.txt`](../readme.linux.txt) | Notas de compilación en Linux. |
| [`../readme.amiga.txt`](../readme.amiga.txt) | Notas históricas (Amiga). |
| [`../ChangeLog.txt`](../ChangeLog.txt) | Cambios por versión del mod. |
