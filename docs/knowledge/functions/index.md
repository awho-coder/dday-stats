# Funciones

* [apply_rollups(ids text[])](apply_rollups.md) - Suma las partidas indicadas a las tablas de resumen (una sola vez por partida).
* [ladder_elo(min_games integer, match_kind text, season text)](ladder_elo.md) - Ladder por rating Elo, por categoria.
* [ladder_kd(min_matches integer, match_kind text, season text, match_event text)](ladder_kd.md) - Ladder por K/D (humano vs humano).
* [ladder_luck(min_matches integer, match_kind text, season text)](ladder_luck.md) - Los mas suertudos (casco que desvia + pie salvado).
* [ladder_streak(match_kind text, max_rows integer, season text)](ladder_streak.md) - Mejores rachas de kills (una fila por partida).
* [ladder_streak_tiers(min_matches integer, match_kind text, season text)](ladder_streak_tiers.md) - Rachas por nivel: quien mas llego a GODLIKE, UNSTOPPABLE, etc.
* [ladder_unlucky(min_matches integer, match_kind text, season text)](ladder_unlucky.md) - Los tiradores mas desafortunados (tiros desviados por cascos).
* [match_category(kind text, event text)](match_category.md) - Categoria de ladder de una partida: official si tiene torneo, si no el modo (public/duel).
* [player_totals(season text, match_kind text, match_event text)](player_totals.md) - Totales por jugador para una temporada, categoria y torneo (base de los ladders).
* [rebuild_rollups()](rebuild_rollups.md) - Recalcula todas las tablas de resumen desde las tablas de detalle.
* [season_lookup(season text)](season_lookup.md) - Resuelve una temporada: current (la vigente), all (NULL) o el nombre.
* [streak_base()](streak_base.md) - Kills seguidas para el primer anuncio de racha (igual al cvar exbattleinfo).
