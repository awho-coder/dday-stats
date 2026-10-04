---
type: PostgreSQL Table
title: player_class_stats
description: Segundos jugados por clase y jugador.
resource: postgresql://dday/public/player_class_stats
tags: [postgresql, table, ladder]
status: stable
generated: { by: process:okf-export, at: 2026-10-04T18:33:53Z }
sources: 
  - id: schema
    resource: /references/schema.sql
---

# Schema

| Columna | Tipo | Nulo | Descripción |
|---|---|---|---|
| `player_id` | integer | no | Jugador. (PK; FK → [players](players.md)) |
| `class` | text | no | Clase (sniper, medic, engineer, ...). (PK) |
| `seconds` | integer | no | Segundos jugados con esa clase. |

# Relaciones

- Referencia a [players](players.md).
