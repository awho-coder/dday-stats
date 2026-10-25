---
type: PostgreSQL Function
title: tournament_series(p_event text)
description: 'Cruces de un torneo: partidas que cuentan entre el mismo par de clanes en la misma fase y ronda. Ganador por mapas, luego por frags (by_frags); winner_id NULL = empate.'
resource: postgresql://dday/public/tournament_series
tags: [postgresql, function, ladder]
status: stable
generated: { by: process:okf-export, at: 2026-10-25T21:14:05Z }
sources: 
  - id: schema
    resource: /references/schema.sql
---

# Firma

```
tournament_series(p_event text) RETURNS TABLE(phase_id integer, round text, clan_a integer, clan_b integer, matches bigint, maps_a bigint, maps_b bigint, map_draws bigint, frags_a bigint, frags_b bigint, winner_id integer, by_frags boolean, first_match timestamp with time zone, last_match timestamp with time zone)
```

Cruces de un torneo: partidas que cuentan entre el mismo par de clanes en la misma fase y ronda. Ganador por mapas, luego por frags (by_frags); winner_id NULL = empate.
