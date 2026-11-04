/*
 * g_ready.c -- en duelo o torneo la partida empieza cuando los dos capitanes ponen ready.
 *
 * Apagado por defecto (ready_mode 0). Con ready_mode 1, en duelo (stats_mode duel) o
 * torneo (tournament 1), mientras la partida no ha empezado:
 *
 *   capitan          el jugador queda de capitan de su equipo (uno por equipo); tambien
 *                    se elige en el menu de equipos ("<equipo> (capitan)")
 *   ready / listo    el capitan marca a su equipo como listo (si el equipo no tiene
 *                    capitan, quien lo escribe queda de capitan)
 *   notready/unready/nolisto  lo desmarca
 *
 * Sirven en la consola y en el chat (el mensaje exacto, con o sin "!"). Cuando los dos
 * equipos estan listos arranca la cuenta de siempre ("Preparados! 3 2 1"). Al empezar,
 * en duelo se borran las kills del calentamiento (ya no deciden el ganador).
 *
 * Duracion: ready_time minutos; con 0 el duelo dura hasta que un equipo gane (sin tiempo).
 * En torneo hace falta tiempo (sin cuenta no se puede disparar): con ready_time 0 se usa
 * timelimit, o 20 minutos.
 *
 * Mientras se espera, cada 10 s el centro de la pantalla explica que hacer y muestra el
 * estado de cada equipo. "sv startcount" del admin sigue funcionando (se salta el ready);
 * "sv resetcount" vuelve a pedir ready. "sv ready" muestra el estado y "sv ready reset"
 * borra capitanes y ready.
 */

#include "g_local.h"
#include <ctype.h>
#include <stdarg.h>

extern int		countdownActive;
extern float	countdownTimeLimit;
extern int		freeze_remaining;

void StartCount (int seconds);

static cvar_t	*ready_mode;	// 1 = la partida de duelo/torneo empieza con el ready de los capitanes
static cvar_t	*ready_time;	// minutos del duelo (0 = hasta que gane un equipo)

static struct
{
	int			captain[2];		// numero de cliente + 1 del capitan de cada equipo (0 = sin capitan)
	qboolean	ready[2];
	qboolean	started;		// la partida ya empezo (cuenta llego a 0 o el admin la tiro)
} rd;

void Ready_Init (void)
{
	ready_mode = gi.cvar ("ready_mode", "0", 0);
	ready_time = gi.cvar ("ready_time", "0", 0);
}

// al cargar cada mapa
void Ready_Reset (void)
{
	memset (&rd, 0, sizeof(rd));
}

static qboolean Ready_IsDuel (void)
{
	cvar_t *stats_mode = gi.cvar ("stats_mode", "public", 0);

	return !Q_stricmp (stats_mode->string, "duel");
}

// duelo o torneo con ready_mode 1 (aunque ya haya empezado)
static qboolean Ready_Mode (void)
{
	if (!ready_mode || !ready_mode->value || !deathmatch->value || G_IsFFA () || level.campaign)
		return false;
	return Ready_IsDuel () || tournament->value;
}

// se esta esperando el ready de los capitanes
qboolean Ready_Waiting (void)
{
	return Ready_Mode () && !rd.started && !countdownActive && countdownTimeLimit <= 0 &&
		!freeze_remaining && !level.intermissiontime;
}

// duelo o torneo que todavia no empieza (con o sin ready_mode): calentamiento
qboolean Ready_MatchPending (void)
{
	if (!deathmatch->value || G_IsFFA () || level.campaign || level.intermissiontime)
		return false;
	return (Ready_IsDuel () || tournament->value) && !rd.started;
}

static int Ready_TeamOf (edict_t *ent)
{
	if (!ent || !ent->inuse || !ent->client || ent->ai || !ent->client->resp.team_on)
		return -1;
	if (ent->client->resp.team_on == team_list[0])
		return 0;
	if (ent->client->resp.team_on == team_list[1])
		return 1;
	return -1;
}

static edict_t *Ready_Captain (int team)
{
	return rd.captain[team] ? g_edicts + rd.captain[team] : NULL;
}

static const char *Ready_TeamName (int team)
{
	return team_list[team] && team_list[team]->teamname ? team_list[team]->teamname : (team ? "Axis" : "Allies");
}

static int Ready_HumansIn (int team)
{
	int i, n = 0;

	for (i = 1; i <= game.maxclients; i++)
		if (Ready_TeamOf (g_edicts + i) == team)
			n++;
	return n;
}

static void Ready_Say (const char *fmt, ...)
{
	char	msg[256];
	va_list	ap;

	va_start (ap, fmt);
	vsnprintf (msg, sizeof(msg), fmt, ap);
	va_end (ap);
	safe_bprintf (PRINT_HIGH, "%s\n", msg);
}

// texto del centro de la pantalla mientras se espera
void Ready_CenterPrint (edict_t *ent)
{
	char	line[2][96];
	int		t;
	edict_t	*cap;

	for (t = 0; t < 2; t++)
	{
		cap = Ready_Captain (t);
		if (rd.ready[t])
			Com_sprintf (line[t], sizeof(line[t]), "%s: LISTO (%s)", Ready_TeamName (t), cap->client->pers.netname);
		else if (cap)
			Com_sprintf (line[t], sizeof(line[t]), "%s: esperando a %s", Ready_TeamName (t), cap->client->pers.netname);
		else
			Com_sprintf (line[t], sizeof(line[t]), "%s: esperando (sin capitan)", Ready_TeamName (t));
	}

	safe_centerprintf (ent, "Modo %s: la partida aun no empieza\n\n"
		"El capitan de cada equipo escribe\n"
		"ready o listo en el chat o la consola\n"
		"(nolisto para deshacerlo)\n"
		"Capitan: se elige al elegir lado\n"
		"o escribiendo capitan\n\n"
		"%s\n%s",
		tournament->value ? "TORNEO" : "DUELO", line[0], line[1]);
}

static void Ready_CenterAll (void)
{
	int i;

	for (i = 1; i <= game.maxclients; i++)
		if (g_edicts[i].inuse && g_edicts[i].client && !g_edicts[i].ai)
			Ready_CenterPrint (g_edicts + i);
}

static void Ready_SetCaptain (edict_t *ent, int team)
{
	rd.captain[team] = ent - g_edicts;
	Ready_Say ("%s es el capitan de %s.", ent->client->pers.netname, Ready_TeamName (team));
}

// "capitan": desde la consola, el chat o el menu de equipos
void Ready_ClaimCaptain (edict_t *ent)
{
	int		team = Ready_TeamOf (ent);
	edict_t	*cap;

	if (!Ready_Waiting ())
	{
		if (Ready_Mode ())
			safe_cprintf (ent, PRINT_HIGH, "La partida ya empezo: el capitan solo cuenta antes de empezar.\n");
		else
			safe_cprintf (ent, PRINT_HIGH, "Capitan solo sirve en duelo o torneo con ready.\n");
		return;
	}
	if (team < 0)
	{
		safe_cprintf (ent, PRINT_HIGH, "Primero elige un equipo.\n");
		return;
	}
	cap = Ready_Captain (team);
	if (cap == ent)
	{
		safe_cprintf (ent, PRINT_HIGH, "Ya eres el capitan de %s. Escribe ready cuando esten listos.\n", Ready_TeamName (team));
		return;
	}
	if (cap)
	{
		safe_cprintf (ent, PRINT_HIGH, "%s ya tiene capitan: %s.\n", Ready_TeamName (team), cap->client->pers.netname);
		return;
	}
	Ready_SetCaptain (ent, team);
	safe_cprintf (ent, PRINT_HIGH, "Escribe ready o listo cuando tu equipo este listo (nolisto lo deshace).\n");
	Ready_CenterAll ();
}

static void Ready_Start (void)
{
	int minutes = (int)ready_time->value;

	if (minutes <= 0 && tournament->value)
		minutes = timelimit->value > 0 ? (int)timelimit->value : 20;

	if (minutes > 0)
		Ready_Say ("Los dos equipos estan listos: empieza la partida (%d minutos).", minutes);
	else
		Ready_Say ("Los dos equipos estan listos: empieza el duelo (hasta que gane un equipo).");
	rd.ready[0] = rd.ready[1] = false;
	StartCount (minutes * 60);
}

static void Ready_Set (edict_t *ent, qboolean on)
{
	int		team = Ready_TeamOf (ent), other;
	edict_t	*cap;

	if (!Ready_Waiting ())
	{
		if (Ready_Mode ())
			safe_cprintf (ent, PRINT_HIGH, "La partida ya empezo.\n");
		else
			safe_cprintf (ent, PRINT_HIGH, "Ready solo sirve en duelo o torneo (ready_mode 1).\n");
		return;
	}
	if (team < 0)
	{
		safe_cprintf (ent, PRINT_HIGH, "Primero elige un equipo.\n");
		return;
	}

	cap = Ready_Captain (team);
	if (!cap && on)
		Ready_SetCaptain (ent, team);		// sin capitan: el que pone ready queda de capitan
	else if (cap != ent)
	{
		safe_cprintf (ent, PRINT_HIGH, "Solo el capitan de %s (%s) puede poner ready.\n",
			Ready_TeamName (team), cap ? cap->client->pers.netname : "nadie");
		return;
	}

	if (rd.ready[team] == on)
	{
		safe_cprintf (ent, PRINT_HIGH, on ? "Tu equipo ya esta listo.\n" : "Tu equipo no estaba listo.\n");
		return;
	}
	rd.ready[team] = on;
	other = !team;

	if (on)
		safe_cprintf (ent, PRINT_HIGH, "Si no estaban listos, escribe nolisto para deshacerlo.\n");

	if (!on)
		Ready_Say ("%s ya NO esta listo (%s): su capitan escribe ready o listo cuando esten.",
			Ready_TeamName (team), ent->client->pers.netname);
	else if (!rd.ready[other])
		Ready_Say ("%s esta LISTO (%s). Falta %s: su capitan escribe ready o listo.",
			Ready_TeamName (team), ent->client->pers.netname, Ready_TeamName (other));

	if (rd.ready[0] && rd.ready[1])
		Ready_Start ();
	else
		Ready_CenterAll ();
}

void Cmd_Ready_f (edict_t *ent)
{
	Ready_Set (ent, true);
}

void Cmd_NotReady_f (edict_t *ent)
{
	Ready_Set (ent, false);
}

// chat: "ready", "!listo", "capitan"... (el mensaje se muestra igual). true si era un comando
qboolean Ready_ChatCommand (edict_t *ent, const char *text)
{
	char	word[32];
	int		n = 0;

	if (!Ready_Mode () || !text)
		return false;

	while (*text == '"' || *text == ' ' || *text == '!')
		text++;
	while (*text && *text != '"' && n < (int)sizeof(word) - 1)
		word[n++] = tolower ((unsigned char)*text++);
	while (n > 0 && word[n - 1] == ' ')
		n--;
	word[n] = '\0';

	if (!strcmp (word, "ready") || !strcmp (word, "listo") || !strcmp (word, "listos"))
		Ready_Set (ent, true);
	else if (!strcmp (word, "notready") || !strcmp (word, "noready") || !strcmp (word, "unready") || !strcmp (word, "nolisto") ||
		!strcmp (word, "no listo") || !strcmp (word, "not ready"))
		Ready_Set (ent, false);
	else if (!strcmp (word, "capitan") || !strcmp (word, "captain"))
		Ready_ClaimCaptain (ent);
	else
		return false;
	return true;
}

// cada frame: capitanes que se fueron de su equipo o del server
void Ready_RunFrame (void)
{
	int		t;
	edict_t	*cap;

	if (!rd.captain[0] && !rd.captain[1])
		return;

	for (t = 0; t < 2; t++)
	{
		cap = Ready_Captain (t);
		if (!cap || Ready_TeamOf (cap) == t)
			continue;
		rd.captain[t] = 0;
		if (rd.ready[t])
			rd.ready[t] = false;
		if (Ready_Waiting ())
		{
			Ready_Say ("%s se quedo sin capitan: el que escriba ready o capitan lo sera.", Ready_TeamName (t));
			Ready_CenterAll ();
		}
	}
}

void Ready_ClientDisconnect (edict_t *ent)
{
	int t;

	for (t = 0; t < 2; t++)
		if (rd.captain[t] == ent - g_edicts)
		{
			rd.captain[t] = 0;
			rd.ready[t] = false;
			if (Ready_Waiting ())
				Ready_Say ("%s se quedo sin capitan (%s se fue).", Ready_TeamName (t), ent->client->pers.netname);
		}
}

// la cuenta llego a 0 ("Que comience el juego!"); tambien al reanudar una pausa
void Ready_MatchStart (void)
{
	int i;

	if (rd.started)
		return;
	rd.started = true;
	rd.ready[0] = rd.ready[1] = false;

	// duelo: las kills del calentamiento no cuentan para el ganador
	if (!Ready_IsDuel () && !tournament->value)
		return;
	for (i = 0; i < MAX_TEAMS; i++)
		if (team_list[i])
		{
			team_list[i]->kills = 0;
			team_list[i]->losses = 0;
			team_list[i]->score = 0;
		}
	for (i = 1; i <= game.maxclients; i++)
		if (g_edicts[i].inuse && g_edicts[i].client)
		{
			g_edicts[i].client->resp.score = 0;
			g_edicts[i].client->resp.points = 0;
			g_edicts[i].client->resp.streak = 0;
		}
}

// "sv resetcount": se vuelve a pedir ready
void Ready_CountReset (void)
{
	rd.started = false;
	rd.ready[0] = rd.ready[1] = false;
}

// "sv ready [reset]"
void Svcmd_Ready_f (void)
{
	int		t;
	edict_t	*cap;

	if (!Q_stricmp (gi.argv (2), "reset"))
	{
		Ready_Reset ();
		gi.cprintf (NULL, PRINT_HIGH, "ready: capitanes y ready borrados\n");
		if (Ready_Waiting ())
			Ready_CenterAll ();
		return;
	}

	gi.cprintf (NULL, PRINT_HIGH, "ready_mode %d, %s, %s\n", (int)ready_mode->value,
		Ready_Mode () ? (tournament->value ? "torneo" : "duelo") : "sin duelo ni torneo",
		Ready_Waiting () ? "esperando ready" : (rd.started ? "partida empezada" : "sin esperar"));
	for (t = 0; t < 2; t++)
	{
		cap = Ready_Captain (t);
		gi.cprintf (NULL, PRINT_HIGH, "  %s: capitan %s, %s, %d humanos, %d kills\n", Ready_TeamName (t),
			cap ? cap->client->pers.netname : "(ninguno)", rd.ready[t] ? "LISTO" : "no listo", Ready_HumansIn (t),
			team_list[t] ? team_list[t]->kills : 0);
	}
}

// menu de equipos: con ready activo se puede entrar como capitan
qboolean Ready_MenuCaptain (void)
{
	return Ready_Waiting ();
}
