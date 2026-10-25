---
type: PostgreSQL Function
title: 'tournament_assign_save(p_match_ids text[], p_phase_id integer, p_round text, p_admin text)'
description: Pone fase y ronda a varias partidas del mismo torneo (las de un cruce). SECURITY DEFINER.
resource: postgresql://dday/public/tournament_assign_save
tags: [postgresql, function, ladder]
status: stable
generated: { by: process:okf-export, at: 2026-10-25T21:14:05Z }
sources: 
  - id: schema
    resource: /references/schema.sql
---

# Firma

```
tournament_assign_save(p_match_ids text[], p_phase_id integer, p_round text, p_admin text) RETURNS integer
```

Pone fase y ronda a varias partidas del mismo torneo (las de un cruce). SECURITY DEFINER.
