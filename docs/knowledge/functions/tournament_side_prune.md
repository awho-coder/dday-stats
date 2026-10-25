---
type: PostgreSQL Function
title: 'tournament_side_prune(p_match_ids text[])'
description: 'Auxiliar: borra las filas vacias de tournament_match_sides. Sin permisos para nadie mas que el dueno.'
resource: postgresql://dday/public/tournament_side_prune
tags: [postgresql, function, ladder]
status: stable
generated: { by: process:okf-export, at: 2026-10-25T21:14:05Z }
sources: 
  - id: schema
    resource: /references/schema.sql
---

# Firma

```
tournament_side_prune(p_match_ids text[]) RETURNS integer
```

Auxiliar: borra las filas vacias de tournament_match_sides. Sin permisos para nadie mas que el dueno.

# Relaciones

- Usa [tournament_match_sides](../tables/tournament_match_sides.md).
