---
type: PostgreSQL Table
title: seasons
description: 'Temporadas del ladder (se crean con "ingest.py --new-season NOMBRE").'
resource: postgresql://dday/public/seasons
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
| `id` | integer | no | Clave interna de la temporada. (PK) |
| `name` | text | no | Nombre de la temporada (unico). |
| `starts_at` | timestamp with time zone | no | Cuando empieza; dura hasta que empieza la siguiente. |
