---
type: Rule
title: Ladder por Elo
description: Cómo se calcula el rating Elo por equipos.
tags: [ladder, reglas, elo]
status: stable
generated: { by: human:awho, at: 2026-10-04T00:00:00Z }
sources:
  - id: readme
    resource: https://github.com/awho-coder/dday-stats/blob/main/stats/README.md
---

# Definición

Rating **Elo por equipos**, solo para partidas **rankeadas**. Se compara el
rating promedio de cada equipo (ponderado por el tiempo jugado) y cada jugador
gana o pierde `K × (resultado − esperado) × participación`.

# Cuándo una partida es rankeada

- Terminó normalmente, con ganador o empate.
- Duró lo suficiente: **5 min** en público (`--min-duration`), **2 min** en
  duelos (`--duel-min-duration`).
- Cada equipo tuvo suficientes humanos: **2** en público (`--min-humans`),
  **1** en duelos (`--duel-min-humans`).

Los bots nunca cuentan. Quien jugó menos del 20 % de la partida en su equipo
(`--min-participation`) no se ve afectado.

# Parámetros

- `K = 40` en las primeras 10 partidas, luego `24`.
- El Elo parte de **1500** en cada temporada.
- Hay tres categorías con rating separado: `public`, `duel` y `official`.

# Consulta

```sql
SELECT * FROM ladder_elo(10);                 -- Elo público, temporada actual
SELECT * FROM ladder_elo(5, 'official');      -- duelos oficiales
SELECT * FROM ladder_elo(10, 'public', 'all');-- histórico
```

# Ver también

- [Ladder por K/D](/rules/kd.md)
- [Temporadas y categorías](/rules/seasons.md)
- Función [`ladder_elo`](/functions/ladder_elo.md)
