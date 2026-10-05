---
type: PostgreSQL Function
title: 'ladder_elo(min_games integer, match_kind text, season text, match_mode text)'
description: 'Ladder por rating Elo, por categoria y modo (all = general).'
resource: postgresql://dday/public/ladder_elo
tags: [postgresql, function, ladder]
status: stable
generated: { by: process:okf-export, at: 2026-10-05T01:21:03Z }
sources: 
  - id: schema
    resource: /references/schema.sql
---

# Firma

```
ladder_elo(min_games integer, match_kind text, season text, match_mode text) RETURNS TABLE(pos bigint, name text, rating integer, peak integer, games integer, wins integer, losses integer, draws integer, win_pct numeric)
```

Ladder por rating Elo, por categoria y modo (all = general).
