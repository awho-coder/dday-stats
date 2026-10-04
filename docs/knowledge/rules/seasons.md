---
type: Rule
title: Temporadas y categorías
description: Cómo se separan los ladders por temporada, categoría y torneo.
tags: [ladder, reglas, temporadas, categorias]
status: stable
generated: { by: human:awho, at: 2026-10-04T00:00:00Z }
sources:
  - id: readme
    resource: https://github.com/awho-coder/dday-stats/blob/main/stats/README.md
---

# Temporadas

Las temporadas se inician a mano. Cada partida pertenece a la temporada vigente
cuando empezó. Los ladders muestran por defecto la temporada actual y el Elo
parte de 1500 en cada una. El histórico (todas las temporadas) se conserva.

```sh
python3 stats/ingest.py --new-season "Temporada 2"
python3 stats/ingest.py --new-season "Copa Invierno" --season-start "2026-07-01 00:00-04"
```

# Categorías

| categoría | qué incluye |
|---|---|
| `public` | partidas del servidor en modo público |
| `duel` | duelos casuales (`stats_mode duel`) |
| `official` | cualquier partida con `stats_event` (torneo) |
| `all` | todas |

Cada categoría tiene su propio ladder K/D y su propio Elo.

# Duelos oficiales de torneo

El admin pone el nombre del torneo antes de `sv startcount`:

```
set stats_event "copa-verano"
sv startcount 20
```

La cvar se mantiene entre mapas: al terminar el torneo hay que borrarla con
`set stats_event ""`.

# Ver también

- [Ladder por K/D](/rules/kd.md)
- [Ladder por Elo](/rules/elo.md)
- Vista [`v_seasons`](/views/v_seasons.md)
