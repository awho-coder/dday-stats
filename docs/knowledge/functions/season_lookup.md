---
type: PostgreSQL Function
title: season_lookup(season text)
description: 'Resuelve una temporada: current (la vigente), all (NULL) o el nombre.'
resource: postgresql://dday/public/season_lookup
tags: [postgresql, function, ladder]
status: stable
generated: { by: process:okf-export, at: 2026-10-04T18:33:53Z }
sources: 
  - id: schema
    resource: /references/schema.sql
---

# Firma

```
season_lookup(season text) RETURNS integer
```

Resuelve una temporada: current (la vigente), all (NULL) o el nombre.
