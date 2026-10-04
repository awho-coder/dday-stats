---
type: PostgreSQL Function
title: 'ladder_streak_tiers(min_matches integer, match_kind text, season text)'
description: 'Rachas por nivel: quien mas llego a GODLIKE, UNSTOPPABLE, etc.'
resource: postgresql://dday/public/ladder_streak_tiers
tags: [postgresql, function, ladder]
status: stable
generated: { by: process:okf-export, at: 2026-10-04T18:27:40Z }
sources: 
  - id: schema
    resource: /references/schema.sql
---

# Firma

```
ladder_streak_tiers(min_matches integer, match_kind text, season text) RETURNS TABLE(pos bigint, name text, matches bigint, godlikes bigint, unstoppables bigint, dominatings bigint, rampages bigint, sprees bigint, streaks_ended bigint, best_streak integer)
```

Rachas por nivel: quien mas llego a GODLIKE, UNSTOPPABLE, etc.
