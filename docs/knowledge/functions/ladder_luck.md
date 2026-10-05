---
type: PostgreSQL Function
title: 'ladder_luck(min_matches integer, match_kind text, season text, match_mode text)'
description: Los mas suertudos (casco que desvia + pie salvado).
resource: postgresql://dday/public/ladder_luck
tags: [postgresql, function, ladder]
status: stable
generated: { by: process:okf-export, at: 2026-10-05T01:21:03Z }
sources: 
  - id: schema
    resource: /references/schema.sql
---

# Firma

```
ladder_luck(min_matches integer, match_kind text, season text, match_mode text) RETURNS TABLE(pos bigint, name text, luck bigint, helmet_saves bigint, foot_saves bigint, matches bigint, luck_per_match numeric)
```

Los mas suertudos (casco que desvia + pie salvado).
