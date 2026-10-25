---
type: PostgreSQL Function
title: tournament_sides(p_event text)
description: 'Clan, fase y ronda de cada partida de un torneo: clan automatico por mayoria de humanos del lado (al menos la mitad, sin empate, entre los inscritos o los clanes activos), correcciones manuales y fase por fecha. counted es la unica definicion de "la partida cuenta" para cruces y tabla.'
resource: postgresql://dday/public/tournament_sides
tags: [postgresql, function, ladder]
status: stable
generated: { by: process:okf-export, at: 2026-10-25T21:14:05Z }
sources: 
  - id: schema
    resource: /references/schema.sql
---

# Firma

```
tournament_sides(p_event text) RETURNS TABLE(match_id text, detected0 integer, detected1 integer, clan0_id integer, clan1_id integer, excluded boolean, overridden boolean, counted boolean, phase_id integer, phase_manual boolean, round text)
```

Clan, fase y ronda de cada partida de un torneo: clan automatico por mayoria de humanos del lado (al menos la mitad, sin empate, entre los inscritos o los clanes activos), correcciones manuales y fase por fecha. counted es la unica definicion de "la partida cuenta" para cruces y tabla.
