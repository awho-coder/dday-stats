---
type: PostgreSQL Function
title: 'player_totals(season text, match_kind text, match_event text)'
description: 'Totales por jugador para una temporada, categoria y torneo (base de los ladders).'
resource: postgresql://dday/public/player_totals
tags: [postgresql, function, ladder]
status: stable
generated: { by: process:okf-export, at: 2026-10-04T18:27:40Z }
sources: 
  - id: schema
    resource: /references/schema.sql
---

# Firma

```
player_totals(season text, match_kind text, match_event text) RETURNS TABLE(player_id integer, name text, matches bigint, wins bigint, losses bigint, draws bigint, kills bigint, deaths bigint, kd numeric, bot_kills bigint, headshots bigint, hs_pct numeric, accuracy_pct numeric, teamkills bigint, suicides bigint, objectives bigint, best_streak integer, hours_played numeric, kills_per_min numeric, win_pct numeric, most_kills_match integer, longest_kill integer, last_match timestamp with time zone, helmet_saves bigint, foot_saves bigint, luck bigint, deflected bigint, sprees bigint, rampages bigint, dominatings bigint, unstoppables bigint, godlikes bigint, streaks_ended bigint)
```

Totales por jugador para una temporada, categoria y torneo (base de los ladders).
