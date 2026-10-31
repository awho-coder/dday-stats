/*
 * g_vote.c -- votacion de los jugadores durante la partida.
 *
 * No es la votacion del final del mapa (mapvoting): esta la abre un jugador
 * cuando quiere, con la misma sintaxis que usaba q2admin en los servers viejos:
 *
 *   vote map <mapa>     propone cambiar a ese mapa (el que propone vota si)
 *   vote restart        propone reiniciar el mapa actual
 *   vote kick <jugador> propone echar a un jugador (nombre o numero)
 *   vote yes | no       vota la propuesta en curso (tambien "si")
 *   vote maps [texto]   busca entre los mapas del servidor
 *   vote players        numero y nombre de cada jugador (para vote kick)
 *   vote                muestra la votacion en curso o la ayuda
 *
 * Votan solo los jugadores humanos que estan en un equipo (no espectadores ni
 * bots). Se aprueba cuando los "si" superan el porcentaje de esos jugadores; si
 * se acaba el tiempo sin llegar, se rechaza (no votar cuenta como no).
 *
 * Cvars:
 *   vote_enable        0    1 = los jugadores pueden abrir votaciones
 *   vote_time          30   segundos que dura una votacion
 *   vote_percent       50   porcentaje de "si" que hay que superar (mapa)
 *   vote_cooldown      60   segundos de espera tras una votacion rechazada
 *   vote_kick          1    0 = no se puede votar kick (con vote_enable 1)
 *   vote_kick_percent  75   porcentaje de "si" que hay que superar (kick)
 *   vote_kick_min      3    humanos en equipo necesarios para un kick
 *
 * Mapas: cualquier .bsp de dday/maps que sea de D-Day (tiene spawns de equipo
 * en el .bsp o en ents/<mapa>.ent). Sin distinguir mayusculas.
 * Kick: el acusado no vota ni cuenta; no se puede echar a un admin (nick
 * protegido, ver g_nicks.c) ni a bots; sin ban, puede volver a entrar.
 * No se vota en torneo, duelo (stats_mode duel), campana ni intermision.
 * El admin puede cortar una votacion con "sv votecancel".
 */

#include "g_local.h"
#include <ctype.h>
#include <stdarg.h>

#ifdef _WIN32
#include <io.h>
#else
#include <dirent.h>
#endif

#define VOTE_NONE	0
#define VOTE_YES	1
#define VOTE_NO		2

#define VTYPE_MAP		0
#define VTYPE_RESTART	1
#define VTYPE_KICK		2

#define VOTE_REMIND		10		// cada cuantos segundos se recuerda la votacion
#define KICK_WAIT		120		// espera tras un kick rechazado (quien propuso) y entre kicks al mismo
#define VOTE_MAX_MAPS	2048
#define VOTE_SUGGEST	5

#ifndef MAX_OSPATH
#define MAX_OSPATH		256
#endif

extern qboolean disable_mapvoting;

// strcat que no se pasa del tamano del destino
static void Vote_Cat (char *dst, const char *src, int size)
{
	int len = strlen (dst);

	if (len < size - 1)
		Com_sprintf (dst + len, size - len, "%s", src);
}

static cvar_t *vote_enable;
static cvar_t *vote_time;
static cvar_t *vote_percent;
static cvar_t *vote_cooldown;
static cvar_t *vote_kick;
static cvar_t *vote_kick_percent;
static cvar_t *vote_kick_min;

static struct
{
	qboolean	active;
	int			type;
	char		map[MAX_QPATH];		// mapa propuesto (el actual si es restart)
	int			target;				// slot del acusado (kick)
	char		target_name[32];
	char		caller[32];
	int			caller_slot;
	float		end_time;
	float		next_remind;
	float		next_allowed;		// level.time desde el que se puede abrir otra
	byte		choice[MAX_CLIENTS];	// por slot de cliente
	float		kick_caller_wait[MAX_CLIENTS];	// no puede proponer kick hasta
	float		kick_target_wait[MAX_CLIENTS];	// no se le puede votar kick hasta
} vote;

// mapas del servidor (se leen una vez por mapa, al primer uso)
static char	vote_maps[VOTE_MAX_MAPS][MAX_QPATH];
static int	vote_nummaps = -1;

void Vote_Init (void)
{
	vote_enable       = gi.cvar ("vote_enable", "0", 0);
	vote_time         = gi.cvar ("vote_time", "30", 0);
	vote_percent      = gi.cvar ("vote_percent", "50", 0);
	vote_cooldown     = gi.cvar ("vote_cooldown", "60", 0);
	vote_kick         = gi.cvar ("vote_kick", "1", 0);
	vote_kick_percent = gi.cvar ("vote_kick_percent", "75", 0);
	vote_kick_min     = gi.cvar ("vote_kick_min", "3", 0);
}

// al iniciar cada mapa (level se borra, pero esto es static)
void Vote_Reset (void)
{
	memset (&vote, 0, sizeof(vote));
	vote_nummaps = -1;	// por si subieron mapas nuevos
}

/*
==============================================================================
Mapas del servidor
==============================================================================
*/

static int Vote_CompareMaps (const void *a, const void *b)
{
	return Q_stricmp ((char *)a, (char *)b);
}

static void Vote_AddMap (const char *file)
{
	char	name[MAX_QPATH];
	int		len = strlen (file), i;

	if (len <= 4 || len - 4 >= MAX_QPATH || Q_stricmp ((char *)file + len - 4, ".bsp"))
		return;
	memcpy (name, file, len - 4);
	name[len - 4] = '\0';

	for (i = 0; i < vote_nummaps; i++)
		if (!Q_stricmp (vote_maps[i], name))
			return;		// ya esta (homedir y basedir)
	if (vote_nummaps < VOTE_MAX_MAPS)
		strcpy (vote_maps[vote_nummaps++], name);
}

static void Vote_ScanDir (const char *base)
{
	char path[MAX_OSPATH];

	if (!base || !base[0])
		return;
	Com_sprintf (path, sizeof(path), "%s/%s/maps", base, GAMEVERSION);

#ifdef _WIN32
	{
		struct _finddata_t	fd;
		intptr_t			h;
		char				pattern[MAX_OSPATH];

		Com_sprintf (pattern, sizeof(pattern), "%s/*.bsp", path);
		h = _findfirst (pattern, &fd);
		if (h == -1)
			return;
		do
			Vote_AddMap (fd.name);
		while (_findnext (h, &fd) == 0);
		_findclose (h);
	}
#else
	{
		DIR				*d = opendir (path);
		struct dirent	*e;

		if (!d)
			return;
		while ((e = readdir (d)) != NULL)
			Vote_AddMap (e->d_name);
		closedir (d);
	}
#endif
}

static void Vote_LoadMaps (void)
{
	if (vote_nummaps >= 0)
		return;
	vote_nummaps = 0;
	Vote_ScanDir (sys_homedir->string);
	if (Q_stricmp (sys_homedir->string, sys_basedir->string))
		Vote_ScanDir (sys_basedir->string);
	qsort (vote_maps, vote_nummaps, sizeof(vote_maps[0]), Vote_CompareMaps);
}

// indice del mapa (sin distinguir mayusculas), -1 si no esta
static int Vote_FindMap (const char *name)
{
	int i;

	Vote_LoadMaps ();
	for (i = 0; i < vote_nummaps; i++)
		if (!Q_stricmp (vote_maps[i], (char *)name))
			return i;
	return -1;
}

// strstr sin distinguir mayusculas
static qboolean Vote_Contains (const char *s, const char *sub)
{
	int i, n = strlen (sub);

	if (!n)
		return true;
	for (; *s; s++)
	{
		for (i = 0; i < n; i++)
			if (tolower ((unsigned char)s[i]) != tolower ((unsigned char)sub[i]))
				break;
		if (i == n)
			return true;
	}
	return false;
}

// distancia de edicion (nombres cortos), sin distinguir mayusculas
static int Vote_Distance (const char *a, const char *b)
{
	int		la = strlen (a), lb = strlen (b), i, j, cost, best;
	int		row[MAX_QPATH + 1], prev, tmp;

	if (la > MAX_QPATH || lb > MAX_QPATH)
		return 99;
	for (j = 0; j <= lb; j++)
		row[j] = j;
	for (i = 1; i <= la; i++)
	{
		prev = row[0];
		row[0] = i;
		for (j = 1; j <= lb; j++)
		{
			cost = tolower ((unsigned char)a[i-1]) != tolower ((unsigned char)b[j-1]);
			best = prev + cost;
			if (row[j] + 1 < best)
				best = row[j] + 1;
			if (row[j-1] + 1 < best)
				best = row[j-1] + 1;
			tmp = row[j];
			row[j] = best;
			prev = tmp;
		}
	}
	return row[lb];
}

// hasta VOTE_SUGGEST mapas parecidos: primero los que contienen el texto, despues por distancia
static void Vote_Suggest (const char *query, char *out, int size)
{
	int		picked[VOTE_SUGGEST], n = 0, i, j, d, maxd;

	out[0] = '\0';
	Vote_LoadMaps ();

	for (i = 0; i < vote_nummaps && n < VOTE_SUGGEST; i++)
		if (Vote_Contains (vote_maps[i], query) || Vote_Contains (query, vote_maps[i]))
			picked[n++] = i;

	maxd = strlen (query) / 3 + 1;
	if (maxd > 3)
		maxd = 3;
	for (d = 1; d <= maxd && n < VOTE_SUGGEST; d++)
		for (i = 0; i < vote_nummaps && n < VOTE_SUGGEST; i++)
		{
			for (j = 0; j < n; j++)
				if (picked[j] == i)
					break;
			if (j == n && Vote_Distance (vote_maps[i], query) == d)
				picked[n++] = i;
		}

	for (i = 0; i < n; i++)
	{
		Vote_Cat (out, i ? ", " : "", size);
		Vote_Cat (out, vote_maps[picked[i]], size);
	}
}

#define MAP_OK			0
#define MAP_NOT_DDAY	1
#define MAP_BROKEN		2

static qboolean Vote_HasTeamSpawns (const char *ents)
{
	return Vote_Contains (ents, "info_team_start") || Vote_Contains (ents, "info_reinforcements_start");
}

// el mapa es de D-Day: spawns de equipo en el .bsp o en ents/<mapa>.ent
static int Vote_CheckMap (const char *map)
{
	char	path[MAX_OSPATH], *ents;
	FILE	*f = NULL;
	byte	header[16];
	int		ofs, len, result = MAP_BROKEN;

	ents = ReadEntFile (va ("ents/%s.ent", map));
	if (ents)
	{
		qboolean ok = Vote_HasTeamSpawns (ents);
		gi.TagFree (ents);
		if (ok)
			return MAP_OK;
	}

	Com_sprintf (path, sizeof(path), "%s/%s/maps/%s.bsp", sys_homedir->string, GAMEVERSION, map);
	f = fopen (path, "rb");
	if (!f)
	{
		Com_sprintf (path, sizeof(path), "%s/%s/maps/%s.bsp", sys_basedir->string, GAMEVERSION, map);
		f = fopen (path, "rb");
	}
	if (!f)
		return MAP_BROKEN;

	// cabecera IBSP: magic, version, y el lump 0 (entidades) = offset, largo
	if (fread (header, 1, sizeof(header), f) == sizeof(header) && !memcmp (header, "IBSP", 4))
	{
		ofs = header[8] | (header[9] << 8) | (header[10] << 16) | (header[11] << 24);
		len = header[12] | (header[13] << 8) | (header[14] << 16) | (header[15] << 24);
		if (ofs > 0 && len > 0 && len < 8 * 1024 * 1024 && !fseek (f, ofs, SEEK_SET))
		{
			char *buf = gi.TagMalloc (len + 1, TAG_LEVEL);

			if (fread (buf, 1, len, f) == (size_t)len)
			{
				buf[len] = '\0';
				result = Vote_HasTeamSpawns (buf) ? MAP_OK : MAP_NOT_DDAY;
			}
			gi.TagFree (buf);
		}
	}
	fclose (f);
	return result;
}

static void Vote_PrintMaps (edict_t *ent, const char *filter)
{
	char	line[1024];
	int		i, shown = 0, total = 0;

	Vote_LoadMaps ();
	if (!filter[0])
	{
		safe_cprintf (ent, PRINT_HIGH, "Hay %d mapas en el servidor. Busca con: vote maps <texto> (ej: vote maps dday)\n",
			vote_nummaps);
		return;
	}

	line[0] = '\0';
	for (i = 0; i < vote_nummaps; i++)
	{
		if (!Vote_Contains (vote_maps[i], filter))
			continue;
		total++;
		if (shown < 60 && strlen (line) + strlen (vote_maps[i]) + 3 < sizeof(line))
		{
			Vote_Cat (line, shown ? ", " : "", sizeof(line));
			Vote_Cat (line, vote_maps[i], sizeof(line));
			shown++;
		}
	}
	if (!total)
	{
		Vote_Suggest (filter, line, sizeof(line));
		safe_cprintf (ent, PRINT_HIGH, "Ningun mapa contiene \"%s\".%s%s\n", filter,
			line[0] ? " Parecidos: " : "", line);
		return;
	}
	safe_cprintf (ent, PRINT_HIGH, "%d mapas con \"%s\": %s%s\n", total, filter, line,
		total > shown ? " ..." : "");
}

/*
==============================================================================
Votacion
==============================================================================
*/

// jugador que puede votar: humano y en un equipo
static qboolean Vote_CanVote (edict_t *ent)
{
	return ent->inuse && ent->client && ent->client->resp.team_on && !ent->ai;
}

static void Vote_Count (int *yes, int *no, int *total)
{
	int		i;
	edict_t	*ent;

	*yes = *no = *total = 0;
	for (i = 0; i < game.maxclients; i++)
	{
		ent = g_edicts + 1 + i;
		if (!Vote_CanVote (ent))
			continue;
		if (vote.type == VTYPE_KICK && i == vote.target)
			continue;	// el acusado no cuenta
		(*total)++;
		if (vote.choice[i] == VOTE_YES)
			(*yes)++;
		else if (vote.choice[i] == VOTE_NO)
			(*no)++;
	}
}

// "si" necesarios para superar el porcentaje de total jugadores
static int Vote_Needed (int total)
{
	int pct = (int)(vote.type == VTYPE_KICK ? vote_kick_percent->value : vote_percent->value);

	if (pct < 0)
		pct = 0;
	if (pct > 99)
		pct = 99;
	return (total * pct) / 100 + 1;
}

static const char *Vote_What (void)
{
	static char buf[MAX_QPATH + 32];

	if (vote.type == VTYPE_RESTART)
		Com_sprintf (buf, sizeof(buf), "reiniciar %s", vote.map);
	else if (vote.type == VTYPE_KICK)
		Com_sprintf (buf, sizeof(buf), "echar a %s", vote.target_name);
	else
		Com_sprintf (buf, sizeof(buf), "cambiar a %s", vote.map);
	return buf;
}

// por que no se puede votar ahora (NULL = se puede)
static const char *Vote_Blocked (void)
{
	cvar_t *stats_mode = gi.cvar ("stats_mode", "public", 0);

	if (!vote_enable->value)
		return "La votacion esta desactivada en este servidor.";
	if (level.intermissiontime)
		return "No se puede votar durante la intermision.";
	if (tournament->value)
		return "No se puede votar durante un torneo.";
	if (!Q_stricmp (stats_mode->string, "duel"))
		return "No se puede votar durante un duelo.";
	if (level.campaign)
		return "No se puede votar en una campana.";
	return NULL;
}

// mensaje al medio de la pantalla de cada humano (como q2admin). En q2pro un centerprint borra
// las kills de arriba, por eso solo se usa al abrir y al cerrar la votacion, no en los recordatorios
static void Vote_Center (const char *fmt, ...)
{
	char	msg[256];
	va_list	ap;
	int		i;
	edict_t	*ent;

	va_start (ap, fmt);
	vsnprintf (msg, sizeof(msg), fmt, ap);
	va_end (ap);

	for (i = 1; i <= game.maxclients; i++)
	{
		ent = g_edicts + i;
		if (ent->inuse && ent->client && !ent->ai)
			safe_centerprintf (ent, "%s", msg);
	}
}

// quienes votaron si (para el log de los kicks)
static void Vote_LogYes (void)
{
	char	line[512] = "";
	int		i;

	for (i = 0; i < game.maxclients; i++)
		if (vote.choice[i] == VOTE_YES && g_edicts[i + 1].inuse && g_edicts[i + 1].client)
		{
			Vote_Cat (line, line[0] ? ", " : "", sizeof(line));
			Vote_Cat (line, g_edicts[i + 1].client->pers.netname, sizeof(line));
		}
	gi.dprintf ("vote: votaron si: %s\n", line);
}

static void Vote_End (qboolean passed, const char *why)
{
	vote.active = false;

	if (vote.type == VTYPE_KICK)
	{
		vote.kick_target_wait[vote.target] = level.time + KICK_WAIT;
		if (!passed)
			vote.kick_caller_wait[vote.caller_slot] = level.time + KICK_WAIT;
	}

	if (!passed)
	{
		safe_bprintf (PRINT_HIGH, "Votacion rechazada (%s): %s.\n", why, Vote_What ());
		Vote_Center ("Votacion RECHAZADA\n\n%s\n(%s)", Vote_What (), why);
		gi.dprintf ("vote: rechazado %s (propuso %s): %s\n", Vote_What (), vote.caller, why);
		vote.next_allowed = level.time + vote_cooldown->value;
		return;
	}

	safe_bprintf (PRINT_HIGH, "Votacion aprobada: %s.\n", Vote_What ());
	Vote_Center ("Votacion APROBADA\n\n%s", Vote_What ());
	gi.dprintf ("vote: aprobado %s (propuso %s)\n", Vote_What (), vote.caller);

	if (vote.type == VTYPE_KICK)
	{
		Vote_LogYes ();
		gi.AddCommandString (va ("kick %d\n", vote.target));
		return;
	}

	// igual que "sv nextmap": la partida queda como cortada y sin votacion final
	disable_mapvoting = true;
	StatsLog_MarkForcedEnd ();
	BeginIntermission (CreateTargetChangeLevel (vote.map));
}

// revisa si la votacion ya se decidio
static void Vote_Check (void)
{
	int yes, no, total, needed;

	if (!vote.active)
		return;

	Vote_Count (&yes, &no, &total);
	if (!total)
	{
		Vote_End (false, "no quedan jugadores");
		return;
	}

	needed = Vote_Needed (total);
	if (yes >= needed)
		Vote_End (true, NULL);
	else if (total - no < needed)
		Vote_End (false, "no alcanzan los votos");
}

static void Vote_Status (edict_t *ent)
{
	int yes, no, total;

	Vote_Count (&yes, &no, &total);
	safe_cprintf (ent, PRINT_HIGH, "Votacion de %s: %s. Si %d, No %d, faltan %d si (%d s).\n",
		vote.caller, Vote_What (), yes, no, Vote_Needed (total) - yes,
		(int)(vote.end_time - level.time + 0.9));
}

static void Vote_Help (edict_t *ent)
{
	safe_cprintf (ent, PRINT_HIGH,
		"Uso: vote map <mapa> | vote restart | vote kick <jugador> | vote yes | vote no\n"
		"\"vote maps <texto>\" busca mapas; \"vote players\" muestra los numeros de los jugadores.\n");
}

static void Vote_PrintPlayers (edict_t *ent)
{
	int		i;
	edict_t	*e;

	for (i = 0; i < game.maxclients; i++)
	{
		e = g_edicts + 1 + i;
		if (!e->inuse || !e->client || e->ai)
			continue;
		safe_cprintf (ent, PRINT_HIGH, "%3d  %s%s\n", i, e->client->pers.netname,
			e->client->resp.team_on ? "" : " (espectador)");
	}
}

static void Vote_Start (edict_t *ent, int type)
{
	vote.active = true;
	vote.type = type;
	vote.caller_slot = ent - g_edicts - 1;
	Com_sprintf (vote.caller, sizeof(vote.caller), "%s", ent->client->pers.netname);
	memset (vote.choice, 0, sizeof(vote.choice));
	vote.choice[ent - g_edicts - 1] = VOTE_YES;
	vote.end_time = level.time + (vote_time->value > 5 ? vote_time->value : 5);
	vote.next_remind = level.time + VOTE_REMIND;

	safe_bprintf (PRINT_HIGH, "%s propone %s. Escribe \"vote yes\" o \"vote no\" en la consola.\n",
		vote.caller, Vote_What ());
	gi.dprintf ("vote: %s propone %s\n", vote.caller, Vote_What ());
	Vote_Center ("VOTACION\n\n%s propone\n%s\n\nvote yes  o  vote no\nen la consola (%d s)",
		vote.caller, Vote_What (), (int)(vote.end_time - level.time + 0.5));

	Vote_Check ();	// con un solo jugador se aprueba al tiro
}

// jugador por numero de slot o por nombre (exacto, o parte unica del nombre); -1 si no
static int Vote_FindPlayer (edict_t *ent, const char *who)
{
	int			i, found = -1, matches = 0;
	edict_t		*e;
	const char	*p;

	for (p = who; *p && isdigit ((unsigned char)*p); p++)
		;
	if (!*p)
	{
		i = atoi (who);
		if (i >= 0 && i < game.maxclients && g_edicts[i + 1].inuse && g_edicts[i + 1].client)
			return i;
		safe_cprintf (ent, PRINT_HIGH, "No hay ningun jugador con el numero %s. Mira \"vote players\".\n", who);
		return -1;
	}

	for (i = 0; i < game.maxclients; i++)
	{
		e = g_edicts + 1 + i;
		if (e->inuse && e->client && !Q_stricmp (e->client->pers.netname, (char *)who))
			return i;
	}
	for (i = 0; i < game.maxclients; i++)
	{
		e = g_edicts + 1 + i;
		if (e->inuse && e->client && Vote_Contains (e->client->pers.netname, who))
		{
			found = i;
			matches++;
		}
	}
	if (matches == 1)
		return found;
	if (matches > 1)
		safe_cprintf (ent, PRINT_HIGH, "Hay varios jugadores con \"%s\". Usa el numero (\"vote players\").\n", who);
	else
		safe_cprintf (ent, PRINT_HIGH, "No hay ningun jugador \"%s\". Mira \"vote players\".\n", who);
	return -1;
}

static void Vote_StartKick (edict_t *ent, const char *who)
{
	int		slot = ent - g_edicts - 1, target, humans;
	edict_t	*t;

	if (!vote_kick->value)
	{
		safe_cprintf (ent, PRINT_HIGH, "La votacion de kick esta desactivada en este servidor.\n");
		return;
	}
	if (!who[0])
	{
		safe_cprintf (ent, PRINT_HIGH, "Uso: vote kick <nombre o numero>. \"vote players\" muestra los numeros.\n");
		return;
	}
	humans = HumanPlayerCount ();
	if (humans < (int)vote_kick_min->value)
	{
		safe_cprintf (ent, PRINT_HIGH, "Para votar un kick hacen falta %d jugadores en equipo (hay %d).\n",
			(int)vote_kick_min->value, humans);
		return;
	}
	if (level.time < vote.kick_caller_wait[slot])
	{
		safe_cprintf (ent, PRINT_HIGH, "Espera %d s para proponer otro kick.\n",
			(int)(vote.kick_caller_wait[slot] - level.time + 0.9));
		return;
	}

	target = Vote_FindPlayer (ent, who);
	if (target < 0)
		return;
	t = g_edicts + 1 + target;
	if (target == slot)
	{
		safe_cprintf (ent, PRINT_HIGH, "No puedes votar para echarte a ti mismo.\n");
		return;
	}
	if (t->ai)
	{
		safe_cprintf (ent, PRINT_HIGH, "No se puede votar para echar a un bot.\n");
		return;
	}
	if (Nick_IsProtected (t))
	{
		safe_cprintf (ent, PRINT_HIGH, "No se puede votar para echar a un admin.\n");
		return;
	}
	if (level.time < vote.kick_target_wait[target])
	{
		safe_cprintf (ent, PRINT_HIGH, "A %s ya lo votaron hace poco. Espera %d s.\n", t->client->pers.netname,
			(int)(vote.kick_target_wait[target] - level.time + 0.9));
		return;
	}

	vote.target = target;
	Com_sprintf (vote.target_name, sizeof(vote.target_name), "%s", t->client->pers.netname);
	Vote_Start (ent, VTYPE_KICK);
}

static void Vote_StartMap (edict_t *ent, const char *arg)
{
	char	sug[256];
	int		idx;

	if (!arg[0])
	{
		safe_cprintf (ent, PRINT_HIGH, "Uso: vote map <mapa>. \"vote maps <texto>\" busca mapas.\n");
		return;
	}
	if (strlen (arg) >= MAX_QPATH || strchr (arg, '/') || strchr (arg, '\\') || strchr (arg, '.')
		|| strchr (arg, '"') || strchr (arg, ';'))
	{
		safe_cprintf (ent, PRINT_HIGH, "Nombre de mapa no valido.\n");
		return;
	}

	idx = Vote_FindMap (arg);
	if (idx < 0)
	{
		Vote_Suggest (arg, sug, sizeof(sug));
		if (sug[0])
			safe_cprintf (ent, PRINT_HIGH, "El mapa \"%s\" no esta en el servidor. Quisiste decir: %s?\n", arg, sug);
		else
			safe_cprintf (ent, PRINT_HIGH, "El mapa \"%s\" no esta en el servidor. \"vote maps <texto>\" busca mapas.\n", arg);
		return;
	}

	switch (Vote_CheckMap (vote_maps[idx]))
	{
	case MAP_NOT_DDAY:
		safe_cprintf (ent, PRINT_HIGH, "\"%s\" esta en el servidor pero no es un mapa de D-Day.\n", vote_maps[idx]);
		return;
	case MAP_BROKEN:
		safe_cprintf (ent, PRINT_HIGH, "El archivo de \"%s\" esta danado, no se puede votar.\n", vote_maps[idx]);
		return;
	}

	Com_sprintf (vote.map, sizeof(vote.map), "%s", vote_maps[idx]);	// nombre real del archivo
	Vote_Start (ent, VTYPE_MAP);
}

void Cmd_Vote_f (edict_t *ent)
{
	char		*arg = gi.argv (1);
	const char	*why;
	int			slot = ent - g_edicts - 1;

	if (!arg[0])
	{
		if (vote.active)
			Vote_Status (ent);
		else
			Vote_Help (ent);
		return;
	}

	if (!Q_stricmp (arg, "maps") || !Q_stricmp (arg, "mapas"))
	{
		Vote_PrintMaps (ent, gi.argv (2));
		return;
	}
	if (!Q_stricmp (arg, "players") || !Q_stricmp (arg, "jugadores"))
	{
		Vote_PrintPlayers (ent);
		return;
	}

	if (!Q_stricmp (arg, "yes") || !Q_stricmp (arg, "si") || !Q_stricmp (arg, "no"))
	{
		if (!vote.active)
		{
			safe_cprintf (ent, PRINT_HIGH, "No hay ninguna votacion en curso.\n");
			return;
		}
		if (!Vote_CanVote (ent))
		{
			safe_cprintf (ent, PRINT_HIGH, "Solo votan los jugadores que estan en un equipo.\n");
			return;
		}
		if (vote.type == VTYPE_KICK && slot == vote.target)
		{
			safe_cprintf (ent, PRINT_HIGH, "No puedes votar en tu propio kick.\n");
			return;
		}
		vote.choice[slot] = Q_stricmp (arg, "no") ? VOTE_YES : VOTE_NO;
		safe_cprintf (ent, PRINT_HIGH, "Votaste %s.\n", vote.choice[slot] == VOTE_YES ? "si" : "no");
		Vote_Check ();
		if (vote.active)
			Vote_Status (ent);
		return;
	}

	if (Q_stricmp (arg, "map") && Q_stricmp (arg, "restart") && Q_stricmp (arg, "kick"))
	{
		Vote_Help (ent);
		return;
	}

	// abrir una votacion nueva
	if ((why = Vote_Blocked ()) != NULL)
	{
		safe_cprintf (ent, PRINT_HIGH, "%s\n", why);
		return;
	}
	if (vote.active)
	{
		Vote_Status (ent);
		return;
	}
	if (!Vote_CanVote (ent))
	{
		safe_cprintf (ent, PRINT_HIGH, "Tienes que estar en un equipo para proponer una votacion.\n");
		return;
	}
	if (level.time < vote.next_allowed)
	{
		safe_cprintf (ent, PRINT_HIGH, "Espera %d s para proponer otra votacion.\n",
			(int)(vote.next_allowed - level.time + 0.9));
		return;
	}

	if (!Q_stricmp (arg, "restart"))
	{
		Com_sprintf (vote.map, sizeof(vote.map), "%s", level.mapname);
		Vote_Start (ent, VTYPE_RESTART);
	}
	else if (!Q_stricmp (arg, "kick"))
	{
		char	who[64] = "";
		int		i;

		// el nombre puede tener espacios: se juntan los argumentos que siguen
		for (i = 2; i < gi.argc (); i++)
		{
			Vote_Cat (who, i > 2 ? " " : "", sizeof(who));
			Vote_Cat (who, gi.argv (i), sizeof(who));
		}
		Vote_StartKick (ent, who);
	}
	else
		Vote_StartMap (ent, gi.argv (2));
}

// cada frame desde G_RunFrame
void Vote_RunFrame (void)
{
	int yes, no, total;

	if (!vote.active)
		return;

	if (level.intermissiontime)
	{
		vote.active = false;	// el mapa termino solo
		return;
	}
	if (!vote_enable->value)
	{
		Vote_End (false, "votacion desactivada");
		return;
	}
	// alguien se fue o paso a espectador: la cuenta puede haber cambiado
	Vote_Check ();
	if (!vote.active)
		return;
	if (level.time >= vote.end_time)
	{
		Vote_End (false, "se acabo el tiempo");
		return;
	}
	if (level.time >= vote.next_remind)
	{
		vote.next_remind = level.time + VOTE_REMIND;
		Vote_Count (&yes, &no, &total);
		safe_bprintf (PRINT_HIGH, "Votacion: %s. Si %d, No %d, faltan %d si (%d s). \"vote yes\" o \"vote no\".\n",
			Vote_What (), yes, no, Vote_Needed (total) - yes, (int)(vote.end_time - level.time + 0.9));
	}
}

// el que se va deja de contar; el proximo que use ese slot empieza sin voto
void Vote_ClientDisconnect (edict_t *ent)
{
	int slot = ent - g_edicts - 1;

	if (slot < 0 || slot >= MAX_CLIENTS)
		return;
	vote.choice[slot] = VOTE_NONE;
	vote.kick_caller_wait[slot] = 0;
	vote.kick_target_wait[slot] = 0;

	if (vote.active && vote.type == VTYPE_KICK && slot == vote.target)
	{
		vote.active = false;
		safe_bprintf (PRINT_HIGH, "Votacion cancelada: %s se fue del servidor.\n", vote.target_name);
	}
}

// sv votecancel
void Svcmd_VoteCancel_f (void)
{
	if (!vote.active)
	{
		safe_cprintf (NULL, PRINT_HIGH, "No hay ninguna votacion en curso.\n");
		return;
	}
	vote.active = false;
	safe_bprintf (PRINT_HIGH, "El admin cancelo la votacion: %s.\n", Vote_What ());
}
