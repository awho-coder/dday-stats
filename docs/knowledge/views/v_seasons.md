---
type: PostgreSQL View
title: v_seasons
description: Temporadas con su fecha de fin y cantidad de partidas.
resource: postgresql://dday/public/v_seasons
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
| `id` | integer | sí |  |
| `name` | text | sí |  |
| `starts_at` | timestamp with time zone | sí |  |
| `ends_at` | timestamp with time zone | sí |  |
| `matches` | bigint | sí |  |

# Relaciones

- Referencia a [matches](/tables/matches.md).
- Referencia a [seasons](/tables/seasons.md).
