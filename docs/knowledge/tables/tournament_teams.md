---
type: PostgreSQL Table
title: tournament_teams
description: 'Clanes inscritos en un torneo, con su grupo. Los grupos y los inscritos sin partidas solo se usan en la primera fase.'
resource: postgresql://dday/public/tournament_teams
tags: [postgresql, table, ladder]
status: stable
generated: { by: process:okf-export, at: 2026-10-25T21:14:05Z }
sources: 
  - id: schema
    resource: /references/schema.sql
---

# Schema

| Columna | Tipo | Nulo | Descripción |
|---|---|---|---|
| `tournament_id` | integer | no | Torneo (se borra en cascada con la ficha). (PK; FK → [tournaments](tournaments.md)) |
| `clan_id` | integer | no | Clan inscrito (se borra en cascada con el clan). (PK; FK → [clans](clans.md)) |
| `group_name` | text | no | Grupo del clan (hasta 24 caracteres); vacio = sin grupo. |

# Relaciones

- Referencia a [tournaments](tournaments.md).
- Referencia a [clans](clans.md).
