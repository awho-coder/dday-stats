/*       D-Day: Normandy by Vipersoft
 ************************************
 *   $Source: /usr/local/cvsroot/dday/src/g_arty.c,v $
 *   $Revision: 1.11 $
 *   $Date: 2002/06/04 19:49:45 $
 * 
 ***********************************

Copyright (C) 2002 Vipersoft

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "g_local.h"
#include "game.h"
#include "m_player.h"
#include "q_shared.h"

// g_arty.c
// D-Day: Normandy Artillery and Airstrikes

//////////////////////////////////////////////////////////////////////
//   This file is for handeling artillery and airstrike functions   //
//////////////////////////////////////////////////////////////////////

/* user definable variables 

	arty_delay  -- seconds for artillary to position           default: 10 
	arty_time   -- seconds between each volley                 default: 60
	arty_max    -- number of shots to be fired in each volley  default: 1
*/
void airstrike_touch (edict_t *ent, edict_t *other, cplane_t *plane, csurface_t *surf);
void Airstrike_Plane_Launch(edict_t *ent);
void fire_Knife ( edict_t *self, vec3_t start, vec3_t aimdir, int damage, int kick, char *wav, qboolean fists);
void P_ProjectSource(gclient_t * client , vec3_t point , vec3_t distance , vec3_t forward , vec3_t right , vec3_t result );
void Weapon_Bayonet_Fire1 (edict_t *ent)
{
	vec3_t  forward, right;
    vec3_t  start;
    vec3_t  offset;
	vec3_t g_offset;
	
	VectorCopy (vec3_origin,g_offset);

    AngleVectors (ent->client->v_angle, forward, right, NULL);
    VectorSet(offset, 40, 8, ent->viewheight-8);
    VectorAdd (offset, g_offset, offset);
    P_ProjectSource (ent->client, ent->s.origin, offset, forward, right, start);

    VectorScale (forward, -2, ent->client->kick_origin);
    ent->client->kick_angles[0] = -1;
 
	//gi.dprintf("going to fire_knife\n");
	fire_Knife (ent, start, forward, 0, 0, "gbr/bayonet/hit.wav", 0);

	ent->client->ps.gunframe++;

//	Play_WepSound(ent, (armedfists)?"fists/fire.wav":"knife/fire.wav");  //faf
	gi.sound(ent, CHAN_WEAPON, gi.soundindex("gbr/bayonet/swipe.wav"), 1, ATTN_NORM, 0);//faf

	//	gi.sound(ent, CHAN_VOICE, gi.soundindex("*jump1.wav"), 1, ATTN_NORM, 0);
	PlayerNoise(ent, ent->s.origin, PNOISE_SELF);



	if (ent->oldstance == ent->stanceflags //faf: not changing stances
		&& ent->client->pers.weapon && !ent->deadflag)
	{
		if (ent->stanceflags == STANCE_STAND)
        {
			ent->client->anim_priority = ANIM_REVERSE;
	        ent->s.frame = FRAME_pain304+1;
            ent->client->anim_end = FRAME_pain304;            
        }
        else if (ent->stanceflags == STANCE_DUCK)
        {
			ent->s.frame = FRAME_crwalk4;
            ent->client->anim_end = FRAME_crwalk6;
        }
        else if (ent->stanceflags == STANCE_CRAWL)
        {
            ent->s.frame = FRAME_crawlpain01;
            ent->client->anim_end = FRAME_crawlpain04;
        }
	}



}
mmove_t civilian_move_stand;
qboolean CheckForTurret(edict_t *ent);
void turret_off (edict_t *self);
void Use_Weapon (edict_t *ent, gitem_t *item);
void Arty_Sound (edict_t *ent);
/*
=================
Cmd_Arty_f
CCH: new function to call in airstrikes
=================
*/
void Cmd_Arty_f (edict_t *ent)
{
/*	vec3_t	start;
	vec3_t	forward;
	vec3_t	end;
	vec3_t world_up;

	edict_t *airstrike;//faf
	trace_t	tr;*/

	//int randnum;	
	vec3_t v;
	edict_t *check;
	int e;


	if (!ent->client)
		return;

	if (ent->client->kamikaze_plane)	// en el avion no hay vuelta atras
		return;

	if (!IsValidPlayer(ent))
		return;



	if (ent->deadflag)
		return;


	if (!ent->client->resp.team_on)
		return;


	Spawn_Chute_Special (ent);

	//faf:  turret stuff
	if (!ent->client->turret &&
		!ent->client->grenade)
	{
		if	(CheckForTurret(ent))
		{
			Use_Weapon (ent, FindItem("Fists"));
			CheckForTurret(ent);
			return;
		}
	}
	else
	{
		turret_off(ent);
		return;
	}
			
	if (ent->client &&
		ent->client->pers.weapon &&
		ent->client->pers.weapon->classnameb == WEAPON_CARCANO)
	{
		if (ent->client->weaponstate == WEAPON_READY &&
			ent->client->ps.gunframe < 104 &&
			!ent->client->aim)
		{
			ent->client->ps.gunframe = 104;
			Weapon_Bayonet_Fire1 (ent);
			return;
		}
		return;
	}


	if (ent->client &&
		ent->client->pers.weapon &&
		ent->client->pers.weapon->classnameb == WEAPON_ENFIELD)
	{
		if (ent->client->weaponstate == WEAPON_READY &&
			ent->client->ps.gunframe < 104 &&
			!ent->client->aim)
		{
			ent->client->ps.gunframe = 104;
			Weapon_Bayonet_Fire1 (ent);
			return;
		}
		return;
	}


	if (ent->client &&
		ent->client->pers.weapon &&
		ent->client->pers.weapon->classnameb == WEAPON_SVT)
	{
		if (ent->client->weaponstate == WEAPON_READY &&
			ent->client->ps.gunframe < 89 &&
			!ent->client->aim)
		{
			ent->client->ps.gunframe = 89;
			Weapon_Bayonet_Fire1 (ent);
			return;
		}
		return;
	}




	if (ent->client->airstrike)
	{
		safe_cprintf(ent, PRINT_HIGH, "Airstrike cancelled sir!\n");

		// kernel: cancel just one arty
		if (ent->client->resp.team_on->arty_num > 0)
			ent->client->resp.team_on->arty_num--;
		else
			ent->client->resp.team_on->arty_num = 0;

		G_FreeEdict(ent->client->airstrike);
		ent->client->airstrike = NULL;
		return;
	}
	else if ((ent->client->pers.weapon &&
		ent->client->pers.weapon->classnameb) &&
		ent->client->pers.weapon->classnameb == WEAPON_BINOCULARS)
	{
		if (ent->client->aim)
			safe_cprintf(ent, PRINT_HIGH, "Press fire to call an airstrike!\n");
		else
			safe_cprintf(ent, PRINT_HIGH, "Aim and then press fire to call an airstrike!\n");
	}







//	if (ent->client->resp.mos != OFFICER) {
//		safe_cprintf(ent, PRINT_HIGH, "You're not an officer, soldier!\n");
//		return;
//	}

	if (ent->deadflag || ent->client->limbo_mode ) {
		safe_cprintf(ent, PRINT_HIGH, "You are dead!\n");
		return;
	}




	//faf:  moving this up so you dont have to look through binocs to cancel arty
	// make sure artillary hasn't already been called

	


	check = g_edicts+1;
	for (e = 1; e < globals.num_edicts; e++, check++)
	{
		if (!check->inuse)
			continue;
		if (check->health < 1)
			continue;
		if (strcmp(check->classname, "misc_civilian"))
			continue;


		VectorSubtract (ent->s.origin, check->s.origin, v);

		if (VectorLength (v)< 100)
		{
			//gi.dprintf("CIV FOUND\n");
			if (check->master == ent)
			{
				check->master = NULL;
				//gi.dprintf("STAY\n");
				gi.sound (ent, CHAN_AUTO, gi.soundindex("misc/trigger1.wav"), .5, ATTN_STATIC, 0);
				check->goalentity = NULL;
				check->movetarget = NULL;
				check->enemy = NULL;
				check->monsterinfo.currentmove = &civilian_move_stand;
			}
			else
			{
				if (check->master && check->obj_owner == ent->client->resp.team_on->index)
					return; // can't steal civilian from teammate

				gi.sound (ent, CHAN_AUTO, gi.soundindex("misc/trigger1.wav"), .5, ATTN_STATIC, 0);

				check->master = ent;
				//gi.dprintf("FOLLOW\n");
				if (check->obj_owner != ent->client->resp.team_on->index)
				{	
					level.obj_team = ent->client->resp.team_on->index;
					check->obj_owner = ent->client->resp.team_on->index;
					check->wait =level.time;
					//if (!check->wait) check->wait =level.time;
				}


			}
			check->obj_owner = ent->client->resp.team_on->index; 
		}

	}



}



void fire_airstrike (edict_t *self, vec3_t start, vec3_t dir, int damage, int speed, float damage_radius, int radius_damage);//faf

 
//faf:  deleted a lot of stuff here from 4.1  airstrike/arty

//put back in for ddaylife

void Think_Arty (edict_t *ent)
{
	vec3_t	start;
	vec3_t	forward;
	vec3_t	end;
	vec3_t	targetdir;
	vec3_t  tempvec;
	//trace_t	tr;
	trace_t tr_2;


/* kernel: this is not necessary
		if (!ent->owner ||
		!ent->owner->client ||
		!ent->owner->inuse ||
		!ent->owner->client->resp.team_on ||
		ent->owner->flyingnun)
	{
		G_FreeEdict(ent);
		return;
	}

	if (!ent->owner->client->airstrike)
	{
		G_FreeEdict(ent);
		return;
	}

	if (ent->owner->client->airstrike &&
		ent->owner->client->airstrike != ent)
	{
		G_FreeEdict(ent);
		return;
	}
*/





	// find the target point
	VectorCopy(ent->owner->s.origin, start);
	start[2] += ent->owner->viewheight;
	AngleVectors(ent->owner->client->v_angle, forward, NULL, NULL);
	VectorMA(start, 8192, forward, end);
	//tr = gi.trace(start, NULL, NULL, end, ent->owner, MASK_SHOT|CONTENTS_SLIME|CONTENTS_LAVA);

	// find the direction from the entry point to the target
    VectorSubtract(ent->arty_target, ent->arty_entry, targetdir);
    VectorNormalize(targetdir);
    VectorAdd(ent->arty_entry, targetdir, start);
	
   // check we have a clear line of fire
    tr_2 = gi.trace(start, NULL, NULL, ent->arty_target, ent, MASK_SHOT|CONTENTS_SLIME|CONTENTS_LAVA);

	// check to make sure we're not materializing in a solid
	if ( gi.pointcontents(start) == CONTENTS_SOLID || tr_2.fraction < 1.0 )
	{
		safe_cprintf(ent->owner, PRINT_HIGH, "Artillery fire was unsuccessful, sir!\n");
		return;
	}

	
//    gi.sound(ent, CHAN_AUTO, gi.soundindex(va("%s/arty/hit%i.wav", ent->client->resp.team_on->teamid, ent->client->arty_location)), 1, ATTN_NORM, 0);
  //		if (IsValidPlayer(ent))
	//		gi.sound(ent->owner, CHAN_AUTO, gi.soundindex(va("%s/arty/hit%i.wav", ent->owner->client->resp.team_on->teamid, 1)), 1, ATTN_NORM, 0);
     

	// fire away!

    /* old arty code
    fire_rocket(ent, start, targetdir, 700, 250, 300, 450);
    fire_rocket(ent, start, targetdir, 600, 450, 200, 430);
    fire_rocket(ent, start, targetdir, 400, 150, 400, 500);
    fire_rocket(ent, start, targetdir, 600, 210, 250, 500);
    fire_rocket(ent, start, targetdir, 300, 430, 200, 450);
    fire_rocket(ent, start, targetdir, 600, 240, 320, 480);
    */

	fire_airstrike(ent->owner, start, targetdir, 700, 250, 300, 450);
	VectorSet(tempvec, 8, 8, 0);
	VectorAdd(tempvec, start, tempvec);
    fire_airstrike(ent->owner, tempvec, targetdir, 600, 450, 200, 430);
	VectorSet(tempvec, 16, 16, 0);
	VectorAdd(tempvec, start, tempvec);
    fire_airstrike(ent->owner, tempvec, targetdir, 400, 150, 400, 500);
	VectorSet(tempvec, 24, 24, 0);
	VectorAdd(tempvec, start, tempvec);
    fire_airstrike(ent->owner, tempvec, targetdir, 600, 210, 250, 500);
	VectorSet(tempvec, 32, 32, 0);
	VectorAdd(tempvec, start, tempvec);
    fire_airstrike(ent->owner, tempvec, targetdir, 300, 430, 200, 450);
	VectorSet(tempvec, 40, 40, 0);
	VectorAdd(tempvec, start, tempvec);
    fire_airstrike(ent->owner, tempvec, targetdir, 600, 240, 320, 480);

	//fire_shell(ent, start, targetdir, 250, ((rand()%500)+900), 300, 75); 	
	
	safe_cprintf(ent->owner, PRINT_HIGH, "Artillery fire confirmed, sir!\n");


	ent->think = G_FreeEdict;
	ent->nextthink = level.time +.1;

	ent->owner->client->airstrike = NULL;

	// kernel: airstrike was already restricted when it was called
	//ent->owner->client->resp.team_on->arty_time_restrict = level.time + arty_time->value; // delay for user defined minutes
}

void Arty_Sound (edict_t *ent)
{
/* kernel: this is not necessary
	if (!ent->owner ||
		!ent->owner->client ||
		!ent->owner->inuse ||
		!ent->owner->client->resp.team_on ||
		ent->owner->flyingnun)
	{
		G_FreeEdict(ent);
		return;
	}

	if (!ent->owner->client->airstrike)
	{
		G_FreeEdict(ent);
		return;
	}

	if (ent->owner->client->airstrike &&
		ent->owner->client->airstrike != ent)
	{
		G_FreeEdict(ent);
		return;
	}
*/
	gi.sound(ent->owner, CHAN_AUTO, gi.soundindex(va("%s/arty/fire.wav", ent->arty_teamid)), 1, ATTN_NONE, 0);
	ent->think = Think_Arty;
	ent->nextthink = level.time +3;
}


//faf

void Drop_Bomb(edict_t *ent)
{
	edict_t *bomb;
	vec3_t forward, bomb_orig;

	AngleVectors (ent->s.angles, forward, NULL, NULL);
	VectorMA (ent->s.origin, -33, forward, bomb_orig);


	bomb = G_Spawn();
	VectorCopy (bomb_orig, bomb->s.origin);

    bomb->movetype     = MOVETYPE_BOUNCE;
    bomb->clipmask     = MASK_SHOT;
    bomb->solid        = SOLID_BBOX;

	VectorClear (bomb->mins);
	VectorClear (bomb->maxs);

	bomb->owner        = ent->owner;
    
	bomb->touch        = airstrike_touch;

	bomb->nextthink    = level.time + 20;
    bomb->think        = G_FreeEdict;

	bomb->dmg          = 700;


	VectorCopy (ent->movedir, bomb->movedir);
	VectorCopy (ent->s.angles, bomb->s.angles);
//	bomb->s.angles[0]= bomb->s.angles[0] *-1;
//	bomb->s.angles[1]= bomb->s.angles[1] *-1;
//	VectorCopy (ent->velocity, bomb->velocity);
	

	bomb->s.modelindex = gi.modelindex ("models/objects/rocket/tris.md2");


	bomb->radius_dmg = 700;
	bomb->dmg_radius = 300;
	bomb->s.sound = gi.soundindex ("weapons/rockfly.wav");
	bomb->classname = "bomb";
	bomb->gravity = 1; // faf

	gi.linkentity (bomb);
}

void Plane_Fly_Off (edict_t *ent)
{
	ent->nextthink = level.time + 1;

	if (ent->s.angles[2] > 15)
	{
			VectorSet (ent->avelocity, 0, 0, -7);//faf
	}
	if (ent->s.angles[2] < -15) 
	{
			VectorSet (ent->avelocity, 0, 0, 7);//faf
	}

	if (ent->s.angles[2] > -1 && ent->s.angles[2] < 1)
	{
		VectorClear (ent->avelocity);
	}

	if (ent->leave_limbo_time < level.time - 20)
	{
		ent->nextthink= level.time + .1;
		ent->think = G_FreeEdict;
	}
}

//////////////////////////////////////////////////////////////////////
//  Kamikaze: el Officer viaja en el avion del airstrike y se estrella //
//////////////////////////////////////////////////////////////////////

#define KAMIKAZE_CAM_BACK		96		// camara detras del avion (ajustar en prueba)
#define KAMIKAZE_CAM_UP			32		// y por encima
#define KAMIKAZE_DIVE_PITCH		75		// grados bajo la horizontal al final de la picada
#define KAMIKAZE_PITCH_STEP0	2		// grados por frame al empezar a picar
#define KAMIKAZE_PITCH_STEP		8		// grados por frame a pleno (tope)
#define KAMIKAZE_PITCH_RAMP		0.5		// segundos que tarda el paso en ir de STEP0 a STEP
#define KAMIKAZE_ROLL_STEP		3		// grados por frame con los que se endereza el alabeo
#define KAMIKAZE_DIVE_ACCEL		300		// u/s2 que acelera el avion mientras cae
#define KAMIKAZE_DIVE_SPEEDCAP	1.6		// velocidad maxima: este multiplo de la que traia
#define KAMIKAZE_DIVE_MAX		6		// segundos maximos de picada antes de forzar el choque

void check_unscope (edict_t *ent);
void Play_Ricochet_Noise (edict_t *ent, vec3_t origin);
void Smoke_Effect (vec3_t origin, float strength);

void Plane_Dive (edict_t *plane);

// el piloto sigue vivo y enganchado a este avion?
static qboolean Kamikaze_IsPilot (edict_t *plane, edict_t *p)
{
	return p && p->inuse && p->client && !p->deadflag &&
		p->client->kamikaze_plane == plane;
}

// el avion del piloto sigue existiendo? (el edict se puede liberar y reusar)
qboolean Kamikaze_PlaneValid (edict_t *p)
{
	edict_t *plane = p->client->kamikaze_plane;

	return plane && plane->inuse && plane->classnameb == PLANE &&
		plane->owner == p && (plane->spawnflags & PLANE_KAMIKAZE);
}

// devuelve a un piloto vivo al juego normal, en su spawn
static void Kamikaze_PutBack (edict_t *p)
{
	p->movetype = MOVETYPE_WALK;
	p->solid = SOLID_BBOX;
	VectorClear (p->velocity);

	// como turret_off: restaurar el arma en vista
	if (p->client->pers.weapon)
		p->client->ps.gunindex = gi.modelindex (p->client->pers.weapon->view_model);

	MoveToTheirSpawnPoint (p);	// actualiza pmove.origin y linkea
	gi.linkentity (p);
}

// desengancha al jugador del avion. to_spawn: si sigue vivo, mandarlo a su spawn
void Kamikaze_Release (edict_t *p, qboolean to_spawn)
{
	if (!p->client || !p->client->kamikaze_plane)
		return;

	p->client->kamikaze_plane = NULL;
	p->svflags &= ~SVF_NOCLIENT;
	p->client->ps.pmove.pm_flags &= ~PMF_NO_PREDICTION;

	if (to_spawn && !p->deadflag)
		Kamikaze_PutBack (p);
}

// pone la camara del piloto junto al avion (llamar en cada think del avion)
static void Kamikaze_Follow (edict_t *plane)
{
	edict_t *p = plane->owner;
	vec3_t forward;
	int i;

	if (!(plane->spawnflags & PLANE_KAMIKAZE) || !Kamikaze_IsPilot (plane, p))
		return;

	// en la picada la vista queda fija mirando por la trompa (con el roll del avion);
	// antes de eso se puede mirar libre. Igual que turret_driver_link.
	if (plane->think == Plane_Dive)
	{
		for (i = 0; i < 3; i++)
			p->client->ps.pmove.delta_angles[i] =
				ANGLE2SHORT(plane->s.angles[i] - p->client->resp.cmd_angles[i]);
		VectorCopy (plane->s.angles, p->client->ps.viewangles);
		VectorCopy (plane->s.angles, p->client->v_angle);
	}

	AngleVectors (plane->s.angles, forward, NULL, NULL);
	VectorMA (plane->s.origin, -KAMIKAZE_CAM_BACK, forward, p->s.origin);
	p->s.origin[2] += KAMIKAZE_CAM_UP;
	VectorClear (p->velocity);
	p->client->ps.gunindex = 0;	// por si algun comando le cambio el arma
	p->client->ps.pmove.pm_flags |= PMF_NO_PREDICTION;
	gi.linkentity (p);
}

// sube al Officer al avion recien creado
static void Kamikaze_LinkPilot (edict_t *plane)
{
	edict_t *p = plane->owner;
	int i;

	if (!p || !p->inuse || !p->client || p->ai || p->deadflag ||
		p->client->limbo_mode || p->flyingnun || !p->client->resp.team_on ||
		p->client->kamikaze_plane)	// ya va en otro avion
		return;

	if (p->client->turret)
		turret_off (p);

	// sale del zoom de los binoculares
	check_unscope (p);
	p->client->aim = false;
	p->client->ps.fov = STANDARD_FOV;

	plane->spawnflags |= PLANE_KAMIKAZE;
	p->client->kamikaze_plane = plane;

	p->solid = SOLID_NOT;
	p->movetype = MOVETYPE_NOCLIP;	// en ClientThink tambien evita G_TouchTriggers
	p->groundentity = NULL;
	p->svflags |= SVF_NOCLIENT;		// que no se vea su cuerpo flotando junto al avion
	VectorClear (p->velocity);
	p->client->ps.gunindex = 0;
	p->client->ps.pmove.pm_type = PM_FREEZE;
	p->client->ps.pmove.pm_flags |= PMF_NO_PREDICTION;

	// apuntar la vista hacia donde vuela (despues puede mirar libre)
	for (i = 0; i < 3; i++)
		p->client->ps.pmove.delta_angles[i] =
			ANGLE2SHORT(plane->s.angles[i] - p->client->resp.cmd_angles[i]);
	VectorCopy (plane->s.angles, p->client->ps.viewangles);
	VectorCopy (plane->s.angles, p->client->v_angle);

	Kamikaze_Follow (plane);
}

// arranca la picada: no toca angulos ni velocidad, eso lo hace Plane_Dive de a poco
static void Plane_StartDive (edict_t *plane)
{
	plane->speed = VectorLength (plane->velocity);	// velocidad con la que venia
	plane->timestamp = level.time;
	plane->think = Plane_Dive;
	plane->nextthink = level.time + FRAMETIME;
}

// choque: mata al piloto (suicidio), explota y libera el avion
static void Plane_Crash (edict_t *plane, vec3_t point)
{
	edict_t *p = plane->owner;
	int n;

	VectorCopy (point, plane->s.origin);
	VectorClear (plane->velocity);
	gi.linkentity (plane);

	// 1) el piloto muere primero y como suicidio (antes del dano radial, para que no
	//    lo mate la explosion con otro MOD)
	if (Kamikaze_IsPilot (plane, p))
	{
		Kamikaze_Release (p, false);
		VectorCopy (point, p->s.origin);
		gi.linkentity (p);
		p->client->last_wound_inflictor = NULL;	// que nadie se lleve la kill
		T_Damage (p, plane, world, vec3_origin, p->s.origin, vec3_origin,
				  10000, 0, DAMAGE_NO_PROTECTION, MOD_SUICIDE);
		if (!p->deadflag)	// p.ej. freeze/cuenta de torneo: T_Damage no hizo nada
			Kamikaze_PutBack (p);
	}

	// 2) explosion mas fuerte que la bomba; atacante = Officer (igual que las bombas)
	T_RadiusDamage (plane, plane->owner, kamikaze_dmg->value, NULL,
					kamikaze_radius->value, MOD_AIRSTRIKE_SPLASH);

	// 3) efectos: explosion de cohete (o de agua) + explosion grande encima
	gi.WriteByte (svc_temp_entity);
	if (gi.pointcontents (point) & MASK_WATER)
		gi.WriteByte (TE_ROCKET_EXPLOSION_WATER);
	else
		gi.WriteByte (TE_ROCKET_EXPLOSION);
	gi.WritePosition (point);
	gi.multicast (point, MULTICAST_PHS);

	gi.WriteByte (svc_temp_entity);
	gi.WriteByte (TE_EXPLOSION2);
	gi.WritePosition (point);
	gi.multicast (point, MULTICAST_PHS);

	n = 3 + rand() % 3;
	while (n--)
		ThrowDebris (plane, "models/objects/debris2/tris.md2", 2, point);

	Play_Ricochet_Noise (plane, point);
	Smoke_Effect (point, .1);

	G_FreeEdict (plane);
}

// normaliza un angulo a -180..180
static float Kamikaze_Angle180 (float a)
{
	while (a > 180)
		a -= 360;
	while (a < -180)
		a += 360;
	return a;
}

// picada: la trompa baja de a poco, el avion acelera y cae hasta chocar
void Plane_Dive (edict_t *plane)
{
	vec3_t end, point, forward;
	trace_t tr;
	float t, step, pitch, roll, speed;

	// pitch: positivo = trompa abajo; el paso crece de STEP0 a STEP durante PITCH_RAMP
	t = level.time - plane->timestamp - FRAMETIME;
	if (t < 0)
		t = 0;
	if (t > KAMIKAZE_PITCH_RAMP)
		t = KAMIKAZE_PITCH_RAMP;
	step = KAMIKAZE_PITCH_STEP0 + (KAMIKAZE_PITCH_STEP - KAMIKAZE_PITCH_STEP0) * t / KAMIKAZE_PITCH_RAMP;

	pitch = Kamikaze_Angle180 (plane->s.angles[PITCH]);
	if (pitch < KAMIKAZE_DIVE_PITCH)
	{
		pitch += step;
		if (pitch > KAMIKAZE_DIVE_PITCH)
			pitch = KAMIKAZE_DIVE_PITCH;
	}
	plane->s.angles[PITCH] = pitch;

	// roll: se endereza de a poco; sin avelocity para que no siga el alabeo de Plane_Think
	roll = Kamikaze_Angle180 (plane->s.angles[ROLL]);
	if (roll > KAMIKAZE_ROLL_STEP)
		roll -= KAMIKAZE_ROLL_STEP;
	else if (roll < -KAMIKAZE_ROLL_STEP)
		roll += KAMIKAZE_ROLL_STEP;
	else
		roll = 0;
	plane->s.angles[ROLL] = roll;
	VectorClear (plane->avelocity);

	// velocidad: acelera como si cayera, con tope; la direccion sale de los angulos
	speed = VectorLength (plane->velocity) + KAMIKAZE_DIVE_ACCEL * FRAMETIME;
	if (speed > plane->speed * KAMIKAZE_DIVE_SPEEDCAP)
		speed = plane->speed * KAMIKAZE_DIVE_SPEEDCAP;
	AngleVectors (plane->s.angles, forward, NULL, NULL);
	VectorScale (forward, speed, plane->velocity);

	Kamikaze_Follow (plane);

	// el pusher se mueve velocity*FRAMETIME antes del proximo think: mirar ese tramo
	VectorMA (plane->s.origin, FRAMETIME, plane->velocity, end);
	tr = gi.trace (plane->s.origin, NULL, NULL, end, plane, MASK_SHOT | MASK_WATER);

	// allsolid y no startsolid: el avion arranca pegado a la superficie del cielo
	if (tr.allsolid || tr.fraction < 1.0 || level.time > plane->timestamp + KAMIKAZE_DIVE_MAX)
	{
		if (tr.fraction < 1.0 && !tr.allsolid)
			VectorMA (tr.endpos, 8, tr.plane.normal, point);	// un poco afuera de la pared
		else
			VectorCopy (plane->s.origin, point);
		Plane_Crash (plane, point);
		return;
	}

	plane->nextthink = level.time + FRAMETIME;
}

void Plane_Fire (edict_t *ent)
{
	Kamikaze_Follow (ent);

	if (ent->leave_limbo_time < level.time - 20)
	{
		ent->nextthink= level.time + .1;
		ent->think = G_FreeEdict;
	}


	if (ent->count >= 4)
	{
		if (ent->spawnflags & PLANE_KAMIKAZE)
			Plane_StartDive (ent);
		else
		{
			ent->think = Plane_Fly_Off;
			ent->nextthink = level.time + .1;
		}
	}
	else
	{
		// kernel: fast plane also needs to drop bombs faster
		if (fast_arty->value)
			ent->nextthink = level.time + .1;
		else
			ent->nextthink = level.time + .2;
	}

	//if (ent->count == 0  && ent->owner->client) // kernel: no need to check client references
	if (ent->count == 0)
		safe_centerprintf(ent->owner, "Airstrike confirmed, sir!\n");

	Drop_Bomb(ent);
	ent->count++;
}





void Plane_Think (edict_t *ent)
{
	vec3_t	length;
	float   distance, drop_distance;
	edict_t *botwarn;
	
/* kernel: this is not necessary
	if (!ent->owner ||
		!ent->owner->client ||
		ent->flyingnun)
	
	{
		G_FreeEdict(ent);
		return;
	}
*/

	// kernel: arty_entry was copied from the airstrike entity
	VectorSubtract(ent->s.origin, ent->arty_entry, length);
	distance = VectorLength(length);

	if (distance < 1000)
	{
		if (ent->s.angles[2] == 0 && ent->avelocity[2] == 0) //not spinning yet
		{
			//warn bots to run!
			botwarn = G_Spawn();
			botwarn->movetype = MOVETYPE_NONE;
			botwarn->solid = SOLID_TRIGGER;
			botwarn->think = G_FreeEdict;
			botwarn->nextthink = level.time + 4;
			VectorClear (botwarn->mins);
			VectorClear (botwarn->maxs);
			botwarn->classname = "botwarn";
			botwarn->classnameb = BOTWARN;
			VectorCopy (ent->arty_entry, botwarn->s.origin);

			//tilt plane
			if (random() < .5)
			{
				VectorSet (ent->avelocity, 0, 0, -10);//faf
			}
			else
			{
				VectorSet (ent->avelocity, 0, 0, 10);//faf
			}

		}


	}
	if (ent->s.angles[2] > 15) //don't tip too far
	{
			VectorSet (ent->avelocity, 0, 0, 0);//faf
	}
	if (ent->s.angles[2] < -15) 
	{
			VectorSet (ent->avelocity, 0, 0, 0);//faf
	}


//	safe_bprintf (PRINT_HIGH, "%s \n", vtos(ent->s.angles));

	// kernel: fast plane needs to drop bombs earlier
	if (fast_arty->value)
		drop_distance = 400;
	else
		drop_distance = 200;

	if (distance < drop_distance)
	{
		ent->think = Plane_Fire;

		if (IsValidPlayer(ent))
			gi.sound(ent->owner, CHAN_AUTO, gi.soundindex(va("%s/arty/hit%i.wav", ent->arty_teamid, 1)),
					 1, ATTN_NORM, 0);
	}

	Kamikaze_Follow (ent);

	ent->nextthink = level.time +.1;
}

void plane_die(edict_t * self , edict_t * inflictor , edict_t * attacker , int damage , vec3_t point )
{
	gi.WriteByte (svc_temp_entity);
	gi.WriteByte (TE_EXPLOSION2);
	gi.WritePosition (self->s.origin);
	gi.multicast (self->s.origin, MULTICAST_PHS);
	G_FreeEdict(self);

}

//faf
// kernel: now ent is the airstrike
void Spawn_Plane(edict_t *ent)
{
	//faf
	vec3_t	start;
	vec3_t	end;
	vec3_t left,right,forward,back;
	vec3_t temp, longest, direction, plane_start;

	float speed;

//	int i,j;

	trace_t	tr;

	edict_t *plane;

	VectorSet(plane_start, 0, 0, 0);

	/* kernel: not checking this because all info is in the airstrike ent
	if (IsValidPlayer(ent) && 
		ent->client && ent->client->arty_entry)
	{
		VectorCopy(ent->client->arty_entry, start);
	}
	else
		return; */

	// kernel: copy the starting position
	VectorCopy(ent->arty_entry, start);

 	VectorClear(longest);

	/*

 
	for(i=-1; i<=1 ;i++)  //faf:  goes through all direction n, ne, e, se etc.
	{
		for(j=-1; j<=1 ;j++)
		{
			if (i ==0 && j == 0)
				break;

			VectorSet (test, i, j, 0);
			VectorMA(start, 8192, test, end);

			tr = gi.trace(start, NULL, NULL, end, ent, MASK_SHOT|CONTENTS_SLIME|CONTENTS_LAVA);


			VectorSubtract (tr.endpos, start, temp);

//			safe_bprintf (PRINT_HIGH, "%s    %f\n", vtos(test), VectorLength(temp)); 



			if (VectorLength(temp) > VectorLength(longest))
			{
				VectorCopy (temp, longest);
				VectorCopy (test, direction);
				direction[0]= -1 * direction[0];//reverse it
				direction[1]= -1 * direction[1];
//				VectorSet (direction, (-1 * test[0]), (-1 * test[1]), 0);//reversed
				VectorCopy (tr.endpos, plane_start);

			}
		}
	}*/

//	safe_bprintf (PRINT_HIGH, "%s\n", vtos(direction)); 




	VectorSet(left, 1, 0, 0);
	VectorSet(right, -1, 0, 0);
	VectorSet(forward, 0, 1, 0);
	VectorSet(back, 0, -1, 0);

	VectorMA(start, 8192, left, end);
	tr = gi.trace(start, NULL, NULL, end, ent->owner, MASK_ALL);//MASK_SHOT|CONTENTS_SLIME|CONTENTS_LAVA);

	VectorSubtract (tr.endpos, start, temp);
	if (VectorLength(temp) > VectorLength(longest) &&
		VectorLength(temp)< 8000)
	{
		VectorCopy (temp, longest);
		VectorCopy (right, direction);
		VectorCopy (tr.endpos, plane_start);

	}
	VectorMA(start, 8192, right, end);

	tr = gi.trace(start, NULL, NULL, end, ent->owner, MASK_ALL);//MASK_SHOT|CONTENTS_SLIME|CONTENTS_LAVA);
	
	VectorSubtract (tr.endpos, start, temp);
	if (VectorLength(temp) > VectorLength(longest) &&
		VectorLength(temp)< 8000)
	{
		VectorCopy (temp, longest);
		VectorCopy (tr.endpos, plane_start);
		VectorCopy (left, direction);
	}
	VectorMA(start, 8192, forward, end);

	tr = gi.trace(start, NULL, NULL, end, ent->owner, MASK_ALL);//MASK_SHOT|CONTENTS_SLIME|CONTENTS_LAVA);
	
	VectorSubtract (tr.endpos, start, temp);
	if (VectorLength(temp) > VectorLength(longest) &&
		VectorLength(temp)< 8000)
	{
		VectorCopy (temp, longest);
		VectorCopy (tr.endpos, plane_start);
		VectorCopy (back, direction);
	}
	VectorMA(start, 8192, back, end);

	tr = gi.trace(start, NULL, NULL, end, ent->owner, MASK_ALL);//MASK_SHOT|CONTENTS_SLIME|CONTENTS_LAVA);
	
	VectorSubtract (tr.endpos, start, temp);
	if (VectorLength(temp) > VectorLength(longest) &&
		VectorLength(temp)< 8000)
	{
		VectorCopy (temp, longest);
		VectorCopy (tr.endpos, plane_start);
		VectorCopy (forward, direction);
	}
		

//	safe_bprintf (PRINT_HIGH, "%f .\n", VectorLength(longest)); 
//	safe_bprintf (PRINT_HIGH, "%s .\n", vtos(direction)); 

	plane = G_Spawn();
	plane->movetype = MOVETYPE_PUSH;
	plane->solid = SOLID_TRIGGER;
	plane->s.modelindex = gi.modelindex ("models/ships/viper/tris.md2");
	
//faf: requires models	
	plane->s.modelindex = gi.modelindex (va("models/ships/%splane/tris.md2", ent->arty_teamid));

	plane->owner = ent->owner;
	

	VectorClear (plane->mins);
	VectorClear (plane->maxs);

	/*
	plane->solid = SOLID_BBOX;
	VectorSet (plane->mins, -60, -60, -60);
	VectorSet (plane->maxs, 30, 30, 30);
	plane->health = 1000;
	plane->takedamage = DAMAGE_YES;
	plane->die = plane_die;
*/
	plane->classname = "plane";
	plane->classnameb = PLANE;

	plane->think = Plane_Think;
	plane->nextthink = level.time +.1;

	VectorCopy (direction, plane->movedir);
	vectoangles (direction, plane->s.angles);

	// kernel: copy coordinates and team info to the plane
	VectorCopy(ent->arty_entry, plane->arty_entry);
	VectorCopy(ent->arty_target, plane->arty_target);
	strncpy(plane->arty_teamid, ent->arty_teamid, 64);

	if (fast_arty->value)
		speed = 1000;
	else if (VectorLength(longest) > 4000)
		speed = 800;
	else if (VectorLength(longest) > 2000)
		speed = 600;
	else
		speed = 450;

	VectorScale (direction, speed, plane->velocity);
//	VectorScale (direction, 450, plane->velocity);
//	VectorScale (direction, 50, plane->velocity);

	plane->s.sound = gi.soundindex ("dinant/tank.wav");
	
	VectorCopy (plane_start, plane->s.origin);

//	plane->s.origin[2] -= 1;


	plane->leave_limbo_time = level.time;
	plane->s.renderfx   = RF_FULLBRIGHT;
	gi.linkentity (plane);

	// kamikaze: el Officer se sube al avion
	if (kamikaze_arty->value)
		Kamikaze_LinkPilot (plane);
}

//faf
void Airstrike_Plane_Launch(edict_t *ent)
{
	if (arty_confirm->value <= 0 &&
		(!ent->owner ||
		!ent->owner->client ||
		!ent->owner->inuse ||
		ent->owner->flyingnun))
	{
		G_FreeEdict(ent);
		return;
	}

/* kernel: this is not necessary
	if (!ent->owner->client->airstrike)
	{
		G_FreeEdict(ent);
		return;
	}

	if (ent->owner->client->airstrike &&
		ent->owner->client->airstrike != ent)
	{
		G_FreeEdict(ent);
		return;
	}
*/
	// kernel: cancel if player is dead or already respawn
	if (arty_confirm->value <= 0 &&
		(ent->owner->deadflag || ent->owner->client->limbo_mode || !ent->owner->client->airstrike))
	{
		// kernel: cancel just one arty
		if (ent->owner->client->resp.team_on->arty_num > 0)
			ent->owner->client->resp.team_on->arty_num--;
		else
			ent->owner->client->resp.team_on->arty_num = 0;

		ent->owner->client->airstrike = NULL;
		G_FreeEdict(ent);
		return;
	}

	if (!strcmp(ent->arty_teamid, "usa"))
	{
		if (fast_arty->value)
			gi.sound(ent->owner, CHAN_AUTO, gi.soundindex(va("faf/p51.wav")), 1, ATTN_NONE, 0);
		else
			gi.sound(ent->owner, CHAN_AUTO, gi.soundindex(va("faf/p51f.wav")), 1, ATTN_NONE, 0);
	}
	else if (!strcmp(ent->arty_teamid, "grm"))
	{
		gi.sound(ent->owner, CHAN_AUTO, gi.soundindex(va("airstrike/stuka.wav")), 1, ATTN_NONE, 0);
	}
	else
		gi.sound(ent->owner, CHAN_AUTO, gi.soundindex(va("%s/arty/fire.wav", ent->arty_teamid)),
				 1, ATTN_NONE, 0);
	
	// kernel: ent is the airstrike
	Spawn_Plane(ent);

	ent->think = G_FreeEdict;
	ent->nextthink = level.time +.1;

	ent->owner->client->airstrike = NULL;

	// kernel: airstrike was already restricted when it was called
	//ent->owner->client->resp.team_on->arty_time_restrict = level.time + arty_time->value;
}

void Airstrike_Confirm(edict_t *ent)
{
	if (!ent->owner ||
		!ent->owner->client ||
		!ent->owner->inuse ||
		ent->owner->flyingnun)
	{
		G_FreeEdict(ent);
		return;
	}

	// kernel: cancel if player is dead
	if (ent->owner->deadflag || ent->owner->client->limbo_mode)
	{
		// kernel: cancel just one arty
		if (ent->owner->client->resp.team_on->arty_num > 0)
			ent->owner->client->resp.team_on->arty_num--;
		else
			ent->owner->client->resp.team_on->arty_num = 0;

		ent->owner->client->airstrike = NULL;
		G_FreeEdict(ent);
		return;
	}

	// if not dead play sound and print confirmation
	safe_centerprintf(ent->owner, "Sir, give us %d seconds to reach the target!\n", (int)arty_delay->value);

	// kernel: play audio confirmation
	if (arty_confirm->value > 0)
		gi.positioned_sound(ent->owner->s.origin, g_edicts, CHAN_AUTO, gi.soundindex("faf/radioint.wav"), 1.0, ATTN_NORM, 0);

	// kernel: fast plane approaching
	if (fast_arty->value)
		gi.sound(&g_edicts[0], CHAN_AUTO, gi.soundindex("afrowuk/p51_flyby.wav"), 1, ATTN_NONE, 0); // faf/spitfire.wav arty/hit1.wav

	if (airstrikes->value == 1)
	{
		ent->think = Airstrike_Plane_Launch;

		if (fast_arty->value)
			ent->nextthink = level.time + arty_delay->value;
		else if (arty_delay->value > 2.1)
			ent->nextthink = level.time + arty_delay->value - 2;
		else
			ent->nextthink = level.time + .1;
	}
	else //ddaylife
	{
		ent->think = Arty_Sound;

		if (fast_arty->value)
			ent->nextthink = level.time + arty_delay->value;
		else if (arty_delay->value > 3.1)
			ent->nextthink = level.time + arty_delay->value - 3;
		else
			ent->nextthink = level.time + .1;
	}
}




void airstrike_touch_i (edict_t *ent, edict_t *other, cplane_t *plane, csurface_t *surf)
{
	vec3_t		origin;
	int			n;
	vec3_t  end, world_up, start;
	trace_t	tr;

	
	VectorSet(world_up, 0, 0, 1);
	VectorCopy(ent->s.origin, start);
	VectorMA(start, 8192, world_up, end);

	tr = gi.trace(start, NULL, NULL, end, ent, MASK_SHOT|CONTENTS_SLIME|CONTENTS_LAVA);

	//this happens on invade1.  bomb gets dropped inside the pill
	if ( tr.surface && !(tr.surface->flags & SURF_SKY) )
	{ 
		ent->think = G_FreeEdict;
		ent->nextthink = level.time + .1;
		return;
	}

	if (other == ent->owner)
		return;

	if (surf && (surf->flags & SURF_SKY))
	{
		G_FreeEdict (ent);
		return;
	}

//	if (ent->owner->client)
//		PlayerNoise(ent->owner, ent->s.origin, PNOISE_IMPACT);

	// calculate position for the explosion entity
	VectorMA (ent->s.origin, -0.02, ent->velocity, origin);

	if (other->takedamage)
	{
		T_Damage (other, ent, ent->owner, ent->velocity, ent->s.origin, plane->normal, ent->dmg, 0, 0, MOD_AIRSTRIKE);
	}
	else
	{
		// don't throw any debris in net games
		//if (!deathmatch->value && !coop->value)
		//{
			if ((surf) && !(surf->flags & (SURF_WARP|SURF_TRANS33|SURF_TRANS66|SURF_FLOWING)))
			{
				n = rand() % 5;
				while(n--)
					ThrowDebris (ent, "models/objects/debris2/tris.md2", 2, ent->s.origin);
			}
		//}
	}

	T_RadiusDamage(ent, ent, ent->radius_dmg, other, ent->dmg_radius, MOD_AIRSTRIKE_SPLASH);

	gi.WriteByte (svc_temp_entity);
	if (ent->waterlevel)
		gi.WriteByte (TE_ROCKET_EXPLOSION_WATER);
	else
		gi.WriteByte (TE_ROCKET_EXPLOSION);
	gi.WritePosition (origin);
	gi.multicast (ent->s.origin, MULTICAST_PHS);

	G_FreeEdict (ent);
}


void Drop_Bomb_i(edict_t *ent)
{
	edict_t *bomb;
	vec3_t forward, bomb_orig;
	AngleVectors (ent->s.angles, forward, NULL, NULL);
	VectorMA (ent->s.origin, -33, forward, bomb_orig);
	bomb = G_Spawn();
	VectorCopy (bomb_orig, bomb->s.origin);
    bomb->movetype     = MOVETYPE_BOUNCE;
    bomb->clipmask     = MASK_SHOT;
    bomb->solid        = SOLID_BBOX;
	VectorClear (bomb->mins);
	VectorClear (bomb->maxs);
	bomb->touch        = airstrike_touch_i;
	bomb->nextthink    = level.time + 20;
    bomb->think        = G_FreeEdict;
	bomb->dmg          = 700;
	VectorCopy (ent->movedir, bomb->movedir);
	VectorCopy (ent->s.angles, bomb->s.angles);
	bomb->s.modelindex = gi.modelindex ("models/objects/rocket/tris.md2");
	bomb->radius_dmg = 700;
	bomb->dmg_radius = 300;
	bomb->s.sound = gi.soundindex ("weapons/rockfly.wav");
	bomb->classname = "bomb";
	bomb->classnameb = BOMB;
	bomb->gravity = 1; // faf
	gi.linkentity (bomb);
}



void Plane_Fire_i (edict_t *ent)
{
	if (ent->leave_limbo_time < level.time - 20)
	{
		ent->nextthink= level.time + .1;
		ent->think = G_FreeEdict;
	}
	if (ent->count >= 4)
	{
		ent->think = Plane_Fly_Off;
		ent->nextthink = level.time + .1;
	}
	else
	{
		ent->nextthink = level.time +.2;
	}
	Drop_Bomb_i(ent);
	ent->count++;
}


void Plane_Think_i (edict_t *ent)
{
	vec3_t	length;
	float   distance;

	VectorSubtract(ent->s.origin, ent->move_origin, length);
	distance = VectorLength(length);
	if (distance < 1000)
	{
		if (ent->s.angles[2] == 0 && ent->avelocity[2] == 0) //not spinning yet
		{
			if (random() < .5)
			{
				VectorSet (ent->avelocity, 0, 0, -10);//faf
			}
			else
			{
				VectorSet (ent->avelocity, 0, 0, 10);//faf
			}

		}
	}
	if (ent->s.angles[2] > 15) //don't tip too far
	{
			VectorSet (ent->avelocity, 0, 0, 0);//faf
	}
	if (ent->s.angles[2] < -15) 
	{
			VectorSet (ent->avelocity, 0, 0, 0);//faf
	}
	if (distance < 200)
	{
		ent->think = Plane_Fire_i;
		ent->nextthink = level.time +.1;

		gi.sound(ent, CHAN_AUTO, gi.soundindex("usa/arty/hit1.wav"), 1, ATTN_NORM, 0);
	}
	else
		ent->nextthink = level.time +.1;
}

void plane_die_i(edict_t * self , edict_t * inflictor , edict_t * attacker , int damage , vec3_t point )
{
	gi.WriteByte (svc_temp_entity);
	gi.WriteByte (TE_EXPLOSION2);
	gi.WritePosition (self->s.origin);
	gi.multicast (self->s.origin, MULTICAST_PHS);
	G_FreeEdict(self);
}

void Spawn_Plane_i(edict_t *ent)
{
	vec3_t	start;
	vec3_t	end;
	vec3_t left,right,forward,back;
	vec3_t temp, longest, direction, plane_start;
	float speed;
	trace_t	tr;
	edict_t *plane;

	VectorSet(plane_start, 0, 0, 0);
	VectorCopy (ent->move_origin, start);
 	VectorClear(longest);
	VectorSet(left, 1, 0, 0);
	VectorSet(right, -1, 0, 0);
	VectorSet(forward, 0, 1, 0);
	VectorSet(back, 0, -1, 0);
	VectorMA(start, 8192, left, end);
	tr = gi.trace(start, NULL, NULL, end, ent, MASK_ALL);//MASK_SHOT|CONTENTS_SLIME|CONTENTS_LAVA);
	VectorSubtract (tr.endpos, start, temp);
	if (VectorLength(temp) > VectorLength(longest) &&
		VectorLength(temp)< 8000)
	{
		VectorCopy (temp, longest);
		VectorCopy (right, direction);
		VectorCopy (tr.endpos, plane_start);
	}
	VectorMA(start, 8192, right, end);
	tr = gi.trace(start, NULL, NULL, end, ent, MASK_ALL);//MASK_SHOT|CONTENTS_SLIME|CONTENTS_LAVA);
	VectorSubtract (tr.endpos, start, temp);
	if (VectorLength(temp) > VectorLength(longest) &&
		VectorLength(temp)< 8000)
	{
		VectorCopy (temp, longest);
		VectorCopy (tr.endpos, plane_start);
		VectorCopy (left, direction);
	}
	VectorMA(start, 8192, forward, end);
	tr = gi.trace(start, NULL, NULL, end, ent, MASK_ALL);//MASK_SHOT|CONTENTS_SLIME|CONTENTS_LAVA);
	VectorSubtract (tr.endpos, start, temp);
	if (VectorLength(temp) > VectorLength(longest) &&
		VectorLength(temp)< 8000)
	{
		VectorCopy (temp, longest);
		VectorCopy (tr.endpos, plane_start);
		VectorCopy (back, direction);
	}
	VectorMA(start, 8192, back, end);
	tr = gi.trace(start, NULL, NULL, end, ent, MASK_ALL);//MASK_SHOT|CONTENTS_SLIME|CONTENTS_LAVA);
	VectorSubtract (tr.endpos, start, temp);
	if (VectorLength(temp) > VectorLength(longest) &&
		VectorLength(temp)< 8000)
	{
		VectorCopy (temp, longest);
		VectorCopy (tr.endpos, plane_start);
		VectorCopy (forward, direction);
	}

	plane = G_Spawn();
	plane->movetype = MOVETYPE_PUSH;
	plane->solid = SOLID_TRIGGER;
	
//faf: requires models	
	if (!ent->model)
		plane->s.modelindex = gi.modelindex ("models/ships/grmplane/tris.md2");
	else
		plane->s.modelindex = gi.modelindex (ent->model);
	

	VectorClear (plane->mins);
	VectorClear (plane->maxs);
	plane->classname = "plane";
	plane->think = Plane_Think_i;
	plane->nextthink = level.time +.1;
	VectorCopy (direction, plane->movedir);
	vectoangles (direction, plane->s.angles);

	VectorCopy (ent->move_origin, plane->move_origin);

	if (VectorLength(longest) > 4000)
		speed = 800;
	else if (VectorLength(longest) > 2000)
		speed = 600;
	else
		speed = 450;

	VectorScale (direction, speed, plane->velocity);
	plane->s.sound = gi.soundindex ("dinant/tank.wav");
	VectorCopy (plane_start, plane->s.origin);
	plane->leave_limbo_time = level.time;


	gi.linkentity (plane);
	
}

//faf
void Airstrike_Plane_Launch_i(edict_t *ent)
{
	if (!ent->obj_name)
		gi.sound(ent, CHAN_AUTO, gi.soundindex("airstrike/stuka.wav"), 1, ATTN_NONE, 0);
	else
		gi.sound(ent, CHAN_AUTO, gi.soundindex(ent->obj_name), 1, ATTN_NONE, 0);

	ent->movetarget = G_PickTarget(ent->target);
	VectorCopy (ent->movetarget->s.origin, ent->move_origin);
	Spawn_Plane_i(ent); //faf
	ent->nextthink =  level.time + ent->delay + ent->wait + crandom() * ent->random;
}

void SP_airstrike(edict_t *ent)
{
	ent->svflags = SVF_NOCLIENT;
	if (!ent->wait)
		ent->wait = 60;
	ent->think = Airstrike_Plane_Launch_i;
	ent->nextthink =  level.time + ent->delay + ent->wait + crandom() * ent->random;
	gi.linkentity (ent);
}


