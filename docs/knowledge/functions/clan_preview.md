---
type: PostgreSQL Function
title: 'clan_preview(p_clan_id integer, p_tags text[])'
description: 'Vista previa del mantenedor: nicks humanos que quedarian en un clan (o en uno nuevo si el id es NULL) con los tags indicados, y el clan del que vendrian.'
resource: postgresql://dday/public/clan_preview
tags: [postgresql, function, ladder]
status: stable
generated: { by: process:okf-export, at: 2026-10-16T01:23:12Z }
sources: 
  - id: schema
    resource: /references/schema.sql
---

# Firma

```
clan_preview(p_clan_id integer, p_tags text[]) RETURNS TABLE(name text, matches bigint, last_match timestamp with time zone, from_clan text)
```

Vista previa del mantenedor: nicks humanos que quedarian en un clan (o en uno nuevo si el id es NULL) con los tags indicados, y el clan del que vendrian.
