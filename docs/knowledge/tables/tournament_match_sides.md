---
type: PostgreSQL Table
title: tournament_match_sides
description: 'Datos manuales de una partida de torneo: clan de cada lado, "no cuenta", fase y ronda. Se escribe con tournament_side_save / tournament_assign_save.'
resource: postgresql://dday/public/tournament_match_sides
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
| `match_id` | text | no | Partida (se borra en cascada con ella). (PK; FK → [matches](matches.md)) |
| `clan0_id` | integer | sí | Clan fijado a mano para el equipo 0; NULL = automatico. (FK → [clans](clans.md)) |
| `clan1_id` | integer | sí | Clan fijado a mano para el equipo 1; NULL = automatico. (FK → [clans](clans.md)) |
| `excluded` | boolean | no | true = "no cuenta": sigue en el torneo (estadisticas) pero no suma a cruces ni tabla. |
| `phase_id` | integer | sí | Fase fijada a mano; NULL = automatica por fecha. (FK → [tournament_phases](tournament_phases.md)) |
| `round` | text | no | Ronda (etiqueta libre de hasta 32 caracteres, p. ej. Semifinal); vacio = sin ronda. |
| `updated_at` | timestamp with time zone | no | Ultima edicion. |
| `updated_by` | text | sí | Quien hizo la ultima edicion (administrador). |

# Relaciones

- Referencia a [matches](matches.md).
- Referencia a [clans](clans.md).
- Referencia a [clans](clans.md).
- Referencia a [tournament_phases](tournament_phases.md).
