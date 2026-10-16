---
type: PostgreSQL Function
title: 'ladder_clans(match_kind text, season text)'
description: 'Ranking de clanes: suma de sus miembros activos en la temporada y categoria (K/D sin muestra con menos de 20 muertes, Elo promedio, MVPs).'
resource: postgresql://dday/public/ladder_clans
tags: [postgresql, function, ladder]
status: stable
generated: { by: process:okf-export, at: 2026-10-16T01:23:12Z }
sources: 
  - id: schema
    resource: /references/schema.sql
---

# Firma

```
ladder_clans(match_kind text, season text) RETURNS TABLE(clan_id integer, clan text, unregistered boolean, members_active bigint, matches bigint, kills bigint, deaths bigint, kd numeric, bot_kills bigint, hours numeric, wins bigint, losses bigint, win_pct numeric, best_streak integer, godlikes bigint, headshots bigint, hs_pct numeric, elo_avg numeric, mvps bigint, top_player text)
```

Ranking de clanes: suma de sus miembros activos en la temporada y categoria (K/D sin muestra con menos de 20 muertes, Elo promedio, MVPs).
