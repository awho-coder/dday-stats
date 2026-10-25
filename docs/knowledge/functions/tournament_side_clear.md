---
type: PostgreSQL Function
title: tournament_side_clear(p_match_id text)
description: 'Vuelve a automatico los clanes y el "no cuenta" de una partida, sin perder fase ni ronda. SECURITY DEFINER.'
resource: postgresql://dday/public/tournament_side_clear
tags: [postgresql, function, ladder]
status: stable
generated: { by: process:okf-export, at: 2026-10-25T21:14:05Z }
sources: 
  - id: schema
    resource: /references/schema.sql
---

# Firma

```
tournament_side_clear(p_match_id text) RETURNS boolean
```

Vuelve a automatico los clanes y el "no cuenta" de una partida, sin perder fase ni ronda. SECURITY DEFINER.
