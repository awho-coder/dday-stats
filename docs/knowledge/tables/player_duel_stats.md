---
type: PostgreSQL Table
title: player_duel_stats
description: Kills entre humanos (nemesis / victima favorita).
resource: postgresql://dday/public/player_duel_stats
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
| `killer_id` | integer | no | Quien mata. (PK; FK → [players](players.md)) |
| `victim_id` | integer | no | A quien mata. (PK; FK → [players](players.md)) |
| `kills` | integer | no | Veces que lo mato. |

# Relaciones

- Referencia a [players](players.md).
- Referencia a [players](players.md).
