---
type: PostgreSQL View
title: v_weapon_stats
description: 'Armas globales: kills, headshots y distancia media.'
resource: postgresql://dday/public/v_weapon_stats
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
| `weapon` | text | sí |  |
| `kills` | bigint | sí |  |
| `headshots` | bigint | sí |  |
| `avg_distance` | numeric | sí |  |
| `max_distance` | integer | sí |  |

# Relaciones

- Referencia a [kills](../tables/kills.md).
- Referencia a [map_weapon_stats](../tables/map_weapon_stats.md).
