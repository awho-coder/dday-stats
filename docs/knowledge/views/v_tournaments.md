---
type: PostgreSQL View
title: v_tournaments
description: 'Torneos: las fichas mas las claves usadas en partidas sin ficha (registered = false, estado deducido: live si la ultima partida fue hace 7 dias o menos).'
resource: postgresql://dday/public/v_tournaments
tags: [postgresql, view, ladder]
status: stable
generated: { by: process:okf-export, at: 2026-10-25T21:14:05Z }
sources: 
  - id: schema
    resource: /references/schema.sql
---

# Schema

| Columna | Tipo | Nulo | Descripción |
|---|---|---|---|
| `id` | integer | sí |  |
| `event` | text | sí |  |
| `name` | text | sí |  |
| `status` | text | sí |  |
| `starts_on` | date | sí |  |
| `ends_on` | date | sí |  |
| `format` | text | sí |  |
| `description` | text | sí |  |
| `rules_url` | text | sí |  |
| `logo_url` | text | sí |  |
| `champion_id` | integer | sí |  |
| `runner_up_id` | integer | sí |  |
| `visible` | boolean | sí |  |
| `registered` | boolean | sí |  |
| `phases` | bigint | sí |  |
| `matches` | bigint | sí |  |
| `first_match` | timestamp with time zone | sí |  |
| `last_match` | timestamp with time zone | sí |  |
| `maps` | text[] | sí |  |
| `updated_at` | timestamp with time zone | sí |  |
| `updated_by` | text | sí |  |

# Relaciones

- Referencia a [matches](../tables/matches.md).
- Referencia a [tournament_phases](../tables/tournament_phases.md).
- Referencia a [tournaments](../tables/tournaments.md).
- Referencia a [v_events](v_events.md).
