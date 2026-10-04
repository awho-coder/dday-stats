---
type: PostgreSQL View
title: v_daily_activity
description: Actividad diaria (partidas y jugadores por dia).
resource: postgresql://dday/public/v_daily_activity
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
| `day` | date | sí |  |
| `matches` | integer | sí |  |
| `players` | integer | sí |  |

# Relaciones

- Referencia a [daily_stats](/tables/daily_stats.md).
- Referencia a [matches](/tables/matches.md).
- Referencia a [players](/tables/players.md).
