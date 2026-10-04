---
type: PostgreSQL Table
title: map_weapon_stats
description: 'Kills por mapa, categoria y arma.'
resource: postgresql://dday/public/map_weapon_stats
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
| `map` | text | no | Mapa. (PK) |
| `kind` | text | no | Categoria: public o duel. (PK) |
| `weapon` | text | no | Arma o causa. (PK) |
| `kills` | integer | no | Kills. |
| `headshots` | integer | no | Kills a la cabeza. |
| `distance_sum` | bigint | no | Suma de distancias. |
| `distance_n` | integer | no | Cantidad de kills con distancia conocida. |
| `max_distance` | integer | sí | Kill mas larga con esa arma. |
