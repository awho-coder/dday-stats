---
type: PostgreSQL Table
title: daily_players
description: Que jugador jugo cada dia (para no contarlo dos veces en daily_stats.players).
resource: postgresql://dday/public/daily_players
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
| `day` | date | no | Dia. (PK) |
| `player_id` | integer | no | Jugador. (PK; FK → [players](players.md)) |

# Relaciones

- Referencia a [players](players.md).
