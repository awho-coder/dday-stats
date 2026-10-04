---
type: Rule
title: Ladder por K/D
description: Cómo se calcula el ranking por kills y muertes.
tags: [ladder, reglas, kd]
status: stable
generated: { by: human:awho, at: 2026-10-04T00:00:00Z }
sources:
  - id: readme
    resource: https://github.com/awho-coder/dday-stats/blob/main/stats/README.md
---

# Definición

El ladder por K/D ordena a los jugadores por `kills / muertes`, contando **solo
enfrentamientos entre humanos**: las kills a bots y las muertes causadas por
bots no cuentan (se muestran aparte en `bot_kills`). Aparecen quienes tienen al
menos un mínimo de partidas (por defecto 10).

# Reglas

- Solo partidas terminadas; las abortadas igual cuentan para el K/D.
- Los bots nunca cuentan.
- El mínimo de partidas es configurable (`ladder_kd(min_matches)`).

# Consulta

```sql
SELECT * FROM ladder_kd(10);                 -- temporada actual, mínimo 10 partidas
SELECT * FROM ladder_kd(10, 'public');       -- solo público
SELECT * FROM ladder_kd(1, 'official');      -- duelos oficiales
```

# Ver también

- [Ladder por Elo](elo.md)
- [Temporadas y categorías](seasons.md)
- Función [`ladder_kd`](../functions/ladder_kd.md)
