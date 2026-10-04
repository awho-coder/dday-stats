---
type: PostgreSQL Table
title: rating_history
description: Elo antes/despues de cada partida (para graficos de evolucion).
resource: postgresql://dday/public/rating_history
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
| `season_id` | integer | no | Temporada; 0 = historico. (PK) |
| `rating_before` | real | no | Elo antes de la partida. |
| `rating_after` | real | no | Elo despues de la partida. |

# Relaciones

- Referencia a [matches](/tables/matches.md).
- Referencia a [players](/tables/players.md).
