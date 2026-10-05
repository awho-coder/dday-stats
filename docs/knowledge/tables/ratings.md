---
type: PostgreSQL Table
title: ratings
description: 'Elo por equipos, por jugador, categoria (public/duel/official) y temporada (todos los modos juntos).'
resource: postgresql://dday/public/ratings
tags: [postgresql, table, ladder]
status: stable
generated: { by: process:okf-export, at: 2026-10-05T02:17:08Z }
sources: 
  - id: schema
    resource: /references/schema.sql
---

# Schema

| Columna | Tipo | Nulo | Descripción |
|---|---|---|---|
| `player_id` | integer | no | Jugador. (PK; FK → [players](players.md)) |
| `kind` | text | no | Categoria: public, duel u official. (PK) |
| `season_id` | integer | no | Temporada; 0 = historico (todas). (PK) |
| `rating` | real | no | Elo actual (arranca en 1500). |
| `peak` | real | no | Elo maximo alcanzado. |
| `games` | integer | no | Partidas rankeadas jugadas. |
| `wins` | integer | no | Partidas ganadas. |
| `losses` | integer | no | Partidas perdidas. |
| `draws` | integer | no | Partidas empatadas. |
| `updated_at` | timestamp with time zone | no | Ultima actualizacion. |

# Relaciones

- Referencia a [players](players.md).
