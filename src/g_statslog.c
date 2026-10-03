/*
 * g_statslog.c -- registro de eventos de partida para estadisticas / ladder.
 *
 * La DLL nunca habla con la base de datos: solo escribe un archivo JSONL
 * por partida (una linea por evento). Mientras la partida esta en curso el
 * archivo se llama <id>.jsonl.part; al terminar se renombra a <id>.jsonl y
 * stats/ingest.py lo carga en PostgreSQL. Ver docs/stats/PLAN.md.
 *
 * Cvars:
 *   stats_log      0/1  activa el registro (se lee al iniciar cada mapa)
 *   stats_server   ""   identificador del servidor (por defecto: hostname)
 *   stats_log_dir  ""   carpeta de salida (por defecto: <gamedir>/stats/events)
 *
 * Cualquier error de E/S solo se informa por consola; nunca detiene el juego.
 */

#include "g_local.h"
#include "x_fire.h"
#include <errno.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#define stats_mkdir(p)	_mkdir(p)
#else
#define stats_mkdir(p)	mkdir((p), 0755)
#endif

#define STATS_FORMAT_VERSION	1
#define STATS_NAME_LEN			32
#define STATS_PATH_LEN			256
#define STATS_NUM_CLASSES		(FLAMER + 1)
#define STATS_FLUSH_FRAMES		100		// fflush cada 10 segundos

#define STATS_WINNER_UNSET		99


static cvar_t *stats_log;
static cvar_t *stats_server;
static cvar_t *stats_log_dir;

// contador que nunca retrocede aunque el valor original se reinicie
// (por ejemplo resp.accuracy_hits vuelve a 0 con InitClientResp)
typedef struct
{
	int		base;
	int		last;
} stats_counter_t;

typedef struct
{
	qboolean	active;
	char		name[STATS_NAME_LEN];
	qboolean	bot;
	int			last_team;			// -2 desconocido, -1 observador, 0/1 equipo
	int			team_time[MAX_TEAMS];	// segundos jugados en cada equipo
	int			class_time[STATS_NUM_CLASSES];

	int			kills;
	int			deaths;
	int			suicides;
	int			teamkills;
	int			headshots;
	int			objectives;

	stats_counter_t	hits;
	stats_counter_t	misses;
	stats_counter_t	points;
	int				score;			// puede bajar (penalizaciones), se guarda el ultimo valor
} stats_player_t;

static struct
{
	qboolean	open;
	FILE		*file;
	char		match_id[96];
	char		part_path[STATS_PATH_LEN];
	char		final_path[STATS_PATH_LEN];
	float		last_time;		// ultimo level.time visto (para partidas abortadas)
	int			winner;
	qboolean	forced_end;
	stats_player_t	players[MAX_CLIENTS];
} sl;

static const char *class_names[STATS_NUM_CLASSES] =
{
	"none", "infantry", "officer", "lgunner", "hgunner",
	"sniper", "special", "engineer", "medic", "flamer"
};

/*
=================
Utilidades
=================
*/

static void StatsLog_Copy (char *dst, const char *src, size_t size)
{
	if (!size)
		return;
	strncpy (dst, src ? src : "", size - 1);
	dst[size - 1] = 0;
}

static const char *StatsLog_ModName (int mod)
{
	switch (mod)
	{
	case MOD_PISTOL:			return "pistol";
	case MOD_SHOTGUN:			return "shotgun";
	case MOD_SHOTGUN2:			return "shotgun";
	case MOD_RIFLE:				return "rifle";
	case MOD_LMG:				return "lmg";
	case MOD_HMG:				return "hmg";
	case MOD_GRENADE:			return "grenade";
	case MOD_G_SPLASH:			return "grenade";
	case MOD_ROCKET:			return "rocket";
	case MOD_R_SPLASH:			return "rocket";
	case MOD_SUBMG:				return "smg";
	case MOD_SNIPER:			return "sniper";
	case MOD_HANDGRENADE:		return "handgrenade";
	case MOD_HG_SPLASH:			return "handgrenade";
	case MOD_HELD_GRENADE:		return "held_grenade";
	case MOD_WATER:				return "water";
	case MOD_SLIME:				return "slime";
	case MOD_LAVA:				return "lava";
	case MOD_CRUSH:				return "crush";
	case MOD_TELEFRAG:			return "telefrag";
	case MOD_FALLING:			return "falling";
	case MOD_SUICIDE:			return "suicide";
	case MOD_EXPLOSIVE:			return "explosive";
	case MOD_BARREL:			return "barrel";
	case MOD_BOMB:				return "bomb";
	case MOD_EXIT:				return "exit";
	case MOD_SPLASH:			return "splash";
	case MOD_TARGET_LASER:		return "laser";
	case MOD_TRIGGER_HURT:		return "trigger_hurt";
	case MOD_HIT:				return "hit";
	case MOD_TARGET_BLASTER:	return "blaster";
	case MOD_BACKBLAST:			return "backblast";
	case MOD_WOUND:				return "wound";
	case MOD_KNIFE:				return "knife";
	case MOD_FISTS:				return "fists";
	case MOD_PENALTY:			return "penalty";
	case MOD_TNT:				return "tnt";
	case MOD_TNT1:				return "tnt";
	case MOD_HELD_TNT:			return "held_tnt";
	case MOD_TNT_SPLASH:		return "tnt";
	case MOD_TNT1_SPLASH:		return "tnt";
	case MOD_HELMET:			return "helmet";
	case MOD_AIRSTRIKE:			return "airstrike";
	case MOD_AIRSTRIKE_SPLASH:	return "airstrike";
	case MOD_BAYONET:			return "bayonet";
	case MOD_PLONK:				return "plonk";
	case MOD_SPAWNCAMP:			return "spawncamp";
	case MOD_BOTTLE:			return "molotov";
	case MOD_TANKHIT:			return "tank";
	case MOD_FIRE:				return "fire";
	case MOD_FIRE_SPLASH:		return "fire";
	case MOD_ON_FIRE:			return "fire";
	case MOD_FIREBALL:			return "fire";
	default:					return "unknown";
	}
}

// muertes donde el arma que el jugador tiene en la mano es la que mato
static qboolean StatsLog_ModUsesHeldWeapon (int mod)
{
	switch (mod)
	{
	case MOD_PISTOL:
	case MOD_SHOTGUN:
	case MOD_SHOTGUN2:
	case MOD_RIFLE:
	case MOD_LMG:
	case MOD_HMG:
	case MOD_SUBMG:
	case MOD_SNIPER:
	case MOD_ROCKET:
	case MOD_R_SPLASH:
	case MOD_KNIFE:
	case MOD_BAYONET:
	case MOD_FIRE:
		return true;
	default:
		return false;
	}
}

// nombre sin colores (bit alto) ni caracteres de control
static void StatsLog_CleanName (const char *in, char *out, size_t size)
{
	size_t	n = 0;
	int		c;

	if (in)
	{
		for ( ; *in && n < size - 1; in++)
		{
			c = (unsigned char)*in & 127;
			if (c < 32 || c == 127)
				continue;
			out[n++] = (char)c;
		}
	}

	// sin espacios al final
	while (n > 0 && out[n-1] == ' ')
		n--;
	out[n] = 0;

	if (!n)
		StatsLog_Copy (out, "unnamed", size);
}

// escribe un string JSON entre comillas; trata la entrada como ASCII de 7 bits
static void StatsLog_WriteString (const char *s)
{
	int c;

	if (!s)
	{
		fputs ("null", sl.file);
		return;
	}

	fputc ('"', sl.file);
	for ( ; *s; s++)
	{
		c = (unsigned char)*s & 127;
		if (c == '"' || c == '\\')
		{
			fputc ('\\', sl.file);
			fputc (c, sl.file);
		}
		else if (c < 32 || c == 127)
			continue;
		else
			fputc (c, sl.file);
	}
	fputc ('"', sl.file);
}

static void StatsLog_WriteKey (const char *key)
{
	fprintf (sl.file, ",\"%s\":", key);
}

static void StatsLog_BeginEvent (const char *ev)
{
	fprintf (sl.file, "{\"ev\":\"%s\",\"t\":%.1f", ev, level.time);
}

static void StatsLog_EndEvent (void)
{
	fputs ("}\n", sl.file);
}

static void StatsLog_CounterSet (stats_counter_t *c, int value)
{
	if (value < c->last)	// el contador original se reinicio
		c->base += c->last;
	c->last = value;
}

static int StatsLog_CounterGet (const stats_counter_t *c)
{
	return c->base + c->last;
}

static int StatsLog_TeamOf (edict_t *ent)
{
	if (!ent->client || !ent->client->resp.team_on || ent->flyingnun)
		return -1;
	if (ent->client->resp.team_on->index < 0 || ent->client->resp.team_on->index >= MAX_TEAMS)
		return -1;
	return ent->client->resp.team_on->index;
}

static int StatsLog_ClassOf (edict_t *ent)
{
	int mos;

	if (!ent->client)
		return NONE;
	mos = ent->client->resp.mos;
	if (mos < 0 || mos >= STATS_NUM_CLASSES)
		return NONE;
	return mos;
}

static int StatsLog_SlotOf (edict_t *ent)
{
	int slot;

	if (!ent || !ent->client)
		return -1;
	slot = (int)(ent - g_edicts) - 1;
	if (slot < 0 || slot >= game.maxclients || slot >= MAX_CLIENTS)
		return -1;
	return slot;
}

/*
=================
Jugadores
=================
*/

static void StatsLog_WritePlayer (stats_player_t *p, const char *reason)
{
	int			i;
	qboolean	first;

	StatsLog_BeginEvent ("player");
	StatsLog_WriteKey ("name");
	StatsLog_WriteString (p->name);
	fprintf (sl.file, ",\"bot\":%d,\"team\":%d", p->bot ? 1 : 0, p->last_team < -1 ? -1 : p->last_team);
	fprintf (sl.file, ",\"time\":[%d,%d]", p->team_time[0], p->team_time[1]);

	fputs (",\"classes\":{", sl.file);
	first = true;
	for (i = 1; i < STATS_NUM_CLASSES; i++)
	{
		if (!p->class_time[i])
			continue;
		fprintf (sl.file, "%s\"%s\":%d", first ? "" : ",", class_names[i], p->class_time[i]);
		first = false;
	}
	fputc ('}', sl.file);

	fprintf (sl.file, ",\"kills\":%d,\"deaths\":%d,\"suicides\":%d,\"tk\":%d,\"hs\":%d,\"objs\":%d",
		p->kills, p->deaths, p->suicides, p->teamkills, p->headshots, p->objectives);
	fprintf (sl.file, ",\"hits\":%d,\"misses\":%d,\"score\":%d,\"points\":%d",
		StatsLog_CounterGet (&p->hits), StatsLog_CounterGet (&p->misses),
		p->score, StatsLog_CounterGet (&p->points));
	StatsLog_WriteKey ("reason");
	StatsLog_WriteString (reason);
	StatsLog_EndEvent ();
}

// escribe el resumen del jugador y libera el slot
static void StatsLog_FlushPlayer (int slot, const char *reason)
{
	stats_player_t *p = &sl.players[slot];

	if (!p->active)
		return;

	// no registrar a quien nunca entro a un equipo ni hizo nada
	if (p->team_time[0] || p->team_time[1] || p->kills || p->deaths)
		StatsLog_WritePlayer (p, reason);

	memset (p, 0, sizeof(*p));
}

static stats_player_t *StatsLog_GetPlayer (edict_t *ent)
{
	int				slot;
	stats_player_t	*p;
	char			name[STATS_NAME_LEN];

	slot = StatsLog_SlotOf (ent);
	if (slot < 0)
		return NULL;

	p = &sl.players[slot];
	StatsLog_CleanName (ent->client->pers.netname, name, sizeof(name));

	// la identidad es el nombre: si cambia, se cierra el registro anterior
	if (p->active && strcmp (p->name, name))
		StatsLog_FlushPlayer (slot, "rename");

	if (!p->active)
	{
		memset (p, 0, sizeof(*p));
		p->active = true;
		p->bot = ent->ai ? true : false;
		p->last_team = -2;
		StatsLog_Copy (p->name, name, sizeof(p->name));
	}

	return p;
}

static void StatsLog_SamplePlayer (edict_t *ent, qboolean add_time)
{
	stats_player_t	*p;
	int				team;

	p = StatsLog_GetPlayer (ent);
	if (!p)
		return;

	team = StatsLog_TeamOf (ent);
	if (team != p->last_team)
	{
		StatsLog_BeginEvent ("team");
		fprintf (sl.file, ",\"slot\":%d", StatsLog_SlotOf (ent));
		StatsLog_WriteKey ("name");
		StatsLog_WriteString (p->name);
		fprintf (sl.file, ",\"bot\":%d,\"team\":%d", p->bot ? 1 : 0, team);
		StatsLog_EndEvent ();
		p->last_team = team;
	}

	if (add_time && team >= 0)
	{
		p->team_time[team]++;
		p->class_time[StatsLog_ClassOf (ent)]++;
	}

	StatsLog_CounterSet (&p->hits, ent->client->resp.accuracy_hits);
	StatsLog_CounterSet (&p->misses, ent->client->resp.accuracy_misses);
	p->score = ent->client->resp.score;
	StatsLog_CounterSet (&p->points, ent->client->resp.points);
}

static void StatsLog_SampleAll (qboolean add_time)
{
	int		i;
	edict_t	*ent;

	for (i = 0; i < game.maxclients && i < MAX_CLIENTS; i++)
	{
		ent = g_edicts + 1 + i;
		if (!ent->inuse || !ent->client || !ent->client->pers.connected)
			continue;
		StatsLog_SamplePlayer (ent, add_time);
	}
}

/*
=================
Archivo de partida
=================
*/

// crea <base>/<sub>/... ; ignora errores (fopen informara si algo falla)
static void StatsLog_MakePath (char *path)
{
	char *s;

	for (s = path + 1; *s; s++)
	{
		if (*s == '/' || *s == '\\')
		{
			char c = *s;
			*s = 0;
			stats_mkdir (path);
			*s = c;
		}
	}
	stats_mkdir (path);
}

static FILE *StatsLog_OpenInDir (const char *dir, const char *file_name)
{
	char	path[STATS_PATH_LEN];
	FILE	*f;

	if (!dir || !*dir)
		return NULL;

	StatsLog_Copy (path, dir, sizeof(path));
	StatsLog_MakePath (path);

	if (snprintf (sl.part_path, sizeof(sl.part_path), "%s/%s.jsonl.part", dir, file_name) >= (int)sizeof(sl.part_path) ||
		snprintf (sl.final_path, sizeof(sl.final_path), "%s/%s.jsonl", dir, file_name) >= (int)sizeof(sl.final_path))
		return NULL;

	f = fopen (sl.part_path, "w");
	return f;
}

static qboolean StatsLog_OpenFile (void)
{
	char	dir[STATS_PATH_LEN];
	char	*bases[3];
	int		i;

	if (*stats_log_dir->string)
	{
		sl.file = StatsLog_OpenInDir (stats_log_dir->string, sl.match_id);
	}
	else
	{
		bases[0] = sys_homedir ? sys_homedir->string : NULL;
		bases[1] = sys_basedir ? sys_basedir->string : NULL;
		bases[2] = ".";

		for (i = 0; i < 3 && !sl.file; i++)
		{
			if (!bases[i] || !*bases[i])
				continue;
			if (snprintf (dir, sizeof(dir), "%s/%s/stats/events", bases[i], GAMEVERSION) >= (int)sizeof(dir))
				continue;
			sl.file = StatsLog_OpenInDir (dir, sl.match_id);
		}
	}

	if (!sl.file)
	{
		gi.dprintf ("StatsLog: no se pudo crear el archivo de eventos (%s); revise stats_log_dir.\n", strerror (errno));
		return false;
	}

	return true;
}

static void StatsLog_ServerId (char *out, size_t size)
{
	const char	*src;
	size_t		n = 0;
	int			c;

	src = stats_server->string;
	if (!*src)
		src = gi.cvar ("hostname", "", 0)->string;

	for ( ; *src && n < size - 1; src++)
	{
		c = (unsigned char)*src & 127;
		if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-')
			out[n++] = (char)c;
		else if (c == ' ' || c == '.')
			out[n++] = '_';
	}
	out[n] = 0;

	if (!n)
		StatsLog_Copy (out, "server", size);
}

static void StatsLog_CloseFile (void)
{
	if (!sl.file)
		return;

	fclose (sl.file);
	sl.file = NULL;

	if (rename (sl.part_path, sl.final_path))
		gi.dprintf ("StatsLog: no se pudo renombrar %s (%s)\n", sl.part_path, strerror (errno));
}

/*
=================
Ciclo de vida
=================
*/

void StatsLog_Init (void)
{
	stats_log = gi.cvar ("stats_log", "0", 0);
	stats_server = gi.cvar ("stats_server", "", 0);
	stats_log_dir = gi.cvar ("stats_log_dir", "", 0);

	memset (&sl, 0, sizeof(sl));
	sl.winner = STATS_WINNER_UNSET;
}

// cierra una partida que no llego a la intermision (cambio de mapa por consola,
// cierre del servidor). Solo usa datos propios, porque level ya puede estar reiniciado.
static void StatsLog_Abort (void)
{
	int i;

	if (!sl.open)
		return;

	for (i = 0; i < MAX_CLIENTS; i++)
		StatsLog_FlushPlayer (i, "aborted");

	fprintf (sl.file, "{\"ev\":\"match_end\",\"t\":%.1f,\"ts\":%ld,\"dur\":%.1f,\"winner\":null,\"reason\":\"aborted\",\"teams\":[]}\n",
		sl.last_time, (long)time (NULL), sl.last_time);

	StatsLog_CloseFile ();
	sl.open = false;
}

void StatsLog_Shutdown (void)
{
	if (!stats_log)
		return;
	StatsLog_Abort ();
}

// llamado al final de SpawnEntities, cuando team_list ya esta armado
void StatsLog_MatchBegin (void)
{
	char		server[32];
	char		stamp[32];
	const char	*mode;
	time_t		now;
	struct tm	*tm;
	int			i;
	qboolean	first;

	if (!stats_log)
		return;

	StatsLog_Abort ();

	memset (sl.players, 0, sizeof(sl.players));
	sl.winner = STATS_WINNER_UNSET;
	sl.forced_end = false;
	sl.last_time = 0;

	if (!stats_log->value || !deathmatch->value)
		return;

	now = time (NULL);
	tm = gmtime (&now);
	if (!tm || !strftime (stamp, sizeof(stamp), "%Y%m%d-%H%M%S", tm))
		StatsLog_Copy (stamp, "00000000-000000", sizeof(stamp));

	StatsLog_ServerId (server, sizeof(server));
	Com_sprintf (sl.match_id, sizeof(sl.match_id), "%s-%s-%04x", stamp, server, rand () & 0xffff);

	if (!StatsLog_OpenFile ())
		return;

	sl.open = true;

	if (level.campaign && *level.campaign)
		mode = "campaign";
	else if (ctb_mode->value)
		mode = "ctb";
	else
		mode = "dm";

	StatsLog_BeginEvent ("match_start");
	fprintf (sl.file, ",\"v\":%d", STATS_FORMAT_VERSION);
	StatsLog_WriteKey ("match");
	StatsLog_WriteString (sl.match_id);
	StatsLog_WriteKey ("server");
	StatsLog_WriteString (server);
	StatsLog_WriteKey ("map");
	StatsLog_WriteString (level.mapname);
	StatsLog_WriteKey ("mode");
	StatsLog_WriteString (mode);
	fprintf (sl.file, ",\"tournament\":%d,\"ts\":%ld,\"teams\":[", tournament->value ? 1 : 0, (long)now);
	first = true;
	for (i = 0; i < MAX_TEAMS; i++)
	{
		if (!team_list[i])
			continue;
		fprintf (sl.file, "%s{\"idx\":%d,\"army\":", first ? "" : ",", i);
		StatsLog_WriteString (team_list[i]->teamid);
		fputs (",\"name\":", sl.file);
		StatsLog_WriteString (team_list[i]->teamname);
		fputc ('}', sl.file);
		first = false;
	}
	fputc (']', sl.file);
	StatsLog_EndEvent ();
	fflush (sl.file);
}

void StatsLog_RunFrame (void)
{
	if (!sl.open || level.intermissiontime)
		return;

	sl.last_time = level.time;

	// tiempo por equipo y clase, con resolucion de 1 segundo
	if (level.framenum % 10 == 0)
		StatsLog_SampleAll (true);

	if (level.framenum % STATS_FLUSH_FRAMES == 0)
		fflush (sl.file);
}

// llamado al inicio de EndDMLevel: en ese punto Last_Team_Winner ya fue
// decidido por CheckDMRules (salvo que la partida se haya forzado)
void StatsLog_EndDMLevel (void)
{
	if (!sl.open || sl.forced_end)
		return;

	if (Last_Team_Winner == 0 || Last_Team_Winner == 1 || Last_Team_Winner == -1)
		sl.winner = Last_Team_Winner;
}

// fin de partida por comando de admin (sv nextmap, sv maplist next...)
void StatsLog_MarkForcedEnd (void)
{
	sl.forced_end = true;
	sl.winner = STATS_WINNER_UNSET;
}

// llamado desde BeginIntermission
void StatsLog_MatchEnd (void)
{
	int			i;
	const char	*reason;
	qboolean	first;

	if (!sl.open)
		return;

	StatsLog_SampleAll (false);

	for (i = 0; i < MAX_CLIENTS; i++)
		StatsLog_FlushPlayer (i, "end");

	if (sl.forced_end)
		reason = "forced";
	else if (sl.winner != STATS_WINNER_UNSET)
		reason = "normal";
	else
		reason = "other";

	StatsLog_BeginEvent ("match_end");
	fprintf (sl.file, ",\"ts\":%ld,\"dur\":%.1f", (long)time (NULL), level.time);
	if (sl.winner == STATS_WINNER_UNSET)
		fputs (",\"winner\":null", sl.file);
	else
		fprintf (sl.file, ",\"winner\":%d", sl.winner);
	StatsLog_WriteKey ("reason");
	StatsLog_WriteString (reason);
	fputs (",\"teams\":[", sl.file);
	first = true;
	for (i = 0; i < MAX_TEAMS; i++)
	{
		if (!team_list[i])
			continue;
		fprintf (sl.file, "%s{\"idx\":%d,\"score\":%d,\"kills\":%d,\"losses\":%d}",
			first ? "" : ",", i, team_list[i]->score, team_list[i]->kills, team_list[i]->losses);
		first = false;
	}
	fputc (']', sl.file);
	StatsLog_EndEvent ();

	StatsLog_CloseFile ();
	sl.open = false;
}

/*
=================
Eventos
=================
*/

void StatsLog_ClientDisconnect (edict_t *ent)
{
	int slot;

	if (!sl.open || level.intermissiontime)
		return;

	slot = StatsLog_SlotOf (ent);
	if (slot < 0)
		return;

	StatsLog_SamplePlayer (ent, false);
	StatsLog_FlushPlayer (slot, "leave");
}

static void StatsLog_WriteActor (const char *key, edict_t *ent, stats_player_t *p, int team)
{
	StatsLog_WriteKey (key);
	fputs ("{\"name\":", sl.file);
	StatsLog_WriteString (p->name);
	fprintf (sl.file, ",\"bot\":%d,\"team\":%d,\"class\":\"%s\",\"pos\":[%d,%d,%d]}",
		p->bot ? 1 : 0, team, class_names[StatsLog_ClassOf (ent)],
		(int)ent->s.origin[0], (int)ent->s.origin[1], (int)ent->s.origin[2]);
}

// llamado desde Killed() cuando muere un jugador (targ->deadflag aun en 0)
void StatsLog_Kill (edict_t *targ, edict_t *inflictor, edict_t *attacker)
{
	stats_player_t	*victim, *killer_p = NULL;
	edict_t			*killer;
	int				mod, vteam, kteam = -1;
	qboolean		suicide, ff = false, hs = false;
	const char		*weapon = NULL;
	vec3_t			d;

	if (!sl.open || level.intermissiontime || !targ || !targ->client)
		return;

	mod = meansOfDeath & ~MOD_FRIENDLY_FIRE;

	// cambiar de equipo no es una muerte real
	if (mod == MOD_CHANGETEAM || mod == MOD_CHANGETEAM_WOUNDED)
		return;

	// igual que Killed(): si murio desangrado, el credito es de quien lo hirio
	killer = attacker;
	if (killer == targ || killer == NULL || killer == world)
	{
		killer = targ->client->last_wound_inflictor;
		if (killer == targ)
			killer = NULL;
	}
	if (killer && (!killer->inuse || !killer->client))
		killer = NULL;

	victim = StatsLog_GetPlayer (targ);
	if (!victim)
		return;
	vteam = StatsLog_TeamOf (targ);

	if (killer)
	{
		killer_p = StatsLog_GetPlayer (killer);
		if (!killer_p)
			killer = NULL;
	}

	suicide = (killer == NULL);
	victim->deaths++;

	if (suicide)
		victim->suicides++;
	else
	{
		kteam = StatsLog_TeamOf (killer);
		ff = (kteam >= 0 && kteam == vteam);
		hs = (targ->client->resp.deathblend == 1);

		if (ff)
			killer_p->teamkills++;
		else
		{
			killer_p->kills++;
			if (hs)
				killer_p->headshots++;
		}

		if (StatsLog_ModUsesHeldWeapon (mod) && killer->client->pers.weapon)
			weapon = killer->client->pers.weapon->pickup_name;
	}

	StatsLog_BeginEvent ("kill");
	StatsLog_WriteActor ("victim", targ, victim, vteam);
	if (killer)
		StatsLog_WriteActor ("killer", killer, killer_p, kteam);
	else
		fputs (",\"killer\":null", sl.file);
	fprintf (sl.file, ",\"mod\":\"%s\"", StatsLog_ModName (mod));
	StatsLog_WriteKey ("weapon");
	StatsLog_WriteString (weapon);
	fprintf (sl.file, ",\"hs\":%d,\"ff\":%d,\"suicide\":%d", hs ? 1 : 0, ff ? 1 : 0, suicide ? 1 : 0);
	if (killer)
	{
		VectorSubtract (killer->s.origin, targ->s.origin, d);
		fprintf (sl.file, ",\"dist\":%d", (int)VectorLength (d));
	}
	StatsLog_EndEvent ();
}

void StatsLog_Objective (const char *type, const char *name, int team, edict_t *player)
{
	stats_player_t *p = NULL;

	if (!sl.open || level.intermissiontime)
		return;

	if (player && player->client)
	{
		p = StatsLog_GetPlayer (player);
		if (p)
			p->objectives++;
	}

	StatsLog_BeginEvent ("obj");
	StatsLog_WriteKey ("type");
	StatsLog_WriteString (type);
	StatsLog_WriteKey ("name");
	StatsLog_WriteString (name);
	fprintf (sl.file, ",\"team\":%d", (team >= 0 && team < MAX_TEAMS) ? team : -1);
	StatsLog_WriteKey ("player");
	StatsLog_WriteString (p ? p->name : NULL);
	StatsLog_EndEvent ();
}
