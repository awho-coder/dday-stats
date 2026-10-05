---
type: PostgreSQL View
title: v_map_stats
description: 'Estadisticas por mapa y modo: balance aliados/eje, duracion, kills.'
resource: postgresql://dday/public/v_map_stats
tags: [postgresql, view, ladder]
status: stable
generated: { by: process:okf-export, at: 2026-10-05T01:21:03Z }
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
