---
type: PostgreSQL Function
title: 'ladder_streak(match_kind text, max_rows integer, season text)'
description: Mejores rachas de kills (una fila por partida).
resource: postgresql://dday/public/ladder_streak
tags: [postgresql, function, ladder]
status: stable
generated: { by: process:okf-export, at: 2026-10-04T18:33:53Z }
sources: 
  - id: schema
    resource: /references/schema.sql
---

# Firma

```
ladder_streak(match_kind text, max_rows integer, season text) RETURNS TABLE(pos bigint, name text, best_streak integer, map text, kind text, event text, played_at timestamp with time zone)
```

Mejores rachas de kills (una fila por partida).

# Relaciones

- Usa [kills](../tables/kills.md).
