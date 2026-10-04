---
type: PostgreSQL View
title: v_player_duels
description: Kills entre pares de jugadores.
resource: postgresql://dday/public/v_player_duels
tags: [postgresql, view, ladder]
status: stable
generated: { by: process:okf-export, at: 2026-10-04T18:33:53Z }
sources: 
  - id: schema
    resource: /references/schema.sql
---

# Schema

| Columna | Tipo | Nulo | Descripción |
|---|---|---|---|
| `killer_id` | integer | sí |  |
| `victim_id` | integer | sí |  |
| `kills` | integer | sí |  |

# Relaciones

- Referencia a [kills](../tables/kills.md).
- Referencia a [player_duel_stats](../tables/player_duel_stats.md).
