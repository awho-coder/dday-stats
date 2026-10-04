---
type: PostgreSQL View
title: v_player_nemesis
description: Quien mas mata a cada jugador (su nemesis).
resource: postgresql://dday/public/v_player_nemesis
tags: [postgresql, view, ladder]
status: stable
generated: { by: process:okf-export, at: 2026-10-04T18:27:40Z }
sources: 
  - id: schema
    resource: /references/schema.sql
---

# Schema

| Columna | Tipo | Nulo | Descripción |
|---|---|---|---|
| `player_id` | integer | sí |  |
| `nemesis` | text | sí |  |
| `kills` | integer | sí |  |

# Relaciones

- Referencia a [kills](/tables/kills.md).
- Referencia a [player_duel_stats](/tables/player_duel_stats.md).
- Referencia a [players](/tables/players.md).
