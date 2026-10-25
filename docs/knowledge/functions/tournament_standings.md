---
type: PostgreSQL Function
title: tournament_standings(p_event text)
description: 'Tabla de posiciones por fase y grupo (PJ, G, E, P, frags, puntos), por mapa o por cruce segun la fase. La app solo la muestra en las fases de liga.'
resource: postgresql://dday/public/tournament_standings
tags: [postgresql, function, ladder]
status: stable
generated: { by: process:okf-export, at: 2026-10-25T21:14:05Z }
sources: 
  - id: schema
    resource: /references/schema.sql
---

# Firma

```
tournament_standings(p_event text) RETURNS TABLE(phase_id integer, group_name text, clan_id integer, played bigint, wins bigint, draws bigint, losses bigint, frags_for bigint, frags_against bigint, points bigint)
```

Tabla de posiciones por fase y grupo (PJ, G, E, P, frags, puntos), por mapa o por cruce segun la fase. La app solo la muestra en las fases de liga.
