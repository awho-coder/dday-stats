---
type: PostgreSQL Function
title: 'match_category(kind text, event text)'
description: 'Categoria de ladder de una partida: official si tiene torneo, si no el modo (public/duel).'
resource: postgresql://dday/public/match_category
tags: [postgresql, function, ladder]
status: stable
generated: { by: process:okf-export, at: 2026-10-04T18:33:53Z }
sources: 
  - id: schema
    resource: /references/schema.sql
---

# Firma

```
match_category(kind text, event text) RETURNS text
```

Categoria de ladder de una partida: official si tiene torneo, si no el modo (public/duel).
