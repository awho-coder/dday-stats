---
type: PostgreSQL Function
title: tournament_delete(p_id integer)
description: Borra una ficha (fases e inscritos en cascada) y los datos manuales de sus partidas; no borra partidas. true si existia. SECURITY DEFINER.
resource: postgresql://dday/public/tournament_delete
tags: [postgresql, function, ladder]
status: stable
generated: { by: process:okf-export, at: 2026-10-25T21:14:05Z }
sources: 
  - id: schema
    resource: /references/schema.sql
---

# Firma

```
tournament_delete(p_id integer) RETURNS boolean
```

Borra una ficha (fases e inscritos en cascada) y los datos manuales de sus partidas; no borra partidas. true si existia. SECURITY DEFINER.
