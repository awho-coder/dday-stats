/*
 * g_statslog.h -- registro de eventos de partida para estadisticas / ladder.
 *
 * Escribe un archivo JSONL por partida en dday/stats/events/ (o en
 * stats_log_dir). Un proceso externo (stats/ingest.py) lo carga en
 * PostgreSQL. Ver docs/stats/PLAN.md.
 *
 * Se activa con "set stats_log 1".
 */

#ifndef G_STATSLOG_H
#define G_STATSLOG_H

// tipos de objetivo para StatsLog_Objective()
#define STATS_OBJ_TOUCH			"touch"
#define STATS_OBJ_AREA			"area"
#define STATS_OBJ_TIMED			"timed"
#define STATS_OBJ_TIMED_HELD	"timed_held"
#define STATS_OBJ_EXPLOSIVE		"explosive"
#define STATS_OBJ_BC_PICKUP		"bc_pickup"
#define STATS_OBJ_BC_DROP		"bc_drop"
#define STATS_OBJ_BC_CAPTURE	"bc_capture"

void StatsLog_Init (void);
void StatsLog_Shutdown (void);

void StatsLog_MatchBegin (void);
void StatsLog_RunFrame (void);
void StatsLog_EndDMLevel (void);
void StatsLog_MarkForcedEnd (void);
void StatsLog_MatchEnd (void);

void StatsLog_ClientDisconnect (edict_t *ent);
void StatsLog_Kill (edict_t *targ, edict_t *inflictor, edict_t *attacker);
void StatsLog_Objective (const char *type, const char *name, int team, edict_t *player);

void StatsLog_CountdownDone (void);
void StatsLog_CountdownReset (void);

#endif
