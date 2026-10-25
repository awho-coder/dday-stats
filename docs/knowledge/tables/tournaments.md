---
type: PostgreSQL Table
title: tournaments
description: Fichas de torneo del Ladder Test (se editan desde el panel de administrador; escritura solo por tournament_save / tournament_delete). La clave es el valor de stats_event (matches.event).
resource: postgresql://dday/public/tournaments
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
| `id` | integer | no | Clave interna de la ficha. (PK) |
| `event` | text | no | Clave del torneo = cvar stats_event = matches.event (unica, 1 a 64 caracteres). |
| `name` | text | no | Nombre para mostrar (vacio = se usa la clave). |
| `status` | text | no | Estado puesto por el administrador: upcoming, live o finished. |
| `starts_on` | date | sí | Fecha de inicio (opcional). |
| `ends_on` | date | sí | Fecha de fin (opcional; no anterior al inicio). |
| `format` | text | no | Formato en texto libre corto, p. ej. Liga ida y vuelta. |
| `description` | text | no | Descripcion del torneo. |
| `rules_url` | text | no | URL https de las bases. |
| `logo_url` | text | no | URL https del logo (puede ser un enlace de Drive); vacio = sin logo. |
| `champion_id` | integer | sí | Clan campeon (opcional; NULL si el clan se borra). (FK → [clans](clans.md)) |
| `runner_up_id` | integer | sí | Clan subcampeon (opcional; NULL si el clan se borra). (FK → [clans](clans.md)) |
| `visible` | boolean | no | false = oculto en la app publica. |
| `created_at` | timestamp with time zone | no | Cuando se creo la ficha. |
| `updated_at` | timestamp with time zone | no | Ultima edicion. |
| `updated_by` | text | sí | Quien hizo la ultima edicion (administrador). |

# Relaciones

- Referencia a [clans](clans.md).
- Referencia a [clans](clans.md).
