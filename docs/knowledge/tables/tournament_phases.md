---
type: PostgreSQL Table
title: tournament_phases
description: 'Fases de un torneo (1 a 10, en orden), cada una con su modalidad. Una partida cae en la fase con el starts_at mas reciente que no sea posterior a ella; la primera fase siempre empieza desde el inicio.'
resource: postgresql://dday/public/tournament_phases
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
| `id` | integer | no | Clave interna de la fase. (PK) |
| `tournament_id` | integer | no | Torneo al que pertenece (se borra en cascada con la ficha). (FK → [tournaments](tournaments.md)) |
| `name` | text | no | Nombre de la fase (1 a 40 caracteres). |
| `scoring` | text | no | Modalidad: league (tabla con puntos y cruces), knockout (eliminacion: cruces por ronda) o results (solo resultados). |
| `points_per` | text | no | Puntos de la liga por mapa (match) o por cruce (series). |
| `points_win` | integer | no | Puntos por victoria (0 a 100). |
| `points_draw` | integer | no | Puntos por empate (0 a 100). |
| `points_loss` | integer | no | Puntos por derrota (0 a 100). |
| `starts_at` | timestamp with time zone | sí | Desde cuando rige la fase; NULL en la primera (desde el inicio). |
| `sort_order` | integer | no | Posicion de la fase dentro del torneo (1 = primera). |

# Relaciones

- Referencia a [tournaments](tournaments.md).
