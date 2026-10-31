/*
 * g_vote.c -- votacion de los jugadores durante la partida.
 *
 * No es la votacion del final del mapa (mapvoting): esta la abre un jugador
 * cuando quiere, con la misma sintaxis que usaba q2admin en los servers viejos:
 *
 *   vote map <mapa>   propone cambiar a ese mapa (el que propone vota si)
 *   vote restart      propone reiniciar el mapa actual
 *   vote yes | no     vota la propuesta en curso (tambien "si")
 *   vote              muestra la votacion en curso o la ayuda
 *
 * Votan solo los jugadores humanos que estan en un equipo (no espectadores ni
 * bots). Se aprueba cuando los "si" superan vote_percent de esos jugadores; si
 * se acaba el tiempo sin llegar, se rechaza (no votar cuenta como no).
 *
 * Cvars:
 *   vote_enable    0    1 = los jugadores pueden abrir votaciones
 *   vote_time      30   segundos que dura una votacion
 *   vote_percent   50   porcentaje de "si" que hay que superar
 *   vote_cooldown  60   segundos de espera tras una votacion rechazada
 *
 * Mapas que se pueden votar: los de sv_maplist y los de votemaps.txt. Si las
 * dos listas estan vacias, cualquier mapa que exista en el server.
 * No se vota en torneo, duelo (stats_mode duel), campana ni intermision.
 * El admin puede cortar una votacion con "sv votecancel".
 */

#include "g_local.h"

#define VOTE_NONE	0
#define VOTE_YES	1
#define VOTE_NO		2

#define VOTE_REMIND	10		// cada cuantos segundos se recuerda la votacion

extern qboolean disable_mapvoting;

static cvar_t *vote_enable;
static cvar_t *vote_time;
static cvar_t *vote_percent;
static cvar_t *vote_cooldown;

static struct
{
	qboolean	active;
	char		map[MAX_QPATH];		// mapa propuesto (el actual si es restart)
	qboolean	restart;
	char		caller[32];
	float		end_time;
	float		next_remind;
	float		next_allowed;		// level.time desde el que se puede abrir otra
	byte		choice[MAX_CLIENTS];	// por slot de cliente
} vote;

void Vote_Init (void)
{
	vote_enable   = gi.cvar ("vote_enable", "0", 0);
	vote_time     = gi.cvar ("vote_time", "30", 0);
	vote_percent  = gi.cvar ("vote_percent", "50", 0);
	vote_cooldown = gi.cvar ("vote_cooldown", "60", 0);
}

// al iniciar cada mapa (level se borra, pero esto es static)
void Vote_Reset (void)
{
	memset (&vote, 0, sizeof(vote));
}

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
		(*total)++;
		if (vote.choice[i] == VOTE_YES)
			(*yes)++;
		else if (vote.choice[i] == VOTE_NO)
			(*no)++;
	}
}

// "si" necesarios para superar vote_percent de total jugadores
static int Vote_Needed (int total)
{
	int pct = (int)vote_percent->value;

	if (pct < 0)
		pct = 0;
	if (pct > 99)
		pct = 99;
	return (total * pct) / 100 + 1;
}

static const char *Vote_What (void)
{
	static char buf[MAX_QPATH + 32];

	if (vote.restart)
		Com_sprintf (buf, sizeof(buf), "reiniciar %s", vote.map);
	else
		Com_sprintf (buf, sizeof(buf), "cambiar a %s", vote.map);
	return buf;
}

// busca word (sin distinguir mayusculas) en una lista separada por espacios o lineas
static qboolean Vote_InList (const char *list, char *word)
{
	char		buf[MAX_QPATH];
	const char	*p = list;
	int			n;

	while (p && *p)
	{
		while (*p == ' ' || *p == ',' || *p == '\t' || *p == '\r' || *p == '\n')
			p++;
		n = 0;
		while (*p && *p != ' ' && *p != ',' && *p != '\t' && *p != '\r' && *p != '\n')
		{
			if (n < (int)sizeof(buf) - 1)
				buf[n++] = *p;
			p++;
		}
		buf[n] = '\0';
		if (n && !Q_stricmp (buf, word))
			return true;
	}
	return false;
}

// el mapa se puede votar: esta en sv_maplist o votemaps.txt (o no hay listas) y existe
static qboolean Vote_MapAllowed (char *map)
{
	char		*votemaps;
	qboolean	have_list = false, found = false;

	if (sv_maplist->string[0] && Q_stricmp (sv_maplist->string, "txt"))
	{
		have_list = true;
		if (Vote_InList (sv_maplist->string, map))
			found = true;
	}

	if (!found)
	{
		votemaps = ReadEntFile ("votemaps.txt");
		if (votemaps)
		{
			have_list = true;
			if (Vote_InList (votemaps, map))
				found = true;
			gi.TagFree (votemaps);
		}
	}

	if (have_list && !found)
		return false;
	return MapExists (map);
}

static void Vote_PrintMaps (edict_t *ent)
{
	char	*votemaps;

	if (sv_maplist->string[0] && Q_stricmp (sv_maplist->string, "txt"))
		safe_cprintf (ent, PRINT_HIGH, "Mapas de la rotacion: %s\n", sv_maplist->string);
	votemaps = ReadEntFile ("votemaps.txt");
	if (votemaps)
	{
		char	line[1024];
		int		i, n = 0;

		for (i = 0; votemaps[i] && n < (int)sizeof(line) - 2; i++)
			line[n++] = (votemaps[i] == '\n' || votemaps[i] == '\r') ? ' ' : votemaps[i];
		line[n] = '\0';
		safe_cprintf (ent, PRINT_HIGH, "Otros mapas votables: %s\n", line);
		gi.TagFree (votemaps);
	}
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

static void Vote_End (qboolean passed, const char *why)
{
	vote.active = false;

	if (!passed)
	{
		safe_bprintf (PRINT_HIGH, "Votacion rechazada (%s): %s.\n", why, Vote_What ());
		vote.next_allowed = level.time + vote_cooldown->value;
		return;
	}

	safe_bprintf (PRINT_HIGH, "Votacion aprobada: %s.\n", Vote_What ());
	gi.dprintf ("vote: aprobado %s (propuso %s)\n", Vote_What (), vote.caller);

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
		"Uso: vote map <mapa> | vote restart | vote yes | vote no\n"
		"\"vote maps\" muestra los mapas que se pueden votar.\n");
}

static void Vote_Start (edict_t *ent, char *map, qboolean restart)
{
	vote.active = true;
	vote.restart = restart;
	Com_sprintf (vote.map, sizeof(vote.map), "%s", map);
	Com_sprintf (vote.caller, sizeof(vote.caller), "%s", ent->client->pers.netname);
	memset (vote.choice, 0, sizeof(vote.choice));
	vote.choice[ent - g_edicts - 1] = VOTE_YES;
	vote.end_time = level.time + (vote_time->value > 5 ? vote_time->value : 5);
	vote.next_remind = level.time + VOTE_REMIND;

	safe_bprintf (PRINT_HIGH, "%s propone %s. Escribe \"vote yes\" o \"vote no\" en la consola.\n",
		vote.caller, Vote_What ());
	gi.dprintf ("vote: %s propone %s\n", vote.caller, Vote_What ());

	Vote_Check ();	// con un solo jugador se aprueba al tiro
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
		Vote_PrintMaps (ent);
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
		vote.choice[slot] = Q_stricmp (arg, "no") ? VOTE_YES : VOTE_NO;
		safe_cprintf (ent, PRINT_HIGH, "Votaste %s.\n", vote.choice[slot] == VOTE_YES ? "si" : "no");
		Vote_Check ();
		if (vote.active)
			Vote_Status (ent);
		return;
	}

	if (Q_stricmp (arg, "map") && Q_stricmp (arg, "restart"))
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
		Vote_Start (ent, level.mapname, true);
		return;
	}

	arg = gi.argv (2);
	if (!arg[0])
	{
		safe_cprintf (ent, PRINT_HIGH, "Uso: vote map <mapa>. \"vote maps\" muestra la lista.\n");
		return;
	}
	if (strlen (arg) >= MAX_QPATH || strchr (arg, '/') || strchr (arg, '\\') || strchr (arg, '.')
		|| strchr (arg, '"') || strchr (arg, ';'))
	{
		safe_cprintf (ent, PRINT_HIGH, "Nombre de mapa no valido.\n");
		return;
	}
	if (!Vote_MapAllowed (arg))
	{
		safe_cprintf (ent, PRINT_HIGH, "El mapa %s no se puede votar. \"vote maps\" muestra la lista.\n", arg);
		return;
	}
	Vote_Start (ent, arg, false);
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
