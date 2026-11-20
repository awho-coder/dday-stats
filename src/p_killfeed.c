// p_killfeed.c: graphical killfeed, independent of the game mode.
//
// The deaths are drawn at the top left by the statusbar program itself (CS_STATUSBAR): there are no free stats left,
// so the lines are written straight into the program and the configstring is sent again when the feed changes (a new
// death or an expired line), which is a rare event. The statusbar of the level (team, Free For All...) is given once
// by Killfeed_SetBase and goes after the lines, so the feed does not depend on it.
//
// Each row is a plate (pics/kf_bg_<team>_<size>.png, olive for the team 0, red for the team 1, khaki for no team),
// the weapon icon (pics/kf_<icon>.png, half the size of the icon of the game) and the names. The pictures are made
// by the clients, see pics/README.md.
//
// The statusbar of the engine has a limit (MAX_QPATH * (CS_AIRACCEL - CS_STATUSBAR)): the base is stored with its
// white space collapsed and the oldest lines are dropped when they do not fit.

#include "g_local.h"
#include "x_fire.h"
#include <stdio.h>
#include <string.h>

#define KF_MAX_LINES	4
#define KF_LIFETIME		6.0		// seconds a line stays on the screen
#define KF_NAME_LEN		12		// the names are cut to this length
#define KF_LABEL_LEN	20		// the verb of the style 6
#define KF_BAR_MAX		(MAX_QPATH * (CS_AIRACCEL - CS_STATUSBAR) - 1)
#define KF_X			2
#define KF_Y			30		// under the lines of the console that the client prints at the top ("You've been hit...")
#define KF_ROW_H		16		// plate (14 pixels) and a gap
#define KF_SPLIT_GAP	2		// between the weapon (or the headshot icon) and the plate of the victim
#define KF_PAD_R		4
#define KF_TEXT_DY		3		// text (8 pixels high) centered on the plate
#define KF_CHAR_W		8
#define KF_GAP			6		// between the names and the weapon icon
#define KF_FLAG_W		12		// faction icon (pics/kf_<faction>.png) and the space after it
#define KF_FLAG_GAP		2
#define KF_HS_GAP		4		// between the weapon and the headshot icon
#define KF_PLATE_MIN	48		// the plates are pics/kf_bg_<team>_<width>.png, in steps of KF_PLATE_STEP
#define KF_PLATE_MAX	344
#define KF_PLATE_STEP	8
#define KF_ICON_GENERIC	"skull"	// deaths without killer (suicides, environment) and with no icon of their own

typedef struct
{
	char	*name;		// the picture is pics/kf_<name>.png
	int		width;		// pixels: the statusbar cannot scale or measure them
} kf_icon_t;

// width of the icons of pics/kf_*.png; an icon that is not here is drawn as the generic one
static kf_icon_t kf_icons[] =
{
	{ "w_303", 68 },
	{ "w_303s", 55 },
	{ "w_arisaka", 59 },
	{ "w_arisakas", 59 },
	{ "w_b34", 16 },
	{ "w_b38", 45 },
	{ "w_b3842", 37 },
	{ "w_bar", 48 },
	{ "w_bazooka", 48 },
	{ "w_bhmg", 48 },
	{ "w_binoc", 24 },
	{ "w_breda", 45 },
	{ "w_bren", 31 },
	{ "w_carcano", 81 },
	{ "w_colt45", 24 },
	{ "w_dpm", 57 },
	{ "w_fists", 12 },
	{ "w_flame", 48 },
	{ "w_healthpack", 24 },
	{ "w_helmet", 12 },
	{ "w_k43", 70 },
	{ "w_katana", 54 },
	{ "w_knife", 48 },
	{ "w_m1", 48 },
	{ "w_m1903", 48 },
	{ "w_m1carb", 12 },
	{ "w_m9130", 48 },
	{ "w_m9130s", 48 },
	{ "w_m98k", 48 },
	{ "w_m98ks", 48 },
	{ "w_masher", 24 },
	{ "w_mg34", 47 },
	{ "w_mg42", 48 },
	{ "w_molotov", 13 },
	{ "w_morphine", 12 },
	{ "w_mors", 47 },
	{ "w_mp40", 48 },
	{ "w_mp43", 48 },
	{ "w_nambu", 24 },
	{ "w_p38", 24 },
	{ "w_panzer", 48 },
	{ "w_panzerf", 72 },
	{ "w_piat", 42 },
	{ "w_pps43", 47 },
	{ "w_ppsh41", 40 },
	{ "w_rkm", 48 },
	{ "w_sandbags", 15 },
	{ "w_shotgun", 46 },
	{ "w_sten", 48 },
	{ "w_svt", 48 },
	{ "w_thompson", 48 },
	{ "w_tnt", 24 },
	{ "w_tt33", 14 },
	{ "w_type_100", 50 },
	{ "w_type_99", 28 },
	{ "w_vickers", 46 },
	{ "w_vis", 25 },
	{ "w_webley", 25 },
	{ "skull", 12 },
	{ "plane", 30 },
	{ "nade", 12 },
	{ "boom", 12 },
	{ "tank", 30 },
	{ "hs", 14 }
};

typedef struct
{
	char		killer[KF_NAME_LEN + 1];	// empty: nobody (suicide, environment)
	char		victim[KF_NAME_LEN + 1];
	kf_icon_t	*icon;
	char		*killer_fac;				// faction (pics/kf_<faction>.png), NULL: no icon
	char		*victim_fac;
	qboolean	headshot;
	char		label[KF_LABEL_LEN + 1];	// the verb of VisorDemos (style 6): "sniped", "grenade", "suicide"...
	char		killer_team;				// color of the plates (pics/kf_bg_<team>_<width>.png): 'a' team 0, 'x' team 1, 'n' none
	char		victim_team;
	float		expire;
} kf_entry_t;

// factions that have an icon (pics/teams/<faction>.pcx of the game)
static char *kf_factions[] = { "usa", "usm", "gbr", "grm", "ita", "jpn", "pol", "rus" };

static kf_entry_t	kf_entries[KF_MAX_LINES];	// [0] is the oldest, it is drawn on top
static int			kf_count;
static char			kf_base[4096];
static qboolean		kf_base_set;
static qboolean		kf_dirty;		// the feed changed this frame: Killfeed_Frame sends it once (several deaths in a frame, one configstring)


// copy of the statusbar program without the extra white space (the quoted text is kept as it is)
static void KF_Compact (char *dst, int size, const char *src)
{
	int			n = 0;
	qboolean	quoted = false;
	qboolean	space = true;	// true: the last char is a space, so a new one is not written
	char		c;

	while (*src && n < size - 2)
	{
		c = *src++;

		if (c == '"')
		{
			quoted = !quoted;
			space = false;
			dst[n++] = c;
		}
		else if (!quoted && (c == ' ' || c == '\t' || c == '\r' || c == '\n'))
		{
			if (!space)
			{
				dst[n++] = ' ';
				space = true;
			}
		}
		else
		{
			space = false;
			dst[n++] = c;
		}
	}

	if (n > 0 && dst[n - 1] != ' ')
		dst[n++] = ' ';
	dst[n] = 0;
}


// the name goes inside quotes in the program: no quotes or control chars
static void KF_CopyName (char *dst, const char *name)
{
	int		n = 0;

	while (*name && n < KF_NAME_LEN)
	{
		if (*name != '"' && (unsigned char)*name >= 32)
			dst[n++] = *name;
		name++;
	}
	dst[n] = 0;
}


static kf_icon_t *KF_FindIcon (char *name)
{
	int		i;

	for (i = 0; i < sizeof (kf_icons) / sizeof (kf_icons[0]); i++)
		if (!Q_stricmp (kf_icons[i].name, name))
			return &kf_icons[i];

	return NULL;
}


// the weapon in hand of the killer for the deaths that come from it, a fixed icon for the weapons that are
// not held when they kill (fire, rockets, TNT, bottles, grenades, airstrikes...) and the skull for the rest
static kf_icon_t *KF_ChooseIcon (edict_t *killer, edict_t *victim, int mod)
{
	kf_icon_t	*icon = NULL;
	char		*held = NULL;

	// suicides and the environment: always the skull
	if (!killer || killer == victim || !killer->client)
		return KF_FindIcon (KF_ICON_GENERIC);

	if (killer->client->pers.weapon)
		held = killer->client->pers.weapon->icon;

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
	case MOD_KNIFE:
	case MOD_FISTS:
	case MOD_BAYONET:
		if (held)
			icon = KF_FindIcon (held);
		break;

	case MOD_ROCKET:
	case MOD_R_SPLASH:
		// the weapon in hand can be another one by the time the rocket explodes
		if (held && (!Q_stricmp (held, "w_bazooka") || !Q_stricmp (held, "w_panzer") ||
			!Q_stricmp (held, "w_panzerf") || !Q_stricmp (held, "w_piat")))
			icon = KF_FindIcon (held);
		else
			icon = KF_FindIcon ("w_bazooka");
		break;

	case MOD_FIRE_SPLASH:
	case MOD_ON_FIRE:
	case MOD_FIREBALL:
		icon = KF_FindIcon ("w_flame");
		break;

	case MOD_BOTTLE:
		icon = KF_FindIcon ("w_molotov");
		break;

	case MOD_TNT:
	case MOD_TNT1:
	case MOD_HELD_TNT:
	case MOD_TNT_SPLASH:
	case MOD_TNT1_SPLASH:
		icon = KF_FindIcon ("w_tnt");
		break;

	case MOD_GRENADE:
	case MOD_G_SPLASH:
	case MOD_HANDGRENADE:
	case MOD_HG_SPLASH:
	case MOD_HELD_GRENADE:
		icon = KF_FindIcon ("nade");
		break;

	case MOD_AIRSTRIKE:
	case MOD_AIRSTRIKE_SPLASH:
		icon = KF_FindIcon ("plane");
		break;

	case MOD_EXPLOSIVE:
	case MOD_BARREL:
	case MOD_BOMB:
		icon = KF_FindIcon ("boom");
		break;

	case MOD_TANKHIT:
		icon = KF_FindIcon ("tank");
		break;

	case MOD_HELMET:
		icon = KF_FindIcon ("w_helmet");
		break;
	}

	if (!icon)
		icon = KF_FindIcon (KF_ICON_GENERIC);
	return icon;
}


// team of one player for the color of his plate: 'a' team 0, 'x' team 1, 'n' none or Free For All
static char KF_TeamOf (edict_t *ent)
{
	if (G_IsFFA () || !ent || !ent->client || !ent->client->resp.team_on)
		return 'n';
	if (ent->client->resp.team_on->index == 0)
		return 'a';
	if (ent->client->resp.team_on->index == 1)
		return 'x';
	return 'n';
}


// faction icon of the player; none in Free For All (everybody is on the same team) or when the faction has no icon
static char *KF_ChooseFaction (edict_t *ent)
{
	int		i;

	if (!ent || !ent->client || !ent->client->resp.team_on || G_IsFFA ())
		return NULL;

	for (i = 0; i < sizeof (kf_factions) / sizeof (kf_factions[0]); i++)
		if (!strcmp (ent->client->resp.team_on->teamid, kf_factions[i]))
			return kf_factions[i];

	return NULL;
}


// Each player chooses the style of his feed (command "killfeed <style>", kept in his userinfo "kf_style",
// default killfeed_style, 0): the statusbar with the feed is sent to every player by himself (svc_configstring by
// unicast); the configstring of the server keeps the statusbar of the level without the feed (new clients, MVD)
//   0  off: no feed, the obituary in text
//   1  like the kill feed of VisorDemos with the weapon (the favourite): "killer  weapon[ HS][ TK]  victim" on a black plate
//   2  like 1 with the flags, the headshot icon and one plate of the color of each team (killer and victim)
//   3  solid plates of two colors (green and red), white names
//   4  one plate of the team of the killer (dark, tinted), names of the color of their team
//   5  the original of Snako: one dark plate of the team of the killer, white names
//   6  the kill feed of VisorDemos as it is: like 1 with the verb in text ("sniped", "grenade"...) instead of the weapon
#define KF_STYLES		7
#define KF_STYLE_OFF	0
#define KF_MAX_CLIENTS	256
#define KF_SVC_CONFIGSTRING	13

typedef struct
{
	char		*name;		// for the command
	char		*plates;	// pics/kf_bg_<plates><team>_<width>.png ("v": pics/kf_bg_v_<width>.png, without team)
	qboolean	visor;		// the layout of VisorDemos (styles 1 and 6)
	qboolean	label;		// the verb in text instead of the weapon icon (style 6)
	qboolean	split;		// one plate for the killer and another one for the victim
	int			pad_l;		// left of the plate to the first picture or text (stripe of the plate)
	char		*color_a;	// names of the team 0 and 1, NULL: white
	char		*color_x;
} kf_style_t;

static kf_style_t kf_styles[KF_STYLES] =
{
	{ "apagado: las muertes en texto, como antes", NULL, false, false, false, 0, NULL, NULL },
	{ "placa negra, nombres de color y el arma", "v", true, false, false, 3, "#8bf", "#f86" },
	{ "placas de color por equipo y banderas", "", false, false, true, 5, "#8bf", "#f86" },
	{ "placas verde y roja, nombres blancos", "s", false, false, true, 5, NULL, NULL },
	{ "placa del que mata, nombres de color", "t", false, false, false, 9, "#9d5", "#f88" },
	{ "placa oscura del que mata, nombres blancos", "o", false, false, false, 5, NULL, NULL },
	{ "placa negra, nombres de color y como murio", "v", true, true, false, 3, "#8bf", "#f86" }
};

static char KF_COLOR_HS[] = "#fb2";	// "HS" (style 1)
static char KF_COLOR_TK[] = "#f44";	// "TK" and the verb of a teamkill (styles 1 and 6)
static char KF_COLOR_LABEL[] = "#bbb";	// the verb (style 6)

static int			kf_client_style[KF_MAX_CLIENTS];	// -1: killfeed_style of the server
static qboolean		kf_client_feed[KF_MAX_CLIENTS];		// the client has a statusbar with the feed (sent by unicast)

// The texts of the feed are drawn after all the pictures, grouped by color: the color command of the client (Q2PRO,
// other clients skip it and draw the texts in white) also tints the pictures, and one command per color is shorter.
// Groups in the order they are written: 'n' white (the statusbar starts in white), 'a' and 'x' the teams,
// 'h' headshot, 'l' the verb, 't' teamkill
static char kf_passes[] = "naxhlt";

typedef struct
{
	char	pass;
	int		x, y;
	char	*text;
} kf_text_t;

#define KF_MAX_TEXTS	(KF_MAX_LINES * 4)
static kf_text_t	kf_texts[KF_MAX_TEXTS];
static int			kf_ntexts;


static char *KF_PassColor (kf_style_t *st, char pass)
{
	switch (pass)
	{
	case 'a':	return st->color_a;
	case 'x':	return st->color_x;
	case 'h':	return KF_COLOR_HS;
	case 'l':	return KF_COLOR_LABEL;
	case 't':	return KF_COLOR_TK;
	}
	return NULL;
}


static void KF_AddText (kf_style_t *st, char pass, int x, int y, char *text)
{
	if (kf_ntexts == KF_MAX_TEXTS)
		return;
	if (!KF_PassColor (st, pass))
		pass = 'n';
	kf_texts[kf_ntexts].pass = pass;
	kf_texts[kf_ntexts].x = x;
	kf_texts[kf_ntexts].y = y;
	kf_texts[kf_ntexts].text = text;
	kf_ntexts++;
}


static int KF_PlateWidth (int width)
{
	width = (width + KF_PLATE_STEP - 1) / KF_PLATE_STEP * KF_PLATE_STEP;
	if (width < KF_PLATE_MIN)
		width = KF_PLATE_MIN;
	if (width > KF_PLATE_MAX)
		width = KF_PLATE_MAX;
	return width;
}


// styles 2 to 5: [faction] killer weapon [headshot] [faction] victim. With split, the part of the killer is on a plate
// of his team and the one of the victim on a plate of hers, drawn over the end of the first one; without split, one
// plate of the team of the killer (of the victim when there is no killer). The plates are the width of their part
// (in steps of KF_PLATE_STEP) and the x of everything follows the length of the names. Writes the pictures to out and
// adds the names to kf_texts
static void KF_LinePlates (char *out, kf_style_t *st, kf_entry_t *e, int row)
{
	int		y = KF_Y + row * KF_ROW_H;
	int		x = KF_X + st->pad_l;
	int		kfx = 0, wx, hx = 0, split = KF_X, vfx = 0;
	char	*p = out;

	if (e->killer[0])
	{
		if (e->killer_fac)
		{
			kfx = x;
			x += KF_FLAG_W + KF_FLAG_GAP;
		}
		KF_AddText (st, e->killer_team, x, y + KF_TEXT_DY, e->killer);
		x += strlen (e->killer) * KF_CHAR_W + KF_GAP;
	}

	wx = x;
	x += e->icon->width;

	if (e->headshot)
	{
		x += KF_HS_GAP;
		hx = x;
		x += KF_FindIcon ("hs")->width;
	}

	if (st->split && e->killer[0])
	{
		split = x + KF_SPLIT_GAP;
		x = split + st->pad_l;
	}
	else
		x += KF_GAP;

	if (e->victim_fac)
	{
		vfx = x;
		x += KF_FLAG_W + KF_FLAG_GAP;
	}
	KF_AddText (st, e->victim_team, x, y + KF_TEXT_DY, e->victim);
	x += strlen (e->victim) * KF_CHAR_W;

	p += sprintf (p, "yt %d ", y);
	if (!st->split)
		p += sprintf (p, "xl %d picn kf_bg_%s%c_%d ", KF_X, st->plates, e->killer[0] ? e->killer_team : e->victim_team,
			KF_PlateWidth (x + KF_PAD_R - KF_X));
	else
	{
		if (e->killer[0])
			p += sprintf (p, "xl %d picn kf_bg_%s%c_%d ", KF_X, st->plates, e->killer_team, KF_PlateWidth (split - KF_X));
		p += sprintf (p, "xl %d picn kf_bg_%s%c_%d ", split, st->plates, e->victim_team, KF_PlateWidth (x + KF_PAD_R - split));
	}
	if (kfx)
		p += sprintf (p, "xl %d picn kf_%s ", kfx, e->killer_fac);
	p += sprintf (p, "xl %d picn kf_%s ", wx, e->icon->name);
	if (hx)
		p += sprintf (p, "xl %d picn kf_hs ", hx);
	if (vfx)
		sprintf (p, "xl %d picn kf_%s ", vfx, e->victim_fac);
}


// styles 1 and 6, the kill feed of VisorDemos: "killer  verb[ HS][ TK]  victim" on one dark plate
// (pics/kf_bg_v_<width>.png), the names in the color of their team, the verb grey (red in a teamkill), HS orange and
// TK red. The style 1 draws the weapon icon instead of the verb
static void KF_LineVisor (char *out, kf_style_t *st, kf_entry_t *e, int row)
{
	int		y = KF_Y + row * KF_ROW_H;
	int		ty = y + KF_TEXT_DY;
	int		x = KF_X + st->pad_l;
	int		wx;
	qboolean	tk = e->killer[0] && e->killer_team == e->victim_team && e->killer_team != 'n';

	if (e->killer[0])
	{
		KF_AddText (st, e->killer_team, x, ty, e->killer);
		x += (strlen (e->killer) + 2) * KF_CHAR_W;
	}

	wx = x;
	if (st->label)
	{
		KF_AddText (st, tk ? 't' : 'l', x, ty, e->label);
		x += strlen (e->label) * KF_CHAR_W;
	}
	else
		x += e->icon->width;

	if (e->headshot)
	{
		KF_AddText (st, 'h', x + KF_CHAR_W, ty, "HS");
		x += 3 * KF_CHAR_W;
	}
	if (tk)
	{
		KF_AddText (st, 't', x + KF_CHAR_W, ty, "TK");
		x += 3 * KF_CHAR_W;
	}

	x += 2 * KF_CHAR_W;
	KF_AddText (st, e->victim_team, x, ty, e->victim);
	x += strlen (e->victim) * KF_CHAR_W;

	out += sprintf (out, "yt %d xl %d picn kf_bg_v_%d ", y, KF_X, KF_PlateWidth (x + st->pad_l - KF_X));
	if (!st->label)
		sprintf (out, "xl %d picn kf_%s ", wx, e->icon->name);
}


static void KF_DropOldest (void)
{
	kf_count--;
	memmove (&kf_entries[0], &kf_entries[1], kf_count * sizeof (kf_entry_t));
}


// the feed of the rows first .. kf_count - 1 in a style; returns its length: the pictures of all the rows, the texts
// grouped by color and the color back to white for the statusbar of the level
static int KF_Build (char *out, kf_style_t *st, int first)
{
	char	line[512];
	char	*color = NULL, *c;
	int		i, j, len = 0;

	out[0] = 0;
	kf_ntexts = 0;
	for (i = first; i < kf_count; i++)
	{
		if (st->visor)
			KF_LineVisor (line, st, &kf_entries[i], i - first);
		else
			KF_LinePlates (line, st, &kf_entries[i], i - first);
		len += sprintf (out + len, "%s", line);
	}

	for (j = 0; kf_passes[j]; j++)
	{
		for (i = 0; i < kf_ntexts; i++)
		{
			if (kf_texts[i].pass != kf_passes[j])
				continue;
			c = KF_PassColor (st, kf_passes[j]);
			if (c && c != color)
				len += sprintf (out + len, "color %s ", color = c);
			len += sprintf (out + len, "yt %d xl %d string \"%s\" ", kf_texts[i].y, kf_texts[i].x, kf_texts[i].text);
		}
	}

	if (color)
		len += sprintf (out + len, "color #fff ");
	// the feed between two words that the clients do not know (they skip them): a demo viewer with its own kill
	// feed (VisorDemos) leaves out what is between them
	if (len)
	{
		memmove (out + 9, out, len + 1);
		memcpy (out, "kf_begin ", 9);
		len += 9;
		len += sprintf (out + len, "kf_end ");
	}
	return len;
}


static int KF_ClientNum (edict_t *ent)
{
	int		n = ent - g_edicts - 1;

	if (n < 0 || n >= KF_MAX_CLIENTS || n >= game.maxclients)
		return -1;
	return n;
}


static int KF_StyleOf (edict_t *ent)
{
	int		n = KF_ClientNum (ent);
	int		style = (n >= 0) ? kf_client_style[n] : -1;

	if (style < 0)
		style = killfeed_style ? (int)killfeed_style->value : KF_STYLE_OFF;
	if (style < 0 || style >= KF_STYLES)
		style = KF_STYLE_OFF;
	return style;
}


static void KF_Unicast (edict_t *ent, char *bar)
{
	gi.WriteByte (KF_SVC_CONFIGSTRING);
	gi.WriteShort (CS_STATUSBAR);
	gi.WriteString (bar);
	gi.unicast (ent, true);
}


// the statusbar of one player: the base with his feed, or the base alone (only when he had a feed: the
// configstring of the server is the base, he already has it)
static void KF_SendTo (edict_t *ent)
{
	static char	bar[KF_BAR_MAX + 1];
	static char	feed[KF_MAX_LINES * 512];
	int			n = KF_ClientNum (ent);
	int			style, first = 0, baselen;

	if (n < 0 || !kf_base_set || !ent->inuse || !ent->client || ent->ai)
		return;

	style = KF_StyleOf (ent);
	baselen = strlen (kf_base);

	// the base keeps its place: the oldest rows are left out until everything fits
	if (style != KF_STYLE_OFF)
		while (first < kf_count && KF_Build (feed, &kf_styles[style], first) + baselen > KF_BAR_MAX)
			first++;

	if (style == KF_STYLE_OFF || first >= kf_count)
	{
		if (kf_client_feed[n])
			KF_Unicast (ent, kf_base);
		kf_client_feed[n] = false;
		return;
	}

	strcpy (bar, feed);
	strcat (bar, kf_base);
	KF_Unicast (ent, bar);
	kf_client_feed[n] = true;
}


static void KF_Send (void)
{
	int		i;

	if (!kf_base_set)
		return;

	for (i = 1; i <= game.maxclients; i++)
		KF_SendTo (&g_edicts[i]);
}


// sets the statusbar of the level (once per map) and clears the feed
// The pictures of the feed are in a pak (killfeed<N>.pkz, tools/killfeed/make_pkz.sh) that Q2PRO downloads by HTTP:
// <sv_downloadserver>dday.filelist lists it, but the client reads that list only when it has to download some
// file. KF_WITNESS is a tiny picture that is only in the pak: a client without the pak misses it, downloads it and
// with it the .filelist and the pak. A new pak goes with a new witness (kf_v2...)
#define KF_WITNESS	"/pics/kf_v2.png"	// killfeed2.pkz: the icons in high resolution (tools/killfeed/kf_icons_hd.py)

void Killfeed_SetBase (char *statusbar)
{
	gi.imageindex (KF_WITNESS);

	KF_Compact (kf_base, sizeof (kf_base), statusbar);
	kf_base_set = true;
	kf_count = 0;
	kf_dirty = false;
	gi.configstring (CS_STATUSBAR, kf_base);
	KF_Send ();
}


// the statusbar of the level changed during the map (control mode: the owner of the zone): the feed is kept
void Killfeed_UpdateBase (char *statusbar)
{
	KF_Compact (kf_base, sizeof (kf_base), statusbar);
	kf_base_set = true;
	kf_dirty = false;
	gi.configstring (CS_STATUSBAR, kf_base);
	KF_Send ();
}


// a new client: the style of the server until his userinfo or the command say another one
void Killfeed_ClientConnect (edict_t *ent)
{
	int		n = KF_ClientNum (ent);

	if (n < 0)
		return;
	kf_client_style[n] = -1;
	kf_client_feed[n] = false;
}


static void KF_SetStyle (edict_t *ent, int style)
{
	int		n = KF_ClientNum (ent);

	if (n < 0 || kf_client_style[n] == style)
		return;
	kf_client_style[n] = style;
	KF_SendTo (ent);
}


// userinfo "kf_style" (the command killfeed sets it in the client, or "set kf_style <style> u" in his config;
// "killfeed" is read too). Every change of level reloads the game library and reconnects the clients: the style
// would be lost if it were only kept here, the userinfo of the client brings it back on every connection
void Killfeed_Userinfo (edict_t *ent, char *userinfo)
{
	char	*s = Info_ValueForKey (userinfo, "kf_style");
	int		style;

	if (!s[0])
		s = Info_ValueForKey (userinfo, "killfeed");

	if (!s[0])
		return;
	style = atoi (s);
	if (style >= 0 && style < KF_STYLES)
		KF_SetStyle (ent, style);
}


// command "killfeed [style]"
void Killfeed_Cmd (edict_t *ent)
{
	int		i, style, current = KF_StyleOf (ent);

	if (gi.argc () < 2)
	{
		safe_cprintf (ent, PRINT_HIGH, "Estilos del killfeed (killfeed <numero>):\n");
		for (i = 0; i < KF_STYLES; i++)
			safe_cprintf (ent, PRINT_HIGH, "%s %d  %s\n", (i == current) ? "->" : "  ", i, kf_styles[i].name);
		safe_cprintf (ent, PRINT_HIGH, "Se mantiene al cambiar de mapa. Para dejarlo fijo siempre, pon en tu autoexec.cfg: set kf_style <numero> u\n");
		return;
	}

	style = atoi (gi.argv (1));
	if (style < 0 || style >= KF_STYLES)
	{
		safe_cprintf (ent, PRINT_HIGH, "killfeed: el estilo va de 0 a %d (killfeed sin numero muestra la lista)\n", KF_STYLES - 1);
		return;
	}

	KF_SetStyle (ent, style);
	// kept in the userinfo of the client, so that it comes back after every change of level (see Killfeed_Userinfo)
	stuffcmd (ent, va ("set kf_style %d u\n", style));
	safe_cprintf (ent, PRINT_HIGH, "Killfeed: %d %s\n", style, kf_styles[style].name);
}


// the text obituary is not printed to a player who sees the feed (killfeed_console 1 prints it anyway: the console
// keeps the history of the deaths; 0 by default, the client cannot keep the text out of the notify area, it would
// show up on top of the feed)
qboolean Killfeed_Shows (edict_t *ent)
{
	return !killfeed_console->value && KF_StyleOf (ent) != KF_STYLE_OFF;
}


// copy of a piece of the obituary without control chars, quotes or the spaces at both ends
static void KF_CleanCopy (char *dst, int size, const char *src, int len)
{
	int		n = 0;
	int		c;

	while (len-- > 0 && *src && n + 1 < size)
	{
		c = *src++ & 127;
		if (c < 32 || c == '"' || (c == ' ' && !n))
			continue;
		dst[n++] = c;
	}
	while (n && dst[n - 1] == ' ')
		n--;
	dst[n] = 0;
}


// the verb of VisorDemos (src/client/feed.c, make_label and add_death), from the pieces of the obituary:
// mid is the message ("was sniped by", "tripped on his own grenade"), tail the weapon ("'s light machine gun")
static void KF_MakeLabel (char *dst, int size, qboolean killer, const char *mid, const char *tail)
{
	char		weapon[KF_LABEL_LEN + 1] = "";
	const char	*end;
	int			len;

	if (!mid)
		mid = "";
	len = strlen (mid);

	if (tail && !strncmp (tail, "'s ", 3))
	{
		tail += 3;
		for (end = tail; *end && *end != '\n' && *end != ')' && strncmp (end, " - ", 3) && strncmp (end, " (", 2); end++)
			;
		KF_CleanCopy (weapon, sizeof (weapon), tail, end - tail);
		if (!Q_stricmp (weapon, "handgrenade"))
			strcpy (weapon, "grenade");
		else if (!Q_stricmp (weapon, "explosive attack"))
			strcpy (weapon, "explosive");
	}

	if (!killer)
	{
		// a death without killer
		if (strstr (mid, "killed himself") || strstr (mid, "killed herself") || strstr (mid, "suicide"))
			KF_CleanCopy (dst, size, "suicide", 7);
		else if (strstr (mid, "blew himself up") || strstr (mid, "blew herself up"))
			KF_CleanCopy (dst, size, "blew up", 7);
		else if (strstr (mid, "fell to"))
			KF_CleanCopy (dst, size, "fell", 4);
		else
		{
			KF_CleanCopy (dst, size, mid, len);
			len = strlen (dst);
			if (len && dst[len - 1] == '.')
				dst[len - 1] = 0;
		}
		return;
	}

	// "was sniped by" -> "sniped"; "was killed by X's airstrike" -> "airstrike"
	if (!strncmp (mid, "was ", 4) && len > 7 && !strcmp (mid + len - 3, " by"))
	{
		if (weapon[0] && !strcmp (mid, "was killed by"))
			KF_CleanCopy (dst, size, weapon, strlen (weapon));
		else
			KF_CleanCopy (dst, size, mid + 4, len - 7);
		return;
	}
	if (strstr (mid, "wounded by") || !tail)
	{
		// the self kills of a wounded player name who wounded him: "tripped on his own grenade (after being wounded by"
		KF_CleanCopy (dst, size, "wounded", 7);
		return;
	}
	if (strstr (mid, "wounds"))
	{
		KF_CleanCopy (dst, size, "wounds", 6);
		return;
	}
	if (strstr (mid, "fell to"))
	{
		KF_CleanCopy (dst, size, "fell", 4);
		return;
	}
	if (weapon[0])
		KF_CleanCopy (dst, size, weapon, strlen (weapon));
	else
		KF_CleanCopy (dst, size, mid, len);
}


// a death: killer is NULL (or not a client) for suicides and the environment
// returns true when the death is on the feed: the text obituary is printed only to the players without it
// (see Killfeed_Shows)
qboolean Killfeed_Add (edict_t *killer, edict_t *victim, int mod, qboolean headshot, char *mid, char *tail)
{
	kf_entry_t	*e;

	if (!victim || !victim->client)
		return false;

	// same visibility as the console obituaries: with death_msg 0 and 2 they are not for everybody
	if (death_msg->value != 1 && death_msg->value != 3)
		return false;

	if (nohud->value)
		return false;

	if (kf_count == KF_MAX_LINES)
		KF_DropOldest ();

	e = &kf_entries[kf_count++];
	KF_CopyName (e->victim, victim->client->pers.netname);
	if (killer && (!killer->inuse || !killer->client))
		killer = NULL;	// last_wound_inflictor can be a player who left
	if (killer && killer != victim)
		KF_CopyName (e->killer, killer->client->pers.netname);
	else
		e->killer[0] = 0;
	e->icon = KF_ChooseIcon (killer, victim, mod);
	e->killer_fac = e->killer[0] ? KF_ChooseFaction (killer) : NULL;
	e->victim_fac = KF_ChooseFaction (victim);
	e->headshot = headshot;
	KF_MakeLabel (e->label, sizeof (e->label), e->killer[0] != 0, mid, tail);
	e->killer_team = KF_TeamOf (killer);
	e->victim_team = KF_TeamOf (victim);
	e->expire = level.time + KF_LIFETIME;

	kf_dirty = true;
	return true;
}


// once per server frame: removes the lines that expired and sends the feed when it changed
void Killfeed_Frame (void)
{
	while (kf_count && level.time >= kf_entries[0].expire)
	{
		KF_DropOldest ();
		kf_dirty = true;
	}

	if (!kf_dirty)
		return;

	kf_dirty = false;
	KF_Send ();
}
