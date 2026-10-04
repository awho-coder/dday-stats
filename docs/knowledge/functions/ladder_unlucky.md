---
type: PostgreSQL Function
title: 'ladder_unlucky(min_matches integer, match_kind text, season text)'
description: Los tiradores mas desafortunados (tiros desviados por cascos).
resource: postgresql://dday/public/ladder_unlucky
tags: [postgresql, function, ladder]
status: stable
generated: { by: process:okf-export, at: 2026-10-04T18:33:53Z }
sources: 
  - id: schema
    resource: /references/schema.sql
---

# Firma

```
ladder_unlucky(min_matches integer, match_kind text, season text) RETURNS TABLE(pos bigint, name text, deflected bigint, matches bigint, deflected_per_match numeric)
```

Los tiradores mas desafortunados (tiros desviados por cascos).
