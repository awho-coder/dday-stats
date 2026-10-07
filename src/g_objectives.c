/*       D-Day: Normandy by Vipersoft
 ************************************
 *   $Source: /usr/local/cvsroot/dday/src/g_objectives.c,v $
 *   $Revision: 1.8 $
 *   $Date: 2002/06/04 19:49:46 $
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
#include "q_shared.h"
//#include "p_menus.h"

// g_objectives.c
// D-Day: Normandy Objective Entities

// evil: global variables for countdown
extern float countdownTimeLimit;

#if 0

	char *obj_name;
	float obj_area;
	float obj_time;
	int	obj_owner;	//entity that owns this item
	int	obj_gain;
	int	obj_loss;
	int obj_count;

#endif // 0

/*
========================
objective_area
========================
*/
void objective_area_think (edict_t *self) {

	edict_t *ent  = NULL;
	int count = 0;
	//int i=0;
	int newteam = -1;
	int delay;

	self->nextthink = level.time + FRAMETIME;

	if (self->delay) // if there's a counter running
	{
	}

	while ((ent = findradius(ent, self->s.origin, self->obj_area)) != NULL)
	{
		if (!ent->inuse)
			continue;
		if (!IsValidPlayer(ent))
			continue;

		newteam = ent->client->resp.team_on->index;

		if (newteam != self->obj_owner)
			count++;

		//gi.dprintf("Found %d players\n", count);		
	}

	// kernel: newteam should had a positive value
	if (newteam < 0)
		return;

	if (count >= self->obj_count)
	{
		if (self->obj_owner != newteam)
			StatsLog_Objective (STATS_OBJ_AREA, self->obj_name, newteam, NULL);

		// kernel: only adds to score when in deathmatch mode
		if (deathmatch->value)
		{
			team_list[self->obj_owner]->score -= self->obj_loss;

			self->obj_owner = team_list[newteam]->index;
			team_list[self->obj_owner]->score += self->obj_gain;

			if (team_list[self->obj_owner]->time_to_win) // If there already is a counter somwhere else
			{
				if (team_list[self->obj_owner]->time_to_win > (self->obj_time + level.time) )
					// If the counter is longer, shorten it up to this one
					team_list[self->obj_owner]->time_to_win = (self->obj_time + level.time);
			}
			else
			{
				// there is no counter
				team_list[self->obj_owner]->time_to_win = (self->obj_time + level.time);
			}

			delay = (int)(team_list[self->obj_owner]->time_to_win - level.time);

			if ((delay/60) >= 1)
				safe_bprintf(PRINT_HIGH, "Team %s has %i minutes before they win the battle.\n",
							 team_list[self->obj_owner]->teamname, (delay/60));
			else
				safe_bprintf(PRINT_HIGH, "Team %s has %i seconds before they win the battle.\n",
							 team_list[self->obj_owner]->teamname, delay);
		}

		gi.sound(self, CHAN_NO_PHS_ADD, gi.soundindex(va("%s/objectives/area_cap.wav", team_list[self->obj_owner]->teamid)), 1, 0, 0);

		if (dedicated->value)
			safe_cprintf(NULL, PRINT_HIGH, "Objective %s taken by team %s!\n",  self->obj_name,  team_list[self->obj_owner]->teamname);

		centerprintall("Objective %s taken\n by team %s!",
			self->obj_name, 
			team_list[self->obj_owner]->teamname);
	}
}

void SP_objective_area(edict_t *self) {	

	if (!self->obj_name)
		 self->obj_name = "Objective";
	if (!self->obj_area)
		 self->obj_area = 100.0;
//	if (!self->obj_time)
		 self->obj_time = 120;
	if (!self->obj_count)
		 self->obj_count = 3;

	gi.dprintf("\n\nobjective_area spawned belonging to team %i (%s) as \"%s\"\n",
		self->obj_owner,
        team_list[self->obj_owner]->teamname,
        self->obj_name);

	gi.dprintf("distance: %f\n", 
		self->obj_area);

	gi.dprintf("award: %i, loss: %i\n", 
		self->obj_gain,
		self->obj_loss);
	   
	gi.dprintf("required persons: %i\n", self->obj_count);
	gi.dprintf("must hold for %i seconds.\n\n",	(int)self->obj_time);
	
	gi.dprintf(" mins: %s\n maxs: %s\n\n",
		vtos(self->mins),
		vtos(self->maxs) );

	self->think=objective_area_think;	
	self->nextthink = level.time + FRAMETIME;

	self->movetype = MOVETYPE_NONE;
//	self->svflags |= SVF_NOCLIENT;
	gi.setmodel (self, self->model);
	self->solid = SOLID_NOT;
	gi.linkentity (self);

	gi.dprintf(" mins: %s\n maxs: %s\n\n",
		vtos(self->mins),
		vtos(self->maxs) );

}


/*
========================
objective_touch
========================
*/
void objective_touch (edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf) {

	int otherteam;
	//edict_t *entC = NULL;

	if (!IsValidPlayer(other) || (other->client->resp.mos == MEDIC && invuln_medic->value == 1) )  
		return;

	//with this type, will only be recapped by obj_perm_owner when they respawn
	if (self->style%3 == 2 && self->obj_perm_owner && self->obj_perm_owner%2 == other->client->resp.team_on->index &&
		other->client->respawn_time < level.time -.5)
		return;

	//gi.dprintf("touch %i:%i (%i)\n", level.framenum, self->obj_count, (level.framenum - self->obj_count));

	if (other->client->resp.team_on->index != self->obj_owner) 
	{
		if (self->style%5 == 3)//hill fix
		{
			if ((level.framenum - self->obj_count) <= 100) //team recaps after 10 seconds even if other team still in area
				return;
		}
		else if ((level.framenum - self->obj_count) <= self->delay)//15) // its been at least a frame since own team touched it
			return;

		self->obj_owner = other->client->resp.team_on->index;

		StatsLog_Objective (STATS_OBJ_TOUCH, self->message, self->obj_owner, other);

		// kernel: only adds to score when in deathmatch mode
		if (deathmatch->value)
		{
			if (self->obj_perm_owner)
			{
				if (self->obj_perm_owner % 2 != other->client->resp.team_on->index)
				{
					if (team_list[self->obj_owner])
						team_list[self->obj_owner]->score += self->health;
				}
			}
			else
			{
				if (team_list[self->obj_owner])
					team_list[self->obj_owner]->score -= self->dmg;

				team_list[self->obj_owner]->score += self->health;
			}
		}
		
		otherteam = (self->obj_owner);
		if (!team_list[otherteam]->need_points ||
			(!team_list[otherteam]->kills_and_points && team_list[otherteam]->score < team_list[otherteam]->need_points) ||
			(team_list[otherteam]->kills_and_points && 
				team_list[otherteam]->kills < team_list[otherteam]->need_kills))
			gi.sound(self, CHAN_NO_PHS_ADD, gi.soundindex(va("%s/objectives/touch_cap.wav", team_list[self->obj_owner]->teamid)), 1, 0, 0);

		if (dedicated->value)
			safe_cprintf(NULL, PRINT_HIGH, "%s taken by %s [%s]\n", 
				self->message, 
				other->client->pers.netname,
				team_list[self->obj_owner]->teamname);

		centerprintall("%s taken by:\n\n%s\n%s",
				self->message, 
				other->client->pers.netname,
				team_list[self->obj_owner]->teamname);
		
		self->obj_count = level.framenum; // reset the touch count

		G_UseTargets (self, other); //faf

		if (self->delay == -1)
			self->touch = NULL;
		
	} 
	else  // own team touched it
	{
		//gi.dprintf("%s deadflag: %i\n", other->client->pers.netname, other->deadflag);

		if (self->style%5==3)return; //HILL FIX

		if (other->deadflag == DEAD_NO)
			self->obj_count = level.framenum; // update the last time team touched it
	}

}




/*
========================
objective_area
========================
*/

/// exactly like the one above, except 1 person needs to touch the thing.
void SP_objective_touch(edict_t *self) 
{	
	vec3_t min;
	vec3_t max;

	int i;

	self->classname = "objective_touch";
	self->classnameb = OBJECTIVE_TOUCH;

	self->touch=objective_touch;
	//self->index=st.obj_owner;
	self->movetype = MOVETYPE_NONE;	
	self->solid = SOLID_TRIGGER;

	if (!self->delay)
		self->delay = 15;

	if (self->model)
	{
		gi.setmodel (self, self->model);

		if (VectorCompare (self->obj_origin, vec3_origin))
		{
			//gi.dprintf("xxx%s\n", self->classname);
			VectorSet (self->obj_origin, (self->absmax[0] + self->absmin[0])/2,
			(self->absmax[1] + self->absmin[1])/2,
			(self->absmax[2] + self->absmin[2])/2);
		}

	}
	else if (!VectorCompare(self->move_origin, vec3_origin) &&
			 !VectorCompare(self->move_angles, vec3_origin))
	{ 
		VectorCopy (self->move_origin, min);
		VectorCopy (self->move_angles, max);

		//make sure mins are really less than maxs
		for (i=0; i< 3; i++)
		{
			if (min[i] > max [i])
			{
				self->move_origin[i] = max[i];
				self->move_angles[i] = min[i];
			}
		}

		VectorSet (self->s.origin, (self->move_angles[0] + self->move_origin[0])/2,
			(self->move_angles[1] + self->move_origin[1])/2,
			(self->move_angles[2] + self->move_origin[2])/2);
		VectorSet (self->mins, self->move_origin[0] - self->s.origin[0],
			self->move_origin[1] - self->s.origin[1],
			self->move_origin[2] - self->s.origin[2]);
		VectorSet (self->maxs, self->move_angles[0]- self->s.origin[0],
			self->move_angles[1]- self->s.origin[1],
			self->move_angles[2]- self->s.origin[2]);

		VectorCopy (self->s.origin, self->obj_origin);

	}



	//so bots, to find nearest campspot.  either mapper sets it or it goes to the center


	gi.linkentity (self);
	
}

void timed_objective_touch_think (edict_t *self) 
{
	self->nextthink = level.time + FRAMETIME;

	if (!self->wait)
		return;

	if (self->wait)
	{
//		safe_bprintf (PRINT_HIGH, "%i \n", (int)(self->wait + self->obj_time - level.time)); 
		level.obj_time =(int)(self->wait + self->obj_time - level.time + 1);
		level.obj_team = self->obj_owner;
	}

	if (level.intermissiontime)	{
		G_FreeEdict(self);
		return;
	}


	if (level.obj_time <= 0)
	{
		if (dedicated->value)
			safe_cprintf(NULL, PRINT_HIGH, "%s has been Successfully Held by the %s!\n", 
				self->message, 
				team_list[self->obj_owner]->teamname);

		safe_bprintf (PRINT_HIGH, "%s Has been Successfully Held by the %s!\n", 
				self->message, 
				team_list[self->obj_owner]->teamname);

		team_list[self->obj_owner]->score += self->health;

		StatsLog_Objective (STATS_OBJ_TIMED_HELD, self->message, self->obj_owner, NULL);
		
		level.obj_time = 0;
		
//		self->obj_count = level.framenum; // reset the touch count
		G_FreeEdict(self);
	}


}


void timed_objective_touch (edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf) 
{
	int otherteam;
	

	//edict_t *entC = NULL;

	if (!IsValidPlayer(other) || (other->client->resp.mos == MEDIC && invuln_medic->value == 1) )  
		return;

//	gi.dprintf("touch %i:%i (%i)\n", level.framenum, self->obj_count, (level.framenum - self->obj_count));



	if (self->obj_owner ==-1 ||
		other->client->resp.team_on->index != self->obj_owner) 
	{
		if ((level.framenum - self->obj_count) <= 15) // its been at least a frame since own team touched it
			return;

		self->wait =level.time;

//		if (self->obj_owner < MAX_TEAMS) // undefined teams
//			team_list[self->obj_owner]->score -= self->dmg;
		
		self->obj_owner = other->client->resp.team_on->index;
//		team_list[self->obj_owner]->score += self->health;

		StatsLog_Objective (STATS_OBJ_TIMED, self->message, self->obj_owner, other);

		otherteam = (self->obj_owner + 1) % 2;
		if ((!team_list[otherteam]->kills_and_points &&
			 team_list[otherteam]->score < team_list[otherteam]->need_points) ||
			(team_list[otherteam]->kills_and_points && 
			 team_list[otherteam]->kills < team_list[otherteam]->need_kills))
			gi.sound(self, CHAN_NO_PHS_ADD,
					 gi.soundindex(va("%s/objectives/touch_cap.wav", team_list[self->obj_owner]->teamid)), 1, 0, 0);
		

		if (dedicated->value)
			safe_cprintf(NULL, PRINT_HIGH, "%s taken by %s [%s]\n", 
				self->message, 
				other->client->pers.netname,
				team_list[self->obj_owner]->teamname);

		centerprintall("%s taken by:\n\n%s\n%s",
				self->message, 
				other->client->pers.netname,
				team_list[self->obj_owner]->teamname);
		
		self->obj_count = level.framenum; // reset the touch count
		
		/*
		if (self->obj_owner == 1 && self->style == 2)//hack so axis don't trigger flag on first cap
			self->style = 0;
		else		{
			self->style = 0;
			G_UseTargets (self, other); //faf
		} */

		if (self->obj_owner == 1 && self->style == 2)
		{
			edict_t *t;

			self->style = 0;
			//trigger spawn_toggle only
			t = NULL;
			while ((t = G_Find (t, FOFS(targetname), self->target)))
			{
				if (t->use)
				{
					if (!strcmp(t->classname,"spawn_toggle"))
						t->use (t,self, other);
				}
			}

		}
		else		
		{
			self->style = 0;
			G_UseTargets (self, other); //faf
		}



	} 
	else  // own team touched it
	{
		//gi.dprintf("%s deadflag: %i\n", other->client->pers.netname, other->deadflag);

		if (other->deadflag == DEAD_NO)
			self->obj_count = level.framenum; // update the last time team touched it
	}

}

/// exactly like the one above, except 1 person needs to touch the thing.
void SP_timed_objective_touch(edict_t *self) 
{	
	vec3_t min;
	vec3_t max;

	int i;

	self->classname = "objective_touch";
	self->classnameb = OBJECTIVE_TOUCH;

	self->obj_owner =-1;
	self->touch=timed_objective_touch;
	self->think=timed_objective_touch_think;
	self->nextthink = level.time + FRAMETIME;
//self->index=st.obj_owner;
	self->movetype = MOVETYPE_NONE;	
	self->solid = SOLID_TRIGGER;


	if (self->model)
		gi.setmodel (self, self->model);
	else if (!VectorCompare(self->move_origin, vec3_origin) &&
			 !VectorCompare(self->move_angles, vec3_origin))
	{ 
		VectorCopy (self->move_origin, min);
		VectorCopy (self->move_angles, max);

		//make sure mins are really less than maxs
		for (i=0; i< 3; i++)
		{
			if (min[i] > max [i])
			{
				self->move_origin[i] = max[i];
				self->move_angles[i] = min[i];
			}
		}

		VectorSet (self->s.origin, (self->move_angles[0] + self->move_origin[0])/2,
			(self->move_angles[1] + self->move_origin[1])/2,
			(self->move_angles[2] + self->move_origin[2])/2);
		VectorSet (self->mins, self->move_origin[0] - self->s.origin[0],
			self->move_origin[1] - self->s.origin[1],
			self->move_origin[2] - self->s.origin[2]);
		VectorSet (self->maxs, self->move_angles[0]- self->s.origin[0],
			self->move_angles[1]- self->s.origin[1],
			self->move_angles[2]- self->s.origin[2]);

	}
	else
	{
		G_FreeEdict(self);
		return;
	}

	//so bots, to find nearest campspot.  either mapper sets it or it goes to the center
	if (VectorCompare (self->obj_origin, vec3_origin))
	{
		VectorCopy (self->s.origin, self->obj_origin);
	}

	self->obj_perm_owner = -1; //for bots to work correctly

	gi.linkentity (self);
	
}


/*
========================
func_explosive_objective
========================
*/
void func_explosive_objective_explode (edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point)
{
	vec3_t	origin;
	vec3_t	chunkorigin;
	vec3_t	size;
	int		count;
	int		mass;
	int		enemy;
	int		otherteam;

	//gi.dprintf("self: %s\ninflictor: %s\n attacker: %s\n",
	//	self->classname, inflictor->classname, attacker->classname);

	if (!attacker->client ||
		!attacker->client->resp.mos)
		return;

	// bmodel origins are (0 0 0), we need to adjust that here
	VectorScale (self->size, 0.5, size);
	VectorAdd (self->absmin, size, origin);
	VectorCopy (origin, self->s.origin);

	self->takedamage = DAMAGE_NO;

	if (self->dmg)
		T_RadiusDamage (self, attacker, self->dmg, NULL, self->dmg+40, MOD_EXPLOSIVE);

	VectorSubtract (self->s.origin, inflictor->s.origin, self->velocity);
	VectorNormalize (self->velocity);
	VectorScale (self->velocity, 150, self->velocity);

	// start chunks towards the center
	VectorScale (size, 0.5, size);

	mass = self->mass;
	if (!mass)
		mass = 75;

	// big chunks
	if (mass >= 100)
	{
		count = mass / 100;
		if (count > 8)
			count = 8;
		while(count--)
		{
			chunkorigin[0] = origin[0] + crandom() * size[0];
			chunkorigin[1] = origin[1] + crandom() * size[1];
			chunkorigin[2] = origin[2] + crandom() * size[2];
			ThrowDebris (self, "models/objects/debris1/tris.md2", 1, chunkorigin);
		}
	}

	// small chunks
	count = mass / 25;
	if (count > 16)
		count = 16;
	while(count--)
	{
		chunkorigin[0] = origin[0] + crandom() * size[0];
		chunkorigin[1] = origin[1] + crandom() * size[1];
		chunkorigin[2] = origin[2] + crandom() * size[2];
		ThrowDebris (self, "models/objects/debris2/tris.md2", 2, chunkorigin);
	}

	G_UseTargets (self, attacker);

	StatsLog_Objective (STATS_OBJ_EXPLOSIVE, self->obj_name, attacker->client->resp.team_on ? attacker->client->resp.team_on->index : -1, attacker);

	// hack for 2 team games

	if (self->obj_owner != 99) {
		team_list[self->obj_owner]->score -= self->obj_loss;
		enemy = (self->obj_owner) ? 0 : 1;
	} else
		enemy = 99;

	// kernel: only adds to score when in deathmatch mode
	if (deathmatch->value)
	{
		if (self->obj_owner != attacker->client->resp.team_on->index)
			team_list[attacker->client->resp.team_on->index]->score += self->obj_gain;
		else if (self->obj_owner == attacker->client->resp.team_on->index && enemy != 99)
			team_list[enemy]->score += self->obj_gain;
	}

	if (dedicated->value)
		safe_cprintf(NULL, PRINT_HIGH, "%s destroyed by %s [%s]\n", 
			self->obj_name, 
			attacker->client->pers.netname,
			team_list[attacker->client->resp.team_on->index]->teamname);

	centerprintall("%s destroyed by:\n\n%s\n%s",
		self->obj_name, 
		attacker->client->pers.netname,
		team_list[attacker->client->resp.team_on->index]->teamname);


	otherteam = (self->obj_owner + 1) % 2;
	if ((!team_list[otherteam]->kills_and_points &&
		 team_list[otherteam]->score < team_list[otherteam]->need_points) ||
		(team_list[otherteam]->kills_and_points &&
		 team_list[otherteam]->kills < team_list[otherteam]->need_kills))
		gi.sound(self, CHAN_NO_PHS_ADD,
				 gi.soundindex(va("%s/objectives/touch_cap.wav", team_list[otherteam]->teamid)), 1, 0, 0);

//		gi.dprintf ("pts:%i  ndpts:%i  kills:%i  ndkills:%i\n",team_list[(self->obj_owner+1)%2]->score,team_list[(self->obj_owner+1)%2]->need_points,
//team_list[(self->obj_owner+1)%2]->kills,team_list[(self->obj_owner+1)%2]->need_kills);

	if (self->deathtarget)
	{	
		self->target = self->deathtarget;
		if (self->target)
			G_UseTargets (self, attacker);
	}



	if (self->dmg)
		BecomeExplosion1 (self);
	else
		G_FreeEdict (self);
}

void func_explosive_objective_use(edict_t *self, edict_t *other, edict_t *activator)
{
	func_explosive_objective_explode (self, self, other, self->health, vec3_origin);
}

void func_explosive_objective_spawn (edict_t *self, edict_t *other, edict_t *activator)
{
	self->solid = SOLID_BSP;
	self->svflags &= ~SVF_NOCLIENT;
	self->use = NULL;
	KillBox (self);
	gi.linkentity (self);
}

void SP_func_explosive_objective (edict_t *self)
{
	self->classnameb = FUNC_EXPLOSIVE_OBJECTIVE;

	self->movetype = MOVETYPE_PUSH;


	gi.modelindex ("models/objects/debris1/tris.md2");
	gi.modelindex ("models/objects/debris2/tris.md2");

	gi.setmodel (self, self->model);

	if (self->spawnflags & 1)
	{
		self->svflags |= SVF_NOCLIENT;
		self->solid = SOLID_NOT;
		self->use = func_explosive_objective_spawn;
	}
	else
	{
		self->solid = SOLID_BSP;
		if (self->targetname)
			self->use = func_explosive_objective_use;
	}

	if (self->spawnflags & 2)
		self->s.effects |= EF_ANIM_ALL;
	if (self->spawnflags & 4)
		self->s.effects |= EF_ANIM_ALLFAST;

	if (self->use != func_explosive_objective_use)
	{
		if (!self->health)
			self->health = 500;
		self->die = func_explosive_objective_explode;
		self->takedamage = DAMAGE_YES;
	}

	if (!self->obj_name)
		self->obj_name = "Objective";
	if (!self->obj_gain)
		self->obj_gain = 5;
//	if (!self->obj_loss)
//		self->obj_loss = 5;




	//so bots can aim at center.  either mapper sets it or it goes to the center
	if (VectorCompare (self->obj_origin, vec3_origin))
	{
		VectorSet (self->obj_origin, (self->absmax[0] + self->absmin[0])/2,
		(self->absmax[1] + self->absmin[1])/2,
		(self->absmax[2] + self->absmin[2])/2);
	}


	gi.linkentity (self);
}

/* kernel: this is no longer needed
void GetMapObjective(void)
{
	char filename[100];
	
	strcpy(filename, GAMEVERSION "/pics/objectives/");		
	strcat(filename, level.mapname);
	strcat(filename,".pcx");

	// kernel: there is no need to load objective pic in the server
	gi.dprintf("Map objective pic is %s\n", filename);
	level.objectivepic = filename;
}
*/

//faf:  ctb code
int briefcase_count = 0;
qboolean briefcase_respawn_needed;

void droptofloor(edict_t *ent);

void briefcase_spawn_unhide(edict_t *ent)
{
	ent->svflags &= ~SVF_NOCLIENT;
	ent->s.event = EV_ITEM_RESPAWN;
	ent->think = droptofloor;
	ent->nextthink = level.time + FRAMETIME;
	briefcase_respawn_needed = false;
}

void briefcase_spawn_think(edict_t *ent)
{
	int idx;

	if (briefcase_respawn_needed)
	{
		// kernel: must select one origin from all available for the map
		if (briefcase_count)
		{
			idx = rand() % briefcase_count;
			VectorCopy(level.briefcase_origin[idx], ent->s.origin);
			VectorCopy(level.briefcase_angles[idx], ent->s.angles);

			// kernel: must remove entities near the spawn point
			KillBox(ent);

			ent->think = briefcase_spawn_unhide; // kernel: reveal after movement
			ent->nextthink = level.time + 1;
		}
	}
	else
		ent->nextthink = level.time + 10; //check every 10 seconds
}

void Set_Briefcase_Respawn (edict_t *ent)
{
	ent->flags |= FL_RESPAWN;
	ent->svflags |= SVF_NOCLIENT;
	ent->solid = SOLID_NOT;
	ent->nextthink = level.time + 10;//level.time + delay;
	ent->think = briefcase_spawn_think;//DoRespawn;
	gi.linkentity (ent);
}


void PlayTeamSound(int teamidx, char* soundfile, qboolean important);

qboolean Pickup_Briefcase (edict_t *ent, edict_t *other)
{
	int			index;
	index = ITEM_INDEX(ent->item);

	// kernel: do not allow to pick up if the match has not begun yet
	if (tournament->value && countdownTimeLimit <= 0)
		return false;

	other->client->has_briefcase = true;
	other->client->pers.inventory[index]++;
	other->s.modelindex3 = gi.modelindex ("models/objects/briefcase/w_briefcase.md2");

	if (!(ent->spawnflags & DROPPED_ITEM) && (!deathmatch->value && coop->value))
		Set_Briefcase_Respawn (ent);

	briefcase_respawn_needed = false;

	StatsLog_Objective (STATS_OBJ_BC_PICKUP, "briefcase", other->client->resp.team_on ? other->client->resp.team_on->index : -1, other);

	// emit a capture sound for team who pickups
	PlayTeamSound(other->client->resp.team_on->index, "ctb/pickup.wav", true);

	// the other team will get an alert
	int otheridx = (other->client->resp.team_on->index + 1) % 2;
	PlayTeamSound(otheridx, "ctb/alert.wav", false);

	safe_centerprintf(other, "You've taken the briefcase!\n\nBring it to the base, soldier, immediately!");
	centerprintothers(other, "%s picked up the briefcase for team %s!", other->client->pers.netname,
			   other->client->resp.team_on->teamname);

	return true;
}

void Drop_Briefcase (edict_t *ent, gitem_t *item)
{
	if (!item)
		return; // out of ammo, switched before frame?

	Drop_Item (ent, item);
	ent->client->pers.inventory[ITEM_INDEX(item)]--;
	ValidateSelectedItem (ent);

	ent->client->has_briefcase = false;
	ent->s.modelindex3 = 0;

	StatsLog_Objective (STATS_OBJ_BC_DROP, "briefcase", ent->client->resp.team_on ? ent->client->resp.team_on->index : -1, ent);

	gi.sound(&g_edicts[0], CHAN_AUTO, gi.soundindex("ctb/drop.wav"), 1, ATTN_NONE, 0);
	centerprintothers(ent, "%s lost the briefcase of team %s!", ent->client->pers.netname,
			   ent->client->resp.team_on->teamname);
}

void briefcase_respawn (edict_t *ent)
{
	// send effect
	gi.WriteByte(svc_muzzleflash);
	gi.WriteShort(ent-g_edicts);
	gi.WriteByte(MZ_RESPAWN);
	gi.multicast(ent->s.origin, MULTICAST_PVS);

	G_FreeEdict (ent);

	briefcase_respawn_needed = true;
}





void briefcase_warn (edict_t *ent)
{
	edict_t *e;
	int i;

	if (ent->owner &&
		ent->owner->client)
	{
		for (i=0 ; i < game.maxclients ; i++)
		{
			e = g_edicts + 1 + i;
			if (!e->inuse || !e->client)
				continue;
			
			gi.centerprintf(e, "The briefcase has not been touched in 30 seconds.\n\n"
							"It will be respawned in 30 seconds if it's not picked up!");
		}
	}

	ent->think = briefcase_respawn;
	ent->nextthink = level.time + 30;
}


//faf:  ctb code
void base_think (edict_t *ent)
{
	ent->s.frame = (ent->s.frame + 1) % 13; //faf
	ent->nextthink = level.time + FRAMETIME;
}

// kernel: ctb code
void base_touch(edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf)
{
	// kernel: this touch function will used only by ctb_mode 2
	if (ctb_mode->value <= 1)
		return;

	if (!other->client)
		return;

	// player must be carrying the briefcase and be in the same team of flag to score a point
	if (!other->client->has_briefcase || self->obj_owner != other->client->resp.team_on->index)
		return;

	// remove briefcase model
	other->client->pers.inventory[ITEM_INDEX(FindItemB(ITEM_BRIEFCASE))]--;
	other->s.modelindex3 = 0;

	// respawn the briefcase
	briefcase_respawn_needed = true;
	other->client->has_briefcase = false;

	// add 1 point to player's team
	other->client->resp.team_on->score++;
	other->client->resp.points++;

	StatsLog_Objective (STATS_OBJ_BC_CAPTURE, "briefcase", other->client->resp.team_on->index, other);

	// scoring team will listen flagcap
	PlayTeamSound(other->client->resp.team_on->index, "ctb/flagcap.wav", true);

	// the other team will get an alert voice
	int otheridx = (other->client->resp.team_on->index + 1) % 2;
	if (otheridx == 0)
		PlayTeamSound(otheridx, "ctb/axisbriefcase.wav", false);
	else
		PlayTeamSound(otheridx, "ctb/alliedbriefcase.wav", false);

	gi.bprintf(PRINT_HIGH, "%s recovered the briefcase for team %s!\n", other->client->pers.netname,
			   other->client->resp.team_on->teamname);
}

void SP_ctb_base(edict_t *ent)
{
	ent->movetype = MOVETYPE_NONE;
	ent->solid = SOLID_TRIGGER;
	ent->s.modelindex = gi.modelindex(va("models/objects/%sflag/tris.md2", team_list[ent->obj_owner]->teamid));
//	ent->s.frame = rand() % 16;
//	ent->s.frame = 1;
	gi.linkentity (ent);

	ent->think = base_think;
	ent->nextthink = level.time + FRAMETIME;
	ent->s.sound = gi.soundindex("faf/flag.wav");

	ent->touch = base_touch;

	// load needed points to team
	if (ctb_mode->value == 1)
		team_list[ent->obj_owner]->need_points = 100;
	else
		team_list[ent->obj_owner]->need_points = ent->health;

	team_list[ent->obj_owner]->need_kills = 0;
	team_list[ent->obj_owner]->kills_and_points = false;
}
//end faf


/*
========================
objective_control (modo control de zona)

Zona en disputa al estilo Overwatch. Un solo equipo dentro de la zona la
captura (mas rapido con 2 o 3 jugadores); con jugadores de ambos equipos
queda disputada. El equipo dueno acumula control solo mientras tenga
jugadores dentro y ningun rival (vacia o disputada se congela), y gana al
llegar al 100%, salvo que el rival tenga una captura en curso: en ese caso
hay tiempo extra hasta borrarla.

Solo existe con deathmatch y control_mode 1 (ver LoadCTLFile).

"obj_name"	nombre de la zona (por defecto "Zona")
"obj_area"	radio horizontal en unidades (por defecto 192)
"polygon"	en vez de radio, borde de la zona como "x y x y ..." (3 a 32 puntos)
"height"	diferencia de altura maxima con el centro (por defecto 96)
========================
*/

extern int countdownActive;
extern qboolean freeze_mode;
extern float gameStartTime;

#define CONTROL_RING_POINTS	24
// estado de la zona en el HUD (stat_string en STAT_OBJECTIVE, ver Control_StatusBar):
// los centerprints borran las kills de la consola en q2pro, por eso no se usan
// para algo que cambia seguido. Ocupa los ultimos configstrings de CS_GENERAL
// (el resto se usa por jugador en p_view.c).
#define CONTROL_CS_STATUS	(CS_GENERAL + MAX_CLIENTS - 8)
#define CONTROL_ST_LOCKED	0
#define CONTROL_ST_CONTESTED	1
#define CONTROL_ST_CAPTURE	2	// + equipo
#define CONTROL_ST_OWNER	4	// + equipo
#define CONTROL_ST_OVERTIME	6	// + equipo
#define CONTROL_STATUS_WIDTH	28	// caracteres; el texto se centra con espacios
#define CONTROL_INTRO_SENDS	3	// la explicacion del modo se repite para que dure en pantalla
#define CONTROL_INTRO_TIME	2.5	// segundos entre repeticiones (lo que dura un centerprint)

static void Control_UpdateHud (void);
static void Control_EngineerUnlock (int leader);
extern char *dday_statusbar;

static float Control_Cvar (cvar_t *cvar, float fallback)
{
	return (cvar && cvar->value > 0) ? cvar->value : fallback;
}

// punto dentro del poligono del borde (solo x y)
static qboolean Control_InPolygon (float x, float y)
{
	int			i, j;
	qboolean	inside = false;
	float		(*p)[2] = level.control_poly;

	for (i = 0, j = level.control_numpoints - 1; i < level.control_numpoints; j = i++)
	{
		if (((p[i][1] > y) != (p[j][1] > y)) &&
			(x < (p[j][0] - p[i][0]) * (y - p[i][1]) / (p[j][1] - p[i][1]) + p[i][0]))
			inside = !inside;
	}

	return inside;
}

static qboolean Control_InZone (edict_t *zone, edict_t *ent)
{
	vec3_t	delta;
	trace_t	tr;

	VectorSubtract (ent->s.origin, zone->s.origin, delta);

	if (fabs(delta[2]) > zone->count)
		return false;

	// el poligono lo marco el mapper a mano: no hace falta linea de vista
	if (level.control_numpoints)
		return Control_InPolygon (ent->s.origin[0], ent->s.origin[1]);

	delta[2] = 0;
	if (VectorLength (delta) > zone->obj_area)
		return false;

	// no se captura a traves de paredes
	tr = gi.trace (zone->s.origin, NULL, NULL, ent->s.origin, zone, MASK_SOLID);
	return (tr.fraction == 1.0);
}

// jugador que puede disputar la zona: vivo y en un equipo
static qboolean Control_CanContest (edict_t *ent)
{
	if (!ent->inuse || !IsValidPlayer(ent))
		return false;
	if (ent->deadflag || ent->health <= 0)
		return false;
	if (ent->client->resp.team_on->index >= MAX_TEAMS)
		return false;
	return true;
}

static void Control_CountPlayers (edict_t *zone)
{
	edict_t	*ent;
	int		i;

	level.control_inzone[0] = level.control_inzone[1] = 0;

	for (i = 1; i <= game.maxclients; i++)
	{
		ent = &g_edicts[i];

		if (!Control_CanContest (ent))
			continue;

		if (Control_InZone (zone, ent))
			level.control_inzone[ent->client->resp.team_on->index]++;
	}
}

// el jugador esta disputando la zona ahora mismo (para las estadisticas)
qboolean Control_PlayerInZone (edict_t *ent)
{
	if (!level.control_zone || !level.control_unlocked)
		return false;
	if (!Control_CanContest (ent))
		return false;
	return Control_InZone (level.control_zone, ent);
}

static void Control_StatusString (int index, char *text)
{
	char	buf[CONTROL_STATUS_WIDTH + 1];
	int		len = strlen (text), pad;

	if (len > CONTROL_STATUS_WIDTH)
		len = CONTROL_STATUS_WIDTH;
	pad = (CONTROL_STATUS_WIDTH - len) / 2;

	Com_sprintf (buf, sizeof(buf), "%*s%.*s", pad, "", len, text);
	gi.configstring (CONTROL_CS_STATUS + index, buf);
}

static void Control_StatusStrings (void)
{
	int		i;

	Control_StatusString (CONTROL_ST_LOCKED, "ZONA BLOQUEADA");
	Control_StatusString (CONTROL_ST_CONTESTED, "ZONA DISPUTADA");

	for (i = 0; i < 2; i++)
	{
		if (!team_list[i])
			continue;
		Control_StatusString (CONTROL_ST_CAPTURE + i, va("%s CAPTURANDO", team_list[i]->teamname));
		Control_StatusString (CONTROL_ST_OWNER + i, va("%s CONTROLA", team_list[i]->teamname));
		Control_StatusString (CONTROL_ST_OVERTIME + i, va("%s - TIEMPO EXTRA", team_list[i]->teamname));
	}
}

static void Control_Reset (edict_t *zone)
{
	int i;

	level.control_owner = 0;
	level.control_capteam = 0;
	level.control_capture = 0;
	level.control_unlocked = false;
	level.control_overtime = false;
	level.control_winner = 0;
	level.control_start = level.time;
	level.control_engunlock[0] = level.control_engunlock[1] = false;
	level.control_gamestart = gameStartTime;

	for (i = 0; i < MAX_TEAMS; i++)
	{
		level.control_pct[i] = 0;

		// en este modo solo gana el control de la zona
		if (team_list[i])
		{
			team_list[i]->score = 0;
			team_list[i]->need_kills = 0;
			team_list[i]->need_points = 0;
			team_list[i]->kills_and_points = false;
		}
	}

	if (level.control_flag)
	{
		level.control_flag->s.modelindex = 0;
		level.control_flag->s.sound = 0;
		level.control_flag->s.effects = 0;
	}

	Control_StatusStrings ();
	Control_UpdateHud ();
}

// aviso general a todos; el estado de la zona espera unos segundos para no taparlo
static void Control_Announce (char *msg)
{
	centerprintall ("%s", msg);
	level.control_msghold = level.time + 3;
}

static void Control_Capture (edict_t *zone, int team)
{
	edict_t	*ent;
	int		i, loser = level.control_owner - 1;

	level.control_owner = team + 1;
	level.control_capteam = 0;
	level.control_capture = 0;
	level.control_overtime = false;

	// cada jugador del equipo que esta en la zona suma la captura
	for (i = 1; i <= game.maxclients; i++)
	{
		ent = &g_edicts[i];

		if (Control_PlayerInZone (ent) && ent->client->resp.team_on->index == team)
			StatsLog_Objective (STATS_OBJ_ZONE_CAPTURE, zone->obj_name, team, ent);
	}

	// bandera del equipo dueno en el centro de la zona
	if (level.control_flag)
	{
		level.control_flag->s.modelindex = gi.modelindex (va("models/objects/%sflag/tris.md2", team_list[team]->teamid));
		level.control_flag->s.sound = gi.soundindex ("faf/flag.wav");

		// luz del color del dueno alrededor de la bandera (azul Aliados, roja Eje, como el borde)
		level.control_flag->s.effects = (team == 0) ? EF_FLAG2 : EF_FLAG1;
	}

	gi.sound (zone, CHAN_NO_PHS_ADD, gi.soundindex(va("%s/objectives/area_cap.wav", team_list[team]->teamid)), 1, 0, 0);

	// un solo centerprint por jugador (cada uno borra las kills de arriba):
	// el equipo que la tenia recibe el suyo, el resto el general
	if (loser < 0 || loser == team || !team_list[loser])
		loser = -1;

	gi.dprintf ("Equipo %s capturo la zona %s\n", team_list[team]->teamname, zone->obj_name);
	for (i = 1; i <= game.maxclients; i++)
	{
		ent = &g_edicts[i];
		if (!ent->inuse || !ent->client)
			continue;

		if (loser >= 0 && ent->client->resp.team_on && ent->client->resp.team_on->index == loser)
			gi.centerprintf (ent, "PERDIERON LA ZONA!\n%s la capturo", team_list[team]->teamname);
		else
			gi.centerprintf (ent, "%s\ncapturo la zona!", team_list[team]->teamname);
	}
	level.control_msghold = level.time + 3;

	if (loser >= 0)
		PlayTeamSound (loser, va("%s/shout/flagtk.wav", team_list[loser]->teamid), false);

	Control_UpdateHud ();
}

static void Control_Spark (edict_t *zone, float x, float y, int color)
{
	vec3_t	start, end, up = {0, 0, 1};
	trace_t	tr;

	// se busca el piso desde arriba del rango de altura de la zona
	start[0] = end[0] = x;
	start[1] = end[1] = y;
	start[2] = zone->s.origin[2] + zone->count;
	end[2] = zone->s.origin[2] - zone->count - 64;

	tr = gi.trace (start, NULL, NULL, end, zone, MASK_SOLID);
	if (tr.startsolid || tr.fraction == 1.0)
		return;

	tr.endpos[2] += 4;

	gi.WriteByte (svc_temp_entity);
	gi.WriteByte (TE_LASER_SPARKS);
	gi.WriteByte (4);
	gi.WritePosition (tr.endpos);
	gi.WriteDir (up);
	gi.WriteByte (color);
	gi.multicast (tr.endpos, MULTICAST_PVS);
}

// borde de chispas de la zona, con el color del dueno
static void Control_DrawRing (edict_t *zone)
{
	float	angle, len, *a, *b;
	int		i, j, steps, color;

	if (level.control_owner == 1)
		color = 0xf3;	// azul
	else if (level.control_owner == 2)
		color = 0xf2;	// rojo
	else
		color = 0x0f;	// blanco

	if (level.control_numpoints)
	{
		// una chispa cada ~96 unidades a lo largo de cada lado
		for (i = 0; i < level.control_numpoints; i++)
		{
			a = level.control_poly[i];
			b = level.control_poly[(i + 1) % level.control_numpoints];
			len = sqrt((b[0] - a[0]) * (b[0] - a[0]) + (b[1] - a[1]) * (b[1] - a[1]));
			steps = (int)(len / 96) + 1;

			for (j = 0; j < steps; j++)
				Control_Spark (zone, a[0] + (b[0] - a[0]) * j / steps, a[1] + (b[1] - a[1]) * j / steps, color);
		}
		return;
	}

	for (i = 0; i < CONTROL_RING_POINTS; i++)
	{
		angle = (2 * M_PI * i) / CONTROL_RING_POINTS;
		Control_Spark (zone, zone->s.origin[0] + cos(angle) * zone->obj_area,
			zone->s.origin[1] + sin(angle) * zone->obj_area, color);
	}
}

// estado de la zona para los que estan dentro: solo cuando cambia, para no
// llenar la consola (los porcentajes se ven en el HUD: ZONA % y TOMA)
// estado de la zona para los jugadores que estan dentro, en el HUD
// (level.control_msgstate guarda el configstring a mostrar, 0 = nada)
static void Control_ZoneMessage (edict_t *zone)
{
	edict_t	*ent;
	int		i, cs, owner = level.control_owner - 1;

	if (!level.control_unlocked)
		cs = CONTROL_CS_STATUS + CONTROL_ST_LOCKED;
	else if (level.control_inzone[0] && level.control_inzone[1])
		cs = CONTROL_CS_STATUS + CONTROL_ST_CONTESTED;
	else if (level.control_capture > 0 && level.control_capteam)
		cs = CONTROL_CS_STATUS + CONTROL_ST_CAPTURE + level.control_capteam - 1;
	else if (owner >= 0)
		cs = CONTROL_CS_STATUS + (level.control_overtime ? CONTROL_ST_OVERTIME : CONTROL_ST_OWNER) + owner;
	else
		cs = 0;	// neutral y vacia: no hay nada que mostrar

	for (i = 1; i <= game.maxclients; i++)
	{
		ent = &g_edicts[i];

		if (!ent->inuse || !IsValidPlayer(ent) || ent->deadflag || !Control_InZone (zone, ent))
			level.control_msgstate[i - 1] = 0;
		else
			level.control_msgstate[i - 1] = cs;
	}
}

// explicacion corta del modo, una vez por mapa, la primera vez que el jugador
// aparece en un equipo (tambien queda en la consola)
static void Control_Intro (edict_t *zone)
{
	char	msg[400], extra[128];
	edict_t	*ent;
	int		i;
	qboolean	nogren, noeng;

	nogren = (Control_GrenadeLimit (NULL) == 0);
	noeng = Control_ClassBanned (NULL, ENGINEER);

	if (nogren && noeng)
		Com_sprintf (extra, sizeof(extra), "Sin granadas ni ingenieros.\n");
	else if (nogren || noeng)
		Com_sprintf (extra, sizeof(extra), "Sin %s.\n", nogren ? "granadas" : "ingenieros");
	else
		extra[0] = 0;

	if (noeng && control_engineer_at && control_engineer_at->value > 0)
		Com_sprintf (extra + strlen (extra), sizeof(extra) - strlen (extra),
			"Si el rival llega a %i%%, tu equipo\nrecibe el ingeniero.\n", (int)control_engineer_at->value);

	Com_sprintf (msg, sizeof(msg),
		"MODO CONTROL DE ZONA\n\n"
		"Capturen la zona %s parandose dentro.\n"
		"Mas jugadores capturan mas rapido.\n"
		"Con rivales dentro queda disputada.\n\n"
		"Gana el primer equipo en llegar\n"
		"a 100%% de control (ZONA %% en el HUD).\n"
		"%s",
		zone->obj_name, extra);

	for (i = 1; i <= game.maxclients; i++)
	{
		ent = &g_edicts[i];

		// slot libre: el proximo que entre la vuelve a ver
		if (!ent->inuse)
		{
			level.control_introsent[i - 1] = 0;
			level.control_intronext[i - 1] = 0;
			continue;
		}

		if (ent->ai || !IsValidPlayer(ent) || ent->deadflag ||
			level.control_introsent[i - 1] >= CONTROL_INTRO_SENDS ||
			level.time < level.control_intronext[i - 1])
			continue;

		gi.centerprintf (ent, "%s", msg);
		level.control_introsent[i - 1]++;
		level.control_intronext[i - 1] = level.time + CONTROL_INTRO_TIME;
	}
}

void objective_control_think (edict_t *self)
{
	float	unlock_time, rate, mult;
	int		n, team, enemy, owner, i, before, after;
	static const int milestones[] = {50, 90};

	self->nextthink = level.time + FRAMETIME;

	if (level.intermissiontime || level.control_winner)
		return;

	// el juego esta pausado o en cuenta regresiva
	if (countdownActive || freeze_mode)
	{
		// la pausa no consume el tiempo de bloqueo de la zona
		if (level.control_start && !level.control_unlocked)
			level.control_start += FRAMETIME;
		return;
	}

	// primer frame del mapa, o una cuenta regresiva reinicio la partida
	if (!level.control_start || level.control_gamestart != gameStartTime)
		Control_Reset (self);

	Control_CountPlayers (self);
	Control_Intro (self);

	if (level.framenum % 10 == 0)
		Control_DrawRing (self);

	unlock_time = level.control_start + Control_Cvar (control_lock, 0.1);

	if (!level.control_unlocked)
	{
		Control_ZoneMessage (self);

		if ((int)(unlock_time - level.time) == 10 && level.framenum % 10 == 0)
			safe_bprintf (PRINT_HIGH, "La zona %s se abre en 10 segundos.\n", self->obj_name);

		if (level.time < unlock_time)
			return;

		level.control_unlocked = true;
		Control_Announce ("La zona esta abierta!\nCapturenla!");
	}

	owner = level.control_owner - 1;

	// equipo atacante: el unico equipo presente que no es dueno
	team = -1;
	if (level.control_inzone[0] && !level.control_inzone[1])
		team = 0;
	else if (level.control_inzone[1] && !level.control_inzone[0])
		team = 1;

	if (team >= 0 && team != owner)
	{
		// 1 jugador = x1, 2 = x1.5, 3 o mas = x2
		n = level.control_inzone[team];
		mult = (n >= 3) ? 2.0 : (n == 2) ? 1.5 : 1.0;
		rate = 100.0 / Control_Cvar (control_captime, 15) * mult * FRAMETIME;

		if (level.control_capteam && level.control_capteam != team + 1)
		{
			// primero se deshace el avance del otro equipo
			level.control_capture -= rate;
			if (level.control_capture <= 0)
			{
				level.control_capture = 0;
				level.control_capteam = 0;
			}
		}
		else
		{
			level.control_capteam = team + 1;
			level.control_capture += rate;

			if (level.control_capture >= 100)
			{
				Control_Capture (self, team);
				owner = team;
			}
		}
	}
	else if (!(level.control_inzone[0] && level.control_inzone[1]) && level.control_capture > 0)
	{
		// nadie disputa la zona: el avance de la captura se pierde
		level.control_capture -= 100.0 / Control_Cvar (control_captime, 15) * FRAMETIME;
		if (level.control_capture <= 0)
		{
			level.control_capture = 0;
			level.control_capteam = 0;
		}
	}

	if (owner >= 0)
	{
		enemy = (owner + 1) % MAX_TEAMS;

		// con el dueno dentro y sin rivales el control sube a velocidad normal; con la
		// zona vacia sube mas lento (control_emptyrate); disputada se congela
		mult = 0;
		if (level.control_inzone[owner] && !level.control_inzone[enemy])
			mult = 1.0;
		else if (!level.control_inzone[owner] && !level.control_inzone[enemy] && control_emptyrate)
			mult = (control_emptyrate->value > 1) ? 1.0 : control_emptyrate->value;

		if (mult > 0)
		{
			before = (int)level.control_pct[owner];
			level.control_pct[owner] += 100.0 / Control_Cvar (control_holdtime, 120) * mult * FRAMETIME;
			after = (int)level.control_pct[owner];

			for (i = 0; i < sizeof(milestones) / sizeof(milestones[0]); i++)
			{
				if (before < milestones[i] && after >= milestones[i])
					safe_bprintf (PRINT_HIGH, "Equipo %s lleva %i/100 de control de la zona.\n", team_list[owner]->teamname, milestones[i]);
			}

			Control_EngineerUnlock (owner);
		}

		if (level.control_pct[owner] >= 100)
		{
			if (level.control_inzone[enemy] || level.control_capture > 0)
			{
				// tiempo extra: no se gana mientras quede captura rival en curso
				level.control_pct[owner] = 99.9;

				if (!level.control_overtime)
				{
					level.control_overtime = true;
					Control_Announce ("TIEMPO EXTRA!");
					safe_bprintf (PRINT_HIGH, "Tiempo extra! %s debe aguantar en la zona hasta borrar la captura rival.\n", team_list[owner]->teamname);
				}
			}
			else
			{
				level.control_pct[owner] = 100;
				level.control_winner = owner + 1;
			}
		}
	}

	for (i = 0; i < MAX_TEAMS; i++)
	{
		if (team_list[i])
			team_list[i]->score = (int)level.control_pct[i];
	}

	Control_ZoneMessage (self);
}

void SP_objective_control (edict_t *self)
{
	edict_t	*flag;
	vec3_t	end;
	trace_t	tr;

	if (!deathmatch->value || !control_mode->value)
	{
		G_FreeEdict (self);
		return;
	}

	if (level.control_zone)
	{
		gi.dprintf ("objective_control: solo se admite una zona por mapa, se ignora la de %s\n", vtos(self->s.origin));
		G_FreeEdict (self);
		return;
	}

	if (!self->obj_name)
		self->obj_name = "Zona";
	if (!self->obj_area)
		self->obj_area = 192;

	// count guarda la tolerancia de altura
	self->count = st.height ? st.height : 96;

	if (st.polygon)
	{
		char	*s = st.polygon;
		float	x, y;
		int		n;

		while (level.control_numpoints < CONTROL_MAX_POINTS &&
			sscanf (s, "%f %f%n", &x, &y, &n) == 2)
		{
			level.control_poly[level.control_numpoints][0] = x;
			level.control_poly[level.control_numpoints][1] = y;
			level.control_numpoints++;
			s += n;
		}

		if (level.control_numpoints < 3)
		{
			gi.dprintf ("objective_control: \"polygon\" necesita al menos 3 puntos, se usa el radio\n");
			level.control_numpoints = 0;
		}
	}

	self->movetype = MOVETYPE_NONE;
	self->solid = SOLID_NOT;
	self->s.modelindex = 0;
	gi.linkentity (self);

	self->think = objective_control_think;
	self->nextthink = level.time + FRAMETIME;

	level.control_zone = self;

	// la bandera va aparte, apoyada en el piso: el origin de la zona esta a la
	// altura de un jugador parado y ahi la bandera quedaba en el aire
	flag = G_Spawn ();
	flag->classname = "control_flag";
	flag->movetype = MOVETYPE_NONE;
	flag->solid = SOLID_NOT;
	VectorCopy (self->s.origin, flag->s.origin);
	VectorCopy (self->s.origin, end);
	end[2] -= self->count + 64;
	tr = gi.trace (self->s.origin, NULL, NULL, end, self, MASK_SOLID);
	if (!tr.startsolid && tr.fraction < 1.0)
		VectorCopy (tr.endpos, flag->s.origin);
	gi.linkentity (flag);
	level.control_flag = flag;
	gi.dprintf ("objective_control: bandera en %s\n", vtos(flag->s.origin));

	if (level.control_numpoints)
		gi.dprintf ("objective_control \"%s\" en %s, poligono de %i puntos, altura %i\n",
			self->obj_name, vtos(self->s.origin), level.control_numpoints, self->count);
	else
		gi.dprintf ("objective_control \"%s\" en %s, radio %i, altura %i\n",
			self->obj_name, vtos(self->s.origin), (int)self->obj_area, self->count);
}

// cambia los titulos del HUD: POINTS muestra el % de zona y TIME el avance de captura
char *Control_StatusBar (char *statusbar)
{
	static char	buf[4096];
	char		*p, name[15];	// nombres largos se cortan para no pisar el marcador
	int			owner, n, x;

	strncpy (buf, statusbar, sizeof(buf) - 1);
	buf[sizeof(buf) - 1] = 0;

	// la imagen de objetivos (stat 16) se reemplaza por el estado de la zona, en texto,
	// a la altura de los centerprints (centrado: el texto ya viene con espacios)
	if ((p = strstr(buf, "if 16    xl 0    yt 0    pic 16 endif ")) != NULL)
	{
		static const char	*old16 = "if 16    xl 0    yt 0    pic 16 endif ";
		static const char	*new16 = "if 16 xv 48 yv 76 stat_string 16 endif ";
		size_t				ol = strlen (old16), nw = strlen (new16);

		if (strlen (buf) - ol + nw < sizeof(buf))
		{
			memmove (p + nw, p + ol, strlen (p + ol) + 1);
			memcpy (p, new16, nw);
		}
	}

	if ((p = strstr(buf, "\"POINTS\"")) != NULL)
		memcpy (p, "\"ZONA %\"", 8);
	if ((p = strstr(buf, "\"TIME\"")) != NULL)
		memcpy (p, "\"TOMA\"", 6);

	// TOMA baja un poco para dejar lugar a la linea del dueno de la zona
	if ((p = strstr(buf, "yt 115 ")) != NULL)
		memcpy (p, "yt 131 ", 7);
	if ((p = strstr(buf, "yt 128 ")) != NULL)
		memcpy (p, "yt 144 ", 7);
	if ((p = strstr(buf, "yt 158 ")) != NULL)
		memcpy (p, "yt 174 ", 7);

	owner = level.control_owner - 1;
	if (owner >= 0 && owner < MAX_TEAMS && team_list[owner])
	{
		// sin comillas para no romper el layout
		strncpy (name, team_list[owner]->teamname, sizeof(name) - 1);
		name[sizeof(name) - 1] = 0;
		for (p = name; *p; p++)
			if (*p == '"')
				*p = '\'';
	}
	else
		strcpy (name, "NEUTRAL");

	// debajo de los equipos, alineado a la derecha como el marcador: "ZONA: <dueno>"
	// (8 pixeles por letra; "ZONA: " son 6)
	x = -(int)(strlen(name) + 6) * 8 - 4;
	n = strlen (buf);
	Com_sprintf (buf + n, sizeof(buf) - n, "yt 104 xr %i string \"ZONA:\" xr %i %s \"%s\" ",
		x, x + 48, (owner >= 0) ? "string2" : "string", name);

	return buf;
}

// reenvia el HUD a todos cuando la zona cambia de dueno
static void Control_UpdateHud (void)
{
	gi.configstring (CS_STATUSBAR, Control_StatusBar (dday_statusbar));
}

// ingeniero que solo esta permitido porque el rival llego a control_engineer_at:
// lleva menos cohetes y granadas (ver Give_Class_Weapon y Give_Class_Ammo)
qboolean Control_LimitedEngineer (edict_t *ent)
{
	if (!level.control_zone || !ent || !ent->client || !ent->client->resp.team_on)
		return false;

	return (ent->client->resp.mos == ENGINEER && control_engineer && !control_engineer->value &&
		level.control_engunlock[ent->client->resp.team_on->index]);
}

// maximo de granadas del jugador en el modo control (0 = sin granadas, -1 = sin limite)
int Control_GrenadeLimit (edict_t *ent)
{
	if (!level.control_zone)
		return -1;

	if (Control_LimitedEngineer (ent))
		return control_engineer_grenades && control_engineer_grenades->value >= 0 ?
			(int)control_engineer_grenades->value : 1;

	if (!control_grenades || control_grenades->value < 0)
		return -1;

	return (int)control_grenades->value;
}

// clases que no se pueden usar en el modo control: el ingeniero, salvo control_engineer 1
// o que el equipo rival haya llegado a control_engineer_at (ent NULL = sin desbloqueo)
qboolean Control_ClassBanned (edict_t *ent, int mos)
{
	if (!level.control_zone || mos != ENGINEER || !control_engineer || control_engineer->value)
		return false;

	if (ent && ent->client && ent->client->resp.team_on &&
		level.control_engunlock[ent->client->resp.team_on->index])
		return false;

	return true;
}

// el equipo que va perdiendo recibe el ingeniero cuando el rival llega a control_engineer_at
static void Control_EngineerUnlock (int leader)
{
	edict_t	*ent;
	int		i, team = 1 - leader;
	int		grenades = (control_engineer_grenades && control_engineer_grenades->value >= 0) ?
		(int)control_engineer_grenades->value : 1;

	if (!control_engineer || control_engineer->value || !control_engineer_at ||
		control_engineer_at->value <= 0 || level.control_engunlock[team] ||
		level.control_pct[leader] < control_engineer_at->value)
		return;

	level.control_engunlock[team] = true;
	gi.dprintf ("%s llego a %i%% de control: %s ya puede usar ingeniero\n",
		team_list[leader]->teamname, (int)control_engineer_at->value, team_list[team]->teamname);

	for (i = 1; i <= game.maxclients; i++)
	{
		ent = &g_edicts[i];
		if (!ent->inuse || ent->ai || !ent->client || ent->client->resp.team_on != team_list[team])
			continue;

		gi.centerprintf (ent, "INGENIERO DESBLOQUEADO\n\n%i cohetes, %i granada%s y TNT.\nCambia de clase en el menu.",
			(int)Control_Cvar (control_engineer_rockets, 3), grenades, grenades == 1 ? "" : "s");
	}
	level.control_msghold = level.time + 3;
}

void Control_HudStats (edict_t *ent)
{
	if (!level.control_zone)
		return;

	if (level.control_capteam && level.control_capture > 0)
	{
		ent->client->ps.stats[STAT_TIMER2] = (level.control_capture < 1) ? 1 : (int)level.control_capture;
		ent->client->ps.stats[STAT_TIMER2_ICON] = gi.imageindex (va("teams/%s", team_list[level.control_capteam - 1]->teamid));
	}
	else
		ent->client->ps.stats[STAT_TIMER2] = 0;

	// en este modo STAT_OBJECTIVE muestra el estado de la zona (ver Control_StatusBar)
	ent->client->ps.stats[STAT_OBJECTIVE] = level.control_msgstate[ent - g_edicts - 1];
}
