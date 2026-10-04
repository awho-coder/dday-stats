---
type: PostgreSQL View
title: v_player_totals_by_kind
description: Totales historicos por jugador y categoria (public/duel/official).
resource: postgresql://dday/public/v_player_totals_by_kind
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
| `kind` | text | sí |  |
| `player_id` | integer | sí |  |
| `name` | text | sí |  |
| `matches` | bigint | sí |  |
| `wins` | bigint | sí |  |
| `losses` | bigint | sí |  |
| `draws` | bigint | sí |  |
| `kills` | bigint | sí |  |
| `deaths` | bigint | sí |  |
| `kd` | numeric | sí |  |
| `bot_kills` | bigint | sí |  |
| `headshots` | bigint | sí |  |
| `hs_pct` | numeric | sí |  |
| `accuracy_pct` | numeric | sí |  |
| `teamkills` | bigint | sí |  |
| `suicides` | bigint | sí |  |
| `objectives` | bigint | sí |  |
| `best_streak` | integer | sí |  |
| `hours_played` | numeric | sí |  |
| `kills_per_min` | numeric | sí |  |
| `win_pct` | numeric | sí |  |
| `most_kills_match` | integer | sí |  |
| `longest_kill` | integer | sí |  |
| `last_match` | timestamp with time zone | sí |  |
| `helmet_saves` | bigint | sí |  |
| `foot_saves` | bigint | sí |  |
| `luck` | bigint | sí |  |
| `deflected` | bigint | sí |  |
| `sprees` | bigint | sí |  |
| `rampages` | bigint | sí |  |
| `dominatings` | bigint | sí |  |
| `unstoppables` | bigint | sí |  |
| `godlikes` | bigint | sí |  |
| `streaks_ended` | bigint | sí |  |

# Relaciones

- Referencia a [kills](/tables/kills.md).
- Referencia a [matches](/tables/matches.md).
- Referencia a [objectives](/tables/objectives.md).
