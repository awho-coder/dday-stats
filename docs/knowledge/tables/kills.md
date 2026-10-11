---
type: PostgreSQL Table
title: kills
description: 'Una kill (o suicidio/muerte por entorno). Tabla de detalle, la mas grande.'
resource: postgresql://dday/public/kills
tags: [postgresql, table, ladder]
status: stable
generated: { by: process:okf-export, at: 2026-10-11T00:24:11Z }
sources: 
  - id: schema
    resource: /references/schema.sql
---

# Schema

| Columna | Tipo | Nulo | Descripción |
|---|---|---|---|
| `id` | bigint | no | Clave interna. (PK) |
| `match_id` | text | no | Partida. (FK → [matches](matches.md)) |
| `t` | real | no | Segundos desde el inicio del mapa. |
| `killer_id` | integer | sí | Quien mato; NULL en suicidio o muerte por entorno. (FK → [players](players.md)) |
| `victim_id` | integer | no | Quien murio. (FK → [players](players.md)) |
| `killer_team` | smallint | sí | Equipo del killer (0 aliados, 1 eje). |
| `victim_team` | smallint | sí | Equipo de la victima. |
| `killer_class` | text | sí | Clase del killer. |
| `victim_class` | text | sí | Clase de la victima. |
| `mod` | text | no | Causa de la muerte: rifle, sniper, grenade, wound, ... |
| `weapon` | text | sí | Arma en mano (solo armas de fuego o cuchillo). |
| `headshot` | boolean | no | true si fue tiro a la cabeza. |
| `friendly_fire` | boolean | no | true si fue fuego amigo. |
| `suicide` | boolean | no | true si fue suicidio. |
| `distance` | integer | sí | Distancia killer-victima en unidades de Quake 2 (~3 cm). |
| `killer_x` | integer | sí | Posicion X del killer (unidades de Quake 2). |
| `killer_y` | integer | sí | Posicion Y del killer. |
| `killer_z` | integer | sí | Posicion Z del killer (altura). |
| `victim_x` | integer | sí | Posicion X de la victima. |
| `victim_y` | integer | sí | Posicion Y de la victima. |
| `victim_z` | integer | sí | Posicion Z de la victima. |
| `killer_in_zone` | boolean | sí | Modo control: true si el que mato estaba dentro de la zona; NULL fuera del modo control (o en partidas anteriores a este dato). |
| `victim_in_zone` | boolean | sí | Modo control: true si el que murio estaba dentro de la zona; NULL fuera del modo control. |

# Relaciones

- Referencia a [matches](matches.md).
- Referencia a [players](players.md).
- Referencia a [players](players.md).
