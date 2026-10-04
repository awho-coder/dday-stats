---
type: PostgreSQL View
title: v_player_fav_weapon
description: Arma favorita de cada jugador.
resource: postgresql://dday/public/v_player_fav_weapon
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
| `weapon` | text | sí |  |
| `kills` | integer | sí |  |

# Relaciones

- Referencia a [kills](/tables/kills.md).
- Referencia a [player_weapon_stats](/tables/player_weapon_stats.md).
