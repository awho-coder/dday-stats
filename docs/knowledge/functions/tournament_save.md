---
type: PostgreSQL Function
title: 'tournament_save(p_id integer, p_data jsonb, p_phases jsonb, p_teams jsonb, p_admin text)'
description: 'Crea (id NULL) o edita una ficha de torneo con sus fases y su lista de inscritos. SECURITY DEFINER: unica via de escritura para dday_clans. Clave repetida 23505; cambiar la clave con partidas 55006; fases invalidas 22023.'
resource: postgresql://dday/public/tournament_save
tags: [postgresql, function, ladder]
status: stable
generated: { by: process:okf-export, at: 2026-10-25T21:14:05Z }
sources: 
  - id: schema
    resource: /references/schema.sql
---

# Firma

```
tournament_save(p_id integer, p_data jsonb, p_phases jsonb, p_teams jsonb, p_admin text) RETURNS integer
```

Crea (id NULL) o edita una ficha de torneo con sus fases y su lista de inscritos. SECURITY DEFINER: unica via de escritura para dday_clans. Clave repetida 23505; cambiar la clave con partidas 55006; fases invalidas 22023.
