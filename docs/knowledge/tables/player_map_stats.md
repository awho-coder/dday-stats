---
type: PostgreSQL Table
title: player_map_stats
description: Resumen por jugador y mapa (todas las temporadas).
resource: postgresql://dday/public/player_map_stats
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
| `map` | text | no | Mapa. (PK) |
| `kind` | text | no | Categoria: public o duel. (PK) |
| `matches` | integer | no | Partidas jugadas en ese mapa. |
| `wins` | integer | no | Partidas ganadas. |
| `kills` | integer | no | Kills. |
| `deaths` | integer | no | Muertes. |
| `seconds` | integer | no | Segundos jugados. |
| `best_streak` | integer | no | Mejor racha en ese mapa. |

# Relaciones

- Referencia a [players](players.md).
