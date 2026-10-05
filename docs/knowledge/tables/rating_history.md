---
type: PostgreSQL Table
title: rating_history
description: Elo antes/despues de cada partida (para graficos de evolucion).
resource: postgresql://dday/public/rating_history
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
| `match_id` | text | no | Partida. (PK; FK → [matches](matches.md)) |
| `player_id` | integer | no | Jugador. (PK; FK → [players](players.md)) |
| `mode` | text | no | Modo del Elo: all = general o el modo de la partida. (PK) |
| `season_id` | integer | no | Temporada; 0 = historico. (PK) |
| `rating_before` | real | no | Elo antes de la partida. |
| `rating_after` | real | no | Elo despues de la partida. |

# Relaciones

- Referencia a [matches](matches.md).
- Referencia a [players](players.md).
