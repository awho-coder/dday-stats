---
type: PostgreSQL Table
title: clans
description: Clanes del Ladder Test (se editan desde el panel de administrador; escritura solo por clan_save / clan_delete).
resource: postgresql://dday/public/clans
tags: [postgresql, table, ladder]
status: stable
generated: { by: process:okf-export, at: 2026-10-16T01:23:12Z }
sources: 
  - id: schema
    resource: /references/schema.sql
---

# Schema

| Columna | Tipo | Nulo | Descripción |
|---|---|---|---|
| `id` | integer | no | Clave interna del clan. (PK) |
| `name` | text | no | Nombre del clan (puede estar vacio). |
| `leader` | text | no | Lider del clan. |
| `declared_members` | integer | sí | Cantidad de miembros declarada (0 a 999); NULL si no se sabe. |
| `favorite_map` | text | no | Mapa favorito declarado. |
| `symbol_player` | text | no | Jugador simbolo del clan. |
| `phrase` | text | no | Frase tipica del clan. |
| `rival` | text | no | Rival historico declarado. |
| `logo_url` | text | no | URL https del logo (puede ser un enlace de Drive); vacio = sin logo. |
| `unregistered` | boolean | no | true para el clan No Inscritos: va al final de las listas, fuera de los podios y nunca cuenta para duelos de clanes. |
| `active` | boolean | no | Solo los clanes activos reclaman nicks (v_player_clan). |
| `sort_order` | integer | no | Orden manual; desempata cuando dos clanes calzan con un tag del mismo largo. |
| `created_at` | timestamp with time zone | no | Cuando se creo el clan. |
| `updated_at` | timestamp with time zone | no | Ultima edicion. |
| `updated_by` | text | sí | Quien hizo la ultima edicion (administrador, o "carga inicial"). |
