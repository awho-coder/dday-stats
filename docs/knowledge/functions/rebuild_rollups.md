---
type: PostgreSQL Function
title: rebuild_rollups()
description: Recalcula todas las tablas de resumen desde las tablas de detalle.
resource: postgresql://dday/public/rebuild_rollups
tags: [postgresql, function, ladder]
status: stable
generated: { by: process:okf-export, at: 2026-10-04T18:27:40Z }
sources: 
  - id: schema
    resource: /references/schema.sql
---

# Firma

```
rebuild_rollups() RETURNS void
```

Recalcula todas las tablas de resumen desde las tablas de detalle.
