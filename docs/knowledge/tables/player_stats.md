---
type: PostgreSQL Table
title: player_stats
description: 'Resumen por jugador, temporada, categoria y torneo (lo mantiene apply_rollups).'
resource: postgresql://dday/public/player_stats
tags: [postgresql, table, ladder]
status: stable
generated: { by: process:okf-export, at: 2026-10-04T18:33:53Z }
sources: 
  - id: schema
    resource: /references/schema.sql
---

# Schema

| Columna | Tipo | Nulo | Descripción |
|---|---|---|---|
| `player_id` | integer | no | Jugador. (PK; FK → [players](players.md)) |
| `season_id` | integer | no | Temporada. (PK; FK → [seasons](seasons.md)) |
| `kind` | text | no | Categoria: public, duel u official. (PK) |
| `event` | text | no | Torneo; cadena vacia = casual. (PK) |
| `matches` | integer | no | Partidas jugadas. |
| `wins` | integer | no | Partidas ganadas. |
| `losses` | integer | no | Partidas perdidas. |
| `draws` | integer | no | Partidas empatadas. |
| `kills` | integer | no | Kills a enemigos (humanos o bots). |
| `deaths` | integer | no | Todas las muertes. |
| `human_kills` | integer | no | Kills a humanos (lo que cuenta el ladder). |
| `human_deaths` | integer | no | Muertes por humanos/suicidio/entorno. |
| `headshots` | integer | no | Kills con tiro a la cabeza. |
| `hits` | integer | no | Disparos acertados. |
| `misses` | integer | no | Disparos fallados. |
| `teamkills` | integer | no | Fuego amigo. |
| `suicides` | integer | no | Suicidios. |
| `objectives` | integer | no | Objetivos completados. |
| `seconds` | integer | no | Segundos jugados. |
| `best_streak` | integer | no | Mejor racha de kills. |
| `max_kills` | integer | no | Mas kills en una sola partida. |
| `helmet_saves` | integer | no | Cascos que desviaron un tiro a la cabeza. |
| `foot_saves` | integer | no | Veces que sobrevivio con el pie herido. |
| `deflected` | integer | no | Tiros desviados por cascos ajenos. |
| `longest_kill` | integer | sí | Kill mas larga (distancia en unidades de Quake 2). |
| `last_match` | timestamp with time zone | sí | Fecha de la ultima partida. |
| `sprees` | integer | no | Veces que alcanzo KILLING SPREE. |
| `rampages` | integer | no | Veces que alcanzo RAMPAGE. |
| `dominatings` | integer | no | Veces que alcanzo DOMINATING. |
| `unstoppables` | integer | no | Veces que alcanzo UNSTOPPABLE. |
| `godlikes` | integer | no | Veces que alcanzo GODLIKE. |
| `streaks_ended` | integer | no | Rachas ajenas que corto. |

# Relaciones

- Referencia a [players](players.md).
- Referencia a [seasons](seasons.md).
