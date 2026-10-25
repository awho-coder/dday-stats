---
type: PostgreSQL Function
title: 'tournament_side_save(p_match_id text, p_clan0 integer, p_clan1 integer, p_excluded boolean, p_admin text)'
description: 'Fija el clan de cada lado de una partida de torneo y/o la marca "no cuenta" (no toca fase ni ronda). SECURITY DEFINER.'
resource: postgresql://dday/public/tournament_side_save
tags: [postgresql, function, ladder]
status: stable
generated: { by: process:okf-export, at: 2026-10-25T21:14:05Z }
sources: 
  - id: schema
    resource: /references/schema.sql
---

# Firma

```
tournament_side_save(p_match_id text, p_clan0 integer, p_clan1 integer, p_excluded boolean, p_admin text) RETURNS boolean
```

Fija el clan de cada lado de una partida de torneo y/o la marca "no cuenta" (no toca fase ni ronda). SECURITY DEFINER.
