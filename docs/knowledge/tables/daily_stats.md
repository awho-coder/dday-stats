---
type: PostgreSQL Table
title: daily_stats
description: 'Actividad por dia (hora de Chile): partidas y jugadores nuevos.'
resource: postgresql://dday/public/daily_stats
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
| `day` | date | no | Dia. (PK) |
| `matches` | integer | no | Partidas jugadas ese dia. |
| `players` | integer | no | Jugadores humanos distintos ese dia. |
