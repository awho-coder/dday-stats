# Documentación — D-Day: Normandy (juego y estadísticas)

Índice de la documentación de este repositorio. Leé en el orden que necesites.

## Estadísticas y ladder

| Documento | Para qué |
|---|---|
| [`../stats/README.md`](../stats/README.md) | **Empezá por acá.** Reglas del ladder, cvars del servidor, ejemplos de consultas y rendimiento. |
| [`stats/PLAN.md`](stats/PLAN.md) | Diseño: arquitectura, formato de eventos v1 (JSONL) y etapas. |
| [`stats/INSTALACION.md`](stats/INSTALACION.md) | Instalación genérica paso a paso (Windows y Linux, todo en una máquina). |
| [`../stats/`](../stats/) | Código del sistema: `schema.sql`, `ingest.py`, `requirements.txt`. |

## Base de conocimiento (OKF)

[`knowledge/`](knowledge/) es un bundle [Open Knowledge Format](https://github.com/GoogleCloudPlatform/open-knowledge-format)
del sistema: una página por **tabla, vista y función** (generadas desde el
catálogo de PostgreSQL con `stats/okf_export.py`, reusando los comentarios del
esquema) más las **reglas del ladder**. Es una forma navegable y versionada de
recorrer el esquema, legible por humanos y agentes.

## El juego

| Documento | Para qué |
|---|---|
| [`../README.md`](../README.md) | Qué es el repositorio (código base del juego) y compilación. |
| [`../readme.linux.txt`](../readme.linux.txt) | Notas de compilación en Linux. |
| [`../readme.amiga.txt`](../readme.amiga.txt) | Notas históricas (Amiga). |
| [`../ChangeLog.txt`](../ChangeLog.txt) | Cambios por versión del mod. |
