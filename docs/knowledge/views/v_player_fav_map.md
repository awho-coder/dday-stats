---
type: PostgreSQL View
title: v_player_fav_map
description: Mapa favorito de cada jugador (el mas jugado).
resource: postgresql://dday/public/v_player_fav_map
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
| `map` | text | sí |  |
| `matches` | bigint | sí |  |
| `seconds_played` | bigint | sí |  |

# Relaciones

- Referencia a [matches](../tables/matches.md).
- Referencia a [v_player_maps](v_player_maps.md).
