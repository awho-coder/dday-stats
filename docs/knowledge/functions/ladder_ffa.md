---
type: PostgreSQL Function
title: 'ladder_ffa(min_matches integer, match_kind text, season text)'
description: 'Free For All: victorias, podios, puesto promedio, K/D y mejor racha por jugador (sin Elo).'
resource: postgresql://dday/public/ladder_ffa
tags: [postgresql, function, ladder]
status: stable
generated: { by: process:okf-export, at: 2026-10-11T00:24:11Z }
sources: 
  - id: schema
    resource: /references/schema.sql
---

# Firma

```
ladder_ffa(min_matches integer, match_kind text, season text) RETURNS TABLE(pos bigint, name text, matches bigint, wins bigint, win_pct numeric, podiums bigint, avg_place numeric, kills bigint, deaths bigint, kd numeric, best_streak integer)
```

Free For All: victorias, podios, puesto promedio, K/D y mejor racha por jugador (sin Elo).
