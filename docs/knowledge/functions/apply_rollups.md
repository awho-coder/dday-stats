---
type: PostgreSQL Function
title: 'apply_rollups(ids text[])'
description: Suma las partidas indicadas a las tablas de resumen (una sola vez por partida).
resource: postgresql://dday/public/apply_rollups
tags: [postgresql, function, ladder]
status: stable
generated: { by: process:okf-export, at: 2026-10-04T18:33:53Z }
sources: 
  - id: schema
    resource: /references/schema.sql
---

# Firma

```
apply_rollups(ids text[]) RETURNS void
```

Suma las partidas indicadas a las tablas de resumen (una sola vez por partida).
