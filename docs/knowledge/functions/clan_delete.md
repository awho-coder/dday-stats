---
type: PostgreSQL Function
title: clan_delete(p_id integer)
description: Borra un clan y sus tags; true si existia. SECURITY DEFINER.
resource: postgresql://dday/public/clan_delete
tags: [postgresql, function, ladder]
status: stable
generated: { by: process:okf-export, at: 2026-10-16T01:23:12Z }
sources: 
  - id: schema
    resource: /references/schema.sql
---

# Firma

```
clan_delete(p_id integer) RETURNS boolean
```

Borra un clan y sus tags; true si existia. SECURITY DEFINER.
