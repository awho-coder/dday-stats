---
type: PostgreSQL Table
title: match_players
description: Resumen de un jugador en una partida (suma de sus tramos/reconexiones).
resource: postgresql://dday/public/match_players
tags: [postgresql, table, ladder]
status: stable
generated: { by: process:okf-export, at: 2026-10-04T18:27:40Z }
sources: 
  - id: schema
    resource: /references/schema.sql
---

# Schema

| Columna | Tipo | Nulo | Descripción |
|---|---|---|---|
| `match_id` | text | no | Partida. (PK; FK → [matches](/tables/matches.md)) |
| `player_id` | integer | no | Jugador. (PK; FK → [players](/tables/players.md)) |
| `team` | smallint | sí | Equipo donde jugo mas tiempo: 0 aliados, 1 eje, -1 observador. |
| `time_team0` | integer | no | Segundos jugados en el equipo 0. |
| `time_team1` | integer | no | Segundos jugados en el equipo 1. |
| `kills` | integer | no | Kills a enemigos (humanos o bots). |
| `deaths` | integer | no | Todas las muertes. |
| `human_kills` | integer | no | Kills a enemigos humanos (lo que cuenta el ladder). |
| `human_deaths` | integer | no | Muertes por humanos, suicidio o entorno (lo que cuenta el ladder). |
| `suicides` | integer | no | Suicidios. |
| `teamkills` | integer | no | Fuego amigo. |
| `headshots` | integer | no | Kills con tiro a la cabeza. |
| `objectives` | integer | no | Objetivos completados (banderas, maletin, etc.). |
| `best_streak` | integer | no | Mejor racha de kills en la partida. |
| `helmet_saves` | integer | no | El casco le desvio un tiro a la cabeza. |
| `foot_saves` | integer | no | Sobrevivio con "almost lost a foot". |
| `deflected` | integer | no | Tiros suyos desviados por el casco de otro. |
| `hits` | integer | no | Disparos acertados. |
| `misses` | integer | no | Disparos fallados. |
| `score` | integer | no | Puntaje del juego. |
| `points` | integer | no | Puntos del juego. |
| `classes` | jsonb | no | Segundos jugados por clase (JSON: {"clase": segundos}). |
| `main_class` | text | sí | Clase en la que jugo mas tiempo. |
| `result` | character(1) | sí | Resultado: W (gano), L (perdio), D (empate) o NULL (sin ganador). |
| `sprees` | integer | no | Veces que alcanzo KILLING SPREE. |
| `rampages` | integer | no | Veces que alcanzo RAMPAGE. |
| `dominatings` | integer | no | Veces que alcanzo DOMINATING. |
| `unstoppables` | integer | no | Veces que alcanzo UNSTOPPABLE. |
| `godlikes` | integer | no | Veces que alcanzo GODLIKE. |
| `streaks_ended` | integer | no | Rachas ajenas (>= base) que corto matando. |

# Relaciones

- Referencia a [matches](/tables/matches.md).
- Referencia a [players](/tables/players.md).
