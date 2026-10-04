---
type: PostgreSQL View
title: v_player_maps
description: Estadisticas de un jugador por mapa.
resource: postgresql://dday/public/v_player_maps
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
| `wins` | bigint | sí |  |
| `kills` | bigint | sí |  |
| `deaths` | bigint | sí |  |
| `kd` | numeric | sí |  |
| `seconds_played` | bigint | sí |  |
| `best_streak` | integer | sí |  |

# Relaciones

- Referencia a [kills](../tables/kills.md).
- Referencia a [matches](../tables/matches.md).
- Referencia a [player_map_stats](../tables/player_map_stats.md).
