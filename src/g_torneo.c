/*
 * g_torneo.c -- torneo en un paso (sv torneo) y menu "Duelo / Torneo".
 *
 *   sv torneo                          estado y torneos abiertos
 *   sv torneo <clave> [mapa] [min]     activa el torneo: stats_event (clave) + stats_event_name (nombre),
 *                                      tournament 1, stats_mode duel, ready_mode 1, ready_time (los minutos
 *                                      del comando o de la ficha), sin votacion ni bots, y el mapa (queda solo
 *                                      ese en la rotacion)
 *   sv torneo fin                      lo termina y vuelve las cvars a como estaban; con una partida en juego,
 *                                      al terminar esa partida
 *
 * La clave se busca en dday/torneos.txt, que escribe el bot del chat con las fichas abiertas de la web
 * (una linea por torneo: "clave|minutos|nombre", minutos vacio = sin dato, 0 = sin tiempo). Vale la clave
 * exacta (sin importar mayusculas) o un pedazo de la clave o del nombre si calza con un solo torneo.
 *
 * El torneo activo se guarda en dday/torneo_activo.txt (clave, nombre, minutos y los valores previos de
 * las cvars): si el server se reinicia, al cargar el mapa se vuelve a poner solo. Si alguien cambia
 * stats_event a mano, el torneo se da por terminado (sin tocar las cvars).
 *
 * Con un torneo activo, cualquier jugador abre el menu "Duelo / Torneo" (comando menuduelo o el menu
 * principal): capitan, listo, pausa, seguir, cancelar (los dos capitanes) y cambiar de mapa antes de
 * empezar (lo propone un capitan y lo acepta el otro).
 */

#include "g_local.h"
#include <ctype.h>
#include <stdarg.h>

#define TORNEO_LIST_FILE	"torneos.txt"
#define TORNEO_ACTIVE_FILE	"torneo_activo.txt"
#define TORNEO_MAX			32
#define TORNEO_MAPS			10
#define TORNEO_PROP_SECS	60
#define TORNEO_MENU_ROWS	24
#define TORNEO_MENU_TOP		4		// primera fila (como el menu de equipos: dentro del portapapeles)
#define TORNEO_REF_TRIES	3		// claves malas de "arbitro" antes de bloquearlo (hasta que se reconecte)
#define TORNEO_LIST_ROWS	10		// torneos que caben en el menu

extern int		countdownActive;

FILE *Game_OpenFile (char *file, char *mode);
qboolean Cfg_Value (const char *name, char *out, int size);
void PMenu_Open (edict_t *ent, pmenu_t *entries, int cur, int num);
void client_menu (edict_t *ent, int entry, char *text, int align, void *arg,
				  void (*SelectFunc)(edict_t *ent, struct pmenu_s *entry, int choice));
void Cmd_Pause_f (edict_t *ent);
void Cmd_Resume_f (edict_t *ent);
void Cmd_Cancel_f (edict_t *ent);
void Cmd_Ready_f (edict_t *ent);
void Cmd_NotReady_f (edict_t *ent);
void Svcmd_ResetCountdown_f (void);
qboolean Ready_RefStart (edict_t *ref);
qboolean Ready_RefPause (edict_t *ref);
qboolean Ready_RefResume (edict_t *ref);

typedef struct
{
	char	key[64];
	int		minutes;		// -1 = sin dato
	char	name[64];
} torneo_t;

// cvars que cambia el torneo; sus valores previos se guardan para "sv torneo fin"
static const char *torneo_cvars[] =
{
	"stats_event", "stats_event_name", "tournament", "stats_mode", "ready_mode", "ready_time",
	"mapvoting", "bots", "sv_maplist", NULL
};
#define TORNEO_NCVARS	9

static struct
{
	qboolean	active;
	char		key[64];
	char		name[64];
	int			minutes;
	char		map[32];			// mapa del torneo (se vuelve a el tras un reinicio)
	char		prev[TORNEO_NCVARS][256];
	qboolean	end_pending;		// "sv torneo fin" con una partida en juego
	qboolean	checked;			// ya se miro torneo_activo.txt en este mapa
	float		next_check;

	// cambio de mapa propuesto por un capitan
	char		prop_map[32];
	int			prop_team;
	float		prop_time;
} tn;

static torneo_t	tn_list[TORNEO_MAX];
static int		tn_count;

static char		tn_maps[TORNEO_MAPS][32];
static int		tn_nmaps;

// arbitros: el que escribio "arbitro <clave rcon>" en la consola (dura hasta que se desconecta)
enum { REF_NONE, REF_RESET, REF_END, REF_MAP };
static struct
{
	qboolean	on;
	int			fails;
	int			confirm;		// accion que espera "Si" (REF_*)
	qboolean	set_minutes;	// el menu de minutos cambia los del torneo activo (no activa uno)
	char		key[64];		// torneo elegido en el menu (activar)
	char		map[32];		// mapa elegido ("" = el actual)
} tn_ref[MAX_CLIENTS];

// textos del menu de cada jugador (el menu guarda punteros)
static char		tn_text[MAX_CLIENTS][TORNEO_MENU_ROWS][32];

static cvar_t	*stats_event_name;

// a quien van las respuestas: el arbitro que usa el menu, o la consola del server (sv torneo)
static edict_t	*tn_reply;

static void Torneo_Print (const char *fmt, ...)
{
	char	msg[1024];
	va_list	ap;

	va_start (ap, fmt);
	vsnprintf (msg, sizeof(msg), fmt, ap);
	va_end (ap);
	if (tn_reply && tn_reply->inuse && tn_reply->client)
		safe_cprintf (tn_reply, PRINT_HIGH, "%s", msg);
	else
		gi.cprintf (NULL, PRINT_HIGH, "%s", msg);
}

void Torneo_Init (void)
{
	stats_event_name = gi.cvar ("stats_event_name", "", 0);
}

static cvar_t *Torneo_Cvar (const char *name)
{
	return gi.cvar ((char *)name, "", 0);
}

qboolean Torneo_Active (void)
{
	return tn.active;
}

// nombre a mostrar: el del torneo activo, o el de un evento puesto a mano ("" = sin evento)
const char *Torneo_Name (void)
{
	cvar_t *ev;

	if (tn.active)
		return tn.name[0] ? tn.name : tn.key;
	if (stats_event_name && stats_event_name->string[0])
		return stats_event_name->string;
	ev = gi.cvar ("stats_event", "", 0);
	return ev->string;
}

/*
=================
Archivos
=================
*/

// minusculas y solo letras y numeros, para comparar ("Clanes-Amigos" ~ "clanes amigos")
static void Torneo_Norm (const char *in, char *out, int size)
{
	int n = 0;

	for (; *in && n < size - 1; in++)
		if (isalnum ((unsigned char)*in))
			out[n++] = tolower ((unsigned char)*in);
	out[n] = '\0';
}

static void Torneo_Trim (char *s)
{
	int n = strlen (s);

	while (n > 0 && (s[n - 1] == '\r' || s[n - 1] == '\n' || s[n - 1] == ' ' || s[n - 1] == '\t'))
		s[--n] = '\0';
}

// dday/torneos.txt -> tn_list; false si no existe
static qboolean Torneo_LoadList (void)
{
	char		line[256], *buf, *p, *a, *b;
	int			len;
	torneo_t	*t;

	tn_count = 0;
	buf = ReadEntFile (TORNEO_LIST_FILE);
	if (!buf)
		return false;

	for (p = buf; *p && tn_count < TORNEO_MAX; )
	{
		for (len = 0; p[len] && p[len] != '\n'; len++)
			;
		Com_sprintf (line, sizeof(line), "%.*s", len < (int)sizeof(line) - 1 ? len : (int)sizeof(line) - 1, p);
		p += len;
		if (*p)
			p++;
		Torneo_Trim (line);
		if (!line[0] || line[0] == '#')
			continue;
		if (!(a = strchr (line, '|')) || !(b = strchr (a + 1, '|')))
			continue;
		*a = *b = '\0';
		if (!line[0])
			continue;
		t = &tn_list[tn_count++];
		Com_sprintf (t->key, sizeof(t->key), "%s", line);
		t->minutes = (a[1] >= '0' && a[1] <= '9') ? atoi (a + 1) : -1;
		Com_sprintf (t->name, sizeof(t->name), "%s", b + 1);
	}
	gi.TagFree (buf);
	return true;
}

// busca un torneo por clave exacta o por un pedazo de clave o nombre; NULL si no hay o hay varios (*many)
static torneo_t *Torneo_Find (const char *query, int *many)
{
	char		q[64], k[64], nm[64];
	torneo_t	*found = NULL;
	int			i;

	*many = 0;
	for (i = 0; i < tn_count; i++)
		if (!Q_stricmp (tn_list[i].key, (char *)query))
			return &tn_list[i];

	Torneo_Norm (query, q, sizeof(q));
	if (strlen (q) < 3)
		return NULL;
	for (i = 0; i < tn_count; i++)
	{
		Torneo_Norm (tn_list[i].key, k, sizeof(k));
		Torneo_Norm (tn_list[i].name, nm, sizeof(nm));
		if (strstr (k, q) || strstr (nm, q))
		{
			(*many)++;
			found = &tn_list[i];
		}
	}
	return *many == 1 ? found : NULL;
}

static void Torneo_PrintList (void)
{
	int i;

	if (!tn_count)
	{
		Torneo_Print ("  (no hay torneos abiertos en dday/" TORNEO_LIST_FILE "; el bot la baja de la web)\n");
		return;
	}
	for (i = 0; i < tn_count; i++)
	{
		if (tn_list[i].minutes > 0)
			Torneo_Print ("  %s - %s (%d min)\n", tn_list[i].key, tn_list[i].name, tn_list[i].minutes);
		else if (tn_list[i].minutes == 0)
			Torneo_Print ("  %s - %s (sin tiempo)\n", tn_list[i].key, tn_list[i].name);
		else
			Torneo_Print ("  %s - %s\n", tn_list[i].key, tn_list[i].name);
	}
}

// guarda el torneo activo (o lo borra dejando el archivo vacio)
static void Torneo_Save (void)
{
	FILE	*f = Game_OpenFile (TORNEO_ACTIVE_FILE, "w");
	int		i;

	if (!f)
	{
		gi.dprintf ("torneo: no se pudo escribir dday/" TORNEO_ACTIVE_FILE "\n");
		return;
	}
	if (tn.active)
	{
		fprintf (f, "clave=%s\nnombre=%s\nminutos=%d\nmapa=%s\n", tn.key, tn.name, tn.minutes, tn.map);
		for (i = 0; torneo_cvars[i]; i++)
			fprintf (f, "prev_%s=%s\n", torneo_cvars[i], tn.prev[i]);
	}
	fclose (f);
}

// lee dday/torneo_activo.txt; true si hay un torneo guardado
static qboolean Torneo_LoadActive (void)
{
	char	line[512], *buf, *p, *v;
	int		len, i;

	buf = ReadEntFile (TORNEO_ACTIVE_FILE);
	if (!buf)
		return false;

	memset (&tn, 0, sizeof(tn));
	tn.minutes = -1;
	for (p = buf; *p; )
	{
		for (len = 0; p[len] && p[len] != '\n'; len++)
			;
		Com_sprintf (line, sizeof(line), "%.*s", len < (int)sizeof(line) - 1 ? len : (int)sizeof(line) - 1, p);
		p += len;
		if (*p)
			p++;
		Torneo_Trim (line);
		if (!(v = strchr (line, '=')))
			continue;
		*v++ = '\0';
		if (!strcmp (line, "clave"))
			Com_sprintf (tn.key, sizeof(tn.key), "%s", v);
		else if (!strcmp (line, "nombre"))
			Com_sprintf (tn.name, sizeof(tn.name), "%s", v);
		else if (!strcmp (line, "minutos"))
			tn.minutes = atoi (v);
		else if (!strcmp (line, "mapa"))
			Com_sprintf (tn.map, sizeof(tn.map), "%s", v);
		else if (!strncmp (line, "prev_", 5))
			for (i = 0; torneo_cvars[i]; i++)
				if (!strcmp (line + 5, torneo_cvars[i]))
					Com_sprintf (tn.prev[i], sizeof(tn.prev[i]), "%s", v);
	}
	gi.TagFree (buf);
	tn.active = tn.key[0] != '\0';
	return tn.active;
}

/*
=================
Activar y terminar
=================
*/

// pone las cvars del torneo activo (sin cambiar de mapa)
static void Torneo_SetCvars (const char *map)
{
	gi.cvar_forceset ("stats_event", tn.key);
	gi.cvar_forceset ("stats_event_name", tn.name);
	gi.cvar_forceset ("tournament", "1");
	gi.cvar_forceset ("stats_mode", "duel");
	gi.cvar_forceset ("ready_mode", "1");
	if (tn.minutes >= 0)
		gi.cvar_forceset ("ready_time", va ("%d", tn.minutes));
	gi.cvar_forceset ("mapvoting", "0");
	gi.cvar_forceset ("bots", "0");
	gi.cvar_forceset ("sv_maplist", (char *)map);
}

static void Torneo_Announce (void)
{
	char	mins[32];
	int		m;

	// sin minutos en la ficha: los del server (ready_time)
	m = (tn.minutes >= 0) ? tn.minutes : (int)Torneo_Cvar ("ready_time")->value;
	if (m > 0)
		Com_sprintf (mins, sizeof(mins), "%d min", m);
	else
		Com_sprintf (mins, sizeof(mins), "sin tiempo");

	safe_bprintf (PRINT_HIGH, "TORNEO %s: partidas OFICIALES (%s). Capitanes: menu Duelo/Torneo (menuduelo) o escriban ready.\n",
		Torneo_Name (), mins);
	centerprintall ("TORNEO\n%.33s\n\npartidas OFICIALES (%s)\n\nCapitanes: menu Duelo/Torneo\n(menuduelo) o escriban ready", Torneo_Name (), mins);
}

static void Torneo_Start (const char *query, const char *map_arg, const char *min_arg)
{
	torneo_t	*t;
	int			many, i;
	char		map[64];
	qboolean	reload;

	Torneo_LoadList ();
	t = Torneo_Find (query, &many);
	if (!t)
	{
		if (many > 1)
			Torneo_Print ("torneo: '%s' calza con %d torneos, escribe la clave:\n", query, many);
		else
			Torneo_Print ("torneo: no hay un torneo abierto '%s'. Torneos abiertos:\n", query);
		Torneo_PrintList ();
		return;
	}
	if (min_arg && min_arg[0] && (!isdigit ((unsigned char)min_arg[0]) || atoi (min_arg) > 180))
	{
		Torneo_Print ("torneo: minutos invalidos '%s' (0 = sin tiempo, maximo 180)\n", min_arg);
		return;
	}
	if (map_arg && map_arg[0] && !Vote_MapPlayable (map_arg))
	{
		Torneo_Print ("torneo: el mapa '%s' no esta en el server o no es de D-Day\n", map_arg);
		return;
	}
	// una partida en juego quedaria a medias (sin evento al empezar): se cancela antes o se da un mapa
	if (Ready_MatchInProgress () && !(map_arg && map_arg[0]))
	{
		Torneo_Print ("torneo: hay una partida en juego; que termine, cancelala (sv resetcount) o da un mapa\n");
		return;
	}

	Com_sprintf (map, sizeof(map), "%s", (map_arg && map_arg[0]) ? map_arg : level.mapname);
	// stats_mode se lee al empezar el mapa: si no estaba en duelo hay que recargarlo
	reload = (map_arg && map_arg[0]) || Q_stricmp (Torneo_Cvar ("stats_mode")->string, "duel");

	// los valores previos solo la primera vez (cambiar de torneo no los pisa)
	if (!tn.active)
		for (i = 0; torneo_cvars[i]; i++)
			Com_sprintf (tn.prev[i], sizeof(tn.prev[i]), "%s", Torneo_Cvar (torneo_cvars[i])->string);

	tn.active = true;
	tn.end_pending = false;
	tn.checked = true;
	Com_sprintf (tn.key, sizeof(tn.key), "%s", t->key);
	Com_sprintf (tn.name, sizeof(tn.name), "%s", t->name);
	tn.minutes = (min_arg && min_arg[0]) ? atoi (min_arg) : t->minutes;
	tn.prop_map[0] = '\0';
	Com_sprintf (tn.map, sizeof(tn.map), "%s", map);

	Torneo_SetCvars (map);
	Torneo_Save ();
	Torneo_Print ("torneo: %s (%s) activo en %s\n", tn.name, tn.key, map);
	Torneo_Announce ();
	if (reload)
		gi.AddCommandString (va ("map %s\n", map));
}

static void Torneo_EndNow (void)
{
	int i;

	for (i = 0; torneo_cvars[i]; i++)
		gi.cvar_forceset ((char *)torneo_cvars[i], tn.prev[i]);
	safe_bprintf (PRINT_HIGH, "Termino el torneo %s: el server vuelve a lo normal.\n", Torneo_Name ());
	Torneo_Print ("torneo: terminado, cvars como estaban\n");
	memset (&tn, 0, sizeof(tn));
	tn.checked = true;
	Torneo_Save ();
}

static void Torneo_End (void)
{
	if (!tn.active)
	{
		Torneo_Print ("torneo: no hay un torneo activo\n");
		return;
	}
	if (Ready_MatchInProgress ())
	{
		tn.end_pending = true;
		safe_bprintf (PRINT_HIGH, "El torneo %s termina al acabar esta partida.\n", Torneo_Name ());
		Torneo_Print ("torneo: termina al acabar la partida en juego\n");
		return;
	}
	Torneo_EndNow ();
}

// "sv torneo [clave [mapa] [minutos] | fin]"
void Svcmd_Torneo_f (void)
{
	char *a = gi.argv (2);

	if (!a[0])
	{
		Torneo_LoadList ();
		if (tn.active)
			Torneo_Print ("Torneo activo: %s (%s), %s, mapa %s%s\n", tn.name, tn.key,
				tn.minutes > 0 ? va ("%d min", tn.minutes) : (tn.minutes == 0 ? "sin tiempo" : "minutos del server"),
				level.mapname, tn.end_pending ? ", termina al acabar la partida" : "");
		else
			Torneo_Print ("Sin torneo activo. Uso: sv torneo <clave> [mapa] [minutos] | sv torneo fin\n");
		Torneo_Print ("Torneos abiertos:\n");
		Torneo_PrintList ();
		return;
	}
	if (!Q_stricmp (a, "fin") || !Q_stricmp (a, "off") || !Q_stricmp (a, "terminar"))
	{
		Torneo_End ();
		return;
	}
	// "sv torneo clave 25" (sin mapa): el segundo argumento numerico son los minutos
	if (isdigit ((unsigned char)gi.argv (3)[0]) && !gi.argv (4)[0])
		Torneo_Start (a, "", gi.argv (3));
	else
		Torneo_Start (a, gi.argv (3), gi.argv (4));
}

/*
=================
Cada frame
=================
*/

// al cargar cada mapa
void Torneo_LevelStart (void)
{
	tn.checked = false;
	tn.prop_map[0] = '\0';
}

void Torneo_RunFrame (void)
{
	cvar_t *ev;

	// primer frame del mapa: torneo guardado tras un reinicio del server
	if (!tn.checked)
	{
		tn.checked = true;
		if (!tn.active && Torneo_LoadActive ())
		{
			qboolean	other_map = tn.map[0] && Q_stricmp (tn.map, level.mapname) && Vote_MapPlayable (tn.map);
			qboolean	reload = other_map || Q_stricmp (Torneo_Cvar ("stats_mode")->string, "duel");

			if (!tn.map[0] || !other_map)
				Com_sprintf (tn.map, sizeof(tn.map), "%s", level.mapname);
			Torneo_SetCvars (tn.map);
			safe_bprintf (PRINT_HIGH, "Se reinicio el server: el torneo %s sigue activo.\n", Torneo_Name ());
			gi.dprintf ("torneo: restaurado %s tras el reinicio (mapa %s)\n", tn.key, tn.map);
			// stats_mode se lee al empezar el mapa, y el server.cfg pudo cargar otro mapa
			if (reload)
				gi.AddCommandString (va ("map %s\n", tn.map));
		}
		return;
	}

	if (!tn.active || level.time < tn.next_check)
		return;
	tn.next_check = level.time + 1;

	// alguien cambio el evento a mano: el torneo se da por terminado (sin tocar las cvars)
	ev = Torneo_Cvar ("stats_event");
	if (Q_stricmp (ev->string, tn.key))
	{
		gi.dprintf ("torneo: stats_event cambiado a mano, el torneo %s queda terminado\n", tn.key);
		memset (&tn, 0, sizeof(tn));
		tn.checked = true;
		Torneo_Save ();
		return;
	}

	if (tn.end_pending && (level.intermissiontime || !Ready_MatchInProgress ()))
		Torneo_EndNow ();
}

/*
=================
Menu "Duelo / Torneo"
=================
*/

static int Torneo_Humans (int team)
{
	int i, n = 0;

	for (i = 1; i <= game.maxclients; i++)
		if (Ready_TeamOfPlayer (g_edicts + i) == team)
			n++;
	return n;
}

// mapas para cambiar: la rotacion del server.cfg (la del torneo es solo el mapa actual)
static void Torneo_LoadMaps (void)
{
	char	list[512], *p, *s;

	tn_nmaps = 0;
	if (!Cfg_Value ("sv_maplist", list, sizeof(list)))
		return;
	for (p = list; *p && tn_nmaps < TORNEO_MAPS; )
	{
		while (*p == ' ' || *p == ',')
			p++;
		s = p;
		while (*p && *p != ' ' && *p != ',')
			p++;
		if (p > s && p - s < 32)
		{
			Com_sprintf (tn_maps[tn_nmaps], sizeof(tn_maps[0]), "%.*s", (int)(p - s), s);
			if (Q_stricmp (tn_maps[tn_nmaps], level.mapname) && Vote_MapPlayable (tn_maps[tn_nmaps]))
				tn_nmaps++;
		}
	}
}

static char *Torneo_Row (edict_t *ent, int row, const char *fmt, ...)
{
	char	*out = tn_text[ent - g_edicts - 1][row];
	va_list	ap;

	va_start (ap, fmt);
	vsnprintf (out, sizeof(tn_text[0][0]), fmt, ap);
	va_end (ap);
	return out;
}

static void Torneo_ClearMenu (edict_t *ent)
{
	memset (ent->client->menu_cur, 0, sizeof(ent->client->menu_cur));
}

static void Torneo_ChangeMap (const char *map_in, const char *why)
{
	char map[32];

	Com_sprintf (map, sizeof(map), "%s", map_in);	// puede ser tn.prop_map, que se borra
	safe_bprintf (PRINT_HIGH, "%s: se cambia a %s.\n", why, map);
	gi.cvar_forceset ("sv_maplist", map);
	tn.prop_map[0] = '\0';
	Com_sprintf (tn.map, sizeof(tn.map), "%s", map);
	Torneo_Save ();
	gi.AddCommandString (va ("map %s\n", map));
}

static qboolean Torneo_PropFresh (void)
{
	return tn.prop_map[0] && level.time - tn.prop_time < TORNEO_PROP_SECS;
}

// el jugador actua como capitan de su equipo (si no hay, queda de capitan); false si es otro
static qboolean Torneo_AsCaptain (edict_t *ent, int team)
{
	edict_t *cap = Ready_CaptainOf (team);

	if (cap == ent)
		return true;
	if (cap)
	{
		safe_cprintf (ent, PRINT_HIGH, "Eso lo hace el capitan de tu equipo (%s).\n", cap->client->pers.netname);
		return false;
	}
	Ready_ClaimCaptain (ent);
	return Ready_CaptainOf (team) == ent;
}

void Torneo_OpenMenu (edict_t *ent);
static void Torneo_RefMenu (edict_t *ent);
static void M_Ref_Open (edict_t *ent, pmenu_t *p, int choice)	{ PMenu_Close (ent); Torneo_RefMenu (ent); }

static qboolean Torneo_IsRef (edict_t *ent)
{
	int i = ent - g_edicts - 1;

	return ent->client && i >= 0 && i < MAX_CLIENTS && tn_ref[i].on;
}

static void M_Tn_Close (edict_t *ent, pmenu_t *p, int choice)	{ PMenu_Close (ent); }
static void M_Tn_Captain (edict_t *ent, pmenu_t *p, int choice)	{ PMenu_Close (ent); Ready_ClaimCaptain (ent); Torneo_OpenMenu (ent); }
static void M_Tn_Ready (edict_t *ent, pmenu_t *p, int choice)	{ PMenu_Close (ent); Cmd_Ready_f (ent); Torneo_OpenMenu (ent); }
static void M_Tn_NotReady (edict_t *ent, pmenu_t *p, int choice)	{ PMenu_Close (ent); Cmd_NotReady_f (ent); Torneo_OpenMenu (ent); }
static void M_Tn_Pause (edict_t *ent, pmenu_t *p, int choice)	{ PMenu_Close (ent); Cmd_Pause_f (ent); }
static void M_Tn_Resume (edict_t *ent, pmenu_t *p, int choice)	{ PMenu_Close (ent); Cmd_Resume_f (ent); }
static void M_Tn_Cancel (edict_t *ent, pmenu_t *p, int choice)	{ PMenu_Close (ent); Cmd_Cancel_f (ent); }

static void M_Tn_Back (edict_t *ent, pmenu_t *p, int choice)	{ PMenu_Close (ent); Torneo_OpenMenu (ent); }

static void M_Tn_MapPick (edict_t *ent, pmenu_t *p, int choice)
{
	const char	*map = (const char *)p->arg;	// antes de cerrar: PMenu_Close limpia las filas
	int			team = Ready_TeamOfPlayer (ent);
	char		why[64];

	PMenu_Close (ent);
	if (!map || team < 0 || !Ready_Waiting () || !Torneo_AsCaptain (ent, team))
		return;
	// el otro equipo sin nadie: no hay a quien preguntarle
	if (!Torneo_Humans (!team))
	{
		Com_sprintf (why, sizeof(why), "%s (%s)", team_list[team]->teamname, ent->client->pers.netname);
		Torneo_ChangeMap (map, why);
		return;
	}
	Com_sprintf (tn.prop_map, sizeof(tn.prop_map), "%s", map);
	tn.prop_team = team;
	tn.prop_time = level.time;
	safe_bprintf (PRINT_HIGH, "%s (%s) propone cambiar a %s. Capitan de %s: aceptalo en el menu Duelo/Torneo (menuduelo) en %d s.\n",
		team_list[team]->teamname, ent->client->pers.netname, map, team_list[!team]->teamname, TORNEO_PROP_SECS);
}

static void M_Tn_Accept (edict_t *ent, pmenu_t *p, int choice)
{
	int team = Ready_TeamOfPlayer (ent);

	PMenu_Close (ent);
	if (team < 0 || !Torneo_PropFresh () || tn.prop_team == team || !Ready_Waiting () || !Torneo_AsCaptain (ent, team))
		return;
	Torneo_ChangeMap (tn.prop_map, "Los dos capitanes aceptaron");
}

static void M_Tn_Maps (edict_t *ent, pmenu_t *p, int choice)
{
	int i, row = TORNEO_MENU_TOP;

	PMenu_Close (ent);
	Torneo_LoadMaps ();
	Torneo_ClearMenu (ent);
	client_menu (ent, row++, "*Cambiar mapa", PMENU_ALIGN_CENTER, NULL, NULL);
	client_menu (ent, row++, "el otro capitan acepta", PMENU_ALIGN_CENTER, NULL, NULL);
	row++;
	for (i = 0; i < tn_nmaps; i++)
		client_menu (ent, row++, tn_maps[i], PMENU_ALIGN_CENTER, tn_maps[i], M_Tn_MapPick);
	if (!tn_nmaps)
		client_menu (ent, row++, "(no hay otros mapas)", PMENU_ALIGN_CENTER, NULL, NULL);
	row++;
	client_menu (ent, row, "Volver", PMENU_ALIGN_CENTER, NULL, M_Tn_Back);
	client_menu (ent, row + 2, "*[ y ] mover, Enter elegir", PMENU_ALIGN_CENTER, NULL, NULL);
	PMenu_Open (ent, ent->client->menu_cur, TORNEO_MENU_TOP + 3, MENU_MAX);
}

static const char *Torneo_TeamState (int t)
{
	if (Ready_Paused ())
		return Ready_WantsResume (t) ? "quiere seguir" : "en pausa";
	if (Ready_Started () || countdownActive)
		return "en juego";
	if (Ready_IsReady (t))
		return "LISTO";
	return Ready_CaptainOf (t) ? "no listo" : "sin capitan";
}

void Torneo_OpenMenu (edict_t *ent)
{
	int			row = TORNEO_MENU_TOP, team, first, t;
	edict_t		*cap;
	qboolean	act, waiting = Ready_Waiting ();

	if (!ent->client || ent->ai)
		return;
	if (!tn.active)
	{
		if (Torneo_IsRef (ent))
			Torneo_RefMenu (ent);
		else
			safe_cprintf (ent, PRINT_HIGH, "El menu Duelo/Torneo esta solo con un torneo activo.\n");
		return;
	}
	if (ent->client->menu)
		PMenu_Close (ent);

	team = Ready_TeamOfPlayer (ent);
	cap = team >= 0 ? Ready_CaptainOf (team) : NULL;
	act = team >= 0 && (!cap || cap == ent);		// puede actuar como capitan

	// mismo estilo que el menu de equipos: titulo, nombre, estado y grupos separados por una linea
	Torneo_ClearMenu (ent);
	client_menu (ent, row++, "*Duelo / Torneo", PMENU_ALIGN_CENTER, NULL, NULL);
	client_menu (ent, row, Torneo_Row (ent, row, "%.24s", tn.name[0] ? tn.name : tn.key), PMENU_ALIGN_CENTER, NULL, NULL);
	row += 2;
	client_menu (ent, row++, level.intermissiontime ? "Partida terminada" : Ready_Paused () ? "*EN PAUSA" :
		countdownActive ? "Cuenta regresiva" : Ready_Started () ? "En juego" : "Calentamiento", PMENU_ALIGN_CENTER, NULL, NULL);
	row++;
	for (t = 0; t < 2; t++)
	{
		edict_t *c = Ready_CaptainOf (t);

		client_menu (ent, row, Torneo_Row (ent, row, "%7.7s: %s", team_list[t]->teamname, Torneo_TeamState (t)), PMENU_ALIGN_LEFT, NULL, NULL);
		row++;
		if (c)
		{
			client_menu (ent, row, Torneo_Row (ent, row, "   capitan %.13s", c->client->pers.netname), PMENU_ALIGN_LEFT, NULL, NULL);
			row++;
		}
	}
	row++;
	first = row;

	if (team < 0)
	{
		client_menu (ent, row++, "Entra a un equipo", PMENU_ALIGN_CENTER, NULL, NULL);
		client_menu (ent, row++, "para jugar el torneo", PMENU_ALIGN_CENTER, NULL, NULL);
		row++;
	}
	else if (!act)
	{
		client_menu (ent, row++, "Tu capitan maneja", PMENU_ALIGN_CENTER, NULL, NULL);
		client_menu (ent, row++, "el equipo", PMENU_ALIGN_CENTER, NULL, NULL);
		row++;
	}
	else if (!level.intermissiontime)
	{
		int start = row;

		if (waiting)
		{
			if (!cap)
				client_menu (ent, row++, "Ser capitan", PMENU_ALIGN_CENTER, NULL, M_Tn_Captain);
			if (Ready_IsReady (team))
				client_menu (ent, row++, "No estamos listos", PMENU_ALIGN_CENTER, NULL, M_Tn_NotReady);
			else
				client_menu (ent, row++, "Listos (ready)", PMENU_ALIGN_CENTER, NULL, M_Tn_Ready);
			if (Torneo_PropFresh () && tn.prop_team != team)
			{
				client_menu (ent, row, Torneo_Row (ent, row, "Aceptar %.15s", tn.prop_map), PMENU_ALIGN_CENTER, NULL, M_Tn_Accept);
				row++;
			}
			client_menu (ent, row++, "Cambiar mapa...", PMENU_ALIGN_CENTER, NULL, M_Tn_Maps);
		}
		if (Ready_InPlayNow ())
		{
			if (!Ready_Paused ())
				client_menu (ent, row++, "Pedir pausa", PMENU_ALIGN_CENTER, NULL, M_Tn_Pause);
			else if (!Ready_WantsResume (team))
				client_menu (ent, row++, "Seguir", PMENU_ALIGN_CENTER, NULL, M_Tn_Resume);
		}
		if (Ready_Started () || countdownActive)
			client_menu (ent, row++, "Cancelar partida", PMENU_ALIGN_CENTER, NULL, M_Tn_Cancel);
		if (row > start)
			row++;
	}

	if (Torneo_IsRef (ent))
		client_menu (ent, row++, "Arbitro...", PMENU_ALIGN_CENTER, NULL, M_Ref_Open);
	client_menu (ent, row, "Cerrar", PMENU_ALIGN_CENTER, NULL, M_Tn_Close);
	client_menu (ent, row + 2, "*[ y ] mover, Enter elegir", PMENU_ALIGN_CENTER, NULL, NULL);
	PMenu_Open (ent, ent->client->menu_cur, first, MENU_MAX);
}

/*
=================
Arbitro: "arbitro <clave rcon>" en la consola y su menu
=================
*/

#define REF(e)	(tn_ref[(e) - g_edicts - 1])

// el menu "Duelo / Torneo" aparece (menu principal y de equipos): torneo activo o arbitro
qboolean Torneo_MenuAvailable (edict_t *ent)
{
	return tn.active || Torneo_IsRef (ent);
}

void Torneo_ClientDisconnect (edict_t *ent)
{
	int i = ent - g_edicts - 1;

	if (i >= 0 && i < MAX_CLIENTS)
		memset (&tn_ref[i], 0, sizeof(tn_ref[i]));
}

static const char *Torneo_RconPassword (void)
{
	return gi.cvar ("rcon_password", "", 0)->string;
}

// la clave no tiene que salir en el chat: "arbitro ..." o un mensaje con la clave rcon no se manda
qboolean Torneo_ChatGuard (edict_t *ent, const char *text)
{
	const char	*pw = Torneo_RconPassword (), *t = text;
	char		word[16];
	int			n = 0;

	if (!text)
		return false;
	while (*t == '"' || *t == ' ' || *t == '!')
		t++;
	while (*t && *t != ' ' && *t != '"' && n < (int)sizeof(word) - 1)
		word[n++] = tolower ((unsigned char)*t++);
	word[n] = '\0';
	if (!strcmp (word, "arbitro") || (pw[0] && strstr (text, pw)))
	{
		safe_cprintf (ent, PRINT_HIGH, "No se mando: la clave del arbitro va en la consola (tecla ~), no en el chat.\n"
			"Escribe en la consola: arbitro <clave rcon>\n");
		return true;
	}
	return false;
}

// "arbitro": abre su menu; "arbitro <clave>": queda de arbitro; "arbitro salir": deja de serlo
void Cmd_Arbitro_f (edict_t *ent)
{
	const char	*pw = Torneo_RconPassword ();
	char		*a = gi.argv (1);

	if (!ent->client || ent->ai)
		return;
	if (!a[0])
	{
		if (Torneo_IsRef (ent))
			Torneo_RefMenu (ent);
		else
			safe_cprintf (ent, PRINT_HIGH, "Uso (en la consola): arbitro <clave rcon>. Despues: arbitro (abre el menu) o arbitro salir.\n");
		return;
	}
	if (!Q_stricmp (a, "salir") || !Q_stricmp (a, "fin"))
	{
		if (Torneo_IsRef (ent))
		{
			REF (ent).on = false;
			safe_bprintf (PRINT_HIGH, "%s ya no es arbitro.\n", ent->client->pers.netname);
		}
		return;
	}
	if (Torneo_IsRef (ent))
	{
		Torneo_RefMenu (ent);
		return;
	}
	if (!pw[0])
	{
		safe_cprintf (ent, PRINT_HIGH, "El server no tiene clave rcon: no hay arbitros.\n");
		return;
	}
	if (REF (ent).fails >= TORNEO_REF_TRIES)
	{
		safe_cprintf (ent, PRINT_HIGH, "Demasiadas claves malas: reconectate para intentar de nuevo.\n");
		return;
	}
	if (strcmp (a, pw))
	{
		REF (ent).fails++;
		gi.dprintf ("arbitro: clave mala de %s (%s), intento %d\n", ent->client->pers.netname,
			Info_ValueForKey (ent->client->pers.userinfo, "ip"), REF (ent).fails);
		safe_cprintf (ent, PRINT_HIGH, "Clave incorrecta (%d de %d intentos).\n", REF (ent).fails, TORNEO_REF_TRIES);
		return;
	}
	REF (ent).on = true;
	REF (ent).fails = 0;
	gi.dprintf ("arbitro: %s (%s)\n", ent->client->pers.netname, Info_ValueForKey (ent->client->pers.userinfo, "ip"));
	safe_bprintf (PRINT_HIGH, "%s es arbitro.\n", ent->client->pers.netname);
	Torneo_RefMenu (ent);
}

static const char *Torneo_MinutesText (int m)
{
	if (m < 0)
		m = (int)Torneo_Cvar ("ready_time")->value;
	return m > 0 ? va ("%d min", m) : "sin tiempo";
}

static void Torneo_RefFoot (edict_t *ent, int row)
{
	client_menu (ent, row + 2, "*[ y ] mover, Enter elegir", PMENU_ALIGN_CENTER, NULL, NULL);
}

// --- elegir torneo -> mapa -> minutos (activar o cambiar de torneo)

static void M_Ref_Minutes (edict_t *ent, pmenu_t *p, int choice);
static void Torneo_RefMinutesMenu (edict_t *ent);

static void M_Ref_MapPickStart (edict_t *ent, pmenu_t *p, int choice)
{
	const char *map = (const char *)p->arg;

	PMenu_Close (ent);
	Com_sprintf (REF (ent).map, sizeof(REF (ent).map), "%s", map ? map : "");
	REF (ent).set_minutes = false;
	Torneo_RefMinutesMenu (ent);
}

static void Torneo_RefMapStartMenu (edict_t *ent)
{
	int row = TORNEO_MENU_TOP, i;

	Torneo_LoadMaps ();
	Torneo_ClearMenu (ent);
	client_menu (ent, row++, "*Mapa del torneo", PMENU_ALIGN_CENTER, NULL, NULL);
	client_menu (ent, row, Torneo_Row (ent, row, "%.24s", REF (ent).key), PMENU_ALIGN_CENTER, NULL, NULL);
	row += 2;
	client_menu (ent, row, Torneo_Row (ent, row, "Este mapa (%.12s)", level.mapname), PMENU_ALIGN_CENTER, NULL, M_Ref_MapPickStart);
	row++;
	for (i = 0; i < tn_nmaps; i++)
		client_menu (ent, row++, tn_maps[i], PMENU_ALIGN_CENTER, tn_maps[i], M_Ref_MapPickStart);
	row++;
	client_menu (ent, row, "Volver", PMENU_ALIGN_CENTER, NULL, M_Ref_Open);
	Torneo_RefFoot (ent, row);
	PMenu_Open (ent, ent->client->menu_cur, TORNEO_MENU_TOP + 3, MENU_MAX);
}

static void M_Ref_TorneoPick (edict_t *ent, pmenu_t *p, int choice)
{
	const char *key = (const char *)p->arg;

	PMenu_Close (ent);
	if (!key)
		return;
	Com_sprintf (REF (ent).key, sizeof(REF (ent).key), "%s", key);
	Torneo_RefMapStartMenu (ent);
}

static void M_Ref_List (edict_t *ent, pmenu_t *p, int choice)
{
	int row = TORNEO_MENU_TOP, i;

	PMenu_Close (ent);
	if (Ready_MatchInProgress ())
	{
		safe_cprintf (ent, PRINT_HIGH, "Hay una partida en juego: reiniciala o espera que termine para cambiar de torneo.\n");
		Torneo_RefMenu (ent);
		return;
	}
	Torneo_LoadList ();
	Torneo_ClearMenu (ent);
	client_menu (ent, row++, tn.active ? "*Cambiar torneo" : "*Activar torneo", PMENU_ALIGN_CENTER, NULL, NULL);
	client_menu (ent, row++, "fichas abiertas de la web", PMENU_ALIGN_CENTER, NULL, NULL);
	row++;
	for (i = 0; i < tn_count && i < TORNEO_LIST_ROWS; i++)
	{
		client_menu (ent, row, Torneo_Row (ent, row, "%.24s", tn_list[i].name[0] ? tn_list[i].name : tn_list[i].key),
			PMENU_ALIGN_CENTER, tn_list[i].key, M_Ref_TorneoPick);
		row++;
	}
	if (!tn_count)
	{
		client_menu (ent, row++, "(no hay torneos abiertos)", PMENU_ALIGN_CENTER, NULL, NULL);
		client_menu (ent, row++, "se crean en la web", PMENU_ALIGN_CENTER, NULL, NULL);
	}
	row++;
	client_menu (ent, row, "Volver", PMENU_ALIGN_CENTER, NULL, M_Ref_Open);
	Torneo_RefFoot (ent, row);
	PMenu_Open (ent, ent->client->menu_cur, TORNEO_MENU_TOP + 3, MENU_MAX);
}

// minutos: activa el torneo elegido o cambia los del activo. arg: 1 = de la ficha, 2 + m = m minutos
static void M_Ref_Minutes (edict_t *ent, pmenu_t *p, int choice)
{
	int			v = (int)(size_t)p->arg;
	char		min[16];
	torneo_t	*t;
	int			many;

	PMenu_Close (ent);
	if (v <= 0)
		return;
	Com_sprintf (min, sizeof(min), "%s", v == 1 ? "" : va ("%d", v - 2));

	if (!REF (ent).set_minutes)
	{
		tn_reply = ent;
		Torneo_Start (REF (ent).key, REF (ent).map, min);
		tn_reply = NULL;
		if (tn.active && !Q_stricmp (tn.key, REF (ent).key))
			safe_bprintf (PRINT_HIGH, "(lo activo el arbitro %s)\n", ent->client->pers.netname);
		return;
	}

	// cambiar los minutos del torneo activo (desde la proxima partida)
	if (!tn.active)
		return;
	if (v == 1)
	{
		Torneo_LoadList ();
		t = Torneo_Find (tn.key, &many);
		tn.minutes = t ? t->minutes : -1;
	}
	else
		tn.minutes = v - 2;
	if (tn.minutes >= 0)
		gi.cvar_forceset ("ready_time", va ("%d", tn.minutes));
	Torneo_Save ();
	safe_bprintf (PRINT_HIGH, "El arbitro (%s) puso las partidas de %s en %s%s.\n", ent->client->pers.netname,
		Torneo_Name (), Torneo_MinutesText (tn.minutes), Ready_MatchInProgress () ? " (desde la proxima partida)" : "");
	Torneo_RefMenu (ent);
}

static void Torneo_RefMinutesMenu (edict_t *ent)
{
	static const int	opts[] = { 10, 15, 20, 25, 30, 0, -1 };
	torneo_t			*t;
	int					row = TORNEO_MENU_TOP, i, many, fm;

	Torneo_LoadList ();
	t = Torneo_Find (REF (ent).set_minutes ? tn.key : REF (ent).key, &many);
	fm = t ? t->minutes : -1;

	Torneo_ClearMenu (ent);
	client_menu (ent, row++, "*Minutos por partida", PMENU_ALIGN_CENTER, NULL, NULL);
	client_menu (ent, row, Torneo_Row (ent, row, "%.24s", REF (ent).set_minutes ? Torneo_Name () : REF (ent).key),
		PMENU_ALIGN_CENTER, NULL, NULL);
	row += 2;
	if (fm >= 0)
		client_menu (ent, row, Torneo_Row (ent, row, "De la ficha (%s)", Torneo_MinutesText (fm)), PMENU_ALIGN_CENTER, (void *)1, M_Ref_Minutes);
	else
		client_menu (ent, row, Torneo_Row (ent, row, "Los del server (%s)", Torneo_MinutesText (-1)), PMENU_ALIGN_CENTER, (void *)1, M_Ref_Minutes);
	row++;
	for (i = 0; opts[i] >= 0; i++, row++)
		client_menu (ent, row, opts[i] ? Torneo_Row (ent, row, "%d minutos", opts[i]) : "Sin tiempo", PMENU_ALIGN_CENTER,
			(void *)(size_t)(opts[i] + 2), M_Ref_Minutes);
	row++;
	client_menu (ent, row, "Volver", PMENU_ALIGN_CENTER, NULL, M_Ref_Open);
	Torneo_RefFoot (ent, row);
	PMenu_Open (ent, ent->client->menu_cur, TORNEO_MENU_TOP + 3, MENU_MAX);
}

static void M_Ref_SetMinutes (edict_t *ent, pmenu_t *p, int choice)
{
	PMenu_Close (ent);
	REF (ent).set_minutes = true;
	Torneo_RefMinutesMenu (ent);
}

// --- confirmar lo que corta una partida

static void M_Ref_Yes (edict_t *ent, pmenu_t *p, int choice)
{
	int		what = REF (ent).confirm;
	char	why[64];

	PMenu_Close (ent);
	REF (ent).confirm = REF_NONE;
	if (what == REF_RESET)
	{
		if (!Ready_MatchInProgress ())
			return;
		safe_bprintf (PRINT_HIGH, "El arbitro (%s) reinicio la partida: vuelve el calentamiento.\n", ent->client->pers.netname);
		Svcmd_ResetCountdown_f ();
	}
	else if (what == REF_END)
	{
		tn_reply = ent;
		Torneo_End ();
		tn_reply = NULL;
	}
	else if (what == REF_MAP && tn.active && REF (ent).map[0])
	{
		Com_sprintf (why, sizeof(why), "El arbitro (%s)", ent->client->pers.netname);
		Torneo_ChangeMap (REF (ent).map, why);
	}
}

static void Torneo_RefConfirm (edict_t *ent, int what, const char *line1, const char *line2, const char *yes)
{
	int row = TORNEO_MENU_TOP;

	REF (ent).confirm = what;
	Torneo_ClearMenu (ent);
	client_menu (ent, row++, "*Seguro?", PMENU_ALIGN_CENTER, NULL, NULL);
	row++;
	client_menu (ent, row, Torneo_Row (ent, row, "%.24s", line1), PMENU_ALIGN_CENTER, NULL, NULL);
	row++;
	if (line2 && line2[0])
	{
		client_menu (ent, row, Torneo_Row (ent, row, "%.24s", line2), PMENU_ALIGN_CENTER, NULL, NULL);
		row++;
	}
	row++;
	client_menu (ent, row, Torneo_Row (ent, row, "%.24s", yes), PMENU_ALIGN_CENTER, NULL, M_Ref_Yes);
	row++;
	client_menu (ent, row, "No, volver", PMENU_ALIGN_CENTER, NULL, M_Ref_Open);
	Torneo_RefFoot (ent, row);
	PMenu_Open (ent, ent->client->menu_cur, row, MENU_MAX);	// el cursor parte en "No"
}

static void M_Ref_Reset (edict_t *ent, pmenu_t *p, int choice)
{
	PMenu_Close (ent);
	Torneo_RefConfirm (ent, REF_RESET, "Se pierde lo jugado", "y vuelve el calentamiento", "Si, reiniciar");
}

static void M_Ref_End (edict_t *ent, pmenu_t *p, int choice)
{
	PMenu_Close (ent);
	Torneo_RefConfirm (ent, REF_END, Ready_MatchInProgress () ? "Termina al acabar" : "El server vuelve",
		Ready_MatchInProgress () ? "la partida en juego" : "a lo normal", "Si, terminar torneo");
}

static void M_Ref_MapPick (edict_t *ent, pmenu_t *p, int choice)
{
	const char *map = (const char *)p->arg;

	PMenu_Close (ent);
	if (!map || !tn.active)
		return;
	Com_sprintf (REF (ent).map, sizeof(REF (ent).map), "%s", map);
	if (Ready_MatchInProgress ())
		Torneo_RefConfirm (ent, REF_MAP, "Se corta la partida", va ("y se cambia a %s", map), "Si, cambiar mapa");
	else
	{
		REF (ent).confirm = REF_MAP;
		M_Ref_Yes (ent, p, choice);
	}
}

static void M_Ref_Maps (edict_t *ent, pmenu_t *p, int choice)
{
	int i, row = TORNEO_MENU_TOP;

	PMenu_Close (ent);
	Torneo_LoadMaps ();
	Torneo_ClearMenu (ent);
	client_menu (ent, row++, "*Cambiar mapa", PMENU_ALIGN_CENTER, NULL, NULL);
	client_menu (ent, row++, "directo (arbitro)", PMENU_ALIGN_CENTER, NULL, NULL);
	row++;
	for (i = 0; i < tn_nmaps; i++)
		client_menu (ent, row++, tn_maps[i], PMENU_ALIGN_CENTER, tn_maps[i], M_Ref_MapPick);
	if (!tn_nmaps)
		client_menu (ent, row++, "(no hay otros mapas)", PMENU_ALIGN_CENTER, NULL, NULL);
	row++;
	client_menu (ent, row, "Volver", PMENU_ALIGN_CENTER, NULL, M_Ref_Open);
	Torneo_RefFoot (ent, row);
	PMenu_Open (ent, ent->client->menu_cur, TORNEO_MENU_TOP + 3, MENU_MAX);
}

// --- acciones directas

static void M_Ref_Start (edict_t *ent, pmenu_t *p, int choice)
{
	PMenu_Close (ent);
	if (!Ready_RefStart (ent))
		safe_cprintf (ent, PRINT_HIGH, "No se puede: la partida ya empezo o no se esta esperando el ready.\n");
}

static void M_Ref_Pause (edict_t *ent, pmenu_t *p, int choice)
{
	PMenu_Close (ent);
	if (!Ready_RefPause (ent))
		safe_cprintf (ent, PRINT_HIGH, "No se puede pausar ahora (la partida no esta en juego o ya esta en pausa).\n");
}

static void M_Ref_Resume (edict_t *ent, pmenu_t *p, int choice)
{
	PMenu_Close (ent);
	if (!Ready_RefResume (ent))
		safe_cprintf (ent, PRINT_HIGH, "No hay pausa que sacar (o ya va a seguir).\n");
}

static void M_Ref_Player (edict_t *ent, pmenu_t *p, int choice)	{ PMenu_Close (ent); Torneo_OpenMenu (ent); }

static void M_Ref_Leave (edict_t *ent, pmenu_t *p, int choice)
{
	PMenu_Close (ent);
	REF (ent).on = false;
	safe_bprintf (PRINT_HIGH, "%s ya no es arbitro.\n", ent->client->pers.netname);
}

static void Torneo_RefMenu (edict_t *ent)
{
	int			row = TORNEO_MENU_TOP, first;
	qboolean	in_match = Ready_MatchInProgress ();

	if (!Torneo_IsRef (ent))
		return;
	if (ent->client->menu)
		PMenu_Close (ent);
	REF (ent).confirm = REF_NONE;

	Torneo_ClearMenu (ent);
	client_menu (ent, row++, "*Arbitro", PMENU_ALIGN_CENTER, NULL, NULL);
	if (tn.active)
	{
		client_menu (ent, row, Torneo_Row (ent, row, "%.24s", Torneo_Name ()), PMENU_ALIGN_CENTER, NULL, NULL);
		row++;
		client_menu (ent, row, Torneo_Row (ent, row, "%.12s, %s", level.mapname, Torneo_MinutesText (tn.minutes)),
			PMENU_ALIGN_CENTER, NULL, NULL);
		row += 2;
		client_menu (ent, row++, level.intermissiontime ? "Partida terminada" : Ready_Paused () ? "*EN PAUSA" :
			countdownActive ? "Cuenta regresiva" : Ready_Started () ? "En juego" : "Calentamiento", PMENU_ALIGN_CENTER, NULL, NULL);
	}
	else
		client_menu (ent, row++, "Sin torneo activo", PMENU_ALIGN_CENTER, NULL, NULL);
	row++;
	first = row;

	if (!tn.active)
		client_menu (ent, row++, "Activar torneo...", PMENU_ALIGN_CENTER, NULL, M_Ref_List);
	else
	{
		if (Ready_Waiting ())
			client_menu (ent, row++, "Empezar ya (sin ready)", PMENU_ALIGN_CENTER, NULL, M_Ref_Start);
		if (Ready_Paused ())
			client_menu (ent, row++, "Sacar la pausa", PMENU_ALIGN_CENTER, NULL, M_Ref_Resume);
		else if (Ready_InPlayNow ())
			client_menu (ent, row++, "Pausar", PMENU_ALIGN_CENTER, NULL, M_Ref_Pause);
		if (in_match)
			client_menu (ent, row++, "Reiniciar partida", PMENU_ALIGN_CENTER, NULL, M_Ref_Reset);
		client_menu (ent, row++, "Cambiar mapa...", PMENU_ALIGN_CENTER, NULL, M_Ref_Maps);
		client_menu (ent, row++, "Minutos...", PMENU_ALIGN_CENTER, NULL, M_Ref_SetMinutes);
		if (!in_match)
			client_menu (ent, row++, "Cambiar torneo...", PMENU_ALIGN_CENTER, NULL, M_Ref_List);
		client_menu (ent, row++, tn.end_pending ? "*Termina al acabar" : "Terminar torneo", PMENU_ALIGN_CENTER, NULL,
			tn.end_pending ? NULL : M_Ref_End);
		row++;
		client_menu (ent, row++, "Menu de jugadores", PMENU_ALIGN_CENTER, NULL, M_Ref_Player);
	}
	row++;
	client_menu (ent, row++, "Dejar de ser arbitro", PMENU_ALIGN_CENTER, NULL, M_Ref_Leave);
	client_menu (ent, row, "Cerrar", PMENU_ALIGN_CENTER, NULL, M_Tn_Close);
	Torneo_RefFoot (ent, row);
	PMenu_Open (ent, ent->client->menu_cur, first, MENU_MAX);
}

void Cmd_TorneoMenu_f (edict_t *ent)
{
	Torneo_OpenMenu (ent);
}
