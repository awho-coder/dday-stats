---
type: PostgreSQL Function
title: 'clan_save(p_id integer, p_tags text[], p_data jsonb, p_admin text)'
description: 'Crea (id NULL) o edita un clan y reemplaza sus tags (el primero es el principal). SECURITY DEFINER: es la unica via de escritura para dday_clans. Un tag repetido da el error 23505.'
resource: postgresql://dday/public/clan_save
tags: [postgresql, function, ladder]
status: stable
generated: { by: process:okf-export, at: 2026-10-16T01:23:12Z }
sources: 
  - id: schema
    resource: /references/schema.sql
---

# Firma

```
clan_save(p_id integer, p_tags text[], p_data jsonb, p_admin text) RETURNS integer
```

Crea (id NULL) o edita un clan y reemplaza sus tags (el primero es el principal). SECURITY DEFINER: es la unica via de escritura para dday_clans. Un tag repetido da el error 23505.
