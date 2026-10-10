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
 *   stats_mode     "public" | "duel"
 *       public: se registra el mapa completo.
 *       duel:   solo cuenta lo jugado despues de "sv startcount" (al llegar la
 *               cuenta a 0). Las pausas (sv freeze) no suman tiempo; "sv
 *               resetcount" descarta lo registrado; si el mapa termina sin una
 *               cuenta iniciada, la partida no se guarda.
 *   stats_event    ""   nombre del torneo (ej. "copa-verano"). Si tiene valor, la
 *                       partida cuenta como oficial y se agrupa por torneo. En
 *                       duelos se toma el valor que tenga al terminar la cuenta.
 *
 * Cualquier error de E/S solo se informa por consola; nunca detiene el juego.
 */

#include "g_local.h"
#include "x_fire.h"
#include <errno.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#include <process.h>
#define stats_mkdir(p)	_mkdir(p)
#define stats_getpid()	_getpid()
#else
#include <unistd.h>
#define stats_mkdir(p)	mkdir((p), 0755)
#define stats_getpid()	getpid()
#endif

#define STATS_FORMAT_VERSION	1
#define STATS_NAME_LEN			32
#define STATS_PATH_LEN			256
#define STATS_NUM_CLASSES		(FLAMER + 1)
#define STATS_FLUSH_FRAMES		100		// fflush cada 10 segundos

#define STATS_WINNER_UNSET		99

// g_main.c
extern int		countdownActive;
extern qboolean	freeze_mode;


static cvar_t *stats_log;
static cvar_t *stats_server;
static cvar_t *stats_log_dir;
static cvar_t *stats_mode;
static cvar_t *stats_event;

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
	char		name[STATS_NAME_LEN];		// nombre registrado (unico entre los conectados)
	char		netname[STATS_NAME_LEN];	// nombre limpio del jugador, para detectar cambios
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
	int			streak;			// racha actual (mismas reglas que KillingSpree)
	int			best_streak;
	int			helmet_saves;	// el casco le desvio un tiro a la cabeza
	int			foot_saves;		// sobrevivio a un tiro con "almost lost a foot"
	int			deflected;		// tiros suyos que desvio el casco de otro
	int			zone_time;		// modo control: segundos dentro de la zona

	stats_counter_t	hits;
	stats_counter_t	misses;
	stats_counter_t	points;
	int				score;			// puede bajar (penalizaciones), se guarda el ultimo valor
	int				score_base;		// valor de resp.score al empezar a contar (duelos)
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
	qboolean	duel;			// stats_mode duel al iniciar el mapa
	qboolean	live;			// duel: la cuenta ya llego a 0
	int			live_seconds;	// duel: segundos jugados (sin pausas ni cuentas)
	stats_player_t	players[MAX_CLIENTS];
} sl;

// generador propio para el id de partida (no se toca el rand() del juego)
static unsigned int stats_random_state;

static unsigned int StatsLog_Random (void)
{
	unsigned int x = stats_random_state;

	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
	stats_random_state = x;
	return x;
}

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
	case MOD_SYRINGE:			return "syringe";
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

// en modo duelo solo cuenta lo jugado con la cuenta ya terminada y sin pausa
static qboolean StatsLog_Counting (void)
{
	if (!sl.duel)
		return true;
	return sl.live && !freeze_mode && !countdownActive;
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

	fprintf (sl.file, ",\"kills\":%d,\"deaths\":%d,\"suicides\":%d,\"tk\":%d,\"hs\":%d,\"objs\":%d,\"best_streak\":%d",
		p->kills, p->deaths, p->suicides, p->teamkills, p->headshots, p->objectives, p->best_streak);
	fprintf (sl.file, ",\"helmet_saves\":%d,\"foot_saves\":%d,\"deflected\":%d",
		p->helmet_saves, p->foot_saves, p->deflected);
	fprintf (sl.file, ",\"hits\":%d,\"misses\":%d,\"score\":%d,\"points\":%d",
		StatsLog_CounterGet (&p->hits), StatsLog_CounterGet (&p->misses),
		p->score, StatsLog_CounterGet (&p->points));
	// solo en mapas con zona de control, para no ensuciar los logs de otros modos
	if (level.control_zone)
		fprintf (sl.file, ",\"zone_time\":%d", p->zone_time);
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

	// no registrar a quien nunca entro a un equipo ni hizo nada, ni el
	// calentamiento de un duelo
	if ((!sl.duel || sl.live) &&
		(p->team_time[0] || p->team_time[1] || p->kills || p->deaths ||
		 p->helmet_saves || p->foot_saves))
		StatsLog_WritePlayer (p, reason);

	memset (p, 0, sizeof(*p));
}

// el nombre ya lo usa otro jugador registrado en esta partida
static qboolean StatsLog_NameTaken (const char *name, int except_slot)
{
	int i;

	for (i = 0; i < MAX_CLIENTS; i++)
	{
		if (i != except_slot && sl.players[i].active && !strcmp (sl.players[i].name, name))
			return true;
	}
	return false;
}

static stats_player_t *StatsLog_GetPlayer (edict_t *ent)
{
	int				slot, n;
	stats_player_t	*p;
	char			name[STATS_NAME_LEN];
	char			suffix[8];

	slot = StatsLog_SlotOf (ent);
	if (slot < 0)
		return NULL;

	p = &sl.players[slot];
	StatsLog_CleanName (ent->client->pers.netname, name, sizeof(name));

	// la identidad es el nombre: si cambia, se cierra el registro anterior
	if (p->active && strcmp (p->netname, name))
		StatsLog_FlushPlayer (slot, "rename");

	if (!p->active)
	{
		memset (p, 0, sizeof(*p));
		p->active = true;
		p->bot = ent->ai ? true : false;
		p->last_team = -2;
		StatsLog_Copy (p->netname, name, sizeof(p->netname));
		StatsLog_Copy (p->name, name, sizeof(p->name));

		// dos conectados con el mismo nombre: el segundo queda como "nombre (2)"
		for (n = 2; StatsLog_NameTaken (p->name, slot) && n < 100; n++)
		{
			Com_sprintf (suffix, sizeof(suffix), " (%d)", n);
			Com_sprintf (p->name, sizeof(p->name), "%.*s%s",
				(int)(sizeof(p->name) - 1 - strlen (suffix)), name, suffix);
		}
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
		p->streak = 0;		// el juego reinicia la racha al cambiar de equipo u observar
	}

	if (add_time && team >= 0 && StatsLog_Counting ())
	{
		p->team_time[team]++;
		p->class_time[StatsLog_ClassOf (ent)]++;
		if (Control_PlayerInZone (ent))
			p->zone_time++;
	}

	StatsLog_CounterSet (&p->hits, ent->client->resp.accuracy_hits);
	StatsLog_CounterSet (&p->misses, ent->client->resp.accuracy_misses);
	p->score = ent->client->resp.score - p->score_base;
	StatsLog_CounterSet (&p->points, ent->client->resp.points);
}

// descarta lo acumulado (por ejemplo el calentamiento de un duelo)
// manteniendo la identidad y el equipo de cada jugador
static void StatsLog_ResetCounters (void)
{
	int				i;
	edict_t			*ent;
	stats_player_t	*p, saved;

	for (i = 0; i < MAX_CLIENTS; i++)
	{
		p = &sl.players[i];
		saved = *p;

		memset (p, 0, sizeof(*p));
		p->active = saved.active;
		p->bot = saved.bot;
		p->last_team = saved.last_team;
		memcpy (p->name, saved.name, sizeof(p->name));
		memcpy (p->netname, saved.netname, sizeof(p->netname));

		// los contadores del juego no se reinician: se toma su valor actual
		// como punto de partida
		if (!p->active || i >= game.maxclients)
			continue;
		ent = g_edicts + 1 + i;
		if (!ent->inuse || !ent->client)
			continue;
		p->hits.base = -ent->client->resp.accuracy_hits;
		p->hits.last = ent->client->resp.accuracy_hits;
		p->misses.base = -ent->client->resp.accuracy_misses;
		p->misses.last = ent->client->resp.accuracy_misses;
		p->points.base = -ent->client->resp.points;
		p->points.last = ent->client->resp.points;
		p->score_base = ent->client->resp.score;
	}
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

// borra el archivo de una partida que no se debe guardar
static void StatsLog_Discard (void)
{
	if (sl.file)
	{
		fclose (sl.file);
		sl.file = NULL;
		if (remove (sl.part_path))
			gi.dprintf ("StatsLog: no se pudo borrar %s (%s)\n", sl.part_path, strerror (errno));
	}
	sl.open = false;
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
	stats_mode = gi.cvar ("stats_mode", "public", 0);
	stats_event = gi.cvar ("stats_event", "", 0);

	memset (&sl, 0, sizeof(sl));
	sl.winner = STATS_WINNER_UNSET;

	// distinto en cada arranque y en cada proceso
	stats_random_state = (unsigned int)time (NULL) * 2654435761u
		^ (unsigned int)stats_getpid () * 40503u
		^ (unsigned int)(size_t)&sl ^ (unsigned int)clock ();
	if (!stats_random_state)
		stats_random_state = 1;
}

// cierra una partida que no llego a la intermision (cambio de mapa por consola,
// cierre del servidor). Solo usa datos propios, porque level ya puede estar reiniciado.
static void StatsLog_Abort (void)
{
	int i;

	if (!sl.open)
		return;

	if (sl.duel && !sl.live)
	{
		StatsLog_Discard ();
		return;
	}

	for (i = 0; i < MAX_CLIENTS; i++)
		StatsLog_FlushPlayer (i, "aborted");

	fprintf (sl.file, "{\"ev\":\"match_end\",\"t\":%.1f,\"ts\":%ld,\"dur\":%.1f,\"winner\":null,\"reason\":\"aborted\",\"teams\":[]}\n",
		sl.last_time, (long)time (NULL), sl.duel ? (float)sl.live_seconds : sl.last_time);

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
	sl.duel = !Q_stricmp (stats_mode->string, "duel");
	sl.live = false;
	sl.live_seconds = 0;

	if (!stats_log->value || !deathmatch->value)
		return;

	now = time (NULL);
	tm = gmtime (&now);
	if (!tm || !strftime (stamp, sizeof(stamp), "%Y%m%d-%H%M%S", tm))
		StatsLog_Copy (stamp, "00000000-000000", sizeof(stamp));

	StatsLog_ServerId (server, sizeof(server));
	Com_sprintf (sl.match_id, sizeof(sl.match_id), "%s-%s-%08x", stamp, server, StatsLog_Random ());

	if (!StatsLog_OpenFile ())
		return;

	sl.open = true;

	if (level.campaign && *level.campaign)
		mode = "campaign";
	else if (ctb_mode->value)
		mode = "ctb";
	else if (level.control_zone)
		mode = "control";
	else if (G_IsFFA ())
		mode = "ffa";
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
	StatsLog_WriteKey ("kind");
	StatsLog_WriteString (sl.duel ? "duel" : "public");
	StatsLog_WriteKey ("event");
	StatsLog_WriteString (stats_event->string);
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
	{
		StatsLog_SampleAll (true);
		if (sl.duel && StatsLog_Counting ())
			sl.live_seconds++;
	}

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

	// duelo sin cuenta iniciada: no se jugo un duelo, no se guarda
	if (sl.duel && !sl.live)
	{
		StatsLog_Discard ();
		return;
	}

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
	fprintf (sl.file, ",\"ts\":%ld,\"dur\":%.1f", (long)time (NULL),
		sl.duel ? (float)sl.live_seconds : level.time);
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

// racha con las mismas reglas que KillingSpree() en p_client.c: usa el
// atacante directo (sin el credito por desangrado), suma una kill a un
// enemigo, el fuego amigo la corta y morir la reinicia
static void StatsLog_Streak (edict_t *targ, edict_t *attacker, stats_player_t *victim)
{
	stats_player_t	*a;
	int				ateam;

	if (attacker && attacker != targ && attacker->inuse && attacker->client &&
		(a = StatsLog_GetPlayer (attacker)) != NULL)
	{
		ateam = StatsLog_TeamOf (attacker);
		if (ateam >= 0 && ateam == StatsLog_TeamOf (targ))
			a->streak = 0;
		else if (++a->streak > a->best_streak)
			a->best_streak = a->streak;
	}

	victim->streak = 0;
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

	if (!sl.open || level.intermissiontime || !targ || !targ->client || !StatsLog_Counting ())
		return;

	mod = meansOfDeath & ~MOD_FRIENDLY_FIRE;

	// cambiar de equipo no es una muerte real, pero corta la racha
	if (mod == MOD_CHANGETEAM || mod == MOD_CHANGETEAM_WOUNDED)
	{
		victim = StatsLog_GetPlayer (targ);
		if (victim)
			victim->streak = 0;
		return;
	}

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

	StatsLog_Streak (targ, attacker, victim);

	if (suicide)
		victim->suicides++;
	else
	{
		kteam = StatsLog_TeamOf (killer);
		// Free For All: no hay companeros, ninguna kill es teamkill
		ff = (!G_IsFFA () && kteam >= 0 && kteam == vteam);
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

	if (!sl.open || level.intermissiontime || !StatsLog_Counting ())
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

/*
=================
Duelos (stats_mode duel)
=================
*/

// la cuenta de "sv startcount" llego a 0. Tambien se llama al reanudar
// despues de una pausa (sv freeze): en ese caso solo se sigue sumando.
void StatsLog_CountdownDone (void)
{
	if (!sl.open || !sl.duel || level.intermissiontime)
		return;

	if (sl.live)
	{
		StatsLog_BeginEvent ("resume");
		StatsLog_EndEvent ();
		return;
	}

	// lo anterior era calentamiento
	StatsLog_ResetCounters ();
	sl.live = true;
	sl.live_seconds = 0;
	StatsLog_BeginEvent ("live");
	StatsLog_WriteKey ("event");
	StatsLog_WriteString (stats_event->string);
	StatsLog_EndEvent ();
	fflush (sl.file);
}

// "sv resetcount": se cancela el duelo en curso y se descarta lo registrado
void StatsLog_CountdownReset (void)
{
	if (!sl.open || !sl.duel || !sl.live)
		return;

	StatsLog_ResetCounters ();
	sl.live = false;
	sl.live_seconds = 0;
	StatsLog_BeginEvent ("live_cancel");
	StatsLog_EndEvent ();
}

/*
=================
Golpes de suerte
=================
*/

// llamado desde T_Damage() cuando el casco desvia un tiro a la cabeza o cuando
// un tiro de rifle deja al jugador desangrandose en vez de matarlo
void StatsLog_Luck (edict_t *targ, edict_t *attacker, int mod, const char *type)
{
	stats_player_t	*p, *a = NULL;
	int				team, ateam = -1;

	if (!sl.open || level.intermissiontime || !targ || !targ->client || targ->deadflag ||
		!StatsLog_Counting ())
		return;

	p = StatsLog_GetPlayer (targ);
	if (!p)
		return;
	team = StatsLog_TeamOf (targ);

	if (!strcmp (type, STATS_LUCK_HELMET))
		p->helmet_saves++;
	else
		p->foot_saves++;

	if (attacker && attacker != targ && attacker->inuse && attacker->client)
	{
		a = StatsLog_GetPlayer (attacker);
		if (a)
		{
			ateam = StatsLog_TeamOf (attacker);
			if (!strcmp (type, STATS_LUCK_HELMET))
				a->deflected++;
		}
	}

	StatsLog_BeginEvent ("luck");
	StatsLog_WriteKey ("type");
	StatsLog_WriteString (type);
	StatsLog_WriteActor ("player", targ, p, team);
	if (a)
		StatsLog_WriteActor ("by", attacker, a, ateam);
	else
		fputs (",\"by\":null", sl.file);
	fprintf (sl.file, ",\"mod\":\"%s\"", StatsLog_ModName (mod & ~MOD_FRIENDLY_FIRE));
	StatsLog_EndEvent ();
}
