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
 * borra capitanes y ready ("sv ready seguir" saca la pausa). Si un equipo esta listo y el otro no, cada ready_remind segundos
 * se le recuerda al otro (0 = sin aviso).
 *
 * Ya empezada la partida:
 *
 *   pausa / pause / timeout     el capitan pausa la partida (pause_max pausas por equipo,
 *                               0 = sin pausas); nadie se mueve ni recibe dano y el reloj se
 *                               detiene. Dura pause_time segundos como maximo
 *   seguir / continuar / unpause  los dos capitanes lo escriben para seguir antes; se
 *                               reanuda con una cuenta de 5 segundos
 *   cancelar / cancel           los dos capitanes (en menos de 30 s) cancelan la partida, como
 *                               "sv resetcount": se vuelve al calentamiento y a pedir ready
 *
 * Si se desconecta un jugador de un equipo, al volver entra solo a su mismo equipo. Con
 * pause_auto 1 ademas se pausa la partida hasta que vuelva (o pause_time segundos).
 * Los espectadores usan el chat de equipo entre ellos (Ready_SpectatorChat).
 */

#include "g_local.h"
#include <ctype.h>
#include <stdarg.h>

extern int		countdownActive;
extern float	countdownTimeLimit;
extern int		freeze_remaining;
extern qboolean	freeze_mode;
extern float	gameStartTime;

void StartCount (int seconds);
void ResetCountTimer (void);
void ResetFreezeMode (void);
void M_Team_Join (edict_t *ent, pmenu_t *p, int choice);

static cvar_t	*ready_mode;	// 1 = la partida de duelo/torneo empieza con el ready de los capitanes
static cvar_t	*ready_time;	// minutos del duelo (0 = hasta que gane un equipo)
static cvar_t	*ready_remind;	// segundos entre avisos al equipo que no pone ready (0 = sin aviso)
static cvar_t	*pause_max;		// pausas de cada capitan por partida (0 = sin pausas)
static cvar_t	*pause_time;	// segundos que dura una pausa como maximo
static cvar_t	*pause_auto;	// 1 = pausa sola cuando se desconecta un jugador de un equipo

#define READY_MAX_DROPPED	16
#define READY_RESUME_SECS	5
#define READY_PAUSE_REF		-2		// pausa del arbitro: sin tiempo, solo el (o sv ready seguir) la saca
#define READY_CANCEL_SECS	30

typedef struct
{
	char		name[32];
	int			team;
	int			mos;			// su clase: al volver la pide de nuevo (si hay cupo)
	qboolean	in_pause;		// la pausa automatica espera que vuelva
} ready_dropped_t;

static struct
{
	int			captain[2];		// numero de cliente + 1 del capitan de cada equipo (0 = sin capitan)
	qboolean	ready[2];
	qboolean	started;		// la partida ya empezo (cuenta llego a 0 o el admin la tiro)
	float		ready_since;	// level.time en que el primer equipo quedo listo
	float		next_remind;

	// pausa de los capitanes (o automatica)
	qboolean	paused;
	int			pause_team;		// equipo que la pidio, -1 = automatica (se cayo alguien), READY_PAUSE_REF = arbitro
	char		pause_who[32];
	float		pause_end;		// level.time en que sigue sola
	float		resume_at;		// > 0: cuenta para seguir
	int			resume_secs;	// ultimo segundo anunciado de la cuenta
	qboolean	resume_ok[2];	// el capitan escribio seguir
	int			pauses_used[2];
	float		next_center;

	float		cancel_at[2];	// level.time del "cancelar" de cada capitan (0 = no)

	ready_dropped_t	dropped[READY_MAX_DROPPED];	// se cayeron con la partida empezada
	int			kicked;			// numero de cliente + 1 echado por votacion (no pausa ni vuelve solo)
} rd;

void Ready_Init (void)
{
	ready_mode = gi.cvar ("ready_mode", "0", 0);
	ready_time = gi.cvar ("ready_time", "0", 0);
	ready_remind = gi.cvar ("ready_remind", "60", 0);
	pause_max = gi.cvar ("pause_max", "2", 0);
	pause_time = gi.cvar ("pause_time", "120", 0);
	pause_auto = gi.cvar ("pause_auto", "1", 0);
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

// la partida de duelo/torneo ya empezo (la cuenta llego a 0)
qboolean Ready_Started (void)
{
	return rd.started;
}

// torneo (tournament 1) que todavia no empieza o esta en la cuenta: sin dano, sin tirar armas ni tomar
// el maletin. Con la partida empezada sin tiempo limite (ready_time 0) ya se juega
qboolean Tournament_Waiting (void)
{
	return tournament->value && (countdownActive > 0 || (countdownTimeLimit <= 0 && !rd.started));
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

	// torneo activado con sv torneo: su nombre y el menu
	if (Torneo_Active ())
	{
		safe_centerprintf (ent, "TORNEO: %.33s\nla partida aun no empieza\n\n"
			"Capitanes: menu Duelo/Torneo\n"
			"(escribe menuduelo) o escriban\n"
			"ready / listo en el chat\n\n"
			"%s\n%s",
			Torneo_Name (), line[0], line[1]);
		return;
	}

	safe_centerprintf (ent, "Modo %s: la partida aun no empieza\n%s\n"
		"El capitan de cada equipo escribe\n"
		"ready o listo en el chat o la consola\n"
		"(nolisto para deshacerlo)\n"
		"Capitan: se elige al elegir lado\n"
		"o escribiendo capitan\n\n"
		"%s\n%s",
		tournament->value ? "TORNEO" : "DUELO",
		// evento puesto a mano sin el modo torneo: se juega con dano en el calentamiento
		(Torneo_Name ()[0] && !tournament->value) ? "OJO: evento sin modo torneo\n" : "",
		line[0], line[1]);
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

// el capitan se fue (del server o del equipo): pasa altiro a otro jugador del equipo (si queda alguno).
// Antes de empezar el ready del equipo se borra: el nuevo capitan lo vuelve a poner
static void Ready_CaptainLeft (int team, edict_t *old, const char *why)
{
	int		i;
	edict_t	*e;

	rd.captain[team] = 0;
	if (Ready_Waiting ())
		rd.ready[team] = false;
	for (i = 1; i <= game.maxclients; i++)
	{
		e = g_edicts + i;
		if (e == old || Ready_TeamOf (e) != team)
			continue;
		Ready_Say ("%s %s: el capitan de %s ahora es otro.", old->client->pers.netname, why, Ready_TeamName (team));
		Ready_SetCaptain (e, team);
		safe_centerprintf (e, "Ahora eres el capitan de %s.\n\n%s", Ready_TeamName (team),
			Ready_Waiting () ? "Pon Listos en el menu Duelo/Torneo\n(menuduelo) o escribe ready." :
			"Pausa, seguir y cancelar\nlos manejas tu.");
		if (Ready_Waiting ())
			Ready_CenterAll ();
		return;
	}
	if (Ready_Waiting ())
	{
		Ready_Say ("%s se quedo sin capitan: el que escriba ready o capitan lo sera.", Ready_TeamName (team));
		Ready_CenterAll ();
	}
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
	if (on && !rd.ready[other])
	{
		rd.ready_since = level.time;
		rd.next_remind = 0;
	}

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
	else if (!strcmp (word, "pausa") || !strcmp (word, "pause") || !strcmp (word, "timeout"))
		Cmd_Pause_f (ent);
	else if (!strcmp (word, "seguir") || !strcmp (word, "continuar") || !strcmp (word, "unpause") || !strcmp (word, "resume"))
		Cmd_Resume_f (ent);
	else if (!strcmp (word, "cancelar") || !strcmp (word, "cancel"))
		Cmd_Cancel_f (ent);
	else
		return false;
	return true;
}

// aviso al equipo que no pone ready mientras el otro ya esta listo
static void Ready_Remind (void)
{
	int		t, i;
	edict_t	*cap, *e;
	char	msg[160];

	if (!ready_remind || ready_remind->value <= 0 || rd.ready[0] == rd.ready[1] || !Ready_Waiting ())
		return;
	if (level.time - rd.ready_since < ready_remind->value || level.time < rd.next_remind)
		return;
	rd.next_remind = level.time + ready_remind->value;

	t = rd.ready[0] ? 1 : 0;	// el que falta
	if (!Ready_HumansIn (t))
		return;
	cap = Ready_Captain (t);
	if (cap)
		Com_sprintf (msg, sizeof(msg), "%s esta listo hace %d min. %s: su capitan %s aun no pone ready.",
			Ready_TeamName (!t), (int)((level.time - rd.ready_since) / 60), Ready_TeamName (t), cap->client->pers.netname);
	else
		Com_sprintf (msg, sizeof(msg), "%s esta listo hace %d min. %s: no tiene capitan, el que escriba ready lo sera.",
			Ready_TeamName (!t), (int)((level.time - rd.ready_since) / 60), Ready_TeamName (t));
	Ready_Say ("%s", msg);

	for (i = 1; i <= game.maxclients; i++)
	{
		e = g_edicts + i;
		if (Ready_TeamOf (e) == t)
			safe_centerprintf (e, "%s ya esta listo\n\n%s\nescriban ready o listo", Ready_TeamName (!t),
				cap ? va ("Capitan (%s):", cap->client->pers.netname) : "El capitan");
	}
}

/*
=================
Pausa de los capitanes
=================
*/

qboolean Ready_Paused (void)
{
	return rd.paused;
}

// la partida esta en juego (ya empezo, no ha terminado, no hay cuenta corriendo)
static qboolean Ready_InPlay (void)
{
	return Ready_Mode () && rd.started && !level.intermissiontime && !countdownActive;
}

// ent actua como capitan de team: si el equipo no tiene, queda de capitan
static qboolean Ready_AsCaptain (edict_t *ent, int team, const char *what)
{
	edict_t *cap = Ready_Captain (team);

	if (!cap)
	{
		Ready_SetCaptain (ent, team);
		return true;
	}
	if (cap == ent)
		return true;
	safe_cprintf (ent, PRINT_HIGH, "Solo el capitan de %s (%s) puede %s.\n", Ready_TeamName (team),
		cap->client->pers.netname, what);
	return false;
}

static int Ready_PauseSecs (void)
{
	return pause_time && pause_time->value > 0 ? (int)pause_time->value : 120;
}

static void Ready_BeginPause (int team, const char *who)
{
	freeze_mode = true;
	rd.paused = true;
	rd.pause_team = team;
	Com_sprintf (rd.pause_who, sizeof(rd.pause_who), "%s", who);
	rd.pause_end = level.time + Ready_PauseSecs ();
	rd.resume_at = 0;
	rd.resume_ok[0] = rd.resume_ok[1] = false;
	rd.next_center = 0;
	// aviso corto: la voz de la cuenta (first_count) va recien al seguir
	gi.positioned_sound (vec3_origin, &g_edicts[0], CHAN_AUTO, gi.soundindex ("misc/talk1.wav"), 1, ATTN_NONE, 0);
}

static void Ready_EndPause (void)
{
	int i;

	rd.paused = false;
	rd.resume_at = 0;
	for (i = 0; i < READY_MAX_DROPPED; i++)
		rd.dropped[i].in_pause = false;
	ResetFreezeMode ();
}

static void Ready_StartResume (const char *why)
{
	if (rd.resume_at)
		return;
	rd.resume_at = level.time + READY_RESUME_SECS;
	rd.resume_secs = 0;
	// la misma voz de la cuenta del inicio (dura lo mismo que READY_RESUME_SECS)
	gi.positioned_sound (vec3_origin, &g_edicts[0], CHAN_AUTO, gi.soundindex ("misc/first_count.wav"), 1, ATTN_NONE, 0);
	Ready_Say ("%s: la partida sigue en %d segundos.", why, READY_RESUME_SECS);
}

static qboolean Ready_WaitingDropped (void)
{
	int i;

	for (i = 0; i < READY_MAX_DROPPED; i++)
		if (rd.dropped[i].name[0] && rd.dropped[i].in_pause)
			return true;
	return false;
}

static void Ready_PauseCenter (void)
{
	int		i, left, t;
	char	head[96], who[2][64];
	edict_t	*e;

	if (rd.resume_at)
	{
		left = (int)(rd.resume_at - level.time + 0.99F);
		if (left == rd.resume_secs || left <= 0)
			return;
		rd.resume_secs = left;
		centerprintall ("La partida sigue en %d...", left);
		return;
	}

	if (level.time < rd.next_center)
		return;
	rd.next_center = level.time + 1;

	left = (int)(rd.pause_end - level.time);
	if (left < 0)
		left = 0;
	if (rd.pause_team >= 0)
		Com_sprintf (head, sizeof(head), "PAUSA de %s (%s)", Ready_TeamName (rd.pause_team), rd.pause_who);
	else if (rd.pause_team == READY_PAUSE_REF)
		Com_sprintf (head, sizeof(head), "PAUSA del arbitro (%s)", rd.pause_who);
	else
		Com_sprintf (head, sizeof(head), "PAUSA: se desconecto %s", rd.pause_who);
	for (t = 0; t < 2; t++)
		Com_sprintf (who[t], sizeof(who[t]), "%s: %s", Ready_TeamName (t), rd.resume_ok[t] ? "quiere seguir" : "en pausa");

	for (i = 1; i <= game.maxclients; i++)
	{
		e = g_edicts + i;
		if (!e->inuse || !e->client || e->ai)
			continue;
		if (rd.pause_team == READY_PAUSE_REF)
			safe_centerprintf (e, "%s\n\nLa saca el arbitro", head);
		else if (rd.pause_team < 0)
			safe_centerprintf (e, "%s\n\nSigue cuando vuelva\n"
				"Si no vuelve, los dos capitanes\nescriben seguir\n\n%s\n%s", head, who[0], who[1]);
		else
			safe_centerprintf (e, "%s\n\nSigue sola en %d:%02d\n"
				"Para seguir antes, los dos capitanes\nescriben seguir\n\n%s\n%s",
				head, left / 60, left % 60, who[0], who[1]);
	}
}

static void Ready_PauseFrame (void)
{
	int		i;
	float	old_start = gameStartTime;

	if (!rd.paused)
		return;

	// el admin la saco con "sv freeze", o se termino la partida
	if (!freeze_mode || level.intermissiontime)
	{
		rd.paused = false;
		rd.resume_at = 0;
		return;
	}

	// el reloj no corre en la pausa: el tiempo limite, el de los objetivos y la zona de control
	gameStartTime += FRAMETIME;
	if (level.control_gamestart == old_start)
		level.control_gamestart = gameStartTime;
	for (i = 0; i < MAX_TEAMS; i++)
		if (team_list[i] && team_list[i]->time_to_win > 0)
			team_list[i]->time_to_win += FRAMETIME;

	if (!rd.resume_at)
	{
		// la de un capitan tiene tiempo; la automatica (se cayo alguien) espera que vuelva o los capitanes
		if (rd.pause_team >= 0 && level.time >= rd.pause_end)
			Ready_StartResume ("Se acabo el tiempo de la pausa");
		else if (rd.pause_team == -1 && !Ready_WaitingDropped ())
			Ready_StartResume ("Volvio el que se habia caido");
	}
	else if (level.time >= rd.resume_at)
	{
		Ready_EndPause ();
		centerprintall ("Sigue la partida!");
		Ready_Say ("Sigue la partida.");
		return;
	}

	Ready_PauseCenter ();
}

void Cmd_Pause_f (edict_t *ent)
{
	int team = Ready_TeamOf (ent);

	if (!Ready_Mode ())
	{
		safe_cprintf (ent, PRINT_HIGH, "La pausa solo sirve en duelo o torneo con ready.\n");
		return;
	}
	if (!pause_max || pause_max->value <= 0)
	{
		safe_cprintf (ent, PRINT_HIGH, "Las pausas de los capitanes estan desactivadas.\n");
		return;
	}
	if (rd.paused || freeze_mode)
	{
		safe_cprintf (ent, PRINT_HIGH, "La partida ya esta en pausa.\n");
		return;
	}
	if (!Ready_InPlay ())
	{
		safe_cprintf (ent, PRINT_HIGH, countdownActive ? "Espera que termine la cuenta.\n" : "La partida no esta en juego.\n");
		return;
	}
	if (team < 0)
	{
		safe_cprintf (ent, PRINT_HIGH, "Solo los capitanes pueden pedir pausa.\n");
		return;
	}
	if (!Ready_AsCaptain (ent, team, "pedir pausa"))
		return;
	if (rd.pauses_used[team] >= (int)pause_max->value)
	{
		safe_cprintf (ent, PRINT_HIGH, "%s ya uso sus %d pausas.\n", Ready_TeamName (team), (int)pause_max->value);
		return;
	}

	rd.pauses_used[team]++;
	Ready_BeginPause (team, ent->client->pers.netname);
	Ready_Say ("PAUSA de %s (%s), %d:%02d como maximo (pausa %d de %d). Para seguir antes, los dos capitanes escriben seguir.",
		Ready_TeamName (team), ent->client->pers.netname, Ready_PauseSecs () / 60, Ready_PauseSecs () % 60,
		rd.pauses_used[team], (int)pause_max->value);
}

void Cmd_Resume_f (edict_t *ent)
{
	int team = Ready_TeamOf (ent), t;

	if (!rd.paused)
	{
		safe_cprintf (ent, PRINT_HIGH, freeze_mode ? "La pausa es del admin.\n" : "La partida no esta en pausa.\n");
		return;
	}
	if (rd.resume_at)
	{
		safe_cprintf (ent, PRINT_HIGH, "La partida ya va a seguir.\n");
		return;
	}
	if (rd.pause_team == READY_PAUSE_REF)
	{
		safe_cprintf (ent, PRINT_HIGH, "La pausa es del arbitro (%s): el la saca.\n", rd.pause_who);
		return;
	}
	if (team < 0)
	{
		safe_cprintf (ent, PRINT_HIGH, "Solo los capitanes pueden escribir seguir.\n");
		return;
	}
	if (!Ready_AsCaptain (ent, team, "escribir seguir"))
		return;
	if (rd.resume_ok[team])
	{
		safe_cprintf (ent, PRINT_HIGH, "Ya escribiste seguir; falta el otro capitan.\n");
		return;
	}
	rd.resume_ok[team] = true;

	for (t = 0; t < 2; t++)
		if (!rd.resume_ok[t] && Ready_HumansIn (t))
		{
			Ready_Say ("%s quiere seguir (%s). Falta que el capitan de %s escriba seguir.",
				Ready_TeamName (team), ent->client->pers.netname, Ready_TeamName (t));
			rd.next_center = 0;
			return;
		}
	Ready_StartResume ("Los capitanes escribieron seguir");
}

/*
=================
Cancelar la partida (los dos capitanes)
=================
*/

static void Ready_CancelMatch (void)
{
	StatsLog_CountdownReset ();
	Ready_CountReset ();
	ResetCountTimer ();
	ResetFreezeMode ();
	centerprintall ("Partida CANCELADA por los capitanes\n\nVuelve el calentamiento:\nlos capitanes escriben ready");
	Ready_Say ("Los dos capitanes cancelaron la partida: vuelve el calentamiento, escriban ready para empezar otra.");
}

void Cmd_Cancel_f (edict_t *ent)
{
	int team = Ready_TeamOf (ent), t;

	if (!Ready_Mode () || level.intermissiontime || (!rd.started && !countdownActive))
	{
		safe_cprintf (ent, PRINT_HIGH, "No hay una partida que cancelar.\n");
		return;
	}
	if (team < 0)
	{
		safe_cprintf (ent, PRINT_HIGH, "Solo los capitanes pueden cancelar la partida.\n");
		return;
	}
	if (!Ready_AsCaptain (ent, team, "cancelar"))
		return;

	rd.cancel_at[team] = level.time;
	for (t = 0; t < 2; t++)
		if (Ready_HumansIn (t) && (!rd.cancel_at[t] || level.time - rd.cancel_at[t] > READY_CANCEL_SECS))
		{
			Ready_Say ("%s quiere CANCELAR la partida (%s). Si el capitan de %s escribe cancelar en %d s, se cancela.",
				Ready_TeamName (team), ent->client->pers.netname, Ready_TeamName (t), READY_CANCEL_SECS);
			return;
		}
	rd.cancel_at[0] = rd.cancel_at[1] = 0;
	Ready_CancelMatch ();
}

/*
=================
Los que se caen vuelven a su equipo
=================
*/

static void Ready_AddDropped (edict_t *ent, int team, qboolean in_pause)
{
	int i, free_slot = -1;

	for (i = 0; i < READY_MAX_DROPPED; i++)
	{
		if (!rd.dropped[i].name[0])
		{
			if (free_slot < 0)
				free_slot = i;
		}
		else if (!Q_stricmp (rd.dropped[i].name, ent->client->pers.netname))
		{
			free_slot = i;
			break;
		}
	}
	if (free_slot < 0)
		free_slot = 0;	// lleno: se pisa el mas viejo
	Com_sprintf (rd.dropped[free_slot].name, sizeof(rd.dropped[free_slot].name), "%s", ent->client->pers.netname);
	rd.dropped[free_slot].team = team;
	rd.dropped[free_slot].mos = ent->client->resp.newmos ? ent->client->resp.newmos : ent->client->resp.mos;
	rd.dropped[free_slot].in_pause = in_pause;
}

// los que vuelven entran solos a su mismo equipo (eligen clase en el menu de siempre)
static void Ready_Rejoin (void)
{
	int		i, d;
	edict_t	*e;

	for (i = 1; i <= game.maxclients; i++)
	{
		e = g_edicts + i;
		if (!e->inuse || !e->client || e->ai || e->client->resp.team_on || level.framenum < e->client->resp.enterframe + 10)
			continue;
		for (d = 0; d < READY_MAX_DROPPED; d++)
		{
			if (!rd.dropped[d].name[0] || Q_stricmp (rd.dropped[d].name, e->client->pers.netname))
				continue;
			if (!team_list[rd.dropped[d].team])
				break;
			M_Team_Join (e, NULL, rd.dropped[d].team + 11);
			if (e->client->resp.team_on == team_list[rd.dropped[d].team])
			{
				int mos = rd.dropped[d].mos;

				// pide su clase de antes (al aparecer se revisa el cupo, como al elegirla en el menu): si hace
				// clic sin elegir no aparece de infanteria; el menu de clase queda abierto para cambiarla
				if (mos > NONE && mos < MAX_MOS && e->client->resp.team_on->mos[mos])
				{
					e->client->resp.newmos = mos;
					safe_cprintf (e, PRINT_HIGH, "Vuelves como %s; si quieres otra clase, eligela en el menu.\n",
						e->client->resp.team_on->mos[mos]->name);
				}
				Ready_Say ("%s volvio y entra a %s.", e->client->pers.netname, Ready_TeamName (rd.dropped[d].team));
				rd.dropped[d].name[0] = '\0';
				rd.dropped[d].in_pause = false;
			}
			break;
		}
	}
}

// cada frame: capitanes que se fueron de su equipo o del server, pausa, avisos
void Ready_RunFrame (void)
{
	int		t;
	edict_t	*cap;

	Ready_PauseFrame ();

	if (Ready_Mode () && rd.started && !level.intermissiontime && level.framenum % 5 == 0)
		Ready_Rejoin ();

	Ready_Remind ();

	if (!rd.captain[0] && !rd.captain[1])
		return;

	for (t = 0; t < 2; t++)
	{
		cap = Ready_Captain (t);
		if (!cap || Ready_TeamOf (cap) == t)
			continue;
		Ready_CaptainLeft (t, cap, "dejo el equipo");
	}
}

void Ready_ClientDisconnect (edict_t *ent)
{
	int			t, team = Ready_TeamOf (ent);
	qboolean	pause;

	if (rd.kicked == ent - g_edicts)
	{
		rd.kicked = 0;
		team = -1;
	}

	// con la partida empezada: al volver entra a su equipo, y con pause_auto se pausa hasta que vuelva
	if (team >= 0 && Ready_Mode () && rd.started && !level.intermissiontime)
	{
		pause = pause_auto && pause_auto->value && !countdownActive && (rd.paused || !freeze_mode);
		Ready_AddDropped (ent, team, pause);
		if (pause)
		{
			if (!rd.paused)
				Ready_BeginPause (-1, ent->client->pers.netname);
			else if (rd.pause_team != READY_PAUSE_REF)
			{
				// ya estaba en pausa: ahora espera tambien a este (la del arbitro sigue siendo suya)
				rd.pause_team = -1;
				Com_sprintf (rd.pause_who, sizeof(rd.pause_who), "%s", ent->client->pers.netname);
				rd.resume_at = 0;
				rd.resume_ok[0] = rd.resume_ok[1] = false;
			}
			Ready_Say ("PAUSA: %s (%s) se desconecto. Cuando vuelva entra solo a su equipo y la partida sigue; "
				"si no vuelve, los dos capitanes escriben seguir.", ent->client->pers.netname, Ready_TeamName (team));
		}
	}

	for (t = 0; t < 2; t++)
		if (rd.captain[t] == ent - g_edicts)
			Ready_CaptainLeft (t, ent, "se fue");
}

// la cuenta llego a 0 ("Que comience el juego!"); tambien al reanudar una pausa
void Ready_MatchStart (void)
{
	int i;

	if (rd.started)
		return;
	rd.started = true;
	rd.ready[0] = rd.ready[1] = false;
	rd.pauses_used[0] = rd.pauses_used[1] = 0;
	rd.cancel_at[0] = rd.cancel_at[1] = 0;

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
			// pantalla final: K, M, +/-, precision y curacion solo del duelo
			g_edicts[i].client->resp.stat_human_plus = g_edicts[i].client->resp.stat_bot_plus = 0;
			g_edicts[i].client->resp.stat_human_minus = g_edicts[i].client->resp.stat_bot_minus = 0;
			g_edicts[i].client->resp.accuracy_hits = g_edicts[i].client->resp.accuracy_misses = 0;
			g_edicts[i].client->resp.heal_given = 0;
		}
}

// "sv resetcount": se vuelve a pedir ready
void Ready_CountReset (void)
{
	if (rd.paused)
		Ready_EndPause ();
	memset (rd.dropped, 0, sizeof(rd.dropped));
	rd.started = false;
	rd.ready[0] = rd.ready[1] = false;
}

// "sv ready [reset]"
void Svcmd_Ready_f (void)
{
	int		t;
	edict_t	*cap;

	if (!Q_stricmp (gi.argv (2), "seguir"))
	{
		if (!rd.paused)
			gi.cprintf (NULL, PRINT_HIGH, "ready: la partida no esta en pausa\n");
		else
			Ready_StartResume ("El admin saco la pausa");
		return;
	}
	if (!Q_stricmp (gi.argv (2), "reset"))
	{
		if (rd.paused)
			Ready_EndPause ();
		Ready_Reset ();
		gi.cprintf (NULL, PRINT_HIGH, "ready: capitanes y ready borrados\n");
		if (Ready_Waiting ())
			Ready_CenterAll ();
		return;
	}

	gi.cprintf (NULL, PRINT_HIGH, "ready_mode %d, %s, %s\n", (int)ready_mode->value,
		Ready_Mode () ? (tournament->value ? "torneo" : "duelo") : "sin duelo ni torneo",
		Ready_Waiting () ? "esperando ready" : (rd.started ? "partida empezada" : "sin esperar"));
	if (rd.paused && rd.pause_team >= 0)
		gi.cprintf (NULL, PRINT_HIGH, "  EN PAUSA (%s, %s), sigue sola en %d s\n",
			Ready_TeamName (rd.pause_team), rd.pause_who, (int)(rd.pause_end - level.time));
	else if (rd.paused && rd.pause_team == READY_PAUSE_REF)
		gi.cprintf (NULL, PRINT_HIGH, "  EN PAUSA del arbitro (%s): la saca el o sv ready seguir\n", rd.pause_who);
	else if (rd.paused)
		gi.cprintf (NULL, PRINT_HIGH, "  EN PAUSA automatica (se cayo %s): sigue cuando vuelva o con sv ready seguir\n", rd.pause_who);
	gi.cprintf (NULL, PRINT_HIGH, "  pause_max %d, pause_time %d, pause_auto %d, ready_remind %d\n", (int)pause_max->value,
		(int)pause_time->value, (int)pause_auto->value, (int)ready_remind->value);
	for (t = 0; t < 2; t++)
	{
		cap = Ready_Captain (t);
		gi.cprintf (NULL, PRINT_HIGH, "  %s: capitan %s, %s, %d humanos, %d kills, %d pausas usadas\n", Ready_TeamName (t),
			cap ? cap->client->pers.netname : "(ninguno)", rd.ready[t] ? "LISTO" : "no listo", Ready_HumansIn (t),
			team_list[t] ? team_list[t]->kills : 0, rd.pauses_used[t]);
	}
}

// menu de equipos: con ready activo se puede entrar como capitan
qboolean Ready_MenuCaptain (void)
{
	return Ready_Waiting ();
}

// chat de equipo de un espectador: solo lo ven los otros espectadores. true si lo mando
qboolean Ready_SpectatorChat (edict_t *ent, const char *text)
{
	int		i;
	edict_t	*e;

	if (!Ready_Mode () || ent->client->resp.team_on)
		return false;

	for (i = 1; i <= game.maxclients; i++)
	{
		e = g_edicts + i;
		if (e->inuse && e->client && !e->ai && !e->client->resp.team_on)
			safe_cprintf (e, PRINT_CHAT, "(Espectadores)%s", text);
	}
	if (dedicated->value)
		safe_cprintf (NULL, PRINT_CHAT, "(Espectadores)%s", text);
	return true;
}

// vote kick: el echado no pausa la partida ni vuelve solo a su equipo
void Ready_Kicked (int clientnum)
{
	rd.kicked = clientnum + 1;
}

// capitan de un equipo (NULL = sin capitan) y equipo de un jugador humano (-1 = ninguno): menu del torneo
edict_t *Ready_CaptainOf (int team)
{
	return (team == 0 || team == 1) ? Ready_Captain (team) : NULL;
}

int Ready_TeamOfPlayer (edict_t *ent)
{
	return Ready_TeamOf (ent);
}

qboolean Ready_IsReady (int team)
{
	return (team == 0 || team == 1) && rd.ready[team];
}

qboolean Ready_WantsResume (int team)
{
	return (team == 0 || team == 1) && rd.resume_ok[team];
}

// la partida esta en juego o en la cuenta (sv torneo fin espera a que termine)
qboolean Ready_MatchInProgress (void)
{
	return !level.intermissiontime && (countdownActive || (rd.started && !level.intermissiontime));
}

qboolean Ready_InPlayNow (void)
{
	return Ready_InPlay ();
}

/*
=================
Acciones del arbitro (menu de g_torneo.c)
=================
*/

// salta el ready de los capitanes: empieza la cuenta
qboolean Ready_RefStart (edict_t *ref)
{
	int minutes = (int)ready_time->value;

	if (!Ready_Waiting ())
		return false;
	if (minutes > 0)
		Ready_Say ("El arbitro (%s) da la partida: empieza (%d minutos).", ref->client->pers.netname, minutes);
	else
		Ready_Say ("El arbitro (%s) da la partida: empieza (hasta que gane un equipo).", ref->client->pers.netname);
	rd.ready[0] = rd.ready[1] = false;
	StartCount (minutes * 60);
	return true;
}

// pausa sin limite de tiempo, no gasta pausas de los equipos
qboolean Ready_RefPause (edict_t *ref)
{
	if (rd.paused || freeze_mode || !Ready_InPlay ())
		return false;
	Ready_BeginPause (READY_PAUSE_REF, ref->client->pers.netname);
	Ready_Say ("PAUSA del arbitro (%s): la partida sigue cuando el la saque.", ref->client->pers.netname);
	return true;
}

// saca cualquier pausa (de un capitan, automatica o suya), con la cuenta de 5 s
qboolean Ready_RefResume (edict_t *ref)
{
	if (!rd.paused || rd.resume_at)
		return false;
	Ready_StartResume (va ("El arbitro (%s) saco la pausa", ref->client->pers.netname));
	return true;
}
