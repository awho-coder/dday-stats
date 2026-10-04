---
type: PostgreSQL View
title: v_map_weapons
description: Armas mas letales por mapa.
resource: postgresql://dday/public/v_map_weapons
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
| `map` | text | sí |  |
| `weapon` | text | sí |  |
| `kills` | bigint | sí |  |

# Relaciones

- Referencia a [kills](../tables/kills.md).
- Referencia a [map_weapon_stats](../tables/map_weapon_stats.md).
