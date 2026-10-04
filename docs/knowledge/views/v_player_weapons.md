---
type: PostgreSQL View
title: v_player_weapons
description: Kills de un jugador por arma.
resource: postgresql://dday/public/v_player_weapons
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
| `player_id` | integer | sí |  |
| `weapon` | text | sí |  |
| `kills` | integer | sí |  |
| `headshots` | integer | sí |  |
| `avg_distance` | numeric | sí |  |

# Relaciones

- Referencia a [kills](../tables/kills.md).
- Referencia a [player_weapon_stats](../tables/player_weapon_stats.md).
