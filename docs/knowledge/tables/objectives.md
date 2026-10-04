---
type: PostgreSQL Table
title: objectives
description: 'Eventos de objetivo de una partida (banderas, maletin, zonas, etc.).'
resource: postgresql://dday/public/objectives
tags: [postgresql, table, ladder]
status: stable
generated: { by: process:okf-export, at: 2026-10-04T18:27:40Z }
sources: 
  - id: schema
    resource: /references/schema.sql
---

# Schema

| Columna | Tipo | Nulo | Descripción |
|---|---|---|---|
| `id` | bigint | no | Clave interna. (PK) |
| `match_id` | text | no | Partida. (FK → [matches](/tables/matches.md)) |
| `t` | real | no | Segundos desde el inicio del mapa. |
| `type` | text | no | Tipo: touch, area, timed, timed_held, explosive, bc_pickup/bc_drop/bc_capture. |
| `name` | text | sí | Nombre del objetivo. |
| `team` | smallint | sí | Equipo que lo completo. |
| `player_id` | integer | sí | Jugador que lo hizo (NULL si no aplica). (FK → [players](/tables/players.md)) |

# Relaciones

- Referencia a [matches](/tables/matches.md).
- Referencia a [players](/tables/players.md).
