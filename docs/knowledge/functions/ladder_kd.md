---
type: PostgreSQL Function
title: 'ladder_kd(min_matches integer, match_kind text, season text, match_event text)'
description: Ladder por K/D (humano vs humano).
resource: postgresql://dday/public/ladder_kd
tags: [postgresql, function, ladder]
status: stable
generated: { by: process:okf-export, at: 2026-10-04T18:27:40Z }
sources: 
  - id: schema
    resource: /references/schema.sql
---

# Firma

```
ladder_kd(min_matches integer, match_kind text, season text, match_event text) RETURNS TABLE(pos bigint, name text, matches bigint, kills bigint, deaths bigint, kd numeric, kills_per_min numeric, win_pct numeric, hs_pct numeric)
```

Ladder por K/D (humano vs humano).
