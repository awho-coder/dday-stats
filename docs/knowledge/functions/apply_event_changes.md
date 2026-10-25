---
type: PostgreSQL Function
title: apply_event_changes()
description: Aplica los pedidos pendientes de match_event_changes (cambia matches.event y borra los datos manuales). Solo el dueno (ingestor); despues hay que recalcular resumenes y Elo.
resource: postgresql://dday/public/apply_event_changes
tags: [postgresql, function, ladder]
status: stable
generated: { by: process:okf-export, at: 2026-10-25T21:14:05Z }
sources: 
  - id: schema
    resource: /references/schema.sql
---

# Firma

```
apply_event_changes() RETURNS integer
```

Aplica los pedidos pendientes de match_event_changes (cambia matches.event y borra los datos manuales). Solo el dueno (ingestor); despues hay que recalcular resumenes y Elo.

# Relaciones

- Usa [match_event_changes](../tables/match_event_changes.md).
- Usa [matches](../tables/matches.md).
