---
type: PostgreSQL View
title: v_player_profile
description: Perfil completo de un jugador (totales + Elo general + favoritos + nemesis).
resource: postgresql://dday/public/v_player_profile
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
| `zone_captures` | bigint | sí |  |
| `zone_minutes` | numeric | sí |  |
| `rating` | integer | sí |  |
| `ranked_games` | integer | sí |  |
| `duel_rating` | integer | sí |  |
| `duel_games` | integer | sí |  |
| `official_rating` | integer | sí |  |
| `official_games` | integer | sí |  |
| `fav_map` | text | sí |  |
| `fav_weapon` | text | sí |  |
| `fav_class` | text | sí |  |
| `nemesis` | text | sí |  |
| `favorite_victim` | text | sí |  |

# Relaciones

- Referencia a [kills](../tables/kills.md).
- Referencia a [matches](../tables/matches.md).
- Referencia a [objectives](../tables/objectives.md).
- Referencia a [ratings](../tables/ratings.md).
- Referencia a [v_player_fav_class](v_player_fav_class.md).
- Referencia a [v_player_fav_map](v_player_fav_map.md).
- Referencia a [v_player_fav_weapon](v_player_fav_weapon.md).
- Referencia a [v_player_favorite_victim](v_player_favorite_victim.md).
- Referencia a [v_player_nemesis](v_player_nemesis.md).
- Referencia a [v_player_totals](v_player_totals.md).
