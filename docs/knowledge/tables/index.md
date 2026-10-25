# Tablas

* [clan_tags](clan_tags.md) - Tags de cada clan: uno principal y 0 o mas alternativos, todos cuentan para la pertenencia (regla en clan_tag_matches).
* [clans](clans.md) - Clanes del Ladder Test (se editan desde el panel de administrador; escritura solo por clan_save / clan_delete).
* [daily_players](daily_players.md) - Que jugador jugo cada dia (para no contarlo dos veces en daily_stats.players).
* [daily_stats](daily_stats.md) - Actividad por dia (hora de Chile): partidas y jugadores nuevos.
* [kills](kills.md) - Una kill (o suicidio/muerte por entorno). Tabla de detalle, la mas grande.
* [map_stats](map_stats.md) - Resumen por mapa, categoria y modo de juego (balance, duracion, letalidad).
* [map_weapon_stats](map_weapon_stats.md) - Kills por mapa, categoria y arma.
* [match_event_changes](match_event_changes.md) - Pedidos de cambio de torneo de una partida, creados por el panel admin (tournament_event_request) y aplicados por el ingestor (apply_event_changes).
* [match_players](match_players.md) - Resumen de un jugador en una partida (suma de sus tramos/reconexiones).
* [matches](matches.md) - Una partida (un mapa jugado en un servidor).
* [objectives](objectives.md) - Eventos de objetivo de una partida (banderas, maletin, zonas, etc.).
* [player_class_stats](player_class_stats.md) - Segundos jugados por clase y jugador.
* [player_duel_stats](player_duel_stats.md) - Kills entre humanos (nemesis / victima favorita).
* [player_map_stats](player_map_stats.md) - Resumen por jugador y mapa (todas las temporadas).
* [player_stats](player_stats.md) - Resumen por jugador, temporada, categoria, modo de juego y torneo (lo mantiene apply_rollups).
* [player_weapon_stats](player_weapon_stats.md) - Kills por arma y jugador (arma en mano o, si no aplica, la causa).
* [players](players.md) - Jugador o bot. La identidad es el nombre (sin colores).
* [rating_history](rating_history.md) - Elo antes/despues de cada partida (para graficos de evolucion).
* [ratings](ratings.md) - Elo por equipos, por jugador, categoria (public/duel/official) y temporada (todos los modos juntos).
* [seasons](seasons.md) - Temporadas del ladder (se crean con "ingest.py --new-season NOMBRE").
* [tournament_match_sides](tournament_match_sides.md) - Datos manuales de una partida de torneo: clan de cada lado, "no cuenta", fase y ronda. Se escribe con tournament_side_save / tournament_assign_save.
* [tournament_phases](tournament_phases.md) - Fases de un torneo (1 a 10, en orden), cada una con su modalidad. Una partida cae en la fase con el starts_at mas reciente que no sea posterior a ella; la primera fase siempre empieza desde el inicio.
* [tournament_teams](tournament_teams.md) - Clanes inscritos en un torneo, con su grupo. Los grupos y los inscritos sin partidas solo se usan en la primera fase.
* [tournaments](tournaments.md) - Fichas de torneo del Ladder Test (se editan desde el panel de administrador; escritura solo por tournament_save / tournament_delete). La clave es el valor de stats_event (matches.event).
