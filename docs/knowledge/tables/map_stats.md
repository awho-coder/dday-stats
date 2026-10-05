---
type: PostgreSQL Table
title: map_stats
description: 'Resumen por mapa, categoria y modo de juego (balance, duracion, letalidad).'
resource: postgresql://dday/public/map_stats
tags: [postgresql, table, ladder]
status: stable
generated: { by: process:okf-export, at: 2026-10-05T01:21:03Z }
sources: 
  - id: schema
    resource: /references/schema.sql
---

# Schema

| Columna | Tipo | Nulo | Descripción |
|---|---|---|---|
| `map` | text | no | Mapa. (PK) |
| `kind` | text | no | Categoria: public o duel. (PK) |
| `mode` | text | no | Modo de juego: dm, ctb, campaign, control... (PK) |
| `matches` | integer | no | Partidas jugadas. |
| `allied_wins` | integer | no | Victorias de aliados. |
| `axis_wins` | integer | no | Victorias del eje. |
| `draws` | integer | no | Empates. |
| `seconds` | bigint | no | Segundos totales jugados. |
| `humans` | integer | no | Suma de humanos por partida. |
| `kills` | integer | no | Kills totales. |
| `best_streak` | integer | no | Mejor racha en ese mapa. |
| `last_played` | timestamp with time zone | sí | Ultima vez jugado. |
