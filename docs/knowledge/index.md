---
okf_version: "0.2"
---

# Base de conocimiento — ladder de D-Day

Bundle [Open Knowledge Format (OKF)](https://github.com/GoogleCloudPlatform/open-knowledge-format)
del sistema de estadísticas: una concept por tabla, vista y función, más las
reglas del ladder. Los archivos de `tables/`, `views/` y `functions/` se generan
desde el catálogo de PostgreSQL con `stats/okf_export.py` (reusa los comentarios
del esquema); `rules/` se escribe a mano.

Para regenerar (con la base a mano):

```sh
python3 stats/okf_export.py --dsn "$DDAY_STATS_DSN" --out docs/knowledge
```

`viz.html` es el grafo interactivo (autocontenido) del bundle; se abre en
cualquier navegador.

# Reglas

* [Reglas del ladder](rules/) - K/D, Elo, temporadas y categorías.

# Esquema

* [Tablas](tables/) - 17 tablas del esquema.
* [Vistas](views/) - 18 vistas del ladder.
* [Funciones](functions/) - 12 funciones del ladder.

# Material de origen

* [Referencias](references/) - `schema.sql` (el esquema comentado).
