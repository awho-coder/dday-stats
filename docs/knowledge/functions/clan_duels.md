---
type: PostgreSQL Function
title: 'clan_duels(match_kind text, season text, min_per_side integer)'
description: 'Duelos de clanes: partidas por equipos sin bots donde cada equipo esta formado solo por integrantes de un mismo clan (distintos entre si, ninguno sin inscribir), con un minimo por lado.'
resource: postgresql://dday/public/clan_duels
tags: [postgresql, function, ladder]
status: stable
generated: { by: process:okf-export, at: 2026-10-16T01:23:12Z }
sources: 
  - id: schema
    resource: /references/schema.sql
---

# Firma

```
clan_duels(match_kind text, season text, min_per_side integer) RETURNS TABLE(match_id text, clan0_id integer, clan0 text, clan0_players bigint, clan1_id integer, clan1 text, clan1_players bigint)
```

Duelos de clanes: partidas por equipos sin bots donde cada equipo esta formado solo por integrantes de un mismo clan (distintos entre si, ninguno sin inscribir), con un minimo por lado.
