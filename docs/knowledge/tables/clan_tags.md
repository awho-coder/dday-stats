---
type: PostgreSQL Table
title: clan_tags
description: 'Tags de cada clan: uno principal y 0 o mas alternativos, todos cuentan para la pertenencia (regla en clan_tag_matches).'
resource: postgresql://dday/public/clan_tags
tags: [postgresql, table, ladder]
status: stable
generated: { by: process:okf-export, at: 2026-10-16T01:23:12Z }
sources: 
  - id: schema
    resource: /references/schema.sql
---

# Schema

| Columna | Tipo | Nulo | Descripción |
|---|---|---|---|
| `clan_id` | integer | no | Clan al que pertenece el tag (se borra en cascada con el clan). (FK → [clans](clans.md)) |
| `tag` | text | no | Tag, de 1 a 24 caracteres sin espacios; unico sin distinguir mayusculas. |
| `is_primary` | boolean | no | true en el tag principal (uno por clan); es el que se muestra. |

# Relaciones

- Referencia a [clans](clans.md).
