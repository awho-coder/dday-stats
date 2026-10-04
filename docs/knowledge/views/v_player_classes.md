---
type: PostgreSQL View
title: v_player_classes
description: Segundos por clase y jugador.
resource: postgresql://dday/public/v_player_classes
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
| `class` | text | sí |  |
| `seconds` | integer | sí |  |

# Relaciones

- Referencia a [player_class_stats](../tables/player_class_stats.md).
