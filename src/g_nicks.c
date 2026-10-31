/*
 * g_nicks.c -- nicks y tags de clan protegidos con clave.
 *
 * El archivo dday/nicks.txt (solo en el server, nunca en el repo) tiene una
 * regla por linea:
 *
 *   nick  >ChL<Snako   clave      nadie mas puede usar ese nick
 *   nick  *peruano*    clave      ningun nick que contenga "peruano"
 *   tag   [MR]         clave      el tag del clan, en cualquier parte del nick
 *
 * El jugador manda la clave en su userinfo (una vez, en autoexec.cfg):
 *   set nickpw <clave> u      para su nick
 *   set tagpw <clave> u       para el tag de su clan
 * Las claves se borran del userinfo apenas se revisan.
 *
 * Los nicks se comparan normalizados (sin colores, minusculas, solo letras y
 * numeros: "chl snako" = ">ChL<Snako"); los tags sin colores, en minusculas y
 * sin espacios. Sin la clave no se puede entrar con ese nick: la conexion se
 * rechaza con un mensaje que dice que hacer. Si se cambia el nick ya dentro,
 * se queda con el que tenia (o Jugador<n>) y recibe el aviso. El dueno de un
 * nick protegido no necesita ademas la clave del tag.
 *
 * Cvar nick_protect (1 por defecto: es seguridad; sin nicks.txt no hace nada).
 * "sv nicks" lista las reglas (sin claves); "sv nicks reload" relee el archivo.
 * El archivo tambien se relee al empezar cada mapa.
 */

#include "g_local.h"
#include <ctype.h>

#define NICK_MAX_RULES	128
#define RULE_NICK		0
#define RULE_TAG		1

typedef struct
{
	int			type;
	qboolean	sub;		// *texto*: basta con que el nick lo contenga
	char		raw[64];	// como esta en el archivo (para listarla)
	char		key[64];	// normalizado
	char		pass[64];
} nickrule_t;

static nickrule_t	rules[NICK_MAX_RULES];
static int			numrules;
static cvar_t		*nick_protect;

// aviso pendiente por slot: se manda unos frames despues (al conectar todavia no esta en juego)
static char		notify_msg[MAX_CLIENTS][200];
static int		notify_frames[MAX_CLIENTS];

// ultimo nick que se valido en cada slot: las claves se borran del userinfo, y el mod vuelve
// a pasar el userinfo guardado (SyncUserInfo) sin ellas al aparecer en el mapa
static char		verified[MAX_CLIENTS][64];

// alnum_only: nicks (solo letras y numeros); si no, tags (todo menos espacios)
static void Nick_Norm (const char *src, char *dst, int size, qboolean alnum_only, int *map)
{
	int i, n = 0, c;

	for (i = 0; src[i] && n < size - 1; i++)
	{
		c = tolower ((unsigned char)src[i] & 127);
		if (alnum_only ? !isalnum (c) : c <= ' ')
			continue;
		if (map)
			map[n] = i;
		dst[n++] = c;
	}
	dst[n] = '\0';
}

void Nick_Load (void)
{
	char		*buf, *p, line[256], *s, *e, *last;
	nickrule_t	*r;
	int			len;

	numrules = 0;
	buf = ReadEntFile ("nicks.txt");
	if (!buf)
		return;

	for (p = buf; *p; )
	{
		// una linea
		for (len = 0; p[len] && p[len] != '\n'; len++)
			;
		Com_sprintf (line, sizeof(line), "%.*s", len < (int)sizeof(line) - 1 ? len : (int)sizeof(line) - 1, p);
		p += len;
		if (*p)
			p++;

		// sin espacios ni \r a los lados; se saltan vacias y comentarios
		for (s = line; *s && (unsigned char)*s <= ' '; s++)
			;
		for (e = s + strlen (s); e > s && (unsigned char)e[-1] <= ' '; e--)
			;
		*e = '\0';
		if (!*s || *s == '#' || numrules >= NICK_MAX_RULES)
			continue;

		r = &rules[numrules];
		memset (r, 0, sizeof(*r));

		// tipo
		if (!Q_strncasecmp (s, "nick", 4) && (unsigned char)s[4] <= ' ' && s[4])
			r->type = RULE_NICK;
		else if (!Q_strncasecmp (s, "tag", 3) && (unsigned char)s[3] <= ' ' && s[3])
			r->type = RULE_TAG;
		else
		{
			gi.dprintf ("nicks.txt: linea no valida (falta nick/tag): %s\n", s);
			continue;
		}
		s += (r->type == RULE_NICK) ? 4 : 3;
		while (*s && (unsigned char)*s <= ' ')
			s++;

		// clave = ultima palabra; el nombre es lo del medio (puede tener espacios)
		last = strrchr (s, ' ');
		if (!last)
			last = strrchr (s, '\t');
		if (!last)
		{
			gi.dprintf ("nicks.txt: falta la clave: %s\n", line);
			continue;
		}
		Com_sprintf (r->pass, sizeof(r->pass), "%s", last + 1);
		for (e = last; e > s && (unsigned char)e[-1] <= ' '; e--)
			;
		*e = '\0';

		len = strlen (s);
		if (len > 2 && s[0] == '*' && s[len - 1] == '*')
		{
			r->sub = true;
			s[len - 1] = '\0';
			s++;
		}
		Com_sprintf (r->raw, sizeof(r->raw), "%s", s);
		Nick_Norm (s, r->key, sizeof(r->key), r->type == RULE_NICK, NULL);
		if (!r->key[0] || !r->pass[0])
			continue;
		numrules++;
	}
	gi.TagFree (buf);
	gi.dprintf ("nicks: %d nicks/tags protegidos\n", numrules);
}

void Nick_Init (void)
{
	// el archivo se lee al empezar cada mapa (aca basedir/homedir todavia no existen)
	nick_protect = gi.cvar ("nick_protect", "1", 0);
}

static qboolean Nick_MatchNick (nickrule_t *r, const char *name)
{
	char n[64];

	Nick_Norm (name, n, sizeof(n), true, NULL);
	if (!n[0])
		return false;
	return r->sub ? strstr (n, r->key) != NULL : !strcmp (n, r->key);
}

// el nick tiene el tag de la regla
static qboolean Nick_HasTag (const char *name, nickrule_t *r)
{
	char n[64];

	Nick_Norm (name, n, sizeof(n), false, NULL);
	return strstr (n, r->key) != NULL;
}

#define NICK_OK			0
#define NICK_BAD_NICK	1
#define NICK_BAD_TAG	2

// revisa un nick con las claves que mando el jugador; en what deja el nick o el tag que fallo
static int Nick_Verify (const char *name, const char *nickpw, const char *tagpw, char *what, int size)
{
	int i;

	if (!nick_protect || !nick_protect->value || !numrules)
		return NICK_OK;

	// el dueno de un nick protegido (con su clave) no necesita ademas la del tag
	for (i = 0; i < numrules; i++)
		if (rules[i].type == RULE_NICK && !strcmp (rules[i].pass, nickpw) && Nick_MatchNick (&rules[i], name))
			return NICK_OK;

	for (i = 0; i < numrules; i++)
		if (rules[i].type == RULE_NICK && Nick_MatchNick (&rules[i], name))
		{
			Com_sprintf (what, size, "%s", name);
			return NICK_BAD_NICK;
		}
	for (i = 0; i < numrules; i++)
		if (rules[i].type == RULE_TAG && strcmp (rules[i].pass, tagpw) && Nick_HasTag (name, &rules[i]))
		{
			Com_sprintf (what, size, "%s", rules[i].raw);
			return NICK_BAD_TAG;
		}
	return NICK_OK;
}

// el nick actual es uno protegido (y como los impostores no pueden usarlo, es el dueno)
qboolean Nick_IsProtected (edict_t *ent)
{
	int i;

	if (!ent->client)
		return false;
	for (i = 0; i < numrules; i++)
		if (rules[i].type == RULE_NICK && Nick_MatchNick (&rules[i], ent->client->pers.netname))
			return true;
	return false;
}

// desde ClientConnect: false (y rejmsg) si entra con un nick o tag protegido sin la clave.
// No borra las claves del userinfo: ClientUserinfoChanged las vuelve a revisar enseguida.
qboolean Nick_AllowConnect (edict_t *ent, char *userinfo)
{
	char	name[64], nickpw[64], tagpw[64], what[64], msg[200];
	int		r;

	if (ent->ai)
		return true;
	if (ent - g_edicts - 1 >= 0 && ent - g_edicts - 1 < MAX_CLIENTS)
		verified[ent - g_edicts - 1][0] = '\0';	// jugador nuevo en este slot
	Com_sprintf (name, sizeof(name), "%s", Info_ValueForKey (userinfo, "name"));
	Com_sprintf (nickpw, sizeof(nickpw), "%s", Info_ValueForKey (userinfo, "nickpw"));
	Com_sprintf (tagpw, sizeof(tagpw), "%s", Info_ValueForKey (userinfo, "tagpw"));

	r = Nick_Verify (name, nickpw, tagpw, what, sizeof(what));
	if (r == NICK_OK)
		return true;

	// rejmsg: los valores del userinfo tienen que ser de menos de 64 caracteres
	if (r == NICK_BAD_NICK)
		Com_sprintf (msg, MAX_INFO_VALUE, "Nick protegido: set nickpw TU_CLAVE u y reconecta");
	else
		Com_sprintf (msg, MAX_INFO_VALUE, "Tag %s protegido: set tagpw CLAVE u y reconecta", what);
	Info_SetValueForKey (userinfo, "rejmsg", msg);
	gi.dprintf ("nicks: rechazado \"%s\" (sin la clave del %s)\n", name, r == NICK_BAD_NICK ? "nick" : "tag");
	return false;
}

// desde ClientUserinfoChanged, antes de copiar el nombre: si se cambia a un nick o tag
// protegido sin la clave, se queda con el nick que tenia (o Jugador<n>) y se le avisa
void Nick_Check (edict_t *ent, char *userinfo)
{
	char	name[64], newname[64], nickpw[64], tagpw[64], what[64], dummy[64];
	int		slot = ent - g_edicts - 1, r;

	Com_sprintf (nickpw, sizeof(nickpw), "%s", Info_ValueForKey (userinfo, "nickpw"));
	Com_sprintf (tagpw, sizeof(tagpw), "%s", Info_ValueForKey (userinfo, "tagpw"));
	if (nickpw[0])
		Info_RemoveKey (userinfo, "nickpw");
	if (tagpw[0])
		Info_RemoveKey (userinfo, "tagpw");

	if (ent->ai || slot < 0 || slot >= MAX_CLIENTS)
		return;

	Com_sprintf (name, sizeof(name), "%s", Info_ValueForKey (userinfo, "name"));
	if (verified[slot][0] && !strcmp (verified[slot], name))
		return;		// ya validado (el mod repite el userinfo guardado, sin las claves)
	r = Nick_Verify (name, nickpw, tagpw, what, sizeof(what));
	if (r == NICK_OK)
	{
		Com_sprintf (verified[slot], sizeof(verified[slot]), "%s", name);
		return;
	}

	// el nick que tenia, si sigue valiendo con estas claves
	if (ent->client->pers.netname[0] && strcmp (ent->client->pers.netname, name)
		&& (!strcmp (verified[slot], ent->client->pers.netname)
			|| Nick_Verify (ent->client->pers.netname, nickpw, tagpw, dummy, sizeof(dummy)) == NICK_OK))
		Com_sprintf (newname, sizeof(newname), "%s", ent->client->pers.netname);
	else
		Com_sprintf (newname, sizeof(newname), "Jugador%d", slot + 1);

	Info_SetValueForKey (userinfo, "name", newname);
	gi.dprintf ("nicks: \"%s\" sin la clave del %s, sigue como \"%s\"\n", name,
		r == NICK_BAD_NICK ? "nick" : "tag", newname);

	if (r == NICK_BAD_NICK)
		Com_sprintf (notify_msg[slot], sizeof(notify_msg[slot]),
			"El nick %s esta protegido, sigues como %s. Si es tuyo, escribe en la consola "
			"set nickpw TU_CLAVE u y vuelve a ponerte el nick.\n", what, newname);
	else
		Com_sprintf (notify_msg[slot], sizeof(notify_msg[slot]),
			"El tag %s esta protegido, sigues como %s. Si eres del clan, escribe en la consola "
			"set tagpw CLAVE_DEL_CLAN u y vuelve a ponerte el nick.\n", what, newname);
	notify_frames[slot] = 20;	// 2 s
}

// cada frame desde G_RunFrame: manda los avisos pendientes
void Nick_RunFrame (void)
{
	int		i;
	edict_t	*ent;

	for (i = 0; i < game.maxclients && i < MAX_CLIENTS; i++)
	{
		if (!notify_frames[i])
			continue;
		ent = g_edicts + 1 + i;
		if (!ent->inuse || !ent->client)
			continue;	// todavia conectando
		if (--notify_frames[i])
			continue;
		safe_cprintf (ent, PRINT_HIGH, "%s", notify_msg[i]);
	}
}

void Nick_ClientDisconnect (edict_t *ent)
{
	int slot = ent - g_edicts - 1;

	if (slot >= 0 && slot < MAX_CLIENTS)
	{
		notify_frames[slot] = 0;
		verified[slot][0] = '\0';
	}
}

// sv nicks [reload]
void Svcmd_Nicks_f (void)
{
	int i;

	if (!Q_stricmp (gi.argv (2), "reload"))
		Nick_Load ();

	safe_cprintf (NULL, PRINT_HIGH, "nick_protect %d, %d reglas (dday/nicks.txt):\n",
		nick_protect ? (int)nick_protect->value : 0, numrules);
	for (i = 0; i < numrules; i++)
		safe_cprintf (NULL, PRINT_HIGH, "  %-4s %s%s%s\n", rules[i].type == RULE_NICK ? "nick" : "tag",
			rules[i].sub ? "*" : "", rules[i].raw, rules[i].sub ? "*" : "");
}
