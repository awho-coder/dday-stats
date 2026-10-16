# Funciones

* [apply_rollups(ids text[])](apply_rollups.md) - Suma las partidas indicadas a las tablas de resumen (una sola vez por partida).
* [clan_delete(p_id integer)](clan_delete.md) - Borra un clan y sus tags; true si existia. SECURITY DEFINER.
* [clan_duels(match_kind text, season text, min_per_side integer)](clan_duels.md) - Duelos de clanes: partidas por equipos sin bots donde cada equipo esta formado solo por integrantes de un mismo clan (distintos entre si, ninguno sin inscribir), con un minimo por lado.
* [clan_preview(p_clan_id integer, p_tags text[])](clan_preview.md) - Vista previa del mantenedor: nicks humanos que quedarian en un clan (o en uno nuevo si el id es NULL) con los tags indicados, y el clan del que vendrian.
* [clan_save(p_id integer, p_tags text[], p_data jsonb, p_admin text)](clan_save.md) - Crea (id NULL) o edita un clan y reemplaza sus tags (el primero es el principal). SECURITY DEFINER: es la unica via de escritura para dday_clans. Un tag repetido da el error 23505.
* [clan_tag_matches(nick text, tag text)](clan_tag_matches.md) - Regla de pertenencia: un tag solo de letras y numeros debe estar al comienzo del nick y seguido de un caracter que no sea letra ni numero; otros tags basta con que el nick los contenga. Sin distinguir mayusculas.
* [ladder_clans(match_kind text, season text)](ladder_clans.md) - Ranking de clanes: suma de sus miembros activos en la temporada y categoria (K/D sin muestra con menos de 20 muertes, Elo promedio, MVPs).
* [ladder_elo(min_games integer, match_kind text, season text)](ladder_elo.md) - Ladder por rating Elo, por categoria.
* [ladder_ffa(min_matches integer, match_kind text, season text)](ladder_ffa.md) - Free For All: victorias, podios, puesto promedio, K/D y mejor racha por jugador (sin Elo).
* [ladder_kd(min_matches integer, match_kind text, season text, match_event text, match_mode text)](ladder_kd.md) - Ladder por K/D (humano vs humano), opcionalmente de un modo.
* [ladder_luck(min_matches integer, match_kind text, season text, match_mode text)](ladder_luck.md) - Los mas suertudos (casco que desvia + pie salvado).
* [ladder_streak(match_kind text, max_rows integer, season text, match_mode text)](ladder_streak.md) - Mejores rachas de kills (una fila por partida), opcionalmente de un modo.
* [ladder_streak_tiers(min_matches integer, match_kind text, season text, match_mode text)](ladder_streak_tiers.md) - Rachas por nivel: quien mas llego a GODLIKE, UNSTOPPABLE, etc.
* [ladder_unlucky(min_matches integer, match_kind text, season text, match_mode text)](ladder_unlucky.md) - Los tiradores mas desafortunados (tiros desviados por cascos).
* [ladder_zone(min_matches integer, match_kind text, season text)](ladder_zone.md) - Modo control: capturas, minutos y kills en la zona por jugador.
* [match_category(kind text, event text)](match_category.md) - Categoria de ladder de una partida: official si tiene torneo, si no el modo (public/duel).
* [player_totals(season text, match_kind text, match_event text, match_mode text)](player_totals.md) - Totales por jugador para una temporada, categoria, torneo y modo (base de los ladders).
* [rebuild_rollups()](rebuild_rollups.md) - Recalcula todas las tablas de resumen desde las tablas de detalle.
* [season_lookup(season text)](season_lookup.md) - Resuelve una temporada: current (la vigente), all (NULL) o el nombre.
* [streak_base()](streak_base.md) - Kills seguidas para el primer anuncio de racha (igual al cvar exbattleinfo).
