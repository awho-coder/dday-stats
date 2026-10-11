---
type: PostgreSQL Function
title: 'ladder_zone(min_matches integer, match_kind text, season text)'
description: 'Modo control: capturas, minutos y kills en la zona por jugador.'
resource: postgresql://dday/public/ladder_zone
tags: [postgresql, function, ladder]
status: stable
generated: { by: process:okf-export, at: 2026-10-11T00:24:11Z }
sources: 
  - id: schema
    resource: /references/schema.sql
---

# Firma

```
ladder_zone(min_matches integer, match_kind text, season text) RETURNS TABLE(pos bigint, name text, matches bigint, wins bigint, win_pct numeric, zone_captures bigint, zone_minutes numeric, captures_per_match numeric, zone_kills bigint)
```

Modo control: capturas, minutos y kills en la zona por jugador.

# Relaciones

- Usa [kills](../tables/kills.md).
