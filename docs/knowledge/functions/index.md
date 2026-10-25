# Funciones

* [apply_event_changes()](apply_event_changes.md) - Aplica los pedidos pendientes de match_event_changes (cambia matches.event y borra los datos manuales). Solo el dueno (ingestor); despues hay que recalcular resumenes y Elo.
* [apply_rollups(ids text[])](apply_rollups.md) - Suma las partidas indicadas a las tablas de resumen (una sola vez por partida).
* [clan_delete(p_id integer)](clan_delete.md) - Borra un clan y sus tags; true si existia. SECURITY DEFINER.
* [clan_duels(match_kind text, season text, min_per_side integer)](clan_duels.md) - Duelos de clanes: partidas por equipos sin bots donde cada equipo esta formado solo por integrantes de un mismo clan (distintos entre si, ninguno sin inscribir), con un minimo por lado.
* [clan_preview(p_clan_id integer, p_tags text[])](clan_preview.md) - Vista previa del mantenedor: nicks humanos que quedarian en un clan (o en uno nuevo si el id es NULL) con los tags indicados, y el clan del que vendrian.
* [clan_save(p_id integer, p_tags text[], p_data jsonb, p_admin text)](clan_save.md) - Crea (id NULL) o edita un clan y reemplaza sus tags (el primero es el principal). SECURITY DEFINER: es la unica via de escritura para dday_clans. Un tag repetido da el error 23505.
* [clan_tag_matches(nick text, tag text)](clan_tag_matches.md) - Regla de pertenencia: un tag solo de letras y numeros debe estar al comienzo del nick y seguido de un caracter que no sea letra ni numero; otros tags basta con que el nick los contenga. Sin distinguir mayusculas.
* [ladder_clans(match_kind text, season text)](ladder_clans.md) - Ranking de clanes: suma de sus miembros activos en la temporada y categoria (K/D sin muestra con menos de 20 muertes, Elo promedio, MVPs).
* [ladder_elo(min_games integer, match_kind text, season text)](ladder_elo.md) - Ladder por rating Elo, por categoria.
* [ladder_ffa(min_matches integer, match_kind text, season text)](ladder_ffa.md) - Free For All: victorias, podios, puesto promedio, K/D y mejor racha por jugador (sin Elo). Partidas y % de victorias cuentan solo las FFA terminadas (con puesto); K/D y racha, todas.
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
* [tournament_assign_save(p_match_ids text[], p_phase_id integer, p_round text, p_admin text)](tournament_assign_save.md) - Pone fase y ronda a varias partidas del mismo torneo (las de un cruce). SECURITY DEFINER.
* [tournament_delete(p_id integer)](tournament_delete.md) - Borra una ficha (fases e inscritos en cascada) y los datos manuales de sus partidas; no borra partidas. true si existia. SECURITY DEFINER.
* [tournament_event_request(p_match_ids text[], p_event text, p_admin text)](tournament_event_request.md) - Pide agregar partidas a un torneo (event de una ficha) o sacarlas (event vacio); lo aplica el ingestor. Devuelve cuantos pedidos creo. SECURITY DEFINER.
* [tournament_save(p_id integer, p_data jsonb, p_phases jsonb, p_teams jsonb, p_admin text)](tournament_save.md) - Crea (id NULL) o edita una ficha de torneo con sus fases y su lista de inscritos. SECURITY DEFINER: unica via de escritura para dday_clans. Clave repetida 23505; cambiar la clave con partidas 55006; fases invalidas 22023.
* [tournament_series(p_event text)](tournament_series.md) - Cruces de un torneo: partidas que cuentan entre el mismo par de clanes en la misma fase y ronda. Ganador por mapas, luego por frags (by_frags); winner_id NULL = empate.
* [tournament_side_clear(p_match_id text)](tournament_side_clear.md) - Vuelve a automatico los clanes y el "no cuenta" de una partida, sin perder fase ni ronda. SECURITY DEFINER.
* [tournament_side_prune(p_match_ids text[])](tournament_side_prune.md) - Auxiliar: borra las filas vacias de tournament_match_sides. Sin permisos para nadie mas que el dueno.
* [tournament_side_save(p_match_id text, p_clan0 integer, p_clan1 integer, p_excluded boolean, p_admin text)](tournament_side_save.md) - Fija el clan de cada lado de una partida de torneo y/o la marca "no cuenta" (no toca fase ni ronda). SECURITY DEFINER.
* [tournament_sides(p_event text)](tournament_sides.md) - Clan, fase y ronda de cada partida de un torneo: clan automatico por mayoria de humanos del lado (al menos la mitad, sin empate, entre los inscritos o los clanes activos), correcciones manuales y fase por fecha. counted es la unica definicion de "la partida cuenta" para cruces y tabla.
* [tournament_standings(p_event text)](tournament_standings.md) - Tabla de posiciones por fase y grupo (PJ, G, E, P, frags, puntos), por mapa o por cruce segun la fase. La app solo la muestra en las fases de liga.
