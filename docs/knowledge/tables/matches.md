---
type: PostgreSQL Table
title: matches
description: Una partida (un mapa jugado en un servidor).
resource: postgresql://dday/public/matches
tags: [postgresql, table, ladder]
status: stable
generated: { by: process:okf-export, at: 2026-10-05T01:21:03Z }
sources: 
  - id: schema
    resource: /references/schema.sql
---

# Schema

| Columna | Tipo | Nulo | Descripción |
|---|---|---|---|
| `id` | text | no | Id de partida: <fecha>-<servidor>-<random>. Clave primaria. (PK) |
| `server` | text | no | Id del servidor (cvar stats_server). |
| `map` | text | no | Mapa jugado. |
| `mode` | text | no | Modo del juego: dm, ctb, campaign, control (o el que mande la DLL). |
| `kind` | text | no | Tipo de partida: public o duel (cvar stats_mode). |
| `event` | text | no | Torneo (cvar stats_event); cadena vacia = casual. |
| `season_id` | integer | sí | Temporada a la que pertenece la partida. (FK → [seasons](seasons.md)) |
| `tournament` | boolean | no | true si el servidor estaba en modo torneo. |
| `started_at` | timestamp with time zone | no | Inicio de la partida. |
| `ended_at` | timestamp with time zone | sí | Fin de la partida (NULL si no se cerro). |
| `duration_s` | real | no | Duracion de la partida en segundos. |
| `winner` | smallint | sí | Ganador: 0 aliados, 1 eje, -1 empate, NULL sin ganador. |
| `end_reason` | text | no | Como termino: normal, forced, other o aborted. |
| `team0_army` | text | sí | Ejercito del equipo 0 (aliados). |
| `team1_army` | text | sí | Ejercito del equipo 1 (eje). |
| `team0_score` | integer | sí | Puntaje final del equipo 0. |
| `team1_score` | integer | sí | Puntaje final del equipo 1. |
| `team0_kills` | integer | sí | Kills del equipo 0. |
| `team1_kills` | integer | sí | Kills del equipo 1. |
| `humans` | integer | no | Cantidad de jugadores humanos en la partida. |
| `ranked` | boolean | no | true si la partida cuenta para el Elo. |
| `ingested_at` | timestamp with time zone | no | Cuando se cargo en la base. |

# Relaciones

- Referencia a [seasons](seasons.md).
