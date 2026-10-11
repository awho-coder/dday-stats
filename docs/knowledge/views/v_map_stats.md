---
type: PostgreSQL View
title: v_map_stats
description: 'Estadisticas por mapa: balance aliados/eje, duracion, kills (todos los modos).'
resource: postgresql://dday/public/v_map_stats
tags: [postgresql, view, ladder]
status: stable
generated: { by: process:okf-export, at: 2026-10-11T00:24:11Z }
sources: 
  - id: schema
    resource: /references/schema.sql
---

# Schema

| Columna | Tipo | Nulo | Descripción |
|---|---|---|---|
| `map` | text | sí |  |
| `kind` | text | sí |  |
| `matches` | bigint | sí |  |
| `allied_wins` | bigint | sí |  |
| `axis_wins` | bigint | sí |  |
| `draws` | bigint | sí |  |
| `allied_win_pct` | numeric | sí |  |
| `avg_minutes` | numeric | sí |  |
| `avg_humans` | numeric | sí |  |
| `best_streak` | integer | sí |  |
| `kills` | bigint | sí |  |
| `last_played` | timestamp with time zone | sí |  |

# Relaciones

- Referencia a [kills](../tables/kills.md).
- Referencia a [map_stats](../tables/map_stats.md).
- Referencia a [matches](../tables/matches.md).
