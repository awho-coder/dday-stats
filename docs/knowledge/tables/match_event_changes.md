---
type: PostgreSQL Table
title: match_event_changes
description: 'Pedidos de cambio de torneo de una partida, creados por el panel admin (tournament_event_request) y aplicados por el ingestor (apply_event_changes).'
resource: postgresql://dday/public/match_event_changes
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
| `id` | integer | no | Clave interna del pedido. (PK) |
| `match_id` | text | no | Partida (se borra en cascada con ella). (FK → [matches](matches.md)) |
| `new_event` | text | no | Torneo al que pasa la partida; vacio = sacarla del torneo (partida normal). |
| `requested_by` | text | sí | Administrador que lo pidio. |
| `requested_at` | timestamp with time zone | no | Cuando se pidio. |
| `applied_at` | timestamp with time zone | sí | Cuando lo aplico el ingestor; NULL = pendiente. |

# Relaciones

- Referencia a [matches](matches.md).
