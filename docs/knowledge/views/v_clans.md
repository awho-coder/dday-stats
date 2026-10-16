---
type: PostgreSQL View
title: v_clans
description: Clanes con su tag principal (tag) y la lista de tags alternativos (aliases).
resource: postgresql://dday/public/v_clans
tags: [postgresql, view, ladder]
status: stable
generated: { by: process:okf-export, at: 2026-10-16T01:23:12Z }
sources: 
  - id: schema
    resource: /references/schema.sql
---

# Schema

| Columna | Tipo | Nulo | Descripción |
|---|---|---|---|
| `id` | integer | sí |  |
| `name` | text | sí |  |
| `leader` | text | sí |  |
| `declared_members` | integer | sí |  |
| `favorite_map` | text | sí |  |
| `symbol_player` | text | sí |  |
| `phrase` | text | sí |  |
| `rival` | text | sí |  |
| `logo_url` | text | sí |  |
| `unregistered` | boolean | sí |  |
| `active` | boolean | sí |  |
| `sort_order` | integer | sí |  |
| `created_at` | timestamp with time zone | sí |  |
| `updated_at` | timestamp with time zone | sí |  |
| `updated_by` | text | sí |  |
| `tag` | text | sí |  |
| `aliases` | text[] | sí |  |

# Relaciones

- Referencia a [clan_tags](../tables/clan_tags.md).
- Referencia a [clans](../tables/clans.md).
