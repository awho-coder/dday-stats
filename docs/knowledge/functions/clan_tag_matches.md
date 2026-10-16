---
type: PostgreSQL Function
title: 'clan_tag_matches(nick text, tag text)'
description: 'Regla de pertenencia: un tag solo de letras y numeros debe estar al comienzo del nick y seguido de un caracter que no sea letra ni numero; otros tags basta con que el nick los contenga. Sin distinguir mayusculas.'
resource: postgresql://dday/public/clan_tag_matches
tags: [postgresql, function, ladder]
status: stable
generated: { by: process:okf-export, at: 2026-10-16T01:23:12Z }
sources: 
  - id: schema
    resource: /references/schema.sql
---

# Firma

```
clan_tag_matches(nick text, tag text) RETURNS boolean
```

Regla de pertenencia: un tag solo de letras y numeros debe estar al comienzo del nick y seguido de un caracter que no sea letra ni numero; otros tags basta con que el nick los contenga. Sin distinguir mayusculas.
