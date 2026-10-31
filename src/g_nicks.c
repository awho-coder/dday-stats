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
 * sin espacios. Sin la clave, el nick queda como Jugador<n> y el tag se quita
 * del nick; el jugador recibe un aviso con lo que tiene que hacer. No se echa
 * a nadie. El dueno de un nick protegido no necesita ademas la clave del tag.
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

// quita del nick el tag de la regla; true si lo encontro
static qboolean Nick_StripTag (char *name, int size, nickrule_t *r)
{
	char	n[64], *found;
	int		map[64], start, end, klen = strlen (r->key);

	Nick_Norm (name, n, sizeof(n), false, map);
	found = strstr (n, r->key);
	if (!found)
		return false;
	start = map[found - n];
	end = map[found - n + klen - 1];
	memmove (name + start, name + end + 1, strlen (name + end + 1) + 1);
	return true;
}

// nombre que no quedo con letras ni numeros
static qboolean Nick_Empty (const char *name)
{
	char n[64];

	Nick_Norm (name, n, sizeof(n), true, NULL);
	return !n[0];
}

// el nick actual es uno protegido (y como los impostores se renombran, es el dueno)
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

// desde ClientUserinfoChanged, antes de copiar el nombre: corrige el name del userinfo
void Nick_Check (edict_t *ent, char *userinfo)
{
	char	name[64], newname[64], nickpw[64], tagpw[64];
	char	*msg = NULL, what[64] = "";
	int		slot = ent - g_edicts - 1, i, pass;

	Com_sprintf (nickpw, sizeof(nickpw), "%s", Info_ValueForKey (userinfo, "nickpw"));
	Com_sprintf (tagpw, sizeof(tagpw), "%s", Info_ValueForKey (userinfo, "tagpw"));
	if (nickpw[0])
		Info_RemoveKey (userinfo, "nickpw");
	if (tagpw[0])
		Info_RemoveKey (userinfo, "tagpw");

	if (!nick_protect || !nick_protect->value || !numrules || ent->ai || slot < 0 || slot >= MAX_CLIENTS)
		return;

	Com_sprintf (name, sizeof(name), "%s", Info_ValueForKey (userinfo, "name"));
	Com_sprintf (newname, sizeof(newname), "%s", name);

	// el dueno de un nick protegido (con su clave) no necesita ademas la del tag
	for (i = 0; i < numrules; i++)
		if (rules[i].type == RULE_NICK && !strcmp (rules[i].pass, nickpw)
			&& Nick_MatchNick (&rules[i], name))
			return;

	// tags primero: "[MR]+Pepe" sin la clave del clan queda "+Pepe"
	for (pass = 0; pass < 4; pass++)	// por si repite el tag
		for (i = 0; i < numrules; i++)
			if (rules[i].type == RULE_TAG && strcmp (rules[i].pass, tagpw)
				&& Nick_StripTag (newname, sizeof(newname), &rules[i]))
			{
				msg = "tag";
				Com_sprintf (what, sizeof(what), "%s", rules[i].raw);
			}

	for (i = 0; i < numrules; i++)
		if (rules[i].type == RULE_NICK && strcmp (rules[i].pass, nickpw)
			&& Nick_MatchNick (&rules[i], newname))
		{
			msg = "nick";
			Com_sprintf (what, sizeof(what), "%s", name);
			break;
		}

	if (!msg)
		return;
	if (!strcmp (msg, "nick") || Nick_Empty (newname))
		Com_sprintf (newname, sizeof(newname), "Jugador%d", slot + 1);

	Info_SetValueForKey (userinfo, "name", newname);
	gi.dprintf ("nicks: \"%s\" sin la clave del %s, queda como \"%s\"\n", name, msg, newname);

	if (!strcmp (msg, "nick"))
		Com_sprintf (notify_msg[slot], sizeof(notify_msg[slot]),
			"El nick %s esta protegido y quedaste como %s. Si es tuyo, escribe en la consola "
			"set nickpw <tu clave> u y vuelve a ponerte el nick.\n", what, newname);
	else
		Com_sprintf (notify_msg[slot], sizeof(notify_msg[slot]),
			"El tag %s esta protegido y se quito de tu nick. Si eres del clan, escribe en la consola "
			"set tagpw <clave del clan> u y vuelve a ponerte el nick.\n", what);
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
		notify_frames[slot] = 0;
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
