---
type: PostgreSQL View
title: v_player_clan
description: Clan de cada nick humano segun clan_tag_matches (solo clanes activos; gana el tag mas largo). clan_id NULL = sin clan.
resource: postgresql://dday/public/v_player_clan
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
| `player_id` | integer | sí |  |
| `name` | text | sí |  |
| `clan_id` | integer | sí |  |
| `clan` | text | sí |  |
| `unregistered` | boolean | sí |  |

# Relaciones

- Referencia a [clan_tags](../tables/clan_tags.md).
- Referencia a [clans](../tables/clans.md).
- Referencia a [players](../tables/players.md).
