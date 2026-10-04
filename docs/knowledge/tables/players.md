---
type: PostgreSQL Table
title: players
description: Jugador o bot. La identidad es el nombre (sin colores).
resource: postgresql://dday/public/players
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
| `id` | integer | no | Clave interna del jugador. (PK) |
| `name` | text | no | Nombre del jugador (sin colores ni sufijos). |
| `is_bot` | boolean | no | true si es un bot. |
| `first_seen` | timestamp with time zone | no | Primera vez visto. |
| `last_seen` | timestamp with time zone | no | Ultima vez visto. |
