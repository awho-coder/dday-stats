---
type: PostgreSQL Function
title: streak_base()
description: Kills seguidas para el primer anuncio de racha (igual al cvar exbattleinfo).
resource: postgresql://dday/public/streak_base
tags: [postgresql, function, ladder]
status: stable
generated: { by: process:okf-export, at: 2026-10-04T18:27:40Z }
sources: 
  - id: schema
    resource: /references/schema.sql
---

# Firma

```
streak_base() RETURNS integer
```

Kills seguidas para el primer anuncio de racha (igual al cvar exbattleinfo).
