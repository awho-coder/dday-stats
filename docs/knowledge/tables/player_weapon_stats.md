---
type: PostgreSQL Table
title: player_weapon_stats
description: 'Kills por arma y jugador (arma en mano o, si no aplica, la causa).'
resource: postgresql://dday/public/player_weapon_stats
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
| `player_id` | integer | no | Jugador. (PK; FK → [players](/tables/players.md)) |
| `weapon` | text | no | Arma o causa (rifle, sniper, grenade, ...). (PK) |
| `kills` | integer | no | Kills con esa arma. |
| `headshots` | integer | no | Kills a la cabeza con esa arma. |
| `distance_sum` | bigint | no | Suma de distancias (para el promedio). |
| `distance_n` | integer | no | Cantidad de kills con distancia conocida. |

# Relaciones

- Referencia a [players](/tables/players.md).
