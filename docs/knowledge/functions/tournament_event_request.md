---
type: PostgreSQL Function
title: 'tournament_event_request(p_match_ids text[], p_event text, p_admin text)'
description: Pide agregar partidas a un torneo (event de una ficha) o sacarlas (event vacio); lo aplica el ingestor. Devuelve cuantos pedidos creo. SECURITY DEFINER.
resource: postgresql://dday/public/tournament_event_request
tags: [postgresql, function, ladder]
status: stable
generated: { by: process:okf-export, at: 2026-10-25T21:14:05Z }
sources: 
  - id: schema
    resource: /references/schema.sql
---

# Firma

```
tournament_event_request(p_match_ids text[], p_event text, p_admin text) RETURNS integer
```

Pide agregar partidas a un torneo (event de una ficha) o sacarlas (event vacio); lo aplica el ingestor. Devuelve cuantos pedidos creo. SECURITY DEFINER.
