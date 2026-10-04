---
type: PostgreSQL View
title: v_events
description: Torneos jugados (partidas y mapas).
resource: postgresql://dday/public/v_events
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
| `event` | text | sí |  |
| `matches` | bigint | sí |  |
| `first_match` | timestamp with time zone | sí |  |
| `last_match` | timestamp with time zone | sí |  |
| `maps` | text[] | sí |  |

# Relaciones

- Referencia a [matches](/tables/matches.md).
