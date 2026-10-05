---
type: PostgreSQL View
title: v_map_stats_by_mode
description: 'Estadisticas por mapa, tipo y modo de juego.'
resource: postgresql://dday/public/v_map_stats_by_mode
tags: [postgresql, view, ladder]
status: stable
generated: { by: process:okf-export, at: 2026-10-05T02:17:08Z }
sources: 
  - id: schema
    resource: /references/schema.sql
---

# Schema

| Columna | Tipo | Nulo | Descripción |
|---|---|---|---|
| `map` | text | sí |  |
| `kind` | text | sí |  |
| `mode` | text | sí |  |
| `matches` | integer | sí |  |
| `allied_wins` | integer | sí |  |
| `axis_wins` | integer | sí |  |
| `draws` | integer | sí |  |
| `allied_win_pct` | numeric | sí |  |
| `avg_minutes` | numeric | sí |  |
| `avg_humans` | numeric | sí |  |
| `best_streak` | integer | sí |  |
| `kills` | integer | sí |  |
| `last_played` | timestamp with time zone | sí |  |

# Relaciones

- Referencia a [kills](../tables/kills.md).
- Referencia a [map_stats](../tables/map_stats.md).
- Referencia a [matches](../tables/matches.md).
