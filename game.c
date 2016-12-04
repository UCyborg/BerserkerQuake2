/*
===========================================================================
Copyright (C) 1997-2001 Id Software, Inc.
Copyright (C) 2004-2014 Serge Borodulin aka Berserker (tm)
                         <http://berserker.quakegate.ru>

This file is part of Berserker@Quake2 source code.

Berserker@Quake2 source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Berserker@Quake2 source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Berserker@Quake2 source code; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
===========================================================================
*/

#ifdef _MSC_VER
#pragma warning( disable: 4305 4309 4244 4018 4551 4116 4146 4800 4996)
#endif

#include "defs.h"
#include "gamedata.h"

bool SV_movestep (edict_t *ent, vec3_t move, bool relink);
bool visible (edict_t *self, edict_t *other);
bool infront (edict_t *self, edict_t *other);
int range (edict_t *self, edict_t *other);
void FoundTarget (edict_t *self, char *func);
void G_TouchTriggers (edict_t *ent);
bool M_CheckBottom (edict_t *ent);
void M_CheckGround (edict_t *ent);
void G_ProjectSource (vec3_t point, float *distance, vec3_t forward, vec3_t right, vec3_t result);
void fire_bullet (edict_t *self, vec3_t start, vec3_t aimdir, int damage, int kick, int hspread, int vspread, int mod);
void fire_shotgun (edict_t *self, vec3_t start, vec3_t aimdir, int damage, int kick, int hspread, int vspread, int count, int mod);
void fire_blaster (edict_t *self, vec3_t start, vec3_t dir, int damage, int speed, int effect, bool hyper_);
void fire_grenade (edict_t *self, vec3_t start, vec3_t aimdir, int damage, int speed, float timer, float damage_radius);
void fire_rocket (edict_t *self, vec3_t start, vec3_t dir, int damage, int speed, float damage_radius, int radius_damage);
void fire_rail (edict_t *self, vec3_t start, vec3_t aimdir, int damage, int kick);
void fire_bfg (edict_t *self, vec3_t start, vec3_t dir, int damage, int speed, float damage_radius);
void M_droptofloor (edict_t *ent);
edict_t *G_PickTarget (char *targetname, char *func);
void M_CatagorizePosition (edict_t *ent);
void T_Damage (edict_t *targ, edict_t *inflictor, edict_t *attacker, vec3_t dir, vec3_t point, vec3_t normal, int damage, int knockback, int dflags, int mod);
bool M_CheckAttack (edict_t *self);
void MakronToss (edict_t *self);



///////////////// COMMON PROGS (FIXME: put it common.c) //////////////////////////////////////////////
vec3_t vec3_origin = {0,0,0};


//void Sys_Error (char *error, ...)
//{
//	va_list		argptr;
//	char		text[1024];
//
//	va_start (argptr, error);
//	vsprintf (text, error, argptr);
//	va_end (argptr);
//
//	gi.error (/*ERR_FATAL,*/ "%s", text);	// Quake2 DLL Bug
//}


static inline void VectorMA (vec3_t veca, float scale, vec3_t vecb, vec3_t vecc)
{
	vecc[0] = veca[0] + scale*vecb[0];
	vecc[1] = veca[1] + scale*vecb[1];
	vecc[2] = veca[2] + scale*vecb[2];
}


void CrossProduct (vec3_t v1, vec3_t v2, vec3_t cross)
{
	cross[0] = v1[1]*v2[2] - v1[2]*v2[1];
	cross[1] = v1[2]*v2[0] - v1[0]*v2[2];
	cross[2] = v1[0]*v2[1] - v1[1]*v2[0];
}


int		com_token_len;
char	com_token[MAX_TOKEN_CHARS];
/*
==============
COM_Parse

Parse a token out of a string
==============
*/
char *COM_Parse (char **data_p)
{
	int		c;
	char	*data;

	data = *data_p;
	com_token_len = 0;
	com_token[0] = 0;

	if (!data)
	{
		*data_p = NULL;
		return "";
	}

// skip whitespace
skipwhite:
	while ( (c = *data) <= ' ')
	{
		if (c == 0)
		{
			*data_p = NULL;
			return "";
		}
		data++;
	}

// skip // comments
	if (c=='/' && data[1] == '/')
	{
		while (*data && *data != '\n')
			data++;
		goto skipwhite;
	}

// handle quoted strings specially
	if (c == '\"')
	{
		data++;
		while (1)
		{
			c = *data++;
			if (c=='\"' || !c)
			{
				com_token[com_token_len] = 0;
				*data_p = data;
				return com_token;
			}
			if (com_token_len < MAX_TOKEN_CHARS)
			{
				com_token[com_token_len] = c;
				com_token_len++;
			}
		}
	}

// parse a regular word
	do
	{
		if (com_token_len < MAX_TOKEN_CHARS)
		{
			com_token[com_token_len] = c;
			com_token_len++;
		}
		data++;
		c = *data;
	} while (c>32);

	if (com_token_len == MAX_TOKEN_CHARS)
	{
//		Com_Printf ("Token exceeded %i chars, discarded.\n", MAX_TOKEN_CHARS);
		com_token_len = 0;
	}
	com_token[com_token_len] = 0;

	*data_p = data;
	return com_token;
}


/*
============
va

does a varargs printf into a temp buffer, so I don't need to have
varargs versions of all text functions.
FIXME: make this buffer size safe someday
============
*/
char	*va(char *format, ...)
{
	va_list		argptr;
	static char		string[1024];

	va_start (argptr, format);
	vsprintf (string, format,argptr);
	va_end (argptr);

	return string;
}

/*
void Com_Printf (char *msg, ...)
{
	va_list		argptr;
	char		text[1024];

	va_start (argptr, msg);
	vsprintf (text, msg, argptr);
	va_end (argptr);

	gi.dprintf ("%s", text);
}
*/

void Com_sprintf (char *dest, int size, char *fmt, ...)
{
	int		len;
	va_list		argptr;
	char	bigbuffer[0x10000];

	va_start (argptr,fmt);
	len = vsprintf (bigbuffer,fmt,argptr);
	va_end (argptr);
	if (len >= size)
		gi.dprintf/*Com_Printf*/ ("Com_sprintf: overflow of %i in %i\n", len, size);
	strncpy (dest, bigbuffer, size-1);
}


int Q_strncasecmp (char *s1, char *s2, int n)
{
	int		c1, c2;

	if(!s1 || !s2)
		return -1;			// one or both strings are NULL

	do
	{
		c1 = *s1++;
		c2 = *s2++;

		if (!n--)
			return 0;		// strings are equal until end point

		if (c1 != c2)
		{
			if (c1 >= 'a' && c1 <= 'z')
				c1 -= ('a' - 'A');
			if (c2 >= 'a' && c2 <= 'z')
				c2 -= ('a' - 'A');
			if (c1 != c2)
				return -1;		// strings not equal
		}
	} while (c1);

	return 0;		// strings are equal
}


int Q_strcasecmp (char *s1, char *s2)
{
	return Q_strncasecmp (s1, s2, 99999);
}


/*
=============
VectorToString

This is just a convenience function
for printing vectors
=============
*/
char	*vtos (vec3_t v)
{
	static	int		index;
	static	char	str[8][32];
	char	*s;

	// use an array so that multiple vtos won't collide
	s = str[index];
	index = (index + 1)&7;
	Com_sprintf (s, 32, "(%i %i %i)", (int)v[0], (int)v[1], (int)v[2]);
	return s;
}


/*
=============
TempVector

This is just a convenience function
for making temporary vectors for function calls
=============
*/
float	*tv (float x, float y, float z)
{
	static	int		index;
	static	vec3_t	vecs[8];
	float	*v;

	// use an array so that multiple tempvectors won't collide
	// for a while
	v = vecs[index];
	index = (index + 1)&7;

	v[0] = x;
	v[1] = y;
	v[2] = z;

	return v;
}


void VectorScale (vec3_t in, float scale, vec3_t out)
{
	out[0] = in[0]*scale;
	out[1] = in[1]*scale;
	out[2] = in[2]*scale;
}


float VectorNormalize (vec3_t v)
{
	float	length, ilength;

	length = v[0]*v[0] + v[1]*v[1] + v[2]*v[2];
	length = sqrt (length);		// FIXME

	if (length)
	{
		ilength = 1/length;
		v[0] *= ilength;
		v[1] *= ilength;
		v[2] *= ilength;
	}

	return length;

}


int VectorCompare (vec3_t v1, vec3_t v2)
{
	if (v1[0] != v2[0] || v1[1] != v2[1] || v1[2] != v2[2])
			return 0;

	return 1;
}


float VectorLength(vec3_t v)
{
	int		i;
	float	length;

	length = 0;
	for (i=0 ; i< 3 ; i++)
		length += v[i]*v[i];
	length = sqrt (length);		// FIXME

	return length;
}


void SinCos( float radians, float *sine, float *cosine )
{
#ifdef ENABLE_ASM
#ifdef _MSC_VER
	_asm
	{
		fld	dword ptr [radians]
		fsincos

		mov edx, dword ptr [cosine]
		mov eax, dword ptr [sine]

		fstp dword ptr [edx]
		fstp dword ptr [eax]
	}
#else
	__asm__
	(
		"flds %0\n"
		"fsincos\n"

		"movl %1,%%edx\n"
		"movl %2,%%eax\n"

		"fstps (%%edx)\n"
		"fstps (%%eax)\n"
		:
		: "m" (radians), "m" (cosine), "m" (sine)
		: "%edx", "%eax"
	);
#endif
#elif defined (__linux__)
	sincosf(radians, sine, cosine);
#else
	*sine = sinf(radians);
	*cosine = cosf(radians);
#endif
}

void AngleVectors (vec3_t angles, vec3_t forward, vec3_t right, vec3_t up)
{
	float			angle;
	static float	sp, sy, cp, cy;
	// static to help MS compiler fp bugs

	if (angles[0] || angles[1] || angles[2])
	{
		angle = angles[YAW] * (M_PI*2 / 360);
		SinCos(angle, &sy, &cy);
///		sy = sin(angle);
///		cy = cos(angle);
		angle = angles[PITCH] * (M_PI*2 / 360);
		SinCos(angle, &sp, &cp);
///		sp = sin(angle);
///		cp = cos(angle);
		if (forward)
		{
			forward[0] = cp*cy;
			forward[1] = cp*sy;
			forward[2] = -sp;
		}
		if (right || up)
		{
			static float	sr, cr;
			// static to help MS compiler fp bugs

			angle = angles[ROLL] * (M_PI*2 / 360);
			SinCos(angle, &sr, &cr);
///			sr = sin(angle);
///			cr = cos(angle);

			if (right)
			{
				right[0] = (-sr*sp*cy+cr*sy);
				right[1] = (-sr*sp*sy-cr*cy);
				right[2] = -sr*cp;
			}
			if (up)
			{
				up[0] = (cr*sp*cy+sr*sy);
				up[1] = (cr*sp*sy-sr*cy);
				up[2] = cr*cp;
			}
		}
	}
	else
	{
		if (forward)
			VectorSet(forward, 1,0,0);
		if (right)
			VectorSet(right, 0,-1,0);
		if (up)
			VectorSet(up, 0,0,1);
	}
}


/*
===============
Info_ValueForKey

Searches the string for the given
key and returns the associated value, or an empty string.
===============
*/
char *Info_ValueForKey (char *s, char *key)
{
	char	pkey[MAX_INFO_STRING];
	static	char value[2][MAX_INFO_STRING];	// use two buffers so compares
								// work without stomping on each other
	static	int	valueindex;
	char	*o;

	valueindex ^= 1;
	if (*s == '\\')
		s++;
	while (1)
	{
		o = pkey;
		while (*s != '\\')
		{
			if (!*s)
				return "";
			*o++ = *s++;
		}
		*o = 0;
		s++;

		o = value[valueindex];

		while (*s != '\\' && *s)
		{
			if (!*s)
				return "";
			*o++ = *s++;
		}
		*o = 0;

		if (!strcmp (key, pkey) )
			return value[valueindex];

		if (!*s)
			return "";
		s++;
	}
}


void ClearBounds (vec3_t mins, vec3_t maxs)
{
	mins[0] = mins[1] = mins[2] = 99999;
	maxs[0] = maxs[1] = maxs[2] = -99999;
}


void AddPointToBounds (vec3_t v, vec3_t mins, vec3_t maxs)
{
	int		i;
	float	val;

	for (i=0 ; i<3 ; i++)
	{
		val = v[i];
		if (val < mins[i])
			mins[i] = val;
		if (val > maxs[i])
			maxs[i] = val;
	}
}


void vectoangles (vec3_t value1, vec3_t angles)
{
	float	forward;
	float	yaw, pitch;

	if (value1[1] == 0 && value1[0] == 0)
	{
		yaw = 0;
		if (value1[2] > 0)
			pitch = 90;
		else
			pitch = 270;
	}
	else
	{
		if (value1[0])
			yaw = (int) (atan2(value1[1], value1[0]) * 180 / M_PI);
		else if (value1[1] > 0)
			yaw = 90;
		else
			yaw = -90;
		if (yaw < 0)
			yaw += 360;

		forward = sqrt (value1[0]*value1[0] + value1[1]*value1[1]);
		pitch = (int) (atan2(value1[2], forward) * 180 / M_PI);
		if (pitch < 0)
			pitch += 360;
	}

	angles[PITCH] = -pitch;
	angles[YAW] = yaw;
	angles[ROLL] = 0;
}


/*
==================
Info_Validate

Some characters are illegal in info strings because they
can mess up the server's parsing
==================
*/
bool Info_Validate (char *s)
{
	if (strchr (s, '\"'))
		return false;
	if (strchr (s, ';'))
		return false;
	return true;
}


void Info_RemoveKey (char *s, char *key)
{
	char	*start;
	char	pkey[MAX_INFO_STRING];
	char	value[MAX_INFO_STRING];
	char	*o;

	if (strchr (key, '\\'))
		return;

	while (1)
	{
		start = s;
		if (*s == '\\')
			s++;
		o = pkey;
		while (*s != '\\')
		{
			if (!*s)
				return;
			*o++ = *s++;
		}
		*o = 0;
		s++;

		o = value;
		while (*s != '\\' && *s)
		{
			if (!*s)
				return;
			*o++ = *s++;
		}
		*o = 0;

		if (!strcmp (key, pkey) )
		{
			strcpy (start, s);	// remove this part
			return;
		}

		if (!*s)
			return;
	}

}


void Info_SetValueForKey (char *s, char *key, char *value)
{
	char	newi[MAX_INFO_STRING], *v;
	int		c;
	int		maxsize = MAX_INFO_STRING;

	if (strchr (key, '\\') )
	{
		gi.dprintf/*Com_Printf*/ ("Can't use keys or values with a \\\n");
		return;
	}

	if (strchr (key, ';') )
	{
		gi.dprintf/*Com_Printf*/ ("Can't use keys or values with a semicolon\n");
		return;
	}

	if (strchr (key, '\"') )
	{
		gi.dprintf/*Com_Printf*/ ("Can't use keys or values with a \"\n");
		return;
	}

	if (strlen(key) > MAX_INFO_KEY-1 || strlen(value) > MAX_INFO_KEY-1)
	{
		gi.dprintf/*Com_Printf*/ ("Keys and values must be < 64 characters.\n");
		return;
	}
	Info_RemoveKey (s, key);
	if (!value || !strlen(value))
		return;

	Com_sprintf (newi, sizeof(newi), "\\%s\\%s", key, value);

	if (strlen(newi) + strlen(s) > maxsize)
	{
		gi.dprintf/*Com_Printf*/ ("Info string length exceeded\n");
		return;
	}

	// only copy ascii values
	s += strlen(s);
	v = newi;
	while (*v)
	{
		c = *v++;
		c &= 127;		// strip high bits
		if (c >= 32 /*&& c < 127*/)
			*s++ = c;
	}
	*s = 0;
}


float	anglemod(float a)
{
	a = (360.0/65536) * ((int)(a*(65536/360.0)) & 65535);
	return a;
}


float vectoyaw (vec3_t vec)
{
	float	yaw;

	if (vec[PITCH] == 0)
	{
		yaw = 0;
		if (vec[YAW] > 0)
			yaw = 90;
		else if (vec[YAW] < 0)
			yaw = -90;
	}
	else
	{
		yaw = (int) (atan2(vec[YAW], vec[PITCH]) * 180 / M_PI);
		if (yaw < 0)
			yaw += 360;
	}

	return yaw;
}
//////////////////// COMMON PROGS END ////////////////////////////////////////////////////////////////






///////////////// AI ///////////////////////////////////////////////////////

static void M_FliesOff (edict_t *self)
{
	self->s.effects &= ~EF_FLIES;
	self->s.sound = 0;
}

static void M_FliesOn (edict_t *self)
{
	if (self->waterlevel)
		return;
	self->s.effects |= EF_FLIES;
	self->s.sound = gi.soundindex ("infantry/inflies1.wav");
	self->think = M_FliesOff;
	self->nextthink = level.time + 60;
}

static void SP_monster_flyQbe2 (edict_t *self)
{
	SP_monster_flyQbe(self);		// стартовала третья муха
	self->think = SP_monster_flyQbe;	// запрограммировали запуск четвертой мухи
	self->nextthink = level.time + 1 + 1*random();
}

static void SP_monster_flyQbe1 (edict_t *self)
{
	SP_monster_flyQbe(self);		// стартовала вторая муха
	self->think = SP_monster_flyQbe2;
	self->nextthink = level.time + 1 + 1*random();
}

static void M_FlyQbeOn (edict_t *self)
{
	if (self->waterlevel)
		return;
	SP_monster_flyQbe(self);		// стартовала первая муха
	self->think = SP_monster_flyQbe1;
	self->nextthink = level.time + 1 + 1*random();
}

#define MAX_FLYQBES	16
void M_FlyCheck (edict_t *self)
{
	if (self->waterlevel)
		return;

	if (!net_compatibility->value)
	{
		if (num_flyqbes>=MAX_FLYQBES)	// Не более MAX_FLYQBES кубмух на карте
			goto old;
		if (random() < sv_flyqbe->value)
			self->think = M_FlyQbeOn;
		else
			goto old;
	}
	else
	{
old:	if (random() > 0.5)
			return;
		self->think = M_FliesOn;
	}

	self->nextthink = level.time + 5 + 10 * random();
}

void AttackFinished (edict_t *self, float time)
{
	self->monsterinfo.attack_finished = level.time + time;
}


/*
===============
M_walkmove
===============
*/
bool M_walkmove (edict_t *ent, float yaw, float dist)
{
	vec3_t	move;
	float	s, c;

	if (!ent->groundentity && !(ent->flags & (FL_FLY|FL_SWIM)))
		return false;

	yaw = yaw*M_PI*2 / 360;

	SinCos(yaw, &s, &c);
	move[0] = c*dist;
	move[1] = s*dist;
	move[2] = 0;

	return SV_movestep(ent, move, true);
}


/*
===============
M_ChangeYaw
===============
*/
void M_ChangeYaw (edict_t *ent)
{
	float	ideal;
	float	current;
	float	move;
	float	speed;

	current = anglemod(ent->s.angles[YAW]);
	ideal = ent->ideal_yaw;

	if (current == ideal)
		return;

	move = ideal - current;
	speed = ent->yaw_speed;
	if (ideal > current)
	{
		if (move >= 180)
			move = move - 360;
	}
	else
	{
		if (move <= -180)
			move = move + 360;
	}
	if (move > 0)
	{
		if (move > speed)
			move = speed;
	}
	else
	{
		if (move < -speed)
			move = -speed;
	}

	ent->s.angles[YAW] = anglemod (current + move);
}


void HuntTarget (edict_t *self)
{
	vec3_t	vec;

	self->goalentity = self->enemy;
	if (self->monsterinfo.aiflags & AI_STAND_GROUND)
		self->monsterinfo.stand (self);
	else
		self->monsterinfo.run (self);
	VectorSubtract (self->enemy->s.origin, self->s.origin, vec);
	self->ideal_yaw = vectoyaw(vec);
	// wait a while before first attack
	if (!(self->monsterinfo.aiflags & AI_STAND_GROUND))
		AttackFinished (self, 1);
}


/*
===========
FindTarget

Self is currently not attacking anything, so try to find a target

Returns TRUE if an enemy was sighted

When a player fires a missile, the point of impact becomes a fakeplayer so
that monsters that see the impact will respond as if they had seen the
player.

To avoid spending too much time, only a single client (or fakeclient) is
checked each frame.  This means multi player games will have slightly
slower noticing monsters.
============
*/
bool FindTarget (edict_t *self)
{
	edict_t		*client;
	bool		heardit;
	int			r;

	if (self->monsterinfo.aiflags & AI_GOOD_GUY)
	{
		if (self->goalentity && self->goalentity->inuse && self->goalentity->classname)
		{
			if (strcmp(self->goalentity->classname, "target_actor") == 0)
				return false;
		}

		//FIXME look for monsters?
		return false;
	}

	// if we're going to a combat point, just proceed
	if (self->monsterinfo.aiflags & AI_COMBAT_POINT)
		return false;

// if the first spawnflag bit is set, the monster will only wake up on
// really seeing the player, not another monster getting angry or hearing
// something

// revised behavior so they will wake up if they "see" a player make a noise
// but not weapon impact/explosion noises

	heardit = false;
	if ((level.sight_entity_framenum >= (level.framenum - 1)) && !(self->spawnflags & 1) )
	{
		client = level.sight_entity;
		if (client->enemy == self->enemy)
		{
			return false;
		}
	}
	else if (level.sound_entity_framenum >= (level.framenum - 1))
	{
		client = level.sound_entity;
		heardit = true;
	}
	else if (!(self->enemy) && (level.sound2_entity_framenum >= (level.framenum - 1)) && !(self->spawnflags & 1) )
	{
		client = level.sound2_entity;
		heardit = true;
	}
	else
	{
		client = level.sight_client;
		if (!client)
			return false;	// no clients to get mad at
	}

	// if the entity went away, forget it
	if (!client->inuse)
		return false;

	if (client == self->enemy)
		return true;	// JDC false;

	if (client->client)
	{
		if (client->flags & FL_NOTARGET)
			return false;
		if (client->client->invisible_framenum > level.framenum)
			return false;
	}
	else if (client->svflags & SVF_MONSTER)
	{
		if (!client->enemy)
			return false;
		if (client->enemy->flags & FL_NOTARGET)
			return false;
	}
	else if (heardit)
	{
		if (client->owner->flags & FL_NOTARGET)
			return false;
	}
	else
		return false;

	if (!heardit)
	{
		r = range (self, client);

		if (r == RANGE_FAR)
			return false;

// this is where we would check invisibility

		// is client in an spot too dark to be seen?
		if (client->light_level <= 5)
			return false;

		if (!visible (self, client))
			return false;

		if (r == RANGE_NEAR)
		{
			if (client->show_hostile < level.time && !infront (self, client))
			{
				return false;
			}
		}
		else if (r == RANGE_MID)
		{
			if (!infront (self, client))
			{
				return false;
			}
		}

		self->enemy = client;

		if (strcmp(self->enemy->classname, "player_noise") != 0)
		{
			self->monsterinfo.aiflags &= ~AI_SOUND_TARGET;

			if (!self->enemy->client)
			{
				self->enemy = self->enemy->enemy;
				if (!self->enemy->client)
				{
					self->enemy = NULL;
					return false;
				}
			}
		}
	}
	else	// heardit
	{
		vec3_t	temp;

		if (self->spawnflags & 1)
		{
			if (!visible (self, client))
				return false;
		}
		else
		{
			if (!gi.inPHS(self->s.origin, client->s.origin))
				return false;
		}

		VectorSubtract (client->s.origin, self->s.origin, temp);

		if (VectorLength(temp) > 1000)	// too far to hear
		{
			return false;
		}

		// check area portals - if they are different and not connected then we can't hear it
		if (client->areanum != self->areanum)
			if (!gi.AreasConnected(self->areanum, client->areanum))
				return false;

		self->ideal_yaw = vectoyaw(temp);
		M_ChangeYaw (self);

		// hunt the sound for a bit; hopefully find the real player
		self->monsterinfo.aiflags |= AI_SOUND_TARGET;
		self->enemy = client;
	}

//
// got one
//
	FoundTarget (self, NULL);

	if (!(self->monsterinfo.aiflags & AI_SOUND_TARGET) && (self->monsterinfo.sight))
		self->monsterinfo.sight (self, self->enemy);

	return true;
}


/*
============
FacingIdeal

============
*/
bool FacingIdeal(edict_t *self)
{
	float	delta;

	delta = anglemod(self->s.angles[YAW] - self->ideal_yaw);
	if (delta > 45 && delta < 315)
		return false;
	return true;
}


/*
=================
fire_hit

Used for all impact (hit/punch/slash) attacks
=================
*/
bool fire_hit (edict_t *self, vec3_t aim, int damage, int kick)
{
	trace_t		tr;
	vec3_t		forward, right, up;
	vec3_t		v;
	vec3_t		point;
	float		range;
	vec3_t		dir;

	//see if enemy is in range
	VectorSubtract (self->enemy->s.origin, self->s.origin, dir);
	range = VectorLength(dir);
	if (range > aim[0])
		return false;

	if (aim[1] > self->mins[0] && aim[1] < self->maxs[0])
	{
		// the hit is straight on so back the range up to the edge of their bbox
		range -= self->enemy->maxs[0];
	}
	else
	{
		// this is a side hit so adjust the "right" value out to the edge of their bbox
		if (aim[1] < 0)
			aim[1] = self->enemy->mins[0];
		else
			aim[1] = self->enemy->maxs[0];
	}

	VectorMA (self->s.origin, range, dir, point);

	tr = gi.trace (self->s.origin, NULL, NULL, point, self, MASK_SHOT);
	if (tr.fraction < 1)
	{
		if (!tr.ent->takedamage)
			return false;
		// if it will hit any client/monster then hit the one we wanted to hit
		if ((tr.ent->svflags & SVF_MONSTER) || (tr.ent->client))
			tr.ent = self->enemy;
	}

	AngleVectors(self->s.angles, forward, right, up);
	VectorMA (self->s.origin, range, forward, point);
	VectorMA (point, aim[1], right, point);
	VectorMA (point, aim[2], up, point);
	VectorSubtract (point, self->enemy->s.origin, dir);

	// do the damage
	T_Damage (tr.ent, self, self, dir, point, vec3_origin, damage, kick/2, DAMAGE_NO_KNOCKBACK, MOD_HIT);

	if (!(tr.ent->svflags & SVF_MONSTER) && (!tr.ent->client))
		return false;

	// do our special form of knockback here
	VectorMA (self->enemy->absmin, 0.5, self->enemy->size, v);
	VectorSubtract (v, point, v);
	VectorNormalize (v);
	VectorMA (self->enemy->velocity, kick, v, self->enemy->velocity);
	if (self->enemy->velocity[2] > 0)
		self->enemy->groundentity = NULL;
	return true;
}


/*
======================
SV_CloseEnough

======================
*/
bool SV_CloseEnough (edict_t *ent, edict_t *goal, float dist)
{
	int		i;

	for (i=0 ; i<3 ; i++)
	{
		if (goal->absmin[i] > ent->absmax[i] + dist)
			return false;
		if (goal->absmax[i] < ent->absmin[i] - dist)
			return false;
	}
	return true;
}


/*
======================
SV_FixCheckBottom

======================
*/
void SV_FixCheckBottom (edict_t *ent)
{
	ent->flags |= FL_PARTIALGROUND;
}


/*
======================
SV_StepDirection

Turns to the movement direction, and walks the current distance if
facing it.

======================
*/
bool SV_StepDirection (edict_t *ent, float yaw, float dist)
{
	vec3_t		move, oldorigin;
	float		delta, s, c;

	ent->ideal_yaw = yaw;
	M_ChangeYaw (ent);

	yaw = yaw*M_PI*2 / 360;
	SinCos(yaw, &s, &c);
	move[0] = c*dist;
	move[1] = s*dist;
	move[2] = 0;

	VectorCopy (ent->s.origin, oldorigin);
	if (SV_movestep (ent, move, false))
	{
		delta = ent->s.angles[YAW] - ent->ideal_yaw;
		if (delta > 45 && delta < 315)
		{		// not turned far enough, so don't take the step
			VectorCopy (oldorigin, ent->s.origin);
		}
		gi.linkentity (ent);
		G_TouchTriggers (ent);
		return true;
	}
	gi.linkentity (ent);
	G_TouchTriggers (ent);
	return false;
}


/*
================
SV_NewChaseDir

================
*/
#define	DI_NODIR	-1
void SV_NewChaseDir (edict_t *actor, edict_t *enemy, float dist)
{
	float	deltax,deltay;
	float	d[3];
	float	tdir, olddir, turnaround;

	//FIXME: how did we get here with no enemy
	if (!enemy)
		return;

	olddir = anglemod( (int)(actor->ideal_yaw/45)*45 );
	turnaround = anglemod(olddir - 180);

	deltax = enemy->s.origin[0] - actor->s.origin[0];
	deltay = enemy->s.origin[1] - actor->s.origin[1];
	if (deltax>10)
		d[1]= 0;
	else if (deltax<-10)
		d[1]= 180;
	else
		d[1]= DI_NODIR;
	if (deltay<-10)
		d[2]= 270;
	else if (deltay>10)
		d[2]= 90;
	else
		d[2]= DI_NODIR;

// try direct route
	if (d[1] != DI_NODIR && d[2] != DI_NODIR)
	{
		if (d[1] == 0)
			tdir = d[2] == 90 ? 45 : 315;
		else
			tdir = d[2] == 90 ? 135 : 215;

		if (tdir != turnaround && SV_StepDirection(actor, tdir, dist))
			return;
	}

// try other directions
	if ( ((rand()&3) & 1) ||  fabs(deltay)>fabs(deltax))
	{
		tdir=d[1];
		d[1]=d[2];
		d[2]=tdir;
	}

	if (d[1]!=DI_NODIR && d[1]!=turnaround
	&& SV_StepDirection(actor, d[1], dist))
			return;

	if (d[2]!=DI_NODIR && d[2]!=turnaround
	&& SV_StepDirection(actor, d[2], dist))
			return;

/* there is no direct path to the player, so pick another direction */

	if (olddir!=DI_NODIR && SV_StepDirection(actor, olddir, dist))
			return;

	if (rand()&1) 	/*randomly determine direction of search*/
	{
		for (tdir=0 ; tdir<=315 ; tdir += 45)
			if (tdir!=turnaround && SV_StepDirection(actor, tdir, dist) )
					return;
	}
	else
	{
		for (tdir=315 ; tdir >=0 ; tdir -= 45)
			if (tdir!=turnaround && SV_StepDirection(actor, tdir, dist) )
					return;
	}

	if (turnaround != DI_NODIR && SV_StepDirection(actor, turnaround, dist) )
			return;

	actor->ideal_yaw = olddir;		// can't move

// if a bridge was pulled out from underneath a monster, it may not have
// a valid standing position at all

	if (!M_CheckBottom (actor))
		SV_FixCheckBottom (actor);
}


/*
======================
M_MoveToGoal
======================
*/
void M_MoveToGoal (edict_t *ent, float dist)
{
	edict_t		*goal;

	goal = ent->goalentity;

	if (!ent->groundentity && !(ent->flags & (FL_FLY|FL_SWIM)))
		return;

// if the next step hits the enemy, return immediately
	if (ent->enemy &&  SV_CloseEnough (ent, ent->enemy, dist) )
		return;

// bump around...
	if ( (rand()&3)==1 || !SV_StepDirection (ent, ent->ideal_yaw, dist))
		if (ent->inuse)
			SV_NewChaseDir (ent, goal, dist);
}


//
// monster weapons
//

//FIXME mosnters should call these with a totally accurate direction
// and we can mess it up based on skill.  Spread should be for normal
// and we can tighten or loosen based on skill.  We could muck with
// the damages too, but I'm not sure that's such a good idea.
void monster_fire_bullet (edict_t *self, vec3_t start, vec3_t dir, int damage, int kick, int hspread, int vspread, int flashtype)
{
	fire_bullet (self, start, dir, damage, kick, hspread, vspread, MOD_UNKNOWN);

	gi.WriteByte (svc_muzzleflash2);
	gi.WriteShort (self - g_edicts);
	gi.WriteByte (flashtype);
	gi.multicast (start, MULTICAST_PVS);
}

void monster_fire_shotgun (edict_t *self, vec3_t start, vec3_t aimdir, int damage, int kick, int hspread, int vspread, int count, int flashtype)
{
	fire_shotgun (self, start, aimdir, damage, kick, hspread, vspread, count, MOD_UNKNOWN);

	gi.WriteByte (svc_muzzleflash2);
	gi.WriteShort (self - g_edicts);
	gi.WriteByte (flashtype);
	gi.multicast (start, MULTICAST_PVS);
}

void monster_fire_blaster (edict_t *self, vec3_t start, vec3_t dir, int damage, int speed, int flashtype, int effect)
{
	fire_blaster (self, start, dir, damage, speed, effect, false);

	gi.WriteByte (svc_muzzleflash2);
	gi.WriteShort (self - g_edicts);
	gi.WriteByte (flashtype);
	gi.multicast (start, MULTICAST_PVS);
}

void monster_fire_grenade (edict_t *self, vec3_t start, vec3_t aimdir, int damage, int speed, int flashtype)
{
	fire_grenade (self, start, aimdir, damage, speed, 2.5, damage+40);

	gi.WriteByte (svc_muzzleflash2);
	gi.WriteShort (self - g_edicts);
	gi.WriteByte (flashtype);
	gi.multicast (start, MULTICAST_PVS);
}

void monster_fire_rocket (edict_t *self, vec3_t start, vec3_t dir, int damage, int speed, int flashtype)
{
	fire_rocket (self, start, dir, damage, speed + 50 * skill->value, damage+20, damage);

	gi.WriteByte (svc_muzzleflash2);
	gi.WriteShort (self - g_edicts);
	gi.WriteByte (flashtype);
	gi.multicast (start, MULTICAST_PVS);
}

void monster_fire_railgun (edict_t *self, vec3_t start, vec3_t aimdir, int damage, int kick, int flashtype)
{
	fire_rail (self, start, aimdir, damage, kick);

	gi.WriteByte (svc_muzzleflash2);
	gi.WriteShort (self - g_edicts);
	gi.WriteByte (flashtype);
	gi.multicast (start, MULTICAST_PVS);
}

void monster_fire_bfg (edict_t *self, vec3_t start, vec3_t aimdir, int damage, int speed, int kick, float damage_radius, int flashtype)
{
	fire_bfg (self, start, aimdir, damage, speed, damage_radius);

	gi.WriteByte (svc_muzzleflash2);
	gi.WriteShort (self - g_edicts);
	gi.WriteByte (flashtype);
	gi.multicast (start, MULTICAST_PVS);
}


void M_WorldEffects (edict_t *ent)
{
	int		dmg;

	if (ent->health > 0)
	{
		if (!(ent->flags & FL_SWIM))
		{
			if (ent->waterlevel < 3)
			{
				ent->air_finished = level.time + 12;
			}
			else if (ent->air_finished < level.time)
			{	// drown!
				if (ent->pain_debounce_time < level.time)
				{
					dmg = 2 + 2 * floor(level.time - ent->air_finished);
					if (dmg > 15)
						dmg = 15;
					T_Damage (ent, world, world, vec3_origin, ent->s.origin, vec3_origin, dmg, 0, DAMAGE_NO_ARMOR, MOD_WATER);
					ent->pain_debounce_time = level.time + 1;
				}
			}
		}
		else
		{
			if (ent->waterlevel > 0)
			{
				ent->air_finished = level.time + 9;
			}
			else if (ent->air_finished < level.time)
			{	// suffocate!
				if (ent->pain_debounce_time < level.time)
				{
					dmg = 2 + 2 * floor(level.time - ent->air_finished);
					if (dmg > 15)
						dmg = 15;
					T_Damage (ent, world, world, vec3_origin, ent->s.origin, vec3_origin, dmg, 0, DAMAGE_NO_ARMOR, MOD_WATER);
					ent->pain_debounce_time = level.time + 1;
				}
			}
		}
	}

	if (ent->waterlevel == 0)
	{
		if (ent->flags & FL_INWATER)
		{
			gi.sound (ent, CHAN_BODY, gi.soundindex("player/watr_out.wav"), 1, ATTN_NORM, 0);
			ent->flags &= ~FL_INWATER;
		}
		return;
	}

	if ((ent->watertype & CONTENTS_LAVA) && !(ent->flags & FL_IMMUNE_LAVA))
	{
		if (ent->damage_debounce_time < level.time)
		{
			ent->damage_debounce_time = level.time + 0.2;
			T_Damage (ent, world, world, vec3_origin, ent->s.origin, vec3_origin, 10*ent->waterlevel, 0, 0, MOD_LAVA);
		}
	}
	if ((ent->watertype & CONTENTS_SLIME) && !(ent->flags & FL_IMMUNE_SLIME))
	{
		if (ent->damage_debounce_time < level.time)
		{
			ent->damage_debounce_time = level.time + 1;
			T_Damage (ent, world, world, vec3_origin, ent->s.origin, vec3_origin, 4*ent->waterlevel, 0, 0, MOD_SLIME);
		}
	}

	if ( !(ent->flags & FL_INWATER) )
	{
		if (!(ent->svflags & SVF_DEADMONSTER))
		{
			if (ent->watertype & CONTENTS_LAVA)
				if (random() <= 0.5)
					gi.sound (ent, CHAN_BODY, gi.soundindex("player/lava1.wav"), 1, ATTN_NORM, 0);
				else
					gi.sound (ent, CHAN_BODY, gi.soundindex("player/lava2.wav"), 1, ATTN_NORM, 0);
			else if (ent->watertype & CONTENTS_SLIME)
				gi.sound (ent, CHAN_BODY, gi.soundindex("player/watr_in.wav"), 1, ATTN_NORM, 0);
			else if (ent->watertype & CONTENTS_WATER)
				gi.sound (ent, CHAN_BODY, gi.soundindex("player/watr_in.wav"), 1, ATTN_NORM, 0);
		}

		ent->flags |= FL_INWATER;
		ent->damage_debounce_time = 0;
	}
}


void M_MoveFrame (edict_t *self)
{
	mmove_t	*move;
	int		index;

	move = self->monsterinfo.currentmove;
	self->nextthink = level.time + FRAMETIME;

	if ((self->monsterinfo.nextframe) && (self->monsterinfo.nextframe >= move->firstframe) && (self->monsterinfo.nextframe <= move->lastframe))
	{
		self->s.frame = self->monsterinfo.nextframe;
		self->monsterinfo.nextframe = 0;
	}
	else
	{
		if (self->s.frame == move->lastframe)
		{
			if (move->endfunc)
			{
				move->endfunc (self);

				// regrab move, endfunc is very likely to change it
				move = self->monsterinfo.currentmove;

				// check for death
				if (self->svflags & SVF_DEADMONSTER)
					return;
			}
		}

		if (self->s.frame < move->firstframe || self->s.frame > move->lastframe)
		{
			self->monsterinfo.aiflags &= ~AI_HOLD_FRAME;
			self->s.frame = move->firstframe;
		}
		else
		{
			if (!(self->monsterinfo.aiflags & AI_HOLD_FRAME))
			{
				self->s.frame++;
				if (self->s.frame > move->lastframe)
					self->s.frame = move->firstframe;
			}
		}
	}

	index = self->s.frame - move->firstframe;
	if (move->frame[index].aifunc)
		if (!(self->monsterinfo.aiflags & AI_HOLD_FRAME))
			move->frame[index].aifunc (self, move->frame[index].dist * self->monsterinfo.scale);
		else
			move->frame[index].aifunc (self, 0);

	if (move->frame[index].thinkfunc)
		move->frame[index].thinkfunc (self);
}


void M_SetEffects (edict_t *ent)
{
	ent->s.effects &= ~(EF_COLOR_SHELL|EF_POWERSCREEN);
	ent->s.renderfx &= ~(RF_SHELL_RED|RF_SHELL_GREEN|RF_SHELL_BLUE);

	if (ent->monsterinfo.aiflags & AI_RESURRECTING)
	{
		ent->s.effects |= EF_COLOR_SHELL;
		ent->s.renderfx |= RF_SHELL_RED;
	}

	if (ent->health <= 0)
		return;

	if (ent->powerarmor_time > level.time)
	{
		if (ent->monsterinfo.power_armor_type == POWER_ARMOR_SCREEN)
		{
			ent->s.effects |= EF_POWERSCREEN;
		}
		else if (ent->monsterinfo.power_armor_type == POWER_ARMOR_SHIELD)
		{
			ent->s.effects |= EF_COLOR_SHELL;
			ent->s.renderfx |= RF_SHELL_GREEN;
		}
	}
}


edict_t *PlayerTrail_PickFirst (edict_t *self)
{
	int		marker;
	int		n;

	if (!trail_active)
		return NULL;

	for (marker = trail_head, n = TRAIL_LENGTH; n; n--)
	{
		if(trail[marker]->timestamp <= self->monsterinfo.trail_time)
			marker = NEXT(marker);
		else
			break;
	}

	if (visible(self, trail[marker]))
	{
		return trail[marker];
	}

	if (visible(self, trail[PREV(marker)]))
	{
		return trail[PREV(marker)];
	}

	return trail[marker];
}


edict_t *PlayerTrail_PickNext (edict_t *self)
{
	int		marker;
	int		n;

	if (!trail_active)
		return NULL;

	for (marker = trail_head, n = TRAIL_LENGTH; n; n--)
	{
		if(trail[marker]->timestamp <= self->monsterinfo.trail_time)
			marker = NEXT(marker);
		else
			break;
	}

	return trail[marker];
}


void monster_think (edict_t *self)
{
	M_MoveFrame (self);
	if (self->linkcount != self->monsterinfo.linkcount)
	{
		self->monsterinfo.linkcount = self->linkcount;
		M_CheckGround (self);
	}
	M_CatagorizePosition (self);
	M_WorldEffects (self);
	M_SetEffects (self);
}


/*
================
monster_use

Using a monster makes it angry at the current activator
================
*/
void monster_use (edict_t *self, edict_t *other, edict_t *activator)
{
	if (self->enemy)
		return;
	if (self->health <= 0)
		return;
	if (activator->flags & FL_NOTARGET)
		return;
	if (activator->client)
		if (activator->client->invisible_framenum > level.framenum)
			return;
	if (!(activator->client) && !(activator->monsterinfo.aiflags & AI_GOOD_GUY))
		return;

// delay reaction so if the monster is teleported, its sound is still heard
	self->enemy = activator;
	FoundTarget (self, NULL);
}


bool monster_start (edict_t *self)
{
	if (deathmatch->value)
	{
		G_FreeEdict (self);
		return false;
	}

	if ((self->spawnflags & 4) && !(self->monsterinfo.aiflags & AI_GOOD_GUY))
	{
		self->spawnflags &= ~4;
		self->spawnflags |= 1;
//		gi.dprintf("fixed spawnflags on %s at %s\n", self->classname, vtos(self->s.origin));
	}

	if (!(self->monsterinfo.aiflags & AI_GOOD_GUY))
		level.total_monsters++;

	self->nextthink = level.time + FRAMETIME;
	self->svflags |= SVF_MONSTER;
	self->s.renderfx |= RF_FRAMELERP;
	self->takedamage = DAMAGE_AIM;
	self->air_finished = level.time + 12;
	self->use = monster_use;
	self->max_health = self->health;
	self->clipmask = MASK_MONSTERSOLID;

	self->s.skinnum = 0;
	self->deadflag = DEAD_NO;
	self->svflags &= ~SVF_DEADMONSTER;

	if (!self->monsterinfo.checkattack)
		self->monsterinfo.checkattack = M_CheckAttack;
	VectorCopy (self->s.origin, self->s.old_origin);

	if (st.item)
	{
		self->item = FindItemByClassname (st.item);
		if (!self->item)
			gi.dprintf("%s at %s has bad item: %s\n", self->classname, vtos(self->s.origin), st.item);
	}

	// randomize what frame they start on
	if (self->monsterinfo.currentmove)
		self->s.frame = self->monsterinfo.currentmove->firstframe + (rand() % (self->monsterinfo.currentmove->lastframe - self->monsterinfo.currentmove->firstframe + 1));

	return true;
}


void monster_start_go (edict_t *self)
{
	vec3_t	v;

	if (self->health <= 0)
		return;

	// check for target to combat_point and change to combattarget
	if (self->target)
	{
		bool		notcombat;
		bool		fixup;
		edict_t		*target;

		target = NULL;
		notcombat = false;
		fixup = false;
		while ((target = G_Find (target, FOFS(targetname), self->target, NULL)) != NULL)
		{
			if (strcmp(target->classname, "point_combat") == 0)
			{
				self->combattarget = self->target;
				fixup = true;
			}
			else
			{
				notcombat = true;
			}
		}
		if (notcombat && self->combattarget)
			gi.dprintf("%s at %s has target with mixed types\n", self->classname, vtos(self->s.origin));
		if (fixup)
			self->target = NULL;
	}

	// validate combattarget
	if (self->combattarget)
	{
		edict_t		*target;

		target = NULL;
		while ((target = G_Find (target, FOFS(targetname), self->combattarget, NULL)) != NULL)
		{
			if (strcmp(target->classname, "point_combat") != 0)
			{
				gi.dprintf("%s at (%i %i %i) has a bad combattarget %s : %s at (%i %i %i)\n",
					self->classname, (int)self->s.origin[0], (int)self->s.origin[1], (int)self->s.origin[2],
					self->combattarget, target->classname, (int)target->s.origin[0], (int)target->s.origin[1],
					(int)target->s.origin[2]);
			}
		}
	}

	if (self->target)
	{
		self->goalentity = self->movetarget = G_PickTarget(self->target, NULL);
		if (!self->movetarget)
		{
			gi.dprintf ("%s can't find target %s at %s\n", self->classname, self->target, vtos(self->s.origin));
			self->target = NULL;
			self->monsterinfo.pausetime = 100000000;
			self->monsterinfo.stand (self);
		}
		else if (strcmp (self->movetarget->classname, "path_corner") == 0)
		{
			VectorSubtract (self->goalentity->s.origin, self->s.origin, v);
			self->ideal_yaw = self->s.angles[YAW] = vectoyaw(v);
			self->monsterinfo.walk (self);
			self->target = NULL;
		}
		else
		{
			self->goalentity = self->movetarget = NULL;
			self->monsterinfo.pausetime = 100000000;
			self->monsterinfo.stand (self);
		}
	}
	else
	{
		self->monsterinfo.pausetime = 100000000;
		self->monsterinfo.stand (self);
	}

	self->think = monster_think;
	self->nextthink = level.time + FRAMETIME;
}


void monster_triggered_spawn (edict_t *self)
{
	self->s.origin[2] += 1;
	KillBox (self);

	self->solid = SOLID_BBOX;
	self->movetype = MOVETYPE_STEP;
	self->svflags &= ~SVF_NOCLIENT;
	self->air_finished = level.time + 12;
	gi.linkentity (self);

	monster_start_go (self);

	if (self->enemy && self->enemy->client)
		if (self->enemy->client->invisible_framenum > level.framenum)
		{
			self->enemy = NULL;
			return;
		}

	if (self->enemy && !(self->spawnflags & 1) && !(self->enemy->flags & FL_NOTARGET))
	{
		FoundTarget (self, "monster_triggered_spawn");
	}
	else
	{
		self->enemy = NULL;
	}
}


void monster_triggered_spawn_use (edict_t *self, edict_t *other, edict_t *activator)
{
	// we have a one frame delay here so we don't telefrag the guy who activated us
	self->think = monster_triggered_spawn;
	self->nextthink = level.time + FRAMETIME;
	if (activator->client)
		self->enemy = activator;
	self->use = monster_use;
}


void monster_triggered_start (edict_t *self)
{
	self->solid = SOLID_NOT;
	self->movetype = MOVETYPE_NONE;
	self->svflags |= SVF_NOCLIENT;
	self->nextthink = 0;
	self->use = monster_triggered_spawn_use;
}


void walkmonster_start_go (edict_t *self)
{
	if (!(self->spawnflags & 2) && level.time < 1)
	{
		M_droptofloor (self);

		if (self->groundentity)
			if (!M_walkmove (self, 0, 0))
				gi.dprintf ("%s in solid at %s\n", self->classname, vtos(self->s.origin));
	}

	if (!self->yaw_speed)
		self->yaw_speed = 20;
	self->viewheight = 25;

	monster_start_go (self);

	if (self->spawnflags & 2)
		monster_triggered_start (self);
}

void walkmonster_start (edict_t *self, bool boss)
{
	if (!boss)
		if (!net_compatibility->value)
			if (sv_predator->value > random())
				self->s.renderfx |= RF_DISTORT;
	self->think = walkmonster_start_go;
	monster_start (self);
}


void flymonster_start_go (edict_t *self)
{
	if (!M_walkmove (self, 0, 0))
		gi.dprintf ("%s in solid at %s\n", self->classname, vtos(self->s.origin));

	if (!self->yaw_speed)
		self->yaw_speed = 10;
	self->viewheight = 25;

	monster_start_go (self);

	if (self->spawnflags & 2)
		monster_triggered_start (self);
}


void flymonster_start (edict_t *self, bool boss)
{
	if (!boss)
		if (!net_compatibility->value)
			if (sv_predator->value > random())
				self->s.renderfx |= RF_DISTORT;
	self->flags |= FL_FLY;
	self->think = flymonster_start_go;
	monster_start (self);
}


void swimmonster_start_go (edict_t *self)
{
	if (!self->yaw_speed)
		self->yaw_speed = 10;
	self->viewheight = 10;

	monster_start_go (self);

	if (self->spawnflags & 2)
		monster_triggered_start (self);
}

void swimmonster_start (edict_t *self)
{
	if (!net_compatibility->value)
		if (sv_predator->value > random())
			self->s.renderfx |= RF_DISTORT;
	self->flags |= FL_SWIM;
	self->think = swimmonster_start_go;
	monster_start (self);
}


bool M_CheckAttack (edict_t *self)
{
	vec3_t	spot1, spot2;
	float	chance;
	trace_t	tr;

	/// Berserker:
	if (self->enemy->client)	/// если враг - игрок
		if (self->enemy->client->invisible_framenum > level.framenum)	/// если действует невидимость
			if (enemy_range != RANGE_MELEE)		/// если дальше дистанции ближней атаки
			{
				self->goalentity = NULL;	/// не видим цель
				return false;				/// и не будем атаковать
			}

	if (self->enemy->health > 0)
	{
	// see if any entities are in the way of the shot
		VectorCopy (self->s.origin, spot1);
		spot1[2] += self->viewheight;
		VectorCopy (self->enemy->s.origin, spot2);
		spot2[2] += self->enemy->viewheight;

		tr = gi.trace (spot1, NULL, NULL, spot2, self, CONTENTS_SOLID|CONTENTS_MONSTER|CONTENTS_SLIME|CONTENTS_LAVA|CONTENTS_WINDOW);

		// do we have a clear shot?
		if (tr.ent != self->enemy)
			return false;
	}

	// melee attack
	if (enemy_range == RANGE_MELEE)
	{
		// don't always melee in easy mode
		if (skill->value == 0 && (rand()&3) )
			return false;
		if (self->monsterinfo.melee)
			self->monsterinfo.attack_state = AS_MELEE;
		else
			self->monsterinfo.attack_state = AS_MISSILE;
		return true;
	}

// missile attack
	if (!self->monsterinfo.attack)
		return false;

	if (level.time < self->monsterinfo.attack_finished)
		return false;

	if (enemy_range == RANGE_FAR)
		return false;

	if (self->monsterinfo.aiflags & AI_STAND_GROUND)
	{
		chance = 0.4;
	}
	else if (enemy_range == RANGE_MELEE)
	{
		chance = 0.2;
	}
	else if (enemy_range == RANGE_NEAR)
	{
		chance = 0.1;
	}
	else if (enemy_range == RANGE_MID)
	{
		chance = 0.02;
	}
	else
	{
		return false;
	}

	if (skill->value == 0)
		chance *= 0.5;
	else if (skill->value >= 2)
		chance *= 2;

	if (random () < chance)
	{
		self->monsterinfo.attack_state = AS_MISSILE;
		self->monsterinfo.attack_finished = level.time + 2*random();
		return true;
	}

	if (self->flags & FL_FLY)
	{
		if (random() < 0.3)
			self->monsterinfo.attack_state = AS_SLIDING;
		else
			self->monsterinfo.attack_state = AS_STRAIGHT;
	}

	return false;
}


/*
=================
AI_SetSightClient

Called once each frame to set level.sight_client to the
player to be checked for in findtarget.

If all clients are either dead or in notarget, sight_client
will be null.

In coop games, sight_client will cycle between the clients.
=================
*/
void AI_SetSightClient ()
{
	edict_t	*ent;
	int		start, check;

	if (level.sight_client == NULL)
		start = 1;
	else
		start = level.sight_client - g_edicts;

	check = start;
	while (1)
	{
		check++;
		if (check > game.maxclients)
			check = 1;
		ent = &g_edicts[check];

		if (ent->client)
			if (ent->client->invisible_framenum > level.framenum)
				goto aaa;

		if (ent->inuse && ent->health > 0 && !(ent->flags & FL_NOTARGET) )
		{
			level.sight_client = ent;
			return;		// got one
		}
aaa:	if (check == start)
		{
			level.sight_client = NULL;
			return;		// nobody to see
		}
	}
}


/*
=============
ai_run_melee

Turn and close until within an angle to launch a melee attack
=============
*/
void ai_run_melee(edict_t *self)
{
	self->ideal_yaw = enemy_yaw;
	M_ChangeYaw (self);

	if (FacingIdeal(self))
	{
		self->monsterinfo.melee (self);
		self->monsterinfo.attack_state = AS_STRAIGHT;
	}
}


/*
=============
ai_run_missile

Turn in place until within an angle to launch a missile attack
=============
*/
void ai_run_missile(edict_t *self)
{
	self->ideal_yaw = enemy_yaw;
	M_ChangeYaw (self);

	if (FacingIdeal(self))
	{
		self->monsterinfo.attack (self);
		self->monsterinfo.attack_state = AS_STRAIGHT;
	}
}


/*
=============
ai_checkattack

Decides if we're going to attack or do something else
used by ai_run and ai_stand
=============
*/
bool ai_checkattack (edict_t *self, float dist)
{
	vec3_t	temp;
	bool	hesDeadJim;

// this causes monsters to run blindly to the combat point w/o firing
	if (self->goalentity)
	{
		if (self->monsterinfo.aiflags & AI_COMBAT_POINT)
			return false;

		if (self->monsterinfo.aiflags & AI_SOUND_TARGET)
		{
			if ((level.time - self->enemy->teleport_time) > 5.0)
			{
				if (self->goalentity == self->enemy)
					if (self->movetarget)
						self->goalentity = self->movetarget;
					else
						self->goalentity = NULL;
				self->monsterinfo.aiflags &= ~AI_SOUND_TARGET;
				if (self->monsterinfo.aiflags & AI_TEMP_STAND_GROUND)
					self->monsterinfo.aiflags &= ~(AI_STAND_GROUND | AI_TEMP_STAND_GROUND);
			}
			else
			{
				self->show_hostile = level.time + 1;
				return false;
			}
		}
	}

	enemy_vis = false;

// see if the enemy is dead
	hesDeadJim = false;
	if ((!self->enemy) || (!self->enemy->inuse))
	{
		hesDeadJim = true;
	}
	else if (self->monsterinfo.aiflags & AI_MEDIC)
	{
		if (self->enemy->health > 0)
		{
			hesDeadJim = true;
			self->monsterinfo.aiflags &= ~AI_MEDIC;
		}
	}
	else
	{
		if (self->monsterinfo.aiflags & AI_BRUTAL)
		{
			if (self->enemy->health <= -80)
				hesDeadJim = true;
		}
		else
		{
			if (self->enemy->health <= 0)
				hesDeadJim = true;
		}
	}

	if (hesDeadJim)
	{
		self->enemy = NULL;
	// FIXME: look all around for other targets
		if (self->oldenemy && self->oldenemy->health > 0)
		{
			self->enemy = self->oldenemy;
			self->oldenemy = NULL;
			HuntTarget (self);
		}
		else
		{
			if (self->movetarget)
			{
				self->goalentity = self->movetarget;
				self->monsterinfo.walk (self);
			}
			else
			{
				// we need the pausetime otherwise the stand code
				// will just revert to walking with no target and
				// the monsters will wonder around aimlessly trying
				// to hunt the world entity
				self->monsterinfo.pausetime = level.time + 100000000;
				self->monsterinfo.stand (self);
			}
			return true;
		}
	}

	self->show_hostile = level.time + 1;		// wake up other monsters

// check knowledge of enemy
	enemy_vis = visible(self, self->enemy);
	if (enemy_vis)
	{
		self->monsterinfo.search_time = level.time + 5;
		VectorCopy (self->enemy->s.origin, self->monsterinfo.last_sighting);
	}

// look for other coop players here
//	if (coop && self->monsterinfo.search_time < level.time)
//	{
//		if (FindTarget (self))
//			return true;
//	}

	enemy_infront = infront(self, self->enemy);
	enemy_range = range(self, self->enemy);
	VectorSubtract (self->enemy->s.origin, self->s.origin, temp);
	enemy_yaw = vectoyaw(temp);


	// JDC self->ideal_yaw = enemy_yaw;

	if (self->monsterinfo.attack_state == AS_MISSILE)
	{
		ai_run_missile (self);
		return true;
	}
	if (self->monsterinfo.attack_state == AS_MELEE)
	{
		ai_run_melee (self);
		return true;
	}

	// if enemy is not currently visible, we will never attack
	if (!enemy_vis)
		return false;

	return self->monsterinfo.checkattack (self);
}


/*
=============
ai_stand

Used for standing around and looking for players
Distance is for slight position adjustments needed by the animations
==============
*/
void ai_stand (edict_t *self, float dist)
{
	vec3_t	v;

	if (dist)
		M_walkmove (self, self->s.angles[YAW], dist);

	if (self->monsterinfo.aiflags & AI_STAND_GROUND)
	{
		if (self->enemy)
		{
			VectorSubtract (self->enemy->s.origin, self->s.origin, v);
			self->ideal_yaw = vectoyaw(v);
			if (self->s.angles[YAW] != self->ideal_yaw && self->monsterinfo.aiflags & AI_TEMP_STAND_GROUND)
			{
				self->monsterinfo.aiflags &= ~(AI_STAND_GROUND | AI_TEMP_STAND_GROUND);
				self->monsterinfo.run (self);
			}
			M_ChangeYaw (self);
			ai_checkattack (self, 0);
		}
		else
			FindTarget (self);
		return;
	}

	if (FindTarget (self))
		return;

	if (level.time > self->monsterinfo.pausetime)
	{
		self->monsterinfo.walk (self);
		return;
	}

	if (!(self->spawnflags & 1) && (self->monsterinfo.idle) && (level.time > self->monsterinfo.idle_time))
	{
		if (self->monsterinfo.idle_time)
		{
			self->monsterinfo.idle (self);
			self->monsterinfo.idle_time = level.time + 15 + random() * 15;
		}
		else
		{
			self->monsterinfo.idle_time = level.time + random() * 15;
		}
	}
}


/*
=============
ai_move

Move the specified distance at current facing.
This replaces the QC functions: ai_forward, ai_back, ai_pain, and ai_painforward
==============
*/
void ai_move (edict_t *self, float dist)
{
	M_walkmove (self, self->s.angles[YAW], dist);
}


/*
=============
ai_charge

Turns towards target and advances
Use this call with a distnace of 0 to replace ai_face
==============
*/
void ai_charge (edict_t *self, float dist)
{
	vec3_t	v;

	VectorSubtract (self->enemy->s.origin, self->s.origin, v);
	self->ideal_yaw = vectoyaw(v);
	M_ChangeYaw (self);

	if (dist)
		M_walkmove (self, self->s.angles[YAW], dist);
}


/*
=============
ai_walk

The monster is walking it's beat
=============
*/
void ai_walk (edict_t *self, float dist)
{
	M_MoveToGoal (self, dist);
if (!strcmp(self->classname, "freed"))
return;		/// FIXED: монстр который охотится за кем-то умирает, self->enemy обнуляется, и FindTarget падает.

	// check for noticing a player
	if (FindTarget (self))
		return;

	if ((self->monsterinfo.search) && (level.time > self->monsterinfo.idle_time))
	{
		if (self->monsterinfo.idle_time)
		{
			self->monsterinfo.search (self);
			self->monsterinfo.idle_time = level.time + 15 + random() * 15;
		}
		else
		{
			self->monsterinfo.idle_time = level.time + random() * 15;
		}
	}
}


/*
=============
ai_turn

don't move, but turn towards ideal_yaw
Distance is for slight position adjustments needed by the animations
=============
*/
void ai_turn (edict_t *self, float dist)
{
	if (dist)
		M_walkmove (self, self->s.angles[YAW], dist);

	if (FindTarget (self))
		return;

	M_ChangeYaw (self);
}


/*
=============
ai_run_slide

Strafe sideways, but stay at aproximately the same range
=============
*/
void ai_run_slide(edict_t *self, float distance)
{
	float	ofs;

	self->ideal_yaw = enemy_yaw;
	M_ChangeYaw (self);

	if (self->monsterinfo.lefty)
		ofs = 90;
	else
		ofs = -90;

	if (M_walkmove (self, self->ideal_yaw + ofs, distance))
		return;

	self->monsterinfo.lefty = 1 - self->monsterinfo.lefty;
	M_walkmove (self, self->ideal_yaw - ofs, distance);
}


/*
=============
ai_run

The monster has an enemy it is trying to kill
=============
*/
void ai_run (edict_t *self, float dist)
{
	vec3_t		v;
	edict_t		*tempgoal;
	edict_t		*save;
	bool		new_;
	edict_t		*marker;
	float		d1, d2;
	trace_t		tr;
	vec3_t		v_forward, v_right;
	float		left, center, right;
	vec3_t		left_target, right_target;

	// if we're going to a combat point, just proceed
	if (self->monsterinfo.aiflags & AI_COMBAT_POINT)
	{
		M_MoveToGoal (self, dist);
		return;
	}

	if (self->monsterinfo.aiflags & AI_SOUND_TARGET)
	{
		VectorSubtract (self->s.origin, self->enemy->s.origin, v);
		if (VectorLength(v) < 64)
		{
			self->monsterinfo.aiflags |= (AI_STAND_GROUND | AI_TEMP_STAND_GROUND);
			self->monsterinfo.stand (self);
			return;
		}

		M_MoveToGoal (self, dist);
if (!strcmp(self->classname, "freed"))
return;		/// FIXED: монстр который охотится за кем-то умирает, self->enemy обнуляется, и FindTarget падает.

		if (!FindTarget (self))
			return;
	}

	if (ai_checkattack (self, dist))
		return;

	if (self->monsterinfo.attack_state == AS_SLIDING)
	{
		ai_run_slide (self, dist);
		return;
	}

	if (enemy_vis)
	{
//		if (self.aiflags & AI_LOST_SIGHT)
//			dprint("regained sight\n");
		M_MoveToGoal (self, dist);
if (!self->enemy)
return;		/// FIXED: монстр который охотится за кем-то умирает, self->enemy обнуляется, и далее падает.
		self->monsterinfo.aiflags &= ~AI_LOST_SIGHT;
		VectorCopy (self->enemy->s.origin, self->monsterinfo.last_sighting);
		self->monsterinfo.trail_time = level.time;
		return;
	}

	// coop will change to another enemy if visible
	if (coop->value)
	{	// FIXME: insane guys get mad with this, which causes crashes!
		if (FindTarget (self))
			return;
	}

	if ((self->monsterinfo.search_time) && (level.time > (self->monsterinfo.search_time + 20)))
	{
		M_MoveToGoal (self, dist);
		self->monsterinfo.search_time = 0;
//		dprint("search timeout\n");
		return;
	}

	save = self->goalentity;
	tempgoal = G_Spawn();
	self->goalentity = tempgoal;

	new_ = false;

	if (!(self->monsterinfo.aiflags & AI_LOST_SIGHT))
	{
		// just lost sight of the player, decide where to go first
//		dprint("lost sight of player, last seen at "); dprint(vtos(self.last_sighting)); dprint("\n");
		self->monsterinfo.aiflags |= (AI_LOST_SIGHT | AI_PURSUIT_LAST_SEEN);
		self->monsterinfo.aiflags &= ~(AI_PURSUE_NEXT | AI_PURSUE_TEMP);
		new_ = true;
	}

	if (self->monsterinfo.aiflags & AI_PURSUE_NEXT)
	{
		self->monsterinfo.aiflags &= ~AI_PURSUE_NEXT;
//		dprint("reached current goal: "); dprint(vtos(self.origin)); dprint(" "); dprint(vtos(self.last_sighting)); dprint(" "); dprint(ftos(vlen(self.origin - self.last_sighting))); dprint("\n");

		// give ourself more time since we got this far
		self->monsterinfo.search_time = level.time + 5;

		if (self->monsterinfo.aiflags & AI_PURSUE_TEMP)
		{
//			dprint("was temp goal; retrying original\n");
			self->monsterinfo.aiflags &= ~AI_PURSUE_TEMP;
			marker = NULL;
			VectorCopy (self->monsterinfo.saved_goal, self->monsterinfo.last_sighting);
			new_ = true;
		}
		else if (self->monsterinfo.aiflags & AI_PURSUIT_LAST_SEEN)
		{
			self->monsterinfo.aiflags &= ~AI_PURSUIT_LAST_SEEN;
			marker = PlayerTrail_PickFirst (self);
		}
		else
		{
			marker = PlayerTrail_PickNext (self);
		}

		if (marker)
		{
			VectorCopy (marker->s.origin, self->monsterinfo.last_sighting);
			self->monsterinfo.trail_time = marker->timestamp;
			self->s.angles[YAW] = self->ideal_yaw = marker->s.angles[YAW];
//			dprint("heading is "); dprint(ftos(self.ideal_yaw)); dprint("\n");

//			debug_drawline(self.origin, self.last_sighting, 52);
			new_ = true;
		}
	}

	VectorSubtract (self->s.origin, self->monsterinfo.last_sighting, v);
	d1 = VectorLength(v);
	if (d1 <= dist)
	{
		self->monsterinfo.aiflags |= AI_PURSUE_NEXT;
		dist = d1;
	}

	VectorCopy (self->monsterinfo.last_sighting, self->goalentity->s.origin);

	if (new_)
	{
//		gi.dprintf("checking for course correction\n");

		tr = gi.trace(self->s.origin, self->mins, self->maxs, self->monsterinfo.last_sighting, self, MASK_PLAYERSOLID);
		if (tr.fraction < 1)
		{
			VectorSubtract (self->goalentity->s.origin, self->s.origin, v);
			d1 = VectorLength(v);
			center = tr.fraction;
			d2 = d1 * ((center+1)/2);
			self->s.angles[YAW] = self->ideal_yaw = vectoyaw(v);
			AngleVectors(self->s.angles, v_forward, v_right, NULL);

			VectorSet(v, d2, -16, 0);
			G_ProjectSource (self->s.origin, v, v_forward, v_right, left_target);
			tr = gi.trace(self->s.origin, self->mins, self->maxs, left_target, self, MASK_PLAYERSOLID);
			left = tr.fraction;

			VectorSet(v, d2, 16, 0);
			G_ProjectSource (self->s.origin, v, v_forward, v_right, right_target);
			tr = gi.trace(self->s.origin, self->mins, self->maxs, right_target, self, MASK_PLAYERSOLID);
			right = tr.fraction;

			center = (d1*center)/d2;
			if (left >= center && left > right)
			{
				if (left < 1)
				{
					VectorSet(v, d2 * left * 0.5, -16, 0);
					G_ProjectSource (self->s.origin, v, v_forward, v_right, left_target);
//					gi.dprintf("incomplete path, go part way and adjust again\n");
				}
				VectorCopy (self->monsterinfo.last_sighting, self->monsterinfo.saved_goal);
				self->monsterinfo.aiflags |= AI_PURSUE_TEMP;
				VectorCopy (left_target, self->goalentity->s.origin);
				VectorCopy (left_target, self->monsterinfo.last_sighting);
				VectorSubtract (self->goalentity->s.origin, self->s.origin, v);
				self->s.angles[YAW] = self->ideal_yaw = vectoyaw(v);
//				gi.dprintf("adjusted left\n");
//				debug_drawline(self.origin, self.last_sighting, 152);
			}
			else if (right >= center && right > left)
			{
				if (right < 1)
				{
					VectorSet(v, d2 * right * 0.5, 16, 0);
					G_ProjectSource (self->s.origin, v, v_forward, v_right, right_target);
//					gi.dprintf("incomplete path, go part way and adjust again\n");
				}
				VectorCopy (self->monsterinfo.last_sighting, self->monsterinfo.saved_goal);
				self->monsterinfo.aiflags |= AI_PURSUE_TEMP;
				VectorCopy (right_target, self->goalentity->s.origin);
				VectorCopy (right_target, self->monsterinfo.last_sighting);
				VectorSubtract (self->goalentity->s.origin, self->s.origin, v);
				self->s.angles[YAW] = self->ideal_yaw = vectoyaw(v);
//				gi.dprintf("adjusted right\n");
//				debug_drawline(self.origin, self.last_sighting, 152);
			}
		}
//		else gi.dprintf("course was fine\n");
	}

	M_MoveToGoal (self, dist);

	G_FreeEdict(tempgoal);

	if (self)
		self->goalentity = save;
}


////////////////////////////////////////////////////////////////////////////
















gitem_t	*GetItemByIndex (int index)
{
	if (index == 0 || index >= game.num_items)
		return NULL;

	return &itemlist[index];
}


int PowerArmorType (edict_t *ent)
{
	if (!ent->client)
		return POWER_ARMOR_NONE;

	if (!(ent->flags & FL_POWER_ARMOR))
		return POWER_ARMOR_NONE;

	if (ent->client->pers.inventory[power_shield_index] > 0)
		return POWER_ARMOR_SHIELD;

	if (ent->client->pers.inventory[power_screen_index] > 0)
		return POWER_ARMOR_SCREEN;

	return POWER_ARMOR_NONE;
}

// LARGE_MAP_SIZE
bool LargeMapCheck(vec3_t org)
{
	if (!net_compatibility->value)
		if (	org[0] >  4095 || org[0] < -4096
			||	org[1] >  4095 || org[1] < -4096
			||	org[2] >  4095 || org[2] < -4096 )
			return true;
	return false;
}



void SpawnDamage (int type, vec3_t origin, vec3_t normal/*, int damage*/)
{
//	if (damage > 255)
//		damage = 255;

	bool large_map = LargeMapCheck(origin);
	if (large_map)
		gi.WriteByte (svc_temp_entity_ext);
	else
		gi.WriteByte (svc_temp_entity);
	gi.WriteByte (type);
//	gi.WriteByte (damage);
	gi.WritePosition (origin, large_map);
	gi.WriteDir (normal);
	gi.multicast (origin, MULTICAST_PVS);
}


/*

.enemy
Will be world if not currently angry at anyone.

.movetarget
The next path spot to walk toward.  If .enemy, ignore .movetarget.
When an enemy is killed, the monster will try to return to it's path.

.hunt_time
Set to time + something when the player is in sight, but movement straight for
him is blocked.  This causes the monster to use wall following code for
movement direction instead of sighting on the player.

.ideal_yaw
A yaw angle of the intended direction, which will be turned towards at up
to 45 deg / state.  If the enemy is in view and hunt_time is not active,
this will be the exact line towards the enemy.

.pausetime
A monster will leave it's stand state and head towards it's .movetarget when
time > .pausetime.

walkmove(angle, speed) primitive is all or nothing
*/

/*
=============
range

returns the range catagorization of an entity reletive to self
0	melee range, will become hostile even if back is turned
1	visibility and infront, or visibility and show hostile
2	infront and show hostile
3	only triggered by damage
=============
*/
int range (edict_t *self, edict_t *other)
{
	vec3_t	v;
	float	len;

	VectorSubtract (self->s.origin, other->s.origin, v);
	len = VectorLength (v);
	if (len < MELEE_DISTANCE)
		return RANGE_MELEE;
	if (len < 500)
		return RANGE_NEAR;
	if (len < 1000)
		return RANGE_MID;
	return RANGE_FAR;
}


/*
=============
visible

returns 1 if the entity is visible to self, even if not infront ()
=============
*/
bool visible (edict_t *self, edict_t *other)
{
	vec3_t	spot1;
	vec3_t	spot2;
	trace_t	trace;

	VectorCopy (self->s.origin, spot1);
	spot1[2] += self->viewheight;
	VectorCopy (other->s.origin, spot2);
	spot2[2] += other->viewheight;
	trace = gi.trace (spot1, vec3_origin, vec3_origin, spot2, self, MASK_OPAQUE);

	if (trace.fraction == 1.0)
		return true;
	return false;
}


/*
============
T_Damage

targ		entity that is being damaged
inflictor	entity that is causing the damage
attacker	entity that caused the inflictor to damage targ
	example: targ=monster, inflictor=rocket, attacker=player

dir			direction of the attack
point		point at which the damage is being inflicted
normal		normal vector from that point
damage		amount of damage being inflicted
knockback	force to be applied against targ as a result of the damage

dflags		these flags are used to control how T_Damage works
	DAMAGE_RADIUS			damage was indirect (from a nearby explosion)
	DAMAGE_NO_ARMOR			armor does not protect from this damage
	DAMAGE_ENERGY			damage is from an energy based weapon
	DAMAGE_NO_KNOCKBACK		do not affect velocity, just view angles
	DAMAGE_BULLET			damage is from a bullet (used for ricochets)
	DAMAGE_NO_PROTECTION	kills godmode, armor, everything
============
*/
int CheckPowerArmor (edict_t *ent, vec3_t point, vec3_t normal, int damage, int dflags)
{
	gclient_t	*client;
	int			save;
	int			power_armor_type;
	int			index;
	int			damagePerCell;
	int			pa_te_type;
	int			power;
	int			power_used;

	if (!damage)
		return 0;

	client = ent->client;

	if (dflags & DAMAGE_NO_ARMOR)
		return 0;

	if (client)
	{
		power_armor_type = PowerArmorType (ent);
		if (power_armor_type != POWER_ARMOR_NONE)
		{
			index = ITEM_INDEX(FindItem("Cells"));
			power = client->pers.inventory[index];
		}
	}
	else if (ent->svflags & SVF_MONSTER)
	{
		power_armor_type = ent->monsterinfo.power_armor_type;
		power = ent->monsterinfo.power_armor_power;
	}
	else
		return 0;

	if (power_armor_type == POWER_ARMOR_NONE)
		return 0;
	if (!power)
		return 0;

	if (power_armor_type == POWER_ARMOR_SCREEN)
	{
		vec3_t		vec;
		float		dot;
		vec3_t		forward;

		// only works if damage point is in front
		AngleVectors (ent->s.angles, forward, NULL, NULL);
		VectorSubtract (point, ent->s.origin, vec);
		VectorNormalize (vec);
		dot = DotProduct (vec, forward);
		if (dot <= 0.3)
			return 0;

		damagePerCell = 1;
		pa_te_type = TE_SCREEN_SPARKS;
		damage = damage / 3;
	}
	else
	{
		damagePerCell = 2;
		pa_te_type = TE_SHIELD_SPARKS;
		damage = (2 * damage) / 3;
	}

	save = power * damagePerCell;
	if (!save)
		return 0;
	if (save > damage)
		save = damage;

	SpawnDamage (pa_te_type, point, normal/*, save*/);
	ent->powerarmor_time = level.time + 0.2;

	power_used = save / damagePerCell;

	if (client)
		client->pers.inventory[index] -= power_used;
	else
		ent->monsterinfo.power_armor_power -= power_used;
	return save;
}


int CheckArmor (edict_t *ent, vec3_t point, vec3_t normal, int damage, int te_sparks, int dflags)
{
	gclient_t	*client;
	int			save;
	int			index;
	gitem_t		*armor;

	if (!damage)
		return 0;

	client = ent->client;

	if (!client)
		return 0;

	if (dflags & DAMAGE_NO_ARMOR)
		return 0;

	index = ArmorIndex (ent);
	if (!index)
		return 0;

	armor = GetItemByIndex (index);

	if (dflags & DAMAGE_ENERGY)
		save = ceil(((gitem_armor_t *)armor->info)->energy_protection*damage);
	else
		save = ceil(((gitem_armor_t *)armor->info)->normal_protection*damage);
	if (save >= client->pers.inventory[index])
		save = client->pers.inventory[index];

	if (!save)
		return 0;

	client->pers.inventory[index] -= save;
	SpawnDamage (te_sparks, point, normal/*, save*/);

	return save;
}


/*
=============
G_PickTarget

Searches all active entities for the next one that holds
the matching string at fieldofs (use the FOFS() macro) in the structure.

Searches beginning at the edict after from, or the beginning if NULL
NULL will be returned if the end of the list is reached.

=============
*/
#define MAXCHOICES	8

edict_t *G_PickTarget (char *targetname, char *func)
{
	edict_t	*ent = NULL;
	int		num_choices = 0;
	edict_t	*choice[MAXCHOICES];

	if (!targetname)
	{
		gi.dprintf("G_PickTarget called with NULL targetname\n");
		return NULL;
	}

	while(1)
	{
		ent = G_Find (ent, FOFS(targetname), targetname, func);
		if (!ent)
			break;
		choice[num_choices++] = ent;
		if (num_choices == MAXCHOICES)
			break;
	}

	if (!num_choices)
	{
		gi.dprintf("G_PickTarget: target %s not found\n", targetname);
		return NULL;
	}

	return choice[rand() % num_choices];
}


void FoundTarget (edict_t *self, char *func)
{
	// let other monsters see this monster for a while
	if (self->enemy->client)
	{
		level.sight_entity = self;
		level.sight_entity_framenum = level.framenum;
		level.sight_entity->light_level = 128;
	}

	self->show_hostile = level.time + 1;		// wake up other monsters

	VectorCopy(self->enemy->s.origin, self->monsterinfo.last_sighting);
	self->monsterinfo.trail_time = level.time;

	if (!self->combattarget)
	{
		HuntTarget (self);
		return;
	}

	self->goalentity = self->movetarget = G_PickTarget(self->combattarget, func);
	if (!self->movetarget)
	{
		self->goalentity = self->movetarget = self->enemy;
		HuntTarget (self);
		gi.dprintf("%s at %s, combattarget %s not found\n", self->classname, vtos(self->s.origin), self->combattarget);
		return;
	}

	// clear out our combattarget, these are a one shot deal
	self->combattarget = NULL;
	self->monsterinfo.aiflags |= AI_COMBAT_POINT;

	// clear the targetname, that point is ours!
	self->movetarget->targetname = NULL;
	self->monsterinfo.pausetime = 0;

	// run for it
	self->monsterinfo.run (self);
}


void M_ReactToDamage (edict_t *targ, edict_t *attacker)
{
	if (!(attacker->client) && !(attacker->svflags & SVF_MONSTER))
		return;

	if (attacker == targ || attacker == targ->enemy)
		return;

	// if we are a good guy monster and our attacker is a player
	// or another good guy, do not get mad at them
	if (targ->monsterinfo.aiflags & AI_GOOD_GUY)
	{
		if (attacker->client || (attacker->monsterinfo.aiflags & AI_GOOD_GUY))
			return;
	}

	// we now know that we are not both good guys

	// if attacker is a client, get mad at them because he's good and we're not
	if (attacker->client)
	{
		targ->monsterinfo.aiflags &= ~AI_SOUND_TARGET;

		// this can only happen in coop (both new and old enemies are clients)
		// only switch if can't see the current enemy
		if (targ->enemy && targ->enemy->client)
		{
			if (visible(targ, targ->enemy))
			{
				targ->oldenemy = attacker;
				return;
			}
			targ->oldenemy = targ->enemy;
		}
		targ->enemy = attacker;
		if (!(targ->monsterinfo.aiflags & AI_DUCKED))
			FoundTarget (targ, NULL);
		return;
	}

	// it's the same base (walk/swim/fly) type and a different classname and it's not a tank
	// (they spray too much), get mad at them
	if (((targ->flags & (FL_FLY|FL_SWIM)) == (attacker->flags & (FL_FLY|FL_SWIM))) &&
		 (strcmp (targ->classname, attacker->classname) != 0) &&
		 (strcmp(attacker->classname, "monster_tank") != 0) &&
		 (strcmp(attacker->classname, "monster_supertank") != 0) &&
		 (strcmp(attacker->classname, "monster_makron") != 0) &&
		 (strcmp(attacker->classname, "monster_jorg") != 0) )
	{
		if (targ->enemy && targ->enemy->client)
			targ->oldenemy = targ->enemy;
		targ->enemy = attacker;
		if (!(targ->monsterinfo.aiflags & AI_DUCKED))
			FoundTarget (targ, NULL);
	}
	// if they *meant* to shoot us, then shoot back
	else if (attacker->enemy == targ)
	{
		if (targ->enemy && targ->enemy->client)
			targ->oldenemy = targ->enemy;
		targ->enemy = attacker;
		if (!(targ->monsterinfo.aiflags & AI_DUCKED))
			FoundTarget (targ, NULL);
	}
	// otherwise get mad at whoever they are mad at (help our buddy) unless it is us!
	else if (attacker->enemy && attacker->enemy != targ)
	{
		if (targ->enemy && targ->enemy->client)
			targ->oldenemy = targ->enemy;
		targ->enemy = attacker->enemy;
		if (!(targ->monsterinfo.aiflags & AI_DUCKED))
			FoundTarget (targ, NULL);
	}
}


bool CheckTeamDamage (edict_t *targ, edict_t *attacker)
{
		//FIXME make the next line real and uncomment this block
		// if ((ability to damage a teammate == OFF) && (targ's team == attacker's team))
	return false;
}


char *ClientTeam (edict_t *ent)
{
	char		*p;
	static char	value[MAX_INFO_STRING];

	value[0] = 0;

	if (!ent->client)
		return value;

	strcpy(value, Info_ValueForKey (ent->client->pers.userinfo, "skin"));
	p = strchr(value, '/');
	if (!p)
		return value;

	if ((int)(dmflags->value) & DF_MODELTEAMS)
	{
		*p = 0;
		return value;
	}

	return ++p;
}


bool OnSameTeam (edict_t *ent1, edict_t *ent2)
{
	char	ent1Team [MAX_INFO_STRING];
	char	ent2Team [MAX_INFO_STRING];

	if (!((int)(dmflags->value) & (DF_MODELTEAMS | DF_SKINTEAMS)))
		return false;

	strcpy (ent1Team, ClientTeam (ent1));
	strcpy (ent2Team, ClientTeam (ent2));

	if (strcmp(ent1Team, ent2Team) == 0)
		return true;
	return false;
}


void drop_temp_touch (edict_t *ent, edict_t *other, cplane_t *plane, csurface_t *surf)
{
	if (other == ent->owner)
		return;

	Touch_Item (ent, other, plane, surf);
}


void G_ProjectSource (vec3_t point, float *distance, vec3_t forward, vec3_t right, vec3_t result)
{
	result[0] = point[0] + forward[0] * distance[0] + right[0] * distance[1];
	result[1] = point[1] + forward[1] * distance[0] + right[1] * distance[1];
	result[2] = point[2] + forward[2] * distance[0] + right[2] * distance[1] + distance[2];
}


void drop_make_touchable (edict_t *ent)
{
	ent->touch = Touch_Item;
	if (deathmatch->value)
	{
		ent->nextthink = level.time + 29;
		ent->think = G_FreeEdict;
	}
}


edict_t *Drop_Item (edict_t *ent, gitem_t *item)
{
	edict_t	*dropped;
	vec3_t	forward, right;
	vec3_t	offset;

	dropped = G_Spawn();

	dropped->classname = item->classname;
	dropped->item = item;
	dropped->spawnflags = DROPPED_ITEM;
	dropped->s.effects = item->world_model_flags;
	dropped->s.renderfx = 0/*RF_GLOW*/;
	VectorSet (dropped->mins, -15, -15, -15);
	VectorSet (dropped->maxs, 15, 15, 15);
	gi.setmodel (dropped, dropped->item->world_model);
	dropped->solid = SOLID_TRIGGER;
	dropped->movetype = MOVETYPE_TOSS;
	dropped->touch = drop_temp_touch;
	dropped->owner = ent;

	if (ent->client)
	{
		trace_t	trace;

		AngleVectors (ent->client->v_angle, forward, right, NULL);
		VectorSet(offset, 24, 0, -16);
		G_ProjectSource (ent->s.origin, offset, forward, right, dropped->s.origin);
		trace = gi.trace (ent->s.origin, dropped->mins, dropped->maxs, dropped->s.origin, ent, CONTENTS_SOLID|CONTENTS_AUX);
		VectorCopy (trace.endpos, dropped->s.origin);
	}
	else
	{
		AngleVectors (ent->s.angles, forward, right, NULL);
		VectorCopy (ent->s.origin, dropped->s.origin);
	}

	VectorScale (forward, 100, dropped->velocity);
	dropped->velocity[2] = 300;

	dropped->think = drop_make_touchable;
	dropped->nextthink = level.time + 1;

	gi.linkentity (dropped);

	return dropped;
}


/*
================
monster_death_use

When a monster dies, it fires all of its targets with the current
enemy as activator.
================
*/
void monster_death_use (edict_t *self)
{
	self->flags &= ~(FL_FLY|FL_SWIM);
	self->monsterinfo.aiflags &= AI_GOOD_GUY;

	if (self->item)
	{
		Drop_Item (self, self->item);
		self->item = NULL;
	}

	if (self->deathtarget)
		self->target = self->deathtarget;

	if (!self->target)
		return;

	G_UseTargets (self, self->enemy, "monster_death");
}


void Killed (edict_t *targ, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point)
{
	if (targ->health < -999)
		targ->health = -999;

	targ->enemy = attacker;

	if ((targ->svflags & SVF_MONSTER) && (targ->deadflag != DEAD_DEAD))
	{
		if (!(targ->monsterinfo.aiflags & AI_GOOD_GUY))
		{
			level.killed_monsters++;
			if (coop->value && attacker->client)
				attacker->client->resp.score++;
			// medics won't heal monsters that they kill themselves
			if (strcmp(attacker->classname, "monster_medic") == 0)
				targ->owner = attacker;
		}
	}

	if (targ->movetype == MOVETYPE_PUSH || targ->movetype == MOVETYPE_STOP || targ->movetype == MOVETYPE_NONE)
	{	// doors, triggers, etc
		targ->die (targ, inflictor, attacker, damage, point);
		return;
	}

	if ((targ->svflags & SVF_MONSTER) && (targ->deadflag != DEAD_DEAD))
	{
		targ->touch = NULL;
		monster_death_use (targ);
	}

	targ->die (targ, inflictor, attacker, damage, point);
}


void T_Damage (edict_t *targ, edict_t *inflictor, edict_t *attacker, vec3_t dir, vec3_t point, vec3_t normal, int damage, int knockback, int dflags, int mod)
{
	gclient_t	*client;
	int			take;
	int			save;
	int			asave;
	int			psave;
	int			te_sparks;
	float		ds;

	if (!targ->takedamage)
		return;

	// friendly fire avoidance
	// if enabled you can't hurt teammates (but you can hurt yourself)
	// knockback still occurs
	if ((targ != attacker) && ((deathmatch->value && ((int)(dmflags->value) & (DF_MODELTEAMS | DF_SKINTEAMS))) || coop->value))
	{
		if (OnSameTeam (targ, attacker))
		{
			if ((int)(dmflags->value) & DF_NO_FRIENDLY_FIRE)
				damage = 0;
			else
				mod |= MOD_FRIENDLY_FIRE;
		}
	}
	meansOfDeath = mod;

	client = targ->client;

	// easy mode takes half damage
	if (skill->value == 0 && deathmatch->value == 0 && client)
	{
		damage *= 0.5;
		if (!damage)
			damage = 1;
	}

	if (dflags & DAMAGE_BULLET)
		te_sparks = TE_BULLET_SPARKS;
	else
		te_sparks = TE_SPARKS;

	VectorNormalize(dir);

// bonus damage for suprising a monster
	if (!(dflags & DAMAGE_RADIUS) && (targ->svflags & SVF_MONSTER) && (attacker->client) && (!targ->enemy) && (targ->health > 0))
		damage *= 2;

	if (targ->flags & FL_NO_KNOCKBACK)
		knockback = 0;

	if ((targ != attacker) && (deathmatch->value && ((int)(dmflags->value) & DF_INSTAGIB)) && (mod == MOD_ROCKET || mod == MOD_GRENADE || mod == MOD_RAILGUN))
		damage = 10000000;
	else
	{
		if (attacker->client)
			ds = g_playerDamageScale->value;
		else if (attacker->svflags & SVF_MONSTER)
			ds = g_monsterDamageScale->value;
		else
			ds = g_enviroDamageScale->value;

		damage = (int)((float)damage * ds);
		if (damage <= 0)
			return;
	}

// figure momentum add
	if (!(dflags & DAMAGE_NO_KNOCKBACK))
	{
		if ((knockback) && (targ->movetype != MOVETYPE_NONE) && (targ->movetype != MOVETYPE_BOUNCE) && (targ->movetype != MOVETYPE_PUSH) && (targ->movetype != MOVETYPE_STOP))
		{
			vec3_t	kvel;
			float	mass;

			if (targ->mass < 50)
				mass = 50;
			else
				mass = targ->mass;

			if (targ->client  && attacker == targ)
				VectorScale (dir, fabs(sv_knockback->value) * (float)knockback / mass, kvel);	// the rocket jump hack...
			else
				VectorScale (dir, 500.0 * (float)knockback / mass, kvel);

			VectorAdd (targ->velocity, kvel, targ->velocity);
		}
	}

	take = damage;
	save = 0;

	// check for godmode
	if ( (targ->flags & FL_GODMODE) && !(dflags & DAMAGE_NO_PROTECTION) )
	{
		take = 0;
		save = damage;
		SpawnDamage (te_sparks, point, normal/*, save*/);
	}

	// check for invincibility
	if ((client && client->invincible_framenum > level.framenum ) && !(dflags & DAMAGE_NO_PROTECTION))
	{
		if (targ->pain_debounce_time < level.time)
		{
			gi.sound(targ, CHAN_ITEM, gi.soundindex("items/protect4.wav"), 1, ATTN_NORM, 0);
			targ->pain_debounce_time = level.time + 2;
		}
		take = 0;
		save = damage;
	}

	psave = CheckPowerArmor (targ, point, normal, take, dflags);
	take -= psave;

	asave = CheckArmor (targ, point, normal, take, te_sparks, dflags);
	take -= asave;

	//treat cheat/powerup savings the same as armor
	asave += save;

	// team damage avoidance
	if (!(dflags & DAMAGE_NO_PROTECTION) && CheckTeamDamage (targ, attacker))
		return;

// do the damage
	if (take)
	{
		if ((targ->svflags & SVF_MONSTER) || (client))
		{
			if(targ == attacker)
				SpawnDamage (TE_BLOOD, targ->s.origin, normal/*, take*/);	/// Fixed by Berserker: при взрыве ракеты, если "сам виноват", кровь течет от виновного
			else
				SpawnDamage (TE_BLOOD, point, normal/*, take*/);

			// Berserker: простреливаем таргет навылет
///			if (game.maxclients == 1)		// send effect if in a single-player game
			{
				if (dflags & (DAMAGE_ENERGY|DAMAGE_BULLET))
				{
					float	probability;
					probability = ((float)(targ->max_health - targ->health))/(float)targ->max_health;
					if (random() < probability)
					{	// чем меньше здоровье, тем вероятнее "прострел"
						vec3_t	end, org, ndir;
						// небольшие отклонения от первоначального dir
						VectorMA(point, 24, dir, org);
						org[0] += crandom();
						org[1] += crandom();
						org[2] += crandom();
						VectorSubtract(org, point, ndir);
						VectorNormalize(ndir);
						VectorMA (point, 96 + probability * 128, ndir, end);		// до 96...224 единиц летит кровь

						trace_t	tr = gi.trace (point, NULL, NULL, end, targ, MASK_SHOT);
						if (tr.fraction < 1.0 && !((tr.ent->svflags & SVF_MONSTER) || (tr.ent->svflags & SVF_DEADMONSTER) || (tr.ent->client)))
							SpawnDamage (TE_BLOOD, tr.endpos, normal/*, take*/);
						else
							SpawnDamage (TE_BLOOD, end, normal/*, take*/);
					}
				}
			}
		}
		else
			SpawnDamage (te_sparks, point, normal/*, take*/);

		targ->health = targ->health - take;

		if (targ->health <= 0)
		{
			if ((targ->svflags & SVF_MONSTER) || (client))
				targ->flags |= FL_NO_KNOCKBACK;
			Killed (targ, inflictor, attacker, take, point);
			return;
		}
	}

	if (targ->svflags & SVF_MONSTER)
	{
		M_ReactToDamage (targ, attacker);
		if (!(targ->monsterinfo.aiflags & AI_DUCKED) && (take))
		{
			targ->pain (targ, attacker, knockback, take);
			// nightmare mode monsters don't go into pain frames often
			if (skill->value == 3)
				targ->pain_debounce_time = level.time + 5;
		}
	}
	else if (client)
	{
		if (!(targ->flags & FL_GODMODE) && (take) && (mod!=MOD_UNKNOWN && mod!=MOD_WATER && mod!=MOD_SLIME && mod!=MOD_LAVA))
		{
			targ->pain (targ, attacker, knockback, take);
			targ->shell_debounce_time = level.time + 0.5;
		}
	}
	else if (take)
	{
		if (targ->pain)
			targ->pain (targ, attacker, knockback, take);
	}

	// add to the damage inflicted on a player this frame
	// the total will be turned into screen blends and view angle kicks
	// at the end of the frame
	if (client)
	{
		client->damage_parmor += psave;
		client->damage_armor += asave;
		client->damage_blood += take;
		client->damage_knockback += knockback;
		VectorCopy (point, client->damage_from);
	}
}

































void debris_die (edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point)
{
	G_FreeEdict (self);
}

void ThrowDebris (edict_t *self, char *modelname, float speed, vec3_t origin)
{
	edict_t	*chunk;
	vec3_t	v;

	chunk = G_Spawn();
	VectorCopy (origin, chunk->s.origin);
	gi.setmodel (chunk, modelname);
	v[0] = 100 * crandom();
	v[1] = 100 * crandom();
	v[2] = 100 + 100 * crandom();
	VectorMA (self->velocity, speed, v, chunk->velocity);
	chunk->movetype = MOVETYPE_BOUNCE;
	chunk->solid = SOLID_NOT;
	chunk->avelocity[0] = random()*600;
	chunk->avelocity[1] = random()*600;
	chunk->avelocity[2] = random()*600;
	chunk->think = G_FreeEdict;
	chunk->nextthink = level.time + 5 + random()*5;
	chunk->s.frame = 0;
	chunk->flags = 0;
	chunk->classname = "debris";
	chunk->takedamage = DAMAGE_YES;
	chunk->die = debris_die;
	gi.linkentity (chunk);
}


/*
QUAKED func_explosive (0 .5 .8) ? Trigger_Spawn ANIMATED ANIMATED_FAST
Any brush that you want to explode or break apart.  If you want an
ex0plosion, set dmg and it will do a radius explosion of that amount
at the center of the bursh.

If targeted it will not be shootable.

health defaults to 100.

mass defaults to 75.  This determines how much debris is emitted when
it explodes.  You get one large chunk per 100 of mass (up to 8) and
one small chunk per 25 of mass (up to 16).  So 800 gives the most.
*/
void NetThrowDebrisNum (vec3_t org, vec3_t size, byte count, bool glass)
{
	bool large_map = LargeMapCheck(org);
	if (large_map)
		gi.WriteByte (svc_temp_entity_ext);
	else
		gi.WriteByte (svc_temp_entity);
	if (glass)
		gi.WriteByte (TE_EXPLOSION_NUM_GLASS);
	else
		gi.WriteByte (TE_EXPLOSION_NUM);
	gi.WritePosition (org, large_map);
	gi.WriteByte (size[0]);
	gi.WriteByte (size[1]);
	gi.WriteByte (size[2]);
	gi.WriteByte (count);
	gi.multicast (org, MULTICAST_PVS);
}


void func_explosive_explode (edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point)
{
	vec3_t	origin;
	vec3_t	chunkorigin;
	vec3_t	size;
	int		count;
	int		mass;
	bool	glass;
	vec3_t	probe;

	VectorAdd(self->absmin, self->absmax, probe);
	VectorScale(probe, 0.5, probe);
	glass = (gi.pointcontents(probe) & CONTENTS_WINDOW);

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

	if(net_compatibility->value || !net_fixoverflow->value)
	{
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
	}
	else
		NetThrowDebrisNum (origin, size, mass/25, glass);

	G_UseTargets (self, attacker, "explosive_explode");

	if (self->dmg)
		BecomeExplosion1 (self);
	else
		G_FreeEdict (self);
}

void func_explosive_use(edict_t *self, edict_t *other, edict_t *activator)
{
	func_explosive_explode (self, self, other, self->health, vec3_origin);
}

void func_explosive_spawn (edict_t *self, edict_t *other, edict_t *activator)
{
	self->solid = SOLID_BSP;
	self->svflags &= ~SVF_NOCLIENT;
	self->use = NULL;
	KillBox (self);
	gi.linkentity (self);
}

void SP_func_explosive (edict_t *self)
{
	if (deathmatch->value)
	{	// auto-remove for deathmatch
		G_FreeEdict (self);
		return;
	}

	self->movetype = MOVETYPE_PUSH;

	gi.modelindex ("models/objects/debris1/tris.md2");
	gi.modelindex ("models/objects/debris2/tris.md2");

	gi.setmodel (self, self->model);

	if (self->spawnflags & 1)
	{
		self->svflags |= SVF_NOCLIENT;
		self->solid = SOLID_NOT;
		self->use = func_explosive_spawn;
	}
	else
	{
		self->solid = SOLID_BSP;
		if (self->targetname)
			self->use = func_explosive_use;
	}

	if (self->spawnflags & 2)
		self->s.effects |= EF_ANIM_ALL;
	if (self->spawnflags & 4)
		self->s.effects |= EF_ANIM_ALLFAST;

	if (self->use != func_explosive_use)
	{
		if (!self->health)
			self->health = 100;
		self->die = func_explosive_explode;
		self->takedamage = DAMAGE_YES;
	}

	gi.linkentity (self);
}


bool Pickup_Weapon (edict_t *ent, edict_t *other)
{
	int			index;
	gitem_t		*ammo;

	index = ITEM_INDEX(ent->item);

	if ( ( ((int)(dmflags->value) & DF_WEAPONS_STAY) || coop->value)
		&& other->client->pers.inventory[index])
	{
		if (!(ent->spawnflags & (DROPPED_ITEM | DROPPED_PLAYER_ITEM) ) )
			return false;	// leave the weapon for others to pickup
	}

	other->client->pers.inventory[index]++;

	if (!(ent->spawnflags & DROPPED_ITEM) )
	{
		// give them some ammo with it
		ammo = FindItem (ent->item->ammo);
		if ( (int)dmflags->value & DF_INFINITE_AMMO )
			Add_Ammo (other, ammo, 1000);
		else
			Add_Ammo (other, ammo, ammo->quantity);

		if (! (ent->spawnflags & DROPPED_PLAYER_ITEM) )
		{
			if (deathmatch->value)
			{
				if ((int)(dmflags->value) & DF_WEAPONS_STAY)
					ent->flags |= FL_RESPAWN;
				else
					SetRespawn (ent, 30);
			}
			if (coop->value)
				ent->flags |= FL_RESPAWN;
		}
	}

	if (other->client->pers.weapon != ent->item &&
		(other->client->pers.inventory[index] == 1) &&
		( !deathmatch->value || other->client->pers.weapon == FindItem("blaster") ) )
		other->client->newweapon = ent->item;

	return true;
}














/*
=========================================================

  PLATS

  movement options:

  linear
  smooth start, hard stop
  smooth start, smooth stop

  start
  end
  acceleration
  speed
  deceleration
  begin sound
  end sound
  target fired when reaching end
  wait at end

  object characteristics that use move segments
  ---------------------------------------------
  movetype_push, or movetype_stop
  action when touched
  action when blocked
  action when used
	disabled?
  auto trigger spawning


=========================================================
*/

#define PLAT_LOW_TRIGGER	1

#define	STATE_TOP			0
#define	STATE_BOTTOM		1
#define STATE_UP			2
#define STATE_DOWN			3

#define DOOR_START_OPEN		1
#define DOOR_REVERSE		2
#define DOOR_CRUSHER		4
#define DOOR_NOMONSTER		8
#define DOOR_TOGGLE			32
#define DOOR_X_AXIS			64
#define DOOR_Y_AXIS			128


//
// Support routines for movement (changes in origin using velocity)
//

void Move_Done (edict_t *ent)
{
	VectorClear (ent->velocity);
	ent->moveinfo.endfunc (ent);
}

void Move_Final (edict_t *ent)
{
	if (ent->moveinfo.remaining_distance == 0)
	{
		Move_Done (ent);
		return;
	}

	VectorScale (ent->moveinfo.dir, ent->moveinfo.remaining_distance / FRAMETIME, ent->velocity);

	ent->think = Move_Done;
	ent->nextthink = level.time + FRAMETIME;
}

void Move_Begin (edict_t *ent)
{
	float	frames;

	if ((ent->moveinfo.speed * FRAMETIME) >= ent->moveinfo.remaining_distance)
	{
		Move_Final (ent);
		return;
	}
	VectorScale (ent->moveinfo.dir, ent->moveinfo.speed, ent->velocity);
	frames = floor((ent->moveinfo.remaining_distance / ent->moveinfo.speed) / FRAMETIME);
	ent->moveinfo.remaining_distance -= frames * ent->moveinfo.speed * FRAMETIME;
	ent->nextthink = level.time + (frames * FRAMETIME);
	ent->think = Move_Final;
}

void Think_AccelMove (edict_t *ent);

void Move_Calc (edict_t *ent, vec3_t dest, void(*func)(edict_t*))
{
	VectorClear (ent->velocity);
	VectorSubtract (dest, ent->s.origin, ent->moveinfo.dir);
	ent->moveinfo.remaining_distance = VectorNormalize (ent->moveinfo.dir);
	ent->moveinfo.endfunc = func;

	if (ent->moveinfo.speed == ent->moveinfo.accel && ent->moveinfo.speed == ent->moveinfo.decel)
	{
		if (level.current_entity == ((ent->flags & FL_TEAMSLAVE) ? ent->teammaster : ent))
		{
			Move_Begin (ent);
		}
		else
		{
			ent->nextthink = level.time + FRAMETIME;
			ent->think = Move_Begin;
		}
	}
	else
	{
		// accelerative
		ent->moveinfo.current_speed = 0;
		ent->think = Think_AccelMove;
		ent->nextthink = level.time + FRAMETIME;
	}
}


//
// Support routines for angular movement (changes in angle using avelocity)
//

void AngleMove_Done (edict_t *ent)
{
	VectorClear (ent->avelocity);
	ent->moveinfo.endfunc (ent);
}

void AngleMove_Final (edict_t *ent)
{
	vec3_t	move;

	if (ent->moveinfo.state == STATE_UP)
		VectorSubtract (ent->moveinfo.end_angles, ent->s.angles, move);
	else
		VectorSubtract (ent->moveinfo.start_angles, ent->s.angles, move);

	if (VectorCompare (move, vec3_origin))
	{
		AngleMove_Done (ent);
		return;
	}

	VectorScale (move, 1.0/FRAMETIME, ent->avelocity);

	ent->think = AngleMove_Done;
	ent->nextthink = level.time + FRAMETIME;
}

void AngleMove_Begin (edict_t *ent)
{
	vec3_t	destdelta;
	float	len;
	float	traveltime;
	float	frames;

	// set destdelta to the vector needed to move
	if (ent->moveinfo.state == STATE_UP)
		VectorSubtract (ent->moveinfo.end_angles, ent->s.angles, destdelta);
	else
		VectorSubtract (ent->moveinfo.start_angles, ent->s.angles, destdelta);

	// calculate length of vector
	len = VectorLength (destdelta);

	// divide by speed to get time to reach dest
	traveltime = len / ent->moveinfo.speed;

	if (traveltime < FRAMETIME)
	{
		AngleMove_Final (ent);
		return;
	}

	frames = floor(traveltime / FRAMETIME);

	// scale the destdelta vector by the time spent traveling to get velocity
	VectorScale (destdelta, 1.0 / traveltime, ent->avelocity);

	// set nextthink to trigger a think when dest is reached
	ent->nextthink = level.time + frames * FRAMETIME;
	ent->think = AngleMove_Final;
}

void AngleMove_Calc (edict_t *ent, void(*func)(edict_t*))
{
	VectorClear (ent->avelocity);
	ent->moveinfo.endfunc = func;
	if (level.current_entity == ((ent->flags & FL_TEAMSLAVE) ? ent->teammaster : ent))
	{
		AngleMove_Begin (ent);
	}
	else
	{
		ent->nextthink = level.time + FRAMETIME;
		ent->think = AngleMove_Begin;
	}
}


/*
==============
Think_AccelMove

The team has completed a frame of movement, so
change the speed for the next frame
==============
*/
#define AccelerationDistance(target, rate)	(target * ((target / rate) + 1) / 2)

void plat_CalcAcceleratedMove(moveinfo_t *moveinfo)
{
	float	accel_dist;
	float	decel_dist;

	moveinfo->move_speed = moveinfo->speed;

	if (moveinfo->remaining_distance < moveinfo->accel)
	{
		moveinfo->current_speed = moveinfo->remaining_distance;
		return;
	}

	accel_dist = AccelerationDistance (moveinfo->speed, moveinfo->accel);
	decel_dist = AccelerationDistance (moveinfo->speed, moveinfo->decel);

	if ((moveinfo->remaining_distance - accel_dist - decel_dist) < 0)
	{
		float	f;

		f = (moveinfo->accel + moveinfo->decel) / (moveinfo->accel * moveinfo->decel);
		moveinfo->move_speed = (-2 + sqrt(4 - 4 * f * (-2 * moveinfo->remaining_distance))) / (2 * f);
		decel_dist = AccelerationDistance (moveinfo->move_speed, moveinfo->decel);
	}

	moveinfo->decel_distance = decel_dist;
};

void plat_Accelerate (moveinfo_t *moveinfo)
{
	// are we decelerating?
	if (moveinfo->remaining_distance <= moveinfo->decel_distance)
	{
		if (moveinfo->remaining_distance < moveinfo->decel_distance)
		{
			if (moveinfo->next_speed)
			{
				moveinfo->current_speed = moveinfo->next_speed;
				moveinfo->next_speed = 0;
				return;
			}
			if (moveinfo->current_speed > moveinfo->decel)
				moveinfo->current_speed -= moveinfo->decel;
		}
		return;
	}

	// are we at full speed and need to start decelerating during this move?
	if (moveinfo->current_speed == moveinfo->move_speed)
		if ((moveinfo->remaining_distance - moveinfo->current_speed) < moveinfo->decel_distance)
		{
			float	p1_distance;
			float	p2_distance;
			float	distance;

			p1_distance = moveinfo->remaining_distance - moveinfo->decel_distance;
			p2_distance = moveinfo->move_speed * (1.0 - (p1_distance / moveinfo->move_speed));
			distance = p1_distance + p2_distance;
			moveinfo->current_speed = moveinfo->move_speed;
			moveinfo->next_speed = moveinfo->move_speed - moveinfo->decel * (p2_distance / distance);
			return;
		}

	// are we accelerating?
	if (moveinfo->current_speed < moveinfo->speed)
	{
		float	old_speed;
		float	p1_distance;
		float	p1_speed;
		float	p2_distance;
		float	distance;

		old_speed = moveinfo->current_speed;

		// figure simple acceleration up to move_speed
		moveinfo->current_speed += moveinfo->accel;
		if (moveinfo->current_speed > moveinfo->speed)
			moveinfo->current_speed = moveinfo->speed;

		// are we accelerating throughout this entire move?
		if ((moveinfo->remaining_distance - moveinfo->current_speed) >= moveinfo->decel_distance)
			return;

		// during this move we will accelrate from current_speed to move_speed
		// and cross over the decel_distance; figure the average speed for the
		// entire move
		p1_distance = moveinfo->remaining_distance - moveinfo->decel_distance;
		p1_speed = (old_speed + moveinfo->move_speed) / 2.0;
		p2_distance = moveinfo->move_speed * (1.0 - (p1_distance / p1_speed));
		distance = p1_distance + p2_distance;
		moveinfo->current_speed = (p1_speed * (p1_distance / distance)) + (moveinfo->move_speed * (p2_distance / distance));
		moveinfo->next_speed = moveinfo->move_speed - moveinfo->decel * (p2_distance / distance);
		return;
	}

	// we are at constant velocity (move_speed)
	return;
};

void Think_AccelMove (edict_t *ent)
{
	ent->moveinfo.remaining_distance -= ent->moveinfo.current_speed;

	if (ent->moveinfo.current_speed == 0)		// starting or blocked
		plat_CalcAcceleratedMove(&ent->moveinfo);

	plat_Accelerate (&ent->moveinfo);

	// will the entire move complete on next frame?
	if (ent->moveinfo.remaining_distance <= ent->moveinfo.current_speed)
	{
		Move_Final (ent);
		return;
	}

	VectorScale (ent->moveinfo.dir, ent->moveinfo.current_speed*10, ent->velocity);
	ent->nextthink = level.time + FRAMETIME;
	ent->think = Think_AccelMove;
}


void plat_go_down (edict_t *ent);

void plat_hit_top (edict_t *ent)
{
	if (!(ent->flags & FL_TEAMSLAVE))
	{
		if (ent->moveinfo.sound_end)
			gi.sound (ent, CHAN_NO_PHS_ADD+CHAN_VOICE, ent->moveinfo.sound_end, 1, ATTN_STATIC, 0);
		ent->s.sound = 0;
	}
	ent->moveinfo.state = STATE_TOP;

	ent->think = plat_go_down;
	ent->nextthink = level.time + 3;
}

void plat_hit_bottom (edict_t *ent)
{
	if (!(ent->flags & FL_TEAMSLAVE))
	{
		if (ent->moveinfo.sound_end)
			gi.sound (ent, CHAN_NO_PHS_ADD+CHAN_VOICE, ent->moveinfo.sound_end, 1, ATTN_STATIC, 0);
		ent->s.sound = 0;
	}
	ent->moveinfo.state = STATE_BOTTOM;
}

void plat_go_down (edict_t *ent)
{
	if (!(ent->flags & FL_TEAMSLAVE))
	{
		if (ent->moveinfo.sound_start)
			gi.sound (ent, CHAN_NO_PHS_ADD+CHAN_VOICE, ent->moveinfo.sound_start, 1, ATTN_STATIC, 0);
		ent->s.sound = ent->moveinfo.sound_middle;
	}
	ent->moveinfo.state = STATE_DOWN;
	Move_Calc (ent, ent->moveinfo.end_origin, plat_hit_bottom);
}

void plat_go_up (edict_t *ent)
{
	if (!(ent->flags & FL_TEAMSLAVE))
	{
		if (ent->moveinfo.sound_start)
			gi.sound (ent, CHAN_NO_PHS_ADD+CHAN_VOICE, ent->moveinfo.sound_start, 1, ATTN_STATIC, 0);
		ent->s.sound = ent->moveinfo.sound_middle;
	}
	ent->moveinfo.state = STATE_UP;
	Move_Calc (ent, ent->moveinfo.start_origin, plat_hit_top);
}

void BecomeExplosion1 (edict_t *self)
{
	bool large_map = LargeMapCheck(self->s.origin);
	if (large_map)
		gi.WriteByte (svc_temp_entity_ext);
	else
		gi.WriteByte (svc_temp_entity);
	gi.WriteByte (TE_EXPLOSION1);
	gi.WritePosition (self->s.origin, large_map);
////gi.WriteDir (vec3_origin);		/// FIXME: [0,0,1] ???
	gi.multicast (NULL/*self->s.origin*/, MULTICAST_ALL/*PVS*/);	// О долгоживущих пятнах от взрыва должны "знать" все

	G_FreeEdict (self);
}


void BecomeExplosion2 (edict_t *self)
{
	bool large_map = LargeMapCheck(self->s.origin);
	if (large_map)
		gi.WriteByte (svc_temp_entity_ext);
	else
		gi.WriteByte (svc_temp_entity);
	gi.WriteByte (TE_EXPLOSION2);
	gi.WritePosition (self->s.origin, large_map);
////gi.WriteDir (vec3_origin);		/// FIXME: [0,0,1] ???
	gi.multicast (NULL/*self->s.origin*/, MULTICAST_ALL/*PVS*/);	// О долгоживущих пятнах от взрыва должны "знать" все

	G_FreeEdict (self);
}


void plat_blocked (edict_t *self, edict_t *other)
{
	if (!(other->svflags & SVF_MONSTER) && (!other->client) )
	{
		// give it a chance to go away on it's own terms (like gibs)
		T_Damage (other, self, self, vec3_origin, other->s.origin, vec3_origin, 100000, 1, 0, MOD_CRUSH);
		// if it's still there, nuke it
		if (other)
			BecomeExplosion1 (other);
		return;
	}

	T_Damage (other, self, self, vec3_origin, other->s.origin, vec3_origin, self->dmg, 1, 0, MOD_CRUSH);

	if (self->moveinfo.state == STATE_UP)
		plat_go_down (self);
	else if (self->moveinfo.state == STATE_DOWN)
		plat_go_up (self);
}


void Use_Plat (edict_t *ent, edict_t *other, edict_t *activator)
{
	if (ent->think)
		return;		// already down
	plat_go_down (ent);
}


void Touch_Plat_Center (edict_t *ent, edict_t *other, cplane_t *plane, csurface_t *surf)
{
	if (!other->client)
		return;

	if (other->health <= 0)
		return;

	ent = ent->enemy;	// now point at the plat, not the trigger
	if (ent->moveinfo.state == STATE_BOTTOM)
		plat_go_up (ent);
	else if (ent->moveinfo.state == STATE_TOP)
		ent->nextthink = level.time + 1;	// the player is still on the plat, so delay going down
}

void plat_spawn_inside_trigger (edict_t *ent)
{
	edict_t	*trigger;
	vec3_t	tmin, tmax;

//
// middle trigger
//
	trigger = G_Spawn();
	trigger->touch = Touch_Plat_Center;
	trigger->movetype = MOVETYPE_NONE;
	trigger->solid = SOLID_TRIGGER;
	trigger->enemy = ent;

	tmin[0] = ent->mins[0] + 25;
	tmin[1] = ent->mins[1] + 25;
	tmin[2] = ent->mins[2];

	tmax[0] = ent->maxs[0] - 25;
	tmax[1] = ent->maxs[1] - 25;
	tmax[2] = ent->maxs[2] + 8;

	tmin[2] = tmax[2] - (ent->pos1[2] - ent->pos2[2] + st.lip);

	if (ent->spawnflags & PLAT_LOW_TRIGGER)
		tmax[2] = tmin[2] + 8;

	if (tmax[0] - tmin[0] <= 0)
	{
		tmin[0] = (ent->mins[0] + ent->maxs[0]) *0.5;
		tmax[0] = tmin[0] + 1;
	}
	if (tmax[1] - tmin[1] <= 0)
	{
		tmin[1] = (ent->mins[1] + ent->maxs[1]) *0.5;
		tmax[1] = tmin[1] + 1;
	}

	VectorCopy (tmin, trigger->mins);
	VectorCopy (tmax, trigger->maxs);

	gi.linkentity (trigger);
}


/*QUAKED func_plat (0 .5 .8) ? PLAT_LOW_TRIGGER
speed	default 150

Plats are always drawn in the extended position, so they will light correctly.

If the plat is the target of another trigger or button, it will start out disabled in the extended position until it is trigger, when it will lower and become a normal plat.

"speed"	overrides default 200.
"accel" overrides default 500
"lip"	overrides default 8 pixel lip

If the "height" key is set, that will determine the amount the plat moves, instead of being implicitly determoveinfoned by the model's height.

Set "sounds" to one of the following:
1) base fast
2) chain slow
*/
void SP_func_plat (edict_t *ent)
{
	VectorClear (ent->s.angles);
	ent->solid = SOLID_BSP;
	ent->movetype = MOVETYPE_PUSH;

	gi.setmodel (ent, ent->model);

	ent->blocked = plat_blocked;

	if (!ent->speed)
		ent->speed = 20;
	else
		ent->speed *= 0.1;

	if (!ent->accel)
		ent->accel = 5;
	else
		ent->accel *= 0.1;

	if (!ent->decel)
		ent->decel = 5;
	else
		ent->decel *= 0.1;

	if (!ent->dmg)
		ent->dmg = 2;

	if (!st.lip)
		st.lip = 8;

	// pos1 is the top position, pos2 is the bottom
	VectorCopy (ent->s.origin, ent->pos1);
	VectorCopy (ent->s.origin, ent->pos2);
	if (st.height)
		ent->pos2[2] -= st.height;
	else
		ent->pos2[2] -= (ent->maxs[2] - ent->mins[2]) - st.lip;

	ent->use = Use_Plat;

	plat_spawn_inside_trigger (ent);	// the "start moving" trigger

	if (ent->targetname)
	{
		ent->moveinfo.state = STATE_UP;
	}
	else
	{
		VectorCopy (ent->pos2, ent->s.origin);
		gi.linkentity (ent);
		ent->moveinfo.state = STATE_BOTTOM;
	}

	ent->moveinfo.speed = ent->speed;
	ent->moveinfo.accel = ent->accel;
	ent->moveinfo.decel = ent->decel;
	ent->moveinfo.wait = ent->wait;
	VectorCopy (ent->pos1, ent->moveinfo.start_origin);
	VectorCopy (ent->s.angles, ent->moveinfo.start_angles);
	VectorCopy (ent->pos2, ent->moveinfo.end_origin);
	VectorCopy (ent->s.angles, ent->moveinfo.end_angles);

	ent->moveinfo.sound_start = gi.soundindex ("plats/pt1_strt.wav");
	ent->moveinfo.sound_middle = gi.soundindex ("plats/pt1_mid.wav");
	ent->moveinfo.sound_end = gi.soundindex ("plats/pt1_end.wav");
}

//====================================================================

/*QUAKED func_rotating (0 .5 .8) ? START_ON REVERSE X_AXIS Y_AXIS TOUCH_PAIN STOP ANIMATED ANIMATED_FAST
You need to have an origin brush as part of this entity.  The center of that brush will be
the point around which it is rotated. It will rotate around the Z axis by default.  You can
check either the X_AXIS or Y_AXIS box to change that.

"speed" determines how fast it moves; default value is 100.
"dmg"	damage to inflict when blocked (2 default)

REVERSE will cause the it to rotate in the opposite direction.
STOP mean it will stop moving instead of pushing entities
*/

void rotating_blocked (edict_t *self, edict_t *other)
{
	T_Damage (other, self, self, vec3_origin, other->s.origin, vec3_origin, self->dmg, 1, 0, MOD_CRUSH);
}

void rotating_touch (edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf)
{
	if (self->avelocity[0] || self->avelocity[1] || self->avelocity[2])
		T_Damage (other, self, self, vec3_origin, other->s.origin, vec3_origin, self->dmg, 1, 0, MOD_CRUSH);
}

void rotating_use (edict_t *self, edict_t *other, edict_t *activator)
{
	if (!VectorCompare (self->avelocity, vec3_origin))
	{
		self->s.sound = 0;
		VectorClear (self->avelocity);
		self->touch = NULL;
	}
	else
	{
		self->s.sound = self->moveinfo.sound_middle;
		VectorScale (self->movedir, self->speed, self->avelocity);
		if (self->spawnflags & 16)
			self->touch = rotating_touch;
	}
}

void SP_func_rotating (edict_t *ent)
{
	ent->solid = SOLID_BSP;
	if (ent->spawnflags & 32)
		ent->movetype = MOVETYPE_STOP;
	else
		ent->movetype = MOVETYPE_PUSH;

	// set the axis of rotation
	VectorClear(ent->movedir);
	if (ent->spawnflags & 4)
		ent->movedir[2] = 1.0;
	else if (ent->spawnflags & 8)
		ent->movedir[0] = 1.0;
	else // Z_AXIS
		ent->movedir[1] = 1.0;

	// check for reverse rotation
	if (ent->spawnflags & 2)
		VectorNegate (ent->movedir, ent->movedir);

	if (!ent->speed)
		ent->speed = 100;
	if (!ent->dmg)
		ent->dmg = 2;

//	ent->moveinfo.sound_middle = "doors/hydro1.wav";

	ent->use = rotating_use;
	if (ent->dmg)
		ent->blocked = rotating_blocked;

	if (ent->spawnflags & 1)
		ent->use (ent, NULL, NULL);

	if (ent->spawnflags & 64)
		ent->s.effects |= EF_ANIM_ALL;
	if (ent->spawnflags & 128)
		ent->s.effects |= EF_ANIM_ALLFAST;

	gi.setmodel (ent, ent->model);
	gi.linkentity (ent);
}



/*
======================================================================

BUTTONS

======================================================================
*/

/*QUAKED func_button (0 .5 .8) ?
When a button is touched, it moves some distance in the direction of it's angle, triggers all of it's targets, waits some time, then returns to it's original position where it can be triggered again.

"angle"		determines the opening direction
"target"	all entities with a matching targetname will be used
"speed"		override the default 40 speed
"wait"		override the default 1 second wait (-1 = never return)
"lip"		override the default 4 pixel lip remaining at end of move
"health"	if set, the button must be killed instead of touched
"sounds"
1) silent
2) steam metal
3) wooden clunk
4) metallic click
5) in-out
*/

vec3_t VEC_UP		= {0, -1, 0};
vec3_t MOVEDIR_UP	= {0, 0, 1};
vec3_t VEC_DOWN		= {0, -2, 0};
vec3_t MOVEDIR_DOWN	= {0, 0, -1};

void G_SetMovedir (vec3_t angles, vec3_t movedir)
{
	if (VectorCompare (angles, VEC_UP))
	{
		VectorCopy (MOVEDIR_UP, movedir);
	}
	else if (VectorCompare (angles, VEC_DOWN))
	{
		VectorCopy (MOVEDIR_DOWN, movedir);
	}
	else
	{
		AngleVectors (angles, movedir, NULL, NULL);
	}

	VectorClear (angles);
}


void button_done (edict_t *self)
{
	self->moveinfo.state = STATE_BOTTOM;
	self->s.effects &= ~EF_ANIM23;
	self->s.effects |= EF_ANIM01;
}

void button_return (edict_t *self)
{
	self->moveinfo.state = STATE_DOWN;

	Move_Calc (self, self->moveinfo.start_origin, button_done);

	self->s.frame = 0;

	if (self->health)
		self->takedamage = DAMAGE_YES;
}

void button_wait (edict_t *self)
{
	self->moveinfo.state = STATE_TOP;
	self->s.effects &= ~EF_ANIM01;
	self->s.effects |= EF_ANIM23;

	G_UseTargets (self, self->activator, "button");
	self->s.frame = 1;
	if (self->moveinfo.wait >= 0)
	{
		self->nextthink = level.time + self->moveinfo.wait;
		self->think = button_return;
	}
}

void button_fire (edict_t *self)
{
	if (self->moveinfo.state == STATE_UP || self->moveinfo.state == STATE_TOP)
		return;

	self->moveinfo.state = STATE_UP;
	if (self->moveinfo.sound_start && !(self->flags & FL_TEAMSLAVE))
		gi.sound (self, CHAN_NO_PHS_ADD+CHAN_VOICE, self->moveinfo.sound_start, 1, ATTN_STATIC, 0);
	Move_Calc (self, self->moveinfo.end_origin, button_wait);
}


void button_use (edict_t *self, edict_t *other, edict_t *activator)
{
	self->activator = activator;
	button_fire (self);
}


void button_touch (edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf)
{
	if (!other->client)
		return;

	if (other->health <= 0)
		return;

	self->activator = other;
	button_fire (self);
}


void button_killed (edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point)
{
	self->activator = attacker;
	self->health = self->max_health;
	self->takedamage = DAMAGE_NO;
	button_fire (self);
}


void SP_func_button (edict_t *ent)
{
	vec3_t	abs_movedir;
	float	dist;

	G_SetMovedir (ent->s.angles, ent->movedir);
	ent->movetype = MOVETYPE_STOP;
	ent->solid = SOLID_BSP;
	gi.setmodel (ent, ent->model);

	if (ent->sounds != 1)
		ent->moveinfo.sound_start = gi.soundindex ("switches/butn2.wav");

	if (!ent->speed)
		ent->speed = 40;
	if (!ent->accel)
		ent->accel = ent->speed;
	if (!ent->decel)
		ent->decel = ent->speed;

	if (!ent->wait)
		ent->wait = 3;
	if (!st.lip)
		st.lip = 4;

	VectorCopy (ent->s.origin, ent->pos1);
	abs_movedir[0] = fabs(ent->movedir[0]);
	abs_movedir[1] = fabs(ent->movedir[1]);
	abs_movedir[2] = fabs(ent->movedir[2]);
	dist = abs_movedir[0] * ent->size[0] + abs_movedir[1] * ent->size[1] + abs_movedir[2] * ent->size[2] - st.lip;
	VectorMA (ent->pos1, dist, ent->movedir, ent->pos2);

	ent->use = button_use;
	ent->s.effects |= EF_ANIM01;

	if (ent->health)
	{
		ent->max_health = ent->health;
		ent->die = button_killed;
		ent->takedamage = DAMAGE_YES;
	}
	else if (! ent->targetname)
		ent->touch = button_touch;

	ent->moveinfo.state = STATE_BOTTOM;

	ent->moveinfo.speed = ent->speed;
	ent->moveinfo.accel = ent->accel;
	ent->moveinfo.decel = ent->decel;
	ent->moveinfo.wait = ent->wait;
	VectorCopy (ent->pos1, ent->moveinfo.start_origin);
	VectorCopy (ent->s.angles, ent->moveinfo.start_angles);
	VectorCopy (ent->pos2, ent->moveinfo.end_origin);
	VectorCopy (ent->s.angles, ent->moveinfo.end_angles);

	gi.linkentity (ent);
}

/*
======================================================================

DOORS

  spawn a trigger surrounding the entire team unless it is
  already targeted by another

======================================================================
*/

/*QUAKED func_door (0 .5 .8) ? START_OPEN x CRUSHER NOMONSTER ANIMATED TOGGLE ANIMATED_FAST
TOGGLE		wait in both the start and end states for a trigger event.
START_OPEN	the door to moves to its destination when spawned, and operate in reverse.  It is used to temporarily or permanently close off an area when triggered (not useful for touch or takedamage doors).
NOMONSTER	monsters will not trigger this door

"message"	is printed when the door is touched if it is a trigger door and it hasn't been fired yet
"angle"		determines the opening direction
"targetname" if set, no touch field will be spawned and a remote button or trigger field activates the door.
"health"	if set, door must be shot open
"speed"		movement speed (100 default)
"wait"		wait before returning (3 default, -1 = never return)
"lip"		lip remaining at end of move (8 default)
"dmg"		damage to inflict when blocked (2 default)
"sounds"
1)	silent
2)	light
3)	medium
4)	heavy
*/

void door_use_areaportals (edict_t *self, bool open)
{
	edict_t	*t = NULL;

	if (!self->target)
		return;

	while ((t = G_Find (t, FOFS(targetname), self->target, "door_use_areaportals")))
		if (Q_strcasecmp(t->classname, "func_areaportal") == 0)
			gi.SetAreaPortalState (t->style, open);	// Передадим информацию серверу
}

void door_go_down (edict_t *self);

void door_hit_top (edict_t *self)
{
	/// Berserker: Срабатывает в момент окончания открытия двери
	if(self->spawnflags & DOOR_FIRING_ON_END_OPENING)
		G_UseTargets (self, NULL, "door_end_opening");

	if (!(self->flags & FL_TEAMSLAVE))
	{
		if (self->moveinfo.sound_end)
			gi.sound (self, CHAN_NO_PHS_ADD+CHAN_VOICE, self->moveinfo.sound_end, 1, ATTN_STATIC, 0);
		self->s.sound = 0;
	}
	self->moveinfo.state = STATE_TOP;
	if (self->spawnflags & DOOR_TOGGLE)
		return;
	if (self->moveinfo.wait >= 0)
	{
		self->think = door_go_down;
		self->nextthink = level.time + self->moveinfo.wait;
	}
}

void door_hit_bottom (edict_t *self)
{
	/// Berserker: Срабатывает в момент окончания закрытия двери
	if(self->spawnflags & DOOR_FIRING_ON_END_CLOSING)
		G_UseTargets (self, NULL, "door_end_closing");

	if (!(self->flags & FL_TEAMSLAVE))
	{
		if (self->moveinfo.sound_end)
			gi.sound (self, CHAN_NO_PHS_ADD+CHAN_VOICE, self->moveinfo.sound_end, 1, ATTN_STATIC, 0);
		self->s.sound = 0;
	}
	self->moveinfo.state = STATE_BOTTOM;
	door_use_areaportals (self, false);
}

void door_go_down (edict_t *self)
{
	/// Berserker: Срабатывает в момент начала закрытия двери
	if(self->spawnflags & DOOR_FIRING_ON_START_CLOSING)
		G_UseTargets (self, NULL, "door_start_closing");

	if (!(self->flags & FL_TEAMSLAVE))
	{
		if (self->moveinfo.sound_start)
			gi.sound (self, CHAN_NO_PHS_ADD+CHAN_VOICE, self->moveinfo.sound_start, 1, ATTN_STATIC, 0);
		self->s.sound = self->moveinfo.sound_middle;
	}
	if (self->max_health)
	{
		self->takedamage = DAMAGE_YES;
		self->health = self->max_health;
	}

	self->moveinfo.state = STATE_DOWN;
	if (strcmp(self->classname, "func_door") == 0)
		Move_Calc (self, self->moveinfo.start_origin, door_hit_bottom);
	else if (strcmp(self->classname, "func_door_rotating") == 0)
		AngleMove_Calc (self, door_hit_bottom);
}

void door_go_up (edict_t *self, edict_t *activator)
{
	if (self->moveinfo.state == STATE_UP)
		return;		// already going up

	if (self->moveinfo.state == STATE_TOP)
	{	// reset top wait time
		if (self->moveinfo.wait >= 0)
			self->nextthink = level.time + self->moveinfo.wait;
		return;
	}

	if (!(self->flags & FL_TEAMSLAVE))
	{
		if (self->moveinfo.sound_start)
			gi.sound (self, CHAN_NO_PHS_ADD+CHAN_VOICE, self->moveinfo.sound_start, 1, ATTN_STATIC, 0);
		self->s.sound = self->moveinfo.sound_middle;
	}
	self->moveinfo.state = STATE_UP;
	if (strcmp(self->classname, "func_door") == 0)
		Move_Calc (self, self->moveinfo.end_origin, door_hit_top);
	else if (strcmp(self->classname, "func_door_rotating") == 0)
		AngleMove_Calc (self, door_hit_top);

	/// Berserker: Срабатывает в момент начала открытия двери
	if(!(self->spawnflags & DOOR_NO_FIRING_ON_START_OPENING))
		G_UseTargets (self, activator, "door_start_opening");

	door_use_areaportals (self, true);
}

void door_use (edict_t *self, edict_t *other, edict_t *activator)
{
	edict_t	*ent;

	if (self->flags & FL_TEAMSLAVE)
		return;

	if (self->spawnflags & DOOR_TOGGLE)
	{
		if (self->moveinfo.state == STATE_UP || self->moveinfo.state == STATE_TOP)
		{
			// trigger all paired doors
			for (ent = self ; ent ; ent = ent->teamchain)
			{
				ent->message = NULL;
				ent->touch = NULL;
				door_go_down (ent);
			}
			return;
		}
	}

	// trigger all paired doors
	for (ent = self ; ent ; ent = ent->teamchain)
	{
		ent->message = NULL;
		ent->touch = NULL;
		door_go_up (ent, activator);
	}
};

void Touch_DoorTrigger (edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf)
{
	if (other->health <= 0)
		return;

	if (!(other->svflags & SVF_MONSTER) && (!other->client))
		return;

	if ((self->owner->spawnflags & DOOR_NOMONSTER) && (other->svflags & SVF_MONSTER))
		return;

	if (level.time < self->touch_debounce_time)
		return;
	self->touch_debounce_time = level.time + 1.0;

	door_use (self->owner, other, other);
}

void Think_CalcMoveSpeed (edict_t *self)
{
	edict_t	*ent;
	float	min;
	float	time;
	float	newspeed;
	float	ratio;
	float	dist;

	if (self->flags & FL_TEAMSLAVE)
		return;		// only the team master does this

	// find the smallest distance any member of the team will be moving
	min = fabs(self->moveinfo.distance);
	for (ent = self->teamchain; ent; ent = ent->teamchain)
	{
		dist = fabs(ent->moveinfo.distance);
		if (dist < min)
			min = dist;
	}

	time = min / self->moveinfo.speed;

	// adjust speeds so they will all complete at the same time
	for (ent = self; ent; ent = ent->teamchain)
	{
		newspeed = fabs(ent->moveinfo.distance) / time;
		ratio = newspeed / ent->moveinfo.speed;
		if (ent->moveinfo.accel == ent->moveinfo.speed)
			ent->moveinfo.accel = newspeed;
		else
			ent->moveinfo.accel *= ratio;
		if (ent->moveinfo.decel == ent->moveinfo.speed)
			ent->moveinfo.decel = newspeed;
		else
			ent->moveinfo.decel *= ratio;
		ent->moveinfo.speed = newspeed;
	}
}

void Think_SpawnDoorTrigger (edict_t *ent)
{
	edict_t		*other;
	vec3_t		mins, maxs;

	if (ent->flags & FL_TEAMSLAVE)
		return;		// only the team leader spawns a trigger

	VectorCopy (ent->absmin, mins);
	VectorCopy (ent->absmax, maxs);

	for (other = ent->teamchain ; other ; other=other->teamchain)
	{
		AddPointToBounds (other->absmin, mins, maxs);
		AddPointToBounds (other->absmax, mins, maxs);
	}

	// expand
	mins[0] -= 60;
	mins[1] -= 60;
	maxs[0] += 60;
	maxs[1] += 60;

	other = G_Spawn ();
	VectorCopy (mins, other->mins);
	VectorCopy (maxs, other->maxs);
	other->owner = ent;
	other->solid = SOLID_TRIGGER;
	other->movetype = MOVETYPE_NONE;
	other->touch = Touch_DoorTrigger;
	gi.linkentity (other);

	if (ent->spawnflags & DOOR_START_OPEN)
		door_use_areaportals (ent, true);

	Think_CalcMoveSpeed (ent);
}

void door_blocked  (edict_t *self, edict_t *other)
{
	edict_t	*ent;

	if (!(other->svflags & SVF_MONSTER) && (!other->client) )
	{
		// give it a chance to go away on it's own terms (like gibs)
		T_Damage (other, self, self, vec3_origin, other->s.origin, vec3_origin, 100000, 1, 0, MOD_CRUSH);
		// if it's still there, nuke it
		if (other)
			BecomeExplosion1 (other);
		return;
	}

	T_Damage (other, self, self, vec3_origin, other->s.origin, vec3_origin, self->dmg, 1, 0, MOD_CRUSH);

	if (self->spawnflags & DOOR_CRUSHER)
		return;


// if a door has a negative wait, it would never come back if blocked,
// so let it just squash the object to death real fast
	if (self->moveinfo.wait >= 0)
	{
		if (self->moveinfo.state == STATE_DOWN)
		{
			for (ent = self->teammaster ; ent ; ent = ent->teamchain)
				door_go_up (ent, ent->activator);
		}
		else
		{
			for (ent = self->teammaster ; ent ; ent = ent->teamchain)
				door_go_down (ent);
		}
	}
}

void door_killed (edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point)
{
	edict_t	*ent;

	for (ent = self->teammaster ; ent ; ent = ent->teamchain)
	{
		ent->health = ent->max_health;
		ent->takedamage = DAMAGE_NO;
	}
	door_use (self->teammaster, attacker, attacker);
}

void door_touch (edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf)
{
	if (!other->client)
		return;

	if (level.time < self->touch_debounce_time)
		return;
	self->touch_debounce_time = level.time + 5.0;

	gi.centerprintf (other, "%s", self->message);
	gi.sound (other, CHAN_AUTO, gi.soundindex ("misc/talk1.wav"), 1, ATTN_NORM, 0);
}

void SP_func_door (edict_t *ent)
{
	vec3_t	abs_movedir;

	if (ent->sounds != 1)
	{
		ent->moveinfo.sound_start = gi.soundindex  ("doors/dr1_strt.wav");
		ent->moveinfo.sound_middle = gi.soundindex  ("doors/dr1_mid.wav");
		ent->moveinfo.sound_end = gi.soundindex  ("doors/dr1_end.wav");
	}

	G_SetMovedir (ent->s.angles, ent->movedir);
	ent->movetype = MOVETYPE_PUSH;
	ent->solid = SOLID_BSP;
	gi.setmodel (ent, ent->model);

	ent->blocked = door_blocked;
	ent->use = door_use;

	if (!ent->speed)
		ent->speed = 100;
	if (deathmatch->value)
		ent->speed *= 2;

	if (!ent->accel)
		ent->accel = ent->speed;
	if (!ent->decel)
		ent->decel = ent->speed;

	if (!ent->wait)
		ent->wait = 3;
	if (!st.lip)
		st.lip = 8;
	if (!ent->dmg)
		ent->dmg = 2;

	// calculate second position
	VectorCopy (ent->s.origin, ent->pos1);
	abs_movedir[0] = fabs(ent->movedir[0]);
	abs_movedir[1] = fabs(ent->movedir[1]);
	abs_movedir[2] = fabs(ent->movedir[2]);
	ent->moveinfo.distance = abs_movedir[0] * ent->size[0] + abs_movedir[1] * ent->size[1] + abs_movedir[2] * ent->size[2] - st.lip;
	VectorMA (ent->pos1, ent->moveinfo.distance, ent->movedir, ent->pos2);

	// if it starts open, switch the positions
	if (ent->spawnflags & DOOR_START_OPEN)
	{
		VectorCopy (ent->pos2, ent->s.origin);
		VectorCopy (ent->pos1, ent->pos2);
		VectorCopy (ent->s.origin, ent->pos1);
	}

	ent->moveinfo.state = STATE_BOTTOM;

	if (ent->health)
	{
		ent->takedamage = DAMAGE_YES;
		ent->die = door_killed;
		ent->max_health = ent->health;
	}
	else if (ent->targetname && ent->message)
	{
		gi.soundindex ("misc/talk.wav");
		ent->touch = door_touch;
	}

	ent->moveinfo.speed = ent->speed;
	ent->moveinfo.accel = ent->accel;
	ent->moveinfo.decel = ent->decel;
	ent->moveinfo.wait = ent->wait;
	VectorCopy (ent->pos1, ent->moveinfo.start_origin);
	VectorCopy (ent->s.angles, ent->moveinfo.start_angles);
	VectorCopy (ent->pos2, ent->moveinfo.end_origin);
	VectorCopy (ent->s.angles, ent->moveinfo.end_angles);

	if (ent->spawnflags & 16)
		ent->s.effects |= EF_ANIM_ALL;
	if (ent->spawnflags & 64)
		ent->s.effects |= EF_ANIM_ALLFAST;

	// to simplify logic elsewhere, make non-teamed doors into a team of one
	if (!ent->team)
		ent->teammaster = ent;

	gi.linkentity (ent);

	ent->nextthink = level.time + FRAMETIME;
	if (ent->health || ent->targetname)
		ent->think = Think_CalcMoveSpeed;
	else
		ent->think = Think_SpawnDoorTrigger;
}


/*QUAKED func_door_rotating (0 .5 .8) ? START_OPEN REVERSE CRUSHER NOMONSTER ANIMATED TOGGLE X_AXIS Y_AXIS
TOGGLE causes the door to wait in both the start and end states for a trigger event.

START_OPEN	the door to moves to its destination when spawned, and operate in reverse.  It is used to temporarily or permanently close off an area when triggered (not useful for touch or takedamage doors).
NOMONSTER	monsters will not trigger this door

You need to have an origin brush as part of this entity.  The center of that brush will be
the point around which it is rotated. It will rotate around the Z axis by default.  You can
check either the X_AXIS or Y_AXIS box to change that.

"distance" is how many degrees the door will be rotated.
"speed" determines how fast the door moves; default value is 100.

REVERSE will cause the door to rotate in the opposite direction.

"message"	is printed when the door is touched if it is a trigger door and it hasn't been fired yet
"angle"		determines the opening direction
"targetname" if set, no touch field will be spawned and a remote button or trigger field activates the door.
"health"	if set, door must be shot open
"speed"		movement speed (100 default)
"wait"		wait before returning (3 default, -1 = never return)
"dmg"		damage to inflict when blocked (2 default)
"sounds"
1)	silent
2)	light
3)	medium
4)	heavy
*/

void SP_func_door_rotating (edict_t *ent)
{
	VectorClear (ent->s.angles);

	// set the axis of rotation
	VectorClear(ent->movedir);
	if (ent->spawnflags & DOOR_X_AXIS)
		ent->movedir[2] = 1.0;
	else if (ent->spawnflags & DOOR_Y_AXIS)
		ent->movedir[0] = 1.0;
	else // Z_AXIS
		ent->movedir[1] = 1.0;

	// check for reverse rotation
	if (ent->spawnflags & DOOR_REVERSE)
		VectorNegate (ent->movedir, ent->movedir);

	if (!st.distance)
	{
		gi.dprintf("%s at %s with no distance set\n", ent->classname, vtos(ent->s.origin));
		st.distance = 90;
	}

	VectorCopy (ent->s.angles, ent->pos1);
	VectorMA (ent->s.angles, st.distance, ent->movedir, ent->pos2);
	ent->moveinfo.distance = st.distance;

	ent->movetype = MOVETYPE_PUSH;
	ent->solid = SOLID_BSP;
	gi.setmodel (ent, ent->model);

	ent->blocked = door_blocked;
	ent->use = door_use;

	if (!ent->speed)
		ent->speed = 100;
	if (!ent->accel)
		ent->accel = ent->speed;
	if (!ent->decel)
		ent->decel = ent->speed;

	if (!ent->wait)
		ent->wait = 3;
	if (!ent->dmg)
		ent->dmg = 2;

	if (ent->sounds != 1)
	{
		ent->moveinfo.sound_start = gi.soundindex  ("doors/dr1_strt.wav");
		ent->moveinfo.sound_middle = gi.soundindex  ("doors/dr1_mid.wav");
		ent->moveinfo.sound_end = gi.soundindex  ("doors/dr1_end.wav");
	}

	// if it starts open, switch the positions
	if (ent->spawnflags & DOOR_START_OPEN)
	{
		VectorCopy (ent->pos2, ent->s.angles);
		VectorCopy (ent->pos1, ent->pos2);
		VectorCopy (ent->s.angles, ent->pos1);
		VectorNegate (ent->movedir, ent->movedir);
	}

	if (ent->health)
	{
		ent->takedamage = DAMAGE_YES;
		ent->die = door_killed;
		ent->max_health = ent->health;
	}

	if (ent->targetname && ent->message)
	{
		gi.soundindex ("misc/talk.wav");
		ent->touch = door_touch;
	}

	ent->moveinfo.state = STATE_BOTTOM;
	ent->moveinfo.speed = ent->speed;
	ent->moveinfo.accel = ent->accel;
	ent->moveinfo.decel = ent->decel;
	ent->moveinfo.wait = ent->wait;
	VectorCopy (ent->s.origin, ent->moveinfo.start_origin);
	VectorCopy (ent->pos1, ent->moveinfo.start_angles);
	VectorCopy (ent->s.origin, ent->moveinfo.end_origin);
	VectorCopy (ent->pos2, ent->moveinfo.end_angles);

	if (ent->spawnflags & 16)
		ent->s.effects |= EF_ANIM_ALL;

	// to simplify logic elsewhere, make non-teamed doors into a team of one
	if (!ent->team)
		ent->teammaster = ent;

	gi.linkentity (ent);

	ent->nextthink = level.time + FRAMETIME;
	if (ent->health || ent->targetname)
		ent->think = Think_CalcMoveSpeed;
	else
		ent->think = Think_SpawnDoorTrigger;
}


/*QUAKED func_water (0 .5 .8) ? START_OPEN
func_water is a moveable water brush.  It must be targeted to operate.  Use a non-water texture at your own risk.

START_OPEN causes the water to move to its destination when spawned and operate in reverse.

"angle"		determines the opening direction (up or down only)
"speed"		movement speed (25 default)
"wait"		wait before returning (-1 default, -1 = TOGGLE)
"lip"		lip remaining at end of move (0 default)
"sounds"	(yes, these need to be changed)
0)	no sound
1)	water
2)	lava
*/

void SP_func_water (edict_t *self)
{
	vec3_t	abs_movedir;

	G_SetMovedir (self->s.angles, self->movedir);
	self->movetype = MOVETYPE_PUSH;
	self->solid = SOLID_BSP;
	gi.setmodel (self, self->model);

	switch (self->sounds)
	{
		default:
			break;

		case 1: // water
			self->moveinfo.sound_start = gi.soundindex  ("world/mov_watr.wav");
			self->moveinfo.sound_end = gi.soundindex  ("world/stp_watr.wav");
			break;

		case 2: // lava
			self->moveinfo.sound_start = gi.soundindex  ("world/mov_watr.wav");
			self->moveinfo.sound_end = gi.soundindex  ("world/stp_watr.wav");
			break;
	}

	// calculate second position
	VectorCopy (self->s.origin, self->pos1);
	abs_movedir[0] = fabs(self->movedir[0]);
	abs_movedir[1] = fabs(self->movedir[1]);
	abs_movedir[2] = fabs(self->movedir[2]);
	self->moveinfo.distance = abs_movedir[0] * self->size[0] + abs_movedir[1] * self->size[1] + abs_movedir[2] * self->size[2] - st.lip;
	VectorMA (self->pos1, self->moveinfo.distance, self->movedir, self->pos2);

	// if it starts open, switch the positions
	if (self->spawnflags & DOOR_START_OPEN)
	{
		VectorCopy (self->pos2, self->s.origin);
		VectorCopy (self->pos1, self->pos2);
		VectorCopy (self->s.origin, self->pos1);
	}

	VectorCopy (self->pos1, self->moveinfo.start_origin);
	VectorCopy (self->s.angles, self->moveinfo.start_angles);
	VectorCopy (self->pos2, self->moveinfo.end_origin);
	VectorCopy (self->s.angles, self->moveinfo.end_angles);

	self->moveinfo.state = STATE_BOTTOM;

	if (!self->speed)
		self->speed = 25;
	self->moveinfo.accel = self->moveinfo.decel = self->moveinfo.speed = self->speed;

	if (!self->wait)
		self->wait = -1;
	self->moveinfo.wait = self->wait;

	self->use = door_use;

	if (self->wait == -1)
		self->spawnflags |= DOOR_TOGGLE;

	self->classname = "func_door";

	gi.linkentity (self);
}


#define TRAIN_START_ON		1
#define TRAIN_TOGGLE		2
#define TRAIN_BLOCK_STOPS	4

/*QUAKED func_train (0 .5 .8) ? START_ON TOGGLE BLOCK_STOPS
Trains are moving platforms that players can ride.
The targets origin specifies the min point of the train at each corner.
The train spawns at the first target it is pointing at.
If the train is the target of a button or trigger, it will not begin moving until activated.
speed	default 100
dmg		default	2
noise	looping sound to play when the train is in motion

*/
void train_next (edict_t *self);

void train_blocked (edict_t *self, edict_t *other)
{
	if (!(other->svflags & SVF_MONSTER) && (!other->client) )
	{
		// give it a chance to go away on it's own terms (like gibs)
		T_Damage (other, self, self, vec3_origin, other->s.origin, vec3_origin, 100000, 1, 0, MOD_CRUSH);
		// if it's still there, nuke it
		if (other)
			BecomeExplosion1 (other);
		return;
	}

	if (level.time < self->touch_debounce_time)
		return;

	if (!self->dmg)
		return;
	self->touch_debounce_time = level.time + 0.5;
	T_Damage (other, self, self, vec3_origin, other->s.origin, vec3_origin, self->dmg, 1, 0, MOD_CRUSH);
}

void train_wait (edict_t *self)
{
	if (self->target_ent->pathtarget)
	{
		char	*savetarget;
		edict_t	*ent;

		ent = self->target_ent;
		savetarget = ent->target;
		ent->target = ent->pathtarget;
		G_UseTargets (ent, self->activator, NULL);
		ent->target = savetarget;

		// make sure we didn't get killed by a killtarget
		if (!self->inuse)
			return;
	}

	if (self->moveinfo.wait)
	{
		if (self->moveinfo.wait > 0)
		{
			self->nextthink = level.time + self->moveinfo.wait;
			self->think = train_next;
		}
		else if (self->spawnflags & TRAIN_TOGGLE)  // && wait < 0
		{
			train_next (self);
			self->spawnflags &= ~TRAIN_START_ON;
			VectorClear (self->velocity);
			self->nextthink = 0;
		}

		if (!(self->flags & FL_TEAMSLAVE))
		{
			if (self->moveinfo.sound_end)
				gi.sound (self, CHAN_NO_PHS_ADD+CHAN_VOICE, self->moveinfo.sound_end, 1, ATTN_STATIC, 0);
			self->s.sound = 0;
		}
	}
	else
	{
		train_next (self);
	}

}

void train_next (edict_t *self)
{
	edict_t		*ent;
	vec3_t		dest;
	bool		first;

	first = true;
again:
	if (!self->target)
	{
//		gi.dprintf ("train_next: no next target\n");
		return;
	}

	ent = G_PickTarget (self->target, NULL);
	if (!ent)
	{
		gi.dprintf ("train_next: bad target %s\n", self->target);
		return;
	}

	self->target = ent->target;

	// check for a teleport path_corner
	if (ent->spawnflags & 1)
	{
		if (!first)
		{
			gi.dprintf ("connected teleport path_corners, see %s at %s\n", ent->classname, vtos(ent->s.origin));
			return;
		}
		first = false;
		VectorSubtract (ent->s.origin, self->mins, self->s.origin);
		VectorCopy (self->s.origin, self->s.old_origin);
		self->s.event = EV_OTHER_TELEPORT;
		gi.linkentity (self);
		goto again;
	}

	self->moveinfo.wait = ent->wait;
	self->target_ent = ent;

	if (!(self->flags & FL_TEAMSLAVE))
	{
		if (self->moveinfo.sound_start)
			gi.sound (self, CHAN_NO_PHS_ADD+CHAN_VOICE, self->moveinfo.sound_start, 1, ATTN_STATIC, 0);
		self->s.sound = self->moveinfo.sound_middle;
	}

	VectorSubtract (ent->s.origin, self->mins, dest);
	self->moveinfo.state = STATE_TOP;
	VectorCopy (self->s.origin, self->moveinfo.start_origin);
	VectorCopy (dest, self->moveinfo.end_origin);
	Move_Calc (self, dest, train_wait);
	self->spawnflags |= TRAIN_START_ON;
}

void train_resume (edict_t *self)
{
	edict_t	*ent;
	vec3_t	dest;

	ent = self->target_ent;

	VectorSubtract (ent->s.origin, self->mins, dest);
	self->moveinfo.state = STATE_TOP;
	VectorCopy (self->s.origin, self->moveinfo.start_origin);
	VectorCopy (dest, self->moveinfo.end_origin);
	Move_Calc (self, dest, train_wait);
	self->spawnflags |= TRAIN_START_ON;
}

void func_train_find (edict_t *self)
{
	edict_t *ent;

	if (!self->target)
	{
		gi.dprintf ("train_find: no target\n");
		return;
	}
	ent = G_PickTarget (self->target, NULL);
	if (!ent)
	{
		gi.dprintf ("train_find: target %s not found\n", self->target);
		return;
	}
	self->target = ent->target;

	VectorSubtract (ent->s.origin, self->mins, self->s.origin);
	gi.linkentity (self);

	// if not triggered, start immediately
	if (!self->targetname)
		self->spawnflags |= TRAIN_START_ON;

	if (self->spawnflags & TRAIN_START_ON)
	{
		self->nextthink = level.time + FRAMETIME;
		self->think = train_next;
		self->activator = self;
	}
}

void train_use (edict_t *self, edict_t *other, edict_t *activator)
{
	self->activator = activator;

	if (self->spawnflags & TRAIN_START_ON)
	{
		if (!(self->spawnflags & TRAIN_TOGGLE))
			return;
		self->spawnflags &= ~TRAIN_START_ON;
		VectorClear (self->velocity);
		self->nextthink = 0;
	}
	else
	{
		if (self->target_ent)
			train_resume(self);
		else
			train_next(self);
	}
}

void SP_func_train (edict_t *self)
{
	self->movetype = MOVETYPE_PUSH;

	VectorClear (self->s.angles);
	self->blocked = train_blocked;
	if (self->spawnflags & TRAIN_BLOCK_STOPS)
		self->dmg = 0;
	else
	{
		if (!self->dmg)
			self->dmg = 100;
	}
	self->solid = SOLID_BSP;
	gi.setmodel (self, self->model);

	if (st.noise)
		self->moveinfo.sound_middle = gi.soundindex  (st.noise);

	if (!self->speed)
		self->speed = 100;

	self->moveinfo.speed = self->speed;
	self->moveinfo.accel = self->moveinfo.decel = self->moveinfo.speed;

	self->use = train_use;

	gi.linkentity (self);

	if (self->target)
	{
		// start trains on the second frame, to make sure their targets have had
		// a chance to spawn
		self->nextthink = level.time + FRAMETIME;
		self->think = func_train_find;
	}
	else
	{
		gi.dprintf ("func_train without a target at %s\n", vtos(self->absmin));
	}
}


/*QUAKED trigger_elevator (0.3 0.1 0.6) (-8 -8 -8) (8 8 8)
*/
void trigger_elevator_use (edict_t *self, edict_t *other, edict_t *activator)
{
	edict_t *target;

	if (self->movetarget->nextthink)
	{
//		gi.dprintf("elevator busy\n");
		return;
	}

	if (!other->pathtarget)
	{
		gi.dprintf("elevator used with no pathtarget\n");
		return;
	}

	target = G_PickTarget (other->pathtarget, NULL);
	if (!target)
	{
		gi.dprintf("elevator used with bad pathtarget: %s\n", other->pathtarget);
		return;
	}

	self->movetarget->target_ent = target;
	train_resume (self->movetarget);
}

void trigger_elevator_init (edict_t *self)
{
	if (!self->target)
	{
		gi.dprintf("trigger_elevator has no target\n");
		return;
	}
	self->movetarget = G_PickTarget (self->target, NULL);
	if (!self->movetarget)
	{
		gi.dprintf("trigger_elevator unable to find target %s\n", self->target);
		return;
	}
	if (strcmp(self->movetarget->classname, "func_train") != 0)
	{
		gi.dprintf("trigger_elevator target %s is not a train\n", self->target);
		return;
	}

	self->use = trigger_elevator_use;
	self->svflags = SVF_NOCLIENT;

}

void SP_trigger_elevator (edict_t *self)
{
	self->think = trigger_elevator_init;
	self->nextthink = level.time + FRAMETIME;
}


/*QUAKED func_timer (0.3 0.1 0.6) (-8 -8 -8) (8 8 8) START_ON
"wait"			base time between triggering all targets, default is 1
"random"		wait variance, default is 0

so, the basic time between firing is a random time between
(wait - random) and (wait + random)

"delay"			delay before first firing when turned on, default is 0

"pausetime"		additional delay used only the very first time
				and only if spawned with START_ON

These can used but not touched.
*/
void func_timer_think (edict_t *self)
{
	G_UseTargets (self, self->activator, NULL);
	self->nextthink = level.time + self->wait + crandom() * self->random;
}

void func_timer_use (edict_t *self, edict_t *other, edict_t *activator)
{
	self->activator = activator;

	// if on, turn it off
	if (self->nextthink)
	{
		self->nextthink = 0;
		return;
	}

	// turn it on
	if (self->delay)
		self->nextthink = level.time + self->delay;
	else
		func_timer_think (self);
}

void SP_func_timer (edict_t *self)
{
	if (!self->wait)
		self->wait = 1.0;

	self->use = func_timer_use;
	self->think = func_timer_think;

	if (self->random >= self->wait)
	{
		self->random = self->wait - FRAMETIME;
		gi.dprintf("func_timer at %s has random >= wait\n", vtos(self->s.origin));
	}

	if (self->spawnflags & 1)
	{
		self->nextthink = level.time + 1.0 + st.pausetime + self->delay + self->wait + crandom() * self->random;
		self->activator = self;
	}

	self->svflags = SVF_NOCLIENT;
}


/*QUAKED func_conveyor (0 .5 .8) ? START_ON TOGGLE
Conveyors are stationary brushes that move what's on them.
The brush should be have a surface with at least one current content enabled.
speed	default 100
*/

void func_conveyor_use (edict_t *self, edict_t *other, edict_t *activator)
{
	if (self->spawnflags & 1)
	{
		self->speed = 0;
		self->spawnflags &= ~1;
	}
	else
	{
		self->speed = self->count;
		self->spawnflags |= 1;
	}

	if (!(self->spawnflags & 2))
		self->count = 0;
}

void SP_func_conveyor (edict_t *self)
{
	if (!self->speed)
		self->speed = 100;

	if (!(self->spawnflags & 1))
	{
		self->count = self->speed;
		self->speed = 0;
	}

	self->use = func_conveyor_use;

	gi.setmodel (self, self->model);
	self->solid = SOLID_BSP;
	gi.linkentity (self);
}


/*QUAKED func_door_secret (0 .5 .8) ? always_shoot 1st_left 1st_down
A secret door.  Slide back and then to the side.

open_once		doors never closes
1st_left		1st move is left of arrow
1st_down		1st move is down from arrow
always_shoot	door is shootebale even if targeted

"angle"		determines the direction
"dmg"		damage to inflic when blocked (default 2)
"wait"		how long to hold in the open position (default 5, -1 means hold)
*/

#define SECRET_ALWAYS_SHOOT	1
#define SECRET_1ST_LEFT		2
#define SECRET_1ST_DOWN		4

void door_secret_move1 (edict_t *self);
void door_secret_move2 (edict_t *self);
void door_secret_move3 (edict_t *self);
void door_secret_move4 (edict_t *self);
void door_secret_move5 (edict_t *self);
void door_secret_move6 (edict_t *self);
void door_secret_done (edict_t *self);

void door_secret_use (edict_t *self, edict_t *other, edict_t *activator)
{
	// make sure we're not already moving
	if (!VectorCompare(self->s.origin, vec3_origin))
		return;

	Move_Calc (self, self->pos1, door_secret_move1);
	door_use_areaportals (self, true);
}

void door_secret_move1 (edict_t *self)
{
	self->nextthink = level.time + 1.0;
	self->think = door_secret_move2;
}

void door_secret_move2 (edict_t *self)
{
	Move_Calc (self, self->pos2, door_secret_move3);
}

void door_secret_move3 (edict_t *self)
{
	if (self->wait == -1)
		return;
	self->nextthink = level.time + self->wait;
	self->think = door_secret_move4;
}

void door_secret_move4 (edict_t *self)
{
	Move_Calc (self, self->pos1, door_secret_move5);
}

void door_secret_move5 (edict_t *self)
{
	self->nextthink = level.time + 1.0;
	self->think = door_secret_move6;
}

void door_secret_move6 (edict_t *self)
{
	Move_Calc (self, vec3_origin, door_secret_done);
}

void door_secret_done (edict_t *self)
{
	if (!(self->targetname) || (self->spawnflags & SECRET_ALWAYS_SHOOT))
	{
		self->health = 0;
		self->takedamage = DAMAGE_YES;
	}
	door_use_areaportals (self, false);
}

void door_secret_blocked  (edict_t *self, edict_t *other)
{
	if (!(other->svflags & SVF_MONSTER) && (!other->client) )
	{
		// give it a chance to go away on it's own terms (like gibs)
		T_Damage (other, self, self, vec3_origin, other->s.origin, vec3_origin, 100000, 1, 0, MOD_CRUSH);
		// if it's still there, nuke it
		if (other)
			BecomeExplosion1 (other);
		return;
	}

	if (level.time < self->touch_debounce_time)
		return;
	self->touch_debounce_time = level.time + 0.5;

	T_Damage (other, self, self, vec3_origin, other->s.origin, vec3_origin, self->dmg, 1, 0, MOD_CRUSH);
}

void door_secret_die (edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point)
{
	self->takedamage = DAMAGE_NO;
	door_secret_use (self, attacker, attacker);
}

void SP_func_door_secret (edict_t *ent)
{
	vec3_t	forward, right, up;
	float	side;
	float	width;
	float	length;

	ent->moveinfo.sound_start = gi.soundindex  ("doors/dr1_strt.wav");
	ent->moveinfo.sound_middle = gi.soundindex  ("doors/dr1_mid.wav");
	ent->moveinfo.sound_end = gi.soundindex  ("doors/dr1_end.wav");

	ent->movetype = MOVETYPE_PUSH;
	ent->solid = SOLID_BSP;
	gi.setmodel (ent, ent->model);

	ent->blocked = door_secret_blocked;
	ent->use = door_secret_use;

	if (!(ent->targetname) || (ent->spawnflags & SECRET_ALWAYS_SHOOT))
	{
		ent->health = 0;
		ent->takedamage = DAMAGE_YES;
		ent->die = door_secret_die;
	}

	if (!ent->dmg)
		ent->dmg = 2;

	if (!ent->wait)
		ent->wait = 5;

	ent->moveinfo.accel =
	ent->moveinfo.decel =
	ent->moveinfo.speed = 50;

	// calculate positions
	AngleVectors (ent->s.angles, forward, right, up);
	VectorClear (ent->s.angles);
	side = 1.0 - (ent->spawnflags & SECRET_1ST_LEFT);
	if (ent->spawnflags & SECRET_1ST_DOWN)
		width = fabs(DotProduct(up, ent->size));
	else
		width = fabs(DotProduct(right, ent->size));
	length = fabs(DotProduct(forward, ent->size));
	if (ent->spawnflags & SECRET_1ST_DOWN)
		VectorMA (ent->s.origin, -1 * width, up, ent->pos1);
	else
		VectorMA (ent->s.origin, side * width, right, ent->pos1);
	VectorMA (ent->pos1, length, forward, ent->pos2);

	if (ent->health)
	{
		ent->takedamage = DAMAGE_YES;
		ent->die = door_killed;
		ent->max_health = ent->health;
	}
	else if (ent->targetname && ent->message)
	{
		gi.soundindex ("misc/talk.wav");
		ent->touch = door_touch;
	}

	ent->classname = "func_door";

	gi.linkentity (ent);
}


/*
QUAKED func_clock (0 0 1) (-8 -8 -8) (8 8 8) TIMER_UP TIMER_DOWN START_OFF MULTI_USE
target a target_string with this

The default is to be a time of day clock

TIMER_UP and TIMER_DOWN run for "count" seconds and the fire "pathtarget"
If START_OFF, this entity must be used before it starts

"style"		0 "xx"
			1 "xx:xx"
			2 "xx:xx:xx"
*/

#define	CLOCK_MESSAGE_SIZE	16

// don't let field width of any clock messages change, or it
// could cause an overwrite after a game load

static void func_clock_reset (edict_t *self)
{
	self->activator = NULL;
	if (self->spawnflags & 1)
	{
		self->health = 0;
		self->wait = self->count;
	}
	else if (self->spawnflags & 2)
	{
		self->health = self->count;
		self->wait = 0;
	}
}

static void func_clock_format_countdown (edict_t *self)
{
	if (self->style == 0)
	{
		Com_sprintf (self->message, CLOCK_MESSAGE_SIZE, "%2i", self->health);
		return;
	}

	if (self->style == 1)
	{
		Com_sprintf(self->message, CLOCK_MESSAGE_SIZE, "%2i:%2i", self->health / 60, self->health % 60);
		if (self->message[3] == ' ')
			self->message[3] = '0';
		return;
	}

	if (self->style == 2)
	{
		Com_sprintf(self->message, CLOCK_MESSAGE_SIZE, "%2i:%2i:%2i", self->health / 3600, (self->health - (self->health / 3600) * 3600) / 60, self->health % 60);
		if (self->message[3] == ' ')
			self->message[3] = '0';
		if (self->message[6] == ' ')
			self->message[6] = '0';
		return;
	}
}

void func_clock_think (edict_t *self)
{
	if (!self->enemy)
	{
		self->enemy = G_Find (NULL, FOFS(targetname), self->target, NULL);
		if (!self->enemy)
			return;
	}

	if (self->spawnflags & 1)
	{
		func_clock_format_countdown (self);
		self->health++;
	}
	else if (self->spawnflags & 2)
	{
		func_clock_format_countdown (self);
		self->health--;
	}
	else
	{
		struct tm	*ltime;
		time_t		gmtime;

		time(&gmtime);
		ltime = localtime(&gmtime);
		Com_sprintf (self->message, CLOCK_MESSAGE_SIZE, "%2i:%2i:%2i", ltime->tm_hour, ltime->tm_min, ltime->tm_sec);
		if (self->message[3] == ' ')
			self->message[3] = '0';
		if (self->message[6] == ' ')
			self->message[6] = '0';
	}

	self->enemy->message = self->message;
	self->enemy->use (self->enemy, self, self);

	if (((self->spawnflags & 1) && (self->health > self->wait)) ||
		((self->spawnflags & 2) && (self->health < self->wait)))
	{
		if (self->pathtarget)
		{
			char *savetarget;
			char *savemessage;

			savetarget = self->target;
			savemessage = self->message;
			self->target = self->pathtarget;
			self->message = NULL;
			G_UseTargets (self, self->activator, NULL);
			self->target = savetarget;
			self->message = savemessage;
		}

		if (!(self->spawnflags & 8))
			return;

		func_clock_reset (self);

		if (self->spawnflags & 4)
			return;
	}

	self->nextthink = level.time + 1;
}

void func_clock_use (edict_t *self, edict_t *other, edict_t *activator)
{
	if (!(self->spawnflags & 8))
		self->use = NULL;
	if (self->activator)
		return;
	self->activator = activator;
	self->think (self);
}

void SP_func_clock (edict_t *self)
{
	if (!self->target)
	{
		gi.dprintf("%s with no target at %s\n", self->classname, vtos(self->s.origin));
		G_FreeEdict (self);
		return;
	}

	if ((self->spawnflags & 2) && (!self->count))
	{
		gi.dprintf("%s with no count at %s\n", self->classname, vtos(self->s.origin));
		G_FreeEdict (self);
		return;
	}

	if ((self->spawnflags & 1) && (!self->count))
		self->count = 60*60;

	func_clock_reset (self);

	self->message = (char *) gi.TagMalloc (CLOCK_MESSAGE_SIZE, TAG_LEVEL);

	self->think = func_clock_think;

	if (self->spawnflags & 4)
		self->use = func_clock_use;
	else
		self->nextthink = level.time + 1;
}


/*
QUAKED func_areaportal (0 0 0) ?

This is a non-visible object that divides the world into
areas that are seperated when this portal is not activated.
Usually enclosed in the middle of a door.
*/
void Use_Areaportal (edict_t *ent, edict_t *other, edict_t *activator)
{
	ent->count ^= 1;		// toggle state
	gi.SetAreaPortalState (ent->style, ent->count);	// Передадим информацию серверу
}

void SP_func_areaportal (edict_t *ent)
{
	ent->use = Use_Areaportal;
	ent->count = 0;		// always start closed;
}


/*
QUAKED func_wall (0 .5 .8) ? TRIGGER_SPAWN TOGGLE START_ON ANIMATED ANIMATED_FAST
This is just a solid wall if not inhibited

TRIGGER_SPAWN	the wall will not be present until triggered
				it will then blink in to existance; it will
				kill anything that was in it's way

TOGGLE			only valid for TRIGGER_SPAWN walls
				this allows the wall to be turned on and off

START_ON		only valid for TRIGGER_SPAWN walls
				the wall will initially be present
*/

void func_wall_use (edict_t *self, edict_t *other, edict_t *activator)
{
	if (self->solid == SOLID_NOT)
	{
		self->solid = SOLID_BSP;
		self->svflags &= ~SVF_NOCLIENT;
		KillBox (self);
	}
	else
	{
		self->solid = SOLID_NOT;
		self->svflags |= SVF_NOCLIENT;
	}
	gi.linkentity (self);

	if (!(self->spawnflags & 2))
		self->use = NULL;
}

void SP_func_wall (edict_t *self)
{
	self->movetype = MOVETYPE_PUSH;
	gi.setmodel (self, self->model);

	if (self->spawnflags & 8)
		self->s.effects |= EF_ANIM_ALL;
	if (self->spawnflags & 16)
		self->s.effects |= EF_ANIM_ALLFAST;

	// just a wall
	if ((self->spawnflags & 7) == 0)
	{
		self->solid = SOLID_BSP;
		gi.linkentity (self);
		return;
	}

	// it must be TRIGGER_SPAWN
	if (!(self->spawnflags & 1))
		self->spawnflags |= 1;

	// yell if the spawnflags are odd
	if (self->spawnflags & 4)
	{
		if (!(self->spawnflags & 2))
		{
			gi.dprintf("func_wall START_ON without TOGGLE\n");
			self->spawnflags |= 2;
		}
	}

	self->use = func_wall_use;
	if (self->spawnflags & 4)
	{
		self->solid = SOLID_BSP;
	}
	else
	{
		self->solid = SOLID_NOT;
		self->svflags |= SVF_NOCLIENT;
	}
	gi.linkentity (self);
}


/*
QUAKED func_object (0 .5 .8) ? TRIGGER_SPAWN ANIMATED ANIMATED_FAST
This is solid bmodel that will fall if it's support it removed.
*/

void func_object_touch (edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf)
{
	// only squash thing we fall on top of
	if (!plane)
		return;
	if (plane->normal[2] < 1.0)
		return;
	if (other->takedamage == DAMAGE_NO)
		return;
	T_Damage (other, self, self, vec3_origin, self->s.origin, vec3_origin, self->dmg, 1, 0, MOD_CRUSH);
}

void func_object_release (edict_t *self)
{
	self->movetype = MOVETYPE_TOSS;
	self->touch = func_object_touch;
}

void func_object_use (edict_t *self, edict_t *other, edict_t *activator)
{
	self->solid = SOLID_BSP;
	self->svflags &= ~SVF_NOCLIENT;
	self->use = NULL;
	KillBox (self);
	func_object_release (self);
}

void SP_func_object (edict_t *self)
{
	gi.setmodel (self, self->model);

	self->mins[0] += 1;
	self->mins[1] += 1;
	self->mins[2] += 1;
	self->maxs[0] -= 1;
	self->maxs[1] -= 1;
	self->maxs[2] -= 1;

	if (!self->dmg)
		self->dmg = 100;

	if (self->spawnflags == 0)
	{
		self->solid = SOLID_BSP;
		self->movetype = MOVETYPE_PUSH;
		self->think = func_object_release;
		self->nextthink = level.time + 2 * FRAMETIME;
	}
	else
	{
		self->solid = SOLID_NOT;
		self->movetype = MOVETYPE_PUSH;
		self->use = func_object_use;
		self->svflags |= SVF_NOCLIENT;
	}

	if (self->spawnflags & 2)
		self->s.effects |= EF_ANIM_ALL;
	if (self->spawnflags & 4)
		self->s.effects |= EF_ANIM_ALLFAST;

	self->clipmask = MASK_MONSTERSOLID;

	gi.linkentity (self);
}


/*
=================
KillBox

Kills all entities that would touch the proposed new positioning
of ent.  Ent should be unlinked before calling this!
=================
*/
bool KillBox (edict_t *ent)
{
	trace_t		tr;

	while (1)
	{
		tr = gi.trace (ent->s.origin, ent->mins, ent->maxs, ent->s.origin, NULL, MASK_PLAYERSOLID);
		if (!tr.ent)
			break;

		// nail it
		T_Damage (tr.ent, ent, ent, vec3_origin, ent->s.origin, vec3_origin, 100000, 0, DAMAGE_NO_PROTECTION, MOD_TELEFRAG);

		// if we didn't kill it, fail
		if (tr.ent->solid)
			return false;
	}

	return true;		// all clear
}


/*
QUAKED func_killbox (1 0 0) ?
Kills everything inside when fired, irrespective of protection.
*/
void use_killbox (edict_t *self, edict_t *other, edict_t *activator)
{
	KillBox (self);
}

void SP_func_killbox (edict_t *ent)
{
	gi.setmodel (ent, ent->model);
	ent->use = use_killbox;
	ent->svflags = SVF_NOCLIENT;
}



























//==========================================================

/*QUAKED target_give (1 0 0) (-8 -8 -8) (8 8 8)
Gives the activator all the items pointed to.
*/
void Use_Target_Give( edict_t *ent, edict_t *other, edict_t *activator )
{
	edict_t	*t;

	if ( !activator->client )
		return;

	if ( !ent->target )
		return;

	t = NULL;
	it_stay = true;
	while ( (t = G_Find (t, FOFS(targetname), ent->target, "target_give")) != NULL )
	{
		if ( !t->item )
			continue;

		Touch_Item( t, activator, NULL, NULL );

		// make sure it isn't going to respawn or show any events
		t->nextthink = 0;
		gi.unlinkentity( t );
	}
	it_stay = false;
}

void SP_target_give (edict_t *ent)
{
	ent->use = Use_Target_Give;
}


/*
QUAKED target_temp_entity (1 0 0) (-8 -8 -8) (8 8 8)
Fire an origin based temp entity event to the clients.
"style"		type byte
*/
void Use_Target_Tent (edict_t *ent, edict_t *other, edict_t *activator)
{
	bool large_map = LargeMapCheck(ent->s.origin);
	if (large_map)
		gi.WriteByte (svc_temp_entity_ext);
	else
		gi.WriteByte (svc_temp_entity);
	gi.WriteByte (ent->style);
	gi.WritePosition (ent->s.origin, large_map);
	gi.multicast (ent->s.origin, MULTICAST_PVS);
}

void SP_target_temp_entity (edict_t *ent)
{
	ent->use = Use_Target_Tent;
}


//==========================================================


/*QUAKED light (0 1 0) (-8 -8 -8) (8 8 8) START_OFF
Non-displayed light.
Default light value is 300.
Default style is 0.
If targeted, will toggle between on and off.
Default _cone value is 0
*/

/*
For Emit:
filter is:
part_simple		0
part_blood		1
part_smoke		2
part_bigsmoke	3
part_splash		4
part_bubble		5
part_fast_blood	6
и т.д.
*/

#define START_OFF	1

static void light_use (edict_t *self, edict_t *other, edict_t *activator)
{
	int	cs_lights;
	if (net_compatibility->value)
		cs_lights = CS_LIGHTS_Q2;
	else
		cs_lights = CS_LIGHTS_BERS;
	if (self->spawnflags & START_OFF)
	{
		gi.configstring (cs_lights+self->style, "m");
		self->spawnflags &= ~START_OFF;
	}
	else
	{
		gi.configstring (cs_lights+self->style, "a");
		self->spawnflags |= START_OFF;
	}
}

void SP_light (edict_t *self)
{
	int	cs_lights;
	if (net_compatibility->value)
		cs_lights = CS_LIGHTS_Q2;
	else
		cs_lights = CS_LIGHTS_BERS;
	if (self->style >= 32)
	{
		self->use = light_use;
		if (self->spawnflags & START_OFF)
			gi.configstring (cs_lights+self->style, "a");
		else
			gi.configstring (cs_lights+self->style, "m");
	}

	// no targeted lights in deathmatch, because they cause global messages
	if (!self->targetname || deathmatch->value)		// no any lights in deathmatch, because they cause global messages
		G_FreeEdict (self);							// If not deathmatch - kill only not targeted lights...
}

//==========================================================

bool Pickup_Key (edict_t *ent, edict_t *other)
{
	if (coop->value)
	{
		if (strcmp(ent->classname, "key_power_cube") == 0)
		{
			if (other->client->pers.power_cubes & ((ent->spawnflags & 0x0000ff00)>> 8))
				return false;
			other->client->pers.inventory[ITEM_INDEX(ent->item)]++;
			other->client->pers.power_cubes |= ((ent->spawnflags & 0x0000ff00) >> 8);
		}
		else
		{
			if (other->client->pers.inventory[ITEM_INDEX(ent->item)])
				return false;
			other->client->pers.inventory[ITEM_INDEX(ent->item)] = 1;
		}
		return true;
	}
	other->client->pers.inventory[ITEM_INDEX(ent->item)]++;
	return true;
}

//======================================================================
/*QUAKED target_speaker (1 0 0) (-8 -8 -8) (8 8 8) looped-on looped-off reliable
"noise"		wav file to play
"attenuation"
-1 = none, send to whole level
1 = normal fighting sounds
2 = idle sound level
3 = ambient sound level
"volume"	0.0 to 1.0

Normal sounds play each time the target is used.  The reliable flag can be set for crucial voiceovers.

Looped sounds are always atten 3 / vol 1, and the use function toggles it on/off.
Multiple identical looping sounds will just increase volume without any speed cost.
*/
void Use_Target_Speaker (edict_t *ent, edict_t *other, edict_t *activator)
{
	int		chan;

	if (ent->spawnflags & 3)
	{	// looping sound toggles
		if (ent->s.sound)
			ent->s.sound = 0;	// turn it off
		else
			ent->s.sound = ent->noise_index;	// start it
	}
	else
	{	// normal sound
		if (ent->spawnflags & 4)
			chan = CHAN_VOICE|CHAN_RELIABLE;
		else
			chan = CHAN_VOICE;
		// use a positioned_sound, because this entity won't normally be
		// sent to any clients because it is invisible
		gi.positioned_sound (ent->s.origin, ent, chan, ent->noise_index, ent->volume, ent->attenuation, 0);
	}
}

void SP_target_speaker (edict_t *ent)
{
	char	buffer[MAX_QPATH];

	if(!st.noise)
	{
		gi.dprintf("target_speaker with no noise set at %s\n", vtos(ent->s.origin));
		return;
	}
	if (!strstr (st.noise, ".wav"))
		Com_sprintf (buffer, sizeof(buffer), "%s.wav", st.noise);
	else
		strncpy (buffer, st.noise, sizeof(buffer));
	ent->noise_index = gi.soundindex (buffer);

	if (!ent->volume)
		ent->volume = 1.0;

	if (!ent->attenuation)
		ent->attenuation = 1.0;
	else if (ent->attenuation == -1)	// use -1 so 0 defaults to 1
		ent->attenuation = 0;

	// check for prestarted looping sound
	if (ent->spawnflags & 1)
		ent->s.sound = ent->noise_index;

	ent->use = Use_Target_Speaker;

	// must link the entity so we get areas and clusters so
	// the server can determine who to send updates to
	gi.linkentity (ent);
}


//==========================================================

void Use_Target_Help (edict_t *ent, edict_t *other, edict_t *activator)
{
	if (ent->spawnflags & 1)
		strncpy (game.helpmessage1, ent->message, sizeof(game.helpmessage2)-1);
	else
		strncpy (game.helpmessage2, ent->message, sizeof(game.helpmessage1)-1);

	game.helpchanged++;
}

/*
QUAKED target_help (1 0 1) (-16 -16 -24) (16 16 24) help1
When fired, the "message" key becomes the current personal computer string, and the message light will be set on all clients status bars.
*/
void SP_target_help(edict_t *ent)
{
	if (deathmatch->value)
	{	// auto-remove for deathmatch
		G_FreeEdict (ent);
		return;
	}

	if (!ent->message)
	{
		gi.dprintf ("%s with no message at %s\n", ent->classname, vtos(ent->s.origin));
		G_FreeEdict (ent);
		return;
	}
	ent->use = Use_Target_Help;
}

//==========================================================

/*
QUAKED target_secret (1 0 1) (-8 -8 -8) (8 8 8)
Counts a secret found.
These are single use targets.
*/
void use_target_secret (edict_t *ent, edict_t *other, edict_t *activator)
{
	gi.sound (ent, CHAN_VOICE, ent->noise_index, 1, ATTN_NORM, 0);

	level.found_secrets++;

	G_UseTargets (ent, activator, "secret");
	G_FreeEdict (ent);
}

void SP_target_secret (edict_t *ent)
{
	if (deathmatch->value)
	{	// auto-remove for deathmatch
		G_FreeEdict (ent);
		return;
	}

	ent->use = use_target_secret;
	if (!st.noise)
		st.noise = "misc/secret.wav";
	ent->noise_index = gi.soundindex (st.noise);
	ent->svflags = SVF_NOCLIENT;
	level.total_secrets++;
}

//==========================================================

/*
QUAKED target_goal (1 0 1) (-8 -8 -8) (8 8 8)
Counts a goal completed.
These are single use targets.
*/
void use_target_goal (edict_t *ent, edict_t *other, edict_t *activator)
{
	gi.sound (ent, CHAN_VOICE, ent->noise_index, 1, ATTN_NORM, 0);

	level.found_goals++;

	if (level.found_goals == level.total_goals)
	{
		if (net_compatibility->value)
			gi.configstring (/*CS_CDTRACK*/CS_TRACK_AMBIENT, "0");
		else
			gi.configstring (/*CS_CDTRACK*/CS_TRACK_AMBIENT, va("0 %i %i %i", world_ambient_r, world_ambient_g, world_ambient_b));
	}

	G_UseTargets (ent, activator, "goal");
	G_FreeEdict (ent);
}

void SP_target_goal (edict_t *ent)
{
	if (deathmatch->value)
	{	// auto-remove for deathmatch
		G_FreeEdict (ent);
		return;
	}

	ent->use = use_target_goal;
	if (!st.noise)
		st.noise = "misc/secret.wav";
	ent->noise_index = gi.soundindex (st.noise);
	ent->svflags = SVF_NOCLIENT;
	level.total_goals++;
}

//==========================================================


/*
QUAKED target_explosion (1 0 0) (-8 -8 -8) (8 8 8)
Spawns an explosion temporary entity when used.

"delay"		wait this long before going off
"dmg"		how much radius damage should be done, defaults to 0
*/
void target_explosion_explode (edict_t *self)
{
	float		save;

	bool large_map = LargeMapCheck(self->s.origin);
	if (large_map)
		gi.WriteByte (svc_temp_entity_ext);
	else
		gi.WriteByte (svc_temp_entity);
	gi.WriteByte (TE_EXPLOSION1);
	gi.WritePosition (self->s.origin, large_map);
////gi.WriteDir (vec3_origin);		/// FIXME: [0,0,1] ???
	gi.multicast (NULL/*self->s.origin*/, MULTICAST_ALL/*PHS*/);	// О долгоживущих пятнах от взрыва должны "знать" все

	T_RadiusDamage (self, self->activator, self->dmg, NULL, self->dmg+40, MOD_EXPLOSIVE);

	save = self->delay;
	self->delay = 0;
	G_UseTargets (self, self->activator, "explosion_explode");
	self->delay = save;
}

void use_target_explosion (edict_t *self, edict_t *other, edict_t *activator)
{
	self->activator = activator;

	if (!self->delay)
	{
		target_explosion_explode (self);
		return;
	}

	self->think = target_explosion_explode;
	self->nextthink = level.time + self->delay;
}

void SP_target_explosion (edict_t *ent)
{
	ent->use = use_target_explosion;
	ent->svflags = SVF_NOCLIENT;
}


//==========================================================

void VelocityForDamage (int damage, vec3_t v)
{
	v[0] = 100.0 * crandom();
	v[1] = 100.0 * crandom();
	v[2] = 200.0 + 100.0 * random();

	if (damage < 50)
		VectorScale (v, 0.7, v);
	else
		VectorScale (v, 1.2, v);
}


void ClipGibVelocity (edict_t *ent)
{
	if (ent->velocity[0] < -300)
		ent->velocity[0] = -300;
	else if (ent->velocity[0] > 300)
		ent->velocity[0] = 300;
	if (ent->velocity[1] < -300)
		ent->velocity[1] = -300;
	else if (ent->velocity[1] > 300)
		ent->velocity[1] = 300;
	if (ent->velocity[2] < 200)
		ent->velocity[2] = 200;	// always some upwards
	else if (ent->velocity[2] > 500)
		ent->velocity[2] = 500;
}


void gib_think (edict_t *self)
{
	self->s.frame++;
	self->nextthink = level.time + FRAMETIME;

	if (self->s.frame == 10)
	{
		self->think = G_FreeEdict;
		self->nextthink = level.time + 8 + random()*10;
	}
}

void gib_touch (edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf)
{
	vec3_t	normal_angles, right;

	if (!self->groundentity)
		return;

	self->touch = NULL;

	if (plane)
	{
		gi.sound (self, CHAN_VOICE, gi.soundindex ("misc/fhit3.wav"), 1, ATTN_NORM, 0);

		vectoangles (plane->normal, normal_angles);
		AngleVectors (normal_angles, NULL, right, NULL);
		vectoangles (right, self->s.angles);

		if (self->s.modelindex == sm_meat_index)
		{
			self->s.frame++;
			self->think = gib_think;
			self->nextthink = level.time + FRAMETIME;
		}
	}
}

void gib_die (edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point)
{
	G_FreeEdict (self);
}


void G_FreeEdictExt (edict_t *ed);
void ThrowGib (edict_t *self, char *gibname, int damage, int type)
{
	edict_t *gib;
	vec3_t	vd;
	vec3_t	origin;
	vec3_t	size;
	float	vscale;

	gib = G_Spawn();

	VectorScale (self->size, 0.5, size);
	VectorAdd (self->absmin, size, origin);
	gib->s.origin[0] = origin[0] + crandom() * size[0];
	gib->s.origin[1] = origin[1] + crandom() * size[1];
	gib->s.origin[2] = origin[2] + crandom() * size[2];

	gi.setmodel (gib, gibname);
	gib->solid = SOLID_NOT;
	gib->s.effects |= EF_GIB;
	gib->flags |= FL_NO_KNOCKBACK;
	gib->takedamage = DAMAGE_YES;
	gib->die = gib_die;

	if (type == GIB_ORGANIC)
	{
		gib->movetype = MOVETYPE_TOSS;
		gib->touch = gib_touch;
		vscale = 0.5;
	}
	else
	{
		gib->movetype = MOVETYPE_BOUNCE;
		vscale = 1.0;
	}

	VelocityForDamage (damage, vd);
	VectorMA (self->velocity, vscale, vd, gib->velocity);
	ClipGibVelocity (gib);
	gib->avelocity[0] = random()*600;
	gib->avelocity[1] = random()*600;
	gib->avelocity[2] = random()*600;

	if(sv_gibs->value >= 1)
	{
		gib->think = G_FreeEdict;
		gib->nextthink = level.time + (1 + random()) * sv_gibs->value;
	}
	else
	{
		gib->think = G_FreeEdictExt;
		gib->nextthink = level.time + 1 + random();
	}

	gi.linkentity (gib);
}

void ThrowClientHead (edict_t *self, int damage, bool no_head)
{
	vec3_t	vd;
	char	*gibname;

	if (rand()&1)
	{
		gibname = "models/objects/gibs/head2/tris.md2";
		self->s.skinnum = 1;		// second skin is player
	}
	else
	{
		gibname = "models/objects/gibs/skull/tris.md2";
		self->s.skinnum = 0;
	}

	self->s.origin[2] += 16;
	if (rand()&1)
		self->s.angles[2] = 90;
	else
		self->s.angles[2] = -90;
	self->s.frame = 0;
	if (no_head)
		self->s.modelindex = 0;
	else
		gi.setmodel (self, gibname);
	VectorSet (self->mins, -8, -8, -4);
	VectorSet (self->maxs, 8, 8, 8);

	self->takedamage = DAMAGE_NO;
	self->solid = SOLID_NOT;
	self->s.effects = EF_GIB;
	self->s.sound = 0;
	self->flags |= FL_NO_KNOCKBACK;

	if(coop->value || deathmatch->value)
		self->movetype = MOVETYPE_BOUNCE_DM;
	else
		self->movetype = MOVETYPE_BOUNCE;
	VelocityForDamage (damage, vd);
	VectorAdd (self->velocity, vd, self->velocity);

	if (self->client)	// bodies in the queue don't have a client anymore
	{
		self->client->anim_priority = ANIM_DEATH;
		self->client->anim_end = self->s.frame;
	}
	else
	{
		self->think = NULL;
		self->nextthink = 0;
	}

	gi.linkentity (self);
}


void WriteScaledDir(vec3_t vec)
{
	float len = VectorNormalize(vec);
	gi.WriteDir(vec);	// 1 byte
	gi.WriteFloat(len);	// 4 bytes
}


void NetThrowGibs (edict_t *self, unsigned type, int damage)
{
	vec3_t	size, vd;
	edict_t	gib;

	bool large_map = LargeMapCheck(self->absmin);
	if (large_map)
		gi.WriteByte (svc_temp_entity_ext);
	else
		gi.WriteByte (svc_temp_entity);
	gi.WriteByte (TE_GIBS_ARM + type);
	gi.WritePosition (self->absmin, large_map);

	VectorScale (self->size, 0.5, size);
	gi.WriteByte (size[0]);
	gi.WriteByte (size[1]);
	gi.WriteByte (size[2]);

	VelocityForDamage (damage, vd);
	VectorMA (self->velocity, 0.5, vd, gib.velocity);
	ClipGibVelocity (&gib);
	WriteScaledDir (gib.velocity);

	gi.multicast (self->s.origin, MULTICAST_PVS);
}

void NetThrowMonsterGibs (edict_t *self, unsigned type, int damage)
{
	vec3_t	size, vd;
	edict_t	gib;

	VectorScale (self->size, 0.5, size);
	VectorAdd(size, self->absmin, size);

	bool large_map = LargeMapCheck(size);
	if (large_map)
		gi.WriteByte (svc_temp_entity_ext);
	else
		gi.WriteByte (svc_temp_entity);
	gi.WriteByte (TE_GIBS_ARM + type);
	gi.WritePosition (size, large_map);

	VelocityForDamage (damage, vd);
	VectorMA (self->velocity, 0.5, vd, gib.velocity);
	ClipGibVelocity (&gib);
	WriteScaledDir (gib.velocity);

	gi.WriteAngle(self->s.angles[1]);

	gi.multicast (self->s.origin, MULTICAST_PVS);
}

void NetThrowMonsterGibsExt (edict_t *self, unsigned type, int damage)
{
	vec3_t	size, vd;
	edict_t	gib;

	VectorScale (self->size, 0.5, size);
	VectorAdd(size, self->absmin, size);

	bool large_map = LargeMapCheck(size);
	if (large_map)
		gi.WriteByte (svc_temp_entity_ext);
	else
		gi.WriteByte (svc_temp_entity);
	gi.WriteByte (TE_GIBS_ARM + type);
	gi.WritePosition (size, large_map);

	VelocityForDamage (damage, vd);
	VectorMA (self->velocity, 0.5, vd, gib.velocity);
	ClipGibVelocity (&gib);
	WriteScaledDir (gib.velocity);

	gi.WriteAngle(self->s.angles[1]);
	gi.WriteByte(self->s.skinnum & 0xFF);

	gi.multicast (self->s.origin, MULTICAST_PVS);
}

void body_die (edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point)
{
	int	n;

	if (self->health < -40)
	{
		gi.sound (self, CHAN_BODY, gi.soundindex ("misc/udeath.wav"), 1, ATTN_NORM, 0);
		if(net_compatibility->value || !net_fixoverflow->value)
		{
			for (n= 0; n < 4; n++)
				ThrowGib (self, "models/objects/gibs/sm_meat/tris.md2", damage, GIB_ORGANIC);
			ThrowClientHead (self, damage, false);
		}
		else
		{
			NetThrowMonsterGibsExt (self, GIBS_PLAYER, damage);
			ThrowClientHead (self, damage, true);
		}
		self->takedamage = DAMAGE_NO;
	}
}


void CopyToBodyQue (edict_t *ent)
{
	edict_t		*body;

	// grab a body que and cycle to the next one
	body = &g_edicts[(int)maxclients_cvar->value + level.body_que + 1];
	level.body_que = (level.body_que + 1) % BODY_QUEUE_SIZE;

	// FIXME: send an effect on the removed body
	gi.unlinkentity (ent);

	gi.unlinkentity (body);
	body->s = ent->s;
	body->s.number = body - g_edicts;

	body->svflags = ent->svflags;
	VectorCopy (ent->mins, body->mins);
	VectorCopy (ent->maxs, body->maxs);
	VectorCopy (ent->absmin, body->absmin);
	VectorCopy (ent->absmax, body->absmax);
	VectorCopy (ent->size, body->size);
	body->solid = ent->solid;
	body->clipmask = ent->clipmask;
	body->owner = ent->owner;
	body->movetype = ent->movetype;

	body->die = body_die;
	body->takedamage = DAMAGE_YES;

	gi.linkentity (body);
}


/*
================
SelectRandomDeathmatchSpawnPoint

go to a random point, but NOT the two points closest
to other players
================
*/
edict_t *SelectRandomDeathmatchSpawnPoint ()
{
	edict_t	*spot, *spot1, *spot2;
	int		count = 0;
	int		selection;
	float	range, range1, range2;

	spot = NULL;
	range1 = range2 = 99999;
	spot1 = spot2 = NULL;

	while ((spot = G_Find (spot, FOFS(classname), "info_player_deathmatch", NULL)) != NULL)
	{
		count++;
		range = PlayersRangeFromSpot(spot);
		if (range < range1)
		{
			range1 = range;
			spot1 = spot;
		}
		else if (range < range2)
		{
			range2 = range;
			spot2 = spot;
		}
	}

	if (!count)
		return NULL;

	if (count <= 2)
	{
		spot1 = spot2 = NULL;
	}
	else
		count -= 2;

	selection = rand() % count;

	spot = NULL;
	do
	{
		spot = G_Find (spot, FOFS(classname), "info_player_deathmatch", NULL);
		if (spot == spot1 || spot == spot2)
			selection++;
	} while(selection--);

	return spot;
}


/*
================
PlayersRangeFromSpot

Returns the distance to the nearest player from the given spot
================
*/
float	PlayersRangeFromSpot (edict_t *spot)
{
	edict_t	*player;
	float	bestplayerdistance;
	vec3_t	v;
	int		n;
	float	playerdistance;


	bestplayerdistance = 9999999;

	for (n = 1; n <= maxclients_cvar->value; n++)
	{
		player = &g_edicts[n];

		if (!player->inuse)
			continue;

		if (player->health <= 0)
			continue;

		VectorSubtract (spot->s.origin, player->s.origin, v);
		playerdistance = VectorLength (v);

		if (playerdistance < bestplayerdistance)
			bestplayerdistance = playerdistance;
	}

	return bestplayerdistance;
}


edict_t *SelectFarthestDeathmatchSpawnPoint ()
{
	edict_t	*bestspot;
	float	bestdistance, bestplayerdistance;
	edict_t	*spot;


	spot = NULL;
	bestspot = NULL;
	bestdistance = 0;
	while ((spot = G_Find (spot, FOFS(classname), "info_player_deathmatch", NULL)) != NULL)
	{
		bestplayerdistance = PlayersRangeFromSpot (spot);

		if (bestplayerdistance > bestdistance)
		{
			bestspot = spot;
			bestdistance = bestplayerdistance;
		}
	}

	if (bestspot)
	{
		return bestspot;
	}

	// if there is a player just spawned on each and every start spot
	// we have no choice to turn one into a telefrag meltdown
	spot = G_Find (NULL, FOFS(classname), "info_player_deathmatch", NULL);

	return spot;
}


edict_t *SelectDeathmatchSpawnPoint ()
{
	if ( (int)(dmflags->value) & DF_SPAWN_FARTHEST)
		return SelectFarthestDeathmatchSpawnPoint ();
	else
		return SelectRandomDeathmatchSpawnPoint ();
}


edict_t *SelectCoopSpawnPoint (edict_t *ent)
{
	int		index;
	edict_t	*spot = NULL;
	char	*target;

	index = ent->client - game.clients;

	// player 0 starts in normal player spawn point
	if (!index)
		return NULL;

	spot = NULL;

	// assume there are four coop spots at each spawnpoint
	while (1)
	{
		spot = G_Find (spot, FOFS(classname), "info_player_coop", NULL);
		if (!spot)
			return NULL;	// we didn't have enough...

		target = spot->targetname;
		if (!target)
			target = "";
		if ( Q_strcasecmp(game.spawnpoint, target) == 0 )
		{	// this is a coop spawn point for one of the clients here
			index--;
			if (!index)
				return spot;		// this is it
		}
	}


	return spot;
}


/*
===========
SelectSpawnPoint

Chooses a player start, deathmatch start, coop start, etc
============
*/
void	SelectSpawnPoint (edict_t *ent, vec3_t origin, vec3_t angles)
{
	edict_t	*spot = NULL;

	if (deathmatch->value)
		spot = SelectDeathmatchSpawnPoint ();
	else if (coop->value)
		spot = SelectCoopSpawnPoint (ent);

	// find a single player start spot
	if (!spot)
	{
		while ((spot = G_Find (spot, FOFS(classname), "info_player_start", NULL)) != NULL)
		{
			if (!game.spawnpoint[0] && !spot->targetname)
				break;

			if (!game.spawnpoint[0] || !spot->targetname)
				continue;

			if (Q_strcasecmp(game.spawnpoint, spot->targetname) == 0)
				break;
		}

		if (!spot)
		{
			if (!game.spawnpoint[0])
			{	// there wasn't a spawnpoint without a target, so use any
				spot = G_Find (spot, FOFS(classname), "info_player_start", NULL);
			}
			if (!spot)
				gi.error ("Couldn't find spawn point %s\n", game.spawnpoint);
		}
	}

	VectorCopy (spot->s.origin, origin);
	origin[2] += 9;
	VectorCopy (spot->s.angles, angles);
}


/*
==============
InitClientPersistant

This is only called when the game first initializes in single player,
but is called after each death and level change in deathmatch
==============
*/
void InitClientPersistant (gclient_t *client, char *userinfo)
{
	char		*val;
	int			integer;
	gitem_t		*item;

	memset (&client->pers, 0, sizeof(client->pers));

	item = FindItem("Blaster");
	client->pers.selected_item = ITEM_INDEX(item);
	client->pers.inventory[client->pers.selected_item] = 1;

	client->pers.weapon = item;

///	client->pers.health			= 100;
///	client->pers.max_health		= 100;
	val = Info_ValueForKey(userinfo, "cl_handicap");
	if (strlen(val))
	{
		integer = atoi(val);
		if(integer < 5)
			integer = 5;
		else if(integer > 100)
			integer = 100;
	}
	else
		integer = 100;	/// default for cl_handicap

	client->pers.health			= integer;
	client->pers.max_health		= integer;


	client->pers.max_bullets	= 200;
	client->pers.max_shells		= 100;
	client->pers.max_rockets	= 50;
	client->pers.max_grenades	= 50;
	client->pers.max_cells		= 200;
	client->pers.max_slugs		= 50;

	client->pers.connected = true;
}


/*
===========
ClientUserInfoChanged

called whenever the player updates a userinfo variable.

The game can override any of the settings in place
(forcing skins or names, etc) before copying it off.
============
*/
void ClientUserinfoChanged (edict_t *ent, char *userinfo)
{
	char	*s;
	int		playernum;

	// check for malformed or illegal info strings
	if (!Info_Validate(userinfo))
	{
		strcpy (userinfo, "\\name\\badinfo\\skin\\male/grunt");
	}

	// set name
	s = Info_ValueForKey (userinfo, "name");
	strncpy (ent->client->pers.netname, s, sizeof(ent->client->pers.netname)-1);

	// set railtrail colors
	ent->client->pers.railtrailcore = atoi(Info_ValueForKey (userinfo, "rail_core"));
	ent->client->pers.railtrailspiral = atoi(Info_ValueForKey (userinfo, "rail_spiral"));
	ent->client->pers.railtrailradius = atoi(Info_ValueForKey (userinfo, "rail_radius"));
	if (!ent->client->pers.railtrailcore && !ent->client->pers.railtrailradius)
	{	// если нет ни core, ни spiral, то рисуем луч в старом стиле
		ent->client->pers.railtrailcore = 7;
		ent->client->pers.railtrailradius = 6;
		ent->client->pers.railtrailspiral = 1;
	}

	// set spectator
	s = Info_ValueForKey (userinfo, "spectator");
	// spectators are only supported in deathmatch
	if (deathmatch->value && *s && strcmp(s, "0"))
		ent->client->pers.spectator = true;
	else
		ent->client->pers.spectator = false;

	// set skin
	s = Info_ValueForKey (userinfo, "skin");

	playernum = ent-g_edicts-1;

	// combine name and skin into a configstring
	if (net_compatibility->value)
		gi.configstring (CS_PLAYERSKINS_Q2+playernum, va("%s\\%s", ent->client->pers.netname, s) );
	else
		gi.configstring (CS_PLAYERSKINS_BERS+playernum, va("%s\\%s", ent->client->pers.netname, s) );

	// fov
	if (deathmatch->value && ((int)dmflags->value & DF_FIXED_FOV))
	{
		ent->client->ps.fov = 90;
	}
	else
	{
		ent->client->ps.fov = atoi(Info_ValueForKey(userinfo, "fov"));
		if (ent->client->ps.fov < 1)
			ent->client->ps.fov = 90;
		else if (ent->client->ps.fov > MAX_FOV)
			ent->client->ps.fov = MAX_FOV;
	}

	// handedness
	s = Info_ValueForKey (userinfo, "hand");
	if (strlen(s))
	{
		ent->client->pers.hand = atoi(s);
	}

	// save off the userinfo in case we want to check something later
	strncpy (ent->client->pers.userinfo, userinfo, sizeof(ent->client->pers.userinfo)-1);
}


void FetchClientEntData (edict_t *ent)
{
	ent->health = ent->client->pers.health;
	ent->max_health = ent->client->pers.max_health;
	ent->flags |= ent->client->pers.savedFlags;
////	ent->flags &= ~FL_FLASHLIGHT;
////	ent->client->flashlight = (ent->client->pers.savedFlags & FL_FLASHLIGHT);
	if (coop->value)
		ent->client->resp.score = ent->client->pers.score;
}


void player_pain (edict_t *self, edict_t *other, float kick, int damage)
{
	// player pain is handled at the end of the frame in P_DamageFeedback
}


void LookAtKiller (edict_t *self, edict_t *inflictor, edict_t *attacker)
{
	vec3_t		dir;

	if (attacker && attacker != world && attacker != self)
	{
		VectorSubtract (attacker->s.origin, self->s.origin, dir);
	}
	else if (inflictor && inflictor != world && inflictor != self)
	{
		VectorSubtract (inflictor->s.origin, self->s.origin, dir);
	}
	else
	{
		self->client->killer_yaw = self->s.angles[YAW];
		return;
	}

	if (dir[0])
		self->client->killer_yaw = 180/M_PI*atan2(dir[1], dir[0]);
	else {
		self->client->killer_yaw = 0;
		if (dir[1] > 0)
			self->client->killer_yaw = 90;
		else if (dir[1] < 0)
			self->client->killer_yaw = -90;
	}
	if (self->client->killer_yaw < 0)
		self->client->killer_yaw += 360;
}


bool IsNeutral (edict_t *ent)
{
	char		*info;

	if (!ent->client)
		return false;

	info = Info_ValueForKey (ent->client->pers.userinfo, "gender");

	/// Berserker: crap ((
///	if (info[0] != 'f' && info[0] != 'F'
///		&& info[0] != 'm' && info[0] != 'M'
///		&& info[0] != 'c' && info[0] != 'C'
///		&& info[0] != 'q' && info[0] != 'Q')
///		return true;

	return (Q_strcasecmp(info, "male") && Q_strcasecmp(info, "female"));
}


bool IsFemale (edict_t *ent)
{
	char		*info;

	if (!ent->client)
		return false;

	info = Info_ValueForKey (ent->client->pers.userinfo, "gender");

	/// Berserker: crap ((
///	if (info[0] == 'f' || info[0] == 'F')
///		return true;
///	return false;

	return !Q_strcasecmp(info, "female");
}


void ClientObituary (edict_t *self, edict_t *inflictor, edict_t *attacker)
{
	int			mod;
	char		*message;
	char		*message2;
	bool		ff;

	if (coop->value && attacker->client)
		meansOfDeath |= MOD_FRIENDLY_FIRE;

	message = NULL;
	message2 = "";
	ff = meansOfDeath & MOD_FRIENDLY_FIRE;
	mod = meansOfDeath & ~MOD_FRIENDLY_FIRE;

	if (deathmatch->value || coop->value)
	{
		switch (mod)
		{
		case MOD_SUICIDE:
			message = "suicides";
			break;
		case MOD_FALLING:
			message = "cratered";
			break;
		case MOD_CRUSH:
			message = "was squished";
			break;
		case MOD_WATER:
			message = "sank like a rock";
			break;
		case MOD_SLIME:
			message = "melted";
			break;
		case MOD_LAVA:
			message = "does a back flip into the lava";
			break;
		case MOD_EXPLOSIVE:
		case MOD_BARREL:
			message = "blew up";
			break;
		case MOD_EXIT:
			message = "found a way out";
			break;
		case MOD_TARGET_LASER:
			message = "saw the light";
			break;
		case MOD_TARGET_BLASTER:
			message = "got blasted";
			break;
		case MOD_BOMB:
		case MOD_SPLASH:
		case MOD_TRIGGER_HURT:
			message = "was in the wrong place";
			break;
		}
		if (attacker == self)
		{
			switch (mod)
			{
			case MOD_HELD_GRENADE:
				message = "tried to put the pin back in";
				break;
			case MOD_HG_SPLASH:
			case MOD_G_SPLASH:
				if (IsNeutral(self))
					message = "tripped on its own grenade";
				else if (IsFemale(self))
					message = "tripped on her own grenade";
				else
					message = "tripped on his own grenade";
				break;
			case MOD_R_SPLASH:
				if (IsNeutral(self))
					message = "blew itself up";
				else if (IsFemale(self))
					message = "blew herself up";
				else
					message = "blew himself up";
				break;
			case MOD_BFG_BLAST:
				message = "should have used a smaller gun";
				break;
			default:
				if (IsNeutral(self))
					message = "killed itself";
				else if (IsFemale(self))
					message = "killed herself";
				else
					message = "killed himself";
				break;
			}
		}
		if (message)
		{
			gi.bprintf (PRINT_MEDIUM, "%s %s.\n", self->client->pers.netname, message);
			if (deathmatch->value)
				self->client->resp.score--;
			self->enemy = NULL;
			return;
		}

		self->enemy = attacker;
		if (attacker && attacker->client)
		{
			switch (mod)
			{
			case MOD_BLASTER:
				message = "was blasted by";
				break;
			case MOD_SHOTGUN:
				message = "was gunned down by";
				break;
			case MOD_SSHOTGUN:
				message = "was blown away by";
				message2 = "'s super shotgun";
				break;
			case MOD_MACHINEGUN:
				message = "was machinegunned by";
				break;
			case MOD_CHAINGUN:
				message = "was cut in half by";
				message2 = "'s chaingun";
				break;
			case MOD_GRENADE:
				message = "was popped by";
				message2 = "'s grenade";
				break;
			case MOD_G_SPLASH:
				message = "was shredded by";
				message2 = "'s shrapnel";
				break;
			case MOD_ROCKET:
				message = "ate";
				message2 = "'s rocket";
				break;
			case MOD_R_SPLASH:
				message = "almost dodged";
				message2 = "'s rocket";
				break;
			case MOD_HYPERBLASTER:
				message = "was melted by";
				message2 = "'s hyperblaster";
				break;
			case MOD_RAILGUN:
				message = "was railed by";
				break;
			case MOD_BFG_LASER:
				message = "saw the pretty lights from";
				message2 = "'s BFG";
				break;
			case MOD_BFG_BLAST:
				message = "was disintegrated by";
				message2 = "'s BFG blast";
				break;
			case MOD_BFG_EFFECT:
				message = "couldn't hide from";
				message2 = "'s BFG";
				break;
			case MOD_HANDGRENADE:
				message = "caught";
				message2 = "'s handgrenade";
				break;
			case MOD_HG_SPLASH:
				message = "didn't see";
				message2 = "'s handgrenade";
				break;
			case MOD_HELD_GRENADE:
				message = "feels";
				message2 = "'s pain";
				break;
			case MOD_TELEFRAG:
				message = "tried to invade";
				message2 = "'s personal space";
				break;
			}
			if (message)
			{
				gi.bprintf (PRINT_MEDIUM,"%s %s %s%s\n", self->client->pers.netname, message, attacker->client->pers.netname, message2);
				if (deathmatch->value)
				{
					if (ff)
						attacker->client->resp.score--;
					else
						attacker->client->resp.score++;
				}
				return;
			}
		}
	}

	//Berserker: Single/Coop-player obits (based on Knightmare's code)
	if (!deathmatch->value)
	{
		/// Berserker: special case for monster respawn
		if (mod == MOD_TELEFRAG)
		{
			gi.bprintf (PRINT_MEDIUM,"%s was telefragged\n", self->client->pers.netname);
			if (coop->value)
				self->client->resp.score--;
			self->enemy = NULL;
			return;
		}

		char *common_name = "someone";
		if (attacker->svflags & SVF_MONSTER)
		{
			// Light Guard
			if (!strcmp(attacker->classname, "monster_soldier_light"))
			{
				message = "was blasted by a";
				common_name = "light guard";
			}
			// Shotgun Guard
			else if (!strcmp(attacker->classname, "monster_soldier"))
			{
				message = "was gunned down by a";
				common_name = "shotgun guard";
			}
			// Machinegun Guard
			else if (!strcmp(attacker->classname, "monster_soldier_ss"))
			{
				message = "was machinegunned by a";
				common_name = "machinegun guard";
			}
			// Enforcer
			else if (!strcmp(attacker->classname, "monster_infantry"))
			{
				if (mod == MOD_HIT)
					message = "was bludgened by an";
				else
					message = "was pumped full of lead by an";
				common_name = "infantry";
			}
			// Gunner
			else if (!strcmp(attacker->classname, "monster_gunner"))
			{
				if (mod == MOD_GRENADE)
				{
					message = "was popped by a";
					message2 = "'s grenade";
				}
				else if (mod == MOD_G_SPLASH)
				{
					message = "was shredded by a";
					message2 = "'s shrapnel";
				}
				else
					message = "was machinegunned by a";
				common_name = "gunner";
			}
			// Berserker
			else if (!strcmp(attacker->classname, "monster_berserk"))
			{
				message = "was smashed by a";
				common_name = "berserk";
			}
			// Gladiator
			else if (!strcmp(attacker->classname, "monster_gladiator"))
			{
				if (mod == MOD_RAILGUN)
					message = "was railed by a";
				else
				{
					message = "was mangled by a";
					message2 = "'s claw";
				}
				common_name = "gladiator";
			}
			// Medic
			else if (!strcmp(attacker->classname, "monster_medic"))
			{
				message = "was blasted by a";
				common_name = "medic";
			}
			// Icarus
			else if (!strcmp(attacker->classname, "monster_hover"))
			{
				message = "was blasted by a";
				common_name = "hover";
			}
			// Iron Maiden
			else if (!strcmp(attacker->classname, "monster_chick"))
			{
				if (mod == MOD_ROCKET)
				{
					message = "ate a";
					message2 = "'s rocket";
				}
				else if (mod == MOD_R_SPLASH)
				{
					message = "almost dodged a";
					message2 = "'s rocket";
				}
				else if (mod == MOD_HIT)
					message = "was bitch-slapped by a";
				common_name = "chick";
			}
			// Parasite
			else if (!strcmp(attacker->classname, "monster_parasite"))
			{
				message = "was exsanguiated by a";
				common_name = "parasite";
			}
			// Brain
			else if (!strcmp(attacker->classname, "monster_brain"))
			{
				message = "was torn up by a";
				message2 = "'s tentacles";
				common_name = "brain";
			}
			// Flyer
			else if (!strcmp(attacker->classname, "monster_flyer"))
			{
				if (mod == MOD_BLASTER)
					message = "was blasted by a";
				else if (mod == MOD_HIT)
				{
					message = "was cut up by a";
					message2 = "'s sharp wings";
				}
				common_name = "flyer";
			}
			// Technician
			else if (!strcmp(attacker->classname, "monster_floater"))
			{
				if (mod == MOD_BLASTER)
					message = "was blasted by a";
				else if (mod == MOD_HIT)
				{
					message = "was gouged to death by a";
					message2 = "'s utility claw";
				}
				common_name = "floater";
			}
			// Tank/Tank Commander
			else if (!strcmp(attacker->classname, "monster_tank")
				|| !strcmp(attacker->classname, "monster_tank_commander"))
			{
				if (mod == MOD_BLASTER)
					message = "was blasted by a";
				else if (mod == MOD_ROCKET)
				{
					message = "ate a";
					message2 = "'s rocket";
				}
				else if (mod == MOD_R_SPLASH)
				{
					message = "almost dodged a";
					message2 = "'s rocket";
				}
				else
					message = "was pumped full of lead by a";
				if (!strcmp(attacker->classname, "monster_tank"))
					common_name = "tank";
				else
					common_name = "tank commander";
			}
			// Supertank
			else if (!strcmp(attacker->classname, "monster_supertank"))
			{
				if (mod == MOD_ROCKET)
				{
					message = "ate a";
					message2 = "'s rocket";
				}
				else if (mod == MOD_R_SPLASH)
				{
					message = "almost dodged a";
					message2 = "'s rocket";
				}
				else
					message = "was chaingunned by a";
				common_name = "supertank";
			}
			// Hornet
			else if (!strcmp(attacker->classname, "monster_boss2"))
			{
				if (mod == MOD_ROCKET)
				{
					message = "ate a";
					message2 = "'s rocket";
				}
				else if (mod == MOD_R_SPLASH)
				{
					message = "almost dodged a";
					message2 = "'s rockets";
				}
				else
					message = "was chaingunned by a";
				common_name = "Hornet";
			}
			// Jorg
			else if (!strcmp(attacker->classname, "monster_jorg"))
			{
				if (mod == MOD_BFG_LASER)
				{
					message = "saw the pretty lights from the";
					message2 = "'s BFG";
				}
				else if (mod == MOD_BFG_BLAST)
				{
					message = "was disintegrated by the";
					message2 = "'s BFG blast";
				}
				else if (mod == MOD_BFG_EFFECT)
				{
					message = "couldn't hide from the";
					message2 = "'s BFG";
				}
				else
				{
					message = "was shredded by the";
					message = "'s chain-cannons";
				}
				common_name = "Jorg";
			}
			// Makron
			else if (!strcmp(attacker->classname, "monster_makron"))
			{
				if (mod == MOD_BLASTER)
				{
					message = "was melted by the";
					message = "'s hyperblaster";
				}
				else if (mod == MOD_RAILGUN)
					message = "was railed by the";
				else if (mod == MOD_BFG_LASER)
				{
					message = "saw the pretty lights from the";
					message2 = "'s BFG";
				}
				else if (mod == MOD_BFG_BLAST)
				{
					message = "was disintegrated by the";
					message2 = "'s BFG blast";
				}
				else if (mod == MOD_BFG_EFFECT)
				{
					message = "couldn't hide from the";
					message2 = "'s BFG";
				}
				common_name = "Makron";
			}
			// Barracuda Shark
			else if (!strcmp(attacker->classname, "monster_flipper"))
			{
				message = "was chewed up by a";
				common_name = "flipper";
			}
			// Mutant
			else if (!strcmp(attacker->classname, "monster_mutant"))
			{
				message = "was clawed by a";
				common_name = "mutant";
			}
		}
		if (message)
		{
			gi.bprintf (PRINT_MEDIUM,"%s %s %s%s\n", self->client->pers.netname, message, common_name, message2);
			if (coop->value)
				self->client->resp.score--;
			self->enemy = NULL;
			return;
		}
	}

	gi.bprintf (PRINT_MEDIUM,"%s died.\n", self->client->pers.netname);
	if (deathmatch->value)
		self->client->resp.score--;
}


void TossClientWeapon (edict_t *self)
{
	gitem_t		*item;
	edict_t		*drop;
	bool		quad;
	float		spread;

	if (!deathmatch->value)
		return;

	item = self->client->pers.weapon;
	if (! self->client->pers.inventory[self->client->ammo_index] )
		item = NULL;
	if (item && (strcmp (item->pickup_name, "Blaster") == 0))
		item = NULL;

	if (!((int)(dmflags->value) & DF_QUAD_DROP))
		quad = false;
	else
		quad = (self->client->quad_framenum > (level.framenum + 10));

	if (item && quad)
		spread = 22.5;
	else
		spread = 0.0;

	if (item)
	{
		self->client->v_angle[YAW] -= spread;
		drop = Drop_Item (self, item);
		self->client->v_angle[YAW] += spread;
		drop->spawnflags = DROPPED_PLAYER_ITEM;
	}

	if (quad)
	{
		self->client->v_angle[YAW] += spread;
		drop = Drop_Item (self, FindItemByClassname ("item_quad"));
		self->client->v_angle[YAW] -= spread;
		drop->spawnflags |= DROPPED_PLAYER_ITEM;

		drop->touch = Touch_Item;
		drop->nextthink = level.time + (self->client->quad_framenum - level.framenum) * FRAMETIME;
		drop->think = G_FreeEdict;
	}
}


void TossClientAmmo (edict_t *self)
{
	edict_t		*drop;
	gitem_t		*item;
	bool		quad;
	float		spread;

	if (!deathmatch->value)
		return;

	item = self->client->pers.weapon;
	if (! self->client->pers.inventory[self->client->ammo_index] )
		item = NULL;
	if (item && (strcmp (item->pickup_name, "Blaster") == 0))
		item = NULL;
	if (item && (strcmp (item->pickup_name, "Grenades") == 0))
		item = NULL;

	if (!((int)(dmflags->value) & DF_QUAD_DROP))
		quad = false;
	else
		quad = (self->client->quad_framenum > (level.framenum + 10));

	if (item && quad)
		spread = 22.5;
	else
		spread = 0.0;

	self->client->v_angle[YAW] -= spread;
	drop = Drop_Item (self, FindItemByClassname ("item_pack"));
	self->client->v_angle[YAW] += spread;
	drop->spawnflags = DROPPED_PLAYER_ITEM | DROPPED_PLAYER_PACK;

	drop->weapon = item;
	drop->ammo_bullets = self->client->pers.inventory[ITEM_INDEX(FindItem ("Bullets"))];
	drop->ammo_cells = self->client->pers.inventory[ITEM_INDEX(FindItem ("Cells"))];
	drop->ammo_grenades = self->client->pers.inventory[ITEM_INDEX(FindItem ("Grenades"))];
	drop->ammo_rockets = self->client->pers.inventory[ITEM_INDEX(FindItem ("Rockets"))];
	drop->ammo_shells = self->client->pers.inventory[ITEM_INDEX(FindItem ("Shells"))];
	drop->ammo_slugs = self->client->pers.inventory[ITEM_INDEX(FindItem ("Slugs"))];

	if (quad)
	{
		self->client->v_angle[YAW] += spread;
		drop = Drop_Item (self, FindItemByClassname ("item_quad"));
		self->client->v_angle[YAW] -= spread;
		drop->spawnflags |= DROPPED_PLAYER_ITEM;

		drop->touch = Touch_Item;
		drop->nextthink = level.time + (self->client->quad_framenum - level.framenum) * FRAMETIME;
		drop->think = G_FreeEdict;
	}
}


void DeathmatchScoreboardMessage (edict_t *ent, edict_t *killer)
{
	char	entry[1024];
	char	string[1400];
	int		stringlength;
	int		i, j, k;
	int		sorted[MAX_CLIENTS];
	int		sortedscores[MAX_CLIENTS];
	int		score, total;
//	int		picnum;
	int		x, y;
	gclient_t	*cl;
	edict_t		*cl_ent;
	char	*tag;

	// sort the clients by score
	total = 0;
	for (i=0 ; i<game.maxclients ; i++)
	{
		cl_ent = g_edicts + 1 + i;
		if (!cl_ent->inuse || game.clients[i].resp.spectator)
			continue;
		score = game.clients[i].resp.score;
		for (j=0 ; j<total ; j++)
		{
			if (score > sortedscores[j])
				break;
		}
		for (k=total ; k>j ; k--)
		{
			sorted[k] = sorted[k-1];
			sortedscores[k] = sortedscores[k-1];
		}
		sorted[j] = i;
		sortedscores[j] = score;
		total++;
	}

	// print level name and exit rules
	string[0] = 0;

	stringlength = strlen(string);

	// add the clients in sorted order
	if (total > 12)
		total = 12;

	for (i=0 ; i<total ; i++)
	{
		cl = &game.clients[sorted[i]];
		cl_ent = g_edicts + 1 + sorted[i];

//		picnum = gi.imageindex ("i_fixme");
		x = (i>=6) ? 160 : 0;
		y = 32 + 32 * (i%6);

		// add a dogtag
		if (cl_ent == ent)
			tag = "tag1";
		else if (cl_ent == killer)
			tag = "tag2";
		else
			tag = NULL;
		if (tag)
		{
			Com_sprintf (entry, sizeof(entry),
				"xv %i yv %i picn %s ",x+32, y, tag);
			j = strlen(entry);
			if (stringlength + j > 1024)
				break;
			strcpy (string + stringlength, entry);
			stringlength += j;
		}

		// send the layout
		Com_sprintf (entry, sizeof(entry),
			"client %i %i %i %i %i %i ",
			x, y, sorted[i], cl->resp.score, cl->ping, (level.framenum - cl->resp.enterframe)/600);
		j = strlen(entry);
		if (stringlength + j > 1024)
			break;
		strcpy (string + stringlength, entry);
		stringlength += j;
	}

	gi.WriteByte (svc_layout);
	gi.WriteString (string);
}


void DeathmatchScoreboard (edict_t *ent)
{
	DeathmatchScoreboardMessage (ent, ent->enemy);
	gi.unicast (ent, true);
}


/*
==================
Cmd_Score_f

Display the scoreboard
==================
*/
void Cmd_Score_f (edict_t *ent)
{
	ent->client->showinventory = false;
	ent->client->showhelp = false;

	if (!deathmatch->value && !coop->value)
		return;

	if (ent->client->showscores)
	{
		ent->client->showscores = false;
		return;
	}

	ent->client->showscores = true;
	DeathmatchScoreboard (ent);
}


/*
==================
HelpComputer

Draw help computer.
==================
*/
void HelpComputer (edict_t *ent)
{
	char	string[1024];
	char	*sk;

	if (skill->value == 0)
		sk = "easy";
	else if (skill->value == 1)
		sk = "medium";
	else if (skill->value == 2)
		sk = "hard";
	else
		sk = "hard+";

	// send the layout
	Com_sprintf (string, sizeof(string),
		"xv 32 yv 8 picn help "			// background
		"xv 202 yv 12 string2 \"%s\" "		// skill
		"xv 0 yv 24 cstring2 \"%s\" "		// level name
		"xv 0 yv 54 cstring2 \"%s\" "		// help 1
		"xv 0 yv 110 cstring2 \"%s\" "		// help 2
		"xv 50 yv 164 string2 \" kills     goals    secrets\" "
		"xv 50 yv 172 string2 \"%3i/%3i     %i/%i       %i/%i\" ",
		sk,
		level.level_name,
		game.helpmessage1,
		game.helpmessage2,
		level.killed_monsters, level.total_monsters,
		level.found_goals, level.total_goals,
		level.found_secrets, level.total_secrets);

	gi.WriteByte (svc_layout);
	gi.WriteString (string);
	gi.unicast (ent, true);
}


/*
==================
Cmd_Help_f

Display the current help message
==================
*/
void Cmd_Help_f (edict_t *ent)
{
	// this is for backwards compatability
	if (deathmatch->value)
	{
		Cmd_Score_f (ent);
		return;
	}

	ent->client->showinventory = false;
	ent->client->showscores = false;

	if (ent->client->showhelp && (ent->client->pers.game_helpchanged == game.helpchanged))
	{
		ent->client->showhelp = false;
		return;
	}

	ent->client->showhelp = true;
	ent->client->pers.helpchanged = 0;
	HelpComputer (ent);
}


void player_die (edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point)
{
	int		n;

	VectorClear (self->avelocity);

	self->takedamage = DAMAGE_YES;
	if(coop->value || deathmatch->value)
		self->movetype = MOVETYPE_TOSS_DM;
	else
		self->movetype = MOVETYPE_TOSS;

	self->s.modelindex2 = 0;	// remove linked weapon model

	self->s.angles[0] = 0;
	self->s.angles[2] = 0;

	self->s.sound = 0;
	self->client->weapon_sound = 0;

	self->maxs[2] = -8;

	self->svflags |= SVF_DEADMONSTER;

	if (!self->deadflag)
	{
		self->client->respawn_time = level.time + 1.0;
		LookAtKiller (self, inflictor, attacker);
		self->client->ps.pmove.pm_type = PM_DEAD;
		ClientObituary (self, inflictor, attacker);

		if((int)dmflags->value & DF_DROP_WEAPON)
			TossClientWeapon (self);
		else
			TossClientAmmo (self);

		if (deathmatch->value)
			Cmd_Help_f (self);		// show scores

		// clear inventory
		// this is kind of ugly, but it's how we want to handle keys in coop
		for (n = 0; n < game.num_items; n++)
		{
			if (coop->value && itemlist[n].flags & IT_KEY)
				self->client->resp.coop_respawn.inventory[n] = self->client->pers.inventory[n];
			self->client->pers.inventory[n] = 0;
		}
	}

	// remove powerups
	self->client->quad_framenum = 0;
	self->client->invincible_framenum = 0;
	self->client->invisible_framenum = 0;
	self->flags &= ~(FL_POWER_ARMOR|FL_FLASHLIGHT);
	self->client->enviro_framenum = 0;
	self->client->breather_framenum = 0;
	self->client->flashlight_framenum = 0;
	if(!net_compatibility->value)
	{
		gi.WriteByte (svc_rfx);
		gi.WriteByte (RFX_DISABLE_ALL);		// Выключаем все эффекты
		gi.unicast (self, true);
	}

	if (self->health < -40)
	{	// gib
		gi.sound (self, CHAN_BODY, gi.soundindex ("misc/udeath.wav"), 1, ATTN_NORM, 0);
		if(net_compatibility->value || !net_fixoverflow->value)
		{
			for (n= 0; n < 4; n++)
				ThrowGib (self, "models/objects/gibs/sm_meat/tris.md2", damage, GIB_ORGANIC);
			ThrowClientHead (self, damage, false);
		}
		else
		{
			NetThrowMonsterGibsExt (self, GIBS_PLAYER, damage);
			ThrowClientHead (self, damage, true);
		}
		self->takedamage = DAMAGE_NO;
	}
	else
	{	// normal death
		if (!self->deadflag)
		{
			static int i;

			i = (i+1)%3;
			// start a death animation
			self->client->anim_priority = ANIM_DEATH;
			if (self->client->ps.pmove.pm_flags & PMF_DUCKED)
			{
				self->s.frame = PLAYER_FRAME_crdeath1-1;
				self->client->anim_end = PLAYER_FRAME_crdeath5;
			}
			else switch (i)
			{
			case 0:
				self->s.frame = PLAYER_FRAME_death101-1;
				self->client->anim_end = PLAYER_FRAME_death106;
				break;
			case 1:
				self->s.frame = PLAYER_FRAME_death201-1;
				self->client->anim_end = PLAYER_FRAME_death206;
				break;
			case 2:
				self->s.frame = PLAYER_FRAME_death301-1;
				self->client->anim_end = PLAYER_FRAME_death308;
				break;
			}
			gi.sound (self, CHAN_VOICE, gi.soundindex(va("*death%i.wav", (rand()%4)+1)), 1, ATTN_NORM, 0);
		}
	}

	self->deadflag = DEAD_DEAD;

	gi.linkentity (self);
}


/// Berserker: также меняет forward
void P_ProjectSource(edict_t *ent, vec3_t distance, vec3_t forward, vec3_t right, vec3_t result)
{
	gclient_t	*client = ent->client;
	float		*point = ent->s.origin;
	vec3_t		_distance;

	VectorCopy (distance, _distance);
	if (client->pers.hand == LEFT_HANDED)
		_distance[1] *= -1;
	else if (client->pers.hand == CENTER_HANDED)
		_distance[1] = 0;
	G_ProjectSource (point, _distance, forward, right, result);

	char *value = Info_ValueForKey(client->pers.userinfo, "weaponHitAccuracy");
	if (!value)
		return;
	if (!atoi(value))
		return;

	/// Berserker: fix - теперь заряд попадает точно туда, куда показывает прицел  ;p
	vec3_t	start, end;
	VectorSet(start, ent->s.origin[0], ent->s.origin[1], ent->s.origin[2] + ent->viewheight);
	VectorMA(start, 32768, forward, end);		/// 8192 for Q2 maps, 32768 for large maps support
	trace_t	tr = gi.trace(start, NULL, NULL, end, ent, MASK_SHOT);
	if (tr.fraction < 1)
	{
		VectorSubtract(tr.endpos, result, forward);
		VectorNormalize(forward);
	}
}


/*
=================
fire_lead

This is an internal support routine used for bullet/pellet based weapons.
=================
*/
static void fire_lead (edict_t *self, vec3_t start, vec3_t aimdir, int damage, int kick, int te_impact, int hspread, int vspread, int mod)
{
	trace_t		tr;
	vec3_t		dir;
	vec3_t		forward, right, up;
	vec3_t		end;
	float		r;
	float		u;
	vec3_t		water_start;
	bool		water = false;
	int			content_mask = MASK_SHOT | MASK_WATER;

	tr = gi.trace (self->s.origin, NULL, NULL, start, self, MASK_SHOT);
	if (!(tr.fraction < 1.0))
	{
		vectoangles (aimdir, dir);
		AngleVectors (dir, forward, right, up);

		r = crandom()*hspread;
		u = crandom()*vspread;
		VectorMA (start, 8192, forward, end);
		VectorMA (end, r, right, end);
		VectorMA (end, u, up, end);

		if (gi.pointcontents (start) & MASK_WATER)
		{
			water = true;
			VectorCopy (start, water_start);
			content_mask &= ~MASK_WATER;
		}

		tr = gi.trace (start, NULL, NULL, end, self, content_mask);

		// see if we hit water
		if (tr.contents & MASK_WATER)
		{
			int		color;

			water = true;
			VectorCopy (tr.endpos, water_start);

			if (!VectorCompare (start, tr.endpos))
			{
				if (tr.contents & CONTENTS_WATER)
				{
					if (strcmp(tr.surface->name, "*brwater") == 0)
						color = SPLASH_BROWN_WATER;
					else
						color = SPLASH_BLUE_WATER;
				}
				else if (tr.contents & CONTENTS_SLIME)
					color = SPLASH_SLIME;
				else if (tr.contents & CONTENTS_LAVA)
					color = SPLASH_LAVA;
				else
					color = SPLASH_UNKNOWN;

				if (color != SPLASH_UNKNOWN)
				{
					bool large_map = LargeMapCheck(tr.endpos);
					if (large_map)
						gi.WriteByte (svc_temp_entity_ext);
					else
						gi.WriteByte (svc_temp_entity);
					gi.WriteByte (TE_SPLASH);
					gi.WriteByte (8);
					gi.WritePosition (tr.endpos, large_map);
					gi.WriteDir (tr.plane.normal);
					gi.WriteByte (color);
					gi.multicast (tr.endpos, MULTICAST_PVS);	/// FIXME: заменить на MULTICAST_ALL, тогда будут видны следы от пуль, сделанные в отдаленных областях. (MULTICAST_ALL - для долгоживущих DECALS)
				}												/// (спорно: чтоб сеть не грузить, можно не фиксить! (и не так долго следы от пуль живут))

				// change bullet's course when it enters water
				VectorSubtract (end, start, dir);
				vectoangles (dir, dir);
				AngleVectors (dir, forward, right, up);
				r = crandom()*hspread*2;
				u = crandom()*vspread*2;
				VectorMA (water_start, 8192, forward, end);
				VectorMA (end, r, right, end);
				VectorMA (end, u, up, end);
			}

			// re-trace ignoring water this time
			tr = gi.trace (water_start, NULL, NULL, end, self, MASK_SHOT);
		}
	}

	// send gun puff / flash
	if (!((tr.surface) && (tr.surface->flags & SURF_SKY)))
	{
		if (tr.fraction < 1.0)
		{
			if (tr.ent->takedamage)
			{
				T_Damage (tr.ent, self, self, aimdir, tr.endpos, tr.plane.normal, damage, kick, DAMAGE_BULLET, mod);
			}
			else
			{
				if (strncmp (tr.surface->name, "sky", 3) != 0)
				{
					bool large_map = LargeMapCheck(tr.endpos);
					if (large_map)
						gi.WriteByte (svc_temp_entity_ext);
					else
						gi.WriteByte (svc_temp_entity);
					gi.WriteByte (te_impact);
					gi.WritePosition (tr.endpos, large_map);
					gi.WriteDir (tr.plane.normal);
					gi.multicast (tr.endpos, MULTICAST_PVS);

					if (self->client)
						PlayerNoise(self, tr.endpos, PNOISE_IMPACT);
				}
			}
		}
	}

	// if went through water, determine where the end and make a bubble trail
	if (water)
	{
		int		cont;
		vec3_t	pos;

		VectorSubtract (tr.endpos, water_start, dir);
		VectorNormalize (dir);
		VectorMA (tr.endpos, -2, dir, pos);
		cont = gi.pointcontents (pos) & MASK_WATER;			/// BERSERKER: добавил различие по типу жидкости - пузыри соотв.цвета
		if (cont)
			VectorCopy (pos, tr.endpos);
		else
		{
			tr = gi.trace (pos, NULL, NULL, water_start, tr.ent, MASK_WATER);
			if(!net_compatibility->value)
				cont = gi.pointcontents (water_start) & MASK_WATER;
		}

		VectorAdd (water_start, tr.endpos, pos);
		VectorScale (pos, 0.5, pos);

		byte con;
		if(net_compatibility->value)
			con = TE_BUBBLETRAIL_WATER;
		else
		{
			if(cont&CONTENTS_WATER)
				con = TE_BUBBLETRAIL_WATER;
			else if(cont&CONTENTS_LAVA)
				con = TE_BUBBLETRAIL_LAVA;
			else if(cont&CONTENTS_SLIME)
				con = TE_BUBBLETRAIL_SLIME;
			else
				return;
		}

		bool large_map = LargeMapCheck(water_start) | LargeMapCheck(tr.endpos);
		if (large_map)
			gi.WriteByte (svc_temp_entity_ext);
		else
			gi.WriteByte (svc_temp_entity);
		gi.WriteByte (con);
		gi.WritePosition (water_start, large_map);
		gi.WritePosition (tr.endpos, large_map);
		gi.multicast (pos, MULTICAST_PVS);
	}
}


/*
=================
fire_shotgun

Shoots shotgun pellets.  Used by shotgun and super shotgun.
=================
*/
void fire_shotgun (edict_t *self, vec3_t start, vec3_t aimdir, int damage, int kick, int hspread, int vspread, int count, int mod)
{
	int		i;

	for (i = 0; i < count; i++)
		fire_lead (self, start, aimdir, damage, kick, TE_SHOTGUN, hspread, vspread, mod);
}


/*
=================
fire_bullet

Fires a single round.  Used for machinegun and chaingun.  Would be fine for
pistols, rifles, etc....
=================
*/
void fire_bullet (edict_t *self, vec3_t start, vec3_t aimdir, int damage, int kick, int hspread, int vspread, int mod)
{
	fire_lead (self, start, aimdir, damage, kick, TE_GUNSHOT, hspread, vspread, mod);
}


/*
======================================================================

SHOTGUN / SUPERSHOTGUN

======================================================================
*/
void weapon_shotgun_fire (edict_t *ent)
{
	vec3_t		start;
	vec3_t		forward, right;
	vec3_t		offset;
	int			damage = 4;
	int			kick = 8;

	if (ent->client->ps.gunframe == 9)
	{
		ent->client->ps.gunframe++;
		return;
	}

	AngleVectors (ent->client->v_angle, forward, right, NULL);

	VectorScale (forward, -2, ent->client->kick_origin);
	ent->client->kick_angles[0] = -2;

	VectorSet(offset, 0, 8,  ent->viewheight-8);
	P_ProjectSource (ent, offset, forward, right, start);

	if (is_quad)
	{
		damage *= 4;
		kick *= 4;
	}

	if (deathmatch->value)
		fire_shotgun (ent, start, forward, damage, kick, 500, 500, DEFAULT_DEATHMATCH_SHOTGUN_COUNT, MOD_SHOTGUN);
	else
		fire_shotgun (ent, start, forward, damage, kick, 500, 500, DEFAULT_SHOTGUN_COUNT, MOD_SHOTGUN);

	// send muzzle flash
	gi.WriteByte (svc_muzzleflash);
	gi.WriteShort (ent-g_edicts);
	gi.WriteByte (MZ_SHOTGUN | is_silenced);
	gi.multicast (ent->s.origin, MULTICAST_PVS);

	ent->client->ps.gunframe++;
	PlayerNoise(ent, start, PNOISE_WEAPON);

	if (! ( (int)dmflags->value & DF_INFINITE_AMMO ) )
		ent->client->pers.inventory[ent->client->ammo_index]--;
}

void Weapon_Shotgun (edict_t *ent)
{
	static int	pause_frames[]	= {22, 28, 34, 0};
	static int	fire_frames[]	= {8, 9, 0};

	Weapon_Generic (ent, 7, 18, 36, 39, pause_frames, fire_frames, weapon_shotgun_fire);
}


void weapon_supershotgun_fire (edict_t *ent)
{
	vec3_t		start;
	vec3_t		forward, right;
	vec3_t		offset;
	vec3_t		v;
	int			damage = 6;
	int			kick = 12;

	AngleVectors (ent->client->v_angle, forward, right, NULL);

	VectorScale (forward, -2, ent->client->kick_origin);
	ent->client->kick_angles[0] = -2;

	VectorSet(offset, 0, 8,  ent->viewheight-8);
	P_ProjectSource (ent, offset, forward, right, start);

	if (is_quad)
	{
		damage *= 4;
		kick *= 4;
	}

	v[PITCH] = ent->client->v_angle[PITCH];
	v[YAW]   = ent->client->v_angle[YAW] - 5;
	v[ROLL]  = ent->client->v_angle[ROLL];
	AngleVectors (v, forward, NULL, NULL);
	fire_shotgun (ent, start, forward, damage, kick, DEFAULT_SHOTGUN_HSPREAD, DEFAULT_SHOTGUN_VSPREAD, DEFAULT_SSHOTGUN_COUNT/2, MOD_SSHOTGUN);
	v[YAW]   = ent->client->v_angle[YAW] + 5;
	AngleVectors (v, forward, NULL, NULL);
	fire_shotgun (ent, start, forward, damage, kick, DEFAULT_SHOTGUN_HSPREAD, DEFAULT_SHOTGUN_VSPREAD, DEFAULT_SSHOTGUN_COUNT/2, MOD_SSHOTGUN);

	// send muzzle flash
	gi.WriteByte (svc_muzzleflash);
	gi.WriteShort (ent-g_edicts);
	gi.WriteByte (MZ_SSHOTGUN | is_silenced);
	gi.multicast (ent->s.origin, MULTICAST_PVS);

	ent->client->ps.gunframe++;
	PlayerNoise(ent, start, PNOISE_WEAPON);

	if (! ( (int)dmflags->value & DF_INFINITE_AMMO ) )
		ent->client->pers.inventory[ent->client->ammo_index] -= 2;
}

void Weapon_SuperShotgun (edict_t *ent)
{
	static int	pause_frames[]	= {29, 42, 57, 0};
	static int	fire_frames[]	= {7, 0};

	Weapon_Generic (ent, 6, 17, 57, 61, pause_frames, fire_frames, weapon_supershotgun_fire);
}


/*
======================================================================

MACHINEGUN / CHAINGUN

======================================================================
*/

void Machinegun_Fire (edict_t *ent)
{
	int	i;
	vec3_t		start;
	vec3_t		forward, right;
	vec3_t		angles;
	int			damage = 8;
	int			kick = 2;
	vec3_t		offset;

	if (!(ent->client->buttons & BUTTON_ATTACK))
	{
		ent->client->machinegun_shots = 0;
		ent->client->ps.gunframe++;
		return;
	}

	if (ent->client->ps.gunframe == 5)
		ent->client->ps.gunframe = 4;
	else
		ent->client->ps.gunframe = 5;

	if (ent->client->pers.inventory[ent->client->ammo_index] < 1)
	{
		ent->client->ps.gunframe = 6;
		if (level.time >= ent->pain_debounce_time)
		{
			gi.sound(ent, CHAN_VOICE, gi.soundindex("weapons/noammo.wav"), 1, ATTN_NORM, 0);
			ent->pain_debounce_time = level.time + 1;
		}
		NoAmmoWeaponChange (ent);
		return;
	}

	if (is_quad)
	{
		damage *= 4;
		kick *= 4;
	}

	for (i=1 ; i<3 ; i++)
	{
		ent->client->kick_origin[i] = crandom() * 0.35;
		ent->client->kick_angles[i] = crandom() * 0.7;
	}
	ent->client->kick_origin[0] = crandom() * 0.35;
	ent->client->kick_angles[0] = ent->client->machinegun_shots * -1.5;

	// raise the gun as it is firing
	if (!deathmatch->value)
	{
		ent->client->machinegun_shots++;
		if (ent->client->machinegun_shots > 9)
			ent->client->machinegun_shots = 9;
	}

	// get start / end positions
	VectorAdd (ent->client->v_angle, ent->client->kick_angles, angles);
	AngleVectors (angles, forward, right, NULL);
	VectorSet(offset, 0, 8, ent->viewheight-8);
	P_ProjectSource (ent, offset, forward, right, start);
	fire_bullet (ent, start, forward, damage, kick, DEFAULT_BULLET_HSPREAD, DEFAULT_BULLET_VSPREAD, MOD_MACHINEGUN);

	gi.WriteByte (svc_muzzleflash);
	gi.WriteShort (ent-g_edicts);
	gi.WriteByte (MZ_MACHINEGUN | is_silenced);
	gi.multicast (ent->s.origin, MULTICAST_PVS);

	PlayerNoise(ent, start, PNOISE_WEAPON);

	if (! ( (int)dmflags->value & DF_INFINITE_AMMO ) )
		ent->client->pers.inventory[ent->client->ammo_index]--;

	ent->client->anim_priority = ANIM_ATTACK;
	if (ent->client->ps.pmove.pm_flags & PMF_DUCKED)
	{
		ent->s.frame = PLAYER_FRAME_crattak1 - (int) (random()+0.25);
		ent->client->anim_end = PLAYER_FRAME_crattak9;
	}
	else
	{
		ent->s.frame = PLAYER_FRAME_attack1 - (int) (random()+0.25);
		ent->client->anim_end = PLAYER_FRAME_attack8;
	}
}

void Weapon_Machinegun (edict_t *ent)
{
	static int	pause_frames[]	= {23, 45, 0};
	static int	fire_frames[]	= {4, 5, 0};

	Weapon_Generic (ent, 3, 5, 45, 49, pause_frames, fire_frames, Machinegun_Fire);
}

void Chaingun_Fire (edict_t *ent)
{
	int			i;
	int			shots;
	vec3_t		start;
	vec3_t		forward, right, up;
	float		r, u;
	vec3_t		offset;
	int			damage;
	int			kick = 2;

	if (deathmatch->value)
		damage = 6;
	else
		damage = 8;

	if (ent->client->ps.gunframe == 5)
		gi.sound(ent, CHAN_AUTO, gi.soundindex("weapons/chngnu1a.wav"), 1, ATTN_IDLE, 0);

	if ((ent->client->ps.gunframe == 14) && !(ent->client->buttons & BUTTON_ATTACK))
	{
		ent->client->ps.gunframe = 32;
		ent->client->weapon_sound = 0;
		return;
	}
	else if ((ent->client->ps.gunframe == 21) && (ent->client->buttons & BUTTON_ATTACK)
		&& ent->client->pers.inventory[ent->client->ammo_index])
	{
		ent->client->ps.gunframe = 15;
	}
	else
	{
		ent->client->ps.gunframe++;
	}

	if (ent->client->ps.gunframe == 22)
	{
		ent->client->weapon_sound = 0;
		gi.sound(ent, CHAN_AUTO, gi.soundindex("weapons/chngnd1a.wav"), 1, ATTN_IDLE, 0);
	}
	else
	{
		ent->client->weapon_sound = gi.soundindex("weapons/chngnl1a.wav");
	}

	ent->client->anim_priority = ANIM_ATTACK;
	if (ent->client->ps.pmove.pm_flags & PMF_DUCKED)
	{
		ent->s.frame = PLAYER_FRAME_crattak1 - (ent->client->ps.gunframe & 1);
		ent->client->anim_end = PLAYER_FRAME_crattak9;
	}
	else
	{
		ent->s.frame = PLAYER_FRAME_attack1 - (ent->client->ps.gunframe & 1);
		ent->client->anim_end = PLAYER_FRAME_attack8;
	}

	if (ent->client->ps.gunframe <= 9)
		shots = 1;
	else if (ent->client->ps.gunframe <= 14)
	{
		if (ent->client->buttons & BUTTON_ATTACK)
			shots = 2;
		else
			shots = 1;
	}
	else
		shots = 3;

	if (ent->client->pers.inventory[ent->client->ammo_index] < shots)
		shots = ent->client->pers.inventory[ent->client->ammo_index];

	if (!shots)
	{
		if (level.time >= ent->pain_debounce_time)
		{
			gi.sound(ent, CHAN_VOICE, gi.soundindex("weapons/noammo.wav"), 1, ATTN_NORM, 0);
			ent->pain_debounce_time = level.time + 1;
		}
		NoAmmoWeaponChange (ent);
		return;
	}

	if (is_quad)
	{
		damage *= 4;
		kick *= 4;
	}

	for (i=0 ; i<3 ; i++)
	{
		ent->client->kick_origin[i] = crandom() * 0.35;
		ent->client->kick_angles[i] = crandom() * 0.7;
	}

	for (i=0 ; i<shots ; i++)
	{
		// get start / end positions
		AngleVectors (ent->client->v_angle, forward, right, up);
		r = 7 + crandom()*4;
		u = crandom()*4;
		VectorSet(offset, 0, r, u + ent->viewheight-8);
		P_ProjectSource (ent, offset, forward, right, start);

		fire_bullet (ent, start, forward, damage, kick, DEFAULT_BULLET_HSPREAD, DEFAULT_BULLET_VSPREAD, MOD_CHAINGUN);
	}

	// send muzzle flash
	gi.WriteByte (svc_muzzleflash);
	gi.WriteShort (ent-g_edicts);
	gi.WriteByte ((MZ_CHAINGUN1 + shots - 1) | is_silenced);
	gi.multicast (ent->s.origin, MULTICAST_PVS);

	PlayerNoise(ent, start, PNOISE_WEAPON);

	if (! ( (int)dmflags->value & DF_INFINITE_AMMO ) )
		ent->client->pers.inventory[ent->client->ammo_index] -= shots;
}


void Weapon_Chaingun (edict_t *ent)
{
	static int	pause_frames[]	= {38, 43, 51, 61, 0};
	static int	fire_frames[]	= {5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 0};

	Weapon_Generic (ent, 4, 31, 61, 64, pause_frames, fire_frames, Chaingun_Fire);
}


/*
===============
PlayerNoise

Each player can have two noise objects associated with it:
a personal noise (jumping, pain, weapon firing), and a weapon
target noise (bullet wall impacts)

Monsters that don't directly see the player can move
to a noise in hopes of seeing the player from there.
===============
*/
void PlayerNoise(edict_t *who, vec3_t where, int type)
{
	edict_t		*noise;

	if (type == PNOISE_WEAPON)
	{
		if (who->client->silencer_shots)
		{
			who->client->silencer_shots--;
			return;
		}
	}

	if (deathmatch->value)
		return;

	if (who->flags & FL_NOTARGET)
		return;

	if (who->client)
		if (who->client->invisible_framenum > level.framenum)
			return;

	if (!who->mynoise)
	{
		noise = G_Spawn();
		noise->classname = "player_noise";
		VectorSet (noise->mins, -8, -8, -8);
		VectorSet (noise->maxs, 8, 8, 8);
		noise->owner = who;
		noise->svflags = SVF_NOCLIENT;
		who->mynoise = noise;

		noise = G_Spawn();
		noise->classname = "player_noise";
		VectorSet (noise->mins, -8, -8, -8);
		VectorSet (noise->maxs, 8, 8, 8);
		noise->owner = who;
		noise->svflags = SVF_NOCLIENT;
		who->mynoise2 = noise;
	}

	if (type == PNOISE_SELF || type == PNOISE_WEAPON)
	{
		noise = who->mynoise;
		level.sound_entity = noise;
		level.sound_entity_framenum = level.framenum;
	}
	else // type == PNOISE_IMPACT
	{
		noise = who->mynoise2;
		level.sound2_entity = noise;
		level.sound2_entity_framenum = level.framenum;
	}

	VectorCopy (where, noise->s.origin);
	VectorSubtract (where, noise->maxs, noise->absmin);
	VectorAdd (where, noise->maxs, noise->absmax);
	noise->teleport_time = level.time;
	gi.linkentity (noise);
}


void Grenade_Explode (edict_t *ent)
{
	vec3_t		origin;
	int			mod;

	if (ent->owner->client)
		PlayerNoise(ent->owner, ent->s.origin, PNOISE_IMPACT);

	//FIXME: if we are onground then raise our Z just a bit since we are a point?
	if (ent->enemy)
	{
		float	points;
		vec3_t	v;
		vec3_t	dir;

		VectorAdd (ent->enemy->mins, ent->enemy->maxs, v);
		VectorMA (ent->enemy->s.origin, 0.5, v, v);
		VectorSubtract (ent->s.origin, v, v);
		points = ent->dmg - 0.5 * VectorLength (v);
		VectorSubtract (ent->enemy->s.origin, ent->s.origin, dir);
		if (ent->spawnflags & 1)
			mod = MOD_HANDGRENADE;
		else
			mod = MOD_GRENADE;
		T_Damage (ent->enemy, ent, ent->owner, dir, ent->s.origin, vec3_origin, (int)points, (int)points, DAMAGE_RADIUS, mod);
	}

	if (ent->spawnflags & 2)
		mod = MOD_HELD_GRENADE;
	else if (ent->spawnflags & 1)
		mod = MOD_HG_SPLASH;
	else
		mod = MOD_G_SPLASH;
	T_RadiusDamage(ent, ent->owner, ent->dmg, ent->enemy, ent->dmg_radius, mod);

	VectorMA (ent->s.origin, -0.02, ent->velocity, origin);

	bool large_map = LargeMapCheck(origin);
	if (large_map)
		gi.WriteByte (svc_temp_entity_ext);
	else
		gi.WriteByte (svc_temp_entity);
	if (ent->waterlevel)
	{
		if (ent->groundentity)
			gi.WriteByte (TE_GRENADE_EXPLOSION_WATER);
		else
			gi.WriteByte (TE_ROCKET_EXPLOSION_WATER);
	}
	else
	{
		if (ent->groundentity)
			gi.WriteByte (TE_GRENADE_EXPLOSION);
		else
			gi.WriteByte (TE_ROCKET_EXPLOSION);
	}
	gi.WritePosition (origin, large_map);
////gi.WriteDir (vec3_origin);		/// FIXME: [0,0,1] ???
	gi.multicast (NULL/*ent->s.origin*/, MULTICAST_ALL/*PHS*/);	// О долгоживущих пятнах от взрыва должны "знать" все

	G_FreeEdict (ent);
}


void Grenade_Touch (edict_t *ent, edict_t *other, cplane_t *plane, csurface_t *surf)
{
	if (other == ent->owner)
		return;

	if (surf && (surf->flags & SURF_SKY))
	{
		G_FreeEdict (ent);
		return;
	}

	if (!other->takedamage)
	{
		if (ent->spawnflags & 1)
		{
			if (random() > 0.5)
				gi.sound (ent, CHAN_VOICE, gi.soundindex ("weapons/hgrenb1a.wav"), 1, ATTN_NORM, 0);
			else
				gi.sound (ent, CHAN_VOICE, gi.soundindex ("weapons/hgrenb2a.wav"), 1, ATTN_NORM, 0);
		}
		else
		{
			gi.sound (ent, CHAN_VOICE, gi.soundindex ("weapons/grenlb1b.wav"), 1, ATTN_NORM, 0);
		}
		return;
	}

	ent->enemy = other;
	Grenade_Explode (ent);
}


void fire_grenade (edict_t *self, vec3_t start, vec3_t aimdir, int damage, int speed, float timer, float damage_radius)
{
	edict_t	*grenade;
	vec3_t	dir;
	vec3_t	forward, right, up;

	vectoangles (aimdir, dir);
	AngleVectors (dir, forward, right, up);

	grenade = G_Spawn();
	VectorCopy (start, grenade->s.origin);
	VectorScale (aimdir, speed, grenade->velocity);
	VectorMA (grenade->velocity, 200 + crandom() * 10.0, up, grenade->velocity);
	VectorMA (grenade->velocity, crandom() * 10.0, right, grenade->velocity);
	VectorSet (grenade->avelocity, 300, 300, 300);
	grenade->movetype = MOVETYPE_BOUNCE;
	grenade->clipmask = MASK_SHOT;
	grenade->solid = SOLID_BBOX;
	grenade->s.effects |= EF_GRENADE;
	VectorClear (grenade->mins);
	VectorClear (grenade->maxs);
	grenade->s.modelindex = gi.modelindex ("models/objects/grenade/tris.md2");
	grenade->owner = self;
	grenade->touch = Grenade_Touch;
	grenade->nextthink = level.time + timer;
	grenade->think = Grenade_Explode;
	grenade->dmg = damage;
	grenade->dmg_radius = damage_radius;
	grenade->classname = "grenade";

	gi.linkentity (grenade);

/// Berserker:
/// Fixed bug: стрельба сквозь стены при помощи "сидячего" трупа =)
	if (self->client)
	{
		trace_t	tr;
		vec3_t	point;
		VectorMA (start, 16, aimdir, point);						// Отодвинем тестовую точку на 16 единиц в сторону стрельбы (на кончик оружия)
		tr = gi.trace (start, NULL, NULL, point, NULL, MASK_SOLID);	// Если тестовая точка находится в стене-полу-окне (не учитываем "живность" и "мясо")
		if (tr.fraction < 1 && !(tr.surface->flags & SURF_SKY))
			Grenade_Explode(grenade);								// то принудительно сдетонируем снаряд
	}
}


void fire_grenade2 (edict_t *self, vec3_t start, vec3_t aimdir, int damage, int speed, float timer, float damage_radius, bool held)
{
	edict_t	*grenade;
	vec3_t	dir;
	vec3_t	forward, right, up;

	vectoangles (aimdir, dir);
	AngleVectors (dir, forward, right, up);

	grenade = G_Spawn();
	VectorCopy (start, grenade->s.origin);
	VectorScale (aimdir, speed, grenade->velocity);
	VectorMA (grenade->velocity, 200 + crandom() * 10.0, up, grenade->velocity);
	VectorMA (grenade->velocity, crandom() * 10.0, right, grenade->velocity);
	VectorSet (grenade->avelocity, 300, 300, 300);
	grenade->movetype = MOVETYPE_BOUNCE;
	grenade->clipmask = MASK_SHOT;
	grenade->solid = SOLID_BBOX;
///	grenade->s.effects |= EF_GRENADE;		/// Berserker: уберем дым у лимонки
	VectorClear (grenade->mins);
	VectorClear (grenade->maxs);
	grenade->s.modelindex = gi.modelindex ("models/objects/grenade2/tris.md2");
	grenade->owner = self;
	grenade->touch = Grenade_Touch;
	grenade->nextthink = level.time + timer;
	grenade->think = Grenade_Explode;
	grenade->dmg = damage;
	grenade->dmg_radius = damage_radius;
	grenade->classname = "hgrenade";
	if (held)
		grenade->spawnflags = 3;
	else
		grenade->spawnflags = 1;
	grenade->s.sound = gi.soundindex("weapons/hgrenc1b.wav");

	if (timer <= 0.0)
		Grenade_Explode (grenade);
	else
	{
		gi.sound (self, CHAN_WEAPON, gi.soundindex ("weapons/hgrent1a.wav"), 1, ATTN_NORM, 0);
		gi.linkentity (grenade);

/// Berserker:
/// Fixed bug: стрельба сквозь стены при помощи "сидячего" трупа =)
		if (self->client)
		{
			trace_t	tr;
			vec3_t	point;
			VectorMA (start, 16, aimdir, point);						// Отодвинем тестовую точку на 16 единиц в сторону стрельбы (на кончик оружия)
			tr = gi.trace (start, NULL, NULL, point, NULL, MASK_SOLID);	// Если тестовая точка находится в стене-полу-окне (не учитываем "живность" и "мясо")
			if (tr.fraction < 1 && !(tr.surface->flags & SURF_SKY))
				Grenade_Explode(grenade);								// то принудительно сдетонируем снаряд
		}
	}
}


/*
===============
ChangeWeapon

The old weapon has been dropped all the way, so make the new one
current
===============
*/
void ChangeWeapon (edict_t *ent)
{
	int i;

	if (ent->client->grenade_time)
	{
		ent->client->grenade_time = level.time;
		ent->client->weapon_sound = 0;
		weapon_grenade_fire (ent, false);
		ent->client->grenade_time = 0;
	}

	ent->client->pers.lastweapon = ent->client->pers.weapon;
	ent->client->pers.weapon = ent->client->newweapon;
	ent->client->newweapon = NULL;
	ent->client->machinegun_shots = 0;

	// set visible model
	if (ent->s.modelindex == 255) {
		if (ent->client->pers.weapon)
			i = ((ent->client->pers.weapon->weapmodel & 0xff) << 8);
		else
			i = 0;
		ent->s.skinnum = (ent - g_edicts - 1) | i;
	}

	if (ent->client->pers.weapon && ent->client->pers.weapon->ammo)
		ent->client->ammo_index = ITEM_INDEX(FindItem(ent->client->pers.weapon->ammo));
	else
		ent->client->ammo_index = 0;

	if (!ent->client->pers.weapon)
	{	// dead
		ent->client->ps.gunindex = 0;
		return;
	}

	ent->client->weaponstate = WEAPON_ACTIVATING;
	ent->client->ps.gunframe = 0;
	ent->client->ps.gunindex = gi.modelindex(ent->client->pers.weapon->view_model);

	ent->client->anim_priority = ANIM_PAIN;
	if(ent->client->ps.pmove.pm_flags & PMF_DUCKED)
	{
			ent->s.frame = PLAYER_FRAME_crpain1;
			ent->client->anim_end = PLAYER_FRAME_crpain4;
	}
	else
	{
			ent->s.frame = PLAYER_FRAME_pain301;
			ent->client->anim_end = PLAYER_FRAME_pain304;

	}
}


/*
=============
infront

returns 1 if the entity is in front (in sight) of self
=============
*/
bool infront (edict_t *self, edict_t *other)
{
	vec3_t	vec;
	float	dot;
	vec3_t	forward;

	AngleVectors (self->s.angles, forward, NULL, NULL);
	VectorSubtract (other->s.origin, self->s.origin, vec);
	VectorNormalize (vec);
	dot = DotProduct (vec, forward);

	if (dot > 0.3)
		return true;
	return false;
}


/*
=================
check_dodge

This is a support routine used when a client is firing
a non-instant attack weapon.  It checks to see if a
monster's dodge function should be called.
=================
*/
static void check_dodge (edict_t *self, vec3_t start, vec3_t dir, int speed)
{
	vec3_t	end;
	vec3_t	v;
	trace_t	tr;
	float	eta;

	// easy mode only ducks one quarter the time
	if (skill->value == 0)
	{
		if (random() > 0.25)
			return;
	}
	VectorMA (start, 8192, dir, end);
	tr = gi.trace (start, NULL, NULL, end, self, MASK_SHOT);
	if ((tr.ent) && (tr.ent->svflags & SVF_MONSTER) && (tr.ent->health > 0) && (tr.ent->monsterinfo.dodge) && infront(tr.ent, self))
	{
		VectorSubtract (tr.endpos, start, v);
		eta = (VectorLength(v) - tr.ent->maxs[0]) / speed;
		tr.ent->monsterinfo.dodge (tr.ent, self, eta);
	}
}


/*
=================
fire_blaster

Fires a single blaster bolt.  Used by the blaster and hyper blaster.
=================
*/
void blaster_touch (edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf)
{
	int		mod;

	if (other == self->owner)
		return;

	if (surf && (surf->flags & SURF_SKY))
	{
		G_FreeEdict (self);
		return;
	}

	if (self->owner->client)
		PlayerNoise(self->owner, self->s.origin, PNOISE_IMPACT);

	if (other->takedamage)
	{
		if (self->spawnflags & 1)
			mod = MOD_HYPERBLASTER;
		else
			mod = MOD_BLASTER;
		T_Damage (other, self, self->owner, self->velocity, self->s.origin, plane->normal, self->dmg, 1, DAMAGE_ENERGY, mod);

		if (!(other->svflags & SVF_MONSTER) && !(other->client))	/// Fixed by Berserker: если бластер попадает по "здоровой" стене, взрывы/декали должны быть!
			goto boom;
	}
	else
	{
boom:;
		bool large_map = LargeMapCheck(self->s.origin);
		if (large_map)
			gi.WriteByte (svc_temp_entity_ext);
		else
			gi.WriteByte (svc_temp_entity);
		gi.WriteByte (TE_BLASTER);
		gi.WritePosition (self->s.origin, large_map);
		if (!plane)
			gi.WriteDir (vec3_origin);
		else
			gi.WriteDir (plane->normal);
		gi.multicast (self->s.origin, MULTICAST_PVS);	// Если MULTICAST_ALL - то более правильно, но ИМХО сильнее грузит сеть
	}

	G_FreeEdict (self);
}


void fire_blaster (edict_t *self, vec3_t start, vec3_t dir, int damage, int speed, int effect, bool hyper_)
{
	edict_t	*bolt;
	trace_t	tr;

	VectorNormalize (dir);

	bolt = G_Spawn();
	bolt->svflags = SVF_DEADMONSTER;
	// yes, I know it looks weird that projectiles are deadmonsters
	// what this means is that when prediction is used against the object
	// (blaster/hyperblaster shots), the player won't be solid clipped against
	// the object.  Right now trying to run into a firing hyperblaster
	// is very jerky since you are predicted 'against' the shots.
	VectorCopy (start, bolt->s.origin);
	VectorCopy (start, bolt->s.old_origin);
	vectoangles (dir, bolt->s.angles);
	VectorScale (dir, speed, bolt->velocity);
	bolt->movetype = MOVETYPE_FLYMISSILE;
	bolt->clipmask = MASK_SHOT;
	bolt->solid = SOLID_BBOX;
	bolt->s.effects |= effect;
	VectorClear (bolt->mins);
	VectorClear (bolt->maxs);
	bolt->s.modelindex = gi.modelindex ("models/objects/laser/tris.md2");
	bolt->s.sound = gi.soundindex ("misc/lasfly.wav");
	bolt->owner = self;
	bolt->touch = blaster_touch;
	bolt->nextthink = level.time + 2;
	bolt->think = G_FreeEdict;
	bolt->dmg = damage;
	bolt->classname = "bolt";
	if (hyper_)
		bolt->spawnflags = 1;
	gi.linkentity (bolt);

	if (self->client)
		check_dodge (self, bolt->s.origin, dir, speed);

	tr = gi.trace (self->s.origin, NULL, NULL, bolt->s.origin, bolt, MASK_SHOT);
	if (tr.fraction < 1.0)
	{
		VectorMA (bolt->s.origin, -10, dir, bolt->s.origin);
		blaster_touch(bolt, tr.ent, NULL, NULL);
	}
	else
	{
/// Berserker:
/// Fixed bug: стрельба сквозь стены при помощи "сидячего" трупа =)
		if (self->client)
		{
			vec3_t	point;
			VectorMA (start, 16, dir, point);										// Отодвинем тестовую точку на 16 единиц в сторону стрельбы (на кончик оружия)
			tr = gi.trace (self->s.origin, NULL, NULL, point, NULL, MASK_SOLID);	// Если тестовая точка находится в стене-полу-окне (не учитываем "живность" и "мясо")
			if (tr.fraction < 1 && !(tr.surface->flags & SURF_SKY))
			{
				VectorMA (bolt->s.origin, -10, dir, bolt->s.origin);
				blaster_touch(bolt, bolt, NULL, NULL);								// то принудительно сдетонируем снаряд
			}
		}
	}
}


void Blaster_Fire (edict_t *ent, vec3_t g_offset, int damage, bool hyper_, int effect)
{
	vec3_t	forward, right;
	vec3_t	start;
	vec3_t	offset;

	if (is_quad)
		damage *= 4;
	AngleVectors (ent->client->v_angle, forward, right, NULL);
	VectorSet(offset, 24, 8, ent->viewheight-8);
	VectorAdd (offset, g_offset, offset);
	P_ProjectSource (ent, offset, forward, right, start);

	VectorScale (forward, -2, ent->client->kick_origin);
	ent->client->kick_angles[0] = -1;

	fire_blaster (ent, start, forward, damage, 1000, effect, hyper_);

	// send muzzle flash
	gi.WriteByte (svc_muzzleflash);
	gi.WriteShort (ent-g_edicts);
	if (hyper_)
		gi.WriteByte (MZ_HYPERBLASTER | is_silenced);
	else
		gi.WriteByte (MZ_BLASTER | is_silenced);
	gi.multicast (ent->s.origin, MULTICAST_PVS);	// Если MULTICAST_ALL - то более правильно, но ИМХО сильнее грузит сеть

	PlayerNoise(ent, start, PNOISE_WEAPON);
}


void Weapon_Blaster_Fire (edict_t *ent)
{
	int		damage;

	if (deathmatch->value)
		damage = 15;
	else
		damage = 10;
	Blaster_Fire (ent, vec3_origin, damage, false, EF_BLASTER);
	ent->client->ps.gunframe++;
}


void NoAmmoWeaponChange (edict_t *ent)
{
	if ( ent->client->pers.inventory[ITEM_INDEX(FindItem("slugs"))]
		&&  ent->client->pers.inventory[ITEM_INDEX(FindItem("railgun"))] )
	{
		ent->client->newweapon = FindItem ("railgun");
		return;
	}
	if ( ent->client->pers.inventory[ITEM_INDEX(FindItem("cells"))]
		&&  ent->client->pers.inventory[ITEM_INDEX(FindItem("hyperblaster"))] )
	{
		ent->client->newweapon = FindItem ("hyperblaster");
		return;
	}
	if ( ent->client->pers.inventory[ITEM_INDEX(FindItem("bullets"))]
		&&  ent->client->pers.inventory[ITEM_INDEX(FindItem("chaingun"))] )
	{
		ent->client->newweapon = FindItem ("chaingun");
		return;
	}
	if ( ent->client->pers.inventory[ITEM_INDEX(FindItem("bullets"))]
		&&  ent->client->pers.inventory[ITEM_INDEX(FindItem("machinegun"))] )
	{
		ent->client->newweapon = FindItem ("machinegun");
		return;
	}
	if ( ent->client->pers.inventory[ITEM_INDEX(FindItem("shells"))] > 1
		&&  ent->client->pers.inventory[ITEM_INDEX(FindItem("super shotgun"))] )
	{
		ent->client->newweapon = FindItem ("super shotgun");
		return;
	}
	if ( ent->client->pers.inventory[ITEM_INDEX(FindItem("shells"))]
		&&  ent->client->pers.inventory[ITEM_INDEX(FindItem("shotgun"))] )
	{
		ent->client->newweapon = FindItem ("shotgun");
		return;
	}
	ent->client->newweapon = FindItem ("blaster");
}


/*
================
Weapon_Generic

A generic function to handle the basics of weapon thinking
================
*/
#define FRAME_FIRE_FIRST		(FRAME_ACTIVATE_LAST + 1)
#define FRAME_IDLE_FIRST		(FRAME_FIRE_LAST + 1)
#define FRAME_DEACTIVATE_FIRST	(FRAME_IDLE_LAST + 1)

void Weapon_Generic2 (edict_t *ent, int FRAME_ACTIVATE_LAST, int FRAME_FIRE_LAST, int FRAME_IDLE_LAST, int FRAME_DEACTIVATE_LAST, int *pause_frames, int *fire_frames, void (*fire)(edict_t *ent))
{
	int		n;

	if(ent->deadflag || ent->s.modelindex != 255) // VWep animations screw up corpses
	{
		return;
	}

	if (ent->client->weaponstate == WEAPON_DROPPING)
	{
		if (ent->client->ps.gunframe == FRAME_DEACTIVATE_LAST)
		{
			ChangeWeapon (ent);
			return;
		}
		else if ((FRAME_DEACTIVATE_LAST - ent->client->ps.gunframe) == 4)
		{
			ent->client->anim_priority = ANIM_REVERSE;
			if(ent->client->ps.pmove.pm_flags & PMF_DUCKED)
			{
				ent->s.frame = PLAYER_FRAME_crpain4+1;
				ent->client->anim_end = PLAYER_FRAME_crpain1;
			}
			else
			{
				ent->s.frame = PLAYER_FRAME_pain304+1;
				ent->client->anim_end = PLAYER_FRAME_pain301;

			}
		}

		ent->client->ps.gunframe++;
		return;
	}

	if (ent->client->weaponstate == WEAPON_ACTIVATING)
	{
		if (ent->client->ps.gunframe == FRAME_ACTIVATE_LAST)
		{
			ent->client->weaponstate = WEAPON_READY;
			ent->client->ps.gunframe = FRAME_IDLE_FIRST;
			return;
		}

		ent->client->ps.gunframe++;
		return;
	}

	if ((ent->client->newweapon) && (ent->client->weaponstate != WEAPON_FIRING))
	{
		ent->client->weaponstate = WEAPON_DROPPING;
		ent->client->ps.gunframe = FRAME_DEACTIVATE_FIRST;

		if ((FRAME_DEACTIVATE_LAST - FRAME_DEACTIVATE_FIRST) < 4)
		{
			ent->client->anim_priority = ANIM_REVERSE;
			if(ent->client->ps.pmove.pm_flags & PMF_DUCKED)
			{
				ent->s.frame = PLAYER_FRAME_crpain4+1;
				ent->client->anim_end = PLAYER_FRAME_crpain1;
			}
			else
			{
				ent->s.frame = PLAYER_FRAME_pain304+1;
				ent->client->anim_end = PLAYER_FRAME_pain301;

			}
		}
		return;
	}

	if (ent->client->weaponstate == WEAPON_READY)
	{
		if ( ((ent->client->latched_buttons|ent->client->buttons) & BUTTON_ATTACK) )
		{
			ent->client->latched_buttons &= ~BUTTON_ATTACK;
			if ((!ent->client->ammo_index) ||
				( ent->client->pers.inventory[ent->client->ammo_index] >= ent->client->pers.weapon->quantity))
			{
				ent->client->ps.gunframe = FRAME_FIRE_FIRST;
				ent->client->weaponstate = WEAPON_FIRING;

				// start the animation
				ent->client->anim_priority = ANIM_ATTACK;
				if (ent->client->ps.pmove.pm_flags & PMF_DUCKED)
				{
					ent->s.frame = PLAYER_FRAME_crattak1-1;
					ent->client->anim_end = PLAYER_FRAME_crattak9;
				}
				else
				{
					ent->s.frame = PLAYER_FRAME_attack1-1;
					ent->client->anim_end = PLAYER_FRAME_attack8;
				}
			}
			else
			{
				if (level.time >= ent->pain_debounce_time)
				{
					gi.sound(ent, CHAN_VOICE, gi.soundindex("weapons/noammo.wav"), 1, ATTN_NORM, 0);
					ent->pain_debounce_time = level.time + 1;
				}
				NoAmmoWeaponChange (ent);
			}
		}
		else
		{
			if (ent->client->ps.gunframe == FRAME_IDLE_LAST)
			{
				ent->client->ps.gunframe = FRAME_IDLE_FIRST;
				return;
			}

			if (pause_frames)
			{
				for (n = 0; pause_frames[n]; n++)
				{
					if (ent->client->ps.gunframe == pause_frames[n])
					{
						if (rand()&15)
							return;
					}
				}
			}

			ent->client->ps.gunframe++;
			return;
		}
	}

	if (ent->client->weaponstate == WEAPON_FIRING)
	{
		for (n = 0; fire_frames[n]; n++)
		{
			if (ent->client->ps.gunframe == fire_frames[n])
			{
				if (ent->client->quad_framenum > level.framenum)
					gi.sound(ent, CHAN_ITEM, gi.soundindex("items/damage3.wav"), 1, ATTN_NORM, 0);

				fire (ent);
				break;
			}
		}

		if (!fire_frames[n])
			ent->client->ps.gunframe++;

		if (ent->client->ps.gunframe == FRAME_IDLE_FIRST+1)
			ent->client->weaponstate = WEAPON_READY;
	}
}

/// Idea from CTF
void Weapon_Generic (edict_t *ent, int FRAME_ACTIVATE_LAST, int FRAME_FIRE_LAST, int FRAME_IDLE_LAST, int FRAME_DEACTIVATE_LAST, int *pause_frames, int *fire_frames, void (*fire)(edict_t *ent))
{
	int oldstate = ent->client->weaponstate;

	Weapon_Generic2 (ent, FRAME_ACTIVATE_LAST, FRAME_FIRE_LAST,
		FRAME_IDLE_LAST, FRAME_DEACTIVATE_LAST, pause_frames,
		fire_frames, fire);

	// run the weapon frame again if hasted
	if (((int)dmflags->value & DF_HASTE) && oldstate == ent->client->weaponstate)
	{
		Weapon_Generic2 (ent, FRAME_ACTIVATE_LAST, FRAME_FIRE_LAST,
			FRAME_IDLE_LAST, FRAME_DEACTIVATE_LAST, pause_frames,
			fire_frames, fire);
	}
}


void Weapon_Blaster (edict_t *ent)
{
	static int	pause_frames[]	= {19, 32, 0};
	static int	fire_frames[]	= {5, 0};

	Weapon_Generic (ent, 4, 8, 52, 55, pause_frames, fire_frames, Weapon_Blaster_Fire);
}


/*
================
Use_Weapon

Make the weapon ready if there is ammo
================
*/
void Use_Weapon (edict_t *ent, gitem_t *item)
{
	int			ammo_index;
	gitem_t		*ammo_item;

	// see if we're already using it
	if (item == ent->client->pers.weapon)
		return;

	if (item->ammo && !g_select_empty->value && !(item->flags & IT_AMMO))
	{
		ammo_item = FindItem(item->ammo);
		ammo_index = ITEM_INDEX(ammo_item);

		if (!ent->client->pers.inventory[ammo_index])
		{
			gi.cprintf (ent, PRINT_HIGH, "No %s for %s.\n", ammo_item->pickup_name, item->pickup_name);
			return;
		}

		if (ent->client->pers.inventory[ammo_index] < item->quantity)
		{
			gi.cprintf (ent, PRINT_HIGH, "Not enough %s for %s.\n", ammo_item->pickup_name, item->pickup_name);
			return;
		}
	}

	// change to this weapon when down
	ent->client->newweapon = item;
}


bool Pickup_Pack (edict_t *ent, edict_t *other)
{
	gitem_t	*item;
	int		index;
	int		add;
	char	message[1024];
	message[0] = 0;

	if (other->client->pers.max_bullets < 300)
		other->client->pers.max_bullets = 300;
	if (other->client->pers.max_shells < 200)
		other->client->pers.max_shells = 200;
	if (other->client->pers.max_rockets < 100)
		other->client->pers.max_rockets = 100;
	if (other->client->pers.max_grenades < 100)
		other->client->pers.max_grenades = 100;
	if (other->client->pers.max_cells < 300)
		other->client->pers.max_cells = 300;
	if (other->client->pers.max_slugs < 100)
		other->client->pers.max_slugs = 100;

	if(ent->spawnflags & DROPPED_PLAYER_PACK)
	{
		item = ent->weapon;
		if(item)
		{
			Com_sprintf (message, sizeof(message), item->pickup_name);
			other->client->pers.inventory[ITEM_INDEX(item)]++;
			if	(other->client->pers.weapon != item &&
				(other->client->pers.inventory[ITEM_INDEX(item)] == 1) &&
				(!deathmatch->value || other->client->pers.weapon == FindItem("blaster")))
				other->client->newweapon = item;
		}
	}

	item = FindItem("Bullets");
	if (item)
	{
		index = ITEM_INDEX(item);
		if(ent->spawnflags & DROPPED_PLAYER_PACK)
		{
			add = ent->ammo_bullets;
			if (add)
				Com_sprintf (message, sizeof(message), "%s %i bullets", message, add);
		}
		else
			add = item->quantity;
		other->client->pers.inventory[index] += add;
		if (other->client->pers.inventory[index] > other->client->pers.max_bullets)
			other->client->pers.inventory[index] = other->client->pers.max_bullets;
	}

	item = FindItem("Shells");
	if (item)
	{
		index = ITEM_INDEX(item);
		if(ent->spawnflags & DROPPED_PLAYER_PACK)
		{
			add = ent->ammo_shells;
			if (add)
				Com_sprintf (message, sizeof(message), "%s %i shells", message, add);
		}
		else
			add = item->quantity;
		other->client->pers.inventory[index] += add;
		if (other->client->pers.inventory[index] > other->client->pers.max_shells)
			other->client->pers.inventory[index] = other->client->pers.max_shells;
	}

	item = FindItem("Cells");
	if (item)
	{
		index = ITEM_INDEX(item);
		if(ent->spawnflags & DROPPED_PLAYER_PACK)
		{
			add = ent->ammo_cells;
			if (add)
				Com_sprintf (message, sizeof(message), "%s %i cells", message, add);
		}
		else
			add = item->quantity;
		other->client->pers.inventory[index] += add;
		if (other->client->pers.inventory[index] > other->client->pers.max_cells)
			other->client->pers.inventory[index] = other->client->pers.max_cells;
	}

	item = FindItem("Grenades");
	if (item)
	{
		index = ITEM_INDEX(item);
		if(ent->spawnflags & DROPPED_PLAYER_PACK)
		{
			add = ent->ammo_grenades;
			if (add)
				Com_sprintf (message, sizeof(message), "%s %i grenades", message, add);
		}
		else
			add = item->quantity;
		other->client->pers.inventory[index] += add;
		if (other->client->pers.inventory[index] > other->client->pers.max_grenades)
			other->client->pers.inventory[index] = other->client->pers.max_grenades;
	}

	item = FindItem("Rockets");
	if (item)
	{
		index = ITEM_INDEX(item);
		if(ent->spawnflags & DROPPED_PLAYER_PACK)
		{
			add = ent->ammo_rockets;
			if (add)
				Com_sprintf (message, sizeof(message), "%s %i rockets", message, add);
		}
		else
			add = item->quantity;
		other->client->pers.inventory[index] += add;
		if (other->client->pers.inventory[index] > other->client->pers.max_rockets)
			other->client->pers.inventory[index] = other->client->pers.max_rockets;
	}

	item = FindItem("Slugs");
	if (item)
	{
		index = ITEM_INDEX(item);
		if(ent->spawnflags & DROPPED_PLAYER_PACK)
		{
			add = ent->ammo_slugs;
			if (add)
				Com_sprintf (message, sizeof(message), "%s %i slugs", message, add);
		}
		else
			add = item->quantity;
		other->client->pers.inventory[index] += add;
		if (other->client->pers.inventory[index] > other->client->pers.max_slugs)
			other->client->pers.inventory[index] = other->client->pers.max_slugs;
	}

	if (!(ent->spawnflags & (DROPPED_ITEM | DROPPED_PLAYER_PACK)) && (deathmatch->value))
		SetRespawn (ent, ent->item->quantity);

	if(ent->spawnflags & DROPPED_PLAYER_PACK && message[0])
		gi.cprintf (other, PRINT_HIGH, "You get %s\n", message);

	return true;
}


/*
===========
PutClientInServer

Called when a player connects to a server or respawns in
a deathmatch.
============
*/
void PutClientInServer (edict_t *ent)
{
	vec3_t	mins = {-16, -16, -24};
	vec3_t	maxs = {16, 16, 32};
	int		index;
	vec3_t	spawn_origin, spawn_angles;
	gclient_t	*client;
	int		i;
	client_persistant_t	saved;
	client_respawn_t	resp;

	// find a spawn point
	// do it before setting health back up, so farthest
	// ranging doesn't count this client
	SelectSpawnPoint (ent, spawn_origin, spawn_angles);

	index = ent-g_edicts-1;
	client = ent->client;

	// deathmatch wipes most client data every spawn
	if (deathmatch->value)
	{
		char		userinfo[MAX_INFO_STRING];

		resp = client->resp;
		memcpy (userinfo, client->pers.userinfo, sizeof(userinfo));
		InitClientPersistant (client, userinfo);
		ClientUserinfoChanged (ent, userinfo);
	}
	else if (coop->value)
	{
		char		userinfo[MAX_INFO_STRING];

		resp = client->resp;
		memcpy (userinfo, client->pers.userinfo, sizeof(userinfo));
		resp.coop_respawn.game_helpchanged = client->pers.game_helpchanged;
		resp.coop_respawn.helpchanged = client->pers.helpchanged;
		client->pers = resp.coop_respawn;
		ClientUserinfoChanged (ent, userinfo);
		if (resp.score > client->pers.score)
			client->pers.score = resp.score;
	}
	else
		memset (&resp, 0, sizeof(resp));

	// clear everything but the persistant data
	saved = client->pers;
	memset (client, 0, sizeof(*client));
	client->pers = saved;
	if (client->pers.health <= 0)
		InitClientPersistant(client, client->pers.userinfo);
	client->resp = resp;

	// copy some data from the client to the entity
	FetchClientEntData (ent);

	// clear entity values
	ent->groundentity = NULL;
	ent->client = &game.clients[index];
	ent->takedamage = DAMAGE_AIM;
	ent->movetype = MOVETYPE_WALK;
	ent->viewheight = 22;
	ent->inuse = true;
	ent->classname = "player";
	ent->mass = 200;
	ent->solid = SOLID_BBOX;
	ent->deadflag = DEAD_NO;
	ent->air_finished = level.time + 12;
	ent->wet_finished = level.time - 12;
	ent->clipmask = MASK_PLAYERSOLID;
	ent->model = "players/male/tris.md2";
	ent->pain = player_pain;
	ent->die = player_die;
	ent->waterlevel = 0;
	ent->watertype = 0;
	ent->flags &= ~FL_NO_KNOCKBACK;
	ent->svflags &= ~SVF_DEADMONSTER;

	VectorCopy (mins, ent->mins);
	VectorCopy (maxs, ent->maxs);
	VectorClear (ent->velocity);

	// clear playerstate values
	memset (&ent->client->ps, 0, sizeof(client->ps));

	client->ps.pmove.origin[0] = spawn_origin[0]*8;
	client->ps.pmove.origin[1] = spawn_origin[1]*8;
	client->ps.pmove.origin[2] = spawn_origin[2]*8;

	if (deathmatch->value && ((int)dmflags->value & DF_FIXED_FOV))
	{
		client->ps.fov = 90;
	}
	else
	{
		client->ps.fov = atoi(Info_ValueForKey(client->pers.userinfo, "fov"));
		if (client->ps.fov < 1)
			client->ps.fov = 90;
		else if (client->ps.fov > MAX_FOV)
			client->ps.fov = MAX_FOV;
	}

	client->ps.gunindex = gi.modelindex(client->pers.weapon->view_model);

	// clear entity state values
	ent->s.effects = 0;
	ent->s.modelindex = 255;		// will use the skin specified model
	ent->s.modelindex2 = 255;		// custom gun model
	// sknum is player num and weapon number
	// weapon number will be added in changeweapon
	ent->s.skinnum = ent - g_edicts - 1;

	ent->s.frame = 0;
	VectorCopy (spawn_origin, ent->s.origin);
	ent->s.origin[2] += 1;	// make sure off ground
	VectorCopy (ent->s.origin, ent->s.old_origin);

	// set the delta angle
	for (i=0 ; i<3 ; i++)
	{
		client->ps.pmove.delta_angles[i] = ANGLE2SHORT(spawn_angles[i] - client->resp.cmd_angles[i]);
	}

	ent->s.angles[PITCH] = 0;
	ent->s.angles[YAW] = spawn_angles[YAW];
	ent->s.angles[ROLL] = 0;
	VectorCopy (ent->s.angles, client->ps.viewangles);
	VectorCopy (ent->s.angles, client->v_angle);

	// spawn a spectator
	if (client->pers.spectator) {
		client->chase_target = NULL;

		client->resp.spectator = true;

		ent->movetype = MOVETYPE_NOCLIP;
		ent->solid = SOLID_NOT;
		ent->svflags |= SVF_NOCLIENT;
		ent->client->ps.gunindex = 0;
		gi.linkentity (ent);
		return;
	} else
		client->resp.spectator = false;

	if (!KillBox (ent))
	{	// could't spawn in?
	}

	gi.linkentity (ent);

	// force the current weapon up
	client->newweapon = client->pers.weapon;
	ChangeWeapon (ent);
}


void respawn (edict_t *self)
{
	if (deathmatch->value || coop->value)
	{
		// spectator's don't leave bodies
		if (self->movetype != MOVETYPE_NOCLIP)
			CopyToBodyQue (self);
		self->svflags &= ~SVF_NOCLIENT;
		PutClientInServer (self);

		// add a teleportation effect
		self->s.event = EV_PLAYER_TELEPORT;

		// hold in place briefly
		self->client->ps.pmove.pm_flags = PMF_TIME_TELEPORT;
		self->client->ps.pmove.pm_time = 14;

		self->client->respawn_time = level.time;

		return;
	}

	// restart the entire server
	gi.AddCommandString ("menu_loadgame\n");
}


void MoveClientToIntermission (edict_t *ent)
{
	if (deathmatch->value || coop->value)
		ent->client->showscores = true;
	VectorCopy (level.intermission_origin, ent->s.origin);
	ent->client->ps.pmove.origin[0] = level.intermission_origin[0]*8;
	ent->client->ps.pmove.origin[1] = level.intermission_origin[1]*8;
	ent->client->ps.pmove.origin[2] = level.intermission_origin[2]*8;
	VectorCopy (level.intermission_angle, ent->client->ps.viewangles);
	ent->client->ps.pmove.pm_type = PM_FREEZE;
	ent->client->ps.gunindex = 0;
	ent->client->ps.blend[3] = 0;
	ent->client->ps.rdflags &= ~RDF_UNDERWATER;

	// clean up powerup info
	ent->client->quad_framenum = 0;
	ent->client->invincible_framenum = 0;
	ent->client->invisible_framenum = 0;
	ent->client->grenade_blew_up = false;
	ent->client->grenade_time = 0;
	ent->client->enviro_framenum = 0;
	ent->client->breather_framenum = 0;
	ent->client->flashlight_framenum = 0;
	if(!net_compatibility->value)
	{
		gi.WriteByte (svc_rfx);
		gi.WriteByte (RFX_DISABLE_ALL);		// Выключаем все эффекты
		gi.unicast (ent, true);
	}

	ent->viewheight = 0;
	ent->s.modelindex = 0;
	ent->s.modelindex2 = 0;
	ent->s.modelindex3 = 0;
	ent->s.modelindex = 0;
	ent->s.effects = 0;
	ent->s.sound = 0;
	ent->solid = SOLID_NOT;

	// add the layout
	if (deathmatch->value || coop->value)
	{
		DeathmatchScoreboardMessage (ent, NULL);
		gi.unicast (ent, true);
	}

}


void BeginIntermission (edict_t *targ)
{
	int		i, n;
	edict_t	*ent, *client;

	if (level.intermissiontime)
		return;		// already activated

	game.autosaved = false;

	// respawn any dead clients
	for (i=0 ; i<maxclients_cvar->value ; i++)
	{
		client = g_edicts + 1 + i;
		if (!client->inuse)
			continue;
		if (client->health <= 0)
			respawn(client);
	}

	level.intermissiontime = level.time;
	level.changemap = targ->map;

	if (strchr(level.changemap, '*'))
	{
		if (coop->value)
		{
			for (i=0 ; i<maxclients_cvar->value ; i++)
			{
				client = g_edicts + 1 + i;
				if (!client->inuse)
					continue;
				// strip players of all keys between units
				for (n = 0; n < MAX_ITEMS; n++)
				{
					if (itemlist[n].flags & IT_KEY)
						client->client->pers.inventory[n] = 0;
				}
			}
		}
	}
	else
	{
		if (!deathmatch->value)
		{
			level.exitintermission = 1;		// go immediately to the next level
			return;
		}
	}

	level.exitintermission = 0;

	// find an intermission spot
	ent = G_Find (NULL, FOFS(classname), "info_player_intermission", NULL);
	if (!ent)
	{	// the map creator forgot to put in an intermission point...
		ent = G_Find (NULL, FOFS(classname), "info_player_start", NULL);
		if (!ent)
			ent = G_Find (NULL, FOFS(classname), "info_player_deathmatch", NULL);
	}
	else
	{	// chose one of four spots
		i = rand() & 3;
		while (i--)
		{
			ent = G_Find (ent, FOFS(classname), "info_player_intermission", NULL);
			if (!ent)	// wrap around the list
				ent = G_Find (ent, FOFS(classname), "info_player_intermission", NULL);
		}
	}

	VectorCopy (ent->s.origin, level.intermission_origin);
	VectorCopy (ent->s.angles, level.intermission_angle);

	// move all clients to the intermission point
	for (i=0 ; i<maxclients_cvar->value ; i++)
	{
		client = g_edicts + 1 + i;
		if (!client->inuse)
			continue;
		MoveClientToIntermission (client);
	}
}


/*
=================
CreateTargetChangeLevel

Returns the created target changelevel
=================
*/
edict_t *CreateTargetChangeLevel(char *map)
{
	edict_t *ent;

	ent = G_Spawn ();
	ent->classname = "target_changelevel";
	Com_sprintf(level.nextmap, sizeof(level.nextmap), "%s", map);
	ent->map = level.nextmap;
	return ent;
}


/*
QUAKED target_changelevel (1 0 0) (-8 -8 -8) (8 8 8)
Changes level to "map" when fired
*/
void use_target_changelevel (edict_t *self, edict_t *other, edict_t *activator)
{
	if (level.intermissiontime)
		return;		// already activated

	if (!deathmatch->value && !coop->value)
	{
		if (g_edicts[1].health <= 0)
			return;
	}

	// if noexit, do a ton of damage to other
	if (deathmatch->value && !( (int)dmflags->value & DF_ALLOW_EXIT) && other != world)
	{
		T_Damage (other, self, self, vec3_origin, other->s.origin, vec3_origin, 10 * other->max_health, 1000, 0, MOD_EXIT);
		return;
	}

	// if multiplayer, let everyone know who hit the exit
	if (deathmatch->value)
	{
		if (activator && activator->client)
			gi.bprintf (PRINT_HIGH, "%s exited the level.\n", activator->client->pers.netname);
	}

	// if going to a new unit, clear cross triggers
	if (strchr(self->map, '*'))
		game.serverflags &= ~(SFL_CROSS_TRIGGER_MASK);

//// Berserker: allow DF_SAME_LEVEL to affects on exit from level
	if (deathmatch->value)
	{
		// stay on same level flag
		if ((int)dmflags->value & DF_SAME_LEVEL)
		{
			BeginIntermission (CreateTargetChangeLevel (level.mapname) );
			return;
		}
	}
////

	BeginIntermission (self);
}

void SP_target_changelevel (edict_t *ent)
{
	if (!ent->map)
	{
		gi.dprintf("target_changelevel with no map at %s\n", vtos(ent->s.origin));
		G_FreeEdict (ent);
		return;
	}

	ent->use = use_target_changelevel;
	ent->svflags = SVF_NOCLIENT;
}


//==========================================================

/*
=================
findradius

Returns entities that have origins within a spherical area

findradius (origin, radius)
=================
*/
edict_t *findradius (edict_t *from, vec3_t org, float rad)
{
	vec3_t	eorg;
	int		j;

	if (!from)
		from = g_edicts;
	else
		from++;
	for ( ; from < &g_edicts[globals.num_edicts]; from++)
	{
		if (!from->inuse)
			continue;
		if (from->solid == SOLID_NOT)
			continue;
		for (j=0 ; j<3 ; j++)
			eorg[j] = org[j] - (from->s.origin[j] + (from->mins[j] + from->maxs[j])*0.5);
		if (VectorLength(eorg) > rad)
			continue;
		return from;
	}

	return NULL;
}


/*
============
CanDamage

Returns true if the inflictor can directly damage the target.  Used for
explosions and melee attacks.

FIXED by Berserker: подходишь к маленькому ящику около большого ящика (Q2DM1) и стреляешь из рокета - не было повреждений. Исправлено  :)
Просто добавил trace-тестов еще и по Z-координате...
============
*/
bool CanDamage (edict_t *targ, edict_t *inflictor)
{
	vec3_t	dest;
	trace_t	trace;

// bmodels need special checking because their origin is 0,0,0
	if (targ->movetype == MOVETYPE_PUSH)
	{
		VectorAdd (targ->absmin, targ->absmax, dest);
		VectorScale (dest, 0.5, dest);
		trace = gi.trace (inflictor->s.origin, vec3_origin, vec3_origin, dest, inflictor, MASK_SOLID);
		if (trace.fraction == 1.0)
			return true;
		if (trace.ent == targ)
			return true;
		return false;
	}

	trace = gi.trace (inflictor->s.origin, vec3_origin, vec3_origin, targ->s.origin, inflictor, MASK_SOLID);
	if (trace.fraction == 1.0)
		return true;

	VectorCopy (targ->s.origin, dest);
	dest[0] += 15.0;
	dest[1] += 15.0;
	dest[2] += 23.0;
	trace = gi.trace (inflictor->s.origin, vec3_origin, vec3_origin, dest, inflictor, MASK_SOLID);
	if (trace.fraction == 1.0)
		return true;

	VectorCopy (targ->s.origin, dest);
	dest[0] += 15.0;
	dest[1] -= 15.0;
	dest[2] += 23.0;
	trace = gi.trace (inflictor->s.origin, vec3_origin, vec3_origin, dest, inflictor, MASK_SOLID);
	if (trace.fraction == 1.0)
		return true;

	VectorCopy (targ->s.origin, dest);
	dest[0] -= 15.0;
	dest[1] += 15.0;
	dest[2] += 23.0;
	trace = gi.trace (inflictor->s.origin, vec3_origin, vec3_origin, dest, inflictor, MASK_SOLID);
	if (trace.fraction == 1.0)
		return true;

	VectorCopy (targ->s.origin, dest);
	dest[0] -= 15.0;
	dest[1] -= 15.0;
	dest[2] += 23.0;
	trace = gi.trace (inflictor->s.origin, vec3_origin, vec3_origin, dest, inflictor, MASK_SOLID);
	if (trace.fraction == 1.0)
		return true;

	VectorCopy (targ->s.origin, dest);
	dest[0] += 15.0;
	dest[1] += 15.0;
	dest[2] -= 23.0;
	trace = gi.trace (inflictor->s.origin, vec3_origin, vec3_origin, dest, inflictor, MASK_SOLID);
	if (trace.fraction == 1.0)
		return true;

	VectorCopy (targ->s.origin, dest);
	dest[0] += 15.0;
	dest[1] -= 15.0;
	dest[2] -= 23.0;
	trace = gi.trace (inflictor->s.origin, vec3_origin, vec3_origin, dest, inflictor, MASK_SOLID);
	if (trace.fraction == 1.0)
		return true;

	VectorCopy (targ->s.origin, dest);
	dest[0] -= 15.0;
	dest[1] += 15.0;
	dest[2] -= 23.0;
	trace = gi.trace (inflictor->s.origin, vec3_origin, vec3_origin, dest, inflictor, MASK_SOLID);
	if (trace.fraction == 1.0)
		return true;

	VectorCopy (targ->s.origin, dest);
	dest[0] -= 15.0;
	dest[1] -= 15.0;
	dest[2] -= 23.0;
	trace = gi.trace (inflictor->s.origin, vec3_origin, vec3_origin, dest, inflictor, MASK_SOLID);
	if (trace.fraction == 1.0)
		return true;

	return false;
}


void T_RadiusDamage (edict_t *inflictor, edict_t *attacker, float damage, edict_t *ignore, float radius, int mod)
{
	float	points;
	edict_t	*ent = NULL;
	vec3_t	v;
	vec3_t	dir;

	while ((ent = findradius(ent, inflictor->s.origin, radius)) != NULL)
	{
		if (ent == ignore)
			continue;
		if (!ent->takedamage)
			continue;

		VectorAdd (ent->mins, ent->maxs, v);
		VectorMA (ent->s.origin, 0.5, v, v);
		VectorSubtract (inflictor->s.origin, v, v);
		points = damage - 0.5 * VectorLength (v);
		if (ent == attacker)
			points = points * 0.5;
		if (points > 0)
		{
			if (CanDamage (ent, inflictor))
			{
				VectorSubtract (ent->s.origin, inflictor->s.origin, dir);
				T_Damage (ent, inflictor, attacker, dir, inflictor->s.origin, vec3_origin, (int)points, (int)points, DAMAGE_RADIUS, mod);
			}
		}
	}
}


/*
QUAKED target_splash (1 0 0) (-8 -8 -8) (8 8 8)
Creates a particle splash effect when used.

Set "sounds" to one of the following:
  1) sparks
  2) blue water
  3) brown water
  4) slime
  5) lava
  6) blood

"count"	how many pixels in the splash
"dmg"	if set, does a radius damage at this location when it splashes
		useful for lava/sparks
*/

void use_target_splash (edict_t *self, edict_t *other, edict_t *activator)
{
	bool large_map = LargeMapCheck(self->s.origin);
	if (large_map)
		gi.WriteByte (svc_temp_entity_ext);
	else
		gi.WriteByte (svc_temp_entity);
	gi.WriteByte (TE_SPLASH);
	gi.WriteByte (self->count);
	gi.WritePosition (self->s.origin, large_map);
	gi.WriteDir (self->movedir);
	gi.WriteByte (self->sounds);
	gi.multicast (self->s.origin, MULTICAST_PVS);

	if (self->dmg)
		T_RadiusDamage (self, activator, self->dmg, NULL, self->dmg+40, MOD_SPLASH);
}

void SP_target_splash (edict_t *self)
{
	self->use = use_target_splash;
	G_SetMovedir (self->s.angles, self->movedir);

	if (!self->count)
		self->count = 32;

	self->svflags = SVF_NOCLIENT;
}


//==========================================================

/*
QUAKED target_spawner (1 0 0) (-8 -8 -8) (8 8 8)
Set target to the type of entity you want spawned.
Useful for spawning monsters and gibs in the factory levels.

For monsters:
	Set direction to the facing you want it to have.

For gibs:
	Set direction if you want it moving and
	speed how fast it should be moving otherwise it
	will just be dropped
*/
void use_target_spawner (edict_t *self, edict_t *other, edict_t *activator)
{
	edict_t	*ent;

	ent = G_Spawn();
	ent->classname = self->target;
	VectorCopy (self->s.origin, ent->s.origin);
	VectorCopy (self->s.angles, ent->s.angles);
//	поместил до ED_CallSpawn, чтобы к клиенту шла корректная скорость
	if (self->speed)
		VectorCopy (self->movedir, ent->velocity);
//
	ED_CallSpawn (ent);
	if(net_compatibility->value || !net_fixoverflow->value)
	{
		gi.unlinkentity (ent);
		KillBox (ent);
		gi.linkentity (ent);
	}
}

void SP_target_spawner (edict_t *self)
{
	self->use = use_target_spawner;
	self->svflags = SVF_NOCLIENT;
	if (self->speed)
	{
		G_SetMovedir (self->s.angles, self->movedir);
		VectorScale (self->movedir, self->speed, self->movedir);
	}
}


/*
QUAKED target_blaster (1 0 0) (-8 -8 -8) (8 8 8) NOTRAIL NOEFFECTS
Fires a blaster bolt in the set direction when triggered.

dmg		default is 15 (blaster)
speed	default is 1000 (blaster)
style	тип выстрела:
		0 - blaster (default)
		1 - rocket
		2 - grenade
		3 - rail
		4 - BFG
		255 - random
*/

void use_target_blaster (edict_t *self, edict_t *other, edict_t *activator)
{
	int effect, style;

	if (self->style == 255)
	{
		style = rand() % 5;		/// 5 видов: blaster, rocket, grenade, rail, BFG
		switch (style)
		{
		case 0:		// blaster
			self->noise_index = gi.soundindex ("weapons/laser2.wav");
			self->dmg = 15;
			self->speed = 1000;
			break;

		case 1:		// rocket
			self->noise_index = gi.soundindex("weapons/rocklf1a.wav");
			self->dmg = 120;
			self->speed = 1000;
			break;

		case 2:		// grenade
			self->noise_index = gi.soundindex ("weapons/hgrenb1a.wav");
			self->dmg = 125;
			self->speed = 600;
			break;

		case 3:		// rail
			self->noise_index = gi.soundindex ("weapons/railgf1a.wav");
			self->dmg = 125;
			break;

		case 4:		// BFG
			self->noise_index = gi.soundindex ("weapons/bfg__f1y.wav");
			self->dmg = 350;
			self->speed = 400;
			break;
		}
	}
	else
		style = self->style;

	switch (style)
	{
	case 0:		// blaster
		if (self->spawnflags & 2)
			effect = 0;
		else if (self->spawnflags & 1)
			effect = EF_HYPERBLASTER;
		else
			effect = EF_BLASTER;

		fire_blaster (self, self->s.origin, self->movedir, self->dmg, self->speed, effect/*EF_BLASTER*/, MOD_TARGET_BLASTER);	/// Berserker: fixed bug...
		break;

	case 1:		// rocket
		fire_rocket (self, self->s.origin, self->movedir, self->dmg, self->speed, self->dmg, self->dmg);
		break;

	case 2:		// grenade
		fire_grenade (self, self->s.origin, self->movedir, self->dmg, self->speed, 2.5, self->dmg);
		break;

	case 3:		// rail
		fire_rail (self, self->s.origin, self->movedir, self->dmg, self->dmg*2);
		break;

	case 4:		// BFG
		fire_bfg (self, self->s.origin, self->movedir, self->dmg, self->speed, self->dmg*2);
		break;
	}

	gi.sound (self, CHAN_VOICE, self->noise_index, 1, ATTN_NORM, 0);
}

void SP_target_blaster (edict_t *self)
{
	self->use = use_target_blaster;
	G_SetMovedir (self->s.angles, self->movedir);

	if (self->style != 255)
		if (self->style > 4 || self->style < 0)	/// 5 видов: blaster, rocket, grenade, rail, BFG
			self->style = 0;

	switch (self->style)
	{
	case 0:		// blaster
		self->noise_index = gi.soundindex ("weapons/laser2.wav");
		if (!self->dmg)
			self->dmg = 15;
		if (!self->speed)
			self->speed = 1000;
		break;

	case 1:		// rocket
		self->noise_index = gi.soundindex("weapons/rocklf1a.wav");
		if (!self->dmg)
			self->dmg = 120;
		if (!self->speed)
			self->speed = 1000;
		break;

	case 2:		// grenade
		self->noise_index = gi.soundindex ("weapons/hgrenb1a.wav");
		if (!self->dmg)
			self->dmg = 125;
		if (!self->speed)
			self->speed = 600;
		break;

	case 3:		// rail
		self->noise_index = gi.soundindex ("weapons/railgf1a.wav");
		if (!self->dmg)
			self->dmg = 125;
		break;

	case 4:		// BFG
		self->noise_index = gi.soundindex ("weapons/bfg__f1y.wav");
		if (!self->dmg)
			self->dmg = 350;
		if (!self->speed)
			self->speed = 400;
		break;
	}

	self->svflags = SVF_NOCLIENT;
}


//==========================================================

/*
QUAKED target_kill (.5 .5 .5) (-8 -8 -8) (8 8 8)
Kills the activator
*/
void target_kill_use (edict_t *self, edict_t *other, edict_t *activator)
{
	T_Damage (activator, self, self, vec3_origin, activator->s.origin, vec3_origin, 100000, 0, DAMAGE_NO_PROTECTION, MOD_TELEFRAG);
}

void SP_target_kill (edict_t *self)
{
	self->use = target_kill_use;
}


/*
QUAKED target_damage (.5 .5 .5) (-8 -8 -8) (8 8 8)
Damage the activator - dmg
*/
void target_damage_use (edict_t *self, edict_t *other, edict_t *activator)
{
	T_Damage (activator, self, self, vec3_origin, activator->s.origin, vec3_origin, self->dmg, 0, 0, MOD_TELEFRAG);
}

void SP_target_damage (edict_t *self)
{
	self->use = target_damage_use;
}


/*
QUAKED target_crosslevel_trigger (.5 .5 .5) (-8 -8 -8) (8 8 8) trigger1 trigger2 trigger3 trigger4 trigger5 trigger6 trigger7 trigger8
Once this trigger is touched/used, any trigger_crosslevel_target with the same trigger number is automatically used when a level is started within the same unit.  It is OK to check multiple triggers.  Message, delay, target, and killtarget also work.
*/
void trigger_crosslevel_trigger_use (edict_t *self, edict_t *other, edict_t *activator)
{
	game.serverflags |= self->spawnflags;
	G_FreeEdict (self);
}

void SP_target_crosslevel_trigger (edict_t *self)
{
	self->svflags = SVF_NOCLIENT;
	self->use = trigger_crosslevel_trigger_use;
}

/*
QUAKED target_crosslevel_target (.5 .5 .5) (-8 -8 -8) (8 8 8) trigger1 trigger2 trigger3 trigger4 trigger5 trigger6 trigger7 trigger8
Triggered by a trigger_crosslevel elsewhere within a unit.  If multiple triggers are checked, all must be true.  Delay, target and
killtarget also work.

"delay"		delay before using targets if the trigger has been activated (default 1)
*/
void target_crosslevel_target_think (edict_t *self)
{
	if (self->spawnflags == (game.serverflags & SFL_CROSS_TRIGGER_MASK & self->spawnflags))
	{
		G_UseTargets (self, self, NULL);
		G_FreeEdict (self);
	}
}

void SP_target_crosslevel_target (edict_t *self)
{
	if (! self->delay)
		self->delay = 1;
	self->svflags = SVF_NOCLIENT;

	self->think = target_crosslevel_target_think;
	self->nextthink = level.time + self->delay;
}

//==========================================================

/*
QUAKED target_laser (0 .5 .8) (-8 -8 -8) (8 8 8) START_ON RED GREEN BLUE YELLOW FIOLET(not ORANGE!) FAT
When triggered, fires a laser.  You can either set a target
or a direction.
*/

void target_laser_think (edict_t *self)
{
	edict_t	*ignore;
	vec3_t	start;
	vec3_t	end;
	trace_t	tr;
	vec3_t	point;
	vec3_t	last_movedir;
	int		count;

	if (self->spawnflags & 0x80000000)
		count = 8;
	else
		count = 4;

	if (self->enemy)
	{
		VectorCopy (self->movedir, last_movedir);
		VectorMA (self->enemy->absmin, 0.5, self->enemy->size, point);
		VectorSubtract (point, self->s.origin, self->movedir);
		VectorNormalize (self->movedir);
		if (!VectorCompare(self->movedir, last_movedir))
			self->spawnflags |= 0x80000000;
	}

	ignore = self;
	VectorCopy (self->s.origin, start);
	VectorMA (start, 2048, self->movedir, end);
	while(1)
	{
		tr = gi.trace (start, NULL, NULL, end, ignore, CONTENTS_SOLID|CONTENTS_MONSTER|CONTENTS_DEADMONSTER);

		if (!tr.ent)
			break;

		// hurt it if we can
		if ((tr.ent->takedamage) && !(tr.ent->flags & FL_IMMUNE_LASER))
			T_Damage (tr.ent, self, self->activator, self->movedir, tr.endpos, vec3_origin, self->dmg, 1, DAMAGE_ENERGY, MOD_TARGET_LASER);

		// if we hit something that's not a monster or player or is immune to lasers, we're done
		if (!(tr.ent->svflags & SVF_MONSTER) && (!tr.ent->client))
		{
			if (self->spawnflags & 0x80000000)
			{
				self->spawnflags &= ~0x80000000;
				bool large_map = LargeMapCheck(tr.endpos);
				if (large_map)
					gi.WriteByte (svc_temp_entity_ext);
				else
					gi.WriteByte (svc_temp_entity);
				gi.WriteByte (TE_LASER_SPARKS);
				gi.WriteByte (count);
				gi.WritePosition (tr.endpos, large_map);
				gi.WriteDir (tr.plane.normal);
				gi.WriteByte (self->s.skinnum);
				gi.multicast (tr.endpos, MULTICAST_PVS);
			}
			break;
		}

		ignore = tr.ent;
		VectorCopy (tr.endpos, start);
	}

	VectorCopy (tr.endpos, self->s.old_origin);

	self->nextthink = level.time + FRAMETIME;
}

void target_laser_on (edict_t *self)
{
	if (!self->activator)
		self->activator = self;
	self->spawnflags |= 0x80000001;
	self->svflags &= ~SVF_NOCLIENT;
	target_laser_think (self);
}

void target_laser_off (edict_t *self)
{
	self->spawnflags &= ~1;
	self->svflags |= SVF_NOCLIENT;
	self->nextthink = 0;
}

void target_laser_use (edict_t *self, edict_t *other, edict_t *activator)
{
	self->activator = activator;
	if (self->spawnflags & 1)
		target_laser_off (self);
	else
		target_laser_on (self);
}

void target_laser_start (edict_t *self)
{
	edict_t *ent;

	self->movetype = MOVETYPE_NONE;
	self->solid = SOLID_NOT;
	self->s.renderfx |= RF_BEAM|RF_TRANSLUCENT;
	self->s.modelindex = 1;			// must be non-zero

	// set the beam diameter
	if (self->spawnflags & 64)
		self->s.frame = 16;
	else
		self->s.frame = 4;

	// set the color
	if (self->spawnflags & 2)
		self->s.skinnum = 0xf2f2f0f0;
	else if (self->spawnflags & 4)
		self->s.skinnum = 0xd0d1d2d3;
	else if (self->spawnflags & 8)
		self->s.skinnum = 0xf3f3f1f1;
	else if (self->spawnflags & 16)
		self->s.skinnum = 0xdcdddedf;
	else if (self->spawnflags & 32)
		self->s.skinnum = 0xe0e1e2e3;

	if (!self->enemy)
	{
		if (self->target)
		{
			ent = G_Find (NULL, FOFS(targetname), self->target, "target_laser_start");
			if (!ent)
				gi.dprintf ("%s at %s: %s is a bad target\n", self->classname, vtos(self->s.origin), self->target);
			self->enemy = ent;
		}
		else
		{
			G_SetMovedir (self->s.angles, self->movedir);
		}
	}
	self->use = target_laser_use;
	self->think = target_laser_think;

	if (!self->dmg)
		self->dmg = 1;

	VectorSet (self->mins, -8, -8, -8);
	VectorSet (self->maxs, 8, 8, 8);
	gi.linkentity (self);

	if (self->spawnflags & 1)
		target_laser_on (self);
	else
		target_laser_off (self);
}

void SP_target_laser (edict_t *self)
{
	// let everything else get spawned before we start firing
	self->think = target_laser_start;
	self->nextthink = level.time + 1;
}

//==========================================================

/*
QUAKED target_earthquake (1 0 0) (-8 -8 -8) (8 8 8)
When triggered, this initiates a level-wide earthquake.
All players and monsters are affected.
"speed"		severity of the quake (default:200)
"count"		duration of the quake (default:5)
*/

void target_earthquake_think (edict_t *self)
{
	int		i;
	edict_t	*e;

	if (self->last_move_time < level.time)
	{
		gi.positioned_sound (self->s.origin, self, CHAN_AUTO, self->noise_index, 1.0, ATTN_NONE, 0);
		self->last_move_time = level.time + 0.5;
	}

	for (i=1, e=g_edicts+i; i < globals.num_edicts; i++,e++)
	{
		if (!e->inuse)
			continue;
		if (!e->client)
			continue;
		if (!e->groundentity)
			continue;

		e->groundentity = NULL;
		e->velocity[0] += crandom()* 150;
		e->velocity[1] += crandom()* 150;
		e->velocity[2] = self->speed * (100.0 / e->mass);
	}

	if (level.time < self->timestamp)
		self->nextthink = level.time + FRAMETIME;
}

void target_earthquake_use (edict_t *self, edict_t *other, edict_t *activator)
{
	self->timestamp = level.time + self->count;
	self->nextthink = level.time + FRAMETIME;
	self->activator = activator;
	self->last_move_time = 0;
}

void SP_target_earthquake (edict_t *self)
{
	if (!self->targetname)
		gi.dprintf("untargeted %s at %s\n", self->classname, vtos(self->s.origin));

	if (!self->count)
		self->count = 5;

	if (!self->speed)
		self->speed = 200;

	self->svflags |= SVF_NOCLIENT;
	self->think = target_earthquake_think;
	self->use = target_earthquake_use;

	self->noise_index = gi.soundindex ("world/quake.wav");
}



























void InitItems ()
{
	game.num_items = sizeof(itemlist)/sizeof(itemlist[0]) - 1;
}


void DoRespawn (edict_t *ent)
{
	if (ent->team)
	{
		edict_t	*master;
		int	count;
		int choice;

		master = ent->teammaster;
		for (count = 0, ent = master; ent; ent = ent->chain, count++);
		choice = rand() % count;
		for (count = 0, ent = master; count < choice; ent = ent->chain, count++);
	}

	ent->svflags &= ~SVF_NOCLIENT;
	ent->solid = SOLID_TRIGGER;
	gi.linkentity (ent);

	// send an effect
	ent->s.event = EV_ITEM_RESPAWN;
}


void SetRespawn (edict_t *ent, float delay)
{
	ent->flags |= FL_RESPAWN;
	ent->svflags |= SVF_NOCLIENT;
	ent->solid = SOLID_NOT;
	ent->nextthink = level.time + delay;
	ent->think = DoRespawn;
	gi.linkentity (ent);
}


/*
=================
G_FreeEdict

Marks the edict as free
=================
*/
void G_FreeEdict (edict_t *ed)
{
	gi.unlinkentity (ed);		// unlink from world

	if ((ed - g_edicts) <= (maxclients_cvar->value + BODY_QUEUE_SIZE))
		return;

	memset (ed, 0, sizeof(*ed));
	ed->classname = "freed";
	ed->freetime = level.time;
	ed->inuse = false;
}


void G_FreeEdictExt (edict_t *ed)
{
	vec3_t	dir;
	VectorSet(dir,0,0,1);

	bool large_map = LargeMapCheck(ed->s.origin);
	if (large_map)
		gi.WriteByte (svc_temp_entity_ext);
	else
		gi.WriteByte (svc_temp_entity);
	gi.WriteByte (TE_SPLASH);
	gi.WriteByte (8);
	gi.WritePosition (ed->s.origin, large_map);
	gi.WriteDir (dir);
	gi.WriteByte (SPLASH_BLOOD);
	gi.multicast (ed->s.origin, MULTICAST_PVS);

	G_FreeEdict(ed);
}


void MegaHealth_think (edict_t *self)
{
	if (self->owner->health > self->owner->max_health)
	{
		self->nextthink = level.time + 1;
		self->owner->health -= 1;
		return;
	}

	if (!(self->spawnflags & DROPPED_ITEM) && (deathmatch->value))
		SetRespawn (self, 20);
	else
		G_FreeEdict (self);
}


bool Pickup_Health (edict_t *ent, edict_t *other)
{
	if (!(ent->style & HEALTH_IGNORE_MAX))
		if (other->health >= other->max_health)
			return false;

	other->health += ent->count;

	if (!(ent->style & HEALTH_IGNORE_MAX))
	{
		if (other->health > other->max_health)
			other->health = other->max_health;
	}

	if (ent->style & HEALTH_TIMED)
	{
		ent->think = MegaHealth_think;
		ent->nextthink = level.time + 5;
		ent->owner = other;
		ent->flags |= FL_RESPAWN;
		ent->svflags |= SVF_NOCLIENT;
		ent->solid = SOLID_NOT;
	}
	else
	{
		if (!(ent->spawnflags & DROPPED_ITEM) && (deathmatch->value))
			SetRespawn (ent, 30);
	}

	return true;
}


/*
============
InitGame

This will be called when the dll is first loaded, which
only happens when a new game is started or a save game
is loaded.
============
*/
void InitGame ()
{
	gi.dprintf ("==== InitGame ====\n");

	// reset rand() generator
	srand(time(NULL));

	gun_x = gi.cvar ("gun_x", "0", 0);
	gun_y = gi.cvar ("gun_y", "0", 0);
	gun_z = gi.cvar ("gun_z", "0", 0);

	net_compatibility = gi.cvar ("net_compatibility", "0", CVAR_SERVERINFO|CVAR_NOSET);
	net_fixoverflow = gi.cvar ("net_fixoverflow", "1", CVAR_SERVERINFO);
	if(!net_compatibility->value)
		sv_flyqbe = gi.cvar ("sv_flyqbe", "0.5", CVAR_ARCHIVE);

	//FIXME: sv_ prefix is wrong for these
	sv_rollspeed = gi.cvar ("sv_rollspeed", "200", 0);
	sv_rollangle = gi.cvar ("sv_rollangle", "2", 0);
	sv_maxvelocity = gi.cvar ("sv_maxvelocity", "2000", 0);
	sv_gravity = gi.cvar ("sv_gravity", "800", 0);

	// noset vars
	dedicated = gi.cvar ("dedicated", "0", CVAR_NOSET);

	// latched vars
	sv_cheats = gi.cvar ("cheats", "0", CVAR_SERVERINFO|CVAR_LATCH);
	gi.cvar ("gamename", GAMEVERSION , CVAR_SERVERINFO | CVAR_LATCH);
	gi.cvar ("gamedate", __DATE__ , CVAR_SERVERINFO | CVAR_LATCH);

	maxclients_cvar = gi.cvar ("maxclients", "4", CVAR_SERVERINFO | CVAR_LATCH);
	maxspectators = gi.cvar ("maxspectators", "4", CVAR_SERVERINFO);
	deathmatch = gi.cvar ("deathmatch", "0", CVAR_LATCH);
	sv_predator = gi.cvar ("sv_predator", "0", CVAR_LATCH|CVAR_ARCHIVE);
	sv_knockback = gi.cvar ("sv_knockback", "1600", CVAR_ARCHIVE);
	g_monsterRespawn = gi.cvar ("g_monsterRespawn", "0", CVAR_ARCHIVE);

	g_playerDamageScale = gi.cvar("g_playerDamageScale", "1", 0);
	g_monsterDamageScale = gi.cvar("g_monsterDamageScale", "1", 0);
	g_enviroDamageScale = gi.cvar("g_enviroDamageScale", "1", 0);
	weaponHitAccuracy = gi.cvar("weaponHitAccuracy", "1", CVAR_USERINFO | CVAR_ARCHIVE);

	sv_gibs = gi.cvar ("sv_gibs", "10", 0);

	coop = gi.cvar ("coop", "0", CVAR_LATCH);
	skill = gi.cvar ("skill", "1", CVAR_LATCH);
	maxentities = gi.cvar ("maxentities", "1024", CVAR_LATCH);

	// change anytime vars
	dmflags = gi.cvar ("dmflags", "0", CVAR_SERVERINFO);
	fraglimit = gi.cvar ("fraglimit", "0", CVAR_SERVERINFO);
	timelimit = gi.cvar ("timelimit", "0", CVAR_SERVERINFO);
	password = gi.cvar ("password", "", CVAR_USERINFO);
	spectator_password = gi.cvar ("spectator_password", "", CVAR_USERINFO);
	needpass = gi.cvar ("needpass", "0", CVAR_SERVERINFO);
	filterban = gi.cvar ("filterban", "1", 0);

	g_select_empty = gi.cvar ("g_select_empty", "0", CVAR_ARCHIVE);
	g_ignore_external_layoutstring = gi.cvar ("g_ignore_external_layoutstring", "0", CVAR_ARCHIVE|CVAR_LATCH);

	run_pitch = gi.cvar ("run_pitch", "0.002", 0);
	run_roll = gi.cvar ("run_roll", "0.005", 0);
	bob_up  = gi.cvar ("bob_up", "0.005", 0);
	bob_pitch = gi.cvar ("bob_pitch", "0.002", 0);
	bob_roll = gi.cvar ("bob_roll", "0.002", 0);

	// flood control
	flood_msgs = gi.cvar ("flood_msgs", "4", 0);
	flood_persecond = gi.cvar ("flood_persecond", "4", 0);
	flood_waitdelay = gi.cvar ("flood_waitdelay", "10", 0);

	// dm map list
	sv_maplist = gi.cvar ("sv_maplist", "", 0);

	// items
	InitItems ();

	Com_sprintf (game.helpmessage1, sizeof(game.helpmessage1), "");

	Com_sprintf (game.helpmessage2, sizeof(game.helpmessage2), "");

	// initialize all entities for this game
	game.maxentities = maxentities->value;
	g_edicts =  (edict_t *) gi.TagMalloc (game.maxentities * sizeof(g_edicts[0]), TAG_GAME);
	globals.edicts = g_edicts;
	globals.max_edicts = game.maxentities;

	// initialize all clients for this game
	game.maxclients = maxclients_cvar->value;
	game.clients = (gclient_t *) gi.TagMalloc (game.maxclients * sizeof(game.clients[0]), TAG_GAME);
	globals.num_edicts = game.maxclients+1;
}


void ShutdownGame ()
{
	gi.dprintf ("==== ShutdownGame ====\n");

	gi.FreeTags (TAG_LEVEL);
	gi.FreeTags (TAG_GAME);
}


/*
==================
SaveClientData

Some information that should be persistant, like health,
is still stored in the edict structure, so it needs to
be mirrored out to the client structure before all the
edicts are wiped.
==================
*/
void SaveClientData ()
{
	int		i;
	edict_t	*ent;

	for (i=0 ; i<game.maxclients ; i++)
	{
		ent = &g_edicts[1+i];
		if (!ent->inuse)
			continue;
		game.clients[i].pers.health = ent->health;
		game.clients[i].pers.max_health = ent->max_health;
			game.clients[i].pers.savedFlags = (ent->flags & (FL_GODMODE|FL_NOTARGET|FL_POWER_ARMOR|		FL_FLASHLIGHT));
////		if (ent->client->flashlight)
////			game.clients[i].pers.savedFlags |= FL_FLASHLIGHT;
////		else
////			game.clients[i].pers.savedFlags &= ~FL_FLASHLIGHT;
		if (coop->value)
			game.clients[i].pers.score = ent->client->resp.score;
	}
}


void G_InitEdict (edict_t *e)
{
	e->inuse = true;
	e->classname = "noclass";
	e->gravity = 1.0;
	e->s.number = e - g_edicts;
}


/*
=================
G_Spawn

Either finds a free edict, or allocates a new one.
Try to avoid reusing an entity that was recently freed, because it
can cause the client to think the entity morphed into something else
instead of being removed and recreated, which can cause interpolated
angles and bad trails.
=================
*/
edict_t *G_Spawn ()
{
	int			i;
	edict_t		*e;

	e = &g_edicts[(int)maxclients_cvar->value+1];
	for ( i=maxclients_cvar->value+1 ; i<globals.num_edicts ; i++, e++)
	{
		// the first couple seconds of server time can involve a lot of
		// freeing and allocating, so relax the replacement policy
		if (!e->inuse && ( e->freetime < 2 || level.time - e->freetime > 0.5 ) )
		{
			G_InitEdict (e);
			return e;
		}
	}

	if (i == game.maxentities)
		gi.error ("ED_Alloc: no free edicts");

	globals.num_edicts++;
	G_InitEdict (e);
	return e;
}


char *ED_NewString (char *string)
{
	char	*newb, *new_p;
	int		i,l;

	l = strlen(string) + 1;
	newb = (char *) gi.TagMalloc (l, TAG_LEVEL);
	new_p = newb;

	for (i=0 ; i< l ; i++)
	{
		if (string[i] == '\\' && i < l-1)
		{
			i++;
			if (string[i] == 'n')
				*new_p++ = '\n';
			else
				*new_p++ = '\\';
		}
		else
			*new_p++ = string[i];
	}

	return newb;
}


/*
===============
ED_ParseField

Takes a key/value pair and sets the binary values
in an edict
===============
*/
void ED_ParseField (char *key, char *value, edict_t *ent)
{
	field_t	*f;
	byte	*b;
	float	v;
	vec3_t	vec;
	char	*end_ptr;

	for (f=fields ; f->name ; f++)
	{
		if (!(f->flags & FFL_NOSPAWN) && !Q_strcasecmp(f->name, key))
		{	// found it
			if (f->flags & FFL_SPAWNTEMP)
				b = (byte *)&st;
			else
				b = (byte *)ent;

			switch (f->type)
			{
			case F_LSTRING:
				*(char **)(b+f->ofs) = ED_NewString (value);
				break;
			case F_VECTOR:
				sscanf (value, "%f %f %f", &vec[0], &vec[1], &vec[2]);
				((float *)(b+f->ofs))[0] = vec[0];
				((float *)(b+f->ofs))[1] = vec[1];
				((float *)(b+f->ofs))[2] = vec[2];
				break;
			case F_INT:
///				*(int *)(b+f->ofs) = atoi(value);
				*(int *)(b+f->ofs) = strtoul(value,&end_ptr,10);	// чтоб работал бит	DOOR_FIRING_ON_END_CLOSING = 0x80000000
				break;
			case F_FLOAT:
				*(float *)(b+f->ofs) = atof(value);
				break;
			case F_ANGLEHACK:
				v = atof(value);
				((float *)(b+f->ofs))[0] = 0;
				((float *)(b+f->ofs))[1] = v;
				((float *)(b+f->ofs))[2] = 0;
				break;
			case F_IGNORE:
				break;
			}
			return;
		}
	}
	gi.dprintf ("%s is not a field\n", key);
}


/*
====================
ED_ParseEdict

Parses an edict out of the given string, returning the new position
ed should be a properly initialized empty edict.
====================
*/
char *ED_ParseEdict (char *data, edict_t *ent)
{
	bool		init;
	char		keyname[256];
	char		*com_token;

	init = false;
	memset (&st, 0, sizeof(st));

// go through all the dictionary pairs
	while (1)
	{
	// parse key
		com_token = COM_Parse (&data);
		if (com_token[0] == '}' && com_token_len == 1)
			break;
		if (!data)
			gi.error ("ED_ParseEdict: EOF without closing brace");

		strncpy (keyname, com_token, sizeof(keyname)-1);

	// parse value
		com_token = COM_Parse (&data);
		if (!data)
			gi.error ("ED_ParseEdict: EOF without closing brace");

		if (com_token[0] == '}' && com_token_len == 1)
			gi.error ("ED_ParseEdict: closing brace without data");

		init = true;

	// keynames with a leading underscore are used for utility comments,
	// and are immediately discarded by quake
		ED_ParseField (keyname, com_token, ent);
	}

	if (!init)
		memset (ent, 0, sizeof(*ent));

	return data;
}


gitem_t	*FindItem (char *pickup_name)
{
	int		i;
	gitem_t	*it;

	it = itemlist;
	for (i=0 ; i<game.num_items ; i++, it++)
	{
		if (!it->pickup_name)
			continue;
		if (!Q_strcasecmp(it->pickup_name, pickup_name))
			return it;
	}
	return NULL;
}


/*
===============
PrecacheItem

Precaches all data needed for a given item.
This will be called for each item spawned in a level,
and for each item in each client's inventory.
===============
*/
void PrecacheItem (gitem_t *it)
{
	char	*s, *start;
	char	data[MAX_QPATH];
	int		len;
	gitem_t	*ammo;

	if (!it)
		return;

	if (it->pickup_sound)
		gi.soundindex (it->pickup_sound);
	if (it->world_model)
		gi.modelindex (it->world_model);
	if (it->view_model)
		gi.modelindex (it->view_model);
	if (it->icon)
		gi.imageindex (it->icon);

	// parse everything for its ammo
	if (it->ammo && it->ammo[0])
	{
		ammo = FindItem (it->ammo);
		if (ammo != it)
			PrecacheItem (ammo);
	}

	// parse the space seperated precache string for other items
	s = it->precaches;
	if (!s || !s[0])
		return;

	while (*s)
	{
		start = s;
		while (*s && *s != ' ')
			s++;

		len = s-start;
		if (len >= MAX_QPATH || len < 5)
			gi.error ("PrecacheItem: %s has bad precache string", it->classname);
		memcpy (data, start, len);
		data[len] = 0;
		if (*s)
			s++;

		// determine type based on extension
		if (!Q_strcasecmp(data+len-3, "md2"))
			gi.modelindex (data);
		else if (!Q_strcasecmp(data+len-3, "md3"))
			gi.modelindex (data);
		else if (!Q_strcasecmp(data+len-3, "ase"))
			gi.modelindex (data);
		else if (!Q_strcasecmp(data+len-3, "sp2"))
			gi.modelindex (data);
		else if (!Q_strcasecmp(data+len-3, "sp3"))
			gi.modelindex (data);
		else if (!Q_strcasecmp(data+len-3, "wav"))
			gi.soundindex (data);
		else if (!Q_strcasecmp(data+len-3, "tga"))
			gi.imageindex (data);
		else if (!Q_strcasecmp(data+len-3, "pcx"))
			gi.imageindex (data);
		else if (!Q_strcasecmp(data+len-3, "jpg"))
			gi.imageindex (data);
		else if (!Q_strcasecmp(data+len-3, "dds"))
			gi.imageindex (data);
		else if (!Q_strcasecmp(data+len-3, "png"))
			gi.imageindex (data);
		else if (!Q_strcasecmp(data+len-3, "wal"))
			gi.imageindex (data);
	}
}


int ArmorIndex (edict_t *ent)
{
	if (!ent->client)
		return 0;

	if (ent->client->pers.inventory[jacket_armor_index] > 0)
		return jacket_armor_index;

	if (ent->client->pers.inventory[combat_armor_index] > 0)
		return combat_armor_index;

	if (ent->client->pers.inventory[body_armor_index] > 0)
		return body_armor_index;

	return 0;
}


bool Pickup_Armor (edict_t *ent, edict_t *other)
{
	int				old_armor_index;
	gitem_armor_t	*oldinfo;
	gitem_armor_t	*newinfo;
	int				newcount;
	float			salvage;
	int				salvagecount;

	// get info on new armor
	newinfo = (gitem_armor_t *)ent->item->info;

	old_armor_index = ArmorIndex (other);

	// handle armor shards specially
	if (ent->item->tag == ARMOR_SHARD)
	{
		if (!old_armor_index)
			other->client->pers.inventory[jacket_armor_index] = 2;
		else
			other->client->pers.inventory[old_armor_index] += 2;
	}

	// if player has no armor, just use it
	else if (!old_armor_index)
	{
		other->client->pers.inventory[ITEM_INDEX(ent->item)] = newinfo->base_count;
	}

	// use the better armor
	else
	{
		// get info on old armor
		if (old_armor_index == jacket_armor_index)
			oldinfo = &jacketarmor_info;
		else if (old_armor_index == combat_armor_index)
			oldinfo = &combatarmor_info;
		else // (old_armor_index == body_armor_index)
			oldinfo = &bodyarmor_info;

		if (newinfo->normal_protection > oldinfo->normal_protection)
		{
			// calc new armor values
			salvage = oldinfo->normal_protection / newinfo->normal_protection;
			salvagecount = salvage * other->client->pers.inventory[old_armor_index];
			newcount = newinfo->base_count + salvagecount;
			if (newcount > newinfo->max_count)
				newcount = newinfo->max_count;

			// zero count of old armor so it goes away
			other->client->pers.inventory[old_armor_index] = 0;

			// change armor to new item with computed value
			other->client->pers.inventory[ITEM_INDEX(ent->item)] = newcount;
		}
		else
		{
			// calc new armor values
			salvage = newinfo->normal_protection / oldinfo->normal_protection;
			salvagecount = salvage * newinfo->base_count;
			newcount = other->client->pers.inventory[old_armor_index] + salvagecount;
			if (newcount > oldinfo->max_count)
				newcount = oldinfo->max_count;

			// if we're already maxed out then we don't need the new armor
			if (other->client->pers.inventory[old_armor_index] >= newcount)
				return false;

			// update current armor value
			other->client->pers.inventory[old_armor_index] = newcount;
		}
	}

	if (!(ent->spawnflags & DROPPED_ITEM) && (deathmatch->value))
		SetRespawn (ent, 20);

	return true;
}


bool Pickup_PowerArmor (edict_t *ent, edict_t *other)
{
	int		quantity;

	quantity = other->client->pers.inventory[ITEM_INDEX(ent->item)];

	other->client->pers.inventory[ITEM_INDEX(ent->item)]++;

	if (deathmatch->value)
	{
		if (!(ent->spawnflags & DROPPED_ITEM) )
			SetRespawn (ent, ent->item->quantity);
		// auto-use for DM only if we didn't already have one
		if (!quantity)
			ent->item->use (other, ent->item);
	}

	return true;
}


void ChaseNext(edict_t *ent)
{
	int i;
	edict_t *e;

	if (!ent->client->chase_target)
		return;

	i = ent->client->chase_target - g_edicts;
	do {
		i++;
		if (i > maxclients_cvar->value)
			i = 1;
		e = g_edicts + i;
		if (!e->inuse)
			continue;
		if (!e->client->resp.spectator)
			break;
	} while (e != ent->client->chase_target);

	ent->client->chase_target = e;
	ent->client->update_chase = true;
}


void ChasePrev(edict_t *ent)
{
	int i;
	edict_t *e;

	if (!ent->client->chase_target)
		return;

	i = ent->client->chase_target - g_edicts;
	do {
		i--;
		if (i < 1)
			i = maxclients_cvar->value;
		e = g_edicts + i;
		if (!e->inuse)
			continue;
		if (!e->client->resp.spectator)
			break;
	} while (e != ent->client->chase_target);

	ent->client->chase_target = e;
	ent->client->update_chase = true;
}


void SelectNextItem (edict_t *ent, int itflags)
{
	gclient_t	*cl;
	int			i, index;
	gitem_t		*it;

	cl = ent->client;

	if (cl->chase_target) {
		ChaseNext(ent);
		return;
	}

	// scan  for the next valid one
	for (i=1 ; i<=MAX_ITEMS ; i++)
	{
		index = (cl->pers.selected_item + i)%MAX_ITEMS;
		if (!cl->pers.inventory[index])
			continue;
		it = &itemlist[index];
		if (!it->use)
			continue;
		if (!(it->flags & itflags))
			continue;

		cl->pers.selected_item = index;
		return;
	}

	cl->pers.selected_item = -1;
}

void SelectPrevItem (edict_t *ent, int itflags)
{
	gclient_t	*cl;
	int			i, index;
	gitem_t		*it;

	cl = ent->client;

	if (cl->chase_target) {
		ChasePrev(ent);
		return;
	}

	// scan  for the next valid one
	for (i=1 ; i<=MAX_ITEMS ; i++)
	{
		index = (cl->pers.selected_item + MAX_ITEMS - i)%MAX_ITEMS;
		if (!cl->pers.inventory[index])
			continue;
		it = &itemlist[index];
		if (!it->use)
			continue;
		if (!(it->flags & itflags))
			continue;

		cl->pers.selected_item = index;
		return;
	}

	cl->pers.selected_item = -1;
}


void ValidateSelectedItem (edict_t *ent)
{
	gclient_t	*cl;

	cl = ent->client;

	if (cl->pers.inventory[cl->pers.selected_item])
		return;		// valid

	SelectNextItem (ent, -1);
}


/*QUAKED info_null (0 0.5 0) (-4 -4 -4) (4 4 4)
Used as a positional target for spotlights, etc.
*/
void SP_info_null (edict_t *self)
{
	G_FreeEdict (self);
};


/*QUAKED info_notnull (0 0.5 0) (-4 -4 -4) (4 4 4)
Used as a positional target for lightning.
*/
void SP_info_notnull (edict_t *self)
{
	VectorCopy (self->s.origin, self->absmin);
	VectorCopy (self->s.origin, self->absmax);
};


//==========================================================

/*QUAKED target_lightramp (0 .5 .8) (-8 -8 -8) (8 8 8) TOGGLE
speed		How many seconds the ramping will take
message		two letters; starting lightlevel and ending lightlevel
*/

void target_lightramp_think (edict_t *self)
{
	char	style[2];

	style[0] = 'a' + self->movedir[0] + (level.time - self->timestamp) / FRAMETIME * self->movedir[2];
	style[1] = 0;
	if (net_compatibility->value)
		gi.configstring (CS_LIGHTS_Q2+self->enemy->style, style);
	else
		gi.configstring (CS_LIGHTS_BERS+self->enemy->style, style);

	if ((level.time - self->timestamp) < self->speed)
	{
		self->nextthink = level.time + FRAMETIME;
	}
	else if (self->spawnflags & 1)
	{
		char	temp;

		temp = self->movedir[0];
		self->movedir[0] = self->movedir[1];
		self->movedir[1] = temp;
		self->movedir[2] *= -1;
	}
}

void target_lightramp_use (edict_t *self, edict_t *other, edict_t *activator)
{
	if (!self->enemy)
	{
		edict_t		*e;

		// check all the targets
		e = NULL;
		while (1)
		{
			e = G_Find (e, FOFS(targetname), self->target, "target_lightramp_use");
			if (!e)
				break;
			if (strcmp(e->classname, "light") != 0)
			{
				gi.dprintf("%s at %s ", self->classname, vtos(self->s.origin));
				gi.dprintf("target %s (%s at %s) is not a light\n", self->target, e->classname, vtos(e->s.origin));
			}
			else
			{
				self->enemy = e;
			}
		}

		if (!self->enemy)
		{
			gi.dprintf("%s target %s not found at %s\n", self->classname, self->target, vtos(self->s.origin));
			G_FreeEdict (self);
			return;
		}
	}

	self->timestamp = level.time;
	target_lightramp_think (self);
}

void SP_target_lightramp (edict_t *self)
{
	if (!self->message || strlen(self->message) != 2 || self->message[0] < 'a' || self->message[0] > 'z' || self->message[1] < 'a' || self->message[1] > 'z' || self->message[0] == self->message[1])
	{
		gi.dprintf("target_lightramp has bad ramp (%s) at %s\n", self->message, vtos(self->s.origin));
		G_FreeEdict (self);
		return;
	}

	if (deathmatch->value)
	{
		G_FreeEdict (self);
		return;
	}

	if (!self->target)
	{
		gi.dprintf("%s with no target at %s\n", self->classname, vtos(self->s.origin));
		G_FreeEdict (self);
		return;
	}

	self->svflags |= SVF_NOCLIENT;
	self->use = target_lightramp_use;
	self->think = target_lightramp_think;

	self->movedir[0] = self->message[0] - 'a';
	self->movedir[1] = self->message[1] - 'a';
	self->movedir[2] = (self->movedir[1] - self->movedir[0]) / (self->speed / FRAMETIME);
}


void Use_Quad (edict_t *ent, gitem_t *item)
{
	int		timeout;

	ent->client->pers.inventory[ITEM_INDEX(item)]--;
	ValidateSelectedItem (ent);

	if (quad_drop_timeout_hack)
	{
		timeout = quad_drop_timeout_hack;
		quad_drop_timeout_hack = 0;
	}
	else
	{
		timeout = 300;
	}

	if (ent->client->quad_framenum > level.framenum)
		ent->client->quad_framenum += timeout;
	else
		ent->client->quad_framenum = level.framenum + timeout;

	gi.sound(ent, CHAN_ITEM, gi.soundindex("items/damage.wav"), 1, ATTN_NORM, 0);
}


bool Pickup_Powerup (edict_t *ent, edict_t *other)
{
	int		quantity;

	if (net_compatibility->value)
		if (!strcmp(ent->item->classname, "item_invisibility"))
			return false;

	quantity = other->client->pers.inventory[ITEM_INDEX(ent->item)];
	if ((skill->value == 1 && quantity >= 2) || (skill->value >= 2 && quantity >= 1))
		return false;

	if ((coop->value) && (ent->item->flags & IT_STAY_COOP) && (quantity > 0))
		return false;

	other->client->pers.inventory[ITEM_INDEX(ent->item)]++;

	if (deathmatch->value)
	{
		if (!(ent->spawnflags & DROPPED_ITEM) )
			SetRespawn (ent, ent->item->quantity);
		if (((int)dmflags->value & DF_INSTANT_ITEMS) || ((ent->item->use == Use_Quad) && (ent->spawnflags & DROPPED_PLAYER_ITEM)))
		{
			if ((ent->item->use == Use_Quad) && (ent->spawnflags & DROPPED_PLAYER_ITEM))
				quad_drop_timeout_hack = (ent->nextthink - level.time) / FRAMETIME;
			ent->item->use (other, ent->item);
		}
	}

	return true;
}


bool Pickup_Adrenaline (edict_t *ent, edict_t *other)
{
	if (!deathmatch->value)
		other->max_health += 1;

	if (other->health < other->max_health)
		other->health = other->max_health;

	if (!(ent->spawnflags & DROPPED_ITEM) && (deathmatch->value))
		SetRespawn (ent, ent->item->quantity);

	return true;
}


bool Pickup_AncientHead (edict_t *ent, edict_t *other)
{
	other->max_health += 2;

	if (!(ent->spawnflags & DROPPED_ITEM) && (deathmatch->value))
		SetRespawn (ent, ent->item->quantity);

	return true;
}


void Think_Delay (edict_t *ent)
{
	G_UseTargets (ent, ent->activator, NULL);
	G_FreeEdict (ent);
}


/*
=============
G_Find

Searches all active entities for the next one that holds
the matching string at fieldofs (use the FOFS() macro) in the structure.

Searches beginning at the edict after from, or the beginning if NULL
NULL will be returned if the end of the list is reached.

=============
*/
edict_t *G_Find (edict_t *from, int fieldofs, char *match, char *func)
{
	char	*s;
	bool	spy;

	if (func)
		spy = (fieldofs == FOFS(targetname));		// отслеживать будем обращения к TargetName
	else
		spy = false;

	if (!from)
		from = g_edicts;
	else
		from++;

	for ( ; from < &g_edicts[globals.num_edicts] ; from++)
	{
		if (!from->inuse)
			continue;
		s = *(char **) ((byte *)from + fieldofs);
		if (!s)
			continue;
		if (!Q_strcasecmp (s, match))
		{
			if (spy)
				gi.dprintf/*Com_Printf*/ ("\"%s\" activated \"%s\"\n", func, s);
			return from;
		}
	}

	return NULL;
}


/*
==============================
G_UseTargets

the global "activator" should be set to the entity that initiated the firing.

If self.delay is set, a DelayedUse entity will be created that will actually
do the SUB_UseTargets after that many seconds have passed.

Centerprints any self.message to the activator.

Search for (string)targetname in all entities that
match (string)self.target and call their .use function

==============================
*/
void G_UseTargets (edict_t *ent, edict_t *activator, char *func)
{
	edict_t		*t;

//
// check for a delay
//
	if (ent->delay)
	{
	// create a temp object to fire at a later time
		t = G_Spawn();
		t->classname = "DelayedUse";
		t->nextthink = level.time + ent->delay;
		t->think = Think_Delay;
		t->activator = activator;
		if (!activator)
			gi.dprintf ("Think_Delay with no activator\n");
		t->message = ent->message;
		t->target = ent->target;
		t->killtarget = ent->killtarget;
		return;
	}


//
// print the message
//
	if ((ent->message) && !(activator->svflags & SVF_MONSTER))
	{
		gi.centerprintf (activator, "%s", ent->message);
		if (ent->noise_index)
			gi.sound (activator, CHAN_AUTO, ent->noise_index, 1, ATTN_NORM, 0);
		else
			gi.sound (activator, CHAN_AUTO, gi.soundindex ("misc/talk1.wav"), 1, ATTN_NORM, 0);
	}

//
// kill killtargets
//
	if (ent->killtarget)
	{
		t = NULL;
		while ((t = G_Find (t, FOFS(targetname), ent->killtarget, NULL)))
		{
			G_FreeEdict (t);
			if (!ent->inuse)
			{
				gi.dprintf("entity was removed while using killtargets\n");
				return;
			}
		}
	}

//
// fire targets
//
	if (ent->target)
	{
		t = NULL;
		while ((t = G_Find (t, FOFS(targetname), ent->target, func)))
		{
			// doors fire area portals in a specific way
			if (!Q_strcasecmp(t->classname, "func_areaportal") && (!Q_strcasecmp(ent->classname, "func_door") || !Q_strcasecmp(ent->classname, "func_door_rotating")))
				continue;

			if (t == ent)
			{
				gi.dprintf ("WARNING: Entity used itself.\n");
			}
			else
			{
				if (t->use)
					t->use (t, ent, activator);
			}
			if (!ent->inuse)
			{
				gi.dprintf("entity was removed while using targets\n");
				return;
			}
		}
	}
}


void Touch_Item (edict_t *ent, edict_t *other, cplane_t *plane, csurface_t *surf)
{
	bool	taken;

	if (!other->client)
		return;
	if (other->health < 1)
		return;		// dead people can't pickup
	if (!ent->item || !ent->item->pickup)		/// Berserker: fixed q2 bug: в DMatch: "Give all" при запрещенных предметах (например броня) - вылет игры!!!
		return;		// not a grabbable item?

	taken = ent->item->pickup(ent, other);

	if (taken)
	{
		// flash the screen
		other->client->bonus_alpha = 0.25;

		// show icon and name on status bar
		other->client->ps.stats[STAT_PICKUP_ICON] = gi.imageindex(ent->item->icon);
		if (net_compatibility->value)
			other->client->ps.stats[STAT_PICKUP_STRING] = CS_ITEMS_Q2+ITEM_INDEX(ent->item);
		else
			other->client->ps.stats[STAT_PICKUP_STRING] = CS_ITEMS_BERS+ITEM_INDEX(ent->item);
		other->client->pickup_msg_time = level.time + 3.0;

		// change selected item
		if (ent->item->use)
			other->client->pers.selected_item = other->client->ps.stats[STAT_SELECTED_ITEM] = ITEM_INDEX(ent->item);

		if (ent->item->pickup == Pickup_Health)
		{
			if (ent->count == 2)
				gi.sound(other, CHAN_ITEM, gi.soundindex("items/s_health.wav"), 1, ATTN_NORM, 0);
			else if (ent->count == 10)
				gi.sound(other, CHAN_ITEM, gi.soundindex("items/n_health.wav"), 1, ATTN_NORM, 0);
			else if (ent->count == 25)
				gi.sound(other, CHAN_ITEM, gi.soundindex("items/l_health.wav"), 1, ATTN_NORM, 0);
			else // (ent->count == 100)
				gi.sound(other, CHAN_ITEM, gi.soundindex("items/m_health.wav"), 1, ATTN_NORM, 0);
		}
		else if (ent->item->pickup_sound)
		{
			gi.sound(other, CHAN_ITEM, gi.soundindex(ent->item->pickup_sound), 1, ATTN_NORM, 0);
		}
	}

	if (!(ent->spawnflags & ITEM_TARGETS_USED))
	{
		G_UseTargets (ent, other, "touch_item");
		ent->spawnflags |= ITEM_TARGETS_USED;
	}

	if (!taken || it_stay)
		return;

	if (!((coop->value) &&  (ent->item->flags & IT_STAY_COOP)) || (ent->spawnflags & (DROPPED_ITEM | DROPPED_PLAYER_ITEM)))
	{
		if (ent->flags & FL_RESPAWN)
			ent->flags &= ~FL_RESPAWN;
		else
			G_FreeEdict (ent);
	}
}


void Use_Item (edict_t *ent, edict_t *other, edict_t *activator)
{
	ent->svflags &= ~SVF_NOCLIENT;
	ent->use = NULL;

	if (ent->spawnflags & ITEM_NO_TOUCH)
	{
		ent->solid = SOLID_BBOX;
		ent->touch = NULL;
	}
	else
	{
		ent->solid = SOLID_TRIGGER;
		ent->touch = Touch_Item;
	}

	gi.linkentity (ent);
}


void droptofloor (edict_t *ent)
{
	trace_t		tr;
	vec3_t		dest;
	float		*v;

	v = tv(-15,-15,-15);
	VectorCopy (v, ent->mins);
	v = tv(15,15,15);
	VectorCopy (v, ent->maxs);

	if (ent->model)
		gi.setmodel (ent, ent->model);
	else
		gi.setmodel (ent, ent->item->world_model);
	ent->solid = SOLID_TRIGGER;
	ent->movetype = MOVETYPE_TOSS;
	ent->touch = Touch_Item;

	v = tv(0,0,-128);
	VectorAdd (ent->s.origin, v, dest);

	tr = gi.trace (ent->s.origin, ent->mins, ent->maxs, dest, ent, MASK_SOLID);
	if (tr.startsolid)
	{
		gi.dprintf ("droptofloor: %s startsolid at %s\n", ent->classname, vtos(ent->s.origin));
		G_FreeEdict (ent);
		return;
	}

	VectorCopy (tr.endpos, ent->s.origin);

	if (ent->team)
	{
		ent->flags &= ~FL_TEAMSLAVE;
		ent->chain = ent->teamchain;
		ent->teamchain = NULL;

		ent->svflags |= SVF_NOCLIENT;
		ent->solid = SOLID_NOT;
		if (ent == ent->teammaster)
		{
			ent->nextthink = level.time + FRAMETIME;
			ent->think = DoRespawn;
		}
	}

	if (ent->spawnflags & ITEM_NO_TOUCH)
	{
		ent->solid = SOLID_BBOX;
		ent->touch = NULL;
		ent->s.effects &= ~EF_ROTATE;
///		ent->s.renderfx &= ~RF_GLOW;
	}

	if (ent->spawnflags & ITEM_TRIGGER_SPAWN)
	{
		ent->svflags |= SVF_NOCLIENT;
		ent->solid = SOLID_NOT;
		ent->use = Use_Item;
	}

	gi.linkentity (ent);
}


void Drop_Weapon (edict_t *ent, gitem_t *item)
{
	int		index;

	if ((int)(dmflags->value) & DF_WEAPONS_STAY)
		return;

	index = ITEM_INDEX(item);
	// see if we're already using it
	if ( ((item == ent->client->pers.weapon) || (item == ent->client->newweapon))&& (ent->client->pers.inventory[index] == 1) )
	{
		gi.cprintf (ent, PRINT_HIGH, "Can't drop current weapon\n");
		return;
	}

	Drop_Item (ent, item);
	ent->client->pers.inventory[index]--;
}


void Weapon_HyperBlaster_Fire (edict_t *ent)
{
	float	rotation;
	vec3_t	offset;
	int		effect;
	int		damage;

	ent->client->weapon_sound = gi.soundindex("weapons/hyprbl1a.wav");

	if (!(ent->client->buttons & BUTTON_ATTACK))
	{
		ent->client->ps.gunframe++;
	}
	else
	{
		if (! ent->client->pers.inventory[ent->client->ammo_index] )
		{
			if (level.time >= ent->pain_debounce_time)
			{
				gi.sound(ent, CHAN_VOICE, gi.soundindex("weapons/noammo.wav"), 1, ATTN_NORM, 0);
				ent->pain_debounce_time = level.time + 1;
			}
			NoAmmoWeaponChange (ent);
		}
		else
		{
			float s, c;
			rotation = (ent->client->ps.gunframe - 5) * 2*M_PI/6;
			SinCos(rotation, &s, &c);
			offset[0] = -4 * s;
			offset[1] = 0;
			offset[2] = 4 * c;

			if ((ent->client->ps.gunframe == 6) || (ent->client->ps.gunframe == 9))
				effect = EF_HYPERBLASTER;
			else
				effect = 0;
			if (deathmatch->value)
				damage = 15;
			else
				damage = 20;
			Blaster_Fire (ent, offset, damage, true, effect);
			if (! ( (int)dmflags->value & DF_INFINITE_AMMO ) )
				ent->client->pers.inventory[ent->client->ammo_index]--;

			ent->client->anim_priority = ANIM_ATTACK;
			if (ent->client->ps.pmove.pm_flags & PMF_DUCKED)
			{
				ent->s.frame = PLAYER_FRAME_crattak1 - 1;
				ent->client->anim_end = PLAYER_FRAME_crattak9;
			}
			else
			{
				ent->s.frame = PLAYER_FRAME_attack1 - 1;
				ent->client->anim_end = PLAYER_FRAME_attack8;
			}
		}

		ent->client->ps.gunframe++;
		if (ent->client->ps.gunframe == 12 && ent->client->pers.inventory[ent->client->ammo_index])
			ent->client->ps.gunframe = 6;
	}

	if (ent->client->ps.gunframe == 12)
	{
		gi.sound(ent, CHAN_AUTO, gi.soundindex("weapons/hyprbd1a.wav"), 1, ATTN_NORM, 0);
		ent->client->weapon_sound = 0;
	}

}


void Weapon_HyperBlaster (edict_t *ent)
{
	static int	pause_frames[]	= {0};
	static int	fire_frames[]	= {6, 7, 8, 9, 10, 11, 0};

	Weapon_Generic (ent, 5, 20, 49, 53, pause_frames, fire_frames, Weapon_HyperBlaster_Fire);
}


/*
=================
fire_rail
=================
*/
void fire_rail (edict_t *self, vec3_t start, vec3_t aimdir, int damage, int kick)
{
	vec3_t		from;
	vec3_t		end;
	trace_t		tr;
	edict_t		*ignore;
	int			mask;
	bool		water;
	BYTE		tmp;

	if (net_compatibility->value)
		VectorMA (start, 8192, aimdir, end);
	else
		VectorMA (start, 32768, aimdir, end);	// увеличил чтоб доставало до любого места на карте (LARGE_MAP_SIZE)
	VectorCopy (start, from);
	ignore = self;
	water = false;
	mask = MASK_SHOT|CONTENTS_SLIME|CONTENTS_LAVA;
	while (ignore)
	{
		tr = gi.trace (from, NULL, NULL, end, ignore, mask);

		if (tr.contents & (CONTENTS_SLIME|CONTENTS_LAVA))
		{
			mask &= ~(CONTENTS_SLIME|CONTENTS_LAVA);
			water = true;
		}
		else
		{
			//ZOID--added so rail goes through SOLID_BBOX entities (gibs, etc)
			if ((tr.ent->svflags & SVF_MONSTER) || (tr.ent->client) || (tr.ent->solid == SOLID_BBOX))
				ignore = tr.ent;
			else
				ignore = NULL;

			if ((tr.ent != self) && (tr.ent->takedamage))
				T_Damage (tr.ent, self, self, aimdir, tr.endpos, tr.plane.normal, damage, kick, 0, MOD_RAILGUN);
			else				// Nick so back to the end of the loop here - need to stop it!
				ignore = NULL;	// Nick fix the lock in this 'while (ignore)' loop?
		}

		VectorCopy (tr.endpos, from);
	}

	// send gun puff / flash
	bool large_map = LargeMapCheck(start) | LargeMapCheck(tr.endpos);
	if (large_map)
		gi.WriteByte (svc_temp_entity_ext);
	else
		gi.WriteByte (svc_temp_entity);
	if(net_compatibility->value)
	{
old_rail:
		gi.WriteByte (TE_RAILTRAIL);
		gi.WritePosition (start, large_map);
		gi.WritePosition (tr.endpos, large_map);
		gi.multicast (self->s.origin, MULTICAST_PHS);
		if (water)
		{
			if (large_map)
				gi.WriteByte (svc_temp_entity_ext);
			else
				gi.WriteByte (svc_temp_entity);
			gi.WriteByte (TE_RAILTRAIL);
			gi.WritePosition (start, large_map);
			gi.WritePosition (tr.endpos, large_map);
			gi.multicast (tr.endpos, MULTICAST_PHS);
		}
	}
	else
	{
		if(!self->client)
			goto old_rail;	// не клиенты (монстры) стреляют старой рельсой, сэкономим сетевой трафик
		tmp = self->client->pers.railtrailcore & 0xf;
///		if (!tmp)	// Berserker: при 0 просто не будем рисовать rail core
///			tmp = 1 + rand() % 7;
		BYTE byt = (tmp << 4) + (self->client->pers.railtrailspiral & 0xf);
        if (self->client->pers.railtrailradius==6 && byt==0x71)
			goto old_rail;	// если клиент пользуется настройками по умолчанию, сэкономим сетевой трафик

		gi.WriteByte (TE_RAILTRAIL_BERS_NEW);
		gi.WritePosition (start, large_map);
		gi.WritePosition (tr.endpos, large_map);
		if((tr.surface) && (tr.surface->flags & SURF_SKY))
			gi.WriteDir (vec3_origin);
		else
			gi.WriteDir (tr.plane.normal);
		gi.WriteByte(byt);
		gi.WriteByte(self->client->pers.railtrailradius);
		gi.multicast (self->s.origin, MULTICAST_PHS);

		if (water)
		{
			if (large_map)
				gi.WriteByte (svc_temp_entity_ext);
			else
				gi.WriteByte (svc_temp_entity);
			gi.WriteByte (TE_RAILTRAIL_BERS_NEW);
			gi.WritePosition (start, large_map);
			gi.WritePosition (tr.endpos, large_map);
			if((tr.surface) && (tr.surface->flags & SURF_SKY))
				gi.WriteDir (vec3_origin);
			else
				gi.WriteDir (tr.plane.normal);
			gi.WriteByte(byt);
			gi.WriteByte(self->client->pers.railtrailradius);
			gi.multicast (tr.endpos, MULTICAST_PHS);
		}
	}

	if (self->client)
		PlayerNoise(self, tr.endpos, PNOISE_IMPACT);
}


/*
=================
fire_bfg
=================
*/
void bfg_explode (edict_t *self)
{
	edict_t	*ent;
	float	points;
	vec3_t	v;
	float	dist;

	if (self->s.frame == 0)
	{
		// the BFG effect
		ent = NULL;
		while ((ent = findradius(ent, self->s.origin, self->dmg_radius)) != NULL)
		{
			if (!ent->takedamage)
				continue;
			if (ent == self->owner)
				continue;
			if (!CanDamage (ent, self))
				continue;
			if (!CanDamage (ent, self->owner))
				continue;

			VectorAdd (ent->mins, ent->maxs, v);
			VectorMA (ent->s.origin, 0.5, v, v);
			VectorSubtract (self->s.origin, v, v);
			dist = VectorLength(v);
			points = self->radius_dmg * (1.0 - sqrt(dist/self->dmg_radius));
			if (ent == self->owner)
				points = points * 0.5;

			bool large_map = LargeMapCheck(ent->s.origin);
			if (large_map)
				gi.WriteByte (svc_temp_entity_ext);
			else
				gi.WriteByte (svc_temp_entity);
			gi.WriteByte (TE_BFG_EXPLOSION);
			gi.WritePosition (ent->s.origin, large_map);
			gi.multicast (ent->s.origin, MULTICAST_PHS);
			T_Damage (ent, self, self->owner, self->velocity, ent->s.origin, vec3_origin, (int)points, 0, DAMAGE_ENERGY, MOD_BFG_EFFECT);
		}
	}

	self->nextthink = level.time + FRAMETIME;
	self->s.frame++;
	if (self->s.frame == 5)
		self->think = G_FreeEdict;
}

void bfg_touch (edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf)
{
	if (other == self->owner)
		return;

	if (surf && (surf->flags & SURF_SKY))
	{
		G_FreeEdict (self);
		return;
	}

	if (self->owner->client)
		PlayerNoise(self->owner, self->s.origin, PNOISE_IMPACT);

	// core explosion - prevents firing it into the wall/floor
	if (other->takedamage)
		T_Damage (other, self, self->owner, self->velocity, self->s.origin, plane->normal, 200, 0, 0, MOD_BFG_BLAST);
	T_RadiusDamage(self, self->owner, 200, other, 100, MOD_BFG_BLAST);

	gi.sound (self, CHAN_VOICE, gi.soundindex ("weapons/bfg__x1b.wav"), 1, ATTN_NORM, 0);
	self->solid = SOLID_NOT;
	self->touch = NULL;
	VectorMA (self->s.origin, -1 * FRAMETIME, self->velocity, self->s.origin);
	VectorClear (self->velocity);
	self->s.modelindex = gi.modelindex ("sprites/s_bfg3.sp2");
	self->s.frame = 0;
	self->s.sound = 0;
	self->s.effects &= ~EF_ANIM_ALLFAST;
	self->think = bfg_explode;
	self->nextthink = level.time + FRAMETIME;
	self->enemy = other;

	bool large_map = LargeMapCheck(self->s.origin);
	if (large_map)
		gi.WriteByte (svc_temp_entity_ext);
	else
		gi.WriteByte (svc_temp_entity);
	gi.WriteByte (TE_BFG_BIGEXPLOSION);
	gi.WritePosition (self->s.origin, large_map);
	gi.multicast (self->s.origin, MULTICAST_PVS);
}


void bfg_think (edict_t *self)
{
	edict_t	*ent;
	edict_t	*ignore;
	vec3_t	point;
	vec3_t	dir;
	vec3_t	start;
	vec3_t	end;
	int		dmg, limit;
	trace_t	tr;

	if (deathmatch->value)
		dmg = 5;
	else
		dmg = 10;

	ent = NULL;
	while ((ent = findradius(ent, self->s.origin, 256)) != NULL)
	{
		if (ent == self)
			continue;

		if (ent == self->owner)
			continue;

		if (!ent->takedamage)
			continue;

		if (!(ent->svflags & SVF_MONSTER) && (!ent->client) && (strcmp(ent->classname, "misc_explobox") != 0))
			continue;

		VectorMA (ent->absmin, 0.5, ent->size, point);

		VectorSubtract (point, self->s.origin, dir);
		VectorNormalize (dir);

		ignore = self;
		VectorCopy (self->s.origin, start);
		VectorMA (start, 2048, dir, end);

		limit = 32;		/// Berserker: fixed infinity loop
		while(limit)
		{
			tr = gi.trace (start, NULL, NULL, end, ignore, CONTENTS_SOLID|CONTENTS_MONSTER|CONTENTS_DEADMONSTER);

			if (!tr.ent)
				break;

			// hurt it if we can
			if ((tr.ent->takedamage) && !(tr.ent->flags & FL_IMMUNE_LASER) && (tr.ent != self->owner))
				T_Damage (tr.ent, self, self->owner, dir, tr.endpos, vec3_origin, dmg, 1, DAMAGE_ENERGY, MOD_BFG_LASER);

			// if we hit something that's not a monster or player we're done
			if (!(tr.ent->svflags & SVF_MONSTER) && (!tr.ent->client))
			{
				bool large_map = LargeMapCheck(tr.endpos);
				if (large_map)
					gi.WriteByte (svc_temp_entity_ext);
				else
					gi.WriteByte (svc_temp_entity);
				gi.WriteByte (TE_LASER_SPARKS);
				gi.WriteByte (4);
				gi.WritePosition (tr.endpos, large_map);
				gi.WriteDir (tr.plane.normal);
				gi.WriteByte (self->s.skinnum);
				gi.multicast (tr.endpos, MULTICAST_PVS);
				break;
			}

			ignore = tr.ent;
			VectorCopy (tr.endpos, start);

			limit--;
		}

		bool large_map = LargeMapCheck(self->s.origin) | LargeMapCheck(tr.endpos);
		if (large_map)
			gi.WriteByte (svc_temp_entity_ext);
		else
			gi.WriteByte (svc_temp_entity);
		gi.WriteByte (TE_BFG_LASER);
		gi.WritePosition (self->s.origin, large_map);
		gi.WritePosition (tr.endpos, large_map);
		gi.multicast (self->s.origin, MULTICAST_PHS);
	}

	self->nextthink = level.time + FRAMETIME;
}


void fire_bfg (edict_t *self, vec3_t start, vec3_t dir, int damage, int speed, float damage_radius)
{
	edict_t	*bfg;

	bfg = G_Spawn();
	VectorCopy (start, bfg->s.origin);
	VectorCopy (dir, bfg->movedir);
	vectoangles (dir, bfg->s.angles);
	VectorScale (dir, speed, bfg->velocity);
	bfg->movetype = MOVETYPE_FLYMISSILE;
	bfg->clipmask = MASK_SHOT;
	bfg->solid = SOLID_BBOX;
	bfg->s.effects |= EF_BFG | EF_ANIM_ALLFAST;
	VectorClear (bfg->mins);
	VectorClear (bfg->maxs);
	bfg->s.modelindex = gi.modelindex ("sprites/s_bfg1.sp2");
	bfg->owner = self;
	bfg->touch = bfg_touch;
	if (net_compatibility->value)
		bfg->nextthink = level.time + 8000/speed;
	else
		bfg->nextthink = level.time + 32768/speed;		// LARGE_MAP_SIZE
	bfg->think = G_FreeEdict;
	bfg->radius_dmg = damage;
	bfg->dmg_radius = damage_radius;
	bfg->classname = "bfg blast";
	bfg->s.sound = gi.soundindex ("weapons/bfg__l1a.wav");

	bfg->think = bfg_think;
	bfg->nextthink = level.time + FRAMETIME;
	bfg->teammaster = bfg;
	bfg->teamchain = NULL;

	if (self->client)
		check_dodge (self, bfg->s.origin, dir, speed);

	gi.linkentity (bfg);

/// Berserker:
/// Fixed bug: стрельба сквозь стены при помощи "сидячего" трупа =)
	if (self->client)
	{
		trace_t	tr;
		vec3_t	point;
		VectorMA (start, 16, dir, point);							// Отодвинем тестовую точку на 16 единиц в сторону стрельбы (на кончик оружия)
		tr = gi.trace (start, NULL, NULL, point, NULL, MASK_SOLID);	// Если тестовая точка находится в стене-полу-окне (не учитываем "живность" и "мясо")
		if (tr.fraction < 1 && !(tr.surface->flags & SURF_SKY))
			bfg_touch(bfg, bfg, NULL, NULL);						// то принудительно сдетонируем снаряд
	}
}


/*
======================================================================

RAILGUN

======================================================================
*/

void weapon_railgun_fire (edict_t *ent)
{
	vec3_t		start;
	vec3_t		forward, right;
	vec3_t		offset;
	int			damage;
	int			kick;

	if (deathmatch->value)
	{	// normal damage is too extreme in dm
		damage = 100;
		kick = 200;
	}
	else
	{
		damage = 150;
		kick = 250;
	}

	if (is_quad)
	{
		damage *= 4;
		kick *= 4;
	}

	AngleVectors (ent->client->v_angle, forward, right, NULL);

	VectorScale (forward, -3, ent->client->kick_origin);
	ent->client->kick_angles[0] = -3;

	VectorSet(offset, 0, 7,  ent->viewheight-8);
	P_ProjectSource (ent, offset, forward, right, start);
	fire_rail (ent, start, forward, damage, kick);

	// send muzzle flash
	gi.WriteByte (svc_muzzleflash);
	gi.WriteShort (ent-g_edicts);
	gi.WriteByte (MZ_RAILGUN | is_silenced);
	gi.multicast (ent->s.origin, MULTICAST_PVS);

	ent->client->ps.gunframe++;
	PlayerNoise(ent, start, PNOISE_WEAPON);

	if (! ( (int)dmflags->value & DF_INFINITE_AMMO ) )
		ent->client->pers.inventory[ent->client->ammo_index]--;
}


void Weapon_Railgun (edict_t *ent)
{
	static int	pause_frames[]	= {56, 0};
	static int	fire_frames[]	= {4, 0};

	Weapon_Generic (ent, 3, 18, 56, 61, pause_frames, fire_frames, weapon_railgun_fire);
}


/*
======================================================================

BFG10K

======================================================================
*/

void weapon_bfg_fire (edict_t *ent)
{
	vec3_t	offset, start;
	vec3_t	forward, right;
	int		damage;
	float	damage_radius = 1000;

	if (deathmatch->value)
		damage = 200;
	else
		damage = 500;

	if (ent->client->ps.gunframe == 9)
	{
		// send muzzle flash
		gi.WriteByte (svc_muzzleflash);
		gi.WriteShort (ent-g_edicts);
		gi.WriteByte (MZ_BFG | is_silenced);
		gi.multicast (ent->s.origin, MULTICAST_PVS);

		ent->client->ps.gunframe++;

		PlayerNoise(ent, start, PNOISE_WEAPON);
		return;
	}

	// cells can go down during windup (from power armor hits), so
	// check again and abort firing if we don't have enough now
	if (ent->client->pers.inventory[ent->client->ammo_index] < 50)
	{
		ent->client->ps.gunframe++;
		return;
	}

	if (is_quad)
		damage *= 4;

	AngleVectors (ent->client->v_angle, forward, right, NULL);

	VectorScale (forward, -2, ent->client->kick_origin);

	// make a big pitch kick with an inverse fall
	ent->client->v_dmg_pitch = -40;
	ent->client->v_dmg_roll = crandom()*8;
	ent->client->v_dmg_time = level.time + DAMAGE_TIME;

	VectorSet(offset, 8, 8, ent->viewheight-8);
	P_ProjectSource (ent, offset, forward, right, start);
	fire_bfg (ent, start, forward, damage, 400, damage_radius);

	ent->client->ps.gunframe++;

	PlayerNoise(ent, start, PNOISE_WEAPON);

	if (! ( (int)dmflags->value & DF_INFINITE_AMMO ) )
		ent->client->pers.inventory[ent->client->ammo_index] -= 50;
}

void Weapon_BFG (edict_t *ent)
{
	static int	pause_frames[]	= {39, 45, 50, 55, 0};
	static int	fire_frames[]	= {9, 17, 0};

	Weapon_Generic (ent, 8, 32, 55, 58, pause_frames, fire_frames, weapon_bfg_fire);
}


/*
============
SpawnItem

Sets the clipping size and plants the object on the floor.

Items can't be immediately dropped to floor, because they might
be on an entity that hasn't spawned yet.
============
*/
void SpawnItem (edict_t *ent, gitem_t *item)
{
	PrecacheItem (item);

	if (ent->spawnflags)
	{
		if (strcmp(ent->classname, "key_power_cube") != 0)
		{
			ent->spawnflags = 0;
			gi.dprintf("%s at %s has invalid spawnflags set\n", ent->classname, vtos(ent->s.origin));
		}
	}

	// some items will be prevented in deathmatch
	if (deathmatch->value)
	{
		if ( (int)dmflags->value & DF_NO_ARMOR )
		{
			if (item->pickup == Pickup_Armor || item->pickup == Pickup_PowerArmor)
			{
				G_FreeEdict (ent);
				return;
			}
		}
		if ( (int)dmflags->value & DF_NO_ITEMS )
		{
			if (item->pickup == Pickup_Powerup)
			{
				G_FreeEdict (ent);
				return;
			}
		}
		if ( (int)dmflags->value & DF_NO_HEALTH )
		{
			if (item->pickup == Pickup_Health || item->pickup == Pickup_Adrenaline || item->pickup == Pickup_AncientHead)
			{
				G_FreeEdict (ent);
				return;
			}
		}
		if ( (int)dmflags->value & DF_INFINITE_AMMO )
		{
			if ( (item->flags == IT_AMMO) || (strcmp(ent->classname, "weapon_bfg") == 0) )
			{
				G_FreeEdict (ent);
				return;
			}
		}
	}

	if (coop->value && (strcmp(ent->classname, "key_power_cube") == 0))
	{
		ent->spawnflags |= (1 << (8 + level.power_cubes));
		level.power_cubes++;
	}

	// don't let them drop items that stay in a coop game
	if ((coop->value) && (item->flags & IT_STAY_COOP))
	{
		item->drop = NULL;
	}

	ent->item = item;
	ent->nextthink = level.time + 2 * FRAMETIME;    // items start after other solids
	ent->think = droptofloor;
	ent->s.effects = item->world_model_flags;
	ent->s.renderfx = 0/*RF_GLOW*/;
	if (ent->model)
		gi.modelindex (ent->model);
}


/*QUAKED misc_satellite_dish (1 .5 0) (-64 -64 0) (64 64 128)
*/
void misc_satellite_dish_think (edict_t *self)
{
	self->s.frame++;
	if (self->s.frame < 38)
		self->nextthink = level.time + FRAMETIME;
}

void misc_satellite_dish_use (edict_t *self, edict_t *other, edict_t *activator)
{
	self->s.frame = 0;
	self->think = misc_satellite_dish_think;
	self->nextthink = level.time + FRAMETIME;
}

void SP_misc_satellite_dish (edict_t *ent)
{
	ent->movetype = MOVETYPE_NONE;
	ent->solid = SOLID_BBOX;
	VectorSet (ent->mins, -64, -64, 0);
	VectorSet (ent->maxs, 64, 64, 128);
	ent->s.modelindex = gi.modelindex ("models/objects/satellite/tris.md2");
	ent->use = misc_satellite_dish_use;
	gi.linkentity (ent);
}

//=================================================================================

void barrel_touch (edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf)
{
	float	ratio;
	vec3_t	v;

	if ((!other->groundentity) || (other->groundentity == self))
		return;

	ratio = (float)other->mass / (float)self->mass;
	VectorSubtract (self->s.origin, other->s.origin, v);
	M_walkmove (self, vectoyaw(v), 20 * ratio * FRAMETIME);
}


void NetThrowDebris (edict_t *self)
{
	bool large_map = LargeMapCheck(self->s.origin);
	if (large_map)
		gi.WriteByte (svc_temp_entity_ext);
	else
		gi.WriteByte (svc_temp_entity);
	gi.WriteByte (TE_MODEL_EXPLOSION);
	gi.WritePosition (self->s.origin, large_map);
	gi.WriteByte (self->size[0]);
	gi.WriteByte (self->size[1]);
	gi.WriteByte (self->size[2]);
	gi.multicast (self->s.origin, MULTICAST_PVS);
}


void barrel_explode (edict_t *self)
{
	vec3_t	org;
	float	spd;
	vec3_t	save;

/// Berserker:	если dmg == 0, то не будет взрыва...
	if(self->dmg)	// barrel
		T_RadiusDamage (self, self->activator, self->dmg, NULL, self->dmg+40, MOD_BARREL);

	VectorCopy (self->s.origin, save);
	VectorMA (self->absmin, 0.5, self->size, self->s.origin);

	if(net_compatibility->value || !net_fixoverflow->value)
	{
		// a few big chunks
		spd = 1.5 * (float)self->dmg / 200.0;
		org[0] = self->s.origin[0] + crandom() * self->size[0];
		org[1] = self->s.origin[1] + crandom() * self->size[1];
		org[2] = self->s.origin[2] + crandom() * self->size[2];
		ThrowDebris (self, "models/objects/debris1/tris.md2", spd, org);
		org[0] = self->s.origin[0] + crandom() * self->size[0];
		org[1] = self->s.origin[1] + crandom() * self->size[1];
		org[2] = self->s.origin[2] + crandom() * self->size[2];
		ThrowDebris (self, "models/objects/debris1/tris.md2", spd, org);

		// bottom corners
		spd = 1.75 * (float)self->dmg / 200.0;
		VectorCopy (self->absmin, org);
		ThrowDebris (self, "models/objects/debris3/tris.md2", spd, org);
		VectorCopy (self->absmin, org);
		org[0] += self->size[0];
		ThrowDebris (self, "models/objects/debris3/tris.md2", spd, org);
		VectorCopy (self->absmin, org);
		org[1] += self->size[1];
		ThrowDebris (self, "models/objects/debris3/tris.md2", spd, org);
		VectorCopy (self->absmin, org);
		org[0] += self->size[0];
		org[1] += self->size[1];
		ThrowDebris (self, "models/objects/debris3/tris.md2", spd, org);

		// a bunch of little chunks
		spd = 2 * self->dmg / 200;
		org[0] = self->s.origin[0] + crandom() * self->size[0];
		org[1] = self->s.origin[1] + crandom() * self->size[1];
		org[2] = self->s.origin[2] + crandom() * self->size[2];
		ThrowDebris (self, "models/objects/debris2/tris.md2", spd, org);
		org[0] = self->s.origin[0] + crandom() * self->size[0];
		org[1] = self->s.origin[1] + crandom() * self->size[1];
		org[2] = self->s.origin[2] + crandom() * self->size[2];
		ThrowDebris (self, "models/objects/debris2/tris.md2", spd, org);
		org[0] = self->s.origin[0] + crandom() * self->size[0];
		org[1] = self->s.origin[1] + crandom() * self->size[1];
		org[2] = self->s.origin[2] + crandom() * self->size[2];
		ThrowDebris (self, "models/objects/debris2/tris.md2", spd, org);
		org[0] = self->s.origin[0] + crandom() * self->size[0];
		org[1] = self->s.origin[1] + crandom() * self->size[1];
		org[2] = self->s.origin[2] + crandom() * self->size[2];
		ThrowDebris (self, "models/objects/debris2/tris.md2", spd, org);
		org[0] = self->s.origin[0] + crandom() * self->size[0];
		org[1] = self->s.origin[1] + crandom() * self->size[1];
		org[2] = self->s.origin[2] + crandom() * self->size[2];
		ThrowDebris (self, "models/objects/debris2/tris.md2", spd, org);
		org[0] = self->s.origin[0] + crandom() * self->size[0];
		org[1] = self->s.origin[1] + crandom() * self->size[1];
		org[2] = self->s.origin[2] + crandom() * self->size[2];
		ThrowDebris (self, "models/objects/debris2/tris.md2", spd, org);
		org[0] = self->s.origin[0] + crandom() * self->size[0];
		org[1] = self->s.origin[1] + crandom() * self->size[1];
		org[2] = self->s.origin[2] + crandom() * self->size[2];
		ThrowDebris (self, "models/objects/debris2/tris.md2", spd, org);
		org[0] = self->s.origin[0] + crandom() * self->size[0];
		org[1] = self->s.origin[1] + crandom() * self->size[1];
		org[2] = self->s.origin[2] + crandom() * self->size[2];
		ThrowDebris (self, "models/objects/debris2/tris.md2", spd, org);
	}
	else
	{
		if (strcmp(self->classname, "misc_explobox"))
			NetThrowDebris (self);		// misc_model
		else
			NetThrowMonsterGibs (self, GIBS_BARREL_DEBRIS, 1000);	// barrels (misc_explobox)
	}

	VectorCopy (save, self->s.origin);

/// Berserker:	если dmg == 0, то не будет взрыва...
	if(self->dmg)	// barrel
	{
		if (self->groundentity)
			BecomeExplosion2 (self);
		else
			BecomeExplosion1 (self);
	}
	else
		G_FreeEdict (self);
}

void barrel_delay (edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point)
{
	self->takedamage = DAMAGE_NO;
	self->nextthink = level.time + 2 * FRAMETIME;
	self->think = barrel_explode;
	self->activator = attacker;
}


void model_think (edict_t *self)
{
	self->s.frame += self->delay;
	if (self->s.frame == self->accel)
		self->delay = -1;
	else if (self->s.frame <= self->decel)
		self->delay = 1;
	self->nextthink = level.time + FRAMETIME;
}

void model_use (edict_t *self, edict_t *other, edict_t *activator)
{
	if (self->nextthink)
		self->nextthink = 0;
	else
		self->nextthink = level.time + FRAMETIME;
}

#define	GFX_CHROME			1
#define	GFX_ANIMATION		2
#define	GFX_SHELL			4
#define	GFX_POWER_RED		8
#define	GFX_POWER_GREEN		16
#define	GFX_POWER_BLUE		32
#define	GFX_GRAVITY			64
#define	GFX_DISTORT			128

void SP_misc_model (edict_t *ent)
{
	char	buffer[MAX_QPATH+4];

	if (st.label)
	{	/// Berserker: Если модель имеет "label" - то игнорируем её (она обрабатывается только клиентами!)
		G_FreeEdict (ent);
		return;
	}

	if (ent->dmg!=-1)
		if (deathmatch->value)
		{	/// Berserker: for models w/health - auto-remove for deathmatch
			G_FreeEdict (ent);
			return;
		}

	gi.modelindex ("models/objects/debris1/tris.md2");
	gi.modelindex ("models/objects/debris2/tris.md2");
	gi.modelindex ("models/objects/debris3/tris.md2");

	ent->s.modelindex = gi.modelindex (ent->model);
	ent->s.skinnum = ent->style;

	if (VectorCompare(ent->move_origin, vec3_origin))
	{
		VectorSet(ent->move_origin, 8,8,16);
		VectorSet (ent->mins, -ent->move_origin[0], -ent->move_origin[1], 0);
		VectorSet (ent->maxs, ent->move_origin[0], ent->move_origin[1], ent->move_origin[2]);
	}
	if (VectorCompare(ent->move_angles, vec3_origin))
	{
		VectorSet (ent->mins, -ent->move_origin[0], -ent->move_origin[1], 0);
		VectorSet (ent->maxs, ent->move_origin[0], ent->move_origin[1], ent->move_origin[2]);
	}
	else
	{
		VectorSet (ent->mins, ent->move_origin[0], ent->move_origin[1], ent->move_origin[2]);
		VectorSet (ent->maxs, ent->move_angles[0], ent->move_angles[1], ent->move_angles[2]);
	}

	if (!ent->mass)
		ent->mass = 300;
	if (ent->health)
		ent->takedamage = DAMAGE_YES;
	ent->touch = barrel_touch;
	ent->die = barrel_delay;

	ent->s.effects = 0;
	ent->s.renderfx = 0;
	ent->s.frame = (int)ent->decel;
	ent->think = 0;

	if(ent->spawnflags & GFX_ANIMATION)
	{
		int len = strlen(ent->model);
		if (len >= 5 && Q_strcasecmp(ent->model+len-4, ".sp2") && Q_strcasecmp(ent->model+len-4, ".sp3"))
		{
			if (ent->accel<=0)
			{	// если не определён конечный кадр, анимация всегда положительная (и на стороне клиента, не грузит сеть)
				ent->s.renderfx = RF_ANIMFRAME;
			}
			else
			{
				ent->think = model_think;
				ent->nextthink = level.time + FRAMETIME;
				ent->delay = 1;		// на старте: анимация положительная
			}
		}
		else
			ent->s.renderfx = RF_ANIMFRAME;		// анимированные спрайты не могут "думать" (и, соответственно, грузить сеть)
	}

	if(ent->spawnflags & GFX_CHROME)
		ent->s.renderfx |= RF_CHROME;
	else
	{
		if(ent->spawnflags & GFX_SHELL)
		{
			ent->s.effects |= EF_COLOR_SHELL;
			if(ent->spawnflags & GFX_POWER_RED)
				ent->s.renderfx |= RF_SHELL_RED;
			if(ent->spawnflags & GFX_POWER_GREEN)
				ent->s.renderfx |= RF_SHELL_GREEN;
			if(ent->spawnflags & GFX_POWER_BLUE)
				ent->s.renderfx |= RF_SHELL_BLUE;
		}
		else
		{
			if(ent->spawnflags & GFX_POWER_RED)
				ent->s.renderfx |= RF_POWER_RED;
			if(ent->spawnflags & GFX_POWER_GREEN)
				ent->s.renderfx |= RF_POWER_GREEN;
			if(ent->spawnflags & GFX_POWER_BLUE)
				ent->s.renderfx |= RF_POWER_BLUE;
		}
	}

	if(ent->spawnflags & GFX_DISTORT)
		ent->s.renderfx |= RF_DISTORT;

	if(ent->spawnflags & GFX_GRAVITY)
		ent->movetype = MOVETYPE_STEP;

	ent->solid = SOLID_BBOX;

	if (ent->targetname && ent->think)
		ent->use = model_use;

	if (st.noise && st.noise[0])
	{
		if (!strstr (st.noise, ".wav"))
			Com_sprintf (buffer, sizeof(buffer), "%s.wav", st.noise);
		else
			strncpy (buffer, st.noise, sizeof(buffer));
		ent->s.sound = gi.soundindex (buffer);
		ent->volume = 1.0;
		ent->attenuation = 1.0;
	}

	gi.linkentity (ent);
}

//=================================================================================
/// Berserker: new "target_teleporter"
///		(spawnflags & 1) => c эффектом EV_PLAYER_TELEPORT
///		(spawnflags & 2) => незаметный телепорт (углы телепорт-дестинейшна НЕ копируем клиенту и осуществляем перемещение клиента на разницу координат двух ентити: target_teleporter и *_teleporter_dest
///		(spawnflags & 4) => гасить скорость клиента
void teleporter_use (edict_t *self, edict_t *other, edict_t *activator)
{
	edict_t		*dest;
	int			i;

	if (!activator->client)
		return;

	dest = G_Find (NULL, FOFS(targetname), self->target, "teleporter");
	if (!dest)
	{
		gi.dprintf ("Couldn't find destination\n");
		return;
	}
	// unlink to make sure it can't possibly interfere with KillBox
	gi.unlinkentity (activator);

	vec3_t	save;
	VectorSubtract(activator->s.origin, activator->s.old_origin, save);
	if(self->spawnflags & 2)
	{
		activator->s.origin[0] = activator->s.origin[0] - self->s.origin[0] + dest->s.origin[0];
		activator->s.origin[1] = activator->s.origin[1] - self->s.origin[1] + dest->s.origin[1];
		activator->s.origin[2] = activator->s.origin[2] - self->s.origin[2] + dest->s.origin[2];
	}
	else
		VectorCopy (dest->s.origin, activator->s.origin);
	VectorAdd (dest->s.origin, save, activator->s.old_origin);

	// draw the teleport splash at source and on the player
	if(self->spawnflags & 1)
	{
		self->s.event = EV_PLAYER_TELEPORT;
		activator->s.event = EV_PLAYER_TELEPORT;
	}
	else
		activator->s.event = EV_OTHER_TELEPORT;		// hack for teleportation w/o particle effect

	if(!(self->spawnflags & 2))		// set angles from misc_teleport_dest
	{
		for (i=0 ; i<3 ; i++)
			activator->client->ps.pmove.delta_angles[i] = ANGLE2SHORT(dest->s.angles[i] - activator->client->resp.cmd_angles[i]);

		VectorClear (activator->s.angles);
		VectorClear (activator->client->ps.viewangles);
		VectorClear (activator->client->v_angle);
	}

	// clear the velocity and hold them in place briefly
	if(self->spawnflags & 4)
	{
		VectorClear (activator->velocity);
		VectorClear (activator->client->oldvelocity);
		activator->client->ps.pmove.pm_time = 160>>3;		// hold time
		activator->client->ps.pmove.pm_flags |= PMF_TIME_TELEPORT;
	}

	// kill anything at the destination
	KillBox (activator);

	gi.linkentity (activator);
}


void SP_target_teleporter (edict_t *self)
{
	self->use = teleporter_use;
}

void teleporter_touch (edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf)
{
	edict_t		*dest;
	int			i;

	if (!other->client)
		return;
	dest = G_Find (NULL, FOFS(targetname), self->target, "teleporter");
	if (!dest)
	{
		gi.dprintf ("Couldn't find destination\n");
		return;
	}

	// unlink to make sure it can't possibly interfere with KillBox
	gi.unlinkentity (other);

	VectorCopy (dest->s.origin, other->s.origin);
	VectorCopy (dest->s.origin, other->s.old_origin);
	other->s.origin[2] += 10;

	// clear the velocity and hold them in place briefly
	VectorClear (other->velocity);
	other->client->ps.pmove.pm_time = 160>>3;		// hold time
	other->client->ps.pmove.pm_flags |= PMF_TIME_TELEPORT;

	// draw the teleport splash at source and on the player
	self->owner->s.event = EV_PLAYER_TELEPORT;
	other->s.event = EV_PLAYER_TELEPORT;

	// set angles
	for (i=0 ; i<3 ; i++)
	{
		other->client->ps.pmove.delta_angles[i] = ANGLE2SHORT(dest->s.angles[i] - other->client->resp.cmd_angles[i]);
	}

	VectorClear (other->s.angles);
	VectorClear (other->client->ps.viewangles);
	VectorClear (other->client->v_angle);

	// kill anything at the destination
	KillBox (other);

	gi.linkentity (other);
}


/*QUAKED misc_teleporter (1 0 0) (-32 -32 -24) (32 32 -16)
Stepping onto this disc will teleport players to the targeted misc_teleporter_dest object.
/// Berserker: (spawnflags & 1) = без модели
*/
void SP_info_teleporter (edict_t *ent)
{
	ent->spawnflags |= 1;
	SP_misc_teleporter (ent);
}

void SP_misc_teleporter (edict_t *ent)
{
	edict_t		*trig;

	if (!ent->target)
	{
		gi.dprintf ("teleporter without a target.\n");
		G_FreeEdict (ent);
		return;
	}

	if(!(ent->spawnflags & 1))
	{
		gi.setmodel (ent, "models/objects/dmspot/tris.md2");
		ent->s.skinnum = 1;
		ent->s.effects = EF_TELEPORTER;
		ent->solid = SOLID_BBOX;
	}
	ent->s.sound = gi.soundindex ("world/amb10.wav");

	VectorSet (ent->mins, -32, -32, -24);
	VectorSet (ent->maxs, 32, 32, -16);
	gi.linkentity (ent);

	trig = G_Spawn ();
	trig->touch = teleporter_touch;
	trig->solid = SOLID_TRIGGER;
	trig->target = ent->target;
	trig->owner = ent;
	VectorCopy (ent->s.origin, trig->s.origin);
	VectorSet (trig->mins, -8, -8, 8);
	VectorSet (trig->maxs, 8, 8, 24);
	gi.linkentity (trig);

}


/*QUAKED misc_teleporter_dest (1 0 0) (-32 -32 -24) (32 32 -16)
Point teleporters at these.
/// Berserker: (spawnflags & 1) = без модели
*/
void SP_info_teleporter_dest (edict_t *ent)
{
	ent->spawnflags |= 1;
	SP_misc_teleporter_dest (ent);
}

void SP_misc_teleporter_dest (edict_t *ent)
{
	if(!(ent->spawnflags & 1))
	{
		gi.setmodel (ent, "models/objects/dmspot/tris.md2");
		ent->s.skinnum = 0;
		ent->solid = SOLID_BBOX;
	}
//	ent->s.effects |= EF_FLIES;
	VectorSet (ent->mins, -32, -32, -24);
	VectorSet (ent->maxs, 32, 32, -16);
	gi.linkentity (ent);
}


/*QUAKED misc_viper (1 .5 0) (-16 -16 0) (16 16 32)
This is the Viper for the flyby bombing.
It is trigger_spawned, so you must have something use it for it to show up.
There must be a path for it to follow once it is activated.

"speed"		How fast the Viper should fly
*/
void misc_viper_use  (edict_t *self, edict_t *other, edict_t *activator)
{
	self->svflags &= ~SVF_NOCLIENT;
	self->use = train_use;
	train_use (self, other, activator);
}

void SP_misc_viper (edict_t *ent)
{
	if (!ent->target)
	{
		gi.dprintf ("misc_viper without a target at %s\n", vtos(ent->absmin));
		G_FreeEdict (ent);
		return;
	}

	if (!ent->speed)
		ent->speed = 300;

	ent->movetype = MOVETYPE_PUSH;
	ent->solid = SOLID_NOT;
	ent->s.modelindex = gi.modelindex ("models/ships/viper/tris.md2");
	VectorSet (ent->mins, -16, -16, 0);
	VectorSet (ent->maxs, 16, 16, 32);

	ent->think = func_train_find;
	ent->nextthink = level.time + FRAMETIME;
	ent->use = misc_viper_use;
	ent->svflags |= SVF_NOCLIENT;
	ent->moveinfo.accel = ent->moveinfo.decel = ent->moveinfo.speed = ent->speed;

	gi.linkentity (ent);
}


/*QUAKED misc_bigviper (1 .5 0) (-176 -120 -24) (176 120 72)
This is a large stationary viper as seen in Paul's intro
*/
void SP_misc_bigviper (edict_t *ent)
{
	ent->movetype = MOVETYPE_NONE;
	ent->solid = SOLID_BBOX;
	VectorSet (ent->mins, -176, -120, -24);
	VectorSet (ent->maxs, 176, 120, 72);
	ent->s.modelindex = gi.modelindex ("models/ships/bigviper/tris.md2");
	gi.linkentity (ent);
}


/*QUAKED misc_viper_bomb (1 0 0) (-8 -8 -8) (8 8 8)
"dmg"	how much boom should the bomb make?
*/
void misc_viper_bomb_touch (edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf)
{
	G_UseTargets (self, self->activator, "viper_bomb");

	self->s.origin[2] = self->absmin[2] + 1;
	T_RadiusDamage (self, self, self->dmg, NULL, self->dmg+40, MOD_BOMB);
	BecomeExplosion2 (self);
}

void misc_viper_bomb_prethink (edict_t *self)
{
	vec3_t	v;
	float	diff;

	self->groundentity = NULL;

	diff = self->timestamp - level.time;
	if (diff < -1.0)
		diff = -1.0;

	VectorScale (self->moveinfo.dir, 1.0 + diff, v);
	v[2] = diff;

	diff = self->s.angles[2];
	vectoangles (v, self->s.angles);
	self->s.angles[2] = diff + 10;
}

void misc_viper_bomb_use (edict_t *self, edict_t *other, edict_t *activator)
{
	edict_t	*viper;

	self->solid = SOLID_BBOX;
	self->svflags &= ~SVF_NOCLIENT;
	self->s.effects |= EF_ROCKET;
	self->use = NULL;
	self->movetype = MOVETYPE_TOSS;
	self->prethink = misc_viper_bomb_prethink;
	self->touch = misc_viper_bomb_touch;
	self->activator = activator;

	viper = G_Find (NULL, FOFS(classname), "misc_viper", NULL);
	VectorScale (viper->moveinfo.dir, viper->moveinfo.speed, self->velocity);

	self->timestamp = level.time;
	VectorCopy (viper->moveinfo.dir, self->moveinfo.dir);
}

void SP_misc_viper_bomb (edict_t *self)
{
	self->movetype = MOVETYPE_NONE;
	self->solid = SOLID_NOT;
	VectorSet (self->mins, -8, -8, -8);
	VectorSet (self->maxs, 8, 8, 8);

	self->s.modelindex = gi.modelindex ("models/objects/bomb/tris.md2");

	if (!self->dmg)
		self->dmg = 1000;

	self->use = misc_viper_bomb_use;
	self->svflags |= SVF_NOCLIENT;

	gi.linkentity (self);
}


/*QUAKED misc_eastertank (1 .5 0) (-32 -32 -16) (32 32 32)
*/
void misc_eastertank_think (edict_t *self)
{
	if (++self->s.frame < 293)
		self->nextthink = level.time + FRAMETIME;
	else
	{
		self->s.frame = 254;
		self->nextthink = level.time + FRAMETIME;
	}
}

void SP_misc_eastertank (edict_t *ent)
{
	ent->movetype = MOVETYPE_NONE;
	ent->solid = SOLID_BBOX;
	VectorSet (ent->mins, -32, -32, -16);
	VectorSet (ent->maxs, 32, 32, 32);
	ent->s.modelindex = gi.modelindex ("models/monsters/tank/tris.md2");
	ent->s.frame = 254;
	ent->think = misc_eastertank_think;
	ent->nextthink = level.time + 2 * FRAMETIME;
	gi.linkentity (ent);
}


/*QUAKED light_mine1 (0 1 0) (-2 -2 -12) (2 2 12)
*/
void SP_light_mine1 (edict_t *ent)
{
	ent->movetype = MOVETYPE_NONE;
	ent->solid = SOLID_NOT;///SOLID_BBOX;	/// Berserker: пусть он не мешает движению!
	ent->s.modelindex = gi.modelindex ("models/objects/minelite/light1/tris.md2");
	gi.linkentity (ent);
}


/*QUAKED light_mine2 (0 1 0) (-2 -2 -12) (2 2 12)
*/
void SP_light_mine2 (edict_t *ent)
{
	ent->movetype = MOVETYPE_NONE;
	ent->solid = SOLID_NOT;///SOLID_BBOX;	/// Berserker: пусть он не мешает движению!
	ent->s.modelindex = gi.modelindex ("models/objects/minelite/light2/tris.md2");
	gi.linkentity (ent);
}


/*QUAKED misc_easterchick (1 .5 0) (-32 -32 0) (32 32 32)
*/


void misc_easterchick_think (edict_t *self)
{
	if (++self->s.frame < 247)
		self->nextthink = level.time + FRAMETIME;
	else
	{
		self->s.frame = 208;
		self->nextthink = level.time + FRAMETIME;
	}
}

void SP_misc_easterchick (edict_t *ent)
{
	ent->movetype = MOVETYPE_NONE;
	ent->solid = SOLID_BBOX;
	VectorSet (ent->mins, -32, -32, 0);
	VectorSet (ent->maxs, 32, 32, 32);
	ent->s.modelindex = gi.modelindex ("models/monsters/bitch/tris.md2");
	ent->s.frame = 208;
	ent->think = misc_easterchick_think;
	ent->nextthink = level.time + 2 * FRAMETIME;
	gi.linkentity (ent);
}

/*QUAKED misc_easterchick2 (1 .5 0) (-32 -32 0) (32 32 32)
*/
void misc_easterchick2_think (edict_t *self)
{
	if (++self->s.frame < 287)
		self->nextthink = level.time + FRAMETIME;
	else
	{
		self->s.frame = 248;
		self->nextthink = level.time + FRAMETIME;
	}
}

void SP_misc_easterchick2 (edict_t *ent)
{
	ent->movetype = MOVETYPE_NONE;
	ent->solid = SOLID_BBOX;
	VectorSet (ent->mins, -32, -32, 0);
	VectorSet (ent->maxs, 32, 32, 32);
	ent->s.modelindex = gi.modelindex ("models/monsters/bitch/tris.md2");
	ent->s.frame = 248;
	ent->think = misc_easterchick2_think;
	ent->nextthink = level.time + 2 * FRAMETIME;
	gi.linkentity (ent);
}


//======================================================================

void	Use_Invulnerability (edict_t *ent, gitem_t *item)
{
	ent->client->pers.inventory[ITEM_INDEX(item)]--;
	ValidateSelectedItem (ent);

	if (ent->client->invincible_framenum > level.framenum)
		ent->client->invincible_framenum += 300;
	else
		ent->client->invincible_framenum = level.framenum + 300;

	gi.sound(ent, CHAN_ITEM, gi.soundindex("items/protect.wav"), 1, ATTN_NORM, 0);
}


//======================================================================

void	Use_Invisibility (edict_t *ent, gitem_t *item)
{
	if (net_compatibility->value)
		return;

	ent->client->pers.inventory[ITEM_INDEX(item)]--;
	ValidateSelectedItem (ent);

	if (ent->client->invisible_framenum > level.framenum)
		ent->client->invisible_framenum += 300;
	else
		ent->client->invisible_framenum = level.framenum + 300;

	gi.sound(ent, CHAN_ITEM, gi.soundindex("items/invis.wav"), 1, ATTN_NORM, 0);
}
//======================================================================

void Use_Breather (edict_t *ent, gitem_t *item)
{
	ent->client->pers.inventory[ITEM_INDEX(item)]--;
	ValidateSelectedItem (ent);

	if (ent->client->breather_framenum > level.framenum)
		ent->client->breather_framenum += 300;
	else
		ent->client->breather_framenum = level.framenum + 300;

		ent->client->ps.rdflags |= RDF_BREATHER;	// Включаем эффект breather
		ent->client->ps.rdflags |= RDF_MASK;

////	if(!net_compatibility->value)
////	{
////		gi.WriteByte (svc_rfx);
////		gi.WriteByte (RFX_BREATHER);	// Включаем эффект breather
////		gi.unicast (ent, true);
////	}

//	gi.sound(ent, CHAN_ITEM, gi.soundindex("items/damage.wav"), 1, ATTN_NORM, 0);
}

//======================================================================

void Use_Envirosuit (edict_t *ent, gitem_t *item)
{
	ent->client->pers.inventory[ITEM_INDEX(item)]--;
	ValidateSelectedItem (ent);

	if (ent->client->enviro_framenum > level.framenum)
		ent->client->enviro_framenum += 300;
	else
		ent->client->enviro_framenum = level.framenum + 300;

		ent->client->ps.rdflags |= RDF_ENVIRO;	// Включаем эффект enviro
		ent->client->ps.rdflags |= RDF_MASK;
////	if(!net_compatibility->value)
////	{
////		gi.WriteByte (svc_rfx);
////		gi.WriteByte (RFX_ENVIRO);	// Включаем эффект enviro
////		gi.unicast (ent, true);
////	}

//	gi.sound(ent, CHAN_ITEM, gi.soundindex("items/damage.wav"), 1, ATTN_NORM, 0);
}

//======================================================================

bool Pickup_Bandolier (edict_t *ent, edict_t *other)
{
	gitem_t	*item;
	int		index;

	if (other->client->pers.max_bullets < 250)
		other->client->pers.max_bullets = 250;
	if (other->client->pers.max_shells < 150)
		other->client->pers.max_shells = 150;
	if (other->client->pers.max_cells < 250)
		other->client->pers.max_cells = 250;
	if (other->client->pers.max_slugs < 75)
		other->client->pers.max_slugs = 75;

	item = FindItem("Bullets");
	if (item)
	{
		index = ITEM_INDEX(item);
		other->client->pers.inventory[index] += item->quantity;
		if (other->client->pers.inventory[index] > other->client->pers.max_bullets)
			other->client->pers.inventory[index] = other->client->pers.max_bullets;
	}

	item = FindItem("Shells");
	if (item)
	{
		index = ITEM_INDEX(item);
		other->client->pers.inventory[index] += item->quantity;
		if (other->client->pers.inventory[index] > other->client->pers.max_shells)
			other->client->pers.inventory[index] = other->client->pers.max_shells;
	}

	if (!(ent->spawnflags & DROPPED_ITEM) && (deathmatch->value))
		SetRespawn (ent, ent->item->quantity);

	return true;
}


/*QUAKED monster_commander_body (1 .5 0) (-32 -32 0) (32 32 48)
Not really a monster, this is the Tank Commander's decapitated body.
There should be a item_commander_head that has this as it's target.
*/

void commander_body_think (edict_t *self)
{
	if (++self->s.frame < 24)
		self->nextthink = level.time + FRAMETIME;
	else
		self->nextthink = 0;

	if (self->s.frame == 22)
		gi.sound (self, CHAN_BODY, gi.soundindex ("tank/thud.wav"), 1, ATTN_NORM, 0);
}

void commander_body_use (edict_t *self, edict_t *other, edict_t *activator)
{
	self->think = commander_body_think;
	self->nextthink = level.time + FRAMETIME;
	gi.sound (self, CHAN_BODY, gi.soundindex ("tank/pain.wav"), 1, ATTN_NORM, 0);
}

void commander_body_drop (edict_t *self)
{
	self->movetype = MOVETYPE_TOSS;
	self->s.origin[2] += 2;
}

void SP_monster_commander_body (edict_t *self)
{
	self->movetype = MOVETYPE_NONE;
	self->solid = SOLID_BBOX;
	self->model = "models/monsters/commandr/tris.md2";
	self->s.modelindex = gi.modelindex (self->model);
	VectorSet (self->mins, -32, -32, 0);
	VectorSet (self->maxs, 32, 32, 48);
	self->use = commander_body_use;
	self->takedamage = DAMAGE_YES;
	self->flags = FL_GODMODE;
	self->s.renderfx |= RF_FRAMELERP;
	gi.linkentity (self);

	gi.soundindex ("tank/thud.wav");
	gi.soundindex ("tank/pain.wav");

	self->think = commander_body_drop;
	self->nextthink = level.time + 5 * FRAMETIME;
}


/*QUAKED item_health (.3 .3 1) (-16 -16 -16) (16 16 16)*/
void SP_item_health (edict_t *self)
{
	if ( deathmatch->value && ((int)dmflags->value & DF_NO_HEALTH) )
	{
		G_FreeEdict (self);
		return;
	}

	self->model = "models/items/healing/medium/tris.md2";
	self->count = 10;
	SpawnItem (self, FindItem ("Health"));
	gi.soundindex ("items/n_health.wav");
}


/*QUAKED item_health_small (.3 .3 1) (-16 -16 -16) (16 16 16)*/
void SP_item_health_small (edict_t *self)
{
	if ( deathmatch->value && ((int)dmflags->value & DF_NO_HEALTH) )
	{
		G_FreeEdict (self);
		return;
	}

	self->model = "models/items/healing/stimpack/tris.md2";
	self->count = 2;
	SpawnItem (self, FindItem ("Health"));
	self->style = HEALTH_IGNORE_MAX;
	gi.soundindex ("items/s_health.wav");
}


/*QUAKED item_health_large (.3 .3 1) (-16 -16 -16) (16 16 16)*/
void SP_item_health_large (edict_t *self)
{
	if ( deathmatch->value && ((int)dmflags->value & DF_NO_HEALTH) )
	{
		G_FreeEdict (self);
		return;
	}

	self->model = "models/items/healing/large/tris.md2";
	self->count = 25;
	SpawnItem (self, FindItem ("Health"));
	gi.soundindex ("items/l_health.wav");
}


/*QUAKED item_health_mega (.3 .3 1) (-16 -16 -16) (16 16 16)*/
void SP_item_health_mega (edict_t *self)
{
	if ( deathmatch->value && ((int)dmflags->value & DF_NO_HEALTH) )
	{
		G_FreeEdict (self);
		return;
	}

	self->model = "models/items/mega_h/tris.md2";
	self->count = 100;
	SpawnItem (self, FindItem ("Health"));
	gi.soundindex ("items/m_health.wav");
	self->style = HEALTH_IGNORE_MAX|HEALTH_TIMED;
}


/*
QUAKED info_player_start (1 0 0) (-16 -16 -24) (16 16 32)
The normal starting point for a level.
*/
void SP_info_player_start(edict_t *self)
{
///	if (!coop->value)
///		return;
}


/*
QUAKED info_player_deathmatch (1 0 1) (-16 -16 -24) (16 16 32)
potential spawning position for deathmatch games
*/
void SP_info_player_deathmatch(edict_t *self)
{
	if (!deathmatch->value)
	{
		G_FreeEdict (self);
		return;
	}
	SP_misc_teleporter_dest (self);
}


void SP_misc_player_deathmatch(edict_t *self)
{
	self->spawnflags |= 1;
	SP_info_player_deathmatch (self);
}


/*
QUAKED info_player_coop (1 0 1) (-16 -16 -24) (16 16 32)
potential spawning position for coop games
*/
void SP_info_player_coop(edict_t *self)
{
	if (!coop->value)
	{
		G_FreeEdict (self);
		return;
	}
}


/*
QUAKED info_player_intermission (1 0 1) (-16 -16 -24) (16 16 32)
The deathmatch intermission point will be at one of these
Use 'angles' instead of 'angle', so you can set pitch or roll as well as yaw.  'pitch yaw roll'
*/
void SP_info_player_intermission(edict_t *self)
{
}


/*QUAKED misc_blackhole (1 .5 0) (-8 -8 -8) (8 8 8)
*/
void misc_blackhole_use (edict_t *ent, edict_t *other, edict_t *activator)
{
	/*
	bool large_map = LargeMapCheck(ent->s.origin);
	if (large_map)
		gi.WriteByte (svc_temp_entity_ext);
	else
		gi.WriteByte (svc_temp_entity);
	gi.WriteByte (TE_BOSSTPORT);
	gi.WritePosition (ent->s.origin, large_map);
	gi.multicast (ent->s.origin, MULTICAST_PVS);
	*/
	G_FreeEdict (ent);
}

void misc_blackhole_think (edict_t *self)
{
	if (++self->s.frame < 19)
		self->nextthink = level.time + FRAMETIME;
	else
	{
		self->s.frame = 0;
		self->nextthink = level.time + FRAMETIME;
	}
}

void SP_misc_blackhole (edict_t *ent)
{
	ent->movetype = MOVETYPE_NONE;
	ent->solid = SOLID_NOT;
	VectorSet (ent->mins, -64, -64, 0);
	VectorSet (ent->maxs, 64, 64, 8);
	ent->s.modelindex = gi.modelindex ("models/objects/black/tris.md2");
	ent->s.renderfx = RF_TRANSLUCENT;
	ent->use = misc_blackhole_use;
	ent->think = misc_blackhole_think;
	ent->nextthink = level.time + 2 * FRAMETIME;
	gi.linkentity (ent);
}


/*
===============
ED_CallSpawn

Finds the spawn function for the entity and calls it
===============
*/
void ED_CallSpawn (edict_t *ent)
{
	spawn_t	*s;
	gitem_t	*item;
	int		i;

	if (!ent->classname)
	{
		gi.dprintf ("ED_CallSpawn: NULL classname\n");
		return;
	}

	if (net_compatibility->value)
		if (!strcmp(ent->classname, "item_invisibility"))
		{
			gi.dprintf ("ED_CallSpawn: Invisibility in net_compatibility mode\n");
			return;
		}

	// check item spawn functions
	for (i=0,item=itemlist ; i<game.num_items ; i++,item++)
	{
		if (!item->classname)
			continue;
		if (!strcmp(item->classname, ent->classname))
		{	// found it
			SpawnItem (ent, item);
			return;
		}
	}

	// check normal spawn functions
	for (s=spawns ; s->name ; s++)
	{
		if (!strcmp(s->name, ent->classname))
		{	// found it
			s->spawn (ent);
			return;
		}
	}
	gi.dprintf ("%s doesn't have a spawn function\n", ent->classname);
}


/*
================
G_FindTeams

Chain together all entities with a matching team field.

All but the first will have the FL_TEAMSLAVE flag set.
All but the last will have the teamchain field set to the next one
================
*/
void G_FindTeams ()
{
	edict_t	*e, *e2, *chain;
	int		i, j;
	int		c, c2;

	c = 0;
	c2 = 0;
	for (i=1, e=g_edicts+i ; i < globals.num_edicts ; i++,e++)
	{
		if (!e->inuse)
			continue;
		if (!e->team)
			continue;
		if (e->flags & FL_TEAMSLAVE)
			continue;
		chain = e;
		e->teammaster = e;
		c++;
		c2++;
		for (j=i+1, e2=e+1 ; j < globals.num_edicts ; j++,e2++)
		{
			if (!e2->inuse)
				continue;
			if (!e2->team)
				continue;
			if (e2->flags & FL_TEAMSLAVE)
				continue;
			if (!strcmp(e->team, e2->team))
			{
				c2++;
				chain->teamchain = e2;
				e2->teammaster = e;
				chain = e2;
				e2->flags |= FL_TEAMSLAVE;
			}
		}
	}

	gi.dprintf ("%i teams with %i entities\n", c, c2);
}


void PlayerTrail_Init ()
{
	int		n;

	if (deathmatch->value /* FIXME || coop */)
		return;

	for (n = 0; n < TRAIL_LENGTH; n++)
	{
		trail[n] = G_Spawn();
		trail[n]->classname = "player_trail";
	}

	trail_head = 0;
	trail_active = true;
}


void	SetupLightStyles()
{
// Setup light animation tables. 'a' is total darkness, 'z' is doublebright.
	int		i;
	char	*map, *styles[] =
	{
	// 1 FLICKER (first variety)
		"mmnmmommommnonmmonqnmmo",
	// 2 SLOW STRONG PULSE
		"abcdefghijklmnopqrstuvwxyzyxwvutsrqponmlkjihgfedcba",
	// 3 CANDLE (first variety)
		"mmmmmaaaaammmmmaaaaaabcdefgabcdefg",
	// 4 FAST STROBE
		"mamamamamama",
	// 5 GENTLE PULSE 1
		"jklmnopqrstuvwxyzyxwvutsrqponmlkj",
	// 6 FLICKER (second variety)
		"nmonqnmomnmomomno",
	// 7 CANDLE (second variety)
		"mmmaaaabcdefgmmmmaaaammmaamm",
	// 8 CANDLE (third variety)
		"mmmaaammmaaammmabcdefaaaammmmabcdefmmmaaaa",
	// 9 SLOW STROBE (fourth variety)
		"aaaaaaaazzzzzzzz",
	// 10 FLUORESCENT FLICKER
		"mmamammmmammamamaaamammma",
	// 11 SLOW PULSE NOT FADE TO BLACK
		"abcdefghijklmnopqrrqponmlkjihgfedcba"
	};

	int	cs_lights;
	if (net_compatibility->value)
		cs_lights = CS_LIGHTS_Q2;
	else
		cs_lights = CS_LIGHTS_BERS;
	// 0 normal
	gi.configstring(cs_lights+0, "m");
	for (i=1; i<MAX_LIGHTSTYLES_OVERRIDE; i++)
	{
		map = gi.lightstyle(i);
		if (map[0])
			gi.configstring(cs_lights+i, map);
		else if (i<12)
			gi.configstring(cs_lights+i, styles[i-1]);
	}

	// styles 32-62 are assigned by the light program for switchable lights

	// 63 testing
	gi.configstring(cs_lights+63, "a");
}


/*
==============
SpawnEntities

Creates a server's entity / program execution context by
parsing textual entity definitions out of an ent file.
==============
*/
void SpawnEntities (char *mapname, char *entities, char *spawnpoint)
{
	edict_t		*ent;
	int			inhibit;
	char		*com_token;
	int			i;
	float		skill_level;

	skill_level = floor (skill->value);
	if (skill_level < 0)
		skill_level = 0;
	if (skill_level > 3)
		skill_level = 3;
	if (skill->value != skill_level)
		gi.cvar_forceset("skill", va("%f", skill_level));

	SaveClientData ();

	gi.FreeTags (TAG_LEVEL);

	memset (&level, 0, sizeof(level));
	memset (g_edicts, 0, game.maxentities * sizeof (g_edicts[0]));

	strncpy (level.mapname, mapname, sizeof(level.mapname)-1);
	strncpy (game.spawnpoint, spawnpoint, sizeof(game.spawnpoint)-1);

	// set client fields on player ents
	for (i=0 ; i<game.maxclients ; i++)
		g_edicts[i+1].client = game.clients + i;

	ent = NULL;
	inhibit = 0;

// parse ents
	while (1)
	{
		// parse the opening brace
		com_token = COM_Parse (&entities);
		if (!entities)
			break;
		if (com_token[0] != '{')
			gi.error ("SpawnEntities: found %s when expecting {",com_token);

		if (!ent)
			ent = g_edicts;
		else
			ent = G_Spawn ();
		entities = ED_ParseEdict (entities, ent);

		// yet another map hack
		if (!Q_strcasecmp(level.mapname, "command") && !Q_strcasecmp(ent->classname, "trigger_once") && !Q_strcasecmp(ent->model, "*27"))
			ent->spawnflags &= ~SPAWNFLAG_NOT_HARD;

		// remove things (except the world) from different skill levels or deathmatch
		if (ent != g_edicts)
		{
			if (deathmatch->value)
			{
				if ( ent->spawnflags & SPAWNFLAG_NOT_DEATHMATCH )
				{
					G_FreeEdict (ent);
					inhibit++;
					continue;
				}
			}
			else
			{
				if (((skill->value == 0) && (ent->spawnflags & SPAWNFLAG_NOT_EASY)) ||
					((skill->value == 1) && (ent->spawnflags & SPAWNFLAG_NOT_MEDIUM)) ||
					(((skill->value == 2) || (skill->value == 3)) && (ent->spawnflags & SPAWNFLAG_NOT_HARD)))
					{
						G_FreeEdict (ent);
						inhibit++;
						continue;
					}
			}

			ent->spawnflags &= ~(SPAWNFLAG_NOT_EASY|SPAWNFLAG_NOT_MEDIUM|SPAWNFLAG_NOT_HARD|SPAWNFLAG_NOT_COOP|SPAWNFLAG_NOT_DEATHMATCH);
		}

		ED_CallSpawn (ent);
	}

	SetupLightStyles();

	gi.dprintf ("%i entities inhibited\n", inhibit);

	G_FindTeams ();

	PlayerTrail_Init ();





}


/*
QUAKED trigger_always (.5 .5 .5) (-8 -8 -8) (8 8 8)
This trigger will always fire.  It is activated by the world.
*/
void SP_trigger_always (edict_t *ent)
{
	// we must have some delay to make sure our use targets are present
	if (ent->delay < 0.2)
		ent->delay = 0.2;
	G_UseTargets(ent, ent, "trigger_always");
}


// the wait time has passed, so set back up for another activation
void multi_wait (edict_t *ent)
{
	ent->nextthink = 0;
}


// the trigger was just activated
// ent->activator should be set to the activator so it can be held through a delay
// so wait for the delay time before firing
void multi_trigger (edict_t *ent)
{
	if (ent->nextthink)
		return;		// already been triggered

	G_UseTargets (ent, ent->activator, NULL);

	if (ent->wait > 0)
	{
		ent->think = multi_wait;
		ent->nextthink = level.time + ent->wait;
	}
	else
	{	// we can't just remove (self) here, because this is a touch function
		// called while looping through area links...
		ent->touch = NULL;
		ent->nextthink = level.time + FRAMETIME;
		ent->think = G_FreeEdict;
	}
}


void Touch_Multi (edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf)
{
	if(other->client)
	{
		if (self->spawnflags & 2)
			return;
	}
	else if (other->svflags & SVF_MONSTER)
	{
		if (!(self->spawnflags & 1))
			return;
	}
	else
		return;

	if (!VectorCompare(self->movedir, vec3_origin))
	{
		vec3_t	forward;

		AngleVectors(other->s.angles, forward, NULL, NULL);
		if (DotProduct(forward, self->movedir) < 0)
			return;
	}

	self->activator = other;
	multi_trigger (self);
}


void Use_Multi (edict_t *ent, edict_t *other, edict_t *activator)
{
	ent->activator = activator;
	multi_trigger (ent);
}


void trigger_enable (edict_t *self, edict_t *other, edict_t *activator)
{
	self->solid = SOLID_TRIGGER;
	self->use = Use_Multi;
	gi.linkentity (self);
}


void SP_trigger_multiple (edict_t *ent)
{
	if (ent->sounds == 1)
		ent->noise_index = gi.soundindex ("misc/secret.wav");
	else if (ent->sounds == 2)
		ent->noise_index = gi.soundindex ("misc/talk.wav");
	else if (ent->sounds == 3)
		ent->noise_index = gi.soundindex ("misc/trigger1.wav");

	if (!ent->wait)
		ent->wait = 0.2;
	ent->touch = Touch_Multi;
	ent->movetype = MOVETYPE_NONE;
	ent->svflags |= SVF_NOCLIENT;


	if (ent->spawnflags & 4)
	{
		ent->solid = SOLID_NOT;
		ent->use = trigger_enable;
	}
	else
	{
		ent->solid = SOLID_TRIGGER;
		ent->use = Use_Multi;
	}

	if (!VectorCompare(ent->s.angles, vec3_origin))
		G_SetMovedir (ent->s.angles, ent->movedir);

	gi.setmodel (ent, ent->model);
	gi.linkentity (ent);
}


/*
QUAKED trigger_once (.5 .5 .5) ? x x TRIGGERED
Triggers once, then removes itself.
You must set the key "target" to the name of another object in the level that has a matching "targetname".

If TRIGGERED, this trigger must be triggered before it is live.

sounds
 1)	secret
 2)	beep beep
 3)	large switch
 4)

"message"	string to be displayed when triggered
*/
void SP_trigger_once(edict_t *ent)
{
	// make old maps work because I messed up on flag assignments here
	// triggered was on bit 1 when it should have been on bit 4
	if (ent->spawnflags & 1)
	{
		vec3_t	v;

		VectorMA (ent->mins, 0.5, ent->size, v);
		ent->spawnflags &= ~1;
		ent->spawnflags |= 4;
		gi.dprintf("fixed TRIGGERED flag on %s at %s\n", ent->classname, vtos(v));
	}

	ent->wait = -1;
	SP_trigger_multiple (ent);
}


/*
QUAKED trigger_relay (.5 .5 .5) (-8 -8 -8) (8 8 8)
This fixed size trigger cannot be touched, it can only be fired by other events.
*/
void trigger_relay_use (edict_t *self, edict_t *other, edict_t *activator)
{
	G_UseTargets (self, activator, NULL);
}


void SP_trigger_relay (edict_t *self)
{
	self->use = trigger_relay_use;
}


void InitTrigger (edict_t *self)
{
	if (!VectorCompare (self->s.angles, vec3_origin))
		G_SetMovedir (self->s.angles, self->movedir);

	self->solid = SOLID_TRIGGER;
	self->movetype = MOVETYPE_NONE;
	gi.setmodel (self, self->model);
	self->svflags = SVF_NOCLIENT;
}


void trigger_push_touch (edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf)
{
	if (strcmp(other->classname, "grenade") == 0)
	{
		VectorScale (self->movedir, self->speed * 10, other->velocity);
	}
	else if (other->health > 0)
	{
		VectorScale (self->movedir, self->speed * 10, other->velocity);

		if (other->client)
		{
			// don't take falling damage immediately from this
			VectorCopy (other->velocity, other->client->oldvelocity);
			if (other->fly_sound_debounce_time < level.time)
			{
				other->fly_sound_debounce_time = level.time + 1.5;
				gi.sound (other, CHAN_AUTO, self->style, 1, ATTN_NORM, 0);
			}
		}
	}
	if (self->spawnflags & PUSH_ONCE)
		G_FreeEdict (self);
}


/*
QUAKED trigger_key (.5 .5 .5) (-8 -8 -8) (8 8 8)
A relay trigger that only fires it's targets if player has the proper key.
Use "item" to specify the required key, for example "key_data_cd"
*/
void trigger_key_use (edict_t *self, edict_t *other, edict_t *activator)
{
	int			index;

	if (!self->item)
		return;
	if (!activator->client)
		return;

	index = ITEM_INDEX(self->item);
	if (!activator->client->pers.inventory[index])
	{
		if (level.time < self->touch_debounce_time)
			return;
		self->touch_debounce_time = level.time + 5.0;
		gi.centerprintf (activator, "You need the %s", self->item->pickup_name);
		gi.sound (activator, CHAN_AUTO, gi.soundindex ("misc/keytry.wav"), 1, ATTN_NORM, 0);
		return;
	}

	gi.sound (activator, CHAN_AUTO, gi.soundindex ("misc/keyuse.wav"), 1, ATTN_NORM, 0);
	if (coop->value)
	{
		int		player;
		edict_t	*ent;

		if (strcmp(self->item->classname, "key_power_cube") == 0)
		{
			int	cube;

			for (cube = 0; cube < 8; cube++)
				if (activator->client->pers.power_cubes & (1 << cube))
					break;
			for (player = 1; player <= game.maxclients; player++)
			{
				ent = &g_edicts[player];
				if (!ent->inuse)
					continue;
				if (!ent->client)
					continue;
				if (ent->client->pers.power_cubes & (1 << cube))
				{
					ent->client->pers.inventory[index]--;
					ent->client->pers.power_cubes &= ~(1 << cube);
				}
			}
		}
		else
		{
			for (player = 1; player <= game.maxclients; player++)
			{
				ent = &g_edicts[player];
				if (!ent->inuse)
					continue;
				if (!ent->client)
					continue;
				ent->client->pers.inventory[index] = 0;
			}
		}
	}
	else
	{
		activator->client->pers.inventory[index]--;
	}

	G_UseTargets (self, activator, "key_use");

	self->use = NULL;
}


gitem_t	*FindItemByClassname (char *classname)
{
	int		i;
	gitem_t	*it;

	it = itemlist;
	for (i=0 ; i<game.num_items ; i++, it++)
	{
		if (!it->classname)
			continue;
		if (!Q_strcasecmp(it->classname, classname))
			return it;
	}

	return NULL;
}


void SP_trigger_key (edict_t *self)
{
	if (!st.item)
	{
		gi.dprintf("no key item for trigger_key at %s\n", vtos(self->s.origin));
		return;
	}
	self->item = FindItemByClassname (st.item);

	if (!self->item)
	{
		gi.dprintf("item %s not found for trigger_key at %s\n", st.item, vtos(self->s.origin));
		return;
	}

	if (!self->target)
	{
		gi.dprintf("%s at %s has no target\n", self->classname, vtos(self->s.origin));
		return;
	}

	gi.soundindex ("misc/keytry.wav");
	gi.soundindex ("misc/keyuse.wav");

	self->use = trigger_key_use;
}


/*QUAKED trigger_monsterjump (.5 .5 .5) ?
Walking monsters that touch this will jump in the direction of the trigger's angle
"speed" default to 200, the speed thrown forward
"height" default to 200, the speed thrown upwards
*/

void trigger_monsterjump_touch (edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf)
{
	if (other->flags & (FL_FLY | FL_SWIM) )
		return;
	if (other->svflags & SVF_DEADMONSTER)
		return;
	if ( !(other->svflags & SVF_MONSTER))
		return;

// set XY even if not on ground, so the jump will clear lips
	other->velocity[0] = self->movedir[0] * self->speed;
	other->velocity[1] = self->movedir[1] * self->speed;

	if (!other->groundentity)
		return;

	other->groundentity = NULL;
	other->velocity[2] = self->movedir[2];
}


void SP_trigger_monsterjump (edict_t *self)
{
	if (!self->speed)
		self->speed = 200;
	if (!st.height)
		st.height = 200;
	if (self->s.angles[YAW] == 0)
		self->s.angles[YAW] = 360;
	InitTrigger (self);
	self->touch = trigger_monsterjump_touch;
	self->movedir[2] = st.height;
}


/*
QUAKED trigger_gravity (.5 .5 .5) ?
Changes the touching entites gravity to
the value of "gravity".  1.0 is standard
gravity for the level.
*/
void trigger_gravity_touch (edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf)
{
	other->gravity = self->gravity;
}


void SP_trigger_gravity (edict_t *self)
{
	if (st.gravity == 0)
	{
		gi.dprintf("trigger_gravity without gravity set at %s\n", vtos(self->s.origin));
		G_FreeEdict  (self);
		return;
	}

	InitTrigger (self);
	self->gravity = atoi(st.gravity);
	self->touch = trigger_gravity_touch;
}


/*
QUAKED trigger_counter (.5 .5 .5) ? nomessage
Acts as an intermediary for an action that takes multiple inputs.

If nomessage is not set, t will print "1 more.. " etc when triggered and "sequence complete" when finished.

After the counter has been triggered "count" times (default 2), it will fire all of it's targets and remove itself.
*/

void trigger_counter_use(edict_t *self, edict_t *other, edict_t *activator)
{
	if (self->count == 0)
		return;

	self->count--;

	if (self->count)
	{
		if (! (self->spawnflags & 1))
		{
			gi.centerprintf(activator, "%i more to go...", self->count);
			gi.sound (activator, CHAN_AUTO, gi.soundindex ("misc/talk1.wav"), 1, ATTN_NORM, 0);
		}
		return;
	}

	if (! (self->spawnflags & 1))
	{
		gi.centerprintf(activator, "Sequence completed!");
		gi.sound (activator, CHAN_AUTO, gi.soundindex ("misc/talk1.wav"), 1, ATTN_NORM, 0);
	}
	self->activator = activator;
	multi_trigger (self);
}


void SP_trigger_counter (edict_t *self)
{
	self->wait = -1;
	if (!self->count)
		self->count = 2;

	self->use = trigger_counter_use;
}



/*
QUAKED trigger_hurt (.5 .5 .5) ? START_OFF TOGGLE SILENT NO_PROTECTION SLOW
Any entity that touches this will be hurt.

It does dmg points of damage each server frame

SILENT			supresses playing the sound
SLOW			changes the damage rate to once per second
NO_PROTECTION	*nothing* stops the damage

"dmg"			default 5 (whole numbers only)

*/
void hurt_use (edict_t *self, edict_t *other, edict_t *activator)
{
	if (self->solid == SOLID_NOT)
		self->solid = SOLID_TRIGGER;
	else
		self->solid = SOLID_NOT;
	gi.linkentity (self);

	if (!(self->spawnflags & 2))
		self->use = NULL;
}


void hurt_touch (edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf)
{
	int		dflags;

	if (!other->takedamage)
		return;

	if (self->timestamp > level.time)
		return;

	if (self->spawnflags & 16)
		self->timestamp = level.time + 1;
	else
		self->timestamp = level.time + FRAMETIME;

	if (!(self->spawnflags & 4))
	{
		if ((level.framenum % 10) == 0)
			gi.sound (other, CHAN_AUTO, self->noise_index, 1, ATTN_NORM, 0);
	}

	if (self->spawnflags & 8)
		dflags = DAMAGE_NO_PROTECTION;
	else
		dflags = 0;

	T_Damage (other, self, self, vec3_origin, other->s.origin, vec3_origin, self->dmg, self->dmg, dflags, MOD_TRIGGER_HURT);
}


void SP_trigger_hurt (edict_t *self)
{
	InitTrigger (self);

	self->noise_index = gi.soundindex ("world/electro.wav");
	self->touch = hurt_touch;

	if (!self->dmg)
		self->dmg = 5;

	if (self->spawnflags & 1)
		self->solid = SOLID_NOT;
	else
		self->solid = SOLID_TRIGGER;

	if (self->spawnflags & 2)
		self->use = hurt_use;

	gi.linkentity (self);
}


/*
QUAKED trigger_push (.5 .5 .5) ? PUSH_ONCE
Pushes the player
"speed"		defaults to 1000
*/
void SP_trigger_push (edict_t *self)
{
	InitTrigger (self);
	self->style = gi.soundindex ("misc/windfly.wav");
	self->touch = trigger_push_touch;
	if (!self->speed)
		self->speed = 1000;
	gi.linkentity (self);
}


/*
QUAKED target_character (0 0 1) ?
used with target_string (must be on same "team")
"count" is position in the string (starts at 1)
*/

void SP_target_character (edict_t *self)
{
	self->movetype = MOVETYPE_PUSH;
	gi.setmodel (self, self->model);
	self->solid = SOLID_BSP;
	self->s.frame = 12;
	gi.linkentity (self);
	return;
}


/*QUAKED target_string (0 0 1) (-8 -8 -8) (8 8 8)*/
void target_string_use (edict_t *self, edict_t *other, edict_t *activator)
{
	edict_t *e;
	int		n, l;
	char	c;

	l = strlen(self->message);
	for (e = self->teammaster; e; e = e->teamchain)
	{
		if (!e->count)
			continue;
		n = e->count - 1;
		if (n > l)
		{
			e->s.frame = 12;
			continue;
		}

		c = self->message[n];
		if (c >= '0' && c <= '9')
			e->s.frame = c - '0';
		else if (c == '-')
			e->s.frame = 10;
		else if (c == ':')
			e->s.frame = 11;
		else
			e->s.frame = 12;
	}
}

void SP_target_string (edict_t *self)
{
	if (!self->message)
		self->message = "";
	self->use = target_string_use;
}


void InitBodyQue ()
{
	int		i;
	edict_t	*ent;

	level.body_que = 0;
	for (i=0; i<BODY_QUEUE_SIZE ; i++)
	{
		ent = G_Spawn();
		ent->classname = "bodyque";
	}
}


/*
===============
SetItemNames

Called by worldspawn
===============
*/
void SetItemNames ()
{
	int		i, cs_items;
	gitem_t	*it;

	if (net_compatibility->value)
		cs_items = CS_ITEMS_Q2;
	else
		cs_items = CS_ITEMS_BERS;

	for (i=0 ; i<game.num_items ; i++)
	{
		it = &itemlist[i];
		gi.configstring (cs_items+i, it->pickup_name);
	}

	jacket_armor_index = ITEM_INDEX(FindItem("Jacket Armor"));
	combat_armor_index = ITEM_INDEX(FindItem("Combat Armor"));
	body_armor_index   = ITEM_INDEX(FindItem("Body Armor"));
	power_screen_index = ITEM_INDEX(FindItem("Power Screen"));
	power_shield_index = ITEM_INDEX(FindItem("Power Shield"));
}


#ifdef _WIN32
// Berserker: file system error handling, from MSDN
char *FS_GetError(errno_t err)
{
	static char	errmess[16];
	char		*mess;

	if (!err)
		return NULL;

	switch(err)
	{
		case EPERM:
			mess = "Operation not permitted";
			break;
		case ENOENT:
 			mess = "No such file or directory";
			break;
		case ESRCH:
 			mess = "No such process";
			break;
		case EINTR:
 			mess = "Interrupted function";
			break;
		case EIO:
 			mess = "I/O error";
			break;
		case ENXIO:
 			mess = "No such device or address";
			break;
		case E2BIG:
 			mess = "Argument list too long";
			break;
		case ENOEXEC:
 			mess = "Exec format error";
			break;
		case EBADF:
 			mess = "Bad file number";
			break;
		case ECHILD:
 			mess = "No spawned processes";
			break;
		case EAGAIN:
 			mess = "No more processes or not enough memory or maximum nesting level reached";
			break;
		case ENOMEM:
 			mess = "Not enough memory";
			break;
		case EACCES:
 			mess = "Permission denied";
			break;
		case EFAULT:
 			mess = "Bad address";
			break;
		case EBUSY:
 			mess = "Device or resource busy";
			break;
		case EEXIST:
 			mess = "File exists";
			break;
		case EXDEV:
 			mess = "Cross-device link";
			break;
		case ENODEV:
 			mess = "No such device";
			break;
		case ENOTDIR:
 			mess = "Not a directory";
			break;
		case EISDIR:
 			mess = "Is a directory";
			break;
		case EINVAL:
 			mess = "Invalid argument";
			break;
		case ENFILE:
 			mess = "Too many files open in system";
			break;
		case EMFILE:
 			mess = "Too many open files";
			break;
		case ENOTTY:
 			mess = "Inappropriate I/O control operation";
			break;
		case EFBIG:
 			mess = "File too large";
			break;
		case ENOSPC:
 			mess = "No space left on device";
			break;
		case ESPIPE:
 			mess = "Invalid seek";
			break;
		case EROFS:
 			mess = "Read-only file system";
			break;
		case EMLINK:
 			mess = "Too many links";
			break;
		case EPIPE:
 			mess = "Broken pipe";
			break;
		case EDOM:
 			mess = "Math argument";
			break;
		case ERANGE:
 			mess = "Result too large";
			break;
		case EDEADLK:
 			mess = "Resource deadlock would occur";
			break;
		case ENAMETOOLONG:
 			mess = "Filename too long";
			break;
		case ENOLCK:
 			mess = "No locks available";
			break;
		case ENOSYS:
 			mess = "Function not supported";
			break;
		case ENOTEMPTY:
 			mess = "Directory not empty";
			break;
		case EILSEQ:
 			mess = "Illegal byte sequence";
			break;
		case STRUNCATE:
 			mess = "String was truncated";
			break;
		default:
			Com_sprintf(errmess, sizeof(errmess), "%i", err);
			mess = errmess;
			break;
	}

	return mess;
}
#endif

FILE *FS_Fopen(const char *name, const char *mode)
{
	FILE	*file;
#ifndef _WIN32
	file = fopen (name, mode);
	if (!file)
	{
		if (errno != ENOENT)
			gi.dprintf("FS_Fopen(\"%s\", \"%s\"): %s\n", name, mode, strerror(errno));
	}
#else
	errno_t	err;
	char	*mess;
	err = fopen_s(&file, name, mode);
	if (err == ENOENT)
		mess = NULL;		// Berserker: отсутствие файла - обычное дело, не будем спамить!
	else
		mess = FS_GetError(err);
	if (mess)
	{
		gi.dprintf("FS_Fopen(\"%s\", \"%s\"): %s\n", name, mode, mess);
		if (file)
		{
			fclose(file);
			file = NULL;
		}
	}
#endif
	return file;
}


void LoadStatusbarProgram()
{
	FILE	*f;
	char	name[MAX_OSPATH];
	cvar_t	*game;
	char	buffer[4096];
	int		len;

	game = gi.cvar("game", "", 0);

	if (net_compatibility->value || g_ignore_external_layoutstring->value)
		goto old;

	if (deathmatch->value)
	{
		if (!*game->string)
			sprintf (name, "%s/dm_hud.txt", GAMEVERSION);
		else
			sprintf (name, "%s/dm_hud.txt", game->string);
	}
	else
	{
		if (!*game->string)
			sprintf (name, "%s/sp_hud.txt", GAMEVERSION);
		else
			sprintf (name, "%s/sp_hud.txt", game->string);
	}

	gi.cprintf (NULL, PRINT_HIGH, "Using %s.\n", name);

	f = FS_Fopen(name, "rb");
	if (!f)
	{
		gi.cprintf (NULL, PRINT_HIGH, "Couldn't open %s\n", name);
		goto old;
	}

	memset(buffer, 0, sizeof(buffer));
	len = fread (buffer, 1, sizeof(buffer), f);
	fclose (f);

	if (len<=0)
	{
		gi.cprintf (NULL, PRINT_HIGH, "Couldn't read %s\n", name);
		goto old;
	}

	gi.configstring (CS_STATUSBAR, buffer);
	return;

old:gi.cprintf (NULL, PRINT_HIGH, "Using internal hud program\n");
	if (deathmatch->value)
		gi.configstring (CS_STATUSBAR, dm_statusbar);
	else
		gi.configstring (CS_STATUSBAR, single_statusbar);
}


/*QUAKED worldspawn (0 0 0) ?

Only used for the world.
"sky"	environment map name
"skyaxis"	vector axis for rotating sky
"skyrotate"	speed of rotation in degrees/second
"sounds"	music cd track number
"gravity"	800 is default gravity
"message"	text to print at user logon
*/
void SP_worldspawn (edict_t *ent)
{
	// Для быстрой инициализации FlyQbe
	num_flyqbes = 0;
	if(!net_compatibility->value && sv_flyqbe->value)
	{
		flyQbe_teleport = gi.modelindex ("sprites/flyqbe/teleport.sp3");
		flyQbe_tele_out = gi.modelindex ("sprites/flyqbe/tele_out.sp3");
		flyQbe_model = gi.modelindex ("models/monsters/flyqbe/flyqbe.ase");
		flyQbe_sound_fly = gi.soundindex ("flyqbe/fly.wav");
		flyQbe_sound_attack = gi.soundindex ("flyqbe/attack.wav");
		flyQbe_sound_attack2 = gi.soundindex ("flyqbe/attack2.wav");
		flyqbe_teleport_sound = gi.soundindex ("flyqbe/teleport/tele_in.wav");
		flyqbe_exit_sound = gi.soundindex ("flyqbe/teleport/tele_out.wav");
	}

	ent->movetype = MOVETYPE_PUSH;
	ent->solid = SOLID_BSP;
	ent->inuse = true;			// since the world doesn't use G_Spawn()
	ent->s.modelindex = 1;		// world model is always index 1

	//---------------

	// reserve some spots for dead player bodies for coop / deathmatch
	InitBodyQue ();

	// set configstrings for items
	SetItemNames ();

	if (st.nextmap)
		strcpy (level.nextmap, st.nextmap);

	// make some data visible to the server

	if (ent->message && ent->message[0])
	{
		gi.configstring (CS_NAME, ent->message);
		strncpy (level.level_name, ent->message, sizeof(level.level_name));
	}
	else
		strncpy (level.level_name, level.mapname, sizeof(level.level_name));

	if (st.sky && st.sky[0])
		gi.configstring (CS_SKY, st.sky);
	else
		gi.configstring (CS_SKY, "unit1_");

	gi.configstring (CS_SKYROTATE, va("%f", st.skyrotate) );

	gi.configstring (CS_SKYAXIS, va("%f %f %f", st.skyaxis[0], st.skyaxis[1], st.skyaxis[2]) );

	if (net_compatibility->value)
		gi.configstring (CS_TRACK_AMBIENT, va("%i", ent->sounds) );
	else
	{
		if (deathmatch->value)
			ent->sounds = -ent->sounds;

		world_ambient_r = 256*ent->move_angles[0];
		world_ambient_g = 256*ent->move_angles[1];
		world_ambient_b = 256*ent->move_angles[2];
		gi.configstring (CS_TRACK_AMBIENT, va("%i %i %i %i", ent->sounds, world_ambient_r, world_ambient_g, world_ambient_b));

		if (deathmatch->value)
			ent->sounds = -ent->sounds;
	}

	gi.configstring (CS_MAXCLIENTS, va("%i", (int)(maxclients_cvar->value) ) );

	// status bar program
	LoadStatusbarProgram();

	// help icon for statusbar
	gi.imageindex ("i_help");
	level.pic_health = gi.imageindex ("i_health");
	gi.imageindex ("help");
	gi.imageindex ("field_3");

	if (!st.gravity)
		gi.cvar_set("sv_gravity", "800");
	else
		gi.cvar_set("sv_gravity", st.gravity);

	snd_fry = gi.soundindex ("player/fry.wav");	// standing in lava / slime

	PrecacheItem (FindItem ("Blaster"));

	gi.soundindex ("player/lava1.wav");
	gi.soundindex ("player/lava2.wav");

	gi.soundindex ("misc/pc_up.wav");
	gi.soundindex ("misc/talk1.wav");

	gi.soundindex ("misc/udeath.wav");

	// gibs
	gi.soundindex ("items/respawn1.wav");

	// sexed sounds
	gi.soundindex ("*death1.wav");
	gi.soundindex ("*death2.wav");
	gi.soundindex ("*death3.wav");
	gi.soundindex ("*death4.wav");
	gi.soundindex ("*fall1.wav");
	gi.soundindex ("*fall2.wav");
	gi.soundindex ("*gurp1.wav");		// drowning damage
	gi.soundindex ("*gurp2.wav");
	gi.soundindex ("*jump1.wav");		// player jump
	gi.soundindex ("*pain25_1.wav");
	gi.soundindex ("*pain25_2.wav");
	gi.soundindex ("*pain50_1.wav");
	gi.soundindex ("*pain50_2.wav");
	gi.soundindex ("*pain75_1.wav");
	gi.soundindex ("*pain75_2.wav");
	gi.soundindex ("*pain100_1.wav");
	gi.soundindex ("*pain100_2.wav");

	// sexed models
	// THIS ORDER MUST MATCH THE DEFINES IN g_local.h
	// you can add more, max 15
	gi.modelindex ("#w_blaster.md2");
	gi.modelindex ("#w_shotgun.md2");
	gi.modelindex ("#w_sshotgun.md2");
	gi.modelindex ("#w_machinegun.md2");
	gi.modelindex ("#w_chaingun.md2");
	gi.modelindex ("#a_grenades.md2");
	gi.modelindex ("#w_glauncher.md2");
	gi.modelindex ("#w_rlauncher.md2");
	gi.modelindex ("#w_hyperblaster.md2");
	gi.modelindex ("#w_railgun.md2");
	gi.modelindex ("#w_bfg.md2");

	//-------------------

	gi.soundindex ("player/gasp1.wav");		// gasping for air
	gi.soundindex ("player/gasp2.wav");		// head breaking surface, not gasping

	gi.soundindex ("player/watr_in.wav");	// feet hitting water
	gi.soundindex ("player/watr_out.wav");	// feet leaving water

	gi.soundindex ("player/watr_un.wav");	// head going underwater

	gi.soundindex ("player/u_breath1.wav");
	gi.soundindex ("player/u_breath2.wav");

	gi.soundindex ("items/pkup.wav");		// bonus item pickup
	gi.soundindex ("world/land.wav");		// landing thud
	gi.soundindex ("misc/h2ohit1.wav");		// landing splash

	gi.soundindex ("items/damage.wav");
	gi.soundindex ("items/protect.wav");
	gi.soundindex ("items/protect4.wav");
	gi.soundindex ("weapons/noammo.wav");

	gi.soundindex ("infantry/inflies1.wav");

	sm_meat_index = gi.modelindex ("models/objects/gibs/sm_meat/tris.md2");
///	gi.modelindex ("models/objects/gibs/arm/tris.md2");		// юзается в специфичных случаях, нет смысла кэшировать
///	gi.modelindex ("models/objects/gibs/leg/tris.md2");		// юзается в специфичных случаях, нет смысла кэшировать
///	gi.modelindex ("models/objects/gibs/head/tris.md2");	// юзается в специфичных случаях, нет смысла кэшировать
	gi.modelindex ("models/objects/gibs/head2/tris.md2");
	gi.modelindex ("models/objects/gibs/bone/tris.md2");
///	gi.modelindex ("models/objects/gibs/bone2/tris.md2");	// not used?
	gi.modelindex ("models/objects/gibs/chest/tris.md2");
	gi.modelindex ("models/objects/gibs/skull/tris.md2");
	gi.modelindex ("models/objects/gibs/gear/tris.md2");
	gi.modelindex ("models/objects/gibs/sm_metal/tris.md2");
}


void Drop_Ammo (edict_t *ent, gitem_t *item)
{
	edict_t	*dropped;
	int		index;

	index = ITEM_INDEX(item);
	dropped = Drop_Item (ent, item);
	if (ent->client->pers.inventory[index] >= item->quantity)
		dropped->count = item->quantity;
	else
		dropped->count = ent->client->pers.inventory[index];

	if (ent->client->pers.weapon &&
		ent->client->pers.weapon->tag == AMMO_GRENADES &&
		item->tag == AMMO_GRENADES &&
		ent->client->pers.inventory[index] - dropped->count <= 0) {
		gi.cprintf (ent, PRINT_HIGH, "Can't drop current weapon\n");
		G_FreeEdict(dropped);
		return;
	}

	ent->client->pers.inventory[index] -= dropped->count;
	ValidateSelectedItem (ent);
}


bool Pickup_Ammo (edict_t *ent, edict_t *other)
{
	int			oldcount;
	int			count;
	bool		weapon;

	weapon = (ent->item->flags & IT_WEAPON);
	if ( (weapon) && ( (int)dmflags->value & DF_INFINITE_AMMO ) )
		count = 1000;
	else if (ent->count)
		count = ent->count;
	else
		count = ent->item->quantity;

	oldcount = other->client->pers.inventory[ITEM_INDEX(ent->item)];

	if (!Add_Ammo (other, ent->item, count))
		return false;

	if (weapon && !oldcount)
	{
		if (other->client->pers.weapon != ent->item && ( !deathmatch->value || other->client->pers.weapon == FindItem("blaster") ) )
			other->client->newweapon = ent->item;
	}

	if (!(ent->spawnflags & (DROPPED_ITEM | DROPPED_PLAYER_ITEM)) && (deathmatch->value))
		SetRespawn (ent, 30);
	return true;
}


/*
======================================================================

GRENADE

======================================================================
*/

#define GRENADE_TIMER		3.0
#define GRENADE_MINSPEED	400
#define GRENADE_MAXSPEED	800

void weapon_grenade_fire (edict_t *ent, bool held)
{
	vec3_t	offset;
	vec3_t	forward, right;
	vec3_t	start;
	int		damage = 125;
	float	timer;
	int		speed;
	float	radius;

	radius = damage+40;
	if (is_quad)
		damage *= 4;

	VectorSet(offset, 8, 8, ent->viewheight-8);
	AngleVectors (ent->client->v_angle, forward, right, NULL);
	P_ProjectSource (ent, offset, forward, right, start);

	timer = ent->client->grenade_time - level.time;
	speed = GRENADE_MINSPEED + (GRENADE_TIMER - timer) * ((GRENADE_MAXSPEED - GRENADE_MINSPEED) / GRENADE_TIMER);
	fire_grenade2 (ent, start, forward, damage, speed, timer, radius, held);

	if (! ( (int)dmflags->value & DF_INFINITE_AMMO ) )
		ent->client->pers.inventory[ent->client->ammo_index]--;

	ent->client->grenade_time = level.time + 1.0;

	if(ent->deadflag || ent->s.modelindex != 255) // VWep animations screw up corpses
	{
		return;
	}

	if (ent->health <= 0)
		return;

	if (ent->client->ps.pmove.pm_flags & PMF_DUCKED)
	{
		ent->client->anim_priority = ANIM_ATTACK;
		ent->s.frame = PLAYER_FRAME_crattak1-1;
		ent->client->anim_end = PLAYER_FRAME_crattak3;
	}
	else
	{
		ent->client->anim_priority = ANIM_REVERSE;
		ent->s.frame = PLAYER_FRAME_wave08;
		ent->client->anim_end = PLAYER_FRAME_wave01;
	}
}

void Weapon_Grenade (edict_t *ent)
{
	if ((ent->client->newweapon) && (ent->client->weaponstate == WEAPON_READY))
	{
		ChangeWeapon (ent);
		return;
	}

	if (ent->client->weaponstate == WEAPON_ACTIVATING)
	{
		ent->client->weaponstate = WEAPON_READY;
		ent->client->ps.gunframe = 16;
		return;
	}

	if (ent->client->weaponstate == WEAPON_READY)
	{
		if ( ((ent->client->latched_buttons|ent->client->buttons) & BUTTON_ATTACK) )
		{
			ent->client->latched_buttons &= ~BUTTON_ATTACK;
			if (ent->client->pers.inventory[ent->client->ammo_index])
			{
				ent->client->ps.gunframe = 1;
				ent->client->weaponstate = WEAPON_FIRING;
				ent->client->grenade_time = 0;
			}
			else
			{
				if (level.time >= ent->pain_debounce_time)
				{
					gi.sound(ent, CHAN_VOICE, gi.soundindex("weapons/noammo.wav"), 1, ATTN_NORM, 0);
					ent->pain_debounce_time = level.time + 1;
				}
				NoAmmoWeaponChange (ent);
			}
			return;
		}

		if ((ent->client->ps.gunframe == 29) || (ent->client->ps.gunframe == 34) || (ent->client->ps.gunframe == 39) || (ent->client->ps.gunframe == 48))
		{
			if (rand()&15)
				return;
		}

		if (++ent->client->ps.gunframe > 48)
			ent->client->ps.gunframe = 16;
		return;
	}

	if (ent->client->weaponstate == WEAPON_FIRING)
	{
		if (ent->client->ps.gunframe == 5)
			gi.sound(ent, CHAN_WEAPON, gi.soundindex("weapons/hgrena1b.wav"), 1, ATTN_NORM, 0);

		if (ent->client->ps.gunframe == 11)
		{
			if (!ent->client->grenade_time)
			{
				ent->client->grenade_time = level.time + GRENADE_TIMER + 0.2;
				ent->client->weapon_sound = gi.soundindex("weapons/hgrenc1b.wav");
			}

			// they waited too long, detonate it in their hand
			if (!ent->client->grenade_blew_up && level.time >= ent->client->grenade_time)
			{
				ent->client->weapon_sound = 0;
				weapon_grenade_fire (ent, true);
				ent->client->grenade_blew_up = true;
			}

			if (ent->client->buttons & BUTTON_ATTACK)
				return;

			if (ent->client->grenade_blew_up)
			{
				if (level.time >= ent->client->grenade_time)
				{
					ent->client->ps.gunframe = 15;
					ent->client->grenade_blew_up = false;
				}
				else
				{
					return;
				}
			}
		}

		if (ent->client->ps.gunframe == 12)
		{
			ent->client->weapon_sound = 0;
			weapon_grenade_fire (ent, false);
		}

		if ((ent->client->ps.gunframe == 15) && (level.time < ent->client->grenade_time))
			return;

		ent->client->ps.gunframe++;

		if (ent->client->ps.gunframe == 16)
		{
			ent->client->grenade_time = 0;
			ent->client->weaponstate = WEAPON_READY;
		}
	}
}

/*
======================================================================

GRENADE LAUNCHER

======================================================================
*/

void weapon_grenadelauncher_fire (edict_t *ent)
{
	vec3_t	offset;
	vec3_t	forward, right;
	vec3_t	start;
	int		damage = 120;
	float	radius;

	radius = damage+40;
	if (is_quad)
		damage *= 4;

	VectorSet(offset, 8, 8, ent->viewheight-8);
	AngleVectors (ent->client->v_angle, forward, right, NULL);
	P_ProjectSource (ent, offset, forward, right, start);

	VectorScale (forward, -2, ent->client->kick_origin);
	ent->client->kick_angles[0] = -1;

	fire_grenade (ent, start, forward, damage, 600, 2.5, radius);

	gi.WriteByte (svc_muzzleflash);
	gi.WriteShort (ent-g_edicts);
	gi.WriteByte (MZ_GRENADE | is_silenced);
	gi.multicast (ent->s.origin, MULTICAST_PVS);

	ent->client->ps.gunframe++;

	PlayerNoise(ent, start, PNOISE_WEAPON);

	if (! ( (int)dmflags->value & DF_INFINITE_AMMO ) )
		ent->client->pers.inventory[ent->client->ammo_index]--;
}

void Weapon_GrenadeLauncher (edict_t *ent)
{
	static int	pause_frames[]	= {34, 51, 59, 0};
	static int	fire_frames[]	= {6, 0};

	Weapon_Generic (ent, 5, 16, 59, 64, pause_frames, fire_frames, weapon_grenadelauncher_fire);
}


void NetThrowRocketDebris (vec3_t org)
{
	bool large_map = LargeMapCheck(org);
	if (large_map)
		gi.WriteByte (svc_temp_entity_ext);
	else
		gi.WriteByte (svc_temp_entity);
	gi.WriteByte (TE_EXPLOSION_ROCKET);
	gi.WritePosition (org, large_map);
	gi.multicast (org, MULTICAST_PVS);
}


void rocket_touch (edict_t *ent, edict_t *other, cplane_t *plane, csurface_t *surf)
{
	vec3_t		origin;
	int			n;

	if (other == ent->owner)
		return;

	if (surf && (surf->flags & SURF_SKY))
	{
		G_FreeEdict (ent);
		return;
	}

	if (ent->owner->client)
		PlayerNoise(ent->owner, ent->s.origin, PNOISE_IMPACT);

	// calculate position for the explosion entity
	VectorMA (ent->s.origin, -0.02, ent->velocity, origin);

	if (other->takedamage)
	{
		T_Damage (other, ent, ent->owner, ent->velocity, ent->s.origin, plane->normal, ent->dmg, 0, 0, MOD_ROCKET);
	}
	else
	{
		// don't throw any debris in net games
		if (!deathmatch->value && !coop->value)
		{
			if ((surf) && !(surf->flags & (SURF_WARP|SURF_TRANS33|SURF_TRANS66|SURF_FLOWING)))
			{
				if(net_compatibility->value || !net_fixoverflow->value)
				{
					n = rand() % 5;
					while(n--)
						ThrowDebris (ent, "models/objects/debris2/tris.md2", 2, ent->s.origin);
				}
				else
					NetThrowRocketDebris (ent->s.origin);
			}
		}
	}

	T_RadiusDamage(ent, ent->owner, ent->radius_dmg, other, ent->dmg_radius, MOD_R_SPLASH);

	bool large_map = LargeMapCheck(origin);
	if (large_map)
		gi.WriteByte (svc_temp_entity_ext);
	else
		gi.WriteByte (svc_temp_entity);
	if (ent->waterlevel)
		gi.WriteByte (TE_ROCKET_EXPLOSION_WATER);
	else
		gi.WriteByte (TE_ROCKET_EXPLOSION);
	gi.WritePosition (origin, large_map);
////	if (!plane)
////		gi.WriteDir (vec3_origin);
////	else
////		gi.WriteDir (plane->normal);
	gi.multicast (NULL/*ent->s.origin*/, MULTICAST_ALL/*PHS*/);	// О долгоживущих пятнах от взрыва должны "знать" все

	G_FreeEdict (ent);
}


void fire_rocket (edict_t *self, vec3_t start, vec3_t dir, int damage, int speed, float damage_radius, int radius_damage)
{
	edict_t	*rocket;

	rocket = G_Spawn();
	VectorCopy (start, rocket->s.origin);
	VectorCopy (dir, rocket->movedir);
	vectoangles (dir, rocket->s.angles);
	VectorScale (dir, speed, rocket->velocity);
	rocket->movetype = MOVETYPE_FLYMISSILE;
	rocket->clipmask = MASK_SHOT;
	rocket->solid = SOLID_BBOX;
	rocket->s.effects |= EF_ROCKET;
	VectorClear (rocket->mins);
	VectorClear (rocket->maxs);
	rocket->s.modelindex = gi.modelindex ("models/objects/rocket/tris.md2");
	rocket->owner = self;
	rocket->touch = rocket_touch;
	if (net_compatibility->value)
		rocket->nextthink = level.time + 8000/speed;
	else
		rocket->nextthink = level.time + 32768/speed;		// LARGE_MAP_SIZE
	rocket->think = G_FreeEdict;
	rocket->dmg = damage;
	rocket->radius_dmg = radius_damage;
	rocket->dmg_radius = damage_radius;
	rocket->s.sound = gi.soundindex ("weapons/rockfly.wav");
	rocket->classname = "rocket";

	if (self->client)
		check_dodge (self, rocket->s.origin, dir, speed);

	gi.linkentity (rocket);

/// Berserker:
/// Fixed bug: стрельба сквозь стены при помощи "сидячего" трупа =)
	if (self->client)
	{
		trace_t	tr;
		vec3_t	point;
		VectorMA (start, 16, dir, point);							// Отодвинем тестовую точку на 16 единиц в сторону стрельбы (на кончик оружия)
		tr = gi.trace (start, NULL, NULL, point, NULL, MASK_SOLID);	// Если тестовая точка находится в стене-полу-окне (не учитываем "живность" и "мясо")
		if (tr.fraction < 1 && !(tr.surface->flags & SURF_SKY))
			rocket_touch(rocket, rocket, NULL, NULL);				// то принудительно сдетонируем снаряд
	}
}


/*
======================================================================

ROCKET

======================================================================
*/

void Weapon_RocketLauncher_Fire (edict_t *ent)
{
	vec3_t	offset, start;
	vec3_t	forward, right;
	int		damage;
	float	damage_radius;
	int		radius_damage;
	int		spd;

	damage = 100 + (int)(random() * 20.0);
	radius_damage = 120;
	damage_radius = 120;
	if (is_quad)
	{
		damage *= 4;
		radius_damage *= 4;
	}

	AngleVectors (ent->client->v_angle, forward, right, NULL);

	VectorScale (forward, -2, ent->client->kick_origin);
	ent->client->kick_angles[0] = -1;

	VectorSet(offset, 8, 8, ent->viewheight-8);
	P_ProjectSource (ent, offset, forward, right, start);

	if ((int)dmflags->value & DF_FASTROCKET)
		spd = 1000;	// quake rocket's speed	(Berserker: ускорим ракету как в Ку1)
	else
		spd = 650;	// q2 rocket's speed

	fire_rocket (ent, start, forward, damage, spd, damage_radius, radius_damage);

	// send muzzle flash
	gi.WriteByte (svc_muzzleflash);
	gi.WriteShort (ent-g_edicts);
	gi.WriteByte (MZ_ROCKET | is_silenced);
	gi.multicast (ent->s.origin, MULTICAST_PVS);

	ent->client->ps.gunframe++;

	PlayerNoise(ent, start, PNOISE_WEAPON);

	if (! ( (int)dmflags->value & DF_INFINITE_AMMO ) )
		ent->client->pers.inventory[ent->client->ammo_index]--;
}

void Weapon_RocketLauncher (edict_t *ent)
{
	static int	pause_frames[]	= {25, 33, 42, 50, 0};
	static int	fire_frames[]	= {5, 0};

	Weapon_Generic (ent, 4, 12, 50, 54, pause_frames, fire_frames, Weapon_RocketLauncher_Fire);
}



/*
QUAKED path_corner (.5 .3 0) (-8 -8 -8) (8 8 8) TELEPORT
Target: next path corner
Pathtarget: gets used when an entity that has
	this path_corner targeted touches it
*/
void path_corner_touch (edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf)
{
	vec3_t		v;
	edict_t		*next;

	if (other->movetarget != self)
		return;

	if (other->enemy)
		return;

	if (self->pathtarget)
	{
		char *savetarget;

		savetarget = self->target;
		self->target = self->pathtarget;
		G_UseTargets (self, other, NULL);
		self->target = savetarget;
	}

	if (self->target)
		next = G_PickTarget(self->target, NULL);
	else
		next = NULL;

	if ((next) && (next->spawnflags & 1))
	{
		VectorCopy (next->s.origin, v);
		v[2] += next->mins[2];
		v[2] -= other->mins[2];
		VectorCopy (v, other->s.origin);
		next = G_PickTarget(next->target, NULL);
		other->s.event = EV_OTHER_TELEPORT;
	}

	other->goalentity = other->movetarget = next;

	if (self->wait)
	{
		other->monsterinfo.pausetime = level.time + self->wait;
		other->monsterinfo.stand (other);
		return;
	}

	if (!other->movetarget)
	{
		other->monsterinfo.pausetime = level.time + 100000000;
		other->monsterinfo.stand (other);
	}
	else
	{
		VectorSubtract (other->goalentity->s.origin, other->s.origin, v);
		other->ideal_yaw = vectoyaw (v);
	}
}

void SP_path_corner (edict_t *self)
{
	if (!self->targetname)
	{
		gi.dprintf ("path_corner with no targetname at %s\n", vtos(self->s.origin));
		G_FreeEdict (self);
		return;
	}

	self->solid = SOLID_TRIGGER;
	self->touch = path_corner_touch;
	VectorSet (self->mins, -8, -8, -8);
	VectorSet (self->maxs, 8, 8, 8);
	self->svflags |= SVF_NOCLIENT;
	gi.linkentity (self);
}


/*
QUAKED point_combat (0.5 0.3 0) (-8 -8 -8) (8 8 8) Hold
Makes this the target of a monster and it will head here
when first activated before going after the activator.  If
hold is selected, it will stay here.
*/
void point_combat_touch (edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf)
{
	edict_t	*activator;

	if (other->movetarget != self)
		return;

	if (self->target)
	{
		other->target = self->target;
		other->goalentity = other->movetarget = G_PickTarget(other->target, NULL);
		if (!other->goalentity)
		{
			gi.dprintf("%s at %s target %s does not exist\n", self->classname, vtos(self->s.origin), self->target);
			other->movetarget = self;
		}
		self->target = NULL;
	}
	else if ((self->spawnflags & 1) && !(other->flags & (FL_SWIM|FL_FLY)))
	{
		other->monsterinfo.pausetime = level.time + 100000000;
		other->monsterinfo.aiflags |= AI_STAND_GROUND;
		other->monsterinfo.stand (other);
	}

	if (other->movetarget == self)
	{
		other->target = NULL;
		other->movetarget = NULL;
		other->goalentity = other->enemy;
		other->monsterinfo.aiflags &= ~AI_COMBAT_POINT;
	}

	if (self->pathtarget)
	{
		char *savetarget;

		savetarget = self->target;
		self->target = self->pathtarget;
		if (other->enemy && other->enemy->client)
			activator = other->enemy;
		else if (other->oldenemy && other->oldenemy->client)
			activator = other->oldenemy;
		else if (other->activator && other->activator->client)
			activator = other->activator;
		else
			activator = other;
		G_UseTargets (self, activator, NULL);
		self->target = savetarget;
	}
}

void SP_point_combat (edict_t *self)
{
	if (deathmatch->value)
	{
		G_FreeEdict (self);
		return;
	}
	self->solid = SOLID_TRIGGER;
	self->touch = point_combat_touch;
	VectorSet (self->mins, -8, -8, -16);
	VectorSet (self->maxs, 8, 8, 16);
	self->svflags = SVF_NOCLIENT;
	gi.linkentity (self);
};


int FunctionToIndex(void *func);
void *IndexToFunction(int index);
int MmoveToIndex(void *func);
void *IndexToMmove(int index);

void WriteField1 (FILE *f, field_t *field, byte *base)
{
	void		*p;
	int			len;
	int			index;

	if (field->flags & FFL_SPAWNTEMP)
		return;

	p = (void *)(base + field->ofs);
	switch (field->type)
	{
	case F_INT:
	case F_FLOAT:
	case F_ANGLEHACK:
	case F_VECTOR:
	case F_IGNORE:
		break;

	case F_LSTRING:
	case F_GSTRING:
		if ( *(char **)p )
			len = strlen(*(char **)p) + 1;
		else
			len = 0;
		*(int *)p = len;
		break;
	case F_EDICT:
		if ( *(edict_t **)p == NULL)
			index = -1;
		else
			index = *(edict_t **)p - g_edicts;
		*(int *)p = index;
		break;
	case F_CLIENT:
		if ( *(gclient_t **)p == NULL)
			index = -1;
		else
			index = *(gclient_t **)p - game.clients;
		*(int *)p = index;
		break;
	case F_ITEM:
		if ( *(edict_t **)p == NULL)
			index = -1;
		else
			index = *(gitem_t **)p - itemlist;
		*(int *)p = index;
		break;

	//relative to code segment
	case F_FUNCTION:
#if 0
		if (*(byte **)p == NULL)
			index = 0;
		else
			index = *(byte **)p - ((byte *)InitGame);
		*(int *)p = index;
#else
		if (*(byte **)p == NULL)
			*(int *)p = 0;
		else
			*(int *)p = FunctionToIndex(*(byte **)p);		/// Berserker: translate function to index
#endif
		break;

	//relative to data segment
	case F_MMOVE:
#if 0
		if (*(byte **)p == NULL)
			index = 0;
		else
			index = *(byte **)p - (byte *)&mmove_reloc;
		*(int *)p = index;
#else
		if (*(byte **)p == NULL)
			*(int *)p = 0;
		else
			*(int *)p = MmoveToIndex(*(byte **)p);		/// Berserker: translate mmove to index
#endif
		break;

	default:
		gi.error ("WriteEdict: unknown field type");
	}
}


void WriteField2 (FILE *f, field_t *field, byte *base)
{
	int			len;
	void		*p;

	if (field->flags & FFL_SPAWNTEMP)
		return;

	p = (void *)(base + field->ofs);
	switch (field->type)
	{
	case F_LSTRING:
		if ( *(char **)p )
		{
			len = strlen(*(char **)p) + 1;
			fwrite (*(char **)p, len, 1, f);
		}
		break;
	}
}

void ReadField (FILE *f, field_t *field, byte *base)
{
	void		*p;
	int			len;
	int			index;

	if (field->flags & FFL_SPAWNTEMP)
		return;

	p = (void *)(base + field->ofs);
	switch (field->type)
	{
	case F_INT:
	case F_FLOAT:
	case F_ANGLEHACK:
	case F_VECTOR:
	case F_IGNORE:
		break;

	case F_LSTRING:
		len = *(int *)p;
		if (!len)
			*(char **)p = NULL;
		else
		{
			*(char **)p = (char *) gi.TagMalloc (len, TAG_LEVEL);
			fread (*(char **)p, len, 1, f);
		}
		break;
	case F_EDICT:
		index = *(int *)p;
		if ( index == -1 )
			*(edict_t **)p = NULL;
		else
			*(edict_t **)p = &g_edicts[index];
		break;
	case F_CLIENT:
		index = *(int *)p;
		if ( index == -1 )
			*(gclient_t **)p = NULL;
		else
			*(gclient_t **)p = &game.clients[index];
		break;
	case F_ITEM:
		index = *(int *)p;
		if ( index == -1 )
			*(gitem_t **)p = NULL;
		else
			*(gitem_t **)p = &itemlist[index];
		break;

	//relative to code segment
	case F_FUNCTION:
		index = *(int *)p;
#if 0
		if ( index == 0 )
			*(byte **)p = NULL;
		else
			*(byte **)p = ((byte *)InitGame) + index;
#else
		if ( index == 0 )
			*(byte **)p = NULL;
		else
			*(byte **)p = (byte*)IndexToFunction(index);		/// Berserker: translate index to function
#endif
		break;

	//relative to data segment
	case F_MMOVE:
		index = *(int *)p;
#if 0
		if (index == 0)
			*(byte **)p = NULL;
		else
			*(byte **)p = (byte *)&mmove_reloc + index;
#else
		if ( index == 0 )
			*(byte **)p = NULL;
		else
			*(byte **)p = (byte*)IndexToMmove(index);		/// Berserker: translate index to mmove
#endif
		break;

	default:
		gi.error ("ReadEdict: unknown field type");
	}
}


/*
==============
WriteClient

All pointer variables (except function pointers) must be handled specially.
==============
*/
field_t		clientfields[] =
{
	{"pers.weapon", CLOFS(pers.weapon), F_ITEM},
	{"pers.lastweapon", CLOFS(pers.lastweapon), F_ITEM},
	{"newweapon", CLOFS(newweapon), F_ITEM},
	{NULL, 0, F_INT}
};

void WriteClient (FILE *f, gclient_t *client)
{
	field_t		*field;
	gclient_t	temp;

	// all of the ints, floats, and vectors stay as they are
	temp = *client;

	// change the pointers to lengths or indexes
	for (field=clientfields ; field->name ; field++)
	{
		WriteField1 (f, field, (byte *)&temp);
	}

	// write the block
	fwrite (&temp, sizeof(temp), 1, f);

	// now write any allocated data following the edict
	for (field=clientfields ; field->name ; field++)
	{
		WriteField2 (f, field, (byte *)client);
	}
}


/*
==============
ReadClient

All pointer variables (except function pointers) must be handled specially.
==============
*/
void ReadClient (FILE *f, gclient_t *client)
{
	field_t		*field;

	fread (client, sizeof(*client), 1, f);

	for (field=clientfields ; field->name ; field++)
	{
		ReadField (f, field, (byte *)client);
	}
}


/*
============
WriteGame

This will be called whenever the game goes to a new level,
and when the user explicitly saves the game.

Game information include cross level data, like multi level
triggers, help computer info, and all client states.

A single player death will automatically restore from the
last save position.
============
*/
void WriteGame (char *filename, bool autosave)
{
	FILE	*f;
	int		i;
////	char	str[16];

	if (!autosave)
		SaveClientData ();

	f = FS_Fopen(filename, "wb");
	if (!f)
		gi.error ("Couldn't open %s", filename);

/// Berserker: not need more
/*
	memset (str, 0, sizeof(str));
	strcpy (str, __DATE__);
	fwrite (str, sizeof(str), 1, f);
*/
	game.autosaved = autosave;
	fwrite (&game, sizeof(game), 1, f);
	game.autosaved = false;

	for (i=0 ; i<game.maxclients ; i++)
		WriteClient (f, &game.clients[i]);

	fclose (f);
}


void ReadGame (char *filename)
{
	FILE	*f;
	int		i;
////	char	str[16];

	gi.FreeTags (TAG_GAME);

	f = FS_Fopen(filename, "rb");
	if (!f)
		gi.error ("Couldn't open %s", filename);

/// Berserker: not need more
/*
	fread (str, sizeof(str), 1, f);
	if (strcmp (str, __DATE__))
	{
		fclose (f);
#if 0
		gi.error ("Savegame from an older version.\n");
#else
		gi.error ("Savegame from a different version (%s / %s).\n", str, __DATE__); // <--> ADJ fptrs
#endif
	}
*/
	g_edicts =  (edict_t *) gi.TagMalloc (game.maxentities * sizeof(g_edicts[0]), TAG_GAME);
	globals.edicts = g_edicts;

	fread (&game, sizeof(game), 1, f);
	game.clients = (gclient_t *) gi.TagMalloc (game.maxclients * sizeof(game.clients[0]), TAG_GAME);
	for (i=0 ; i<game.maxclients ; i++)
		ReadClient (f, &game.clients[i]);

	fclose (f);
}


/*
==============
ReadLevelLocals

All pointer variables (except function pointers) must be handled specially.
==============
*/
field_t		levelfields[] =
{
	{"changemap", LLOFS(changemap), F_LSTRING},

	{"sight_client", LLOFS(sight_client), F_EDICT},
	{"sight_entity", LLOFS(sight_entity), F_EDICT},
	{"sound_entity", LLOFS(sound_entity), F_EDICT},
	{"sound2_entity", LLOFS(sound2_entity), F_EDICT},

	{NULL, 0, F_INT}
};

void ReadLevelLocals (FILE *f)
{
	field_t		*field;

	fread (&level, sizeof(level), 1, f);

	for (field=levelfields ; field->name ; field++)
	{
		ReadField (f, field, (byte *)&level);
	}
}


/*
==============
WriteLevelLocals

All pointer variables (except function pointers) must be handled specially.
==============
*/
void WriteLevelLocals (FILE *f)
{
	field_t		*field;
	level_locals_t		temp;

	// all of the ints, floats, and vectors stay as they are
	temp = level;

	// change the pointers to lengths or indexes
	for (field=levelfields ; field->name ; field++)
	{
		WriteField1 (f, field, (byte *)&temp);
	}

	// write the block
	fwrite (&temp, sizeof(temp), 1, f);

	// now write any allocated data following the edict
	for (field=levelfields ; field->name ; field++)
	{
		WriteField2 (f, field, (byte *)&level);
	}
}


/*
==============
WriteEdict

All pointer variables (except function pointers) must be handled specially.
==============
*/
void WriteEdict (FILE *f, edict_t *ent)
{
	field_t		*field;
	edict_t		temp;

	// all of the ints, floats, and vectors stay as they are
	temp = *ent;

	// change the pointers to lengths or indexes
	for (field=fields ; field->name ; field++)
	{
		WriteField1 (f, field, (byte *)&temp);
	}

	// write the block
	fwrite (&temp, sizeof(temp), 1, f);

	// now write any allocated data following the edict
	for (field=fields ; field->name ; field++)
	{
		WriteField2 (f, field, (byte *)ent);
	}

}


/*
==============
ReadEdict

All pointer variables (except function pointers) must be handled specially.
==============
*/
void ReadEdict (FILE *f, edict_t *ent)
{
	field_t		*field;

	fread (ent, sizeof(*ent), 1, f);

	for (field=fields ; field->name ; field++)
	{
		ReadField (f, field, (byte *)ent);
	}
}


/*
=================
GetFunctionInStdBlock - ADJ fptrs

This uses win32 API's to work out where in memory the DLL has been loaded
- so even if some other DLL has been loaded into 0x20000000 we can return
the address in that range so that we can tell if function pointers really
are moving.
=================
*/
/*
void *GetFunctionInStdBlock(void *p)
{
#ifdef _WIN32
	byte *baseAddress = (byte *)0x20000000;
	int offset;
    MEMORY_BASIC_INFORMATION mbi;
	bool mbiValid = VirtualQuery(InitGame, &mbi, sizeof(mbi));

	if (mbiValid)
		baseAddress = (byte*)mbi.AllocationBase;

	offset = ((byte*)p - baseAddress);

	return (void*)(offset + 0x20000000);
#else
	return ((void *)InitGame);
#endif
}
*/

void WriteLevel (char *filename)
{
	int		i;
	edict_t	*ent;
	FILE	*f;
////	void	*base;

	f = FS_Fopen(filename, "wb");
	if (!f)
		gi.error ("Couldn't open %s", filename);

	// write out edict size for checking
	i = sizeof(edict_t);
	fwrite (&i, sizeof(i), 1, f);

#if 0	/// Berserker: not need more
	// write out a function pointer for checking
#if 0
	base = (void *)InitGame;
#else
	base = GetFunctionInStdBlock((void *)InitGame);  // <--> ADJ fptrs
#endif
	fwrite (&base, sizeof(base), 1, f);
#else
	i = SAVEGAME_VERSION;				/// Berserker: если требуется отбросить предыдущие saves, сменить версию SAVEGAME_VERSION !!!
	fwrite (&i, sizeof(i), 1, f);
#endif

	// write out level_locals_t
	WriteLevelLocals (f);

	// write out all the entities
	for (i=0 ; i<globals.num_edicts ; i++)
	{
		ent = &g_edicts[i];
		if (!ent->inuse)
			continue;
		fwrite (&i, sizeof(i), 1, f);
		WriteEdict (f, ent);
	}
	i = -1;
	fwrite (&i, sizeof(i), 1, f);

	fclose (f);
}


/*
=================
ReadLevel

SpawnEntities will allready have been called on the
level the same way it was when the level was saved.

That is necessary to get the baselines
set up identically.

The server will have cleared all of the world links before
calling ReadLevel.

No clients are connected yet.
=================
*/
void ReadLevel (char *filename)
{
	int		entnum;
	FILE	*f;
	int		i;
////	void	*base;
	edict_t	*ent;
////	void	*initGamePtr = GetFunctionInStdBlock((void*)InitGame); // <--> ADJ fptrs

	f = FS_Fopen(filename, "rb");
	if (!f)
		gi.error ("Couldn't open %s\n", filename);

	// free any dynamic memory allocated by loading the level
	// base state
	gi.FreeTags (TAG_LEVEL);

	// wipe all the entities
	memset (g_edicts, 0, game.maxentities*sizeof(g_edicts[0]));
	globals.num_edicts = maxclients_cvar->value+1;

	// check edict size
	fread (&i, sizeof(i), 1, f);
	if (i != sizeof(edict_t))
	{
		fclose (f);
		gi.error ("ReadLevel: mismatched edict size\n");
	}

#if 0		/// Berserker: уже нет нужды в данной проверке!!!
	// check function pointer base address
	fread (&base, sizeof(base), 1, f);		// Berserker: not need!
#ifdef _WIN32
	if (base != initGamePtr) // ADJ fptrs -->
	{
		fclose (f);
		gi.error("ReadLevel: function pointers have moved\n"\
				"InitGame addresses = %x / %x\n\n", base, initGamePtr);
	}
	else if (base != (void *)InitGame)
		gi.dprintf("Warning: DLL relocated - InitGame address = %x\n", ((byte *)InitGame));
	// <-- ADJ fptrs
#else
	gi.dprintf("Function offsets %d\n", ((byte *)base) - ((byte *)InitGame));
#endif
#else
	fread (&i, sizeof(i), 1, f);
	if (i != SAVEGAME_VERSION)				/// Berserker: если требуется отбросить предыдущие saves, сменить версию SAVEGAME_VERSION !!!
	{
		fclose (f);
		gi.error ("ReadLevel: Savegame from a different version (%i, must be %i)\n", i, SAVEGAME_VERSION);
	}
#endif

	// load the level locals
	ReadLevelLocals (f);

	// load all the entities
	while (1)
	{
		if (fread (&entnum, sizeof(entnum), 1, f) != 1)
		{
			fclose (f);
			gi.error ("ReadLevel: failed to read entnum");
		}
		if (entnum == -1)
			break;
		if (entnum >= globals.num_edicts)
			globals.num_edicts = entnum+1;

		ent = &g_edicts[entnum];
		ReadEdict (f, ent);

		// let the server rebuild world links for this ent
		memset (&ent->area, 0, sizeof(ent->area));
		gi.linkentity (ent);
	}

	fclose (f);

	// mark all clients as unconnected
	for (i=0 ; i<maxclients_cvar->value ; i++)
	{
		ent = &g_edicts[i+1];
		ent->client = game.clients + i;
		ent->client->pers.connected = false;
	}

	// do any load time things at this point
	for (i=0 ; i<globals.num_edicts ; i++)
	{
		ent = &g_edicts[i];

		if (!ent->inuse)
			continue;

		// fire any cross-level triggers
		if (ent->classname)
			if (strcmp(ent->classname, "target_crosslevel_target") == 0)
				ent->nextthink = level.time + ent->delay;
	}
}


// pmove doesn't need to know about passent and contentmask
trace_t	PM_trace (vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, int mask)
{
	if (pm_passent->health <= 0)
		return gi.trace (start, mins, maxs, end, pm_passent, MASK_DEADSOLID);
	else
		return gi.trace (start, mins, maxs, end, pm_passent, mask);
}


void	G_TouchTriggers (edict_t *ent)
{
	int			i, num;
	edict_t		*touch[MAX_EDICTS], *hit;

	// dead things don't activate triggers!
	if ((ent->client || (ent->svflags & SVF_MONSTER)) && (ent->health <= 0))
		return;

	num = gi.BoxEdicts (ent->absmin, ent->absmax, touch, MAX_EDICTS, AREA_TRIGGERS);

	// be careful, it is possible to have an entity in this
	// list removed before we get to it (killtriggered)
	for (i=0 ; i<num ; i++)
	{
		hit = touch[i];
		if (!hit->inuse)
			continue;
		if (!hit->touch)
			continue;

		hit->touch (hit, ent, NULL, NULL);
	}
}


void UpdateChaseCam(edict_t *ent)
{
	vec3_t o, ownerv, goal;
	edict_t *targ;
	vec3_t forward, right;
	trace_t trace;
	int i;
//	vec3_t oldgoal;
	vec3_t angles;

	// is our chase target gone?
	if (!ent->client->chase_target->inuse || ent->client->chase_target->client->resp.spectator)
	{
		edict_t *old = ent->client->chase_target;
		ChaseNext(ent);
		if (ent->client->chase_target == old)
		{
			ent->client->chase_target = NULL;
			ent->client->ps.pmove.pm_flags &= ~PMF_NO_PREDICTION;
			return;
		}
	}

	targ = ent->client->chase_target;

	VectorCopy(targ->s.origin, ownerv);
//	VectorCopy(ent->s.origin, oldgoal);

	ownerv[2] += targ->viewheight;

	VectorCopy(targ->client->v_angle, angles);
	if (angles[PITCH] > 56)
		angles[PITCH] = 56;
	AngleVectors (angles, forward, right, NULL);
	VectorNormalize(forward);
	VectorMA(ownerv, -30, forward, o);

	if (o[2] < targ->s.origin[2] + 20)
		o[2] = targ->s.origin[2] + 20;

	// jump animation lifts
	if (!targ->groundentity)
		o[2] += 16;

	trace = gi.trace(ownerv, vec3_origin, vec3_origin, o, targ, MASK_SOLID);

	VectorCopy(trace.endpos, goal);

	VectorMA(goal, 2, forward, goal);

	// pad for floors and ceilings
	VectorCopy(goal, o);
	o[2] += 6;
	trace = gi.trace(goal, vec3_origin, vec3_origin, o, targ, MASK_SOLID);
	if (trace.fraction < 1) {
		VectorCopy(trace.endpos, goal);
		goal[2] -= 6;
	}

	VectorCopy(goal, o);
	o[2] -= 6;
	trace = gi.trace(goal, vec3_origin, vec3_origin, o, targ, MASK_SOLID);
	if (trace.fraction < 1) {
		VectorCopy(trace.endpos, goal);
		goal[2] += 6;
	}

	if (targ->deadflag)
		ent->client->ps.pmove.pm_type = PM_DEAD;
	else
		ent->client->ps.pmove.pm_type = PM_FREEZE;

	VectorCopy(goal, ent->s.origin);
	for (i=0 ; i<3 ; i++)
		ent->client->ps.pmove.delta_angles[i] = ANGLE2SHORT(targ->client->v_angle[i] - ent->client->resp.cmd_angles[i]);

	if (targ->deadflag) {
		ent->client->ps.viewangles[ROLL] = 40;
		ent->client->ps.viewangles[PITCH] = -15;
		ent->client->ps.viewangles[YAW] = targ->client->killer_yaw;
	} else {
		VectorCopy(targ->client->v_angle, ent->client->ps.viewangles);
		VectorCopy(targ->client->v_angle, ent->client->v_angle);
	}

	ent->viewheight = 0;
	ent->client->ps.pmove.pm_flags |= PMF_NO_PREDICTION;
	gi.linkentity(ent);
}


void GetChaseTarget(edict_t *ent)
{
	int i;
	edict_t *other;

	for (i = 1; i <= maxclients_cvar->value; i++) {
		other = g_edicts + i;
		if (other->inuse && !other->client->resp.spectator) {
			ent->client->chase_target = other;
			ent->client->update_chase = true;
			UpdateChaseCam(ent);
			return;
		}
	}
	gi.centerprintf(ent, "No other players to chase.");
}


/*
=================
Think_Weapon

Called by ClientBeginServerFrame and ClientThink
=================
*/
void Think_Weapon (edict_t *ent)
{
	// if just died, put the weapon away
	if (ent->health < 1)
	{
		ent->client->newweapon = NULL;
		ChangeWeapon (ent);
	}

	// call active weapon think routine
	if (ent->client->pers.weapon && ent->client->pers.weapon->weaponthink)
	{
		is_quad = (ent->client->quad_framenum > level.framenum);
		if (ent->client->silencer_shots)
			is_silenced = MZ_SILENCED;
		else
			is_silenced = 0;
		ent->client->pers.weapon->weaponthink (ent);
	}
}


/*
==============
ClientThink

This will be called once for each client frame, which will
usually be a couple times for each server frame.
==============
*/
void ClientThink (edict_t *ent, usercmd_t *ucmd)
{
	gclient_t	*client;
	edict_t	*other;
	int		i, j;
	pmove_t	pm;

	level.current_entity = ent;
	client = ent->client;

	if (level.intermissiontime)
	{
		client->ps.pmove.pm_type = PM_FREEZE;
		// can exit intermission after five seconds
		if (level.time > level.intermissiontime + 5.0
			&& (ucmd->buttons & BUTTON_ANY) )
			level.exitintermission = true;
		return;
	}

	pm_passent = ent;

	if (ent->client->chase_target)
	{
		client->resp.cmd_angles[0] = SHORT2ANGLE(ucmd->angles[0]);
		client->resp.cmd_angles[1] = SHORT2ANGLE(ucmd->angles[1]);
		client->resp.cmd_angles[2] = SHORT2ANGLE(ucmd->angles[2]);
	}
	else
	{

		// set up for pmove
		memset (&pm, 0, sizeof(pm));

		if (ent->movetype == MOVETYPE_NOCLIP)
			client->ps.pmove.pm_type = PM_SPECTATOR;
		else if (ent->s.modelindex != 255)
			client->ps.pmove.pm_type = PM_GIB;
		else if (ent->deadflag)
			client->ps.pmove.pm_type = PM_DEAD;
		else
			client->ps.pmove.pm_type = PM_NORMAL;

		client->ps.pmove.gravity = sv_gravity->value;
		pm.s = client->ps.pmove;

		for (i=0 ; i<3 ; i++)
		{
			pm.s.origin[i] = ent->s.origin[i]*8;
			pm.s.velocity[i] = ent->velocity[i]*8;
		}

		if (memcmp(&client->old_pmove, &pm.s, sizeof(pm.s)))
			pm.snapinitial = true;

		pm.cmd = *ucmd;

		pm.trace = PM_trace;	// adds default parms
		pm.pointcontents = gi.pointcontents;

		// perform a pmove
		gi.Pmove (&pm);

		// save results of pmove
		client->ps.pmove = pm.s;
		client->old_pmove = pm.s;

		for (i=0 ; i<3 ; i++)
		{
			ent->s.origin[i] = pm.s.origin[i]*0.125;
			ent->velocity[i] = pm.s.velocity[i]*0.125;
		}

		VectorCopy (pm.mins, ent->mins);
		VectorCopy (pm.maxs, ent->maxs);

		client->resp.cmd_angles[0] = SHORT2ANGLE(ucmd->angles[0]);
		client->resp.cmd_angles[1] = SHORT2ANGLE(ucmd->angles[1]);
		client->resp.cmd_angles[2] = SHORT2ANGLE(ucmd->angles[2]);

		if (ent->groundentity && !pm.groundentity && (pm.cmd.upmove >= 10) && (pm.waterlevel == 0))
		{
			gi.sound(ent, CHAN_VOICE, gi.soundindex("*jump1.wav"), 1, ATTN_NORM, 0);
			PlayerNoise(ent, ent->s.origin, PNOISE_SELF);
		}

		ent->viewheight = pm.viewheight;
		ent->waterlevel = pm.waterlevel;
		ent->watertype = pm.watertype;
		ent->groundentity = pm.groundentity;
		if (pm.groundentity)
			ent->groundentity_linkcount = pm.groundentity->linkcount;

		if (ent->deadflag)
		{
			client->ps.viewangles[ROLL] = 40;
			client->ps.viewangles[PITCH] = -15;
			client->ps.viewangles[YAW] = client->killer_yaw;
		}
		else
		{
			VectorCopy (pm.viewangles, client->v_angle);
			VectorCopy (pm.viewangles, client->ps.viewangles);
		}

		gi.linkentity (ent);

		if (ent->movetype != MOVETYPE_NOCLIP)
			G_TouchTriggers (ent);

		// touch other objects
		for (i=0 ; i<pm.numtouch ; i++)
		{
			other = pm.touchents[i];
			for (j=0 ; j<i ; j++)
				if (pm.touchents[j] == other)
					break;
			if (j != i)
				continue;	// duplicated
			if (!other->touch)
				continue;
			other->touch (other, ent, NULL, NULL);
		}

	}

	client->oldbuttons = client->buttons;
	client->buttons = ucmd->buttons;
	client->latched_buttons |= client->buttons & ~client->oldbuttons;

	// save light level the player is standing on for
	// monster sighting AI
	ent->light_level = ucmd->lightlevel;

	// flashlight
	bool flashLightEnabled;
	if (deathmatch->value)
	{
		if ((int)dmflags->value & DF_FLASHLIGHT)
			flashLightEnabled = true;
		else
			flashLightEnabled = false;
	}
	else
		flashLightEnabled = true;

	if (flashLightEnabled)
	{
		bool flashLightCells = ((int)(dmflags->value) & DF_FLASHLIGHTCELLS);
		if (client->latched_buttons & BUTTON_FLASHLIGHT)
		{
////			client->flashlight = !client->flashlight;
				ent->flags ^= FL_FLASHLIGHT;
			client->latched_buttons &= ~BUTTON_FLASHLIGHT;
			if (!flashLightCells)
			{
				if (ent->flags & FL_FLASHLIGHT)
					gi.sound(ent, CHAN_AUTO, gi.soundindex("misc/flashlight1.wav"), 1, ATTN_NORM, 0);
				else
					gi.sound(ent, CHAN_AUTO, gi.soundindex("misc/flashlight2.wav"), 1, ATTN_NORM, 0);
			}
			else
			{
				if (ent->flags & FL_FLASHLIGHT)
				{
					if (!client->pers.inventory[ITEM_INDEX(FindItem("cells"))])
					{
						ent->flags &= ~FL_FLASHLIGHT;
						gi.cprintf (ent, PRINT_HIGH, "No cells for flashlight.\n");
					}
					else
					{
						client->flashlight_framenum += level.framenum;		// flashlight ON
						gi.sound(ent, CHAN_AUTO, gi.soundindex("misc/flashlight1.wav"), 1, ATTN_NORM, 0);
					}
				}
				else
				{
					client->flashlight_framenum -= level.framenum;	// flashlight OFF
					gi.sound(ent, CHAN_AUTO, gi.soundindex("misc/flashlight2.wav"), 1, ATTN_NORM, 0);
				}
			}
		}
		if (flashLightCells)
		{
			if (ent->flags & FL_FLASHLIGHT)
			{
				if (client->flashlight_framenum < level.framenum)
				{
					int index = ITEM_INDEX(FindItem ("cells"));
					if (client->pers.inventory[index] <= 0)
					{	// батарейки кончились, гаснуть тихо
						ent->flags &= ~FL_FLASHLIGHT;
						client->flashlight_framenum -= level.framenum;	// flashlight OFF
					}
					else
					{
						if (! ( (int)dmflags->value & DF_INFINITE_AMMO ) )
							client->pers.inventory[index]--;
						client->flashlight_framenum = level.framenum + 100;		// фонарь кушает 1 cell за 10 сек.
					}
				}
			}
		}
	}
	else
		if (client->latched_buttons & BUTTON_FLASHLIGHT)
			client->latched_buttons &= ~BUTTON_FLASHLIGHT;

	// fire weapon from final position if needed
	if (client->latched_buttons & BUTTON_ATTACK)
	{
		if (client->resp.spectator)
		{
			client->latched_buttons = 0;

			if (client->chase_target)
			{
				client->chase_target = NULL;
				client->ps.pmove.pm_flags &= ~PMF_NO_PREDICTION;
			} else
				GetChaseTarget(ent);

		} else if (!client->weapon_thunk)
		{
			client->weapon_thunk = true;
			Think_Weapon (ent);
		}
	}

	if (client->resp.spectator)
	{
		if (ucmd->upmove >= 10)
		{
			if (!(client->ps.pmove.pm_flags & PMF_JUMP_HELD))
			{
				client->ps.pmove.pm_flags |= PMF_JUMP_HELD;
				if (client->chase_target)
					ChaseNext(ent);
				else
					GetChaseTarget(ent);
			}
		} else
			client->ps.pmove.pm_flags &= ~PMF_JUMP_HELD;
	}

	// update chase cam if being followed
	for (i = 1; i <= maxclients_cvar->value; i++) {
		other = g_edicts + i;
		if (other->inuse && other->client->chase_target == ent)
			UpdateChaseCam(other);
	}
}


bool SV_FilterPacket (char *from)
{
	int		i;
	unsigned	in;
	byte m[4];
	char *p;

	i = 0;
	p = from;
	while (*p && i < 4) {
		m[i] = 0;
		while (*p >= '0' && *p <= '9') {
			m[i] = m[i]*10 + (*p - '0');
			p++;
		}
		if (!*p || *p == ':')
			break;
		i++, p++;
	}

	in = *(unsigned *)m;

	for (i=0 ; i<numipfilters ; i++)
		if ( (in & ipfilters[i].mask) == ipfilters[i].compare)
			return (int)filterban->value;

	return (int)!filterban->value;
}


void InitClientResp (gclient_t *client)
{
	memset (&client->resp, 0, sizeof(client->resp));
	client->resp.enterframe = level.framenum;
	client->resp.coop_respawn = client->pers;
}


/*
===========
ClientConnect

Called when a player begins connecting to the server.
The game can refuse entrance to a client by returning false.
If the client is allowed, the connection process will continue
and eventually get to ClientBegin()
Changing levels will NOT cause this to be called again, but
loadgames will.
============
*/
bool ClientConnect (edict_t *ent, char *userinfo)
{
	char	*value;

	// check to see if they are on the banned IP list
	value = Info_ValueForKey (userinfo, "ip");
	if (SV_FilterPacket(value))
	{
		Info_SetValueForKey(userinfo, "rejmsg", "Banned.");
		return false;
	}

	// check for a spectator
	value = Info_ValueForKey (userinfo, "spectator");
	if (deathmatch->value && *value && strcmp(value, "0")) {
		int i, numspec;

		if (*spectator_password->string &&
			strcmp(spectator_password->string, "none") &&
			strcmp(spectator_password->string, value)) {
			Info_SetValueForKey(userinfo, "rejmsg", "Spectator password required or incorrect.");
			return false;
		}

		// count spectators
		for (i = numspec = 0; i < maxclients_cvar->value; i++)
			if (g_edicts[i+1].inuse && g_edicts[i+1].client->pers.spectator)
				numspec++;

		if (numspec >= maxspectators->value) {
			Info_SetValueForKey(userinfo, "rejmsg", "Server spectator limit is full.");
			return false;
		}
	} else {
		// check for a password
		value = Info_ValueForKey (userinfo, "password");
		if (*password->string && strcmp(password->string, "none") &&
			strcmp(password->string, value)) {
			Info_SetValueForKey(userinfo, "rejmsg", "Password required or incorrect.");
			return false;
		}
	}


	// they can connect
	ent->client = game.clients + (ent - g_edicts - 1);

	// if there is already a body waiting for us (a loadgame), just
	// take it, otherwise spawn one from scratch
	if (ent->inuse == false)
	{
		// clear the respawning variables
		InitClientResp (ent->client);
		if (!game.autosaved || !ent->client->pers.weapon)
			InitClientPersistant (ent->client, userinfo);
	}

	ClientUserinfoChanged (ent, userinfo);

	if (game.maxclients > 1)
		gi.dprintf ("%s connected\n", ent->client->pers.netname);

	ent->svflags = 0; // make sure we start with known default
	ent->client->pers.connected = true;
	return true;
}

/*
===========
ClientDisconnect

Called when a player drops from the server.
Will not be called between levels.
============
*/
void ClientDisconnect (edict_t *ent)
{
	int		playernum;

	if (!ent->client)
		return;

	gi.bprintf (PRINT_HIGH, "%s disconnected\n", ent->client->pers.netname);

	// send effect
	gi.WriteByte (svc_muzzleflash);
	gi.WriteShort (ent-g_edicts);
	gi.WriteByte (MZ_LOGOUT);
	gi.multicast (ent->s.origin, MULTICAST_PVS);

	gi.unlinkentity (ent);
	ent->s.modelindex = 0;
	ent->solid = SOLID_NOT;
	ent->inuse = false;
	ent->classname = "disconnected";
	ent->client->pers.connected = false;

	playernum = ent-g_edicts-1;
	if (net_compatibility->value)
		gi.configstring (CS_PLAYERSKINS_Q2+playernum, "");
	else
		gi.configstring (CS_PLAYERSKINS_BERS+playernum, "");
}


void G_SetStats (edict_t *ent)
{
	gitem_t		*item;
	int			index, cells;
	int			power_armor_type;

	//
	// health
	//
	ent->client->ps.stats[STAT_HEALTH_ICON] = level.pic_health;
	ent->client->ps.stats[STAT_HEALTH] = ent->health;

	//
	// ammo
	//
	if ((ent->flags & FL_FLASHLIGHT) && (level.framenum & 8) && ((int)(dmflags->value) & DF_FLASHLIGHTCELLS))
	{	// flash between cells and other ammo
		ent->client->ps.stats[STAT_AMMO_ICON] = gi.imageindex ("a_cells");
		ent->client->ps.stats[STAT_AMMO] = ent->client->pers.inventory[ITEM_INDEX(FindItem ("cells"))];;
	}
	else
	{
		if (!ent->client->ammo_index /* || !ent->client->pers.inventory[ent->client->ammo_index] */)
		{
			ent->client->ps.stats[STAT_AMMO_ICON] = 0;
			ent->client->ps.stats[STAT_AMMO] = 0;
		}
		else
		{
			item = &itemlist[ent->client->ammo_index];
			ent->client->ps.stats[STAT_AMMO_ICON] = gi.imageindex (item->icon);
			ent->client->ps.stats[STAT_AMMO] = ent->client->pers.inventory[ent->client->ammo_index];
		}
	}

	//
	// armor
	//
	power_armor_type = PowerArmorType (ent);
	if (power_armor_type)
	{
		cells = ent->client->pers.inventory[ITEM_INDEX(FindItem ("cells"))];
		if (cells == 0)
		{	// ran out of cells for power armor
			ent->flags &= ~FL_POWER_ARMOR;
			gi.sound(ent, CHAN_ITEM, gi.soundindex("misc/power2.wav"), 1, ATTN_NORM, 0);
			power_armor_type = 0;
		}
	}

	index = ArmorIndex (ent);
	if (power_armor_type && (!index || (level.framenum & 8) ) )
	{	// flash between power armor and other armor icon
		ent->client->ps.stats[STAT_ARMOR_ICON] = gi.imageindex ("i_powershield");
		ent->client->ps.stats[STAT_ARMOR] = cells;
	}
	else if (index)
	{
		item = GetItemByIndex (index);
		ent->client->ps.stats[STAT_ARMOR_ICON] = gi.imageindex (item->icon);
		ent->client->ps.stats[STAT_ARMOR] = ent->client->pers.inventory[index];
	}
	else
	{
		ent->client->ps.stats[STAT_ARMOR_ICON] = 0;
		ent->client->ps.stats[STAT_ARMOR] = 0;
	}

	//
	// pickup message
	//
	if (level.time > ent->client->pickup_msg_time)
	{
		ent->client->ps.stats[STAT_PICKUP_ICON] = 0;
		ent->client->ps.stats[STAT_PICKUP_STRING] = 0;
	}

	//
	// timers
	//
	if (ent->client->quad_framenum > level.framenum)
	{
		ent->client->ps.stats[STAT_TIMER_ICON] = gi.imageindex ("p_quad");
		ent->client->ps.stats[STAT_TIMER] = (ent->client->quad_framenum - level.framenum)/10;
	}
	else if (ent->client->invincible_framenum > level.framenum)
	{
		ent->client->ps.stats[STAT_TIMER_ICON] = gi.imageindex ("p_invulnerability");
		ent->client->ps.stats[STAT_TIMER] = (ent->client->invincible_framenum - level.framenum)/10;
	}
	else if (ent->client->invisible_framenum > level.framenum)
	{
		ent->client->ps.stats[STAT_TIMER_ICON] = gi.imageindex ("p_invisibility");
		ent->client->ps.stats[STAT_TIMER] = (ent->client->invisible_framenum - level.framenum)/10;
	}
	else if (ent->client->enviro_framenum > level.framenum)
	{
		ent->client->ps.stats[STAT_TIMER_ICON] = gi.imageindex ("p_envirosuit");
		ent->client->ps.stats[STAT_TIMER] = (ent->client->enviro_framenum - level.framenum)/10;
	}
	else if (ent->client->breather_framenum > level.framenum)
	{
		ent->client->ps.stats[STAT_TIMER_ICON] = gi.imageindex ("p_rebreather");
		ent->client->ps.stats[STAT_TIMER] = (ent->client->breather_framenum - level.framenum)/10;
	}
	else
	{
		ent->client->ps.stats[STAT_TIMER_ICON] = 0;
		ent->client->ps.stats[STAT_TIMER] = 0;
	}

	//
	// selected item
	//
	if (ent->client->pers.selected_item == -1)
		ent->client->ps.stats[STAT_SELECTED_ICON] = 0;
	else
		ent->client->ps.stats[STAT_SELECTED_ICON] = gi.imageindex (itemlist[ent->client->pers.selected_item].icon);

	ent->client->ps.stats[STAT_SELECTED_ITEM] = ent->client->pers.selected_item;

	//
	// layouts
	//
	ent->client->ps.stats[STAT_LAYOUTS] = 0;

	if (deathmatch->value)
	{
		if (ent->client->pers.health <= 0 || level.intermissiontime
			|| ent->client->showscores)
			ent->client->ps.stats[STAT_LAYOUTS] |= 1;
		if (ent->client->showinventory && ent->client->pers.health > 0)
			ent->client->ps.stats[STAT_LAYOUTS] |= 2;
	}
	else
	{
		if (ent->client->showscores || ent->client->showhelp)
			ent->client->ps.stats[STAT_LAYOUTS] |= 1;
		if (ent->client->showinventory && ent->client->pers.health > 0)
			ent->client->ps.stats[STAT_LAYOUTS] |= 2;
	}

	//
	// frags
	//
	ent->client->ps.stats[STAT_FRAGS] = ent->client->resp.score;

	//
	// help icon / current weapon if not shown
	//
	if (ent->client->pers.helpchanged && (level.framenum&8) )
		ent->client->ps.stats[STAT_HELPICON] = gi.imageindex ("i_help");
	else
		if ( (ent->client->pers.hand == CENTER_HANDED/* || ent->client->ps.fov > 91*/) && ent->client->pers.weapon)
			ent->client->ps.stats[STAT_HELPICON] = gi.imageindex (ent->client->pers.weapon->icon);
		else
			ent->client->ps.stats[STAT_HELPICON] = 0;

	ent->client->ps.stats[STAT_SPECTATOR] = 0;
}


void P_WorldEffects ()
{
	bool	breather;
	bool	envirosuit;
	int		waterlevel, old_waterlevel;

	if (current_player->movetype == MOVETYPE_NOCLIP)
	{
		current_player->air_finished = level.time + 12;	// don't need air
		current_player->wet_finished = level.time - 12;
		return;
	}

	waterlevel = current_player->waterlevel;
	old_waterlevel = current_client->old_waterlevel;
	current_client->old_waterlevel = waterlevel;

	breather = current_client->breather_framenum > level.framenum;
	envirosuit = current_client->enviro_framenum > level.framenum;

	//
	// if just entered a water volume, play a sound
	//
	if (!old_waterlevel && waterlevel)
	{
		PlayerNoise(current_player, current_player->s.origin, PNOISE_SELF);
		if (current_player->watertype & CONTENTS_LAVA)
			gi.sound (current_player, CHAN_BODY, gi.soundindex("player/lava_in.wav"), 1, ATTN_NORM, 0);
		else if (current_player->watertype & CONTENTS_SLIME)
			gi.sound (current_player, CHAN_BODY, gi.soundindex("player/watr_in.wav"), 1, ATTN_NORM, 0);
		else if (current_player->watertype & CONTENTS_WATER)
			gi.sound (current_player, CHAN_BODY, gi.soundindex("player/watr_in.wav"), 1, ATTN_NORM, 0);
		current_player->flags |= FL_INWATER;

		// clear damage_debounce, so the pain sound will play immediately
		current_player->damage_debounce_time = level.time - 1;
	}

	//
	// if just completely exited a water volume, play a sound
	//
	if (old_waterlevel && ! waterlevel)
	{
		PlayerNoise(current_player, current_player->s.origin, PNOISE_SELF);
		gi.sound (current_player, CHAN_BODY, gi.soundindex("player/watr_out.wav"), 1, ATTN_NORM, 0);
		current_player->flags &= ~FL_INWATER;
	}

	//
	// check for head just going under water
	//
	if (old_waterlevel != 3 && waterlevel == 3)
	{
		gi.sound (current_player, CHAN_BODY, gi.soundindex("player/watr_un.wav"), 1, ATTN_NORM, 0);
////		if(!net_compatibility->value && (current_player->watertype & CONTENTS_WATER))
////		{
////			gi.WriteByte (svc_rfx);
////			gi.WriteByte (RFX_UNDERWATER);	// Включаем эффект подводного видения
////			gi.unicast (current_player, true);
////		}
	}

	if ((int)dmflags->value & DF_FOOTPRINT)		// Если разрешены мокрые следы:
	{
		if (waterlevel)
			current_player->wet_finished = level.time + 30;		// Если побывали в воде, сохнем 30 секунд
	}
	else
		current_player->wet_finished = level.time - 30;		// сухие

	//
	// check for head just coming out of water
	//
	if (old_waterlevel == 3 && waterlevel != 3)
	{
////		if(!net_compatibility->value)
////		{
////			gi.WriteByte (svc_rfx);
////			gi.WriteByte (RFX_UNDERWATER+0x80);	// Выключаем эффект подводного видения
////			gi.unicast (current_player, true);
////		}
		if (current_player->air_finished < level.time)
		{	// gasp for air
			gi.sound (current_player, CHAN_VOICE, gi.soundindex("player/gasp1.wav"), 1, ATTN_NORM, 0);
			PlayerNoise(current_player, current_player->s.origin, PNOISE_SELF);
		}
		else  if (current_player->air_finished < level.time + 11)
		{	// just break surface
			gi.sound (current_player, CHAN_VOICE, gi.soundindex("player/gasp2.wav"), 1, ATTN_NORM, 0);
		}
	}

	//
	// check for drowning
	//
	current_client->ps.rdflags &= ~RDF_DROWN;	// Выключаем эффект drown
	if (waterlevel == 3)
	{
		// breather or envirosuit give air
		if (breather || envirosuit)
		{
			current_player->air_finished = level.time + 10;

			if (((int)(current_client->breather_framenum - level.framenum) % 25) == 0)
			{
				if (!current_client->breather_sound)
					gi.sound (current_player, CHAN_AUTO, gi.soundindex("player/u_breath1.wav"), 1, ATTN_NORM, 0);
				else
					gi.sound (current_player, CHAN_AUTO, gi.soundindex("player/u_breath2.wav"), 1, ATTN_NORM, 0);
				current_client->breather_sound ^= 1;
				PlayerNoise(current_player, current_player->s.origin, PNOISE_SELF);
				//FIXME: release a bubble?
			}
		}

		// if out of air, start drowning
		if (current_player->air_finished < level.time)
		{	// drown!
			if (current_player->client->next_drown_time < level.time && current_player->health > 0)
			{
				current_player->client->next_drown_time = level.time + 1;

				// take more damage the longer underwater
				current_player->dmg += 2;
				if (current_player->dmg > 15)
					current_player->dmg = 15;

				// play a gurp sound instead of a normal pain sound
				if (current_player->health <= current_player->dmg)
					gi.sound (current_player, CHAN_VOICE, gi.soundindex("player/drown1.wav"), 1, ATTN_NORM, 0);
				else if (rand()&1)
					gi.sound (current_player, CHAN_VOICE, gi.soundindex("*gurp1.wav"), 1, ATTN_NORM, 0);
				else
					gi.sound (current_player, CHAN_VOICE, gi.soundindex("*gurp2.wav"), 1, ATTN_NORM, 0);

				current_player->pain_debounce_time = level.time;

					current_client->ps.rdflags |= RDF_DROWN;	// Включаем эффект drown
////				if(!net_compatibility->value)
////				{
////					gi.WriteByte (svc_rfx);
////					gi.WriteByte (RFX_DROWN_1_SECOND);	// Включаем эффект захлебывания в течении 1 секунды
////					gi.unicast (current_player, false);
////				}
				T_Damage (current_player, world, world, vec3_origin, current_player->s.origin, vec3_origin, current_player->dmg, 0, DAMAGE_NO_ARMOR, MOD_WATER);
			}
		}
	}
	else
	{
		current_player->air_finished = level.time + 12;
		current_player->dmg = 2;
	}

	//
	// check for sizzle damage
	//
	current_client->ps.rdflags &= ~RDF_BURN;	// Выключаем эффект burn
	if (waterlevel && (current_player->watertype&(CONTENTS_LAVA|CONTENTS_SLIME)) )
	{
		if (current_player->watertype & CONTENTS_LAVA)
		{
			if (current_player->health > 0
				&& current_player->pain_debounce_time <= level.time
				&& current_client->invincible_framenum < level.framenum)
			{
				if (rand()&1)
					gi.sound (current_player, CHAN_VOICE, gi.soundindex("player/burn1.wav"), 1, ATTN_NORM, 0);
				else
					gi.sound (current_player, CHAN_VOICE, gi.soundindex("player/burn2.wav"), 1, ATTN_NORM, 0);

					current_client->ps.rdflags |= RDF_BURN;	// Включаем эффект burn
////				if(!net_compatibility->value)
////				{
////					gi.WriteByte (svc_rfx);
////					gi.WriteByte (RFX_BURN_1_SECOND);	// Включаем эффект горения в течении 1 секунды
////					gi.unicast (current_player, false);
////				}
				current_player->pain_debounce_time = level.time + 1;
			}

			if (envirosuit)	// take 1/3 damage with envirosuit
				T_Damage (current_player, world, world, vec3_origin, current_player->s.origin, vec3_origin, 1*waterlevel, 0, 0, MOD_LAVA);
			else
				T_Damage (current_player, world, world, vec3_origin, current_player->s.origin, vec3_origin, 3*waterlevel, 0, 0, MOD_LAVA);
		}

		if (current_player->watertype & CONTENTS_SLIME)
		{
			if (!envirosuit)
			{	// no damage from slime with envirosuit
				T_Damage (current_player, world, world, vec3_origin, current_player->s.origin, vec3_origin, 1*waterlevel, 0, 0, MOD_SLIME);
			}
		}
	}
}


float SV_CalcRoll (vec3_t angles, vec3_t velocity)
{
	float	sign;
	float	side;
	float	value;

	side = DotProduct (velocity, right);
	sign = side < 0 ? -1 : 1;
	side = fabs(side);

	value = sv_rollangle->value;

	if (side < sv_rollspeed->value)
		side = side * value / sv_rollspeed->value;
	else
		side = value;

	return side*sign;

}


void P_StepsDetect (edict_t *ent)
{
	float	delta;

	if (ent->s.modelindex != 255)
		return;		// not in the player model

	if (ent->movetype == MOVETYPE_NOCLIP)
		return;

	if (ent->waterlevel)
		return;

	if (VectorCompare(ent->velocity, vec3_origin))
	{
		if (!VectorCompare(ent->client->oldvelocity, vec3_origin))
		{
			if (!ent->s.event)
				ent->s.event = ent->wet_finished > level.time ? EV_FOOTSTEP_DOUBLE_PRINT : EV_FOOTSTEP_DOUBLE;
		}
		else
		{
			delta = fabs(ent->client->ps.oldviewangle - ent->client->ps.viewangles[1]);
			if (delta >= 53)	// При повороте более чем на 53 градусов, повернем шаги
			{
				ent->client->ps.oldviewangle = ent->client->ps.viewangles[1];
				if (!ent->s.event)
					ent->s.event = ent->wet_finished > level.time ? EV_FOOTSTEP_DOUBLE_PRINT : EV_FOOTSTEP_DOUBLE;
			}
		}
	}
}


void P_FallingDamage (edict_t *ent)
{
	float	delta;
	int		damage;
	vec3_t	dir;

	if (ent->s.modelindex != 255)
		return;		// not in the player model

	if (ent->movetype == MOVETYPE_NOCLIP)
		return;

	if ((ent->client->oldvelocity[2] < 0) && (ent->velocity[2] > ent->client->oldvelocity[2]) && (!ent->groundentity))
		delta = ent->client->oldvelocity[2];
	else
	{
		if (!ent->groundentity)
			return;
		delta = ent->velocity[2] - ent->client->oldvelocity[2];
	}
	delta = delta*delta * 0.0001;

	// never take falling damage if completely underwater
	if (ent->waterlevel == 3)
		return;
	if (ent->waterlevel == 2)
		delta *= 0.25;
	if (ent->waterlevel == 1)
		delta *= 0.5;

	if (delta < 1)
		return;

	if (delta < 15)
	{
		if((ent->wet_finished > level.time) && (!ent->waterlevel))
			ent->s.event = EV_FOOTSTEP_DOUBLE_PRINT;
		else
			ent->s.event = EV_FOOTSTEP_DOUBLE;

		return;
	}

	ent->client->fall_value = delta*0.5;
	if (ent->client->fall_value > 40)
		ent->client->fall_value = 40;
	ent->client->fall_time = level.time + FALL_TIME;

	if (delta > 30)
	{
		if (ent->health > 0)
		{
			if (delta >= 55)
				ent->s.event = ent->wet_finished > level.time ? EV_FALLFAR_PRINT : EV_FALLFAR;
			else
				ent->s.event = ent->wet_finished > level.time ? EV_FALL_PRINT : EV_FALL;
		}

		if(!net_compatibility->value)
		{
			gi.WriteByte (svc_rfx);
			gi.WriteByte (RFX_PAIN_2_SECOND);	// Включаем эффект боли в течении 2 секунды
			gi.unicast (ent, false);
		}

		ent->pain_debounce_time = level.time;	// no normal pain sound
		damage = (delta-30)/2;
		if (damage < 1)
			damage = 1;
		VectorSet (dir, 0, 0, 1);

		if (!deathmatch->value || !((int)dmflags->value & DF_NO_FALLING) )
			T_Damage (ent, world, world, dir, ent->s.origin, vec3_origin, damage, 0, 0, MOD_FALLING);
	}
	else
	{
		ent->s.event = ent->wet_finished > level.time ? EV_FALLSHORT_PRINT : EV_FALLSHORT;
		return;
	}
}


/*
===============
P_DamageFeedback

Handles color blends and view kicks
===============
*/
void P_DamageFeedback (edict_t *player)
{
	gclient_t	*client;
	float	side;
	float	realcount, count, kick;
	vec3_t	v;
	int		r, l;
	static	vec3_t	power_color = {0.0, 1.0, 0.0};
	static	vec3_t	acolor = {1.0, 1.0, 1.0};
	static	vec3_t	bcolor = {1.0, 0.0, 0.0};

	client = player->client;

	// flash the backgrounds behind the status numbers
	client->ps.stats[STAT_FLASHES] = 0;
	if (client->damage_blood)
		client->ps.stats[STAT_FLASHES] |= 1;
	if (client->damage_armor && !(player->flags & FL_GODMODE) && (client->invincible_framenum <= level.framenum))
		client->ps.stats[STAT_FLASHES] |= 2;

	// total points of damage shot at the player this frame
	count = (client->damage_blood + client->damage_armor + client->damage_parmor);
	if (count == 0)
		return;		// didn't take any damage

	// start a pain animation if still in the player model
	if (client->anim_priority < ANIM_PAIN && player->s.modelindex == 255)
	{
		static int		i;

		client->anim_priority = ANIM_PAIN;
		if (client->ps.pmove.pm_flags & PMF_DUCKED)
		{
			player->s.frame = PLAYER_FRAME_crpain1-1;
			client->anim_end = PLAYER_FRAME_crpain4;
		}
		else
		{
			i = (i+1)%3;
			switch (i)
			{
			case 0:
				player->s.frame = PLAYER_FRAME_pain101-1;
				client->anim_end = PLAYER_FRAME_pain104;
				break;
			case 1:
				player->s.frame = PLAYER_FRAME_pain201-1;
				client->anim_end = PLAYER_FRAME_pain204;
				break;
			case 2:
				player->s.frame = PLAYER_FRAME_pain301-1;
				client->anim_end = PLAYER_FRAME_pain304;
				break;
			}
		}
	}

	realcount = count;
	if (count < 10)
		count = 10;	// always make a visible effect

	// play an apropriate pain sound
	if ((level.time > player->pain_debounce_time) && !(player->flags & FL_GODMODE) && (client->invincible_framenum <= level.framenum))
	{
		r = 1 + (rand()&1);
		player->pain_debounce_time = level.time + 0.7;
		if (player->health < 25)
			l = 25;
		else if (player->health < 50)
			l = 50;
		else if (player->health < 75)
			l = 75;
		else
			l = 100;
		gi.sound (player, CHAN_VOICE, gi.soundindex(va("*pain%i_%i.wav", l, r)), 1, ATTN_NORM, 0);
		if(!net_compatibility->value)
		{
			gi.WriteByte (svc_rfx);
			gi.WriteByte (RFX_PAIN_07_SECOND);	// Включаем эффект боли в течении 0.7 секунды
			gi.unicast (player, false);
		}
	}

	// the total alpha of the blend is always proportional to count
	if (client->damage_alpha < 0)
		client->damage_alpha = 0;
	client->damage_alpha += count*0.01;
	if (client->damage_alpha < 0.2)
		client->damage_alpha = 0.2;
	if (client->damage_alpha > 0.6)
		client->damage_alpha = 0.6;		// don't go too saturated

	// the color of the blend will vary based on how much was absorbed
	// by different armors
	VectorClear (v);
	if (client->damage_parmor)
		VectorMA (v, (float)client->damage_parmor/realcount, power_color, v);
	if (client->damage_armor)
		VectorMA (v, (float)client->damage_armor/realcount,  acolor, v);
	if (client->damage_blood)
		VectorMA (v, (float)client->damage_blood/realcount,  bcolor, v);
	VectorCopy (v, client->damage_blend);


	//
	// calculate view angle kicks
	//
	kick = abs(client->damage_knockback);
	if (kick && player->health > 0)	// kick of 0 means no view adjust at all
	{
		kick = kick * 100 / player->health;

		if (kick < count*0.5)
			kick = count*0.5;
		if (kick > 50)
			kick = 50;

		VectorSubtract (client->damage_from, player->s.origin, v);
		VectorNormalize (v);

		side = DotProduct (v, right);
		client->v_dmg_roll = kick*side*0.3;

		side = -DotProduct (v, forward);
		client->v_dmg_pitch = kick*side*0.3;

		client->v_dmg_time = level.time + DAMAGE_TIME;
	}

	//
	// clear totals
	//
	client->damage_blood = 0;
	client->damage_armor = 0;
	client->damage_parmor = 0;
	client->damage_knockback = 0;
}


void SV_CalcGunOffset (edict_t *ent)
{
	int		i;
	float	delta;

	// gun angles from bobbing
	ent->client->ps.gunangles[ROLL] = xyspeed * bobfracsin * 0.005;
	ent->client->ps.gunangles[YAW] = xyspeed * bobfracsin * 0.01;
	if (bobcycle & 1)
	{
		ent->client->ps.gunangles[ROLL] = -ent->client->ps.gunangles[ROLL];
		ent->client->ps.gunangles[YAW] = -ent->client->ps.gunangles[YAW];
	}

	ent->client->ps.gunangles[PITCH] = xyspeed * bobfracsin * 0.005;

	// gun angles from delta movement
	for (i=0 ; i<3 ; i++)
	{
		delta = ent->client->oldviewangles[i] - ent->client->ps.viewangles[i];
		if (delta > 180)
			delta -= 360;
		if (delta < -180)
			delta += 360;
		if (delta > 45)
			delta = 45;
		if (delta < -45)
			delta = -45;
		if (i == YAW)
			ent->client->ps.gunangles[ROLL] += 0.1*delta;
		ent->client->ps.gunangles[i] += 0.2 * delta;
	}

	// gun height
	VectorClear (ent->client->ps.gunoffset);

	// gun_x / gun_y / gun_z are development tools
	for (i=0 ; i<3 ; i++)
	{
		ent->client->ps.gunoffset[i] += forward[i]*(gun_y->value);
		ent->client->ps.gunoffset[i] += right[i]*gun_x->value;
		ent->client->ps.gunoffset[i] += up[i]* (-gun_z->value);
	}
}


/*
===============
SV_CalcViewOffset

Auto pitching on slopes?

  fall from 128: 400 = 160000
  fall from 256: 580 = 336400
  fall from 384: 720 = 518400
  fall from 512: 800 = 640000
  fall from 640: 960 =

  damage = deltavelocity*deltavelocity  * 0.0001

===============
*/
void SV_CalcViewOffset (edict_t *ent)
{
	float		*angles;
	float		bob;
	float		ratio;
	float		delta;
	vec3_t		v;

//===================================

	// base angles
	angles = ent->client->ps.kick_angles;

	// if dead, fix the angle and don't add any kick
	if (ent->deadflag)
	{
//		VectorClear (angles);
		ent->client->ps.viewangles[ROLL] = 40;
		ent->client->ps.viewangles[PITCH] = -15;
		ent->client->ps.viewangles[YAW] = ent->client->killer_yaw;
	}
	else
	{
		// add angles based on weapon kick
		VectorCopy (ent->client->kick_angles, angles);

		// add angles based on damage kick
		ratio = (ent->client->v_dmg_time - level.time) / DAMAGE_TIME;
		if (ratio < 0)
		{
			ratio = 0;
			ent->client->v_dmg_pitch = 0;
			ent->client->v_dmg_roll = 0;
		}
		angles[PITCH] += ratio * ent->client->v_dmg_pitch;
		angles[ROLL] += ratio * ent->client->v_dmg_roll;

		// add pitch based on fall kick

		ratio = (ent->client->fall_time - level.time) / FALL_TIME;
		if (ratio < 0)
			ratio = 0;
		angles[PITCH] += ratio * ent->client->fall_value;

		// add angles based on velocity

		delta = DotProduct (ent->velocity, forward);
		angles[PITCH] += delta*run_pitch->value;

		delta = DotProduct (ent->velocity, right);
		angles[ROLL] += delta*run_roll->value;

		// add angles based on bob

		delta = bobfracsin * bob_pitch->value * xyspeed;
		if (ent->client->ps.pmove.pm_flags & PMF_DUCKED)
			delta *= 6;		// crouching
		angles[PITCH] += delta;
		delta = bobfracsin * bob_roll->value * xyspeed;
		if (ent->client->ps.pmove.pm_flags & PMF_DUCKED)
			delta *= 6;		// crouching
		if (bobcycle & 1)
			delta = -delta;
		angles[ROLL] += delta;
	}

//===================================

	// base origin

	VectorClear (v);

	// add view height

	v[2] += ent->viewheight;

	// add fall height

	ratio = (ent->client->fall_time - level.time) / FALL_TIME;
	if (ratio < 0)
		ratio = 0;
	v[2] -= ratio * ent->client->fall_value * 0.4;

	// add bob height

	bob = bobfracsin * xyspeed * bob_up->value;
	if (bob > 6)
		bob = 6;
	//gi.DebugGraph (bob *2, 255);
	v[2] += bob;

	// add kick offset

	VectorAdd (v, ent->client->kick_origin, v);

	// absolutely bound offsets
	// so the view can never be outside the player box

	if (v[0] < -14)
		v[0] = -14;
	else if (v[0] > 14)
		v[0] = 14;
	if (v[1] < -14)
		v[1] = -14;
	else if (v[1] > 14)
		v[1] = 14;
	if (v[2] < -22)
		v[2] = -22;
	else if (v[2] > 30)
		v[2] = 30;

	VectorCopy (v, ent->client->ps.viewoffset);
}


void SV_AddBlend (float r, float g, float b, float a, float *blend)
{
	float	a2, a3;

	if (a <= 0)
		return;
	a2 = blend[3] + (1-blend[3])*a;	// new total alpha
	a3 = blend[3]/a2;		// fraction of color from old

	blend[0] = blend[0]*a3 + r*(1-a3);
	blend[1] = blend[1]*a3 + g*(1-a3);
	blend[2] = blend[2]*a3 + b*(1-a3);
	blend[3] = a2;
}


void SV_CalcBlend (edict_t *ent)
{
	int		contents;
	vec3_t	vieworg;
	int		remaining;

	ent->client->ps.blend[0] = ent->client->ps.blend[1] = ent->client->ps.blend[2] = ent->client->ps.blend[3] = 0;

	// add for contents
	VectorAdd (ent->s.origin, ent->client->ps.viewoffset, vieworg);
	contents = gi.pointcontents (vieworg);
	if (contents & (CONTENTS_LAVA|CONTENTS_SLIME|CONTENTS_WATER) )
		ent->client->ps.rdflags |= RDF_UNDERWATER;
	else
		ent->client->ps.rdflags &= ~RDF_UNDERWATER;

	if (contents & CONTENTS_LAVA)
		SV_AddBlend (1.0, 0.3, 0.0, 0.6, ent->client->ps.blend);
	else if (contents & CONTENTS_SLIME)
		SV_AddBlend (0.0, 0.1, 0.05, 0.6, ent->client->ps.blend);
	else if (contents & CONTENTS_WATER)
		SV_AddBlend (0.5, 0.3, 0.2, 0.4, ent->client->ps.blend);
	else if (contents & CONTENTS_SOLID)
		SV_AddBlend (1.0, 0.7, 0.0, 0.3, ent->client->ps.blend);

	// add for powerups
	if (ent->client->quad_framenum > level.framenum)
	{
		remaining = ent->client->quad_framenum - level.framenum;
		if (remaining == 30)	// beginning to fade
			gi.sound(ent, CHAN_ITEM, gi.soundindex("items/damage2.wav"), 1, ATTN_NORM, 0);
		if (remaining > 30 || (remaining & 4) )
			SV_AddBlend (0, 0, 1, 0.08, ent->client->ps.blend);
	}
///	else		/// Berserker's FIX
	if (ent->client->invincible_framenum > level.framenum)
	{
		remaining = ent->client->invincible_framenum - level.framenum;
		if (remaining == 30)	// beginning to fade
			gi.sound(ent, CHAN_ITEM, gi.soundindex("items/protect2.wav"), 1, ATTN_NORM, 0);
		if (remaining > 30 || (remaining & 4) )
			SV_AddBlend (1, 1, 0, 0.08, ent->client->ps.blend);
	}
///	else		/// Berserker's FIX
	if (ent->client->invisible_framenum > level.framenum)
	{
		remaining = ent->client->invisible_framenum - level.framenum;
		if (remaining > 30 || (remaining & 4) )
			SV_AddBlend (1, 1, 0, 0.08, ent->client->ps.blend);
	}
///	else		/// Berserker's FIX
	if (ent->client->enviro_framenum > level.framenum)
	{
		remaining = ent->client->enviro_framenum - level.framenum;
		if (remaining == 30)	// beginning to fade
		{
			gi.sound(ent, CHAN_ITEM, gi.soundindex("items/airout.wav"), 1, ATTN_NORM, 0);
				ent->client->ps.rdflags &= ~RDF_ENVIRO;	// Включаем эффект enviro
				if (!(ent->client->ps.rdflags & RDF_BREATHER))
					ent->client->ps.rdflags &= ~RDF_MASK;
////			if(!net_compatibility->value)
////			{
////				gi.WriteByte (svc_rfx);
////				gi.WriteByte (RFX_ENVIRO+0x80);	// Выключаем эффект enviro
////				gi.unicast (ent, true);
////			}
		}
		if (remaining > 30 || (remaining & 4) )
			SV_AddBlend (0, 1, 0, 0.08, ent->client->ps.blend);
	}
///	else		/// Berserker's FIX
	if (ent->client->breather_framenum > level.framenum)
	{
		remaining = ent->client->breather_framenum - level.framenum;
		if (remaining == 30)	// beginning to fade
		{
			gi.sound(ent, CHAN_ITEM, gi.soundindex("items/airout.wav"), 1, ATTN_NORM, 0);
				ent->client->ps.rdflags &= ~RDF_BREATHER;	// Выключаем эффект breather
				if (!(ent->client->ps.rdflags & RDF_ENVIRO))
					ent->client->ps.rdflags &= ~RDF_MASK;

////			if(!net_compatibility->value)
////			{
////				gi.WriteByte (svc_rfx);
////				gi.WriteByte (RFX_BREATHER+0x80);	// Выключаем эффект breather
////				gi.unicast (ent, true);
////			}
		}
		if (remaining > 30 || (remaining & 4) )
			SV_AddBlend (0.4, 1, 0.4, 0.04, ent->client->ps.blend);
	}

	// add for damage
	if (ent->client->damage_alpha > 0)
		SV_AddBlend (ent->client->damage_blend[0],ent->client->damage_blend[1],ent->client->damage_blend[2], ent->client->damage_alpha, ent->client->ps.blend);

	if (ent->client->bonus_alpha > 0)
		SV_AddBlend (0.85, 0.7, 0.3, ent->client->bonus_alpha, ent->client->ps.blend);

	// drop the damage value
	ent->client->damage_alpha -= 0.06;
	if (ent->client->damage_alpha < 0)
		ent->client->damage_alpha = 0;

	// drop the bonus value
	ent->client->bonus_alpha -= 0.1;
	if (ent->client->bonus_alpha < 0)
		ent->client->bonus_alpha = 0;
}


void G_SetSpectatorStats (edict_t *ent)
{
	gclient_t *cl = ent->client;

	if (!cl->chase_target)
		G_SetStats (ent);

	cl->ps.stats[STAT_SPECTATOR] = 1;

	// layouts are independant in spectator
	cl->ps.stats[STAT_LAYOUTS] = 0;
	if (cl->pers.health <= 0 || level.intermissiontime || cl->showscores)
		cl->ps.stats[STAT_LAYOUTS] |= 1;
	if (cl->showinventory && cl->pers.health > 0)
		cl->ps.stats[STAT_LAYOUTS] |= 2;

	if (cl->chase_target && cl->chase_target->inuse)
	{
		if (net_compatibility->value)
			cl->ps.stats[STAT_CHASE] = CS_PLAYERSKINS_Q2 + (cl->chase_target - g_edicts) - 1;
		else
			cl->ps.stats[STAT_CHASE] = CS_PLAYERSKINS_BERS + (cl->chase_target - g_edicts) - 1;
	}
	else
		cl->ps.stats[STAT_CHASE] = 0;
}


void G_CheckChaseStats (edict_t *ent)
{
	int i;
	gclient_t *cl;

	for (i = 1; i <= maxclients_cvar->value; i++) {
		cl = g_edicts[i].client;
		if (!g_edicts[i].inuse || cl->chase_target != ent)
			continue;
		memcpy(cl->ps.stats, ent->client->ps.stats, sizeof(cl->ps.stats));
		G_SetSpectatorStats(g_edicts + i);
	}
}


void G_SetClientEvent (edict_t *ent)
{
	if (ent->s.event)
		return;

	if (!ent->groundentity)
		return;

	if ( (int)(current_client->bobtime+bobmove) != bobcycle )
	{
		if (ent->waterlevel || (ent->wet_finished < level.time))
		{
			if ( xyspeed > 225)
				ent->s.event = EV_FOOTSTEP;
			else if ( xyspeed > 80)
				ent->s.event = EV_FOOTSTEP2;
		}
		else
		{
			if ( xyspeed > 225)
				ent->s.event = (bobcycle & 1) ? EV_FOOTSTEP_LEFT_PRINT : EV_FOOTSTEP_RIGHT_PRINT;
			else if ( xyspeed > 80)
				ent->s.event = (bobcycle & 1) ? EV_FOOTSTEP2_LEFT_PRINT : EV_FOOTSTEP2_RIGHT_PRINT;
		}
	}
}


void G_SetClientEffects (edict_t *ent)
{
	int		pa_type;
	int		remaining;

	ent->s.effects = 0;
	ent->s.renderfx = 0;

	if (ent->health <= 0 || level.intermissiontime)
		return;

	if (ent->powerarmor_time > level.time)
	{
		pa_type = PowerArmorType (ent);
		if (pa_type == POWER_ARMOR_SCREEN)
		{
			ent->s.effects |= EF_POWERSCREEN;
		}
		else if (pa_type == POWER_ARMOR_SHIELD)
		{
			ent->s.effects |= EF_COLOR_SHELL;
			ent->s.renderfx |= RF_SHELL_GREEN;
		}
	}

	if (ent->client->quad_framenum > level.framenum)
	{
		remaining = ent->client->quad_framenum - level.framenum;
		if (remaining > 30 || (remaining & 4) )
			ent->s.effects |= EF_QUAD;
	}

	if (ent->client->invincible_framenum > level.framenum)
	{
		remaining = ent->client->invincible_framenum - level.framenum;
		if (remaining > 30 || (remaining & 4) )
			ent->s.effects |= EF_PENT;
	}

	if (ent->client->invisible_framenum > level.framenum)
	{
		remaining = ent->client->invisible_framenum - level.framenum;
		if (remaining > 30 || (remaining & 4) )
			ent->s.effects |= EF_INVIS;
	}

	// show cheaters!!!
	if ((ent->flags & FL_GODMODE) && (!((int)dmflags->value & DF_NOGODLIGHT)))
	{
		ent->s.effects |= EF_COLOR_SHELL;
		ent->s.renderfx |= (RF_SHELL_RED|RF_SHELL_GREEN|RF_SHELL_BLUE|RF_LIGHT);	// Пущай светится, гад!  :)
	}

	if(!(net_compatibility->value))
	{
///		if ((ent->client->flashlight) || (ent->client->pers.weapon && !strcmp(ent->client->pers.weapon->classname, "weapon_blaster")))		// Для оружия Blaster:
////		if (ent->client->flashlight)
			if (ent->flags & FL_FLASHLIGHT)
		{
			if (deathmatch->value)												// если в режиме deathmatch
			{
				if ((int)dmflags->value & DF_FLASHLIGHT)						// и если разрешен фонарик
					ent->s.effects |= EF_FLASHLIGHT;	// БУДЕТ ВАМ ФОНАРИК!
			}
			else																// иначе (если не deathmatch)
				ent->s.effects |= EF_FLASHLIGHT;		// БУДЕТ ВАМ ФОНАРИК!
		}
	}

	if (((int)dmflags->value & DF_SHELL) && deathmatch->value && (ent->shell_debounce_time > level.time))
	{	/// Berserker: при попадании в игрока - он на короткое время озаряется вспышкой
		ent->s.effects |= EF_COLOR_SHELL;
		ent->s.renderfx |= (RF_SHELL_RED|RF_LIGHT);
	}
}


void G_SetClientSound (edict_t *ent)
{
	char	*weap;

	if (ent->client->pers.game_helpchanged != game.helpchanged)
	{
		ent->client->pers.game_helpchanged = game.helpchanged;
		ent->client->pers.helpchanged = 1;
	}

	// help beep (no more than three times)
	if (ent->client->pers.helpchanged && ent->client->pers.helpchanged <= 3 && !(level.framenum&63) )
	{
		ent->client->pers.helpchanged++;
		gi.sound (ent, CHAN_VOICE, gi.soundindex ("misc/pc_up.wav"), 1, ATTN_STATIC, 0);
	}


	if (ent->client->pers.weapon)
		weap = ent->client->pers.weapon->classname;
	else
		weap = "";

	if (ent->waterlevel && (ent->watertype&(CONTENTS_LAVA|CONTENTS_SLIME)) )
		ent->s.sound = snd_fry;
	else if (strcmp(weap, "weapon_railgun") == 0)
		ent->s.sound = gi.soundindex("weapons/rg_hum.wav");
	else if (strcmp(weap, "weapon_bfg") == 0)
		ent->s.sound = gi.soundindex("weapons/bfg_hum.wav");
	else if (ent->client->weapon_sound)
		ent->s.sound = ent->client->weapon_sound;
	else
		ent->s.sound = 0;
}


void G_SetClientFrame (edict_t *ent)
{
	gclient_t	*client;
	bool		duck, run;

	if (ent->s.modelindex != 255)
		return;		// not in the player model

	client = ent->client;

	if (client->ps.pmove.pm_flags & PMF_DUCKED)
		duck = true;
	else
		duck = false;
	if (xyspeed)
		run = true;
	else
		run = false;

	// check for stand/duck and stop/go transitions
	if (duck != client->anim_duck && client->anim_priority < ANIM_DEATH)
		goto newanim;
	if (run != client->anim_run && client->anim_priority == ANIM_BASIC)
		goto newanim;
	if (!ent->groundentity && client->anim_priority <= ANIM_WAVE)
		goto newanim;

	if(client->anim_priority == ANIM_REVERSE)
	{
		if(ent->s.frame > client->anim_end)
		{
			ent->s.frame--;
			return;
		}
	}
	else if (ent->s.frame < client->anim_end)
	{	// continue an animation
		ent->s.frame++;
		return;
	}

	if (client->anim_priority == ANIM_DEATH)
		return;		// stay there
	if (client->anim_priority == ANIM_JUMP)
	{
		if (!ent->groundentity)
			return;		// stay there
		ent->client->anim_priority = ANIM_WAVE;
		ent->s.frame = PLAYER_FRAME_jump3;
		ent->client->anim_end = PLAYER_FRAME_jump6;
		return;
	}

newanim:
	// return to either a running or standing frame
	client->anim_priority = ANIM_BASIC;
	client->anim_duck = duck;
	client->anim_run = run;

	if (!ent->groundentity)
	{
		client->anim_priority = ANIM_JUMP;
		if (ent->s.frame != PLAYER_FRAME_jump2)
			ent->s.frame = PLAYER_FRAME_jump1;
		client->anim_end = PLAYER_FRAME_jump2;
	}
	else if (run)
	{	// running
		if (duck)
		{
			ent->s.frame = PLAYER_FRAME_crwalk1;
			client->anim_end = PLAYER_FRAME_crwalk6;
		}
		else
		{
			ent->s.frame = PLAYER_FRAME_run1;
			client->anim_end = PLAYER_FRAME_run6;
		}
	}
	else
	{	// standing
		if (duck)
		{
			ent->s.frame = PLAYER_FRAME_crstnd01;
			client->anim_end = PLAYER_FRAME_crstnd19;
		}
		else
		{
			ent->s.frame = PLAYER_FRAME_stand01;
			client->anim_end = PLAYER_FRAME_stand40;
		}
	}
}


void G_SelectSoundTrack()
{
	edict_t		*ed;

	current_client->ps.rdflags &= ~RDF_COMBATTRACK;

	if (deathmatch->value)
		return;

	if (net_compatibility->value)
		return;

	for (ed = g_edicts ; ed < &g_edicts[globals.num_edicts]; ed++)
	{
		if (!ed->inuse)
			continue;

		if (!(ed->svflags & SVF_MONSTER))
			continue;

		if (ed->deadflag == DEAD_DEAD)
			continue;

		if (ed->monsterinfo.aiflags & AI_GOOD_GUY)
			continue;

		if (ed->enemy == current_player)		// FIXME:	if (ed->enemy)		???
		{
			current_client->ps.rdflags |= RDF_COMBATTRACK;
			break;
		}
	}
}

void SV_CheckVelocity (edict_t *ent)
{
	int		i;

//
// bound velocity
//
	for (i=0 ; i<3 ; i++)
	{
		if (ent->velocity[i] > sv_maxvelocity->value)
			ent->velocity[i] = sv_maxvelocity->value;
		else if (ent->velocity[i] < -sv_maxvelocity->value)
			ent->velocity[i] = -sv_maxvelocity->value;
	}
}

/*
=================
ClientEndServerFrame

Called for each player at the end of the server frame
and right after spawning
=================
*/
void ClientEndServerFrame (edict_t *ent)
{
	float	bobtime;
	int		i;

	current_player = ent;
	current_client = ent->client;

	//
	// If the origin or velocity have changed since ClientThink(),
	// update the pmove values.  This will happen when the client
	// is pushed by a bmodel or kicked by an explosion.
	//
	// If it wasn't updated here, the view position would lag a frame
	// behind the body position when pushed -- "sinking into plats"
	//
	for (i=0 ; i<3 ; i++)
	{
		current_client->ps.pmove.origin[i] = ent->s.origin[i]*8.0;
		current_client->ps.pmove.velocity[i] = ent->velocity[i]*8.0;
	}

	//
	// If the end of unit layout is displayed, don't give
	// the player any normal movement attributes
	//
	if (level.intermissiontime)
	{
		// FIXME: add view drifting here?
		current_client->ps.blend[3] = 0;
		current_client->ps.fov = 90;
		G_SetStats (ent);
		return;
	}

	AngleVectors (ent->client->v_angle, forward, right, up);

	// burn from lava, etc
	P_WorldEffects ();

	//
	// set model angles from view angles so other things in
	// the world can tell which direction you are looking
	//
	if (ent->client->v_angle[PITCH] > 180)
		ent->s.angles[PITCH] = (-360 + ent->client->v_angle[PITCH])/3;
	else
		ent->s.angles[PITCH] = ent->client->v_angle[PITCH]/3;
	ent->s.angles[YAW] = ent->client->v_angle[YAW];
	ent->s.angles[ROLL] = 0;
	ent->s.angles[ROLL] = SV_CalcRoll (ent->s.angles, ent->velocity)*4;

	//
	// calculate speed and cycle to be used for
	// all cyclic walking effects
	//
	xyspeed = sqrt(ent->velocity[0]*ent->velocity[0] + ent->velocity[1]*ent->velocity[1]);

	if (xyspeed < 5)
	{
		bobmove = 0;
		current_client->bobtime = 0;	// start at beginning of cycle again
	}
	else if (ent->groundentity)
	{	// so bobbing only cycles when on ground
		if (xyspeed > 210)
			bobmove = 0.42;		//0.25;
		else if (xyspeed > 100)
			bobmove = 0.35;		//0.125;
		else
			bobmove = 0.4;		//0.0625;
	}

	bobtime = (current_client->bobtime += bobmove);

///	if (current_client->ps.pmove.pm_flags & PMF_DUCKED)
///		bobtime *= 4;

	bobcycle = (int)bobtime;
	bobfracsin = fabs(sin(bobtime*M_PI));

	// Berserker: detect client rotates and stop walking (for double foot steps...)
	P_StepsDetect (ent);

	// Berserker: for bers@q2 net-protocol only
	G_SelectSoundTrack();

	// detect hitting the floor
	P_FallingDamage (ent);

	// apply all the damage taken this frame
	P_DamageFeedback (ent);

	// determine the view offsets
	SV_CalcViewOffset (ent);

	// determine the gun offsets
	SV_CalcGunOffset (ent);

	// determine the full screen color blend
	// must be after viewoffset, so eye contents can be
	// accurately determined
	// FIXME: with client prediction, the contents
	// should be determined by the client
	SV_CalcBlend (ent);

	// chase cam stuff
	if (ent->client->resp.spectator)
		G_SetSpectatorStats(ent);
	else
		G_SetStats (ent);

	G_CheckChaseStats(ent);

	G_SetClientEvent (ent);

	G_SetClientEffects (ent);

	G_SetClientSound (ent);

	G_SetClientFrame (ent);

///	Berserker: bound vertical velocity
	if (ent->velocity[2] > 3000)
		ent->velocity[2] = 3000;
	else if (ent->velocity[2] < -3000)
		ent->velocity[2] = -3000;

	VectorCopy (ent->velocity, ent->client->oldvelocity);
	VectorCopy (ent->client->ps.viewangles, ent->client->oldviewangles);

	// clear weapon kicks
	VectorClear (ent->client->kick_origin);
	VectorClear (ent->client->kick_angles);

	// if the scoreboard is up, update it
	if (ent->client->showscores && !(level.framenum & 31) )
	{
		DeathmatchScoreboardMessage (ent, ent->enemy);
		gi.unicast (ent, false);
	}
}


void ClientEndServerFrames ()
{
	int		i;
	edict_t	*ent;

	// calc the player views now that all pushing
	// and damage has been added
	for (i=0 ; i<maxclients_cvar->value ; i++)
	{
		ent = g_edicts + 1 + i;
		if (!ent->inuse || !ent->client)
			continue;
		ClientEndServerFrame (ent);
	}

}


/*
=====================
ClientBeginDeathmatch

A client has just connected to the server in
deathmatch mode, so clear everything out before starting them.
=====================
*/
void ClientBeginDeathmatch (edict_t *ent)
{
	G_InitEdict (ent);

	InitClientResp (ent->client);

	// locate ent at a spawn point
	PutClientInServer (ent);

	if (level.intermissiontime)
	{
		MoveClientToIntermission (ent);
	}
	else
	{
		// send effect
		gi.WriteByte (svc_muzzleflash);
		gi.WriteShort (ent-g_edicts);
		gi.WriteByte (MZ_LOGIN);
		gi.multicast (ent->s.origin, MULTICAST_PVS);
	}

	gi.bprintf (PRINT_HIGH, "%s entered the game\n", ent->client->pers.netname);

	// make sure all view stuff is valid
	ClientEndServerFrame (ent);
}


/*
===========
ClientBegin

called when a client has finished connecting, and is ready
to be placed into the game.  This will happen every level load.
============
*/
void ClientBegin (edict_t *ent)
{
	int		i;

	ent->client = game.clients + (ent - g_edicts - 1);

	if (deathmatch->value)
	{
		ClientBeginDeathmatch (ent);
		return;
	}

	// if there is already a body waiting for us (a loadgame), just
	// take it, otherwise spawn one from scratch
	if (ent->inuse == true)
	{
		// the client has cleared the client side viewangles upon
		// connecting to the server, which is different than the
		// state when the game is saved, so we need to compensate
		// with deltaangles
		for (i=0 ; i<3 ; i++)
			ent->client->ps.pmove.delta_angles[i] = ANGLE2SHORT(ent->client->ps.viewangles[i]);
	}
	else
	{
		// a spawn point will completely reinitialize the entity
		// except for the persistant data that was initialized at
		// ClientConnect() time
		G_InitEdict (ent);
		ent->classname = "player";
		InitClientResp (ent->client);
		PutClientInServer (ent);
	}

	if (level.intermissiontime)
	{
		MoveClientToIntermission (ent);
	}
	else
	{
		// send effect if in a multiplayer game
		if (game.maxclients > 1)
		{
			gi.WriteByte (svc_muzzleflash);
			gi.WriteShort (ent-g_edicts);
			gi.WriteByte (MZ_LOGIN);
			gi.multicast (ent->s.origin, MULTICAST_PVS);

			gi.bprintf (PRINT_HIGH, "%s entered the game\n", ent->client->pers.netname);
		}
	}

	// make sure all view stuff is valid
	ClientEndServerFrame (ent);
}


int PlayerSort (void const *a, void const *b)
{
	int		anum, bnum;

	anum = *(int *)a;
	bnum = *(int *)b;

	anum = game.clients[anum].ps.stats[STAT_FRAGS];
	bnum = game.clients[bnum].ps.stats[STAT_FRAGS];

	if (anum < bnum)
		return -1;
	if (anum > bnum)
		return 1;
	return 0;
}


void Cmd_Players_f (edict_t *ent)
{
	int		i;
	int		count;
	char	Small[128];
	char	large[1280];
	int		index[256];

	count = 0;
	for (i = 0 ; i < maxclients_cvar->value ; i++)
		if (game.clients[i].pers.connected)
		{
			index[count] = i;
			count++;
		}

	// sort by frags
	qsort (index, count, sizeof(index[0]), PlayerSort);

	// print information
	large[0] = 0;

	for (i = 0 ; i < count ; i++)
	{
		Com_sprintf (Small, sizeof(Small), "%3i %s\n",
			game.clients[index[i]].ps.stats[STAT_FRAGS],
			game.clients[index[i]].pers.netname);
		if (strlen (Small) + strlen(large) > sizeof(large) - 100 )
		{	// can't print all of them in one packet
			strcat (large, "...\n");
			break;
		}
		strcat (large, Small);
	}

	gi.cprintf (ent, PRINT_HIGH, "%s\n%i players\n", large, count);
}


void Cmd_Say_f (edict_t *ent, bool team, bool arg0)
{
	int		i, j;
	edict_t	*other;
	char	*p;
	char	text[2048];
	gclient_t *cl;

	if (gi.argc () < 2 && !arg0)
		return;

	if (!((int)(dmflags->value) & (DF_MODELTEAMS | DF_SKINTEAMS)))
		team = false;

	if (team)
		Com_sprintf (text, sizeof(text), "(%s): ", ent->client->pers.netname);
	else
		Com_sprintf (text, sizeof(text), "%s: ", ent->client->pers.netname);

	if (arg0)
	{
		strcat (text, gi.argv(0));
		strcat (text, " ");
		strcat (text, gi.args());
	}
	else
	{
		p = gi.args();

		if (*p == '"')
		{
			p++;
			p[strlen(p)-1] = 0;
		}
		strcat(text, p);
	}

	// don't let text be too long for malicious reasons
	if (strlen(text) > 150)
		text[150] = 0;

	strcat(text, "\n");

	if (flood_msgs->value) {
		cl = ent->client;

        if (level.time < cl->flood_locktill)
		{
			gi.cprintf(ent, PRINT_HIGH, "You can't talk for %d more seconds\n", (int)ceil(cl->flood_locktill - level.time));
            return;
        }
        i = cl->flood_whenhead - flood_msgs->value + 1;
        if (i < 0)
            i = (sizeof(cl->flood_when)/sizeof(cl->flood_when[0])) + i;
		if (cl->flood_when[i] && level.time - cl->flood_when[i] < flood_persecond->value)
		{
			cl->flood_locktill = level.time + flood_waitdelay->value;
			gi.cprintf(ent, PRINT_CHAT, "Flood protection:  You can't talk for %d seconds.\n", (int)flood_waitdelay->value);
            return;
        }
		cl->flood_whenhead = (cl->flood_whenhead + 1) %
			(sizeof(cl->flood_when)/sizeof(cl->flood_when[0]));
		cl->flood_when[cl->flood_whenhead] = level.time;
	}

	if (dedicated->value)
		gi.cprintf(NULL, PRINT_CHAT, "%s", text);

	for (j = 1; j <= game.maxclients; j++)
	{
		other = &g_edicts[j];
		if (!other->inuse)
			continue;
		if (!other->client)
			continue;
		if (team)
		{
			if (!OnSameTeam(ent, other))
				continue;
		}
		gi.cprintf(other, PRINT_CHAT, "%s", text);
	}
}


/*
==================
Cmd_Use_f

Use an inventory item
==================
*/
void Cmd_Use_f (edict_t *ent)
{
	int			index;
	gitem_t		*it;
	char		*s;

	s = gi.args();
	it = FindItem (s);
	if (!it)
	{
		gi.cprintf (ent, PRINT_HIGH, "unknown item: %s\n", s);
		return;
	}
	if (!it->use)
	{
		gi.cprintf (ent, PRINT_HIGH, "Item is not usable.\n");
		return;
	}
	index = ITEM_INDEX(it);
	if (!ent->client->pers.inventory[index])
	{
		gi.cprintf (ent, PRINT_HIGH, "Out of item: %s\n", s);
		return;
	}

	it->use (ent, it);
}


/*
==================
Cmd_Drop_f

Drop an inventory item
==================
*/
void Cmd_Drop_f (edict_t *ent)
{
	int			index;
	gitem_t		*it;
	char		*s;

	s = gi.args();
	it = FindItem (s);
	if (!it)
	{
		gi.cprintf (ent, PRINT_HIGH, "unknown item: %s\n", s);
		return;
	}
	if (!it->drop)
	{
		gi.cprintf (ent, PRINT_HIGH, "Item is not dropable.\n");
		return;
	}
	index = ITEM_INDEX(it);
	if (!ent->client->pers.inventory[index])
	{
		gi.cprintf (ent, PRINT_HIGH, "Out of item: %s\n", s);
		return;
	}

	it->drop (ent, it);
}


bool Add_Ammo (edict_t *ent, gitem_t *item, int count)
{
	int			index;
	int			max;

	if (!ent->client)
		return false;

	if (item->tag == AMMO_BULLETS)
		max = ent->client->pers.max_bullets;
	else if (item->tag == AMMO_SHELLS)
		max = ent->client->pers.max_shells;
	else if (item->tag == AMMO_ROCKETS)
		max = ent->client->pers.max_rockets;
	else if (item->tag == AMMO_GRENADES)
		max = ent->client->pers.max_grenades;
	else if (item->tag == AMMO_CELLS)
		max = ent->client->pers.max_cells;
	else if (item->tag == AMMO_SLUGS)
		max = ent->client->pers.max_slugs;
	else
		return false;

	index = ITEM_INDEX(item);

	if (ent->client->pers.inventory[index] == max)
		return false;

	ent->client->pers.inventory[index] += count;

	if (ent->client->pers.inventory[index] > max)
		ent->client->pers.inventory[index] = max;

	return true;
}


void Use_PowerArmor (edict_t *ent, gitem_t *item)
{
	int		index;

	if (ent->flags & FL_POWER_ARMOR)
	{
		ent->flags &= ~FL_POWER_ARMOR;
		gi.sound(ent, CHAN_AUTO, gi.soundindex("misc/power2.wav"), 1, ATTN_NORM, 0);
	}
	else
	{
		index = ITEM_INDEX(FindItem("cells"));
		if (!ent->client->pers.inventory[index])
		{
			gi.cprintf (ent, PRINT_HIGH, "No cells for power armor.\n");
			return;
		}
		ent->flags |= FL_POWER_ARMOR;
		gi.sound(ent, CHAN_AUTO, gi.soundindex("misc/power1.wav"), 1, ATTN_NORM, 0);
	}
}


void Drop_General (edict_t *ent, gitem_t *item)
{
	Drop_Item (ent, item);
	ent->client->pers.inventory[ITEM_INDEX(item)]--;
	ValidateSelectedItem (ent);
}


void Drop_PowerArmor (edict_t *ent, gitem_t *item)
{
	if ((ent->flags & FL_POWER_ARMOR) && (ent->client->pers.inventory[ITEM_INDEX(item)] == 1))
		Use_PowerArmor (ent, item);
	Drop_General (ent, item);
}


void Cmd_Spawn_f (edict_t *self)
{
	char		*s;
	spawn_t		*spawn;
	char		name[MAX_QPATH];
	edict_t		*ent;
	trace_t		tr;
	vec3_t		forward, point, center;

	if (deathmatch->value)
	{
		gi.cprintf (self, PRINT_HIGH, "Not for deathmatch.\n");
		return;
	}

	if (gi.argc()==1)
	{
		gi.cprintf (self, PRINT_HIGH, "Available monsters:\n-------------------\nberserk\nboss2\nboss3_stand\nbrain\nchick\ncommander_body\ninfantry\nflipper\nfloater\nflyer\ngladiator\ngunner\nhover\njorg\nmedic\nmutant\nparasite\nsoldier\nsoldier_light\nsoldier_ss\nsupertank\ntank\ntank_commander\ninsane\nexplobox\n");
		return;
	}

	s = gi.args();
	if (!Q_strcasecmp(s, "insane"))
		Com_sprintf (name, sizeof(name), "misc_insane");
	else if (!Q_strcasecmp(s, "explobox"))
		Com_sprintf (name, sizeof(name), "misc_explobox");
	else
		Com_sprintf (name, sizeof(name), "monster_%s", s);

	for (spawn = spawns; spawn->name; spawn++)
	{
		if (!Q_strcasecmp(spawn->name, name))
		{	// found it
			ent = G_Spawn();
			ent->mass = 0;
			ent->health = 0;
			ent->dmg = 0;
			ent->spawnflags = 0;
			ent->classname = spawn->name;
			VectorCopy (self->s.angles, ent->s.angles);
ent->s.angles[0] = 0;
ent->s.angles[1] = self->s.angles[1];
ent->s.angles[2] = 0;
			VectorClear (ent->velocity);
AngleVectors(self->s.angles, forward, NULL, NULL);
if (net_compatibility->value)
	VectorMA (self->s.origin, 8192, forward, point);
else
	VectorMA (self->s.origin, 32768, forward, point);	// LARGE_MAP_SIZE
tr = gi.trace (self->s.origin, NULL, NULL, point, self, MASK_SHOT);
VectorAdd(tr.endpos, self->s.origin, center);
VectorScale(center, 0.5, ent->s.origin);
			KillBox (ent);
			spawn->spawn (ent);
			return;
		}
	}
	gi.cprintf (self, PRINT_HIGH, "%s not found\n", name);
}

/*
==================
Cmd_Give_f

Give items to a client
==================
*/
void Cmd_Give_f (edict_t *ent)
{
	char		*name;
	gitem_t		*it;
	int			index;
	int			i;
	bool		give_all;
	edict_t		*it_ent;

	if (deathmatch->value && !sv_cheats->value)
	{
		gi.cprintf (ent, PRINT_HIGH, "You must run the server with '+set cheats 1' to enable this command.\n");
		return;
	}

	name = gi.args();

	if (Q_strcasecmp(name, "all") == 0)
		give_all = true;
	else
		give_all = false;

	if (give_all || Q_strcasecmp(gi.argv(1), "health") == 0)
	{
		if (gi.argc() == 3)
			ent->health = atoi(gi.argv(2));
		else
			ent->health = ent->max_health;
		if (!give_all)
			return;
	}

	if (give_all || Q_strcasecmp(name, "weapons") == 0)
	{
		for (i=0 ; i<game.num_items ; i++)
		{
			it = itemlist + i;
			if (!it->pickup)
				continue;
			if (!(it->flags & IT_WEAPON))
				continue;
			ent->client->pers.inventory[i] += 1;
		}
		if (!give_all)
			return;
	}

	if (give_all || Q_strcasecmp(name, "ammo") == 0)
	{
		for (i=0 ; i<game.num_items ; i++)
		{
			it = itemlist + i;
			if (!it->pickup)
				continue;
			if (!(it->flags & IT_AMMO))
				continue;
			Add_Ammo (ent, it, 1000);
		}
		if (!give_all)
			return;
	}

	if (give_all || Q_strcasecmp(name, "armor") == 0)
	{
		gitem_armor_t	*info;

		it = FindItem("Jacket Armor");
		ent->client->pers.inventory[ITEM_INDEX(it)] = 0;

		it = FindItem("Combat Armor");
		ent->client->pers.inventory[ITEM_INDEX(it)] = 0;

		it = FindItem("Body Armor");
		info = (gitem_armor_t *)it->info;
		ent->client->pers.inventory[ITEM_INDEX(it)] = info->max_count;

		if (!give_all)
			return;
	}

	if (give_all || Q_strcasecmp(name, "Power Shield") == 0)
	{
		it = FindItem("Power Shield");
		it_ent = G_Spawn();
		it_ent->classname = it->classname;
		SpawnItem (it_ent, it);
		Touch_Item (it_ent, ent, NULL, NULL);
		if (it_ent->inuse)
			G_FreeEdict(it_ent);

		if (!give_all)
			return;
	}

	if (give_all)
	{
		for (i=0 ; i<game.num_items ; i++)
		{
			it = itemlist + i;
			if (!it->pickup)
				continue;
			if (it->flags & (IT_ARMOR|IT_WEAPON|IT_AMMO))
				continue;
			if (net_compatibility->value)
				if (!strcmp(it->pickup_name, "Invisibility"))
					continue;
			ent->client->pers.inventory[i] = 1;
		}
		return;
	}

	it = FindItem (name);
	if (!it)
	{
		name = gi.argv(1);
		it = FindItem (name);
		if (!it)
		{
			gi.cprintf (ent, PRINT_HIGH, "unknown item\n");
			return;
		}
	}

	if (!it->pickup)
	{
		gi.cprintf (ent, PRINT_HIGH, "non-pickup item\n");
		return;
	}

	if (net_compatibility->value)
		if (!strcmp(it->pickup_name, "Invisibility"))
		{
			gi.cprintf (ent, PRINT_HIGH, "item_invisibility disabled for Quake2 net protocol\n");
			return;
		}

	index = ITEM_INDEX(it);

	if (it->flags & IT_AMMO)
	{
		if (gi.argc() == 3)
			ent->client->pers.inventory[index] = atoi(gi.argv(2));
		else
			ent->client->pers.inventory[index] += it->quantity;
	}
	else
	{
		it_ent = G_Spawn();
		it_ent->classname = it->classname;
		SpawnItem (it_ent, it);
		Touch_Item (it_ent, ent, NULL, NULL);
		if (it_ent->inuse)
			G_FreeEdict(it_ent);
	}
}


/// Berserker: kill all monsters
void Cmd_KillMonsters_f (edict_t *ent)
{
	edict_t		*ed;

	if (deathmatch->value)
		return;

	unsigned	k = 0;
	for (ed = g_edicts ; ed < &g_edicts[globals.num_edicts]; ed++)
	{
		if (!ed->inuse)
			continue;

		if (!(ed->svflags & SVF_MONSTER))
			continue;

		if (ed->deadflag == DEAD_DEAD)
			continue;

		if (ed->monsterinfo.aiflags & AI_GOOD_GUY)
			continue;

		if (ed->die)
		{
			ed->health = -1;
			Killed(ed, NULL, ent, 1, ed->s.origin);
			k++;
		}
	}

	unsigned	n = 0;
	unsigned	i = 0;
	for (ed = g_edicts ; ed < &g_edicts[globals.num_edicts]; ed++)
	{
		if (!ed->inuse)
			continue;

		if (!(ed->svflags & SVF_MONSTER))
			continue;

		if (ed->deadflag == DEAD_DEAD)
			continue;

		if (ed->monsterinfo.aiflags & AI_GOOD_GUY)
			continue;

		if (ed->die)
			n++;
		else
			i++;
	}

	if (k)
		gi.cprintf (ent, PRINT_HIGH, "Killed %i monsters\n", k);
	if (n)
		gi.cprintf (ent, PRINT_HIGH, "%i monsters survived\n", n);
	if (i)
		gi.cprintf (ent, PRINT_HIGH, "%i immortal monsters found!\n", n);
}


/*
==================
Cmd_God_f

Sets client to godmode

argv(0) god
==================
*/
void Cmd_God_f (edict_t *ent)
{
	char	*msg;

	if (deathmatch->value && !sv_cheats->value)
	{
		gi.cprintf (ent, PRINT_HIGH, "You must run the server with '+set cheats 1' to enable this command.\n");
		return;
	}

	ent->flags ^= FL_GODMODE;
	if (!(ent->flags & FL_GODMODE) )
		msg = "godmode OFF\n";
	else
		msg = "godmode ON\n";

	gi.cprintf (ent, PRINT_HIGH, msg);
}


/*
==================
Cmd_Notarget_f

Sets client to notarget

argv(0) notarget
==================
*/
void Cmd_Notarget_f (edict_t *ent)
{
	char	*msg;

	if (deathmatch->value && !sv_cheats->value)
	{
		gi.cprintf (ent, PRINT_HIGH, "You must run the server with '+set cheats 1' to enable this command.\n");
		return;
	}

	ent->flags ^= FL_NOTARGET;
	if (!(ent->flags & FL_NOTARGET) )
		msg = "notarget OFF\n";
	else
		msg = "notarget ON\n";

	gi.cprintf (ent, PRINT_HIGH, msg);
}


/*
==================
Cmd_Noclip_f

argv(0) noclip
==================
*/
void Cmd_Noclip_f (edict_t *ent)
{
	char	*msg;

	if (deathmatch->value && !sv_cheats->value)
	{
		gi.cprintf (ent, PRINT_HIGH, "You must run the server with '+set cheats 1' to enable this command.\n");
		return;
	}

	// Berserker: не позволим мертвецу перемещаться, ибо это чревато ошибкой "ERROR: Game Error: SV_Physics: bad movetype 4"
	// да и подсматривать трупу должностная инструкция не позволяет ))
	if (ent->deadflag)
	{
		gi.cprintf (ent, PRINT_HIGH, "You must be alive.\n");
		return;
	}

	if (ent->movetype == MOVETYPE_NOCLIP)
	{
		ent->movetype = MOVETYPE_WALK;
		msg = "noclip OFF\n";
	}
	else
	{
		ent->movetype = MOVETYPE_NOCLIP;
		msg = "noclip ON\n";
	}

	gi.cprintf (ent, PRINT_HIGH, msg);
}


void Cmd_Inven_f (edict_t *ent)
{
	int			i;
	gclient_t	*cl;

	cl = ent->client;

	cl->showscores = false;
	cl->showhelp = false;

	if (cl->showinventory)
	{
		cl->showinventory = false;
		return;
	}

	cl->showinventory = true;

	gi.WriteByte (svc_inventory);
	for (i=0 ; i<MAX_ITEMS ; i++)
	{
		gi.WriteShort (cl->pers.inventory[i]);
	}
	gi.unicast (ent, true);
}


void Cmd_InvUse_f (edict_t *ent)
{
	gitem_t		*it;

	ValidateSelectedItem (ent);

	if (ent->client->pers.selected_item == -1)
	{
		gi.cprintf (ent, PRINT_HIGH, "No item to use.\n");
		return;
	}

	it = &itemlist[ent->client->pers.selected_item];
	if (!it->use)
	{
		gi.cprintf (ent, PRINT_HIGH, "Item is not usable.\n");
		return;
	}
	it->use (ent, it);
}


void Cmd_WeapPrev_f (edict_t *ent)
{
	gclient_t	*cl;
	int			i, index;
	gitem_t		*it;
	int			selected_weapon;

	cl = ent->client;

	if (!cl->pers.weapon)
		return;

	selected_weapon = ITEM_INDEX(cl->pers.weapon);

	// scan  for the next valid one
	for (i=1 ; i<=MAX_ITEMS ; i++)
	{
		index = (selected_weapon + i)%MAX_ITEMS;
		if (!cl->pers.inventory[index])
			continue;
		it = &itemlist[index];
		if (!it->use)
			continue;
		if (! (it->flags & IT_WEAPON) )
			continue;
		it->use (ent, it);
		if (cl->pers.weapon == it)
			return;	// successful
	}
}


void Cmd_WeapNext_f (edict_t *ent)
{
	gclient_t	*cl;
	int			i, index;
	gitem_t		*it;
	int			selected_weapon;

	cl = ent->client;

	if (!cl->pers.weapon)
		return;

	selected_weapon = ITEM_INDEX(cl->pers.weapon);

	// scan  for the next valid one
	for (i=1 ; i<=MAX_ITEMS ; i++)
	{
		index = (selected_weapon + MAX_ITEMS - i)%MAX_ITEMS;
		if (!cl->pers.inventory[index])
			continue;
		it = &itemlist[index];
		if (!it->use)
			continue;
		if (! (it->flags & IT_WEAPON) )
			continue;
		it->use (ent, it);
		if (cl->pers.weapon == it)
			return;	// successful
	}
}


void Cmd_WeapLast_f (edict_t *ent)
{
	gclient_t	*cl;
	int			index;
	gitem_t		*it;

	cl = ent->client;

	if (!cl->pers.weapon || !cl->pers.lastweapon)
		return;

	index = ITEM_INDEX(cl->pers.lastweapon);
	if (!cl->pers.inventory[index])
		return;
	it = &itemlist[index];
	if (!it->use)
		return;
	if (! (it->flags & IT_WEAPON) )
		return;
	it->use (ent, it);
}


void Cmd_InvDrop_f (edict_t *ent)
{
	gitem_t		*it;

	ValidateSelectedItem (ent);

	if (ent->client->pers.selected_item == -1)
	{
		gi.cprintf (ent, PRINT_HIGH, "No item to drop.\n");
		return;
	}

	it = &itemlist[ent->client->pers.selected_item];
	if (!it->drop)
	{
		gi.cprintf (ent, PRINT_HIGH, "Item is not dropable.\n");
		return;
	}
	it->drop (ent, it);
}


void Cmd_Wave_f (edict_t *ent)
{
	int		i;

	i = atoi (gi.argv(1));

	// can't wave when ducked
	if (ent->client->ps.pmove.pm_flags & PMF_DUCKED)
		return;

	if (ent->client->anim_priority > ANIM_WAVE)
		return;

	if (ent->taunt_finished > level.time)
		return;

	ent->client->anim_priority = ANIM_WAVE;
	ent->taunt_finished = level.time + 4;	// no dup.waves for 4 seconds

	switch (i)
	{
	case 0:
///		gi.cprintf (ent, PRINT_HIGH, "flipoff\n");
		ent->s.frame = PLAYER_FRAME_flip01-1;
		ent->client->anim_end = PLAYER_FRAME_flip12;
		break;
	case 1:
///		gi.cprintf (ent, PRINT_HIGH, "salute\n");
		ent->s.frame = PLAYER_FRAME_salute01-1;
		ent->client->anim_end = PLAYER_FRAME_salute11;
		break;
	case 3:
///		gi.cprintf (ent, PRINT_HIGH, "wave\n");
		ent->s.frame = PLAYER_FRAME_wave01-1;
		ent->client->anim_end = PLAYER_FRAME_wave11;
		break;
	case 4:
///		gi.cprintf (ent, PRINT_HIGH, "point\n");
		ent->s.frame = PLAYER_FRAME_point01-1;
		ent->client->anim_end = PLAYER_FRAME_point12;
		break;
	default:
///	case 2:
///		gi.cprintf (ent, PRINT_HIGH, "taunt\n");
		ent->s.frame = PLAYER_FRAME_taunt01-1;
		ent->client->anim_end = PLAYER_FRAME_taunt17;
		gi.sound(ent, CHAN_AUTO, gi.soundindex(va("*taunt%i.wav", (rand()%2)+1)), 1, ATTN_NORM, 0);
		break;
	}
}


void Cmd_Kill_f (edict_t *ent)
{
	if((level.time - ent->client->respawn_time) < 5)
		return;
	ent->flags &= ~FL_GODMODE;
	ent->health = 0;
	meansOfDeath = MOD_SUICIDE;
	player_die (ent, ent, ent, 100000, vec3_origin);
}


void Cmd_PutAway_f (edict_t *ent)
{
	ent->client->showscores = false;
	ent->client->showhelp = false;
	ent->client->showinventory = false;
}


void Cmd_PlayerList_f(edict_t *ent)
{
	int i;
	char st[80];
	char text[1400];
	edict_t *e2;

	// connect time, ping, score, name
	*text = 0;
	for (i = 0, e2 = g_edicts + 1; i < maxclients_cvar->value; i++, e2++) {
		if (!e2->inuse)
			continue;

		Com_sprintf(st, sizeof(st), "%02d:%02d %4d %3d %s%s\n",
			(level.framenum - e2->client->resp.enterframe) / 600,
			((level.framenum - e2->client->resp.enterframe) % 600)/10,
			e2->client->ping,
			e2->client->resp.score,
			e2->client->pers.netname,
			e2->client->resp.spectator ? " (spectator)" : "");
		if (strlen(text) + strlen(st) > sizeof(text) - 50) {
			sprintf(text+strlen(text), "And more...\n");
			gi.cprintf(ent, PRINT_HIGH, "%s", text);
			return;
		}
		strcat(text, st);
	}
	gi.cprintf(ent, PRINT_HIGH, "%s", text);
}


void ClientCommand (edict_t *ent)
{
	char	*cmd;

	if (!ent->client)
		return;		// not fully in game yet

	cmd = gi.argv(0);

	if (Q_strcasecmp (cmd, "players") == 0)
	{
		Cmd_Players_f (ent);
		return;
	}
	if (Q_strcasecmp (cmd, "say") == 0)
	{
		Cmd_Say_f (ent, false, false);
		return;
	}
	if (Q_strcasecmp (cmd, "say_team") == 0)
	{
		Cmd_Say_f (ent, true, false);
		return;
	}
	if (Q_strcasecmp (cmd, "score") == 0)
	{
		Cmd_Score_f (ent);
		return;
	}
	if (Q_strcasecmp (cmd, "help") == 0)
	{
		Cmd_Help_f (ent);
		return;
	}

	if (level.intermissiontime)
		return;

	if (Q_strcasecmp (cmd, "use") == 0)
		Cmd_Use_f (ent);
	else if (Q_strcasecmp (cmd, "drop") == 0)
		Cmd_Drop_f (ent);
	else if (Q_strcasecmp (cmd, "give") == 0)
		Cmd_Give_f (ent);
	else if (Q_strcasecmp (cmd, "spawn") == 0)
		Cmd_Spawn_f (ent);
	else if (Q_strcasecmp (cmd, "god") == 0)
		Cmd_God_f (ent);
	else if (Q_strcasecmp (cmd, "killmonsters") == 0)
		Cmd_KillMonsters_f (ent);
	else if (Q_strcasecmp (cmd, "notarget") == 0)
		Cmd_Notarget_f (ent);
	else if (Q_strcasecmp (cmd, "noclip") == 0)
		Cmd_Noclip_f (ent);
	else if (Q_strcasecmp (cmd, "inven") == 0)
		Cmd_Inven_f (ent);
	else if (Q_strcasecmp (cmd, "invnext") == 0)
		SelectNextItem (ent, -1);
	else if (Q_strcasecmp (cmd, "invprev") == 0)
		SelectPrevItem (ent, -1);
	else if (Q_strcasecmp (cmd, "invnextw") == 0)
		SelectNextItem (ent, IT_WEAPON);
	else if (Q_strcasecmp (cmd, "invprevw") == 0)
		SelectPrevItem (ent, IT_WEAPON);
	else if (Q_strcasecmp (cmd, "invnextp") == 0)
		SelectNextItem (ent, IT_POWERUP);
	else if (Q_strcasecmp (cmd, "invprevp") == 0)
		SelectPrevItem (ent, IT_POWERUP);
	else if (Q_strcasecmp (cmd, "invuse") == 0)
		Cmd_InvUse_f (ent);
	else if (Q_strcasecmp (cmd, "invdrop") == 0)
		Cmd_InvDrop_f (ent);
	else if (Q_strcasecmp (cmd, "weapprev") == 0)
		Cmd_WeapPrev_f (ent);
	else if (Q_strcasecmp (cmd, "weapnext") == 0)
		Cmd_WeapNext_f (ent);
	else if (Q_strcasecmp (cmd, "weaplast") == 0)
		Cmd_WeapLast_f (ent);
	else if (Q_strcasecmp (cmd, "kill") == 0)
		Cmd_Kill_f (ent);
	else if (Q_strcasecmp (cmd, "putaway") == 0)
		Cmd_PutAway_f (ent);
	else if (Q_strcasecmp (cmd, "wave") == 0)
		Cmd_Wave_f (ent);
	else if (Q_strcasecmp(cmd, "playerlist") == 0)
		Cmd_PlayerList_f(ent);


/// Если добавляешь тут функцию, не забудь аналогично дописать в main.cpp/CL_InitLocal()


	else	// anything that doesn't match a command will be a chat
		Cmd_Say_f (ent, false, true);
}


void ExitLevel ()
{
	int		i;
	edict_t	*ent;
	char	command [256];

	Com_sprintf (command, sizeof(command), "gamemap \"%s\"\n", level.changemap);
	gi.AddCommandString (command);
	level.changemap = NULL;
	level.exitintermission = 0;
	level.intermissiontime = 0;
	ClientEndServerFrames ();

	// clear some things before going to next level
	for (i=0 ; i<maxclients_cvar->value ; i++)
	{
		ent = g_edicts + 1 + i;
		if (!ent->inuse)
			continue;
		if (ent->health > ent->client->pers.max_health)
			ent->health = ent->client->pers.max_health;
	}

}


void M_CheckGround (edict_t *ent)
{
	vec3_t		point;
	trace_t		trace;

	if (ent->flags & (FL_SWIM|FL_FLY))
		return;

	if (ent->velocity[2] > 100)
	{
		ent->groundentity = NULL;
		return;
	}

	// if the hull point one-quarter unit down is solid the entity is on ground
	point[0] = ent->s.origin[0];
	point[1] = ent->s.origin[1];
	point[2] = ent->s.origin[2] - 0.25;

	trace = gi.trace (ent->s.origin, ent->mins, ent->maxs, point, ent, MASK_MONSTERSOLID);

	// check steepness
	if ( trace.plane.normal[2] < 0.7 && !trace.startsolid)
	{
		ent->groundentity = NULL;
		return;
	}

	if (!trace.startsolid && !trace.allsolid)
	{
		VectorCopy (trace.endpos, ent->s.origin);
		ent->groundentity = trace.ent;
		ent->groundentity_linkcount = trace.ent->linkcount;
		ent->velocity[2] = 0;
	}
}


/*
 * only called when pers.spectator changes
 * note that resp.spectator should be the opposite of pers.spectator here
 */
void spectator_respawn (edict_t *ent)
{
	int i, numspec;

	// if the user wants to become a spectator, make sure he doesn't
	// exceed max_spectators

	if (ent->client->pers.spectator) {
		char *value = Info_ValueForKey (ent->client->pers.userinfo, "spectator");
		if (*spectator_password->string &&
			strcmp(spectator_password->string, "none") &&
			strcmp(spectator_password->string, value)) {
			gi.cprintf(ent, PRINT_HIGH, "Spectator password incorrect.\n");
			ent->client->pers.spectator = false;
			gi.WriteByte (svc_stufftext);
			gi.WriteString ("spectator 0\n");
			gi.unicast(ent, true);
			return;
		}

		// count spectators
		for (i = 1, numspec = 0; i <= maxclients_cvar->value; i++)
			if (g_edicts[i].inuse && g_edicts[i].client->pers.spectator)
				numspec++;

		if (numspec >= maxspectators->value) {
			gi.cprintf(ent, PRINT_HIGH, "Server spectator limit is full.");
			ent->client->pers.spectator = false;
			// reset his spectator var
			gi.WriteByte (svc_stufftext);
			gi.WriteString ("spectator 0\n");
			gi.unicast(ent, true);
			return;
		}
	} else {
		// he was a spectator and wants to join the game
		// he must have the right password
		char *value = Info_ValueForKey (ent->client->pers.userinfo, "password");
		if (*password->string && strcmp(password->string, "none") &&
			strcmp(password->string, value)) {
			gi.cprintf(ent, PRINT_HIGH, "Password incorrect.\n");
			ent->client->pers.spectator = true;
			gi.WriteByte (svc_stufftext);
			gi.WriteString ("spectator 1\n");
			gi.unicast(ent, true);
			return;
		}
	}

	// clear client on respawn
	ent->client->resp.score = ent->client->pers.score = 0;

	ent->svflags &= ~SVF_NOCLIENT;
	PutClientInServer (ent);

	// add a teleportation effect
	if (!ent->client->pers.spectator)  {
		// send effect
		gi.WriteByte (svc_muzzleflash);
		gi.WriteShort (ent-g_edicts);
		gi.WriteByte (MZ_LOGIN);
		gi.multicast (ent->s.origin, MULTICAST_PVS);

		// hold in place briefly
		ent->client->ps.pmove.pm_flags = PMF_TIME_TELEPORT;
		ent->client->ps.pmove.pm_time = 14;
	}

	ent->client->respawn_time = level.time;

	if (ent->client->pers.spectator)
		gi.bprintf (PRINT_HIGH, "%s has moved to the sidelines\n", ent->client->pers.netname);
	else
		gi.bprintf (PRINT_HIGH, "%s joined the game\n", ent->client->pers.netname);
}


edict_t *PlayerTrail_LastSpot ()
{
	return trail[PREV(trail_head)];
}


void PlayerTrail_Add (vec3_t spot)
{
	vec3_t	temp;

	if (!trail_active)
		return;

	VectorCopy (spot, trail[trail_head]->s.origin);

	trail[trail_head]->timestamp = level.time;

	VectorSubtract (spot, trail[PREV(trail_head)]->s.origin, temp);
	trail[trail_head]->s.angles[1] = vectoyaw (temp);

	trail_head = NEXT(trail_head);
}


/*
==============
ClientBeginServerFrame

This will be called once for each server frame, before running
any other entities in the world.
==============
*/
void ClientBeginServerFrame (edict_t *ent)
{
	gclient_t	*client;
	int			buttonMask;

	if (level.intermissiontime)
		return;

	client = ent->client;

	if (deathmatch->value && client->pers.spectator != client->resp.spectator && (level.time - client->respawn_time) >= 5)
	{
		spectator_respawn(ent);
		return;
	}

	// run weapon animations if it hasn't been done by a ucmd_t
	if (!client->weapon_thunk && !client->resp.spectator)
		Think_Weapon (ent);
	else
		client->weapon_thunk = false;

	if (ent->deadflag)
	{
		// wait for any button just going down
		if ( level.time > client->respawn_time)
		{
			// in deathmatch, only wait for attack button
			if (deathmatch->value)
				buttonMask = BUTTON_ATTACK;
			else
				buttonMask = -1;

			if ( ( client->latched_buttons & buttonMask ) ||
				(deathmatch->value && ((int)dmflags->value & DF_FORCE_RESPAWN) ) )
			{
				respawn(ent);
				client->latched_buttons = 0;
					ent->flags &= ~FL_FLASHLIGHT;
////				client->flashlight = false;
			}
		}
		return;
	}

	// add player trail so monsters can follow
	if (!deathmatch->value)
		if (!visible (ent, PlayerTrail_LastSpot() ) )
			PlayerTrail_Add (ent->s.old_origin);

	client->latched_buttons = 0;
}


edict_t	*SV_TestEntityPosition (edict_t *ent)
{
	trace_t	trace;
	int		mask;

	if (ent->clipmask)
		mask = ent->clipmask;
	else
		mask = MASK_SOLID;
	trace = gi.trace (ent->s.origin, ent->mins, ent->maxs, ent->s.origin, ent, mask);

	if (trace.startsolid)
		return g_edicts;

	return NULL;
}


/*
============
SV_Push

Objects need to be moved back on a failed push,
otherwise riders would continue to slide.
============
*/
bool SV_Push (edict_t *pusher, vec3_t move, vec3_t amove)
{
	int			i, e;
	edict_t		*check, *block;
	vec3_t		mins, maxs;
	pushed_t	*p;
	vec3_t		org, org2, move2, forward, right, up;

	// clamp the move to 1/8 units, so the position will
	// be accurate for client side prediction
	for (i=0 ; i<3 ; i++)
	{
		float	temp;
		temp = move[i]*8.0;
		if (temp > 0.0)
			temp += 0.5;
		else
			temp -= 0.5;
		move[i] = 0.125 * (int)temp;
	}

	// find the bounding box
	for (i=0 ; i<3 ; i++)
	{
		mins[i] = pusher->absmin[i] + move[i];
		maxs[i] = pusher->absmax[i] + move[i];
	}

	// we need this for pushing things later
	VectorSubtract (vec3_origin, amove, org);
	AngleVectors (org, forward, right, up);

	// save the pusher's original position
	pushed_p->ent = pusher;
	VectorCopy (pusher->s.origin, pushed_p->origin);
	VectorCopy (pusher->s.angles, pushed_p->angles);
	if (pusher->client)
		pushed_p->deltayaw = pusher->client->ps.pmove.delta_angles[YAW];
	pushed_p++;

	// move the pusher to it's final position
	VectorAdd (pusher->s.origin, move, pusher->s.origin);
	VectorAdd (pusher->s.angles, amove, pusher->s.angles);
	gi.linkentity (pusher);

	// see if any solid entities are inside the final position
	check = g_edicts+1;
	for (e = 1; e < globals.num_edicts; e++, check++)
	{
		if (!check->inuse)
			continue;
		if (check->movetype == MOVETYPE_PUSH
		|| check->movetype == MOVETYPE_STOP
		|| check->movetype == MOVETYPE_NONE
		|| check->movetype == MOVETYPE_NOCLIP)
			continue;

		if (!check->area.prev)
			continue;		// not linked in anywhere

		// if the entity is standing on the pusher, it will definitely be moved
		if (check->groundentity != pusher)
		{
			// see if the ent needs to be tested
			if ( check->absmin[0] >= maxs[0]
			|| check->absmin[1] >= maxs[1]
			|| check->absmin[2] >= maxs[2]
			|| check->absmax[0] <= mins[0]
			|| check->absmax[1] <= mins[1]
			|| check->absmax[2] <= mins[2] )
				continue;

			// see if the ent's bbox is inside the pusher's final position
			if (!SV_TestEntityPosition (check))
				continue;
		}

		if ((pusher->movetype == MOVETYPE_PUSH) || (check->groundentity == pusher))
		{
			// move this entity
			pushed_p->ent = check;
			VectorCopy (check->s.origin, pushed_p->origin);
			VectorCopy (check->s.angles, pushed_p->angles);
			pushed_p++;

			// try moving the contacted entity
			VectorAdd (check->s.origin, move, check->s.origin);
			if (check->client)
			{	// FIXME: doesn't rotate monsters?
				check->client->ps.pmove.delta_angles[YAW] += amove[YAW];
			}

			// figure movement due to the pusher's amove
			VectorSubtract (check->s.origin, pusher->s.origin, org);
			org2[0] = DotProduct (org, forward);
			org2[1] = -DotProduct (org, right);
			org2[2] = DotProduct (org, up);
			VectorSubtract (org2, org, move2);
			VectorAdd (check->s.origin, move2, check->s.origin);

			// may have pushed them off an edge
			if (check->groundentity != pusher)
				check->groundentity = NULL;

			block = SV_TestEntityPosition (check);
			if (!block)
			{	// pushed ok
				gi.linkentity (check);
				// impact?
				continue;
			}

			// if it is ok to leave in the old position, do it
			// this is only relevent for riding entities, not pushed
			// FIXME: this doesn't acount for rotation
			VectorSubtract (check->s.origin, move, check->s.origin);
			block = SV_TestEntityPosition (check);
			if (!block)
			{
				pushed_p--;
				continue;
			}
		}

		// save off the obstacle so we can call the block function
		obstacle = check;

		// move back any entities we already moved
		// go backwards, so if the same entity was pushed
		// twice, it goes back to the original position
		for (p=pushed_p-1 ; p>=pushed ; p--)
		{
			VectorCopy (p->origin, p->ent->s.origin);
			VectorCopy (p->angles, p->ent->s.angles);
			if (p->ent->client)
			{
				p->ent->client->ps.pmove.delta_angles[YAW] = p->deltayaw;
			}
			gi.linkentity (p->ent);
		}
		return false;
	}

//FIXME: is there a better way to handle this?
	// see if anything we moved has touched a trigger
	for (p=pushed_p-1 ; p>=pushed ; p--)
		G_TouchTriggers (p->ent);

	return true;
}


/*
=============
SV_RunThink

Runs thinking code for this frame if necessary
=============
*/
bool SV_RunThink (edict_t *ent)
{
	float	thinktime;

	thinktime = ent->nextthink;
	if (thinktime <= 0)
		return true;
	if (thinktime > level.time+0.001)
		return true;

	ent->nextthink = 0;
	if (!ent->think)
		gi.error ("NULL ent->think");
	ent->think (ent);

	return false;
}


/*
================
SV_Physics_Pusher

Bmodel objects don't interact with each other, but
push all box objects
================
*/
void SV_Physics_Pusher (edict_t *ent)
{
	vec3_t		move, amove;
	edict_t		*part, *mv;

	// if not a team captain, so movement will be handled elsewhere
	if ( ent->flags & FL_TEAMSLAVE)
		return;

	// make sure all team slaves can move before commiting
	// any moves or calling any think functions
	// if the move is blocked, all moved objects will be backed out
	pushed_p = pushed;
	for (part = ent ; part ; part=part->teamchain)
	{
		if (part->velocity[0] || part->velocity[1] || part->velocity[2] ||
			part->avelocity[0] || part->avelocity[1] || part->avelocity[2]
			)
		{	// object is moving
			VectorScale (part->velocity, FRAMETIME, move);
			VectorScale (part->avelocity, FRAMETIME, amove);

			if (!SV_Push (part, move, amove))
				break;	// move was blocked
		}
	}
	if (pushed_p > &pushed[MAX_EDICTS])
		gi.error (/*ERR_FATAL,*/ "pushed_p > &pushed[MAX_EDICTS], memory corrupted");	// Quake2 DLL Bug

	if (part)
	{
		// the move failed, bump all nextthink times and back out moves
		for (mv = ent ; mv ; mv=mv->teamchain)
		{
			if (mv->nextthink > 0)
				mv->nextthink += FRAMETIME;
		}

		// if the pusher has a "blocked" function, call it
		// otherwise, just stay in place until the obstacle is gone
		if (part->blocked)
			part->blocked (part, obstacle);
	}
	else
	{
		// the move succeeded, so call all think functions
		for (part = ent ; part ; part=part->teamchain)
			SV_RunThink (part);
	}
}


/*
=============
SV_Physics_None

Non moving objects can only think
=============
*/
void SV_Physics_None (edict_t *ent)
{
// regular thinking
	SV_RunThink (ent);
}


/*
=============
SV_Physics_Noclip

A moving object that doesn't obey physics
=============
*/
void SV_Physics_Noclip (edict_t *ent)
{
// regular thinking
	if (!SV_RunThink (ent))
		return;

	VectorMA (ent->s.angles, FRAMETIME, ent->avelocity, ent->s.angles);
	VectorMA (ent->s.origin, FRAMETIME, ent->velocity, ent->s.origin);

	gi.linkentity (ent);
}


/*
=============
SV_Physics_Step

Monsters freefall when they don't have a ground entity, otherwise
all movement is done with discrete steps.

This is also used for objects that have become still on the ground, but
will fall if the floor is pulled out from under them.
FIXME: is this true?
=============
*/

//FIXME: hacked in for E3 demo
#define	sv_stopspeed		100
#define sv_friction			6
#define sv_waterfriction	1

void SV_AddRotationalFriction (edict_t *ent)
{
	int		n;
	float	adjustment;

	VectorMA (ent->s.angles, FRAMETIME, ent->avelocity, ent->s.angles);
	adjustment = FRAMETIME * sv_stopspeed * sv_friction;
	for (n = 0; n < 3; n++)
	{
		if (ent->avelocity[n] > 0)
		{
			ent->avelocity[n] -= adjustment;
			if (ent->avelocity[n] < 0)
				ent->avelocity[n] = 0;
		}
		else
		{
			ent->avelocity[n] += adjustment;
			if (ent->avelocity[n] > 0)
				ent->avelocity[n] = 0;
		}
	}
}


void SV_AddGravity (edict_t *ent)
{
	ent->velocity[2] -= ent->gravity * sv_gravity->value * FRAMETIME;
}


/*
=============
M_CheckBottom

Returns false if any part of the bottom of the entity is off an edge that
is not a staircase.
=============
*/
int c_yes, c_no;

bool M_CheckBottom (edict_t *ent)
{
	vec3_t	mins, maxs, start, stop;
	trace_t	trace;
	int		x, y;
	float	mid, bottom;

	VectorAdd (ent->s.origin, ent->mins, mins);
	VectorAdd (ent->s.origin, ent->maxs, maxs);

// if all of the points under the corners are solid world, don't bother
// with the tougher checks
// the corners must be within 16 of the midpoint
	start[2] = mins[2] - 1;
	for	(x=0 ; x<=1 ; x++)
		for	(y=0 ; y<=1 ; y++)
		{
			start[0] = x ? maxs[0] : mins[0];
			start[1] = y ? maxs[1] : mins[1];
			int cont = gi.pointcontents (start);
			if ((cont != CONTENTS_SOLID) || (cont != CONTENTS_AUX))
				goto realcheck;
		}

	c_yes++;
	return true;		// we got out easy

realcheck:
	c_no++;
//
// check it for real...
//
	start[2] = mins[2];

// the midpoint must be within 16 of the bottom
	start[0] = stop[0] = (mins[0] + maxs[0])*0.5;
	start[1] = stop[1] = (mins[1] + maxs[1])*0.5;
	stop[2] = start[2] - 2*STEPSIZE;
	trace = gi.trace (start, vec3_origin, vec3_origin, stop, ent, MASK_MONSTERSOLID);

	if (trace.fraction == 1.0)
		return false;
	mid = bottom = trace.endpos[2];

// the corners must be within 16 of the midpoint
	for	(x=0 ; x<=1 ; x++)
		for	(y=0 ; y<=1 ; y++)
		{
			start[0] = stop[0] = x ? maxs[0] : mins[0];
			start[1] = stop[1] = y ? maxs[1] : mins[1];

			trace = gi.trace (start, vec3_origin, vec3_origin, stop, ent, MASK_MONSTERSOLID);

			if (trace.fraction != 1.0 && trace.endpos[2] > bottom)
				bottom = trace.endpos[2];
			if (trace.fraction == 1.0 || mid - trace.endpos[2] > STEPSIZE)
				return false;
		}

	c_yes++;
	return true;
}


/*
==================
SV_Impact

Two entities have touched, so run their touch functions
==================
*/
void SV_Impact (edict_t *e1, trace_t *trace)
{
	edict_t		*e2;
	e2 = trace->ent;

	if (e1->touch && e1->solid != SOLID_NOT)
		e1->touch (e1, e2, &trace->plane, trace->surface);

	if (e2->touch && e2->solid != SOLID_NOT)
		e2->touch (e2, e1, NULL, NULL);
}


/*
==================
ClipVelocity

Slide off of the impacting object
returns the blocked flags (1 = floor, 2 = step / wall)
==================
*/
#define	STOP_EPSILON	0.1

int ClipVelocity (vec3_t in, vec3_t normal, vec3_t out, float overbounce)
{
	float	backoff;
	float	change;
	int		i, blocked;

	blocked = 0;
	if (normal[2] > 0)
		blocked |= 1;		// floor
	if (!normal[2])
		blocked |= 2;		// step

	backoff = DotProduct (in, normal) * overbounce;

	for (i=0 ; i<3 ; i++)
	{
		change = normal[i]*backoff;
		out[i] = in[i] - change;
		if (out[i] > -STOP_EPSILON && out[i] < STOP_EPSILON)
			out[i] = 0;
	}

	return blocked;
}


/*
============
SV_FlyMove

The basic solid body movement clip that slides along multiple planes
Returns the clipflags if the velocity was modified (hit something solid)
1 = floor
2 = wall / step
4 = dead stop
============
*/
#define	MAX_CLIP_PLANES	5
int SV_FlyMove (edict_t *ent, float time, int mask)
{
	edict_t		*hit;
	int			bumpcount, numbumps;
	vec3_t		dir;
	float		d;
	int			numplanes;
	vec3_t		planes[MAX_CLIP_PLANES];
	vec3_t		primal_velocity, original_velocity, new_velocity;
	int			i, j;
	trace_t		trace;
	vec3_t		end;
	float		time_left;
	int			blocked;

	numbumps = 4;

	blocked = 0;
	VectorCopy (ent->velocity, original_velocity);
	VectorCopy (ent->velocity, primal_velocity);
	numplanes = 0;

	time_left = time;

	ent->groundentity = NULL;
	for (bumpcount=0 ; bumpcount<numbumps ; bumpcount++)
	{
		for (i=0 ; i<3 ; i++)
			end[i] = ent->s.origin[i] + time_left * ent->velocity[i];

		trace = gi.trace (ent->s.origin, ent->mins, ent->maxs, end, ent, mask);

		if (trace.allsolid)
		{	// entity is trapped in another solid
			VectorClear (ent->velocity);
			return 3;
		}

		if (trace.fraction > 0)
		{	// actually covered some distance
			VectorCopy (trace.endpos, ent->s.origin);
			VectorCopy (ent->velocity, original_velocity);
			numplanes = 0;
		}

		if (trace.fraction == 1)
			 break;		// moved the entire distance

		hit = trace.ent;

		if (trace.plane.normal[2] > 0.7)
		{
			blocked |= 1;		// floor
			if ( hit->solid == SOLID_BSP)
			{
				ent->groundentity = hit;
				ent->groundentity_linkcount = hit->linkcount;
			}
		}
		if (!trace.plane.normal[2])
		{
			blocked |= 2;		// step
		}

//
// run the impact function
//
		SV_Impact (ent, &trace);
		if (!ent->inuse)
			break;		// removed by the impact function


		time_left -= time_left * trace.fraction;

		// cliped to another plane
		if (numplanes >= MAX_CLIP_PLANES)
		{	// this shouldn't really happen
			VectorClear (ent->velocity);
			return 3;
		}

		VectorCopy (trace.plane.normal, planes[numplanes]);
		numplanes++;

//
// modify original_velocity so it parallels all of the clip planes
//
		for (i=0 ; i<numplanes ; i++)
		{
			ClipVelocity (original_velocity, planes[i], new_velocity, 1);

			for (j=0 ; j<numplanes ; j++)
				if ((j != i) && !VectorCompare (planes[i], planes[j]))
				{
					if (DotProduct (new_velocity, planes[j]) < 0)
						break;	// not ok
				}
			if (j == numplanes)
				break;
		}

		if (i != numplanes)
		{	// go along this plane
			VectorCopy (new_velocity, ent->velocity);
		}
		else
		{	// go along the crease
			if (numplanes != 2)
			{
				VectorClear (ent->velocity);
				return 7;
			}
			CrossProduct (planes[0], planes[1], dir);
			d = DotProduct (dir, ent->velocity);
			VectorScale (dir, d, ent->velocity);
		}

//
// if original velocity is against the original velocity, stop dead
// to avoid tiny occilations in sloping corners
//
		if (DotProduct (ent->velocity, primal_velocity) <= 0)
		{
			VectorClear (ent->velocity);
			return blocked;
		}
	}

	return blocked;
}


void SV_Physics_Step (edict_t *ent)
{
	bool		wasonground;
	bool		hitsound = false;
	float		*vel;
	float		speed, newspeed, control;
	float		friction;
	edict_t		*groundentity;
	int			mask;

	// airborn monsters should always check for ground
	if (!ent->groundentity)
		M_CheckGround (ent);

	groundentity = ent->groundentity;

	SV_CheckVelocity (ent);

	if (groundentity)
		wasonground = true;
	else
		wasonground = false;

	if (ent->avelocity[0] || ent->avelocity[1] || ent->avelocity[2])
		SV_AddRotationalFriction (ent);

	// add gravity except:
	//   flying monsters
	//   swimming monsters who are in the water
	if (! wasonground)
		if (!(ent->flags & FL_FLY))
			if (!((ent->flags & FL_SWIM) && (ent->waterlevel > 2)))
			{
				if (ent->velocity[2] < sv_gravity->value*-0.1)
					hitsound = true;
				if (ent->waterlevel == 0)
					SV_AddGravity (ent);
			}

	// friction for flying monsters that have been given vertical velocity
	if ((ent->flags & FL_FLY) && (ent->velocity[2] != 0))
	{
		speed = fabs(ent->velocity[2]);
		control = speed < sv_stopspeed ? sv_stopspeed : speed;
		friction = sv_friction/3;
		newspeed = speed - (FRAMETIME * control * friction);
		if (newspeed < 0)
			newspeed = 0;
		newspeed /= speed;
		ent->velocity[2] *= newspeed;
	}

	// friction for flying monsters that have been given vertical velocity
	if ((ent->flags & FL_SWIM) && (ent->velocity[2] != 0))
	{
		speed = fabs(ent->velocity[2]);
		control = speed < sv_stopspeed ? sv_stopspeed : speed;
		newspeed = speed - (FRAMETIME * control * sv_waterfriction * ent->waterlevel);
		if (newspeed < 0)
			newspeed = 0;
		newspeed /= speed;
		ent->velocity[2] *= newspeed;
	}

	if (ent->velocity[2] || ent->velocity[1] || ent->velocity[0])
	{
		// apply friction
		// let dead monsters who aren't completely onground slide
		if ((wasonground) || (ent->flags & (FL_SWIM|FL_FLY)))
			if (!(ent->health <= 0.0 && !M_CheckBottom(ent)))
			{
				vel = ent->velocity;
				speed = sqrt(vel[0]*vel[0] +vel[1]*vel[1]);
				if (speed)
				{
					friction = sv_friction;

					control = speed < sv_stopspeed ? sv_stopspeed : speed;
					newspeed = speed - FRAMETIME*control*friction;

					if (newspeed < 0)
						newspeed = 0;
					newspeed /= speed;

					vel[0] *= newspeed;
					vel[1] *= newspeed;
				}
			}

		if (ent->svflags & SVF_MONSTER)
			mask = MASK_MONSTERSOLID;
		else
			mask = MASK_SOLID;
		SV_FlyMove (ent, FRAMETIME, mask);

		gi.linkentity (ent);
		G_TouchTriggers (ent);
		if (!ent->inuse)
			return;

		if (ent->groundentity)
			if (!wasonground)
				if (hitsound)
					gi.sound (ent, 0, gi.soundindex("world/land.wav"), 1, 1, 0);
	}

// regular thinking
	SV_RunThink (ent);
}


/*QUAKED misc_banner (1 .5 0) (-4 -4 -4) (4 4 4)
The origin is the bottom of the banner.
The banner is 128 tall.
*/
void misc_banner_think (edict_t *ent)
{
	ent->s.frame = (ent->s.frame + 1) % 16;
	ent->nextthink = level.time + FRAMETIME;
}


void SP_misc_banner (edict_t *ent)
{
	ent->movetype = MOVETYPE_NONE;
	ent->solid = SOLID_NOT;
	ent->s.modelindex = gi.modelindex ("models/objects/banner/tris.md2");
	ent->s.frame = rand() % 16;
	gi.linkentity (ent);

	ent->think = misc_banner_think;
	ent->nextthink = level.time + FRAMETIME;
}


void ThrowHead (edict_t *self, char *gibname, int damage, int type)
{
	vec3_t	vd;
	float	vscale;

	self->s.skinnum = 0;
	self->s.frame = 0;
	VectorClear (self->mins);
	VectorClear (self->maxs);

	self->s.modelindex2 = 0;
	gi.setmodel (self, gibname);
	self->solid = SOLID_NOT;
	self->s.effects |= EF_GIB;
	self->s.effects &= ~EF_FLIES;
	self->s.sound = 0;
	self->flags |= FL_NO_KNOCKBACK;
	self->svflags &= ~SVF_MONSTER;
	self->takedamage = DAMAGE_YES;
	self->die = gib_die;

	if (type == GIB_ORGANIC)
	{
		self->movetype = MOVETYPE_TOSS;
		self->touch = gib_touch;
		vscale = 0.5;
	}
	else
	{
		self->movetype = MOVETYPE_BOUNCE;
		vscale = 1.0;
	}

	VelocityForDamage (damage, vd);
	VectorMA (self->velocity, vscale, vd, self->velocity);
	ClipGibVelocity (self);

	self->avelocity[YAW] = crandom()*600;

	if(sv_gibs->value >= 1)
	{
		self->think = G_FreeEdict;
		self->nextthink = level.time + (1 + random()) * sv_gibs->value;
	}
	else
	{
		self->think = G_FreeEdictExt;
		self->nextthink = level.time + 1 + random();
	}

	gi.linkentity (self);
}


/*
=============
SV_movestep

Called by monster program code.
The move will be adjusted for slopes and stairs, but if the move isn't
possible, no move is done, false is returned, and
pr_global_struct->trace_normal is set to the normal of the blocking wall
=============
*/
//FIXME since we need to test end position contents here, can we avoid doing
//it again later in catagorize position?
bool SV_movestep (edict_t *ent, vec3_t move, bool relink)
{
	float		dz;
	vec3_t		oldorg, neworg, end;
	trace_t		trace;
	int			i;
	float		stepsize;
	vec3_t		test;
	int			contents;

// try the move
	VectorCopy (ent->s.origin, oldorg);
	VectorAdd (ent->s.origin, move, neworg);

// flying monsters don't step up
	if ( ent->flags & (FL_SWIM | FL_FLY) )
	{
	// try one move with vertical motion, then one without
		for (i=0 ; i<2 ; i++)
		{
			VectorAdd (ent->s.origin, move, neworg);
			if (i == 0 && ent->enemy)
			{
				if (!ent->goalentity)
					ent->goalentity = ent->enemy;
				dz = ent->s.origin[2] - ent->goalentity->s.origin[2];
				if (ent->goalentity->client)
				{
					if (dz > 40)
						neworg[2] -= 8;
					if (!((ent->flags & FL_SWIM) && (ent->waterlevel < 2)))
						if (dz < 30)
							neworg[2] += 8;
				}
				else
				{
					if (dz > 8)
						neworg[2] -= 8;
					else if (dz > 0)
						neworg[2] -= dz;
					else if (dz < -8)
						neworg[2] += 8;
					else
						neworg[2] += dz;
				}
			}
			trace = gi.trace (ent->s.origin, ent->mins, ent->maxs, neworg, ent, MASK_MONSTERSOLID);

			// fly monsters don't enter water voluntarily
			if (ent->flags & FL_FLY)
			{
				if (!ent->waterlevel)
				{
					test[0] = trace.endpos[0];
					test[1] = trace.endpos[1];
					test[2] = trace.endpos[2] + ent->mins[2] + 1;
					contents = gi.pointcontents(test);
					if (contents & MASK_WATER)
						return false;
				}
			}

			// swim monsters don't exit water voluntarily
			if (ent->flags & FL_SWIM)
			{
				if (ent->waterlevel < 2)
				{
					test[0] = trace.endpos[0];
					test[1] = trace.endpos[1];
					test[2] = trace.endpos[2] + ent->mins[2] + 1;
					contents = gi.pointcontents(test);
					if (!(contents & MASK_WATER))
						return false;
				}
			}

			if (trace.fraction == 1)
			{
				VectorCopy (trace.endpos, ent->s.origin);
				if (relink)
				{
					gi.linkentity (ent);
					G_TouchTriggers (ent);
				}
				return true;
			}

			if (!ent->enemy)
				break;
		}

		return false;
	}

// push down from a step height above the wished position
	if (!(ent->monsterinfo.aiflags & AI_NOSTEP))
		stepsize = STEPSIZE;
	else
		stepsize = 1;

	neworg[2] += stepsize;
	VectorCopy (neworg, end);
	end[2] -= stepsize*2;

	trace = gi.trace (neworg, ent->mins, ent->maxs, end, ent, MASK_MONSTERSOLID);

	if (trace.allsolid)
		return false;

	if (trace.startsolid)
	{
		neworg[2] -= stepsize;
		trace = gi.trace (neworg, ent->mins, ent->maxs, end, ent, MASK_MONSTERSOLID);
		if (trace.allsolid || trace.startsolid)
			return false;
	}


	// don't go in to water
	if (ent->waterlevel == 0)
	{
		test[0] = trace.endpos[0];
		test[1] = trace.endpos[1];
		test[2] = trace.endpos[2] + ent->mins[2] + 1;
		contents = gi.pointcontents(test);

		if (contents & MASK_WATER)
			return false;
	}

	if (trace.fraction == 1)
	{
	// if monster had the ground pulled out, go ahead and fall
		if ( ent->flags & FL_PARTIALGROUND )
		{
			VectorAdd (ent->s.origin, move, ent->s.origin);
			if (relink)
			{
				gi.linkentity (ent);
				G_TouchTriggers (ent);
			}
			ent->groundentity = NULL;
			return true;
		}

		return false;		// walked off an edge
	}

// check point traces down for dangling corners
	VectorCopy (trace.endpos, ent->s.origin);

	if (!M_CheckBottom (ent))
	{
		if ( ent->flags & FL_PARTIALGROUND )
		{	// entity had floor mostly pulled out from underneath it
			// and is trying to correct
			if (relink)
			{
				gi.linkentity (ent);
				G_TouchTriggers (ent);
			}
			return true;
		}
		VectorCopy (oldorg, ent->s.origin);
		return false;
	}

	if ( ent->flags & FL_PARTIALGROUND )
	{
		ent->flags &= ~FL_PARTIALGROUND;
	}
	ent->groundentity = trace.ent;
	ent->groundentity_linkcount = trace.ent->linkcount;

// the move is ok
	if (relink)
	{
		gi.linkentity (ent);
		G_TouchTriggers (ent);
	}
	return true;
}


void M_CatagorizePosition (edict_t *ent)
{
	vec3_t		point;
	int			cont;

	// get waterlevel
	point[0] = ent->s.origin[0];
	point[1] = ent->s.origin[1];
	point[2] = ent->s.origin[2] + ent->mins[2] + 1;
	cont = gi.pointcontents (point);

	if (!(cont & MASK_WATER))
	{
		ent->waterlevel = 0;
		ent->watertype = 0;
		return;
	}

	ent->watertype = cont;
	ent->waterlevel = 1;
	point[2] += 26;
	cont = gi.pointcontents (point);
	if (!(cont & MASK_WATER))
		return;

	ent->waterlevel = 2;
	point[2] += 22;
	cont = gi.pointcontents (point);
	if (cont & MASK_WATER)
		ent->waterlevel = 3;
}


void M_droptofloor (edict_t *ent)
{
	vec3_t		end;
	trace_t		trace;

	ent->s.origin[2] += 1;
	VectorCopy (ent->s.origin, end);
	end[2] -= 256;

	trace = gi.trace (ent->s.origin, ent->mins, ent->maxs, end, ent, MASK_MONSTERSOLID);

	if (trace.fraction == 1 || trace.allsolid)
		return;

	VectorCopy (trace.endpos, ent->s.origin);

	gi.linkentity (ent);
	M_CheckGround (ent);
	M_CatagorizePosition (ent);
}


/*QUAKED misc_strogg_ship (1 .5 0) (-16 -16 0) (16 16 32)
This is a Storgg ship for the flybys.
It is trigger_spawned, so you must have something use it for it to show up.
There must be a path for it to follow once it is activated.

"speed"		How fast it should fly
*/

void misc_strogg_ship_use  (edict_t *self, edict_t *other, edict_t *activator)
{
	self->svflags &= ~SVF_NOCLIENT;
	self->use = train_use;
	train_use (self, other, activator);
}

void SP_misc_strogg_ship (edict_t *ent)
{
	if (!ent->target)
	{
		gi.dprintf ("%s without a target at %s\n", ent->classname, vtos(ent->absmin));
		G_FreeEdict (ent);
		return;
	}

	if (!ent->speed)
		ent->speed = 300;

	ent->movetype = MOVETYPE_PUSH;
	ent->solid = SOLID_NOT;
	ent->s.modelindex = gi.modelindex ("models/ships/strogg1/tris.md2");
	VectorSet (ent->mins, -16, -16, 0);
	VectorSet (ent->maxs, 16, 16, 32);

	ent->think = func_train_find;
	ent->nextthink = level.time + FRAMETIME;
	ent->use = misc_strogg_ship_use;
	ent->svflags |= SVF_NOCLIENT;
	ent->moveinfo.accel = ent->moveinfo.decel = ent->moveinfo.speed = ent->speed;

	gi.linkentity (ent);
}


void NetThrowGib (edict_t *self, unsigned type)
{
	bool large_map = LargeMapCheck(self->s.origin);
	if (large_map)
		gi.WriteByte (svc_temp_entity_ext);
	else
		gi.WriteByte (svc_temp_entity);
	gi.WriteByte (TE_GIBS_ARM + type);
	gi.WritePosition (self->s.origin, large_map);
	WriteScaledDir (self->velocity);
	gi.multicast (self->s.origin, MULTICAST_PVS);
	G_FreeEdict(self);
}

/*QUAKED misc_gib_arm (1 0 0) (-8 -8 -8) (8 8 8)
Intended for use with the target_spawner
*/
void SP_misc_gib_arm (edict_t *ent)
{
	if(net_compatibility->value || !net_fixoverflow->value)
	{
		gi.setmodel (ent, "models/objects/gibs/arm/tris.md2");
		ent->solid = SOLID_NOT;
		ent->s.effects |= EF_GIB;
		ent->takedamage = DAMAGE_YES;
		ent->die = gib_die;
		ent->movetype = MOVETYPE_TOSS;
		ent->svflags |= SVF_MONSTER;
		ent->deadflag = DEAD_DEAD;
		ent->avelocity[0] = random()*200;
		ent->avelocity[1] = random()*200;
		ent->avelocity[2] = random()*200;
		ent->think = G_FreeEdict;
		ent->nextthink = level.time + 10;	///30;		уменьшил время жизни джибсов чтоб не перегружать сеть
		gi.linkentity (ent);
	}
	else
		NetThrowGib(ent, GIBS_ARM);
}

/*QUAKED misc_gib_leg (1 0 0) (-8 -8 -8) (8 8 8)
Intended for use with the target_spawner
*/
void SP_misc_gib_leg (edict_t *ent)
{
	if(net_compatibility->value || !net_fixoverflow->value)
	{
		gi.setmodel (ent, "models/objects/gibs/leg/tris.md2");
		ent->solid = SOLID_NOT;
		ent->s.effects |= EF_GIB;
		ent->takedamage = DAMAGE_YES;
		ent->die = gib_die;
		ent->movetype = MOVETYPE_TOSS;
		ent->svflags |= SVF_MONSTER;
		ent->deadflag = DEAD_DEAD;
		ent->avelocity[0] = random()*200;
		ent->avelocity[1] = random()*200;
		ent->avelocity[2] = random()*200;
		ent->think = G_FreeEdict;
		ent->nextthink = level.time + 10;	///30;		уменьшил время жизни джибсов чтоб не перегружать сеть
		gi.linkentity (ent);
	}
	else
		NetThrowGib(ent, GIBS_LEG);
}

/*QUAKED misc_gib_head (1 0 0) (-8 -8 -8) (8 8 8)
Intended for use with the target_spawner
*/
void SP_misc_gib_head (edict_t *ent)
{
	if(net_compatibility->value || !net_fixoverflow->value)
	{
		gi.setmodel (ent, "models/objects/gibs/head/tris.md2");
		ent->solid = SOLID_NOT;
		ent->s.effects |= EF_GIB;
		ent->takedamage = DAMAGE_YES;
		ent->die = gib_die;
		ent->movetype = MOVETYPE_TOSS;
		ent->svflags |= SVF_MONSTER;
		ent->deadflag = DEAD_DEAD;
		ent->avelocity[0] = random()*200;
		ent->avelocity[1] = random()*200;
		ent->avelocity[2] = random()*200;
		ent->think = G_FreeEdict;
		ent->nextthink = level.time + 10;	///30;		уменьшил время жизни джибсов чтоб не перегружать сеть
		gi.linkentity (ent);
	}
	else
		NetThrowGib(ent, GIBS_HEAD);
}


/*QUAKED misc_explobox (0 .5 .8) (-16 -16 0) (16 16 40)
Large exploding box.  You can override its mass (100),
health (80), and dmg (150).
*/
void SP_misc_explobox (edict_t *self)
{
	if (self->dmg!=-1)
		if (deathmatch->value)
		{	// auto-remove for deathmatch
			G_FreeEdict (self);
			return;
		}

	gi.modelindex ("models/objects/debris1/tris.md2");
	gi.modelindex ("models/objects/debris2/tris.md2");
	gi.modelindex ("models/objects/debris3/tris.md2");

	self->solid = SOLID_BBOX;
	self->movetype = MOVETYPE_STEP;

	self->model = "models/objects/barrels/tris.md2";
	self->s.modelindex = gi.modelindex (self->model);
	VectorSet (self->mins, -16, -16, 0);
	VectorSet (self->maxs, 16, 16, 40);

	if (!self->mass)
		self->mass = 400;
	if (!self->health)
		self->health = 10;
	if (!self->dmg)
		self->dmg = 150;

	if (self->dmg==-1)
		self->takedamage = DAMAGE_NO;
	else
		self->takedamage = DAMAGE_YES;
	self->die = barrel_delay;
	self->monsterinfo.aiflags = AI_NOSTEP;

	self->touch = barrel_touch;

	self->think = M_droptofloor;
	self->nextthink = level.time + 2 * FRAMETIME;

	gi.linkentity (self);
}


/*
============
SV_PushEntity

Does not change the entities velocity at all
============
*/
trace_t SV_PushEntity (edict_t *ent, vec3_t push)
{
	trace_t	trace;
	vec3_t	start;
	vec3_t	end;
	int		mask;

	VectorCopy (ent->s.origin, start);
	VectorAdd (start, push, end);

retry:
	if (ent->clipmask)
		mask = ent->clipmask;
	else
		mask = MASK_SOLID;

	trace = gi.trace (start, ent->mins, ent->maxs, end, ent, mask);

	VectorCopy (trace.endpos, ent->s.origin);
	gi.linkentity (ent);

	if (trace.fraction != 1.0)
	{
		SV_Impact (ent, &trace);

		// if the pushed entity went away and the pusher is still there
		if (!trace.ent->inuse && ent->inuse)
		{
			// move the pusher back and try again
			VectorCopy (start, ent->s.origin);
			gi.linkentity (ent);
			goto retry;
		}
	}

	if (ent->inuse)
		G_TouchTriggers (ent);

	return trace;
}


void Use_Silencer (edict_t *ent, gitem_t *item)
{
	ent->client->pers.inventory[ITEM_INDEX(item)]--;
	ValidateSelectedItem (ent);
	ent->client->silencer_shots += 30;
}


/*
=============
SV_Physics_Toss

Toss, bounce, and fly movement.  When onground, do nothing.
=============
*/
void SV_Physics_Toss (edict_t *ent)
{
	trace_t		trace;
	vec3_t		move;
	float		backoff;
	edict_t		*slave;
	bool		wasinwater;
	bool		isinwater;
	vec3_t		old_origin;

// regular thinking
	SV_RunThink (ent);

	// if not a team captain, so movement will be handled elsewhere
	if (ent->flags & FL_TEAMSLAVE)
		return;

	if (ent->velocity[2] > 0)
		ent->groundentity = NULL;

// check for the groundentity going away
	if (ent->groundentity)
		if (!ent->groundentity->inuse)
			ent->groundentity = NULL;

// if onground, return without moving
	if ( ent->groundentity )
	{
//// Berserker:
//// Fixing bug: во время сетевой игры если игрок умер и его труп / череп продолжают лететь,
//// а игрок в это время респавнится - то возможна ситуация, что труп / череп зависают в воздухе...
		if (ent->movetype == MOVETYPE_TOSS_DM || ent->movetype == MOVETYPE_BOUNCE_DM)
		{
			/// Berserker: проверим ка, а точно ли мы на земле???
			unsigned mask = 0;
			trace_t	trace;
			vec3_t	start, end;
			// прощупаем, пересекает bounding box игрока CONTENTS_SOLID (можно конечно и MASK_SHOT, но для простоты - не будем...)
			// (тест простейший, но более сложный делать imho не стоит...)

			if (ent->movetype == MOVETYPE_TOSS_DM)
			{
				start[2] = ent->s.origin[2] + 32;
				end[2] = ent->s.origin[2] - 24 - 32;	/// щупаем ниже на 32 пиксела (body)
			}
			else
			{
				start[2] = ent->s.origin[2] + 16;
				end[2] = ent->s.origin[2] - 16 - 24;	/// щупаем ниже на 24 пиксела (head)
			}

			end[0] = start[0] = ent->s.origin[0] - 16;
			end[1] = start[1] = ent->s.origin[1] - 16;
			trace = gi.trace (ent->s.origin, NULL, NULL, end, NULL, CONTENTS_SOLID);
			if (trace.fraction < 1)
				mask |= 1;

			end[0] = start[0] = ent->s.origin[0] + 16;
			trace = gi.trace (ent->s.origin, NULL, NULL, end, NULL, CONTENTS_SOLID);
			if (trace.fraction < 1)
				mask |= 2;

			end[1] = start[1] = ent->s.origin[1] + 16;
			trace = gi.trace (ent->s.origin, NULL, NULL, end, NULL, CONTENTS_SOLID);
			if (trace.fraction < 1)
				mask |= 4;

			end[0] = start[0] = ent->s.origin[0] - 16;
			trace = gi.trace (ent->s.origin, NULL, NULL, end, NULL, CONTENTS_SOLID);
			if (trace.fraction < 1)
				mask |= 8;

			if (mask == 0)
			{
				gi.dprintf("fixed bug: groundentity != NULL, but edict not on ground\n");
				ent->groundentity = NULL;	// сбросим признак groundentity,
				goto noret;
			}

			if(ent->movetype == MOVETYPE_TOSS_DM)	// а иначе сбросим признак движения
				ent->movetype = MOVETYPE_TOSS;		// в нормальные соотв. виды...
			else
				ent->movetype = MOVETYPE_BOUNCE;
		}
		return;										// и выйдем. (падение трупа остановлено)
	}
noret:

	VectorCopy (ent->s.origin, old_origin);

	SV_CheckVelocity (ent);

// add gravity
	if (ent->movetype != MOVETYPE_FLY
	&& ent->movetype != MOVETYPE_FLYMISSILE)
		SV_AddGravity (ent);

// move angles
	VectorMA (ent->s.angles, FRAMETIME, ent->avelocity, ent->s.angles);

// move origin
	VectorScale (ent->velocity, FRAMETIME, move);
	trace = SV_PushEntity (ent, move);
	if (!ent->inuse)
		return;

	if (trace.fraction < 1)
	{
		if (ent->movetype == MOVETYPE_BOUNCE)
			backoff = 1.5;
		else
			backoff = 1;

		ClipVelocity (ent->velocity, trace.plane.normal, ent->velocity, backoff);

	// stop if on ground
		if (trace.plane.normal[2] > 0.7)
		{
			if (ent->velocity[2] < 60 || ent->movetype != MOVETYPE_BOUNCE )
			{
				ent->groundentity = trace.ent;
				ent->groundentity_linkcount = trace.ent->linkcount;
				VectorClear (ent->velocity);
				VectorClear (ent->avelocity);
			}
		}
	}

// check for water transition
	wasinwater = (ent->watertype & MASK_WATER);
	ent->watertype = gi.pointcontents (ent->s.origin);
	isinwater = ent->watertype & MASK_WATER;

	if (isinwater)
		ent->waterlevel = 1;
	else
		ent->waterlevel = 0;

	if (!wasinwater && isinwater)
		gi.positioned_sound (old_origin, g_edicts, CHAN_AUTO, gi.soundindex("misc/h2ohit1.wav"), 1, 1, 0);
	else if (wasinwater && !isinwater)
		gi.positioned_sound (ent->s.origin, g_edicts, CHAN_AUTO, gi.soundindex("misc/h2ohit1.wav"), 1, 1, 0);

// move teamslaves
	for (slave = ent->teamchain; slave; slave = slave->teamchain)
	{
		VectorCopy (ent->s.origin, slave->s.origin);
		gi.linkentity (slave);
	}
}


void G_RunEntity (edict_t *ent)
{
	if (ent->prethink)
		ent->prethink (ent);

	switch ( (int)ent->movetype)
	{
	case MOVETYPE_PUSH:
	case MOVETYPE_STOP:
		SV_Physics_Pusher (ent);
		break;
	case MOVETYPE_NONE:
		SV_Physics_None (ent);
		break;
	case MOVETYPE_NOCLIP:
		SV_Physics_Noclip (ent);
		break;
	case MOVETYPE_STEP:
		SV_Physics_Step (ent);
		break;
	case MOVETYPE_TOSS:
	case MOVETYPE_TOSS_DM:
	case MOVETYPE_BOUNCE:
	case MOVETYPE_BOUNCE_DM:
	case MOVETYPE_FLY:
	case MOVETYPE_FLYMISSILE:
		SV_Physics_Toss (ent);
		break;
	default:
		gi.error ("SV_Physics: bad movetype %i", (int)ent->movetype);
	}
}


/*
=================
EndDMLevel

The timelimit or fraglimit has been exceeded
=================
*/
void EndDMLevel ()
{
	edict_t		*ent;
	char *s, *t, *f;
	static const char *seps = " ,\n\r";

	// stay on same level flag
	if ((int)dmflags->value & DF_SAME_LEVEL)
	{
		BeginIntermission (CreateTargetChangeLevel (level.mapname) );
		return;
	}

	// see if it's in the map list
	if (*sv_maplist->string) {
		s = strdup(sv_maplist->string);
		f = NULL;
		t = strtok(s, seps);
		while (t != NULL) {
			if (Q_strcasecmp(t, level.mapname) == 0) {
				// it's in the list, go to the next one
				t = strtok(NULL, seps);
				if (t == NULL) { // end of list, go to first one
					if (f == NULL) // there isn't a first one, same level
						BeginIntermission (CreateTargetChangeLevel (level.mapname) );
					else
						BeginIntermission (CreateTargetChangeLevel (f) );
				} else
					BeginIntermission (CreateTargetChangeLevel (t) );
				free(s);
				return;
			}
			if (!f)
				f = t;
			t = strtok(NULL, seps);
		}
		free(s);
	}

	if (level.nextmap[0]) // go to a specific map
		BeginIntermission (CreateTargetChangeLevel (level.nextmap) );
	else {	// search for a changelevel
		ent = G_Find (NULL, FOFS(classname), "target_changelevel", NULL);
		if (!ent)
		{	// the map designer didn't include a changelevel,
			// so create a fake ent that goes back to the same level
			BeginIntermission (CreateTargetChangeLevel (level.mapname) );
			return;
		}
		BeginIntermission (ent);
	}
}


void CheckDMRules ()
{
	int			i;
	gclient_t	*cl;

	if (level.intermissiontime)
		return;

	if (!deathmatch->value)
		return;

	if (timelimit->value)
	{
		if (level.time >= timelimit->value*60)
		{
			gi.bprintf (PRINT_HIGH, "Timelimit hit.\n");
			EndDMLevel ();
			return;
		}
	}

	if (fraglimit->value)
	{
		for (i=0 ; i<maxclients_cvar->value ; i++)
		{
			cl = game.clients + i;
			if (!g_edicts[i+1].inuse)
				continue;

			if (cl->resp.score >= fraglimit->value)
			{
				gi.bprintf (PRINT_HIGH, "Fraglimit hit.\n");
				EndDMLevel ();
				return;
			}
		}
	}
}


void CheckNeedPass ()
{
	int need;

	// if password or spectator_password has changed, update needpass
	// as needed
	if (password->modified || spectator_password->modified)
	{
		password->modified = spectator_password->modified = false;

		need = 0;

		if (*password->string && Q_strcasecmp(password->string, "none"))
			need |= 1;
		if (*spectator_password->string && Q_strcasecmp(spectator_password->string, "none"))
			need |= 2;

		gi.cvar_set("needpass", va("%d", need));
	}
}


/*
================
G_RunFrame

Advances the world by 0.1 seconds
================
*/
void G_RunFrame (float stopclock)
{
	int		i;
	edict_t	*ent;

	if (stopclock==0)
		level.framenum++;

	level.time = level.framenum*FRAMETIME;

	// choose a client for monsters to target this frame
	AI_SetSightClient ();

	// exit intermissions

	if (level.exitintermission)
	{
		ExitLevel ();
		return;
	}

	//
	// treat each object in turn
	// even the world gets a chance to think
	//
	ent = &g_edicts[0];
	for (i=0 ; i<globals.num_edicts ; i++, ent++)
	{
		if (!ent->inuse)
			continue;

		level.current_entity = ent;

		VectorCopy (ent->s.origin, ent->s.old_origin);

		// if the ground entity moved, make sure we are still on it
		if ((ent->groundentity) && (ent->groundentity->linkcount != ent->groundentity_linkcount))
		{
			ent->groundentity = NULL;
			if ( !(ent->flags & (FL_SWIM|FL_FLY)) && (ent->svflags & SVF_MONSTER) )
				M_CheckGround (ent);
		}

		if (i > 0 && i <= maxclients_cvar->value)
		{
			ClientBeginServerFrame (ent);
			continue;
		}

		G_RunEntity (ent);
	}

	// see if it is time to end a deathmatch
	CheckDMRules ();

	// see if needpass needs updated
	CheckNeedPass ();

	// build the playerstate_t structures for all players
	ClientEndServerFrames ();
}


void	Svcmd_Test_f ()
{
	gi.cprintf (NULL, PRINT_HIGH, "Server: test command\n");
}


bool StringToFilter (char *s, ipfilter_t *f)
{
	char	num[128];
	int		i, j;
	byte	b[4];
	byte	m[4];

	for (i=0 ; i<4 ; i++)
	{
		b[i] = 0;
		m[i] = 0;
	}

	for (i=0 ; i<4 ; i++)
	{
		if (*s < '0' || *s > '9')
		{
			gi.cprintf(NULL, PRINT_HIGH, "Bad filter address: %s\n", s);
			return false;
		}

		j = 0;
		while (*s >= '0' && *s <= '9')
		{
			num[j++] = *s++;
		}
		num[j] = 0;
		b[i] = atoi(num);
		if (b[i] != 0)
			m[i] = 255;

		if (!*s)
			break;
		s++;
	}

	f->mask = *(unsigned *)m;
	f->compare = *(unsigned *)b;

	return true;
}


void SVCmd_AddIP_f ()
{
	int		i;

	if (gi.argc() < 3)
	{
		gi.cprintf(NULL, PRINT_HIGH, "USAGE: sv addip <ip-mask>\n");
		return;
	}

	for (i=0 ; i<numipfilters ; i++)
		if (ipfilters[i].compare == 0xffffffff)
			break;		// free spot
	if (i == numipfilters)
	{
		if (numipfilters == MAX_IPFILTERS)
		{
			gi.cprintf (NULL, PRINT_HIGH, "IP filter list is full\n");
			return;
		}
		numipfilters++;
	}

	if (!StringToFilter (gi.argv(2), &ipfilters[i]))
		ipfilters[i].compare = 0xffffffff;
}


void SVCmd_RemoveIP_f ()
{
	ipfilter_t	f;
	int			i, j;

	if (gi.argc() < 3)
	{
		gi.cprintf(NULL, PRINT_HIGH, "USAGE: sv removeip <ip-mask>\n");
		return;
	}

	if (!StringToFilter (gi.argv(2), &f))
		return;

	for (i=0 ; i<numipfilters ; i++)
		if (ipfilters[i].mask == f.mask
		&& ipfilters[i].compare == f.compare)
		{
			for (j=i+1 ; j<numipfilters ; j++)
				ipfilters[j-1] = ipfilters[j];
			numipfilters--;
			gi.cprintf (NULL, PRINT_HIGH, "Removed.\n");
			return;
		}
	gi.cprintf (NULL, PRINT_HIGH, "Didn't find %s.\n", gi.argv(2));
}


void SVCmd_ListIP_f ()
{
	int		i;
	byte	b[4];

	gi.cprintf (NULL, PRINT_HIGH, "Filter list:\n");
	for (i=0 ; i<numipfilters ; i++)
	{
		*(unsigned *)b = ipfilters[i].compare;
		gi.cprintf (NULL, PRINT_HIGH, "%3i.%3i.%3i.%3i\n", b[0], b[1], b[2], b[3]);
	}
}


void SVCmd_WriteIP_f ()
{
	FILE	*f;
	char	name[MAX_OSPATH];
	byte	b[4];
	int		i;
	cvar_t	*game;

	game = gi.cvar("game", "", 0);

	if (!*game->string)
		sprintf (name, "%s/listip.cfg", GAMEVERSION);
	else
		sprintf (name, "%s/listip.cfg", game->string);

	gi.cprintf (NULL, PRINT_HIGH, "Writing %s.\n", name);

	f = FS_Fopen(name, "wb");
	if (!f)
	{
		gi.cprintf (NULL, PRINT_HIGH, "Couldn't open %s\n", name);
		return;
	}

	fprintf(f, "set filterban %d\n", (int)filterban->value);

	for (i=0 ; i<numipfilters ; i++)
	{
		*(unsigned *)b = ipfilters[i].compare;
		fprintf (f, "sv addip %i.%i.%i.%i\n", b[0], b[1], b[2], b[3]);
	}

	fclose (f);
}


/*
=================
ServerCommand

ServerCommand will be called when an "sv" command is issued.
The game can issue gi.argc() / gi.argv() commands to get the rest
of the parameters
=================
*/
void	ServerCommand ()
{
	char	*cmd;

	cmd = gi.argv(1);
	if (Q_strcasecmp (cmd, "test") == 0)
		Svcmd_Test_f ();
	else if (Q_strcasecmp (cmd, "addip") == 0)
		SVCmd_AddIP_f ();
	else if (Q_strcasecmp (cmd, "removeip") == 0)
		SVCmd_RemoveIP_f ();
	else if (Q_strcasecmp (cmd, "listip") == 0)
		SVCmd_ListIP_f ();
	else if (Q_strcasecmp (cmd, "writeip") == 0)
		SVCmd_WriteIP_f ();
	else
		gi.cprintf (NULL, PRINT_HIGH, "Unknown server command \"%s\"\n", cmd);
}


///float	LevelTime ()
///{
///	return level.time;
///}


/*
=================
GetGameAPI

Returns a pointer to the structure with all entry points
and global variables
=================
*/
EXPORT game_export_t *GetGameAPI (game_import_t *import)
{
	gi = *import;

	globals.apiversion = GAME_API_VERSION;
	globals.Init = InitGame;
	globals.Shutdown = ShutdownGame;
	globals.SpawnEntities = SpawnEntities;

	globals.WriteGame = WriteGame;
	globals.ReadGame = ReadGame;
	globals.WriteLevel = WriteLevel;
	globals.ReadLevel = ReadLevel;

	globals.ClientThink = ClientThink;
	globals.ClientConnect = ClientConnect;
	globals.ClientUserinfoChanged = ClientUserinfoChanged;
	globals.ClientDisconnect = ClientDisconnect;
	globals.ClientBegin = ClientBegin;
	globals.ClientCommand = ClientCommand;

	globals.RunFrame = G_RunFrame;

	globals.ServerCommand = ServerCommand;

///	globals.LevelTime = LevelTime;

	globals.edict_size = sizeof(edict_t);

	return &globals;
}



































//////////////// MONSTERS CODE //////////////////////////////////////////////////////////////////////

void BossExplode (edict_t *self)
{
	vec3_t	org;
	int		n;

	self->think = BossExplode;
	VectorCopy (self->s.origin, org);
	org[2] += 24 + (rand()&15);
	switch (self->count++)
	{
	case 0:
		org[0] -= 24;
		org[1] -= 24;
		break;
	case 1:
		org[0] += 24;
		org[1] += 24;
		break;
	case 2:
		org[0] += 24;
		org[1] -= 24;
		break;
	case 3:
		org[0] -= 24;
		org[1] += 24;
		break;
	case 4:
		org[0] -= 48;
		org[1] -= 48;
		break;
	case 5:
		org[0] += 48;
		org[1] += 48;
		break;
	case 6:
		org[0] -= 48;
		org[1] += 48;
		break;
	case 7:
		org[0] += 48;
		org[1] -= 48;
		break;
	case 8:
		self->s.sound = 0;
		if(net_compatibility->value || !net_fixoverflow->value)
		{
			for (n= 0; n < 4; n++)
				ThrowGib (self, "models/objects/gibs/sm_meat/tris.md2", 500, GIB_ORGANIC);
			for (n= 0; n < 8; n++)
				ThrowGib (self, "models/objects/gibs/sm_metal/tris.md2", 500, GIB_METALLIC);
			ThrowGib (self, "models/objects/gibs/chest/tris.md2", 500, GIB_ORGANIC);
			ThrowHead (self, "models/objects/gibs/gear/tris.md2", 500, GIB_METALLIC);
			self->deadflag = DEAD_DEAD;
		}
		else
		{
			if (!strcmp(self->classname, "monster_jorg"))
				NetThrowMonsterGibs (self, GIBS_JORG, 1000);
			else if (!strcmp(self->classname, "monster_boss2"))
				NetThrowMonsterGibs (self, GIBS_BOSS2, 1000);
			else if (!strcmp(self->classname, "monster_supertank"))
				NetThrowMonsterGibs (self, GIBS_SUPERTANK, 1000);
			G_FreeEdict (self);
		}
		return;
	}

	bool large_map = LargeMapCheck(org);
	if (large_map)
		gi.WriteByte (svc_temp_entity_ext);
	else
		gi.WriteByte (svc_temp_entity);
	gi.WriteByte (TE_EXPLOSION1);
	gi.WritePosition (org, large_map);
////gi.WriteDir (vec3_origin);		/// FIXME: [0,0,1] ???
	gi.multicast (self->s.origin, MULTICAST_PVS);

	self->nextthink = level.time + 0.1;
}


/*
==============================================================================

JORG

==============================================================================
*/

void jorg_search (edict_t *self)
{
	float r;

	r = random();

	if (r <= 0.3)
		gi.sound (self, CHAN_VOICE, jorg_sound_search1, 1, ATTN_NORM, 0);
	else if (r <= 0.6)
		gi.sound (self, CHAN_VOICE, jorg_sound_search2, 1, ATTN_NORM, 0);
	else
		gi.sound (self, CHAN_VOICE, jorg_sound_search3, 1, ATTN_NORM, 0);
}


void jorg_dead (edict_t *self);
void jorgBFG (edict_t *self);
void jorgMachineGun (edict_t *self);
void jorg_firebullet (edict_t *self);
void jorg_reattack1(edict_t *self);
void jorg_attack1(edict_t *self);
void jorg_idle(edict_t *self);
void jorg_step_left(edict_t *self);
void jorg_step_right(edict_t *self);
void jorg_death_hit(edict_t *self);

//
// stand
//

mframe_t jorg_frames_stand []=
{
	ai_stand, 0, jorg_idle,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,		// 10
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,		// 20
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,		// 30
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 19, NULL,
	ai_stand, 11, jorg_step_left,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 6, NULL,
	ai_stand, 9, jorg_step_right,
	ai_stand, 0, NULL,		// 40
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, -2, NULL,
	ai_stand, -17, jorg_step_left,
	ai_stand, 0, NULL,
	ai_stand, -12, NULL,		// 50
	ai_stand, -14, jorg_step_right	// 51
};
mmove_t	jorg_move_stand = {JORG_FRAME_stand01, JORG_FRAME_stand51, jorg_frames_stand, NULL};

void jorg_idle (edict_t *self)
{
	gi.sound (self, CHAN_VOICE, jorg_sound_idle, 1, ATTN_NORM,0);
}

void jorg_death_hit (edict_t *self)
{
	gi.sound (self, CHAN_BODY, jorg_sound_death_hit, 1, ATTN_NORM,0);
}


void jorg_step_left (edict_t *self)
{
	gi.sound (self, CHAN_BODY, jorg_sound_step_left, 1, ATTN_NORM,0);
}

void jorg_step_right (edict_t *self)
{
	gi.sound (self, CHAN_BODY, jorg_sound_step_right, 1, ATTN_NORM,0);
}


void jorg_stand (edict_t *self)
{
	self->monsterinfo.currentmove = &jorg_move_stand;
}

mframe_t jorg_frames_run [] =
{
	ai_run, 17,	jorg_step_left,
	ai_run, 0,	NULL,
	ai_run, 0,	NULL,
	ai_run, 0,	NULL,
	ai_run, 12,	NULL,
	ai_run, 8,	NULL,
	ai_run, 10,	NULL,
	ai_run, 33,	jorg_step_right,
	ai_run, 0,	NULL,
	ai_run, 0,	NULL,
	ai_run, 0,	NULL,
	ai_run, 9,	NULL,
	ai_run, 9,	NULL,
	ai_run, 9,	NULL
};
mmove_t	jorg_move_run = {JORG_FRAME_walk06, JORG_FRAME_walk19, jorg_frames_run, NULL};

//
// walk
//

mframe_t jorg_frames_start_walk [] =
{
	ai_walk,	5,	NULL,
	ai_walk,	6,	NULL,
	ai_walk,	7,	NULL,
	ai_walk,	9,	NULL,
	ai_walk,	15,	NULL
};
mmove_t jorg_move_start_walk = {JORG_FRAME_walk01, JORG_FRAME_walk05, jorg_frames_start_walk, NULL};

mframe_t jorg_frames_walk [] =
{
	ai_walk, 17,	NULL,
	ai_walk, 0,	NULL,
	ai_walk, 0,	NULL,
	ai_walk, 0,	NULL,
	ai_walk, 12,	NULL,
	ai_walk, 8,	NULL,
	ai_walk, 10,	NULL,
	ai_walk, 33,	NULL,
	ai_walk, 0,	NULL,
	ai_walk, 0,	NULL,
	ai_walk, 0,	NULL,
	ai_walk, 9,	NULL,
	ai_walk, 9,	NULL,
	ai_walk, 9,	NULL
};
mmove_t	jorg_move_walk = {JORG_FRAME_walk06, JORG_FRAME_walk19, jorg_frames_walk, NULL};

mframe_t jorg_frames_end_walk [] =
{
	ai_walk,	11,	NULL,
	ai_walk,	0,	NULL,
	ai_walk,	0,	NULL,
	ai_walk,	0,	NULL,
	ai_walk,	8,	NULL,
	ai_walk,	-8,	NULL
};
mmove_t jorg_move_end_walk = {JORG_FRAME_walk20, JORG_FRAME_walk25, jorg_frames_end_walk, NULL};

void jorg_walk (edict_t *self)
{
		self->monsterinfo.currentmove = &jorg_move_walk;
}

void jorg_run (edict_t *self)
{
	if (self->monsterinfo.aiflags & AI_STAND_GROUND)
		self->monsterinfo.currentmove = &jorg_move_stand;
	else
		self->monsterinfo.currentmove = &jorg_move_run;
}

mframe_t jorg_frames_pain3 [] =
{
	ai_move,	-28,	NULL,
	ai_move,	-6,	NULL,
	ai_move,	-3,	jorg_step_left,
	ai_move,	-9,	NULL,
	ai_move,	0,	jorg_step_right,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	-7,	NULL,
	ai_move,	1,	NULL,
	ai_move,	-11,	NULL,
	ai_move,	-4,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	10,	NULL,
	ai_move,	11,	NULL,
	ai_move,	0,	NULL,
	ai_move,	10,	NULL,
	ai_move,	3,	NULL,
	ai_move,	10,	NULL,
	ai_move,	7,	jorg_step_left,
	ai_move,	17,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	jorg_step_right
};
mmove_t jorg_move_pain3 = {JORG_FRAME_pain301, JORG_FRAME_pain325, jorg_frames_pain3, jorg_run};

mframe_t jorg_frames_pain2 [] =
{
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL
};
mmove_t jorg_move_pain2 = {JORG_FRAME_pain201, JORG_FRAME_pain203, jorg_frames_pain2, jorg_run};

mframe_t jorg_frames_pain1 [] =
{
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL
};
mmove_t jorg_move_pain1 = {JORG_FRAME_pain101, JORG_FRAME_pain103, jorg_frames_pain1, jorg_run};

mframe_t jorg_frames_death1 [] =
{
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,		// 10
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,		// 20
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,		// 30
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,		// 40
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	MakronToss,
	ai_move,	0,	BossExplode		// 50
};
mmove_t jorg_move_death = {JORG_FRAME_death01, JORG_FRAME_death50, jorg_frames_death1, jorg_dead};

mframe_t jorg_frames_attack2 []=
{
	ai_charge,	0,	NULL,
	ai_charge,	0,	NULL,
	ai_charge,	0,	NULL,
	ai_charge,	0,	NULL,
	ai_charge,	0,	NULL,
	ai_charge,	0,	NULL,
	ai_charge,	0,	jorgBFG,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL
};
mmove_t jorg_move_attack2 = {JORG_FRAME_attak201, JORG_FRAME_attak213, jorg_frames_attack2, jorg_run};

mframe_t jorg_frames_start_attack1 [] =
{
	ai_charge,	0,	NULL,
	ai_charge,	0,	NULL,
	ai_charge,	0,	NULL,
	ai_charge,	0,	NULL,
	ai_charge,	0,	NULL,
	ai_charge,	0,	NULL,
	ai_charge,	0,	NULL,
	ai_charge,	0,	NULL
};
mmove_t jorg_move_start_attack1 = {JORG_FRAME_attak101, JORG_FRAME_attak108, jorg_frames_start_attack1, jorg_attack1};

mframe_t jorg_frames_attack1[]=
{
	ai_charge,	0,	jorg_firebullet,
	ai_charge,	0,	jorg_firebullet,
	ai_charge,	0,	jorg_firebullet,
	ai_charge,	0,	jorg_firebullet,
	ai_charge,	0,	jorg_firebullet,
	ai_charge,	0,	jorg_firebullet
};
mmove_t jorg_move_attack1 = {JORG_FRAME_attak109, JORG_FRAME_attak114, jorg_frames_attack1, jorg_reattack1};

mframe_t jorg_frames_end_attack1[]=
{
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL
};
mmove_t jorg_move_end_attack1 = {JORG_FRAME_attak115, JORG_FRAME_attak118, jorg_frames_end_attack1, jorg_run};

void jorg_reattack1(edict_t *self)
{
	if (visible(self, self->enemy))
		if (random() < 0.9)
			self->monsterinfo.currentmove = &jorg_move_attack1;
		else
		{
			self->s.sound = 0;
			self->monsterinfo.currentmove = &jorg_move_end_attack1;
		}
	else
	{
		self->s.sound = 0;
		self->monsterinfo.currentmove = &jorg_move_end_attack1;
	}
}

void jorg_attack1(edict_t *self)
{
	self->monsterinfo.currentmove = &jorg_move_attack1;
}

void jorg_pain (edict_t *self, edict_t *other, float kick, int damage)
{

	if (self->health < (self->max_health / 2))
		self->s.skinnum = 1;

	self->s.sound = 0;

	if (level.time < self->pain_debounce_time)
		return;

	// Lessen the chance of him going into his pain frames if he takes little damage
	if (damage <= 40)
		if (random()<=0.6)
			return;

	/*
	If he's entering his attack1 or using attack1, lessen the chance of him
	going into pain
	*/

	if ( (self->s.frame >= JORG_FRAME_attak101) && (self->s.frame <= JORG_FRAME_attak108) )
		if (random() <= 0.005)
			return;

	if ( (self->s.frame >= JORG_FRAME_attak109) && (self->s.frame <= JORG_FRAME_attak114) )
		if (random() <= 0.00005)
			return;


	if ( (self->s.frame >= JORG_FRAME_attak201) && (self->s.frame <= JORG_FRAME_attak208) )
		if (random() <= 0.005)
			return;


	self->pain_debounce_time = level.time + 3;
	if (skill->value == 3)
		return;		// no pain anims in nightmare

	if (damage <= 50)
	{
		gi.sound (self, CHAN_VOICE, jorg_sound_pain1, 1, ATTN_NORM,0);
		self->monsterinfo.currentmove = &jorg_move_pain1;
	}
	else if (damage <= 100)
	{
		gi.sound (self, CHAN_VOICE, jorg_sound_pain2, 1, ATTN_NORM,0);
		self->monsterinfo.currentmove = &jorg_move_pain2;
	}
	else
	{
		if (random() <= 0.3)
		{
			gi.sound (self, CHAN_VOICE, jorg_sound_pain3, 1, ATTN_NORM,0);
			self->monsterinfo.currentmove = &jorg_move_pain3;
		}
	}
};

void jorgBFG (edict_t *self)
{
	vec3_t	forward, right;
	vec3_t	start;
	vec3_t	dir;
	vec3_t	vec;

	AngleVectors (self->s.angles, forward, right, NULL);
	G_ProjectSource (self->s.origin, &monster_flash_offset[MZ2_JORG_BFG_1*3], forward, right, start);

	VectorCopy (self->enemy->s.origin, vec);
	vec[2] += self->enemy->viewheight;
	VectorSubtract (vec, start, dir);
	VectorNormalize (dir);
	gi.sound (self, CHAN_VOICE, jorg_sound_attack2, 1, ATTN_NORM, 0);
	/*void monster_fire_bfg (edict_t *self,
							 vec3_t start,
							 vec3_t aimdir,
							 int damage,
							 int speed,
							 int kick,
							 float damage_radius,
							 int flashtype)*/
	monster_fire_bfg (self, start, dir, 50, 300, 100, 200, MZ2_JORG_BFG_1);
}

void jorg_firebullet_right (edict_t *self)
{
	vec3_t	forward, right, target;
	vec3_t	start;

	AngleVectors (self->s.angles, forward, right, NULL);
	G_ProjectSource (self->s.origin, &monster_flash_offset[MZ2_JORG_MACHINEGUN_R1*3], forward, right, start);

	VectorMA (self->enemy->s.origin, -0.2, self->enemy->velocity, target);
	target[2] += self->enemy->viewheight;
	VectorSubtract (target, start, forward);
	VectorNormalize (forward);

	monster_fire_bullet (self, start, forward, 6, 4, DEFAULT_BULLET_HSPREAD, DEFAULT_BULLET_VSPREAD, MZ2_JORG_MACHINEGUN_R1);
}

void jorg_firebullet_left (edict_t *self)
{
	vec3_t	forward, right, target;
	vec3_t	start;

	AngleVectors (self->s.angles, forward, right, NULL);
	G_ProjectSource (self->s.origin, &monster_flash_offset[MZ2_JORG_MACHINEGUN_L1*3], forward, right, start);

	VectorMA (self->enemy->s.origin, -0.2, self->enemy->velocity, target);
	target[2] += self->enemy->viewheight;
	VectorSubtract (target, start, forward);
	VectorNormalize (forward);

	monster_fire_bullet (self, start, forward, 6, 4, DEFAULT_BULLET_HSPREAD, DEFAULT_BULLET_VSPREAD, MZ2_JORG_MACHINEGUN_L1);
}

void jorg_firebullet (edict_t *self)
{
	jorg_firebullet_left(self);
	jorg_firebullet_right(self);
};

void jorg_attack(edict_t *self)
{
//	vec3_t	vec;
//	float	range;

//	VectorSubtract (self->enemy->s.origin, self->s.origin, vec);
//	range = VectorLength (vec);

	if (random() <= 0.75)
	{
		gi.sound (self, CHAN_VOICE, jorg_sound_attack1, 1, ATTN_NORM,0);
		self->s.sound = gi.soundindex ("boss3/w_loop.wav");
		self->monsterinfo.currentmove = &jorg_move_start_attack1;
	}
	else
	{
		gi.sound (self, CHAN_VOICE, jorg_sound_attack2, 1, ATTN_NORM,0);
		self->monsterinfo.currentmove = &jorg_move_attack2;
	}
}

void jorg_dead (edict_t *self)
{
#if 0
	edict_t	*tempent;
	/*
	VectorSet (self->mins, -16, -16, -24);
	VectorSet (self->maxs, 16, 16, -8);
	*/

	// Jorg is on modelindex2. Do not clear him.
	VectorSet (self->mins, -60, -60, 0);
	VectorSet (self->maxs, 60, 60, 72);
	self->movetype = MOVETYPE_TOSS;
	self->nextthink = 0;
	gi.linkentity (self);

	tempent = G_Spawn();
	VectorCopy (self->s.origin, tempent->s.origin);
	VectorCopy (self->s.angles, tempent->s.angles);
	tempent->killtarget = self->killtarget;
	tempent->target = self->target;
	tempent->activator = self->enemy;
	self->killtarget = 0;
	self->target = 0;
	SP_monster_makron (tempent);
#endif
}


void jorg_die (edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point)
{
	gi.sound (self, CHAN_VOICE, jorg_sound_death, 1, ATTN_NORM, 0);
	self->deadflag = DEAD_DEAD;
	self->takedamage = DAMAGE_NO;
	self->s.sound = 0;
	self->count = 0;
	self->monsterinfo.currentmove = &jorg_move_death;
}

bool Jorg_CheckAttack (edict_t *self)
{
	vec3_t	spot1, spot2;
	vec3_t	temp;
	float	chance;
	trace_t	tr;
//	bool	enemy_infront;
	int		enemy_range;
	float	enemy_yaw;

	if (self->enemy->health > 0)
	{
	// see if any entities are in the way of the shot
		VectorCopy (self->s.origin, spot1);
		spot1[2] += self->viewheight;
		VectorCopy (self->enemy->s.origin, spot2);
		spot2[2] += self->enemy->viewheight;

		tr = gi.trace (spot1, NULL, NULL, spot2, self, CONTENTS_SOLID|CONTENTS_MONSTER|CONTENTS_SLIME|CONTENTS_LAVA);

		// do we have a clear shot?
		if (tr.ent != self->enemy)
			return false;
	}

//	enemy_infront = infront(self, self->enemy);
	enemy_range = range(self, self->enemy);
	VectorSubtract (self->enemy->s.origin, self->s.origin, temp);
	enemy_yaw = vectoyaw(temp);

	self->ideal_yaw = enemy_yaw;


	// melee attack
	if (enemy_range == RANGE_MELEE)
	{
		if (self->monsterinfo.melee)
			self->monsterinfo.attack_state = AS_MELEE;
		else
			self->monsterinfo.attack_state = AS_MISSILE;
		return true;
	}

// missile attack
	if (!self->monsterinfo.attack)
		return false;

	if (level.time < self->monsterinfo.attack_finished)
		return false;

	if (enemy_range == RANGE_FAR)
		return false;

	if (self->monsterinfo.aiflags & AI_STAND_GROUND)
	{
		chance = 0.4;
	}
	else if (enemy_range == RANGE_MELEE)
	{
		chance = 0.8;
	}
	else if (enemy_range == RANGE_NEAR)
	{
		chance = 0.4;
	}
	else if (enemy_range == RANGE_MID)
	{
		chance = 0.2;
	}
	else
	{
		return false;
	}

	if (random () < chance)
	{
		self->monsterinfo.attack_state = AS_MISSILE;
		self->monsterinfo.attack_finished = level.time + 2*random();
		return true;
	}

	if (self->flags & FL_FLY)
	{
		if (random() < 0.3)
			self->monsterinfo.attack_state = AS_SLIDING;
		else
			self->monsterinfo.attack_state = AS_STRAIGHT;
	}

	return false;
}


void MakronPrecache (void);

/*QUAKED monster_jorg (1 .5 0) (-80 -80 0) (90 90 140) Ambush Trigger_Spawn Sight
*/
void SP_monster_jorg (edict_t *self)
{
	if (deathmatch->value)
	{
		G_FreeEdict (self);
		return;
	}

	jorg_sound_pain1 = gi.soundindex ("boss3/bs3pain1.wav");
	jorg_sound_pain2 = gi.soundindex ("boss3/bs3pain2.wav");
	jorg_sound_pain3 = gi.soundindex ("boss3/bs3pain3.wav");
	jorg_sound_death = gi.soundindex ("boss3/bs3deth1.wav");
	jorg_sound_attack1 = gi.soundindex ("boss3/bs3atck1.wav");
	jorg_sound_attack2 = gi.soundindex ("boss3/bs3atck2.wav");
	jorg_sound_search1 = gi.soundindex ("boss3/bs3srch1.wav");
	jorg_sound_search2 = gi.soundindex ("boss3/bs3srch2.wav");
	jorg_sound_search3 = gi.soundindex ("boss3/bs3srch3.wav");
	jorg_sound_idle = gi.soundindex ("boss3/bs3idle1.wav");
	jorg_sound_step_left = gi.soundindex ("boss3/step1.wav");
	jorg_sound_step_right = gi.soundindex ("boss3/step2.wav");
	jorg_sound_firegun = gi.soundindex ("boss3/xfire.wav");
	jorg_sound_death_hit = gi.soundindex ("boss3/d_hit.wav");

	MakronPrecache ();

	self->movetype = MOVETYPE_STEP;
	self->solid = SOLID_BBOX;
	self->s.modelindex = gi.modelindex ("models/monsters/boss3/rider/tris.md2");
	self->s.modelindex2 = gi.modelindex ("models/monsters/boss3/jorg/tris.md2");
	VectorSet (self->mins, -80, -80, 0);
	VectorSet (self->maxs, 80, 80, 140);

	self->health = 3000;
	self->gib_health = -2000;
	self->mass = 1000;

	self->pain = jorg_pain;
	self->die = jorg_die;
	self->monsterinfo.stand = jorg_stand;
	self->monsterinfo.walk = jorg_walk;
	self->monsterinfo.run = jorg_run;
	self->monsterinfo.dodge = NULL;
	self->monsterinfo.attack = jorg_attack;
	self->monsterinfo.search = jorg_search;
	self->monsterinfo.melee = NULL;
	self->monsterinfo.sight = NULL;
	self->monsterinfo.checkattack = Jorg_CheckAttack;
	gi.linkentity (self);

	self->monsterinfo.currentmove = &jorg_move_stand;
	self->monsterinfo.scale = JORG_MODEL_SCALE;

	walkmonster_start(self, true);
}

/*
==============================================================================

END of JORG

==============================================================================
*/

/*QUAKED misc_deadsoldier (1 .5 0) (-16 -16 0) (16 16 16) ON_BACK ON_STOMACH BACK_DECAP FETAL_POS SIT_DECAP IMPALED
This is the dead player model. Comes in 6 exciting different poses!
*/
void misc_deadsoldier_die (edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point)
{
	int		n;

	if (self->health > -80)
		return;

	gi.sound (self, CHAN_BODY, gi.soundindex ("misc/udeath.wav"), 1, ATTN_NORM, 0);
	if(net_compatibility->value || !net_fixoverflow->value)
	{
		for (n= 0; n < 4; n++)
			ThrowGib (self, "models/objects/gibs/sm_meat/tris.md2", damage, GIB_ORGANIC);
		ThrowHead (self, "models/objects/gibs/head2/tris.md2", damage, GIB_ORGANIC);
	}
	else
	{
		NetThrowMonsterGibs (self, GIBS_DEADSOLDIER, damage);
		G_FreeEdict (self);
	}
}


void SP_misc_deadsoldier (edict_t *ent)
{
	if (deathmatch->value)
	{	// auto-remove for deathmatch
		G_FreeEdict (ent);
		return;
	}

	ent->movetype = MOVETYPE_NONE;
	ent->solid = SOLID_BBOX;
	ent->s.modelindex=gi.modelindex ("models/deadbods/dude/tris.md2");

	// Defaults to frame 0
	if (ent->spawnflags & 2)
		ent->s.frame = 1;
	else if (ent->spawnflags & 4)
		ent->s.frame = 2;
	else if (ent->spawnflags & 8)
		ent->s.frame = 3;
	else if (ent->spawnflags & 16)
		ent->s.frame = 4;
	else if (ent->spawnflags & 32)
		ent->s.frame = 5;
	else
		ent->s.frame = 0;

	VectorSet (ent->mins, -16, -16, 0);
	VectorSet (ent->maxs, 16, 16, 16);
	ent->deadflag = DEAD_DEAD;
	ent->takedamage = DAMAGE_YES;
	ent->svflags |= SVF_MONSTER|SVF_DEADMONSTER;
	ent->die = misc_deadsoldier_die;
	ent->monsterinfo.aiflags |= AI_GOOD_GUY;

	gi.linkentity (ent);
}


/*
==============================================================================

Makron -- Final Boss

==============================================================================
*/

void MakronRailgun (edict_t *self);
void MakronSaveloc (edict_t *self);
void MakronHyperblaster (edict_t *self);
void makron_step_left (edict_t *self);
void makron_step_right (edict_t *self);
void makronBFG (edict_t *self);
void makron_dead (edict_t *self);


void makron_taunt (edict_t *self)
{
	float r;

	r=random();
	if (r <= 0.3)
		gi.sound (self, CHAN_AUTO, makron_sound_taunt1, 1, ATTN_NONE, 0);
	else if (r <= 0.6)
		gi.sound (self, CHAN_AUTO, makron_sound_taunt2, 1, ATTN_NONE, 0);
	else
		gi.sound (self, CHAN_AUTO, makron_sound_taunt3, 1, ATTN_NONE, 0);
}

//
// stand
//

mframe_t makron_frames_stand []=
{
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,		// 10
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,		// 20
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,		// 30
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,		// 40
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,		// 50
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL		// 60
};
mmove_t	makron_move_stand = {MAKRON_FRAME_stand201, MAKRON_FRAME_stand260, makron_frames_stand, NULL};

void makron_stand (edict_t *self)
{
	self->monsterinfo.currentmove = &makron_move_stand;
}

mframe_t makron_frames_run [] =
{
	ai_run, 3,	makron_step_left,
	ai_run, 12,	NULL,
	ai_run, 8,	NULL,
	ai_run, 8,	NULL,
	ai_run, 8,	makron_step_right,
	ai_run, 6,	NULL,
	ai_run, 12,	NULL,
	ai_run, 9,	NULL,
	ai_run, 6,	NULL,
	ai_run, 12,	NULL
};
mmove_t	makron_move_run = {MAKRON_FRAME_walk204, MAKRON_FRAME_walk213, makron_frames_run, NULL};

void makron_hit (edict_t *self)
{
	gi.sound (self, CHAN_AUTO, makron_sound_hit, 1, ATTN_NONE,0);
}

void makron_popup (edict_t *self)
{
	gi.sound (self, CHAN_BODY, makron_sound_popup, 1, ATTN_NONE,0);
}

void makron_step_left (edict_t *self)
{
	gi.sound (self, CHAN_BODY, makron_sound_step_left, 1, ATTN_NORM,0);
}

void makron_step_right (edict_t *self)
{
	gi.sound (self, CHAN_BODY, makron_sound_step_right, 1, ATTN_NORM,0);
}

void makron_brainsplorch (edict_t *self)
{
	gi.sound (self, CHAN_VOICE, makron_sound_brainsplorch, 1, ATTN_NORM,0);
}

void makron_prerailgun (edict_t *self)
{
	gi.sound (self, CHAN_WEAPON, makron_sound_prerailgun, 1, ATTN_NORM,0);
}


mframe_t makron_frames_walk [] =
{
	ai_walk, 3,	makron_step_left,
	ai_walk, 12,	NULL,
	ai_walk, 8,	NULL,
	ai_walk, 8,	NULL,
	ai_walk, 8,	makron_step_right,
	ai_walk, 6,	NULL,
	ai_walk, 12,	NULL,
	ai_walk, 9,	NULL,
	ai_walk, 6,	NULL,
	ai_walk, 12,	NULL
};
mmove_t	makron_move_walk = {MAKRON_FRAME_walk204, MAKRON_FRAME_walk213, makron_frames_run, NULL};

void makron_walk (edict_t *self)
{
		self->monsterinfo.currentmove = &makron_move_walk;
}

void makron_run (edict_t *self)
{
	if (self->monsterinfo.aiflags & AI_STAND_GROUND)
		self->monsterinfo.currentmove = &makron_move_stand;
	else
		self->monsterinfo.currentmove = &makron_move_run;
}

mframe_t makron_frames_pain6 [] =
{
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,		// 10
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	makron_popup,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,		// 20
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	makron_taunt,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL
};
mmove_t makron_move_pain6 = {MAKRON_FRAME_pain601, MAKRON_FRAME_pain627, makron_frames_pain6, makron_run};

mframe_t makron_frames_pain5 [] =
{
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL
};
mmove_t makron_move_pain5 = {MAKRON_FRAME_pain501, MAKRON_FRAME_pain504, makron_frames_pain5, makron_run};

mframe_t makron_frames_pain4 [] =
{
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL
};
mmove_t makron_move_pain4 = {MAKRON_FRAME_pain401, MAKRON_FRAME_pain404, makron_frames_pain4, makron_run};

mframe_t makron_frames_death2 [] =
{
	ai_move,	-15,	NULL,
	ai_move,	3,	NULL,
	ai_move,	-12,	NULL,
	ai_move,	0,	makron_step_left,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,			// 10
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	11,	NULL,
	ai_move,	12,	NULL,
	ai_move,	11,	makron_step_right,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,			// 20
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,			// 30
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	5,	NULL,
	ai_move,	7,	NULL,
	ai_move,	6,	makron_step_left,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	-1,	NULL,
	ai_move,	2,	NULL,			// 40
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,			// 50
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	-6,	NULL,
	ai_move,	-4,	NULL,
	ai_move,	-6,	makron_step_right,
	ai_move,	-4,	NULL,
	ai_move,	-4,	makron_step_left,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,			// 60
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	-2,	NULL,
	ai_move,	-5,	NULL,
	ai_move,	-3,	makron_step_right,
	ai_move,	-8,	NULL,
	ai_move,	-3,	makron_step_left,
	ai_move,	-7,	NULL,
	ai_move,	-4,	NULL,
	ai_move,	-4,	makron_step_right,			// 70
	ai_move,	-6,	NULL,
	ai_move,	-7,	NULL,
	ai_move,	0,	makron_step_left,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,			// 80
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	-2,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	2,	NULL,
	ai_move,	0,	NULL,			// 90
	ai_move,	27,	makron_hit,
	ai_move,	26,	NULL,
	ai_move,	0,	makron_brainsplorch,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL			// 95
};
mmove_t makron_move_death2 = {MAKRON_FRAME_death201, MAKRON_FRAME_death295, makron_frames_death2, makron_dead};

mframe_t makron_frames_death3 [] =
{
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL
};
mmove_t makron_move_death3 = {MAKRON_FRAME_death301, MAKRON_FRAME_death320, makron_frames_death3, NULL};

mframe_t makron_frames_sight [] =
{
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL
};
mmove_t makron_move_sight= {MAKRON_FRAME_active01, MAKRON_FRAME_active13, makron_frames_sight, makron_run};

void makronBFG (edict_t *self)
{
	vec3_t	forward, right;
	vec3_t	start;
	vec3_t	dir;
	vec3_t	vec;

	AngleVectors (self->s.angles, forward, right, NULL);
	G_ProjectSource (self->s.origin, &monster_flash_offset[MZ2_MAKRON_BFG*3], forward, right, start);

	VectorCopy (self->enemy->s.origin, vec);
	vec[2] += self->enemy->viewheight;
	VectorSubtract (vec, start, dir);
	VectorNormalize (dir);
	gi.sound (self, CHAN_VOICE, makron_sound_attack_bfg, 1, ATTN_NORM, 0);
	monster_fire_bfg (self, start, dir, 50, 300, 100, 300, MZ2_MAKRON_BFG);
}


mframe_t makron_frames_attack3 []=
{
	ai_charge,	0,	NULL,
	ai_charge,	0,	NULL,
	ai_charge,	0,	NULL,
	ai_charge,	0,	makronBFG,		// FIXME: BFG Attack here
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL
};
mmove_t makron_move_attack3 = {MAKRON_FRAME_attak301, MAKRON_FRAME_attak308, makron_frames_attack3, makron_run};

mframe_t makron_frames_attack4[]=
{
	ai_charge,	0,	NULL,
	ai_charge,	0,	NULL,
	ai_charge,	0,	NULL,
	ai_charge,	0,	NULL,
	ai_move,	0,	MakronHyperblaster,		// fire
	ai_move,	0,	MakronHyperblaster,		// fire
	ai_move,	0,	MakronHyperblaster,		// fire
	ai_move,	0,	MakronHyperblaster,		// fire
	ai_move,	0,	MakronHyperblaster,		// fire
	ai_move,	0,	MakronHyperblaster,		// fire
	ai_move,	0,	MakronHyperblaster,		// fire
	ai_move,	0,	MakronHyperblaster,		// fire
	ai_move,	0,	MakronHyperblaster,		// fire
	ai_move,	0,	MakronHyperblaster,		// fire
	ai_move,	0,	MakronHyperblaster,		// fire
	ai_move,	0,	MakronHyperblaster,		// fire
	ai_move,	0,	MakronHyperblaster,		// fire
	ai_move,	0,	MakronHyperblaster,		// fire
	ai_move,	0,	MakronHyperblaster,		// fire
	ai_move,	0,	MakronHyperblaster,		// fire
	ai_move,	0,	MakronHyperblaster,		// fire
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL
};
mmove_t makron_move_attack4 = {MAKRON_FRAME_attak401, MAKRON_FRAME_attak426, makron_frames_attack4, makron_run};

mframe_t makron_frames_attack5[]=
{
	ai_charge,	0,	makron_prerailgun,
	ai_charge,	0,	NULL,
	ai_charge,	0,	NULL,
	ai_charge,	0,	NULL,
	ai_charge,	0,	NULL,
	ai_charge,	0,	NULL,
	ai_charge,	0,	NULL,
	ai_charge,	0,	MakronSaveloc,
	ai_move,	0,	MakronRailgun,		// Fire railgun
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL
};
mmove_t makron_move_attack5 = {MAKRON_FRAME_attak501, MAKRON_FRAME_attak516, makron_frames_attack5, makron_run};

void MakronSaveloc (edict_t *self)
{
	VectorCopy (self->enemy->s.origin, self->pos1);	//save for aiming the shot
	self->pos1[2] += self->enemy->viewheight;
};

// FIXME: He's not firing from the proper Z
void MakronRailgun (edict_t *self)
{
	vec3_t	start;
	vec3_t	dir;
	vec3_t	forward, right;

	AngleVectors (self->s.angles, forward, right, NULL);
	G_ProjectSource (self->s.origin, &monster_flash_offset[MZ2_MAKRON_RAILGUN_1*3], forward, right, start);

	// calc direction to where we targted
	VectorSubtract (self->pos1, start, dir);
	VectorNormalize (dir);

	monster_fire_railgun (self, start, dir, 50, 100, MZ2_MAKRON_RAILGUN_1);
}

// FIXME: This is all wrong. He's not firing at the proper angles.
void MakronHyperblaster (edict_t *self)
{
	vec3_t	dir;
	vec3_t	vec;
	vec3_t	start;
	vec3_t	forward, right;
	int		flash_number;

	flash_number = MZ2_MAKRON_BLASTER_1 + (self->s.frame - MAKRON_FRAME_attak405);

	AngleVectors (self->s.angles, forward, right, NULL);
	G_ProjectSource (self->s.origin, &monster_flash_offset[flash_number*3], forward, right, start);

	if (self->enemy)
	{
		VectorCopy (self->enemy->s.origin, vec);
		vec[2] += self->enemy->viewheight;
		VectorSubtract (vec, start, vec);
		vectoangles (vec, vec);
		dir[0] = vec[0];
	}
	else
	{
		dir[0] = 0;
	}
	if (self->s.frame <= MAKRON_FRAME_attak413)
		dir[1] = self->s.angles[1] - 10 * (self->s.frame - MAKRON_FRAME_attak413);
	else
		dir[1] = self->s.angles[1] + 10 * (self->s.frame - MAKRON_FRAME_attak421);
	dir[2] = 0;

	AngleVectors (dir, forward, NULL, NULL);

	monster_fire_blaster (self, start, forward, 15, 1000, MZ2_MAKRON_BLASTER_1, EF_BLASTER);
}


void makron_pain (edict_t *self, edict_t *other, float kick, int damage)
{

	if (self->health < (self->max_health / 2))
			self->s.skinnum = 1;

	if (level.time < self->pain_debounce_time)
			return;

	// Lessen the chance of him going into his pain frames
	if (damage <=25)
		if (random()<0.2)
			return;

	self->pain_debounce_time = level.time + 3;
	if (skill->value == 3)
		return;		// no pain anims in nightmare


	if (damage <= 40)
	{
		gi.sound (self, CHAN_VOICE, makron_sound_pain4, 1, ATTN_NONE,0);
		self->monsterinfo.currentmove = &makron_move_pain4;
	}
	else if (damage <= 110)
	{
		gi.sound (self, CHAN_VOICE, makron_sound_pain5, 1, ATTN_NONE,0);
		self->monsterinfo.currentmove = &makron_move_pain5;
	}
	else
	{
		if (damage <= 150)
			if (random() <= 0.45)
			{
				gi.sound (self, CHAN_VOICE, makron_sound_pain6, 1, ATTN_NONE,0);
				self->monsterinfo.currentmove = &makron_move_pain6;
			}
		else
			if (random() <= 0.35)
			{
				gi.sound (self, CHAN_VOICE, makron_sound_pain6, 1, ATTN_NONE,0);
				self->monsterinfo.currentmove = &makron_move_pain6;
			}
	}
};

void makron_sight(edict_t *self, edict_t *other)
{
	self->monsterinfo.currentmove = &makron_move_sight;
};

void makron_attack(edict_t *self)
{
//	vec3_t	vec;
//	float	range;
	float	r;

	r = random();

//	VectorSubtract (self->enemy->s.origin, self->s.origin, vec);
//	range = VectorLength (vec);


	if (r <= 0.3)
		self->monsterinfo.currentmove = &makron_move_attack3;
	else if (r <= 0.6)
		self->monsterinfo.currentmove = &makron_move_attack4;
	else
		self->monsterinfo.currentmove = &makron_move_attack5;
}

/*
---
Makron Torso. This needs to be spawned in
---
*/

void makron_torso_think (edict_t *self)
{
	if (++self->s.frame < 365)
		self->nextthink = level.time + FRAMETIME;
	else
	{
		self->s.frame = 346;
		self->nextthink = level.time + FRAMETIME;
	}
}

void makron_torso (edict_t *ent)
{
	ent->movetype = MOVETYPE_NONE;
	ent->solid = SOLID_NOT;
	VectorSet (ent->mins, -8, -8, 0);
	VectorSet (ent->maxs, 8, 8, 8);
	ent->s.frame = 346;
	ent->s.modelindex = gi.modelindex ("models/monsters/boss3/rider/tris.md2");
	ent->think = makron_torso_think;
	ent->nextthink = level.time + 2 * FRAMETIME;
	ent->s.sound = gi.soundindex ("makron/spine.wav");
	gi.linkentity (ent);
}


//
// death
//

void makron_dead (edict_t *self)
{
	VectorSet (self->mins, -60, -60, 0);
	VectorSet (self->maxs, 60, 60, 72);
	self->movetype = MOVETYPE_TOSS;
	self->svflags |= SVF_DEADMONSTER;
	self->nextthink = 0;
	gi.linkentity (self);
}


void makron_die (edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point)
{
	edict_t *tempent;

	int		n;

	self->s.sound = 0;
	// check for gib
	if (self->health <= self->gib_health)
	{
		gi.sound (self, CHAN_VOICE, gi.soundindex ("misc/udeath.wav"), 1, ATTN_NORM, 0);
		if(net_compatibility->value || !net_fixoverflow->value)
		{
			for (n= 0; n < 1 /*4*/; n++)
				ThrowGib (self, "models/objects/gibs/sm_meat/tris.md2", damage, GIB_ORGANIC);
			for (n= 0; n < 4; n++)
				ThrowGib (self, "models/objects/gibs/sm_metal/tris.md2", damage, GIB_METALLIC);
			ThrowHead (self, "models/objects/gibs/gear/tris.md2", damage, GIB_METALLIC);
			self->deadflag = DEAD_DEAD;
		}
		else
		{
			NetThrowGibs (self, GIBS_MAKRON, damage);
			G_FreeEdict (self);
		}
		return;
	}

	if (self->deadflag == DEAD_DEAD)
		return;

// regular death
	gi.sound (self, CHAN_VOICE, makron_sound_death, 1, ATTN_NONE, 0);
	self->deadflag = DEAD_DEAD;
	self->takedamage = DAMAGE_YES;

	tempent = G_Spawn();
	VectorCopy (self->s.origin, tempent->s.origin);
	VectorCopy (self->s.angles, tempent->s.angles);
	tempent->s.origin[1] -= 84;
	makron_torso (tempent);

	self->monsterinfo.currentmove = &makron_move_death2;

}


bool Makron_CheckAttack (edict_t *self)
{
	vec3_t	spot1, spot2;
	vec3_t	temp;
	float	chance;
	trace_t	tr;
//	bool	enemy_infront;
	int		enemy_range;
	float	enemy_yaw;

	if (self->enemy->health > 0)
	{
	// see if any entities are in the way of the shot
		VectorCopy (self->s.origin, spot1);
		spot1[2] += self->viewheight;
		VectorCopy (self->enemy->s.origin, spot2);
		spot2[2] += self->enemy->viewheight;

		tr = gi.trace (spot1, NULL, NULL, spot2, self, CONTENTS_SOLID|CONTENTS_MONSTER|CONTENTS_SLIME|CONTENTS_LAVA);

		// do we have a clear shot?
		if (tr.ent != self->enemy)
			return false;
	}

//	enemy_infront = infront(self, self->enemy);
	enemy_range = range(self, self->enemy);
	VectorSubtract (self->enemy->s.origin, self->s.origin, temp);
	enemy_yaw = vectoyaw(temp);

	self->ideal_yaw = enemy_yaw;


	// melee attack
	if (enemy_range == RANGE_MELEE)
	{
		if (self->monsterinfo.melee)
			self->monsterinfo.attack_state = AS_MELEE;
		else
			self->monsterinfo.attack_state = AS_MISSILE;
		return true;
	}

// missile attack
	if (!self->monsterinfo.attack)
		return false;

	if (level.time < self->monsterinfo.attack_finished)
		return false;

	if (enemy_range == RANGE_FAR)
		return false;

	if (self->monsterinfo.aiflags & AI_STAND_GROUND)
	{
		chance = 0.4;
	}
	else if (enemy_range == RANGE_MELEE)
	{
		chance = 0.8;
	}
	else if (enemy_range == RANGE_NEAR)
	{
		chance = 0.4;
	}
	else if (enemy_range == RANGE_MID)
	{
		chance = 0.2;
	}
	else
	{
		return false;
	}

	if (random () < chance)
	{
		self->monsterinfo.attack_state = AS_MISSILE;
		self->monsterinfo.attack_finished = level.time + 2*random();
		return true;
	}

	if (self->flags & FL_FLY)
	{
		if (random() < 0.3)
			self->monsterinfo.attack_state = AS_SLIDING;
		else
			self->monsterinfo.attack_state = AS_STRAIGHT;
	}

	return false;
}


//
// monster_makron
//

void MakronPrecache ()
{
	makron_sound_pain4 = gi.soundindex ("makron/pain3.wav");
	makron_sound_pain5 = gi.soundindex ("makron/pain2.wav");
	makron_sound_pain6 = gi.soundindex ("makron/pain1.wav");
	makron_sound_death = gi.soundindex ("makron/death.wav");
	makron_sound_step_left = gi.soundindex ("makron/step1.wav");
	makron_sound_step_right = gi.soundindex ("makron/step2.wav");
	makron_sound_attack_bfg = gi.soundindex ("makron/bfg_fire.wav");
	makron_sound_brainsplorch = gi.soundindex ("makron/brain1.wav");
	makron_sound_prerailgun = gi.soundindex ("makron/rail_up.wav");
	makron_sound_popup = gi.soundindex ("makron/popup.wav");
	makron_sound_taunt1 = gi.soundindex ("makron/voice4.wav");
	makron_sound_taunt2 = gi.soundindex ("makron/voice3.wav");
	makron_sound_taunt3 = gi.soundindex ("makron/voice.wav");
	makron_sound_hit = gi.soundindex ("makron/bhit.wav");

	gi.modelindex ("models/monsters/boss3/rider/tris.md2");
}

/*QUAKED monster_makron (1 .5 0) (-30 -30 0) (30 30 90) Ambush Trigger_Spawn Sight
*/
void SP_monster_makron (edict_t *self)
{
	if (deathmatch->value)
	{
		G_FreeEdict (self);
		return;
	}

	MakronPrecache ();

	self->movetype = MOVETYPE_STEP;
	self->solid = SOLID_BBOX;
	self->s.modelindex = gi.modelindex ("models/monsters/boss3/rider/tris.md2");
	VectorSet (self->mins, -30, -30, 0);
	VectorSet (self->maxs, 30, 30, 90);

	self->health = 3000;
	self->gib_health = -2000;
	self->mass = 500;

	self->pain = makron_pain;
	self->die = makron_die;
	self->monsterinfo.stand = makron_stand;
	self->monsterinfo.walk = makron_walk;
	self->monsterinfo.run = makron_run;
	self->monsterinfo.dodge = NULL;
	self->monsterinfo.attack = makron_attack;
	self->monsterinfo.melee = NULL;
	self->monsterinfo.sight = makron_sight;
	self->monsterinfo.checkattack = Makron_CheckAttack;

	gi.linkentity (self);

//	self->monsterinfo.currentmove = &makron_move_stand;
	self->monsterinfo.currentmove = &makron_move_sight;
	self->monsterinfo.scale = MAKRON_MODEL_SCALE;

	walkmonster_start(self, true);
}


/*
=================
MakronSpawn

=================
*/
void MakronSpawn (edict_t *self)
{
	vec3_t		vec;
	edict_t		*player;

	SP_monster_makron (self);

	// jump at player
	player = level.sight_client;
	if (!player)
		return;

	VectorSubtract (player->s.origin, self->s.origin, vec);
	self->s.angles[YAW] = vectoyaw(vec);
	VectorNormalize (vec);
	VectorMA (vec3_origin, 400, vec, self->velocity);
	self->velocity[2] = 200;
	self->groundentity = NULL;
}

/*
=================
MakronToss

Jorg is just about dead, so set up to launch Makron out
=================
*/
void MakronToss (edict_t *self)
{
	edict_t	*ent;

	ent = G_Spawn ();
	ent->nextthink = level.time + 0.8;
	ent->think = MakronSpawn;
	ent->target = self->target;
	VectorCopy (self->s.origin, ent->s.origin);
}

/*
==============================================================================

END of Makron -- Final Boss

==============================================================================
*/


/*
==============================================================================

SOLDIER

==============================================================================
*/

void soldier_idle (edict_t *self)
{
	if (random() > 0.8)
		gi.sound (self, CHAN_VOICE, soldierx_sound_idle, 1, ATTN_IDLE, 0);
}

void soldier_cock (edict_t *self)
{
	if (self->s.frame == SOLDIER_FRAME_stand322)
		gi.sound (self, CHAN_WEAPON, soldierx_sound_cock, 1, ATTN_IDLE, 0);
	else
		gi.sound (self, CHAN_WEAPON, soldierx_sound_cock, 1, ATTN_NORM, 0);
}


// STAND

void soldier_stand (edict_t *self);

mframe_t soldier_frames_stand1 [] =
{
	ai_stand, 0, soldier_idle,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,

	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,

	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL
};
mmove_t soldier_move_stand1 = {SOLDIER_FRAME_stand101, SOLDIER_FRAME_stand130, soldier_frames_stand1, soldier_stand};

mframe_t soldier_frames_stand3 [] =
{
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,

	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,

	ai_stand, 0, NULL,
	ai_stand, 0, soldier_cock,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,

	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL
};
mmove_t soldier_move_stand3 = {SOLDIER_FRAME_stand301, SOLDIER_FRAME_stand339, soldier_frames_stand3, soldier_stand};

#if 0
mframe_t soldier_frames_stand4 [] =
{
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,

	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,

	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,

	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,

	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 4, NULL,
	ai_stand, 1, NULL,
	ai_stand, -1, NULL,
	ai_stand, -2, NULL,

	ai_stand, 0, NULL,
	ai_stand, 0, NULL
};
mmove_t soldier_move_stand4 = {SOLDIER_FRAME_stand401, SOLDIER_FRAME_stand452, soldier_frames_stand4, NULL};
#endif

void soldier_stand (edict_t *self)
{
	if ((self->monsterinfo.currentmove == &soldier_move_stand3) || (random() < 0.8))
		self->monsterinfo.currentmove = &soldier_move_stand1;
	else
		self->monsterinfo.currentmove = &soldier_move_stand3;
}


//
// WALK
//

void soldier_walk1_random (edict_t *self)
{
	if (random() > 0.1)
		self->monsterinfo.nextframe = SOLDIER_FRAME_walk101;
}

mframe_t soldier_frames_walk1 [] =
{
	ai_walk, 3,  NULL,
	ai_walk, 6,  NULL,
	ai_walk, 2,  NULL,
	ai_walk, 2,  NULL,
	ai_walk, 2,  NULL,
	ai_walk, 1,  NULL,
	ai_walk, 6,  NULL,
	ai_walk, 5,  NULL,
	ai_walk, 3,  NULL,
	ai_walk, -1, soldier_walk1_random,
	ai_walk, 0,  NULL,
	ai_walk, 0,  NULL,
	ai_walk, 0,  NULL,
	ai_walk, 0,  NULL,
	ai_walk, 0,  NULL,
	ai_walk, 0,  NULL,
	ai_walk, 0,  NULL,
	ai_walk, 0,  NULL,
	ai_walk, 0,  NULL,
	ai_walk, 0,  NULL,
	ai_walk, 0,  NULL,
	ai_walk, 0,  NULL,
	ai_walk, 0,  NULL,
	ai_walk, 0,  NULL,
	ai_walk, 0,  NULL,
	ai_walk, 0,  NULL,
	ai_walk, 0,  NULL,
	ai_walk, 0,  NULL,
	ai_walk, 0,  NULL,
	ai_walk, 0,  NULL,
	ai_walk, 0,  NULL,
	ai_walk, 0,  NULL,
	ai_walk, 0,  NULL
};
mmove_t soldier_move_walk1 = {SOLDIER_FRAME_walk101, SOLDIER_FRAME_walk133, soldier_frames_walk1, NULL};

mframe_t soldier_frames_walk2 [] =
{
	ai_walk, 4,  NULL,
	ai_walk, 4,  NULL,
	ai_walk, 9,  NULL,
	ai_walk, 8,  NULL,
	ai_walk, 5,  NULL,
	ai_walk, 1,  NULL,
	ai_walk, 3,  NULL,
	ai_walk, 7,  NULL,
	ai_walk, 6,  NULL,
	ai_walk, 7,  NULL
};
mmove_t soldier_move_walk2 = {SOLDIER_FRAME_walk209, SOLDIER_FRAME_walk218, soldier_frames_walk2, NULL};

void soldier_walk (edict_t *self)
{
	if (random() < 0.5)
		self->monsterinfo.currentmove = &soldier_move_walk1;
	else
		self->monsterinfo.currentmove = &soldier_move_walk2;
}


//
// RUN
//

void soldier_run (edict_t *self);

mframe_t soldier_frames_start_run [] =
{
	ai_run, 7,  NULL,
	ai_run, 5,  NULL
};
mmove_t soldier_move_start_run = {SOLDIER_FRAME_run01, SOLDIER_FRAME_run02, soldier_frames_start_run, soldier_run};

mframe_t soldier_frames_run [] =
{
	ai_run, 10, NULL,
	ai_run, 11, NULL,
	ai_run, 11, NULL,
	ai_run, 16, NULL,
	ai_run, 10, NULL,
	ai_run, 15, NULL
};
mmove_t soldier_move_run = {SOLDIER_FRAME_run03, SOLDIER_FRAME_run08, soldier_frames_run, NULL};

void soldier_run (edict_t *self)
{
	if (self->monsterinfo.aiflags & AI_STAND_GROUND)
	{
		self->monsterinfo.currentmove = &soldier_move_stand1;
		return;
	}

	if (self->monsterinfo.currentmove == &soldier_move_walk1 ||
		self->monsterinfo.currentmove == &soldier_move_walk2 ||
		self->monsterinfo.currentmove == &soldier_move_start_run)
	{
		self->monsterinfo.currentmove = &soldier_move_run;
	}
	else
	{
		self->monsterinfo.currentmove = &soldier_move_start_run;
	}
}


//
// PAIN
//

mframe_t soldier_frames_pain1 [] =
{
	ai_move, -3, NULL,
	ai_move, 4,  NULL,
	ai_move, 1,  NULL,
	ai_move, 1,  NULL,
	ai_move, 0,  NULL
};
mmove_t soldier_move_pain1 = {SOLDIER_FRAME_pain101, SOLDIER_FRAME_pain105, soldier_frames_pain1, soldier_run};

mframe_t soldier_frames_pain2 [] =
{
	ai_move, -13, NULL,
	ai_move, -1,  NULL,
	ai_move, 2,   NULL,
	ai_move, 4,   NULL,
	ai_move, 2,   NULL,
	ai_move, 3,   NULL,
	ai_move, 2,   NULL
};
mmove_t soldier_move_pain2 = {SOLDIER_FRAME_pain201, SOLDIER_FRAME_pain207, soldier_frames_pain2, soldier_run};

mframe_t soldier_frames_pain3 [] =
{
	ai_move, -8, NULL,
	ai_move, 10, NULL,
	ai_move, -4, NULL,
	ai_move, -1, NULL,
	ai_move, -3, NULL,
	ai_move, 0,  NULL,
	ai_move, 3,  NULL,
	ai_move, 0,  NULL,
	ai_move, 0,  NULL,
	ai_move, 0,  NULL,
	ai_move, 0,  NULL,
	ai_move, 1,  NULL,
	ai_move, 0,  NULL,
	ai_move, 1,  NULL,
	ai_move, 2,  NULL,
	ai_move, 4,  NULL,
	ai_move, 3,  NULL,
	ai_move, 2,  NULL
};
mmove_t soldier_move_pain3 = {SOLDIER_FRAME_pain301, SOLDIER_FRAME_pain318, soldier_frames_pain3, soldier_run};

mframe_t soldier_frames_pain4 [] =
{
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, -10, NULL,
	ai_move, -6,  NULL,
	ai_move, 8,   NULL,
	ai_move, 4,   NULL,
	ai_move, 1,   NULL,
	ai_move, 0,   NULL,
	ai_move, 2,   NULL,
	ai_move, 5,   NULL,
	ai_move, 2,   NULL,
	ai_move, -1,  NULL,
	ai_move, -1,  NULL,
	ai_move, 3,   NULL,
	ai_move, 2,   NULL,
	ai_move, 0,   NULL
};
mmove_t soldier_move_pain4 = {SOLDIER_FRAME_pain401, SOLDIER_FRAME_pain417, soldier_frames_pain4, soldier_run};


void soldier_pain (edict_t *self, edict_t *other, float kick, int damage)
{
	float	r;
	int		n;

	if (self->health < (self->max_health / 2))
			self->s.skinnum |= 1;

	if (level.time < self->pain_debounce_time)
	{
		if ((self->velocity[2] > 100) && ( (self->monsterinfo.currentmove == &soldier_move_pain1) || (self->monsterinfo.currentmove == &soldier_move_pain2) || (self->monsterinfo.currentmove == &soldier_move_pain3)))
			self->monsterinfo.currentmove = &soldier_move_pain4;
		return;
	}

	self->pain_debounce_time = level.time + 3;

	n = self->s.skinnum | 1;
	if (n == 1)
		gi.sound (self, CHAN_VOICE, soldier_light_sound_pain_light, 1, ATTN_NORM, 0);
	else if (n == 3)
		gi.sound (self, CHAN_VOICE, soldier_sound_pain, 1, ATTN_NORM, 0);
	else
		gi.sound (self, CHAN_VOICE, soldier_ss_sound_pain_ss, 1, ATTN_NORM, 0);

	if (self->velocity[2] > 100)
	{
		self->monsterinfo.currentmove = &soldier_move_pain4;
		return;
	}

	if (skill->value == 3)
		return;		// no pain anims in nightmare

	r = random();

	if (r < 0.33)
		self->monsterinfo.currentmove = &soldier_move_pain1;
	else if (r < 0.66)
		self->monsterinfo.currentmove = &soldier_move_pain2;
	else
		self->monsterinfo.currentmove = &soldier_move_pain3;
}

/*
=====================================
      NIGHTMARE REBORN
original idea by Lord Dewl [LeD Clan]
 code by Kirk Barnes and Vortex
       Quake2xp team
=====================================
*/
void teleport_effect_think(edict_t *self);
void monster_respawn_effect(edict_t *self)
{
	edict_t	*tele = G_Spawn();

	VectorCopy (self->s.origin, tele->s.origin);
	tele->movetype = MOVETYPE_NONE;
	tele->clipmask = 0;
	tele->solid = SOLID_NOT;
	VectorClear (tele->mins);
	VectorClear (tele->maxs);
	tele->s.modelindex = flyQbe_teleport;
	tele->s.renderfx = RF_ANIM_ONE_CYCLE;	// применять ТОЛЬКО для Berserker-протокола
	tele->s.frame = 0;
	tele->think = teleport_effect_think;
	tele->nextthink = level.time;
	tele->s.sound = 0;
	tele->owner = self;
	gi.linkentity (tele);
	// send muzzle flash
	gi.WriteByte (svc_muzzleflash);
	gi.WriteShort (tele-g_edicts);
	gi.WriteByte (MZ_MONSTER_RESPAWN);	// применять ТОЛЬКО для Berserker-протокола
	gi.multicast (tele->s.origin, MULTICAST_PVS);
}

void monster_respawn(edict_t *self)
{
	KillBox (self);		/// Berserker: разорвать всех кто находится в месте возрождения монстра

	if (net_compatibility->value)
	{	// just send muzzle flash
		gi.WriteByte (svc_muzzleflash);
		gi.WriteShort (self-g_edicts);
		gi.WriteByte (MZ_NUKE8);
		gi.multicast (self->s.origin, MULTICAST_PVS);
	}
	else
		monster_respawn_effect(self);

///	gi.sound (self, CHAN_VOICE, gi.soundindex ("misc/tele_up.wav"), 1, ATTN_NORM, 0);
	self->spawnflags = 0;
	self->monsterinfo.aiflags = 0;
	self->target = NULL;
	self->targetname = NULL;
	self->combattarget = NULL;
	self->deathtarget = NULL;
	self->owner = self;
	ED_CallSpawn (self);
	self->owner = NULL;

	monster_start_go (self);
	FoundTarget (self, NULL);
	level.killed_monsters--;	// fixed by Berserker: монстр воскрес, вычтем из списка убитых
	level.total_monsters--;		// fixed by Berserker: при рождении монстра прибавится +1, а это неверно. Скомпенсируем.
}

void monster_reborn(edict_t *self)
{
///	if (skill->value == 3)
	{
		if (random() <= g_monsterRespawn->value)
		{
			self->think = monster_respawn;
			self->nextthink = level.time + 10 + 10 * random();
		}
	}
}

//
// ATTACK
//

static int blaster_flash [] = {MZ2_SOLDIER_BLASTER_1, MZ2_SOLDIER_BLASTER_2, MZ2_SOLDIER_BLASTER_3, MZ2_SOLDIER_BLASTER_4, MZ2_SOLDIER_BLASTER_5, MZ2_SOLDIER_BLASTER_6, MZ2_SOLDIER_BLASTER_7, MZ2_SOLDIER_BLASTER_8};
static int shotgun_flash [] = {MZ2_SOLDIER_SHOTGUN_1, MZ2_SOLDIER_SHOTGUN_2, MZ2_SOLDIER_SHOTGUN_3, MZ2_SOLDIER_SHOTGUN_4, MZ2_SOLDIER_SHOTGUN_5, MZ2_SOLDIER_SHOTGUN_6, MZ2_SOLDIER_SHOTGUN_7, MZ2_SOLDIER_SHOTGUN_8};
static int machinegun_flash [] = {MZ2_SOLDIER_MACHINEGUN_1, MZ2_SOLDIER_MACHINEGUN_2, MZ2_SOLDIER_MACHINEGUN_3, MZ2_SOLDIER_MACHINEGUN_4, MZ2_SOLDIER_MACHINEGUN_5, MZ2_SOLDIER_MACHINEGUN_6, MZ2_SOLDIER_MACHINEGUN_7, MZ2_SOLDIER_MACHINEGUN_8};

void soldier_fire (edict_t *self, int flash_number)
{
	vec3_t	start;
	vec3_t	forward, right, up;
	vec3_t	aim;
	vec3_t	dir;
	vec3_t	end;
	float	r, u;
	int		flash_index;

	if (self->s.skinnum < 2)
		flash_index = blaster_flash[flash_number];
	else if (self->s.skinnum < 4)
		flash_index = shotgun_flash[flash_number];
	else
		flash_index = machinegun_flash[flash_number];

	AngleVectors (self->s.angles, forward, right, NULL);
	G_ProjectSource (self->s.origin, &monster_flash_offset[flash_index*3], forward, right, start);

	if (flash_number == 5 || flash_number == 6)
	{
		VectorCopy (forward, aim);
	}
	else
	{
		VectorCopy (self->enemy->s.origin, end);
		end[2] += self->enemy->viewheight;
		VectorSubtract (end, start, aim);
		vectoangles (aim, dir);
		AngleVectors (dir, forward, right, up);

		r = crandom()*1000;
		u = crandom()*500;

		VectorMA (start, 8192, forward, end);
		VectorMA (end, r, right, end);
		VectorMA (end, u, up, end);

		VectorSubtract (end, start, aim);
		VectorNormalize (aim);
	}

	if (self->s.skinnum <= 1)
	{
		monster_fire_blaster (self, start, aim, 5, 600, flash_index, EF_BLASTER);
	}
	else if (self->s.skinnum <= 3)
	{
		monster_fire_shotgun (self, start, aim, 2, 1, DEFAULT_SHOTGUN_HSPREAD, DEFAULT_SHOTGUN_VSPREAD, DEFAULT_SHOTGUN_COUNT, flash_index);
	}
	else
	{
		if (!(self->monsterinfo.aiflags & AI_HOLD_FRAME))
			self->monsterinfo.pausetime = level.time + (3 + rand() % 8) * FRAMETIME;

		monster_fire_bullet (self, start, aim, 2, 4, DEFAULT_BULLET_HSPREAD, DEFAULT_BULLET_VSPREAD, flash_index);

		if (level.time >= self->monsterinfo.pausetime)
			self->monsterinfo.aiflags &= ~AI_HOLD_FRAME;
		else
			self->monsterinfo.aiflags |= AI_HOLD_FRAME;
	}
}

// ATTACK1 (blaster/shotgun)

void soldier_fire1 (edict_t *self)
{
	soldier_fire (self, 0);
}

void soldier_attack1_refire1 (edict_t *self)
{
	if (self->s.skinnum > 1)
		return;

	if (self->enemy->health <= 0)
		return;

	if ( ((skill->value == 3) && (random() < 0.5)) || (range(self, self->enemy) == RANGE_MELEE) )
		self->monsterinfo.nextframe = SOLDIER_FRAME_attak102;
	else
		self->monsterinfo.nextframe = SOLDIER_FRAME_attak110;
}

void soldier_attack1_refire2 (edict_t *self)
{
	if (self->s.skinnum < 2)
		return;

	if (self->enemy->health <= 0)
		return;

	if ( ((skill->value == 3) && (random() < 0.5)) || (range(self, self->enemy) == RANGE_MELEE) )
		self->monsterinfo.nextframe = SOLDIER_FRAME_attak102;
}

mframe_t soldier_frames_attack1 [] =
{
	ai_charge, 0,  NULL,
	ai_charge, 0,  NULL,
	ai_charge, 0,  soldier_fire1,
	ai_charge, 0,  NULL,
	ai_charge, 0,  NULL,
	ai_charge, 0,  soldier_attack1_refire1,
	ai_charge, 0,  NULL,
	ai_charge, 0,  soldier_cock,
	ai_charge, 0,  soldier_attack1_refire2,
	ai_charge, 0,  NULL,
	ai_charge, 0,  NULL,
	ai_charge, 0,  NULL
};
mmove_t soldier_move_attack1 = {SOLDIER_FRAME_attak101, SOLDIER_FRAME_attak112, soldier_frames_attack1, soldier_run};

// ATTACK2 (blaster/shotgun)

void soldier_fire2 (edict_t *self)
{
	soldier_fire (self, 1);
}

void soldier_attack2_refire1 (edict_t *self)
{
	if (self->s.skinnum > 1)
		return;

	if (self->enemy->health <= 0)
		return;

	if ( ((skill->value == 3) && (random() < 0.5)) || (range(self, self->enemy) == RANGE_MELEE) )
		self->monsterinfo.nextframe = SOLDIER_FRAME_attak204;
	else
		self->monsterinfo.nextframe = SOLDIER_FRAME_attak216;
}

void soldier_attack2_refire2 (edict_t *self)
{
	if (self->s.skinnum < 2)
		return;

	if (self->enemy->health <= 0)
		return;

	if ( ((skill->value == 3) && (random() < 0.5)) || (range(self, self->enemy) == RANGE_MELEE) )
		self->monsterinfo.nextframe = SOLDIER_FRAME_attak204;
}

mframe_t soldier_frames_attack2 [] =
{
	ai_charge, 0, NULL,
	ai_charge, 0, NULL,
	ai_charge, 0, NULL,
	ai_charge, 0, NULL,
	ai_charge, 0, soldier_fire2,
	ai_charge, 0, NULL,
	ai_charge, 0, NULL,
	ai_charge, 0, soldier_attack2_refire1,
	ai_charge, 0, NULL,
	ai_charge, 0, NULL,
	ai_charge, 0, NULL,
	ai_charge, 0, NULL,
	ai_charge, 0, soldier_cock,
	ai_charge, 0, NULL,
	ai_charge, 0, soldier_attack2_refire2,
	ai_charge, 0, NULL,
	ai_charge, 0, NULL,
	ai_charge, 0, NULL
};
mmove_t soldier_move_attack2 = {SOLDIER_FRAME_attak201, SOLDIER_FRAME_attak218, soldier_frames_attack2, soldier_run};

// ATTACK3 (duck and shoot)

void soldier_duck_down (edict_t *self)
{
	if (self->monsterinfo.aiflags & AI_DUCKED)
		return;
	self->monsterinfo.aiflags |= AI_DUCKED;
	self->maxs[2] -= 32;
	self->takedamage = DAMAGE_YES;
	self->monsterinfo.pausetime = level.time + 1;
	gi.linkentity (self);
}

void soldier_duck_up (edict_t *self)
{
	self->monsterinfo.aiflags &= ~AI_DUCKED;
	self->maxs[2] += 32;
	self->takedamage = DAMAGE_AIM;
	gi.linkentity (self);
}

void soldier_fire3 (edict_t *self)
{
	soldier_duck_down (self);
	soldier_fire (self, 2);
}

void soldier_attack3_refire (edict_t *self)
{
	if ((level.time + 0.4) < self->monsterinfo.pausetime)
		self->monsterinfo.nextframe = SOLDIER_FRAME_attak303;
}

mframe_t soldier_frames_attack3 [] =
{
	ai_charge, 0, NULL,
	ai_charge, 0, NULL,
	ai_charge, 0, soldier_fire3,
	ai_charge, 0, NULL,
	ai_charge, 0, NULL,
	ai_charge, 0, soldier_attack3_refire,
	ai_charge, 0, soldier_duck_up,
	ai_charge, 0, NULL,
	ai_charge, 0, NULL
};
mmove_t soldier_move_attack3 = {SOLDIER_FRAME_attak301, SOLDIER_FRAME_attak309, soldier_frames_attack3, soldier_run};

// ATTACK4 (machinegun)

void soldier_fire4 (edict_t *self)
{
	soldier_fire (self, 3);
//
//	if (self->enemy->health <= 0)
//		return;
//
//	if ( ((skill->value == 3) && (random() < 0.5)) || (range(self, self->enemy) == RANGE_MELEE) )
//		self->monsterinfo.nextframe = SOLDIER_FRAME_attak402;
}

mframe_t soldier_frames_attack4 [] =
{
	ai_charge, 0, NULL,
	ai_charge, 0, NULL,
	ai_charge, 0, soldier_fire4,
	ai_charge, 0, NULL,
	ai_charge, 0, NULL,
	ai_charge, 0, NULL
};
mmove_t soldier_move_attack4 = {SOLDIER_FRAME_attak401, SOLDIER_FRAME_attak406, soldier_frames_attack4, soldier_run};

#if 0
// ATTACK5 (prone)

void soldier_fire5 (edict_t *self)
{
	soldier_fire (self, 4);
}

void soldier_attack5_refire (edict_t *self)
{
	if (self->enemy->health <= 0)
		return;

	if ( ((skill->value == 3) && (random() < 0.5)) || (range(self, self->enemy) == RANGE_MELEE) )
		self->monsterinfo.nextframe = SOLDIER_FRAME_attak505;
}

mframe_t soldier_frames_attack5 [] =
{
	ai_charge, 8, NULL,
	ai_charge, 8, NULL,
	ai_charge, 0, NULL,
	ai_charge, 0, NULL,
	ai_charge, 0, soldier_fire5,
	ai_charge, 0, NULL,
	ai_charge, 0, NULL,
	ai_charge, 0, soldier_attack5_refire
};
mmove_t soldier_move_attack5 = {SOLDIER_FRAME_attak501, SOLDIER_FRAME_attak508, soldier_frames_attack5, soldier_run};
#endif

// ATTACK6 (run & shoot)

void soldier_fire8 (edict_t *self)
{
	soldier_fire (self, 7);
}

void soldier_attack6_refire (edict_t *self)
{
	if (self->enemy->health <= 0)
		return;

	if (range(self, self->enemy) < RANGE_MID)
		return;

	if (skill->value == 3)
		self->monsterinfo.nextframe = SOLDIER_FRAME_runs03;
}

mframe_t soldier_frames_attack6 [] =
{
	ai_charge, 10, NULL,
	ai_charge,  4, NULL,
	ai_charge, 12, NULL,
	ai_charge, 11, soldier_fire8,
	ai_charge, 13, NULL,
	ai_charge, 18, NULL,
	ai_charge, 15, NULL,
	ai_charge, 14, NULL,
	ai_charge, 11, NULL,
	ai_charge,  8, NULL,
	ai_charge, 11, NULL,
	ai_charge, 12, NULL,
	ai_charge, 12, NULL,
	ai_charge, 17, soldier_attack6_refire
};
mmove_t soldier_move_attack6 = {SOLDIER_FRAME_runs01, SOLDIER_FRAME_runs14, soldier_frames_attack6, soldier_run};

void soldier_attack(edict_t *self)
{
	if (self->s.skinnum < 4)
	{
		if (random() < 0.5)
			self->monsterinfo.currentmove = &soldier_move_attack1;
		else
			self->monsterinfo.currentmove = &soldier_move_attack2;
	}
	else
	{
		self->monsterinfo.currentmove = &soldier_move_attack4;
	}
}


//
// SIGHT
//

void soldier_sight(edict_t *self, edict_t *other)
{
	if (random() < 0.5)
		gi.sound (self, CHAN_VOICE, soldierx_sound_sight1, 1, ATTN_NORM, 0);
	else
		gi.sound (self, CHAN_VOICE, soldierx_sound_sight2, 1, ATTN_NORM, 0);

	if ((skill->value > 0) && (range(self, self->enemy) >= RANGE_MID))
	{
		if (random() > 0.5)
			self->monsterinfo.currentmove = &soldier_move_attack6;
	}
}

//
// DUCK
//

void soldier_duck_hold (edict_t *self)
{
	if (level.time >= self->monsterinfo.pausetime)
		self->monsterinfo.aiflags &= ~AI_HOLD_FRAME;
	else
		self->monsterinfo.aiflags |= AI_HOLD_FRAME;
}

mframe_t soldier_frames_duck [] =
{
	ai_move, 5, soldier_duck_down,
	ai_move, -1, soldier_duck_hold,
	ai_move, 1,  NULL,
	ai_move, 0,  soldier_duck_up,
	ai_move, 5,  NULL
};
mmove_t soldier_move_duck = {SOLDIER_FRAME_duck01, SOLDIER_FRAME_duck05, soldier_frames_duck, soldier_run};

void soldier_dodge (edict_t *self, edict_t *attacker, float eta)
{
	float	r;

	r = random();
	if (r > 0.25)
		return;

	if (!self->enemy)
		self->enemy = attacker;

	if (skill->value == 0)
	{
		self->monsterinfo.currentmove = &soldier_move_duck;
		return;
	}

	self->monsterinfo.pausetime = level.time + eta + 0.3;
	r = random();

	if (skill->value == 1)
	{
		if (r > 0.33)
			self->monsterinfo.currentmove = &soldier_move_duck;
		else
			self->monsterinfo.currentmove = &soldier_move_attack3;
		return;
	}

	if (skill->value >= 2)
	{
		if (r > 0.66)
			self->monsterinfo.currentmove = &soldier_move_duck;
		else
			self->monsterinfo.currentmove = &soldier_move_attack3;
		return;
	}

	self->monsterinfo.currentmove = &soldier_move_attack3;
}


//
// DEATH
//

void soldier_fire6 (edict_t *self)
{
	soldier_fire (self, 5);
}

void soldier_fire7 (edict_t *self)
{
	soldier_fire (self, 6);
}

void soldier_dead (edict_t *self)
{
	VectorSet (self->mins, -16, -16, -24);
	VectorSet (self->maxs, 16, 16, -8);
	self->movetype = MOVETYPE_TOSS;
	self->svflags |= SVF_DEADMONSTER;
	self->nextthink = 0;
	gi.linkentity (self);

	M_FlyCheck (self);

	monster_reborn(self);
}

mframe_t soldier_frames_death1 [] =
{
	ai_move, 0,   NULL,
	ai_move, -10, NULL,
	ai_move, -10, NULL,
	ai_move, -10, NULL,
	ai_move, -5,  NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,

	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,

	ai_move, 0,   NULL,
	ai_move, 0,   soldier_fire6,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   soldier_fire7,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,

	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL
};
mmove_t soldier_move_death1 = {SOLDIER_FRAME_death101, SOLDIER_FRAME_death136, soldier_frames_death1, soldier_dead};

mframe_t soldier_frames_death2 [] =
{
	ai_move, -5,  NULL,
	ai_move, -5,  NULL,
	ai_move, -5,  NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,

	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,

	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,

	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL
};
mmove_t soldier_move_death2 = {SOLDIER_FRAME_death201, SOLDIER_FRAME_death235, soldier_frames_death2, soldier_dead};

mframe_t soldier_frames_death3 [] =
{
	ai_move, -5,  NULL,
	ai_move, -5,  NULL,
	ai_move, -5,  NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,

	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,

	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,

	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,

	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
};
mmove_t soldier_move_death3 = {SOLDIER_FRAME_death301, SOLDIER_FRAME_death345, soldier_frames_death3, soldier_dead};

mframe_t soldier_frames_death4 [] =
{
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,

	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,

	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,

	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,

	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,

	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL
};
mmove_t soldier_move_death4 = {SOLDIER_FRAME_death401, SOLDIER_FRAME_death453, soldier_frames_death4, soldier_dead};

mframe_t soldier_frames_death5 [] =
{
	ai_move, -5,  NULL,
	ai_move, -5,  NULL,
	ai_move, -5,  NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,

	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,

	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL
};
mmove_t soldier_move_death5 = {SOLDIER_FRAME_death501, SOLDIER_FRAME_death524, soldier_frames_death5, soldier_dead};

mframe_t soldier_frames_death6 [] =
{
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL
};
mmove_t soldier_move_death6 = {SOLDIER_FRAME_death601, SOLDIER_FRAME_death610, soldier_frames_death6, soldier_dead};

void soldier_die (edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point)
{
	int		n;

// check for gib
	if (self->health <= self->gib_health)
	{
		gi.sound (self, CHAN_VOICE, gi.soundindex ("misc/udeath.wav"), 1, ATTN_NORM, 0);
		if(net_compatibility->value || !net_fixoverflow->value)
		{
			for (n= 0; n < 3; n++)
				ThrowGib (self, "models/objects/gibs/sm_meat/tris.md2", damage, GIB_ORGANIC);
			ThrowGib (self, "models/objects/gibs/chest/tris.md2", damage, GIB_ORGANIC);
			ThrowHead (self, "models/objects/gibs/head2/tris.md2", damage, GIB_ORGANIC);
			self->deadflag = DEAD_DEAD;
		}
		else
		{
			NetThrowMonsterGibsExt (self, GIBS_SOLDIER, damage);
			G_FreeEdict (self);
		}
		return;
	}

	if (self->deadflag == DEAD_DEAD)
		return;

// regular death
	self->deadflag = DEAD_DEAD;
	self->takedamage = DAMAGE_YES;
	self->s.skinnum |= 1;

	if (self->s.skinnum == 1)
		gi.sound (self, CHAN_VOICE, soldier_light_sound_death_light, 1, ATTN_NORM, 0);
	else if (self->s.skinnum == 3)
		gi.sound (self, CHAN_VOICE, soldier_sound_death, 1, ATTN_NORM, 0);
	else // (self->s.skinnum == 5)
		gi.sound (self, CHAN_VOICE, soldier_ss_sound_death_ss, 1, ATTN_NORM, 0);

	if (fabs((self->s.origin[2] + self->viewheight) - point[2]) <= 4)
	{
		// head shot
		self->monsterinfo.currentmove = &soldier_move_death3;
		return;
	}

	n = rand() % 5;
	if (n == 0)
		self->monsterinfo.currentmove = &soldier_move_death1;
	else if (n == 1)
		self->monsterinfo.currentmove = &soldier_move_death2;
	else if (n == 2)
		self->monsterinfo.currentmove = &soldier_move_death4;
	else if (n == 3)
		self->monsterinfo.currentmove = &soldier_move_death5;
	else
		self->monsterinfo.currentmove = &soldier_move_death6;
}


//
// SPAWN
//

void SP_monster_soldier_x (edict_t *self)
{
	self->s.modelindex = gi.modelindex ("models/monsters/soldier/tris.md2");
	self->monsterinfo.scale = SOLDIER_MODEL_SCALE;
	VectorSet (self->mins, -16, -16, -24);
	VectorSet (self->maxs, 16, 16, 32);
	self->movetype = MOVETYPE_STEP;
	self->solid = SOLID_BBOX;

	soldierx_sound_idle =	gi.soundindex ("soldier/solidle1.wav");
	soldierx_sound_sight1 =	gi.soundindex ("soldier/solsght1.wav");
	soldierx_sound_sight2 =	gi.soundindex ("soldier/solsrch1.wav");
	soldierx_sound_cock =	gi.soundindex ("infantry/infatck3.wav");

	self->mass = 100;

	self->pain = soldier_pain;
	self->die = soldier_die;

	self->monsterinfo.stand = soldier_stand;
	self->monsterinfo.walk = soldier_walk;
	self->monsterinfo.run = soldier_run;
	self->monsterinfo.dodge = soldier_dodge;
	self->monsterinfo.attack = soldier_attack;
	self->monsterinfo.melee = NULL;
	self->monsterinfo.sight = soldier_sight;

	gi.linkentity (self);

	self->monsterinfo.stand (self);

	walkmonster_start (self, false);
}


/*QUAKED monster_soldier_light (1 .5 0) (-16 -16 -24) (16 16 32) Ambush Trigger_Spawn Sight
*/
void SP_monster_soldier_light (edict_t *self)
{
	if (deathmatch->value)
	{
		G_FreeEdict (self);
		return;
	}

	SP_monster_soldier_x (self);

	soldier_light_sound_pain_light = gi.soundindex ("soldier/solpain2.wav");
	soldier_light_sound_death_light =	gi.soundindex ("soldier/soldeth2.wav");
	gi.modelindex ("models/objects/laser/tris.md2");
	gi.soundindex ("misc/lasfly.wav");
	gi.soundindex ("soldier/solatck2.wav");

	self->s.skinnum = 0;
	self->health = 20;
	self->gib_health = -30;
}

/*QUAKED monster_soldier (1 .5 0) (-16 -16 -24) (16 16 32) Ambush Trigger_Spawn Sight
*/
void SP_monster_soldier (edict_t *self)
{
	if (deathmatch->value)
	{
		G_FreeEdict (self);
		return;
	}

	SP_monster_soldier_x (self);

	soldier_sound_pain = gi.soundindex ("soldier/solpain1.wav");
	soldier_sound_death = gi.soundindex ("soldier/soldeth1.wav");
	gi.soundindex ("soldier/solatck1.wav");

	self->s.skinnum = 2;
	self->health = 30;
	self->gib_health = -30;
}

/*QUAKED monster_soldier_ss (1 .5 0) (-16 -16 -24) (16 16 32) Ambush Trigger_Spawn Sight
*/
void SP_monster_soldier_ss (edict_t *self)
{
	if (deathmatch->value)
	{
		G_FreeEdict (self);
		return;
	}

	SP_monster_soldier_x (self);

	soldier_ss_sound_pain_ss = gi.soundindex ("soldier/solpain3.wav");
	soldier_ss_sound_death_ss = gi.soundindex ("soldier/soldeth3.wav");
	gi.soundindex ("soldier/solatck3.wav");

	self->s.skinnum = 4;
	self->health = 40;
	self->gib_health = -30;
}

/*
==============================================================================

END of SOLDIER

==============================================================================
*/


/*
==============================================================================

INFANTRY

==============================================================================
*/

void InfantryMachineGun (edict_t *self);
void infantry_run (edict_t *self);

void infantry_swing (edict_t *self)
{
	gi.sound (self, CHAN_WEAPON, infantry_sound_punch_swing, 1, ATTN_NORM, 0);
}

void infantry_smack (edict_t *self)
{
	vec3_t	aim;

	VectorSet (aim, MELEE_DISTANCE, 0, 0);
	if (fire_hit (self, aim, (5 + (rand() % 5)), 50))
		gi.sound (self, CHAN_WEAPON, infantry_sound_punch_hit, 1, ATTN_NORM, 0);
}

mframe_t infantry_frames_attack2 [] =
{
	ai_charge, 3, NULL,
	ai_charge, 6, NULL,
	ai_charge, 0, infantry_swing,
	ai_charge, 8, NULL,
	ai_charge, 5, NULL,
	ai_charge, 8, infantry_smack,
	ai_charge, 6, NULL,
	ai_charge, 3, NULL,
};
mmove_t infantry_move_attack2 = {INFANTRY_FRAME_attak201, INFANTRY_FRAME_attak208, infantry_frames_attack2, infantry_run};


mframe_t infantry_frames_stand [] =
{
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL
};
mmove_t infantry_move_stand = {INFANTRY_FRAME_stand50, INFANTRY_FRAME_stand71, infantry_frames_stand, NULL};

void infantry_stand (edict_t *self)
{
	self->monsterinfo.currentmove = &infantry_move_stand;
}


mframe_t infantry_frames_fidget [] =
{
	ai_stand, 1,  NULL,
	ai_stand, 0,  NULL,
	ai_stand, 1,  NULL,
	ai_stand, 3,  NULL,
	ai_stand, 6,  NULL,
	ai_stand, 3,  NULL,
	ai_stand, 0,  NULL,
	ai_stand, 0,  NULL,
	ai_stand, 0,  NULL,
	ai_stand, 0,  NULL,
	ai_stand, 1,  NULL,
	ai_stand, 0,  NULL,
	ai_stand, 0,  NULL,
	ai_stand, 0,  NULL,
	ai_stand, 0,  NULL,
	ai_stand, 1,  NULL,
	ai_stand, 0,  NULL,
	ai_stand, -1, NULL,
	ai_stand, 0,  NULL,
	ai_stand, 0,  NULL,
	ai_stand, 1,  NULL,
	ai_stand, 0,  NULL,
	ai_stand, -2, NULL,
	ai_stand, 1,  NULL,
	ai_stand, 1,  NULL,
	ai_stand, 1,  NULL,
	ai_stand, -1, NULL,
	ai_stand, 0,  NULL,
	ai_stand, 0,  NULL,
	ai_stand, -1, NULL,
	ai_stand, 0,  NULL,
	ai_stand, 0,  NULL,
	ai_stand, 0,  NULL,
	ai_stand, 0,  NULL,
	ai_stand, 0,  NULL,
	ai_stand, -1, NULL,
	ai_stand, 0,  NULL,
	ai_stand, 0,  NULL,
	ai_stand, 1,  NULL,
	ai_stand, 0,  NULL,
	ai_stand, 0,  NULL,
	ai_stand, -1, NULL,
	ai_stand, -1, NULL,
	ai_stand, 0,  NULL,
	ai_stand, -3, NULL,
	ai_stand, -2, NULL,
	ai_stand, -3, NULL,
	ai_stand, -3, NULL,
	ai_stand, -2, NULL
};
mmove_t infantry_move_fidget = {INFANTRY_FRAME_stand01, INFANTRY_FRAME_stand49, infantry_frames_fidget, infantry_stand};

void infantry_fidget (edict_t *self)
{
	self->monsterinfo.currentmove = &infantry_move_fidget;
	gi.sound (self, CHAN_VOICE, infantry_sound_idle, 1, ATTN_IDLE, 0);
}

mframe_t infantry_frames_walk [] =
{
	ai_walk, 5,  NULL,
	ai_walk, 4,  NULL,
	ai_walk, 4,  NULL,
	ai_walk, 5,  NULL,
	ai_walk, 4,  NULL,
	ai_walk, 5,  NULL,
	ai_walk, 6,  NULL,
	ai_walk, 4,  NULL,
	ai_walk, 4,  NULL,
	ai_walk, 4,  NULL,
	ai_walk, 4,  NULL,
	ai_walk, 5,  NULL
};
mmove_t infantry_move_walk = {INFANTRY_FRAME_walk03, INFANTRY_FRAME_walk14, infantry_frames_walk, NULL};

void infantry_walk (edict_t *self)
{
	self->monsterinfo.currentmove = &infantry_move_walk;
}

mframe_t infantry_frames_run [] =
{
	ai_run, 10, NULL,
	ai_run, 20, NULL,
	ai_run, 5,  NULL,
	ai_run, 7,  NULL,
	ai_run, 30, NULL,
	ai_run, 35, NULL,
	ai_run, 2,  NULL,
	ai_run, 6,  NULL
};
mmove_t infantry_move_run = {INFANTRY_FRAME_run01, INFANTRY_FRAME_run08, infantry_frames_run, NULL};

void infantry_run (edict_t *self)
{
	if (self->monsterinfo.aiflags & AI_STAND_GROUND)
		self->monsterinfo.currentmove = &infantry_move_stand;
	else
		self->monsterinfo.currentmove = &infantry_move_run;
}


mframe_t infantry_frames_pain1 [] =
{
	ai_move, -3, NULL,
	ai_move, -2, NULL,
	ai_move, -1, NULL,
	ai_move, -2, NULL,
	ai_move, -1, NULL,
	ai_move, 1,  NULL,
	ai_move, -1, NULL,
	ai_move, 1,  NULL,
	ai_move, 6,  NULL,
	ai_move, 2,  NULL
};
mmove_t infantry_move_pain1 = {INFANTRY_FRAME_pain101, INFANTRY_FRAME_pain110, infantry_frames_pain1, infantry_run};

mframe_t infantry_frames_pain2 [] =
{
	ai_move, -3, NULL,
	ai_move, -3, NULL,
	ai_move, 0,  NULL,
	ai_move, -1, NULL,
	ai_move, -2, NULL,
	ai_move, 0,  NULL,
	ai_move, 0,  NULL,
	ai_move, 2,  NULL,
	ai_move, 5,  NULL,
	ai_move, 2,  NULL
};
mmove_t infantry_move_pain2 = {INFANTRY_FRAME_pain201, INFANTRY_FRAME_pain210, infantry_frames_pain2, infantry_run};

void infantry_pain (edict_t *self, edict_t *other, float kick, int damage)
{
	int		n;

	if (self->health < (self->max_health / 2))
		self->s.skinnum = 1;

	if (level.time < self->pain_debounce_time)
		return;

	self->pain_debounce_time = level.time + 3;

	if (skill->value == 3)
		return;		// no pain anims in nightmare

	n = rand() % 2;
	if (n == 0)
	{
		self->monsterinfo.currentmove = &infantry_move_pain1;
		gi.sound (self, CHAN_VOICE, infantry_sound_pain1, 1, ATTN_NORM, 0);
	}
	else
	{
		self->monsterinfo.currentmove = &infantry_move_pain2;
		gi.sound (self, CHAN_VOICE, infantry_sound_pain2, 1, ATTN_NORM, 0);
	}
}


vec3_t	aimangles[] =
{
	0.0, 5.0, 0.0,
	10.0, 15.0, 0.0,
	20.0, 25.0, 0.0,
	25.0, 35.0, 0.0,
	30.0, 40.0, 0.0,
	30.0, 45.0, 0.0,
	25.0, 50.0, 0.0,
	20.0, 40.0, 0.0,
	15.0, 35.0, 0.0,
	40.0, 35.0, 0.0,
	70.0, 35.0, 0.0,
	90.0, 35.0, 0.0
};

void InfantryMachineGun (edict_t *self)
{
	vec3_t	start, target;
	vec3_t	forward, right;
	vec3_t	vec;
	int		flash_number;

	if (self->s.frame == INFANTRY_FRAME_attak111)
	{
		flash_number = MZ2_INFANTRY_MACHINEGUN_1;
		AngleVectors (self->s.angles, forward, right, NULL);
		G_ProjectSource (self->s.origin, &monster_flash_offset[flash_number*3], forward, right, start);

		if (self->enemy)
		{
			VectorMA (self->enemy->s.origin, -0.2, self->enemy->velocity, target);
			target[2] += self->enemy->viewheight;
			VectorSubtract (target, start, forward);
			VectorNormalize (forward);
		}
		else
		{
			AngleVectors (self->s.angles, forward, right, NULL);
		}
	}
	else
	{
		flash_number = MZ2_INFANTRY_MACHINEGUN_2 + (self->s.frame - INFANTRY_FRAME_death211);

		AngleVectors (self->s.angles, forward, right, NULL);
		G_ProjectSource (self->s.origin, &monster_flash_offset[flash_number*3], forward, right, start);

		VectorSubtract (self->s.angles, aimangles[flash_number-MZ2_INFANTRY_MACHINEGUN_2], vec);
		AngleVectors (vec, forward, NULL, NULL);
	}

	monster_fire_bullet (self, start, forward, 3, 4, DEFAULT_BULLET_HSPREAD, DEFAULT_BULLET_VSPREAD, flash_number);
}

void infantry_sight (edict_t *self, edict_t *other)
{
	gi.sound (self, CHAN_BODY, infantry_sound_sight, 1, ATTN_NORM, 0);
}

void infantry_dead (edict_t *self)
{
	VectorSet (self->mins, -16, -16, -24);
	VectorSet (self->maxs, 16, 16, -8);
	self->movetype = MOVETYPE_TOSS;
	self->svflags |= SVF_DEADMONSTER;
	gi.linkentity (self);

	M_FlyCheck (self);

	monster_reborn(self);
}

mframe_t infantry_frames_death1 [] =
{
	ai_move, -4, NULL,
	ai_move, 0,  NULL,
	ai_move, 0,  NULL,
	ai_move, -1, NULL,
	ai_move, -4, NULL,
	ai_move, 0,  NULL,
	ai_move, 0,  NULL,
	ai_move, 0,  NULL,
	ai_move, -1, NULL,
	ai_move, 3,  NULL,
	ai_move, 1,  NULL,
	ai_move, 1,  NULL,
	ai_move, -2, NULL,
	ai_move, 2,  NULL,
	ai_move, 2,  NULL,
	ai_move, 9,  NULL,
	ai_move, 9,  NULL,
	ai_move, 5,  NULL,
	ai_move, -3, NULL,
	ai_move, -3, NULL
};
mmove_t infantry_move_death1 = {INFANTRY_FRAME_death101, INFANTRY_FRAME_death120, infantry_frames_death1, infantry_dead};

// Off with his head
mframe_t infantry_frames_death2 [] =
{
	ai_move, 0,   NULL,
	ai_move, 1,   NULL,
	ai_move, 5,   NULL,
	ai_move, -1,  NULL,
	ai_move, 0,   NULL,
	ai_move, 1,   NULL,
	ai_move, 1,   NULL,
	ai_move, 4,   NULL,
	ai_move, 3,   NULL,
	ai_move, 0,   NULL,
	ai_move, -2,  InfantryMachineGun,
	ai_move, -2,  InfantryMachineGun,
	ai_move, -3,  InfantryMachineGun,
	ai_move, -1,  InfantryMachineGun,
	ai_move, -2,  InfantryMachineGun,
	ai_move, 0,   InfantryMachineGun,
	ai_move, 2,   InfantryMachineGun,
	ai_move, 2,   InfantryMachineGun,
	ai_move, 3,   InfantryMachineGun,
	ai_move, -10, InfantryMachineGun,
	ai_move, -7,  InfantryMachineGun,
	ai_move, -8,  InfantryMachineGun,
	ai_move, -6,  NULL,
	ai_move, 4,   NULL,
	ai_move, 0,   NULL
};
mmove_t infantry_move_death2 = {INFANTRY_FRAME_death201, INFANTRY_FRAME_death225, infantry_frames_death2, infantry_dead};

mframe_t infantry_frames_death3 [] =
{
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, -6,  NULL,
	ai_move, -11, NULL,
	ai_move, -3,  NULL,
	ai_move, -11, NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL
};
mmove_t infantry_move_death3 = {INFANTRY_FRAME_death301, INFANTRY_FRAME_death309, infantry_frames_death3, infantry_dead};


void infantry_die (edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point)
{
	int		n;

// check for gib
	if (self->health <= self->gib_health)
	{
		gi.sound (self, CHAN_VOICE, gi.soundindex ("misc/udeath.wav"), 1, ATTN_NORM, 0);
		if(net_compatibility->value || !net_fixoverflow->value)
		{
			for (n= 0; n < 2; n++)
				ThrowGib (self, "models/objects/gibs/bone/tris.md2", damage, GIB_ORGANIC);
			for (n= 0; n < 4; n++)
				ThrowGib (self, "models/objects/gibs/sm_meat/tris.md2", damage, GIB_ORGANIC);
			ThrowHead (self, "models/objects/gibs/head2/tris.md2", damage, GIB_ORGANIC);
			self->deadflag = DEAD_DEAD;
		}
		else
		{
			NetThrowMonsterGibs (self, GIBS_INFANTRY, damage);
			G_FreeEdict (self);
		}
		return;
	}

	if (self->deadflag == DEAD_DEAD)
		return;

// regular death
	self->deadflag = DEAD_DEAD;
	self->takedamage = DAMAGE_YES;

	n = rand() % 3;
	if (n == 0)
	{
		self->monsterinfo.currentmove = &infantry_move_death1;
		gi.sound (self, CHAN_VOICE, infantry_sound_die2, 1, ATTN_NORM, 0);
	}
	else if (n == 1)
	{
		self->monsterinfo.currentmove = &infantry_move_death2;
		gi.sound (self, CHAN_VOICE, infantry_sound_die1, 1, ATTN_NORM, 0);
	}
	else
	{
		self->monsterinfo.currentmove = &infantry_move_death3;
		gi.sound (self, CHAN_VOICE, infantry_sound_die2, 1, ATTN_NORM, 0);
	}
}


void infantry_duck_down (edict_t *self)
{
	if (self->monsterinfo.aiflags & AI_DUCKED)
		return;
	self->monsterinfo.aiflags |= AI_DUCKED;
	self->maxs[2] -= 32;
	self->takedamage = DAMAGE_YES;
	self->monsterinfo.pausetime = level.time + 1;
	gi.linkentity (self);
}

void infantry_duck_hold (edict_t *self)
{
	if (level.time >= self->monsterinfo.pausetime)
		self->monsterinfo.aiflags &= ~AI_HOLD_FRAME;
	else
		self->monsterinfo.aiflags |= AI_HOLD_FRAME;
}

void infantry_duck_up (edict_t *self)
{
	self->monsterinfo.aiflags &= ~AI_DUCKED;
	self->maxs[2] += 32;
	self->takedamage = DAMAGE_AIM;
	gi.linkentity (self);
}

mframe_t infantry_frames_duck [] =
{
	ai_move, -2, infantry_duck_down,
	ai_move, -5, infantry_duck_hold,
	ai_move, 3,  NULL,
	ai_move, 4,  infantry_duck_up,
	ai_move, 0,  NULL
};
mmove_t infantry_move_duck = {INFANTRY_FRAME_duck01, INFANTRY_FRAME_duck05, infantry_frames_duck, infantry_run};

void infantry_dodge (edict_t *self, edict_t *attacker, float eta)
{
	if (random() > 0.25)
		return;

	if (!self->enemy)
		self->enemy = attacker;

	self->monsterinfo.currentmove = &infantry_move_duck;
}


void infantry_cock_gun (edict_t *self)
{
	int		n;

	gi.sound (self, CHAN_WEAPON, infantry_sound_weapon_cock, 1, ATTN_NORM, 0);
	n = (rand() & 15) + 3 + 7;
	self->monsterinfo.pausetime = level.time + n * FRAMETIME;
}

void infantry_fire (edict_t *self)
{
	InfantryMachineGun (self);

	if (level.time >= self->monsterinfo.pausetime)
		self->monsterinfo.aiflags &= ~AI_HOLD_FRAME;
	else
		self->monsterinfo.aiflags |= AI_HOLD_FRAME;
}

mframe_t infantry_frames_attack1 [] =
{
	ai_charge, 4,  NULL,
	ai_charge, -1, NULL,
	ai_charge, -1, NULL,
	ai_charge, 0,  infantry_cock_gun,
	ai_charge, -1, NULL,
	ai_charge, 1,  NULL,
	ai_charge, 1,  NULL,
	ai_charge, 2,  NULL,
	ai_charge, -2, NULL,
	ai_charge, -3, NULL,
	ai_charge, 1,  infantry_fire,
	ai_charge, 5,  NULL,
	ai_charge, -1, NULL,
	ai_charge, -2, NULL,
	ai_charge, -3, NULL
};
mmove_t infantry_move_attack1 = {INFANTRY_FRAME_attak101, INFANTRY_FRAME_attak115, infantry_frames_attack1, infantry_run};


void infantry_attack(edict_t *self)
{
	if (range (self, self->enemy) == RANGE_MELEE)
		self->monsterinfo.currentmove = &infantry_move_attack2;
	else
		self->monsterinfo.currentmove = &infantry_move_attack1;
}


/*QUAKED monster_infantry (1 .5 0) (-16 -16 -24) (16 16 32) Ambush Trigger_Spawn Sight
*/
void SP_monster_infantry (edict_t *self)
{
	if (deathmatch->value)
	{
		G_FreeEdict (self);
		return;
	}

	infantry_sound_pain1 = gi.soundindex ("infantry/infpain1.wav");
	infantry_sound_pain2 = gi.soundindex ("infantry/infpain2.wav");
	infantry_sound_die1 = gi.soundindex ("infantry/infdeth1.wav");
	infantry_sound_die2 = gi.soundindex ("infantry/infdeth2.wav");

	infantry_sound_gunshot = gi.soundindex ("infantry/infatck1.wav");
	infantry_sound_weapon_cock = gi.soundindex ("infantry/infatck3.wav");
	infantry_sound_punch_swing = gi.soundindex ("infantry/infatck2.wav");
	infantry_sound_punch_hit = gi.soundindex ("infantry/melee2.wav");

	infantry_sound_sight = gi.soundindex ("infantry/infsght1.wav");
	infantry_sound_search = gi.soundindex ("infantry/infsrch1.wav");
	infantry_sound_idle = gi.soundindex ("infantry/infidle1.wav");

	self->movetype = MOVETYPE_STEP;
	self->solid = SOLID_BBOX;
	self->s.modelindex = gi.modelindex("models/monsters/infantry/tris.md2");
	VectorSet (self->mins, -16, -16, -24);
	VectorSet (self->maxs, 16, 16, 32);

	self->health = 100;
	self->gib_health = -40;
	self->mass = 200;

	self->pain = infantry_pain;
	self->die = infantry_die;

	self->monsterinfo.stand = infantry_stand;
	self->monsterinfo.walk = infantry_walk;
	self->monsterinfo.run = infantry_run;
	self->monsterinfo.dodge = infantry_dodge;
	self->monsterinfo.attack = infantry_attack;
	self->monsterinfo.melee = NULL;
	self->monsterinfo.sight = infantry_sight;
	self->monsterinfo.idle = infantry_fidget;

	gi.linkentity (self);

	self->monsterinfo.currentmove = &infantry_move_stand;
	self->monsterinfo.scale = INFANTRY_MODEL_SCALE;

	walkmonster_start (self, false);
}

/*
==============================================================================

END by INFANTRY

==============================================================================
*/


/*
==============================================================================

BERSERK

==============================================================================
*/

void berserk_sight (edict_t *self, edict_t *other)
{
	gi.sound (self, CHAN_VOICE, berserk_sound_sight, 1, ATTN_NORM, 0);
}

void berserk_search (edict_t *self)
{
	gi.sound (self, CHAN_VOICE, berserk_sound_search, 1, ATTN_NORM, 0);
}


void berserk_fidget (edict_t *self);
mframe_t berserk_frames_stand [] =
{
	ai_stand, 0, berserk_fidget,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL
};
mmove_t berserk_move_stand = {BERSERK_FRAME_stand1, BERSERK_FRAME_stand5, berserk_frames_stand, NULL};

void berserk_stand (edict_t *self)
{
	self->monsterinfo.currentmove = &berserk_move_stand;
}

mframe_t berserk_frames_stand_fidget [] =
{
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL
};
mmove_t berserk_move_stand_fidget = {BERSERK_FRAME_standb1, BERSERK_FRAME_standb20, berserk_frames_stand_fidget, berserk_stand};

void berserk_fidget (edict_t *self)
{
	if (self->monsterinfo.aiflags & AI_STAND_GROUND)
		return;
	if (random() > 0.15)
		return;

	self->monsterinfo.currentmove = &berserk_move_stand_fidget;
	gi.sound (self, CHAN_WEAPON, berserk_sound_idle, 1, ATTN_IDLE, 0);
}


mframe_t berserk_frames_walk [] =
{
	ai_walk, 9.1, NULL,
	ai_walk, 6.3, NULL,
	ai_walk, 4.9, NULL,
	ai_walk, 6.7, NULL,
	ai_walk, 6.0, NULL,
	ai_walk, 8.2, NULL,
	ai_walk, 7.2, NULL,
	ai_walk, 6.1, NULL,
	ai_walk, 4.9, NULL,
	ai_walk, 4.7, NULL,
	ai_walk, 4.7, NULL,
	ai_walk, 4.8, NULL
};
mmove_t berserk_move_walk = {BERSERK_FRAME_walkc1, BERSERK_FRAME_walkc11, berserk_frames_walk, NULL};

void berserk_walk (edict_t *self)
{
	self->monsterinfo.currentmove = &berserk_move_walk;
}

/*

  *****************************
  SKIPPED THIS FOR NOW!
  *****************************

   Running -> Arm raised in air

void()	berserk_runb1	=[	$r_att1 ,	berserk_runb2	] {ai_run(21);};
void()	berserk_runb2	=[	$r_att2 ,	berserk_runb3	] {ai_run(11);};
void()	berserk_runb3	=[	$r_att3 ,	berserk_runb4	] {ai_run(21);};
void()	berserk_runb4	=[	$r_att4 ,	berserk_runb5	] {ai_run(25);};
void()	berserk_runb5	=[	$r_att5 ,	berserk_runb6	] {ai_run(18);};
void()	berserk_runb6	=[	$r_att6 ,	berserk_runb7	] {ai_run(19);};
// running with arm in air : start loop
void()	berserk_runb7	=[	$r_att7 ,	berserk_runb8	] {ai_run(21);};
void()	berserk_runb8	=[	$r_att8 ,	berserk_runb9	] {ai_run(11);};
void()	berserk_runb9	=[	$r_att9 ,	berserk_runb10	] {ai_run(21);};
void()	berserk_runb10	=[	$r_att10 ,	berserk_runb11	] {ai_run(25);};
void()	berserk_runb11	=[	$r_att11 ,	berserk_runb12	] {ai_run(18);};
void()	berserk_runb12	=[	$r_att12 ,	berserk_runb7	] {ai_run(19);};
// running with arm in air : end loop
*/


mframe_t berserk_frames_run1 [] =
{
	ai_run, 21, NULL,
	ai_run, 11, NULL,
	ai_run, 21, NULL,
	ai_run, 25, NULL,
	ai_run, 18, NULL,
	ai_run, 19, NULL
};
mmove_t berserk_move_run1 = {BERSERK_FRAME_run1, BERSERK_FRAME_run6, berserk_frames_run1, NULL};

void berserk_run (edict_t *self)
{
	if (self->monsterinfo.aiflags & AI_STAND_GROUND)
		self->monsterinfo.currentmove = &berserk_move_stand;
	else
		self->monsterinfo.currentmove = &berserk_move_run1;
}


void berserk_attack_spike (edict_t *self)
{
	static	vec3_t	aim = {MELEE_DISTANCE, 0, -24};
	fire_hit (self, aim, (15 + (rand() % 6)), 400);		//	Faster attack -- upwards and backwards			Berserker: у меня почему-то было 150, а не 15  8-(
}


void berserk_swing (edict_t *self)
{
	gi.sound (self, CHAN_WEAPON, berserk_sound_punch, 1, ATTN_NORM, 0);
}

mframe_t berserk_frames_attack_spike [] =
{
		ai_charge, 0, NULL,
		ai_charge, 0, NULL,
		ai_charge, 0, berserk_swing,
		ai_charge, 0, berserk_attack_spike,
		ai_charge, 0, NULL,
		ai_charge, 0, NULL,
		ai_charge, 0, NULL,
		ai_charge, 0, NULL
};
mmove_t berserk_move_attack_spike = {BERSERK_FRAME_att_c1, BERSERK_FRAME_att_c8, berserk_frames_attack_spike, berserk_run};


void berserk_attack_club (edict_t *self)
{
	vec3_t	aim;

	VectorSet (aim, MELEE_DISTANCE, self->mins[0], -4);
	fire_hit (self, aim, (5 + (rand() % 6)), 400);		// Slower attack			Berserker: у меня почему-то было 50, а не 5  8-(
}

mframe_t berserk_frames_attack_club [] =
{
	ai_charge, 0, NULL,
	ai_charge, 0, NULL,
	ai_charge, 0, NULL,
	ai_charge, 0, NULL,
	ai_charge, 0, berserk_swing,
	ai_charge, 0, NULL,
	ai_charge, 0, NULL,
	ai_charge, 0, NULL,
	ai_charge, 0, berserk_attack_club,
	ai_charge, 0, NULL,
	ai_charge, 0, NULL,
	ai_charge, 0, NULL
};
mmove_t berserk_move_attack_club = {BERSERK_FRAME_att_c9, BERSERK_FRAME_att_c20, berserk_frames_attack_club, berserk_run};


void berserk_strike (edict_t *self)
{
	//FIXME play impact sound
}


mframe_t berserk_frames_attack_strike [] =
{
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, berserk_swing,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, berserk_strike,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 9.7, NULL,
	ai_move, 13.6, NULL
};

mmove_t berserk_move_attack_strike = {BERSERK_FRAME_att_c21, BERSERK_FRAME_att_c34, berserk_frames_attack_strike, berserk_run};


void berserk_melee (edict_t *self)
{
	if ((rand() % 2) == 0)
		self->monsterinfo.currentmove = &berserk_move_attack_spike;
	else
		self->monsterinfo.currentmove = &berserk_move_attack_club;
}


/*
void() 	berserk_atke1	=[	$r_attb1,	berserk_atke2	] {ai_run(9);};
void() 	berserk_atke2	=[	$r_attb2,	berserk_atke3	] {ai_run(6);};
void() 	berserk_atke3	=[	$r_attb3,	berserk_atke4	] {ai_run(18.4);};
void() 	berserk_atke4	=[	$r_attb4,	berserk_atke5	] {ai_run(25);};
void() 	berserk_atke5	=[	$r_attb5,	berserk_atke6	] {ai_run(14);};
void() 	berserk_atke6	=[	$r_attb6,	berserk_atke7	] {ai_run(20);};
void() 	berserk_atke7	=[	$r_attb7,	berserk_atke8	] {ai_run(8.5);};
void() 	berserk_atke8	=[	$r_attb8,	berserk_atke9	] {ai_run(3);};
void() 	berserk_atke9	=[	$r_attb9,	berserk_atke10	] {ai_run(17.5);};
void() 	berserk_atke10	=[	$r_attb10,	berserk_atke11	] {ai_run(17);};
void() 	berserk_atke11	=[	$r_attb11,	berserk_atke12	] {ai_run(9);};
void() 	berserk_atke12	=[	$r_attb12,	berserk_atke13	] {ai_run(25);};
void() 	berserk_atke13	=[	$r_attb13,	berserk_atke14	] {ai_run(3.7);};
void() 	berserk_atke14	=[	$r_attb14,	berserk_atke15	] {ai_run(2.6);};
void() 	berserk_atke15	=[	$r_attb15,	berserk_atke16	] {ai_run(19);};
void() 	berserk_atke16	=[	$r_attb16,	berserk_atke17	] {ai_run(25);};
void() 	berserk_atke17	=[	$r_attb17,	berserk_atke18	] {ai_run(19.6);};
void() 	berserk_atke18	=[	$r_attb18,	berserk_run1	] {ai_run(7.8);};
*/


mframe_t berserk_frames_pain1 [] =
{
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL
};
mmove_t berserk_move_pain1 = {BERSERK_FRAME_painc1, BERSERK_FRAME_painc4, berserk_frames_pain1, berserk_run};


mframe_t berserk_frames_pain2 [] =
{
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL
};
mmove_t berserk_move_pain2 = {BERSERK_FRAME_painb1, BERSERK_FRAME_painb20, berserk_frames_pain2, berserk_run};

void berserk_pain (edict_t *self, edict_t *other, float kick, int damage)
{
	if (self->health < (self->max_health / 2))
		self->s.skinnum = 1;

	if (level.time < self->pain_debounce_time)
		return;

	self->pain_debounce_time = level.time + 3;
	gi.sound (self, CHAN_VOICE, berserk_sound_pain, 1, ATTN_NORM, 0);

	if (skill->value == 3)
		return;		// no pain anims in nightmare

	if ((damage < 20) || (random() < 0.5))
		self->monsterinfo.currentmove = &berserk_move_pain1;
	else
		self->monsterinfo.currentmove = &berserk_move_pain2;
}


void berserk_dead (edict_t *self)
{
	VectorSet (self->mins, -16, -16, -24);
	VectorSet (self->maxs, 16, 16, -8);
	self->movetype = MOVETYPE_TOSS;
	self->svflags |= SVF_DEADMONSTER;
	self->nextthink = 0;
	gi.linkentity (self);

	M_FlyCheck (self);

	monster_reborn(self);
}


mframe_t berserk_frames_death1 [] =
{
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL

};
mmove_t berserk_move_death1 = {BERSERK_FRAME_death1, BERSERK_FRAME_death13, berserk_frames_death1, berserk_dead};


mframe_t berserk_frames_death2 [] =
{
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL
};
mmove_t berserk_move_death2 = {BERSERK_FRAME_deathc1, BERSERK_FRAME_deathc8, berserk_frames_death2, berserk_dead};


void berserk_die (edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point)
{
	int		n;

	if (self->health <= self->gib_health)
	{
		gi.sound (self, CHAN_VOICE, gi.soundindex ("misc/udeath.wav"), 1, ATTN_NORM, 0);
		if(net_compatibility->value || !net_fixoverflow->value)
		{
			for (n= 0; n < 2; n++)
				ThrowGib (self, "models/objects/gibs/bone/tris.md2", damage, GIB_ORGANIC);
			for (n= 0; n < 4; n++)
				ThrowGib (self, "models/objects/gibs/sm_meat/tris.md2", damage, GIB_ORGANIC);
			ThrowHead (self, "models/objects/gibs/head2/tris.md2", damage, GIB_ORGANIC);
			self->deadflag = DEAD_DEAD;
		}
		else
		{
			NetThrowMonsterGibs (self, GIBS_BERSERK, damage);
			G_FreeEdict (self);
		}
		return;
	}

	if (self->deadflag == DEAD_DEAD)
		return;

	gi.sound (self, CHAN_VOICE, berserk_sound_die, 1, ATTN_NORM, 0);
	self->deadflag = DEAD_DEAD;
	self->takedamage = DAMAGE_YES;

	if (damage >= 50)
		self->monsterinfo.currentmove = &berserk_move_death1;
	else
		self->monsterinfo.currentmove = &berserk_move_death2;
}


/*QUAKED monster_berserk (1 .5 0) (-16 -16 -24) (16 16 32) Ambush Trigger_Spawn Sight
*/
void SP_monster_berserk (edict_t *self)
{
	if (deathmatch->value)
	{
		G_FreeEdict (self);
		return;
	}

	// pre-caches
	berserk_sound_pain  = gi.soundindex ("berserk/berpain2.wav");
	berserk_sound_die   = gi.soundindex ("berserk/berdeth2.wav");
	berserk_sound_idle  = gi.soundindex ("berserk/beridle1.wav");
	berserk_sound_punch = gi.soundindex ("berserk/attack.wav");
	berserk_sound_search = gi.soundindex ("berserk/bersrch1.wav");
	berserk_sound_sight = gi.soundindex ("berserk/sight.wav");

	self->s.modelindex = gi.modelindex("models/monsters/berserk/tris.md2");
	VectorSet (self->mins, -16, -16, -24);
	VectorSet (self->maxs, 16, 16, 32);
	self->movetype = MOVETYPE_STEP;
	self->solid = SOLID_BBOX;

	self->health = 240;
	self->gib_health = -60;
	self->mass = 250;

	self->pain = berserk_pain;
	self->die = berserk_die;

	self->monsterinfo.stand = berserk_stand;
	self->monsterinfo.walk = berserk_walk;
	self->monsterinfo.run = berserk_run;
	self->monsterinfo.dodge = NULL;
	self->monsterinfo.attack = NULL;
	self->monsterinfo.melee = berserk_melee;
	self->monsterinfo.sight = berserk_sight;
	self->monsterinfo.search = berserk_search;

	self->monsterinfo.currentmove = &berserk_move_stand;
	self->monsterinfo.scale = BERSERK_MODEL_SCALE;

	gi.linkentity (self);

	walkmonster_start (self, false);
}

/*
==============================================================================

END of BERSERK

==============================================================================
*/


/*
==============================================================================

GLADIATOR

==============================================================================
*/

void gladiator_idle (edict_t *self)
{
	gi.sound (self, CHAN_VOICE, gladiator_sound_idle, 1, ATTN_IDLE, 0);
}

void gladiator_sight (edict_t *self, edict_t *other)
{
	gi.sound (self, CHAN_VOICE, gladiator_sound_sight, 1, ATTN_NORM, 0);
}

void gladiator_search (edict_t *self)
{
	gi.sound (self, CHAN_VOICE, gladiator_sound_search, 1, ATTN_NORM, 0);
}

void gladiator_cleaver_swing (edict_t *self)
{
	gi.sound (self, CHAN_WEAPON, gladiator_sound_cleaver_swing, 1, ATTN_NORM, 0);
}

mframe_t gladiator_frames_stand [] =
{
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL
};
mmove_t gladiator_move_stand = {GLADIATOR_FRAME_stand1, GLADIATOR_FRAME_stand7, gladiator_frames_stand, NULL};

void gladiator_stand (edict_t *self)
{
	self->monsterinfo.currentmove = &gladiator_move_stand;
}


mframe_t gladiator_frames_walk [] =
{
	ai_walk, 15, NULL,
	ai_walk, 7,  NULL,
	ai_walk, 6,  NULL,
	ai_walk, 5,  NULL,
	ai_walk, 2,  NULL,
	ai_walk, 0,  NULL,
	ai_walk, 2,  NULL,
	ai_walk, 8,  NULL,
	ai_walk, 12, NULL,
	ai_walk, 8,  NULL,
	ai_walk, 5,  NULL,
	ai_walk, 5,  NULL,
	ai_walk, 2,  NULL,
	ai_walk, 2,  NULL,
	ai_walk, 1,  NULL,
	ai_walk, 8,  NULL
};
mmove_t gladiator_move_walk = {GLADIATOR_FRAME_walk1, GLADIATOR_FRAME_walk16, gladiator_frames_walk, NULL};

void gladiator_walk (edict_t *self)
{
	self->monsterinfo.currentmove = &gladiator_move_walk;
}


mframe_t gladiator_frames_run [] =
{
	ai_run, 23,	NULL,
	ai_run, 14,	NULL,
	ai_run, 14,	NULL,
	ai_run, 21,	NULL,
	ai_run, 12,	NULL,
	ai_run, 13,	NULL
};
mmove_t gladiator_move_run = {GLADIATOR_FRAME_run1, GLADIATOR_FRAME_run6, gladiator_frames_run, NULL};

void gladiator_run (edict_t *self)
{
	if (self->monsterinfo.aiflags & AI_STAND_GROUND)
		self->monsterinfo.currentmove = &gladiator_move_stand;
	else
		self->monsterinfo.currentmove = &gladiator_move_run;
}


void GaldiatorMelee (edict_t *self)
{
	vec3_t	aim;

	VectorSet (aim, MELEE_DISTANCE, self->mins[0], -4);
	if (fire_hit (self, aim, (20 + (rand() %5)), 300))
		gi.sound (self, CHAN_AUTO, gladiator_sound_cleaver_hit, 1, ATTN_NORM, 0);
	else
		gi.sound (self, CHAN_AUTO, gladiator_sound_cleaver_miss, 1, ATTN_NORM, 0);
}

mframe_t gladiator_frames_attack_melee [] =
{
	ai_charge, 0, NULL,
	ai_charge, 0, NULL,
	ai_charge, 0, NULL,
	ai_charge, 0, NULL,
	ai_charge, 0, gladiator_cleaver_swing,
	ai_charge, 0, NULL,
	ai_charge, 0, GaldiatorMelee,
	ai_charge, 0, NULL,
	ai_charge, 0, NULL,
	ai_charge, 0, NULL,
	ai_charge, 0, gladiator_cleaver_swing,
	ai_charge, 0, NULL,
	ai_charge, 0, NULL,
	ai_charge, 0, GaldiatorMelee,
	ai_charge, 0, NULL,
	ai_charge, 0, NULL,
	ai_charge, 0, NULL
};
mmove_t gladiator_move_attack_melee = {GLADIATOR_FRAME_melee1, GLADIATOR_FRAME_melee17, gladiator_frames_attack_melee, gladiator_run};

void gladiator_melee(edict_t *self)
{
	self->monsterinfo.currentmove = &gladiator_move_attack_melee;
}


void GladiatorGun (edict_t *self)
{
	vec3_t	start;
	vec3_t	dir;
	vec3_t	forward, right;

	AngleVectors (self->s.angles, forward, right, NULL);
	G_ProjectSource (self->s.origin, &monster_flash_offset[MZ2_GLADIATOR_RAILGUN_1*3], forward, right, start);

	// calc direction to where we targted
	VectorSubtract (self->pos1, start, dir);
	VectorNormalize (dir);

	monster_fire_railgun (self, start, dir, 50, 100, MZ2_GLADIATOR_RAILGUN_1);
}

mframe_t gladiator_frames_attack_gun [] =
{
	ai_charge, 0, NULL,
	ai_charge, 0, NULL,
	ai_charge, 0, NULL,
	ai_charge, 0, GladiatorGun,
	ai_charge, 0, NULL,
	ai_charge, 0, NULL,
	ai_charge, 0, NULL,
	ai_charge, 0, NULL,
	ai_charge, 0, NULL
};
mmove_t gladiator_move_attack_gun = {GLADIATOR_FRAME_attack1, GLADIATOR_FRAME_attack9, gladiator_frames_attack_gun, gladiator_run};

void gladiator_attack(edict_t *self)
{
	float	range;
	vec3_t	v;

	// a small safe zone
	VectorSubtract (self->s.origin, self->enemy->s.origin, v);
	range = VectorLength(v);
	if (range <= (MELEE_DISTANCE + 32))
		return;

	// charge up the railgun
	gi.sound (self, CHAN_WEAPON, gladiator_sound_gun, 1, ATTN_NORM, 0);
	VectorCopy (self->enemy->s.origin, self->pos1);	//save for aiming the shot
	self->pos1[2] += self->enemy->viewheight;
	self->monsterinfo.currentmove = &gladiator_move_attack_gun;
}


mframe_t gladiator_frames_pain [] =
{
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL
};
mmove_t gladiator_move_pain = {GLADIATOR_FRAME_pain1, GLADIATOR_FRAME_pain6, gladiator_frames_pain, gladiator_run};

mframe_t gladiator_frames_pain_air [] =
{
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL
};
mmove_t gladiator_move_pain_air = {GLADIATOR_FRAME_painup1, GLADIATOR_FRAME_painup7, gladiator_frames_pain_air, gladiator_run};

void gladiator_pain (edict_t *self, edict_t *other, float kick, int damage)
{

	if (self->health < (self->max_health / 2))
		self->s.skinnum = 1;

	if (level.time < self->pain_debounce_time)
	{
		if ((self->velocity[2] > 100) && (self->monsterinfo.currentmove == &gladiator_move_pain))
			self->monsterinfo.currentmove = &gladiator_move_pain_air;
		return;
	}

	self->pain_debounce_time = level.time + 3;

	if (random() < 0.5)
		gi.sound (self, CHAN_VOICE, gladiator_sound_pain1, 1, ATTN_NORM, 0);
	else
		gi.sound (self, CHAN_VOICE, gladiator_sound_pain2, 1, ATTN_NORM, 0);

	if (skill->value == 3)
		return;		// no pain anims in nightmare

	if (self->velocity[2] > 100)
		self->monsterinfo.currentmove = &gladiator_move_pain_air;
	else
		self->monsterinfo.currentmove = &gladiator_move_pain;

}


void gladiator_dead (edict_t *self)
{
	VectorSet (self->mins, -16, -16, -24);
	VectorSet (self->maxs, 16, 16, -8);
	self->movetype = MOVETYPE_TOSS;
	self->svflags |= SVF_DEADMONSTER;
	self->nextthink = 0;
	gi.linkentity (self);

	M_FlyCheck (self);

	monster_reborn(self);
}

mframe_t gladiator_frames_death [] =
{
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL
};
mmove_t gladiator_move_death = {GLADIATOR_FRAME_death1, GLADIATOR_FRAME_death22, gladiator_frames_death, gladiator_dead};

void gladiator_die (edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point)
{
	int		n;

// check for gib
	if (self->health <= self->gib_health)
	{
		gi.sound (self, CHAN_VOICE, gi.soundindex ("misc/udeath.wav"), 1, ATTN_NORM, 0);
		if(net_compatibility->value || !net_fixoverflow->value)
		{
			for (n= 0; n < 2; n++)
				ThrowGib (self, "models/objects/gibs/bone/tris.md2", damage, GIB_ORGANIC);
			for (n= 0; n < 4; n++)
				ThrowGib (self, "models/objects/gibs/sm_meat/tris.md2", damage, GIB_ORGANIC);
			ThrowHead (self, "models/objects/gibs/head2/tris.md2", damage, GIB_ORGANIC);
			self->deadflag = DEAD_DEAD;
		}
		else
		{
			NetThrowMonsterGibs (self, GIBS_GLADIATOR, damage);
			G_FreeEdict (self);
		}
		return;
	}

	if (self->deadflag == DEAD_DEAD)
		return;

// regular death
	gi.sound (self, CHAN_VOICE, gladiator_sound_die, 1, ATTN_NORM, 0);
	self->deadflag = DEAD_DEAD;
	self->takedamage = DAMAGE_YES;

	self->monsterinfo.currentmove = &gladiator_move_death;
}


/*QUAKED monster_gladiator (1 .5 0) (-32 -32 -24) (32 32 64) Ambush Trigger_Spawn Sight
*/
void SP_monster_gladiator (edict_t *self)
{
	if (deathmatch->value)
	{
		G_FreeEdict (self);
		return;
	}


	gladiator_sound_pain1 = gi.soundindex ("gladiator/pain.wav");
	gladiator_sound_pain2 = gi.soundindex ("gladiator/gldpain2.wav");
	gladiator_sound_die = gi.soundindex ("gladiator/glddeth2.wav");
	gladiator_sound_gun = gi.soundindex ("gladiator/railgun.wav");
	gladiator_sound_cleaver_swing = gi.soundindex ("gladiator/melee1.wav");
	gladiator_sound_cleaver_hit = gi.soundindex ("gladiator/melee2.wav");
	gladiator_sound_cleaver_miss = gi.soundindex ("gladiator/melee3.wav");
	gladiator_sound_idle = gi.soundindex ("gladiator/gldidle1.wav");
	gladiator_sound_search = gi.soundindex ("gladiator/gldsrch1.wav");
	gladiator_sound_sight = gi.soundindex ("gladiator/sight.wav");

	self->movetype = MOVETYPE_STEP;
	self->solid = SOLID_BBOX;
	self->s.modelindex = gi.modelindex ("models/monsters/gladiatr/tris.md2");
	VectorSet (self->mins, -32, -32, -24);
	VectorSet (self->maxs, 32, 32, 64);

	self->health = 400;
	self->gib_health = -175;
	self->mass = 400;

	self->pain = gladiator_pain;
	self->die = gladiator_die;

	self->monsterinfo.stand = gladiator_stand;
	self->monsterinfo.walk = gladiator_walk;
	self->monsterinfo.run = gladiator_run;
	self->monsterinfo.dodge = NULL;
	self->monsterinfo.attack = gladiator_attack;
	self->monsterinfo.melee = gladiator_melee;
	self->monsterinfo.sight = gladiator_sight;
	self->monsterinfo.idle = gladiator_idle;
	self->monsterinfo.search = gladiator_search;

	gi.linkentity (self);
	self->monsterinfo.currentmove = &gladiator_move_stand;
	self->monsterinfo.scale = GLADIATOR_MODEL_SCALE;

	walkmonster_start (self, false);
}

/*
==============================================================================

END of GLADIATOR

==============================================================================
*/


/*
==============================================================================

GUNNER

==============================================================================
*/

void gunner_idlesound (edict_t *self)
{
	gi.sound (self, CHAN_VOICE, gunner_sound_idle, 1, ATTN_IDLE, 0);
}

void gunner_sight (edict_t *self, edict_t *other)
{
	gi.sound (self, CHAN_VOICE, gunner_sound_sight, 1, ATTN_NORM, 0);
}

void gunner_search (edict_t *self)
{
	gi.sound (self, CHAN_VOICE, gunner_sound_search, 1, ATTN_NORM, 0);
}


void GunnerGrenade (edict_t *self);
void GunnerFire (edict_t *self);
void gunner_fire_chain(edict_t *self);
void gunner_refire_chain(edict_t *self);


void gunner_stand (edict_t *self);

mframe_t gunner_frames_fidget [] =
{
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, gunner_idlesound,
	ai_stand, 0, NULL,

	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,

	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,

	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,

	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL
};
mmove_t	gunner_move_fidget = {GUNNER_FRAME_stand31, GUNNER_FRAME_stand70, gunner_frames_fidget, gunner_stand};

void gunner_fidget (edict_t *self)
{
	if (self->monsterinfo.aiflags & AI_STAND_GROUND)
		return;
	if (random() <= 0.05)
		self->monsterinfo.currentmove = &gunner_move_fidget;
}

mframe_t gunner_frames_stand [] =
{
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, gunner_fidget,

	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, gunner_fidget,

	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, gunner_fidget
};
mmove_t	gunner_move_stand = {GUNNER_FRAME_stand01, GUNNER_FRAME_stand30, gunner_frames_stand, NULL};

void gunner_stand (edict_t *self)
{
		self->monsterinfo.currentmove = &gunner_move_stand;
}


mframe_t gunner_frames_walk [] =
{
	ai_walk, 0, NULL,
	ai_walk, 3, NULL,
	ai_walk, 4, NULL,
	ai_walk, 5, NULL,
	ai_walk, 7, NULL,
	ai_walk, 2, NULL,
	ai_walk, 6, NULL,
	ai_walk, 4, NULL,
	ai_walk, 2, NULL,
	ai_walk, 7, NULL,
	ai_walk, 5, NULL,
	ai_walk, 7, NULL,
	ai_walk, 4, NULL
};
mmove_t gunner_move_walk = {GUNNER_FRAME_walk07, GUNNER_FRAME_walk19, gunner_frames_walk, NULL};

void gunner_walk (edict_t *self)
{
	self->monsterinfo.currentmove = &gunner_move_walk;
}

mframe_t gunner_frames_run [] =
{
	ai_run, 26, NULL,
	ai_run, 9,  NULL,
	ai_run, 9,  NULL,
	ai_run, 9,  NULL,
	ai_run, 15, NULL,
	ai_run, 10, NULL,
	ai_run, 13, NULL,
	ai_run, 6,  NULL
};

mmove_t gunner_move_run = {GUNNER_FRAME_run01, GUNNER_FRAME_run08, gunner_frames_run, NULL};

void gunner_run (edict_t *self)
{
	if (self->monsterinfo.aiflags & AI_STAND_GROUND)
		self->monsterinfo.currentmove = &gunner_move_stand;
	else
		self->monsterinfo.currentmove = &gunner_move_run;
}

mframe_t gunner_frames_runandshoot [] =
{
	ai_run, 32, NULL,
	ai_run, 15, NULL,
	ai_run, 10, NULL,
	ai_run, 18, NULL,
	ai_run, 8,  NULL,
	ai_run, 20, NULL
};

mmove_t gunner_move_runandshoot = {GUNNER_FRAME_runs01, GUNNER_FRAME_runs06, gunner_frames_runandshoot, NULL};

void gunner_runandshoot (edict_t *self)
{
	self->monsterinfo.currentmove = &gunner_move_runandshoot;
}

mframe_t gunner_frames_pain3 [] =
{
	ai_move, -3, NULL,
	ai_move, 1,	 NULL,
	ai_move, 1,	 NULL,
	ai_move, 0,	 NULL,
	ai_move, 1,	 NULL
};
mmove_t gunner_move_pain3 = {GUNNER_FRAME_pain301, GUNNER_FRAME_pain305, gunner_frames_pain3, gunner_run};

mframe_t gunner_frames_pain2 [] =
{
	ai_move, -2, NULL,
	ai_move, 11, NULL,
	ai_move, 6,	 NULL,
	ai_move, 2,	 NULL,
	ai_move, -1, NULL,
	ai_move, -7, NULL,
	ai_move, -2, NULL,
	ai_move, -7, NULL
};
mmove_t gunner_move_pain2 = {GUNNER_FRAME_pain201, GUNNER_FRAME_pain208, gunner_frames_pain2, gunner_run};

mframe_t gunner_frames_pain1 [] =
{
	ai_move, 2,	 NULL,
	ai_move, 0,	 NULL,
	ai_move, -5, NULL,
	ai_move, 3,	 NULL,
	ai_move, -1, NULL,
	ai_move, 0,	 NULL,
	ai_move, 0,	 NULL,
	ai_move, 0,	 NULL,
	ai_move, 0,	 NULL,
	ai_move, 1,	 NULL,
	ai_move, 1,	 NULL,
	ai_move, 2,	 NULL,
	ai_move, 1,	 NULL,
	ai_move, 0,	 NULL,
	ai_move, -2, NULL,
	ai_move, -2, NULL,
	ai_move, 0,	 NULL,
	ai_move, 0,	 NULL
};
mmove_t gunner_move_pain1 = {GUNNER_FRAME_pain101, GUNNER_FRAME_pain118, gunner_frames_pain1, gunner_run};

void gunner_pain (edict_t *self, edict_t *other, float kick, int damage)
{
	if (self->health < (self->max_health / 2))
		self->s.skinnum = 1;

	if (level.time < self->pain_debounce_time)
		return;

	self->pain_debounce_time = level.time + 3;

	if (rand()&1)
		gi.sound (self, CHAN_VOICE, gunner_sound_pain, 1, ATTN_NORM, 0);
	else
		gi.sound (self, CHAN_VOICE, gunner_sound_pain2, 1, ATTN_NORM, 0);

	if (skill->value == 3)
		return;		// no pain anims in nightmare

	if (damage <= 10)
		self->monsterinfo.currentmove = &gunner_move_pain3;
	else if (damage <= 25)
		self->monsterinfo.currentmove = &gunner_move_pain2;
	else
		self->monsterinfo.currentmove = &gunner_move_pain1;
}

void gunner_dead (edict_t *self)
{
	VectorSet (self->mins, -16, -16, -24);
	VectorSet (self->maxs, 16, 16, -8);
	self->movetype = MOVETYPE_TOSS;
	self->svflags |= SVF_DEADMONSTER;
	self->nextthink = 0;
	gi.linkentity (self);

	M_FlyCheck (self);

	monster_reborn(self);
}

mframe_t gunner_frames_death [] =
{
	ai_move, 0,	 NULL,
	ai_move, 0,	 NULL,
	ai_move, 0,	 NULL,
	ai_move, -7, NULL,
	ai_move, -3, NULL,
	ai_move, -5, NULL,
	ai_move, 8,	 NULL,
	ai_move, 6,	 NULL,
	ai_move, 0,	 NULL,
	ai_move, 0,	 NULL,
	ai_move, 0,	 NULL
};
mmove_t gunner_move_death = {GUNNER_FRAME_death01, GUNNER_FRAME_death11, gunner_frames_death, gunner_dead};

void gunner_die (edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point)
{
	int		n;

// check for gib
	if (self->health <= self->gib_health)
	{
		gi.sound (self, CHAN_VOICE, gi.soundindex ("misc/udeath.wav"), 1, ATTN_NORM, 0);
		if(net_compatibility->value || !net_fixoverflow->value)
		{
			for (n= 0; n < 2; n++)
				ThrowGib (self, "models/objects/gibs/bone/tris.md2", damage, GIB_ORGANIC);
			for (n= 0; n < 4; n++)
				ThrowGib (self, "models/objects/gibs/sm_meat/tris.md2", damage, GIB_ORGANIC);
			ThrowHead (self, "models/objects/gibs/head2/tris.md2", damage, GIB_ORGANIC);
			self->deadflag = DEAD_DEAD;
		}
		else
		{
			NetThrowMonsterGibs (self, GIBS_GUNNER, damage);
			G_FreeEdict (self);
		}
		return;
	}

	if (self->deadflag == DEAD_DEAD)
		return;

// regular death
	gi.sound (self, CHAN_VOICE, gunner_sound_death, 1, ATTN_NORM, 0);
	self->deadflag = DEAD_DEAD;
	self->takedamage = DAMAGE_YES;
	self->monsterinfo.currentmove = &gunner_move_death;
}


void gunner_duck_down (edict_t *self)
{
	if (self->monsterinfo.aiflags & AI_DUCKED)
		return;
	self->monsterinfo.aiflags |= AI_DUCKED;
	if (skill->value >= 2)
	{
		if (random() > 0.5)
			GunnerGrenade (self);
	}

	self->maxs[2] -= 32;
	self->takedamage = DAMAGE_YES;
	self->monsterinfo.pausetime = level.time + 1;
	gi.linkentity (self);
}

void gunner_duck_hold (edict_t *self)
{
	if (level.time >= self->monsterinfo.pausetime)
		self->monsterinfo.aiflags &= ~AI_HOLD_FRAME;
	else
		self->monsterinfo.aiflags |= AI_HOLD_FRAME;
}

void gunner_duck_up (edict_t *self)
{
	self->monsterinfo.aiflags &= ~AI_DUCKED;
	self->maxs[2] += 32;
	self->takedamage = DAMAGE_AIM;
	gi.linkentity (self);
}

mframe_t gunner_frames_duck [] =
{
	ai_move, 1,  gunner_duck_down,
	ai_move, 1,  NULL,
	ai_move, 1,  gunner_duck_hold,
	ai_move, 0,  NULL,
	ai_move, -1, NULL,
	ai_move, -1, NULL,
	ai_move, 0,  gunner_duck_up,
	ai_move, -1, NULL
};
mmove_t	gunner_move_duck = {GUNNER_FRAME_duck01, GUNNER_FRAME_duck08, gunner_frames_duck, gunner_run};

void gunner_dodge (edict_t *self, edict_t *attacker, float eta)
{
	if (random() > 0.25)
		return;

	if (!self->enemy)
		self->enemy = attacker;

	self->monsterinfo.currentmove = &gunner_move_duck;
}


void gunner_opengun (edict_t *self)
{
	gi.sound (self, CHAN_VOICE, gunner_sound_open, 1, ATTN_IDLE, 0);
}

void GunnerFire (edict_t *self)
{
	vec3_t	start;
	vec3_t	forward, right;
	vec3_t	target;
	vec3_t	aim;
	int		flash_number;

	flash_number = MZ2_GUNNER_MACHINEGUN_1 + (self->s.frame - GUNNER_FRAME_attak216);

	AngleVectors (self->s.angles, forward, right, NULL);
	G_ProjectSource (self->s.origin, &monster_flash_offset[flash_number*3], forward, right, start);

	// project enemy back a bit and target there
	VectorCopy (self->enemy->s.origin, target);
	VectorMA (target, -0.2, self->enemy->velocity, target);
	target[2] += self->enemy->viewheight;

	VectorSubtract (target, start, aim);
	VectorNormalize (aim);
	monster_fire_bullet (self, start, aim, 3, 4, DEFAULT_BULLET_HSPREAD, DEFAULT_BULLET_VSPREAD, flash_number);
}

void GunnerGrenade (edict_t *self)
{
	vec3_t	start;
	vec3_t	forward, right;
	vec3_t	aim;
	int		flash_number;

	if (self->s.frame == GUNNER_FRAME_attak105)
		flash_number = MZ2_GUNNER_GRENADE_1;
	else if (self->s.frame == GUNNER_FRAME_attak108)
		flash_number = MZ2_GUNNER_GRENADE_2;
	else if (self->s.frame == GUNNER_FRAME_attak111)
		flash_number = MZ2_GUNNER_GRENADE_3;
	else // (self->s.frame == GUNNER_FRAME_attak114)
		flash_number = MZ2_GUNNER_GRENADE_4;

	AngleVectors (self->s.angles, forward, right, NULL);
	G_ProjectSource (self->s.origin, &monster_flash_offset[flash_number*3], forward, right, start);

	//FIXME : do a spread -225 -75 75 225 degrees around forward
	VectorCopy (forward, aim);

	monster_fire_grenade (self, start, aim, 50, 600, flash_number);
}

mframe_t gunner_frames_attack_chain [] =
{
	/*
	ai_charge, 0, NULL,
	ai_charge, 0, NULL,
	ai_charge, 0, NULL,
	ai_charge, 0, NULL,
	ai_charge, 0, NULL,
	ai_charge, 0, NULL,
	ai_charge, 0, NULL,
	ai_charge, 0, NULL,
	*/
	ai_charge, 0, gunner_opengun,
	ai_charge, 0, NULL,
	ai_charge, 0, NULL,
	ai_charge, 0, NULL,
	ai_charge, 0, NULL,
	ai_charge, 0, NULL,
	ai_charge, 0, NULL
};
mmove_t gunner_move_attack_chain = {GUNNER_FRAME_attak209, GUNNER_FRAME_attak215, gunner_frames_attack_chain, gunner_fire_chain};

mframe_t gunner_frames_fire_chain [] =
{
	ai_charge,   0, GunnerFire,
	ai_charge,   0, GunnerFire,
	ai_charge,   0, GunnerFire,
	ai_charge,   0, GunnerFire,
	ai_charge,   0, GunnerFire,
	ai_charge,   0, GunnerFire,
	ai_charge,   0, GunnerFire,
	ai_charge,   0, GunnerFire
};
mmove_t gunner_move_fire_chain = {GUNNER_FRAME_attak216, GUNNER_FRAME_attak223, gunner_frames_fire_chain, gunner_refire_chain};

mframe_t gunner_frames_endfire_chain [] =
{
	ai_charge, 0, NULL,
	ai_charge, 0, NULL,
	ai_charge, 0, NULL,
	ai_charge, 0, NULL,
	ai_charge, 0, NULL,
	ai_charge, 0, NULL,
	ai_charge, 0, NULL
};
mmove_t gunner_move_endfire_chain = {GUNNER_FRAME_attak224, GUNNER_FRAME_attak230, gunner_frames_endfire_chain, gunner_run};

mframe_t gunner_frames_attack_grenade [] =
{
	ai_charge, 0, NULL,
	ai_charge, 0, NULL,
	ai_charge, 0, NULL,
	ai_charge, 0, NULL,
	ai_charge, 0, GunnerGrenade,
	ai_charge, 0, NULL,
	ai_charge, 0, NULL,
	ai_charge, 0, GunnerGrenade,
	ai_charge, 0, NULL,
	ai_charge, 0, NULL,
	ai_charge, 0, GunnerGrenade,
	ai_charge, 0, NULL,
	ai_charge, 0, NULL,
	ai_charge, 0, GunnerGrenade,
	ai_charge, 0, NULL,
	ai_charge, 0, NULL,
	ai_charge, 0, NULL,
	ai_charge, 0, NULL,
	ai_charge, 0, NULL,
	ai_charge, 0, NULL,
	ai_charge, 0, NULL
};
mmove_t gunner_move_attack_grenade = {GUNNER_FRAME_attak101, GUNNER_FRAME_attak121, gunner_frames_attack_grenade, gunner_run};

void gunner_attack(edict_t *self)
{
	if (range (self, self->enemy) == RANGE_MELEE)
	{
		self->monsterinfo.currentmove = &gunner_move_attack_chain;
	}
	else
	{
		if (random() <= 0.5)
			self->monsterinfo.currentmove = &gunner_move_attack_grenade;
		else
			self->monsterinfo.currentmove = &gunner_move_attack_chain;
	}
}

void gunner_fire_chain(edict_t *self)
{
	self->monsterinfo.currentmove = &gunner_move_fire_chain;
}

void gunner_refire_chain(edict_t *self)
{
	if (self->enemy->health > 0)
		if ( visible (self, self->enemy) )
			if (random() <= 0.5)
			{
				self->monsterinfo.currentmove = &gunner_move_fire_chain;
				return;
			}
	self->monsterinfo.currentmove = &gunner_move_endfire_chain;
}

/*QUAKED monster_gunner (1 .5 0) (-16 -16 -24) (16 16 32) Ambush Trigger_Spawn Sight
*/
void SP_monster_gunner (edict_t *self)
{
	if (deathmatch->value)
	{
		G_FreeEdict (self);
		return;
	}

	gunner_sound_death = gi.soundindex ("gunner/death1.wav");
	gunner_sound_pain = gi.soundindex ("gunner/gunpain2.wav");
	gunner_sound_pain2 = gi.soundindex ("gunner/gunpain1.wav");
	gunner_sound_idle = gi.soundindex ("gunner/gunidle1.wav");
	gunner_sound_open = gi.soundindex ("gunner/gunatck1.wav");
	gunner_sound_search = gi.soundindex ("gunner/gunsrch1.wav");
	gunner_sound_sight = gi.soundindex ("gunner/sight1.wav");

	gi.soundindex ("gunner/gunatck2.wav");
	gi.soundindex ("gunner/gunatck3.wav");

	self->movetype = MOVETYPE_STEP;
	self->solid = SOLID_BBOX;
	self->s.modelindex = gi.modelindex ("models/monsters/gunner/tris.md2");
	VectorSet (self->mins, -16, -16, -24);
	VectorSet (self->maxs, 16, 16, 32);

	self->health = 175;
	self->gib_health = -70;
	self->mass = 200;

	self->pain = gunner_pain;
	self->die = gunner_die;

	self->monsterinfo.stand = gunner_stand;
	self->monsterinfo.walk = gunner_walk;
	self->monsterinfo.run = gunner_run;
	self->monsterinfo.dodge = gunner_dodge;
	self->monsterinfo.attack = gunner_attack;
	self->monsterinfo.melee = NULL;
	self->monsterinfo.sight = gunner_sight;
	self->monsterinfo.search = gunner_search;

	gi.linkentity (self);

	self->monsterinfo.currentmove = &gunner_move_stand;
	self->monsterinfo.scale = GUNNER_MODEL_SCALE;

	walkmonster_start (self, false);
}

/*
==============================================================================

END of GUNNER

==============================================================================
*/


/*
==============================================================================

SUPERTANK

==============================================================================
*/

void TreadSound (edict_t *self)
{
	gi.sound (self, CHAN_VOICE, tread_sound, 1, ATTN_NORM, 0);
}

void supertank_search (edict_t *self)
{
	if (random() < 0.5)
		gi.sound (self, CHAN_VOICE, supertank_sound_search1, 1, ATTN_NORM, 0);
	else
		gi.sound (self, CHAN_VOICE, supertank_sound_search2, 1, ATTN_NORM, 0);
}


void supertank_dead (edict_t *self);
void supertankRocket (edict_t *self);
void supertankMachineGun (edict_t *self);
void supertank_reattack1(edict_t *self);


//
// stand
//

mframe_t supertank_frames_stand []=
{
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL
};
mmove_t	supertank_move_stand = {SUPERTANK_FRAME_stand_1, SUPERTANK_FRAME_stand_60, supertank_frames_stand, NULL};

void supertank_stand (edict_t *self)
{
	self->monsterinfo.currentmove = &supertank_move_stand;
}


mframe_t supertank_frames_run [] =
{
	ai_run, 12,	TreadSound,
	ai_run, 12,	NULL,
	ai_run, 12,	NULL,
	ai_run, 12,	NULL,
	ai_run, 12,	NULL,
	ai_run, 12,	NULL,
	ai_run, 12,	NULL,
	ai_run, 12,	NULL,
	ai_run, 12,	NULL,
	ai_run, 12,	NULL,
	ai_run, 12,	NULL,
	ai_run, 12,	NULL,
	ai_run, 12,	NULL,
	ai_run, 12,	NULL,
	ai_run, 12,	NULL,
	ai_run, 12,	NULL,
	ai_run, 12,	NULL,
	ai_run, 12,	NULL
};
mmove_t	supertank_move_run = {SUPERTANK_FRAME_forwrd_1, SUPERTANK_FRAME_forwrd_18, supertank_frames_run, NULL};

//
// walk
//


mframe_t supertank_frames_forward [] =
{
	ai_walk, 4,	TreadSound,
	ai_walk, 4,	NULL,
	ai_walk, 4,	NULL,
	ai_walk, 4,	NULL,
	ai_walk, 4,	NULL,
	ai_walk, 4,	NULL,
	ai_walk, 4,	NULL,
	ai_walk, 4,	NULL,
	ai_walk, 4,	NULL,
	ai_walk, 4,	NULL,
	ai_walk, 4,	NULL,
	ai_walk, 4,	NULL,
	ai_walk, 4,	NULL,
	ai_walk, 4,	NULL,
	ai_walk, 4,	NULL,
	ai_walk, 4,	NULL,
	ai_walk, 4,	NULL,
	ai_walk, 4,	NULL
};
mmove_t	supertank_move_forward = {SUPERTANK_FRAME_forwrd_1, SUPERTANK_FRAME_forwrd_18, supertank_frames_forward, NULL};

void supertank_forward (edict_t *self)
{
		self->monsterinfo.currentmove = &supertank_move_forward;
}

void supertank_walk (edict_t *self)
{
		self->monsterinfo.currentmove = &supertank_move_forward;
}

void supertank_run (edict_t *self)
{
	if (self->monsterinfo.aiflags & AI_STAND_GROUND)
		self->monsterinfo.currentmove = &supertank_move_stand;
	else
		self->monsterinfo.currentmove = &supertank_move_run;
}

mframe_t supertank_frames_turn_right [] =
{
	ai_move,	0,	TreadSound,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL
};
mmove_t supertank_move_turn_right = {SUPERTANK_FRAME_right_1, SUPERTANK_FRAME_right_18, supertank_frames_turn_right, supertank_run};

mframe_t supertank_frames_turn_left [] =
{
	ai_move,	0,	TreadSound,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL
};
mmove_t supertank_move_turn_left = {SUPERTANK_FRAME_left_1, SUPERTANK_FRAME_left_18, supertank_frames_turn_left, supertank_run};


mframe_t supertank_frames_pain3 [] =
{
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL
};
mmove_t supertank_move_pain3 = {SUPERTANK_FRAME_pain3_9, SUPERTANK_FRAME_pain3_12, supertank_frames_pain3, supertank_run};

mframe_t supertank_frames_pain2 [] =
{
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL
};
mmove_t supertank_move_pain2 = {SUPERTANK_FRAME_pain2_5, SUPERTANK_FRAME_pain2_8, supertank_frames_pain2, supertank_run};

mframe_t supertank_frames_pain1 [] =
{
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL
};
mmove_t supertank_move_pain1 = {SUPERTANK_FRAME_pain1_1, SUPERTANK_FRAME_pain1_4, supertank_frames_pain1, supertank_run};

mframe_t supertank_frames_death1 [] =
{
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	BossExplode
};
mmove_t supertank_move_death = {SUPERTANK_FRAME_death_1, SUPERTANK_FRAME_death_24, supertank_frames_death1, supertank_dead};

mframe_t supertank_frames_backward[] =
{
	ai_walk, 0,	TreadSound,
	ai_walk, 0,	NULL,
	ai_walk, 0,	NULL,
	ai_walk, 0,	NULL,
	ai_walk, 0,	NULL,
	ai_walk, 0,	NULL,
	ai_walk, 0,	NULL,
	ai_walk, 0,	NULL,
	ai_walk, 0,	NULL,
	ai_walk, 0,	NULL,
	ai_walk, 0,	NULL,
	ai_walk, 0,	NULL,
	ai_walk, 0,	NULL,
	ai_walk, 0,	NULL,
	ai_walk, 0,	NULL,
	ai_walk, 0,	NULL,
	ai_walk, 0,	NULL,
	ai_walk, 0,	NULL
};
mmove_t	supertank_move_backward = {SUPERTANK_FRAME_backwd_1, SUPERTANK_FRAME_backwd_18, supertank_frames_backward, NULL};

mframe_t supertank_frames_attack4[]=
{
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL
};
mmove_t supertank_move_attack4 = {SUPERTANK_FRAME_attak4_1, SUPERTANK_FRAME_attak4_6, supertank_frames_attack4, supertank_run};

mframe_t supertank_frames_attack3[]=
{
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL
};
mmove_t supertank_move_attack3 = {SUPERTANK_FRAME_attak3_1, SUPERTANK_FRAME_attak3_27, supertank_frames_attack3, supertank_run};

mframe_t supertank_frames_attack2[]=
{
	ai_charge,	0,	NULL,
	ai_charge,	0,	NULL,
	ai_charge,	0,	NULL,
	ai_charge,	0,	NULL,
	ai_charge,	0,	NULL,
	ai_charge,	0,	NULL,
	ai_charge,	0,	NULL,
	ai_charge,	0,	supertankRocket,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	supertankRocket,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	supertankRocket,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL
};
mmove_t supertank_move_attack2 = {SUPERTANK_FRAME_attak2_1, SUPERTANK_FRAME_attak2_27, supertank_frames_attack2, supertank_run};

mframe_t supertank_frames_attack1[]=
{
	ai_charge,	0,	supertankMachineGun,
	ai_charge,	0,	supertankMachineGun,
	ai_charge,	0,	supertankMachineGun,
	ai_charge,	0,	supertankMachineGun,
	ai_charge,	0,	supertankMachineGun,
	ai_charge,	0,	supertankMachineGun,

};
mmove_t supertank_move_attack1 = {SUPERTANK_FRAME_attak1_1, SUPERTANK_FRAME_attak1_6, supertank_frames_attack1, supertank_reattack1};

mframe_t supertank_frames_end_attack1[]=
{
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL
};
mmove_t supertank_move_end_attack1 = {SUPERTANK_FRAME_attak1_7, SUPERTANK_FRAME_attak1_20, supertank_frames_end_attack1, supertank_run};


void supertank_reattack1(edict_t *self)
{
	if (visible(self, self->enemy))
		if (random() < 0.9)
			self->monsterinfo.currentmove = &supertank_move_attack1;
		else
			self->monsterinfo.currentmove = &supertank_move_end_attack1;
	else
		self->monsterinfo.currentmove = &supertank_move_end_attack1;
}

void supertank_pain (edict_t *self, edict_t *other, float kick, int damage)
{

	if (self->health < (self->max_health / 2))
			self->s.skinnum = 1;

	if (level.time < self->pain_debounce_time)
			return;

	// Lessen the chance of him going into his pain frames
	if (damage <=25)
		if (random()<0.2)
			return;

	// Don't go into pain if he's firing his rockets
	if (skill->value >= 2)
		if ( (self->s.frame >= SUPERTANK_FRAME_attak2_1) && (self->s.frame <= SUPERTANK_FRAME_attak2_14) )
			return;

	self->pain_debounce_time = level.time + 3;

	if (skill->value == 3)
		return;		// no pain anims in nightmare

	if (damage <= 10)
	{
		gi.sound (self, CHAN_VOICE, supertank_sound_pain1, 1, ATTN_NORM,0);
		self->monsterinfo.currentmove = &supertank_move_pain1;
	}
	else if (damage <= 25)
	{
		gi.sound (self, CHAN_VOICE, supertank_sound_pain3, 1, ATTN_NORM,0);
		self->monsterinfo.currentmove = &supertank_move_pain2;
	}
	else
	{
		gi.sound (self, CHAN_VOICE, supertank_sound_pain2, 1, ATTN_NORM,0);
		self->monsterinfo.currentmove = &supertank_move_pain3;
	}
};


void supertankRocket (edict_t *self)
{
	vec3_t	forward, right;
	vec3_t	start;
	vec3_t	dir;
	vec3_t	vec;
	int		flash_number;

	if (self->s.frame == SUPERTANK_FRAME_attak2_8)
		flash_number = MZ2_SUPERTANK_ROCKET_1;
	else if (self->s.frame == SUPERTANK_FRAME_attak2_11)
		flash_number = MZ2_SUPERTANK_ROCKET_2;
	else // (self->s.frame == SUPERTANK_FRAME_attak2_14)
		flash_number = MZ2_SUPERTANK_ROCKET_3;

	AngleVectors (self->s.angles, forward, right, NULL);
	G_ProjectSource (self->s.origin, &monster_flash_offset[flash_number*3], forward, right, start);

	VectorCopy (self->enemy->s.origin, vec);
	vec[2] += self->enemy->viewheight;
	VectorSubtract (vec, start, dir);
	VectorNormalize (dir);

	monster_fire_rocket (self, start, dir, 50, 500, flash_number);
}

void supertankMachineGun (edict_t *self)
{
	vec3_t	dir;
	vec3_t	vec;
	vec3_t	start;
	vec3_t	forward, right;
	int		flash_number;

	flash_number = MZ2_SUPERTANK_MACHINEGUN_1 + (self->s.frame - SUPERTANK_FRAME_attak1_1);

	//FIXME!!!
	dir[0] = 0;
	dir[1] = self->s.angles[1];
	dir[2] = 0;

	AngleVectors (dir, forward, right, NULL);
	G_ProjectSource (self->s.origin, &monster_flash_offset[flash_number*3], forward, right, start);

	if (self->enemy)
	{
		VectorCopy (self->enemy->s.origin, vec);
		VectorMA (vec, 0, self->enemy->velocity, vec);
		vec[2] += self->enemy->viewheight;
		VectorSubtract (vec, start, forward);
		VectorNormalize (forward);
  }

	monster_fire_bullet (self, start, forward, 6, 4, DEFAULT_BULLET_HSPREAD, DEFAULT_BULLET_VSPREAD, flash_number);
}


void supertank_attack(edict_t *self)
{
	vec3_t	vec;
	float	range;
	//float	r;

	VectorSubtract (self->enemy->s.origin, self->s.origin, vec);
	range = VectorLength (vec);

	//r = random();

	// Attack 1 == Chaingun
	// Attack 2 == Rocket Launcher

	if (range <= 160)
	{
		self->monsterinfo.currentmove = &supertank_move_attack1;
	}
	else
	{	// fire rockets more often at distance
		if (random() < 0.3)
			self->monsterinfo.currentmove = &supertank_move_attack1;
		else
			self->monsterinfo.currentmove = &supertank_move_attack2;
	}
}


//
// death
//

void supertank_dead (edict_t *self)
{
	VectorSet (self->mins, -60, -60, 0);
	VectorSet (self->maxs, 60, 60, 72);
	self->movetype = MOVETYPE_TOSS;
	self->svflags |= SVF_DEADMONSTER;
	self->nextthink = 0;
	gi.linkentity (self);
}


void supertank_die (edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point)
{
	gi.sound (self, CHAN_VOICE, supertank_sound_death, 1, ATTN_NORM, 0);
	self->deadflag = DEAD_DEAD;
	self->takedamage = DAMAGE_NO;
	self->count = 0;
	self->monsterinfo.currentmove = &supertank_move_death;
}

//
// monster_supertank
//

/*QUAKED monster_supertank (1 .5 0) (-64 -64 0) (64 64 72) Ambush Trigger_Spawn Sight
*/
void SP_monster_supertank (edict_t *self)
{
	if (deathmatch->value)
	{
		G_FreeEdict (self);
		return;
	}

	supertank_sound_pain1 = gi.soundindex ("bosstank/btkpain1.wav");
	supertank_sound_pain2 = gi.soundindex ("bosstank/btkpain2.wav");
	supertank_sound_pain3 = gi.soundindex ("bosstank/btkpain3.wav");
	supertank_sound_death = gi.soundindex ("bosstank/btkdeth1.wav");
	supertank_sound_search1 = gi.soundindex ("bosstank/btkunqv1.wav");
	supertank_sound_search2 = gi.soundindex ("bosstank/btkunqv2.wav");

//	self->s.sound = gi.soundindex ("bosstank/btkengn1.wav");
	tread_sound = gi.soundindex ("bosstank/btkengn1.wav");

	self->movetype = MOVETYPE_STEP;
	self->solid = SOLID_BBOX;
	self->s.modelindex = gi.modelindex ("models/monsters/boss1/tris.md2");
	VectorSet (self->mins, -64, -64, 0);
	VectorSet (self->maxs, 64, 64, 112);

	self->health = 1500;
	self->gib_health = -500;
	self->mass = 800;

	self->pain = supertank_pain;
	self->die = supertank_die;
	self->monsterinfo.stand = supertank_stand;
	self->monsterinfo.walk = supertank_walk;
	self->monsterinfo.run = supertank_run;
	self->monsterinfo.dodge = NULL;
	self->monsterinfo.attack = supertank_attack;
	self->monsterinfo.search = supertank_search;
	self->monsterinfo.melee = NULL;
	self->monsterinfo.sight = NULL;

	gi.linkentity (self);

	self->monsterinfo.currentmove = &supertank_move_stand;
	self->monsterinfo.scale = SUPERTANK_MODEL_SCALE;

	walkmonster_start(self, false);
}

/*
==============================================================================

END of SUPERTANK

==============================================================================
*/


/*
==============================================================================

insane

==============================================================================
*/

void insane_fist (edict_t *self)
{
	gi.sound (self, CHAN_VOICE, insane_sound_fist, 1, ATTN_IDLE, 0);
}

void insane_shake (edict_t *self)
{
	gi.sound (self, CHAN_VOICE, insane_sound_shake, 1, ATTN_IDLE, 0);
}

void insane_moan (edict_t *self)
{
	gi.sound (self, CHAN_VOICE, insane_sound_moan, 1, ATTN_IDLE, 0);
}

void insane_scream (edict_t *self)
{
	gi.sound (self, CHAN_VOICE, insane_sound_scream[rand()%8], 1, ATTN_IDLE, 0);
}


void insane_stand (edict_t *self);
void insane_dead (edict_t *self);
void insane_cross (edict_t *self);
void insane_walk (edict_t *self);
void insane_run (edict_t *self);
void insane_checkdown (edict_t *self);
void insane_checkup (edict_t *self);
void insane_onground (edict_t *self);


mframe_t insane_frames_stand_normal [] =
{
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, insane_checkdown
};
mmove_t insane_move_stand_normal = {INSANE_FRAME_stand60, INSANE_FRAME_stand65, insane_frames_stand_normal, insane_stand};

mframe_t insane_frames_stand_insane [] =
{
	ai_stand,	0,	insane_shake,
	ai_stand,	0,	NULL,
	ai_stand,	0,	NULL,
	ai_stand,	0,	NULL,
	ai_stand,	0,	NULL,
	ai_stand,	0,	NULL,
	ai_stand,	0,	NULL,
	ai_stand,	0,	NULL,
	ai_stand,	0,	NULL,
	ai_stand,	0,	NULL,
	ai_stand,	0,	NULL,
	ai_stand,	0,	NULL,
	ai_stand,	0,	NULL,
	ai_stand,	0,	NULL,
	ai_stand,	0,	NULL,
	ai_stand,	0,	NULL,
	ai_stand,	0,	NULL,
	ai_stand,	0,	NULL,
	ai_stand,	0,	NULL,
	ai_stand,	0,	NULL,
	ai_stand,	0,	NULL,
	ai_stand,	0,	NULL,
	ai_stand,	0,	NULL,
	ai_stand,	0,	NULL,
	ai_stand,	0,	NULL,
	ai_stand,	0,	NULL,
	ai_stand,	0,	NULL,
	ai_stand,	0,	NULL,
	ai_stand,	0,	NULL,
	ai_stand,	0,	insane_checkdown
};
mmove_t insane_move_stand_insane = {INSANE_FRAME_stand65, INSANE_FRAME_stand94, insane_frames_stand_insane, insane_stand};

mframe_t insane_frames_uptodown [] =
{
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	insane_moan,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,

	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,

	ai_move,	2.7,	NULL,
	ai_move,	4.1,	NULL,
	ai_move,	6,		NULL,
	ai_move,	7.6,	NULL,
	ai_move,	3.6,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	insane_fist,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,

	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	insane_fist,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL
};
mmove_t insane_move_uptodown = {INSANE_FRAME_stand1, INSANE_FRAME_stand40, insane_frames_uptodown, insane_onground};


mframe_t insane_frames_downtoup [] =
{
	ai_move,	-0.7,	NULL,			// 41
	ai_move,	-1.2,	NULL,			// 42
	ai_move,	-1.5,		NULL,		// 43
	ai_move,	-4.5,		NULL,		// 44
	ai_move,	-3.5,	NULL,			// 45
	ai_move,	-0.2,	NULL,			// 46
	ai_move,	0,	NULL,			// 47
	ai_move,	-1.3,	NULL,			// 48
	ai_move,	-3,	NULL,				// 49
	ai_move,	-2,	NULL,			// 50
	ai_move,	0,	NULL,				// 51
	ai_move,	0,	NULL,				// 52
	ai_move,	0,	NULL,				// 53
	ai_move,	-3.3,	NULL,			// 54
	ai_move,	-1.6,	NULL,			// 55
	ai_move,	-0.3,	NULL,			// 56
	ai_move,	0,	NULL,				// 57
	ai_move,	0,	NULL,				// 58
	ai_move,	0,	NULL				// 59
};
mmove_t insane_move_downtoup = {INSANE_FRAME_stand41, INSANE_FRAME_stand59, insane_frames_downtoup, insane_stand};

mframe_t insane_frames_jumpdown [] =
{
	ai_move,	0.2,	NULL,
	ai_move,	11.5,	NULL,
	ai_move,	5.1,	NULL,
	ai_move,	7.1,	NULL,
	ai_move,	0,	NULL
};
mmove_t insane_move_jumpdown = {INSANE_FRAME_stand96, INSANE_FRAME_stand100, insane_frames_jumpdown, insane_onground};


mframe_t insane_frames_down [] =
{
	ai_move,	0,		NULL,		// 100
	ai_move,	0,		NULL,
	ai_move,	0,		NULL,
	ai_move,	0,		NULL,
	ai_move,	0,		NULL,
	ai_move,	0,		NULL,
	ai_move,	0,		NULL,
	ai_move,	0,		NULL,
	ai_move,	0,		NULL,
	ai_move,	0,		NULL,
	ai_move,	0,		NULL,		// 110
	ai_move,	-1.7,		NULL,
	ai_move,	-1.6,		NULL,
	ai_move,	0,		NULL,
	ai_move,	0,		NULL,
	ai_move,	0,		NULL,
	ai_move,	0,		insane_fist,
	ai_move,	0,		NULL,
	ai_move,	0,		NULL,
	ai_move,	0,		NULL,
	ai_move,	0,		NULL,		// 120
	ai_move,	0,		NULL,
	ai_move,	0,		NULL,
	ai_move,	0,		NULL,
	ai_move,	0,		NULL,
	ai_move,	0,		NULL,
	ai_move,	0,		NULL,
	ai_move,	0,		NULL,
	ai_move,	0,		NULL,
	ai_move,	0,		NULL,
	ai_move,	0,		NULL,		// 130
	ai_move,	0,		NULL,
	ai_move,	0,		NULL,
	ai_move,	0,		insane_moan,
	ai_move,	0,		NULL,
	ai_move,	0,		NULL,
	ai_move,	0,		NULL,
	ai_move,	0,		NULL,
	ai_move,	0,		NULL,
	ai_move,	0,		NULL,
	ai_move,	0,		NULL,		// 140
	ai_move,	0,		NULL,
	ai_move,	0,		NULL,
	ai_move,	0,		NULL,
	ai_move,	0,		NULL,
	ai_move,	0,		NULL,
	ai_move,	0,		NULL,
	ai_move,	0,		NULL,
	ai_move,	0,		NULL,
	ai_move,	0,		NULL,
	ai_move,	0,		NULL,		// 150
	ai_move,	0.5,		NULL,
	ai_move,	0,		NULL,
	ai_move,	-0.2,		insane_scream,
	ai_move,	0,		NULL,
	ai_move,	0.2,		NULL,
	ai_move,	0.4,		NULL,
	ai_move,	0.6,		NULL,
	ai_move,	0.8,		NULL,
	ai_move,	0.7,		NULL,
	ai_move,	0,		insane_checkup		// 160
};
mmove_t insane_move_down = {INSANE_FRAME_stand100, INSANE_FRAME_stand160, insane_frames_down, insane_onground};

mframe_t insane_frames_walk_normal [] =
{
	ai_walk,	0,		insane_scream,
	ai_walk,	2.5,	NULL,
	ai_walk,	3.5,	NULL,
	ai_walk,	1.7,	NULL,
	ai_walk,	2.3,	NULL,
	ai_walk,	2.4,	NULL,
	ai_walk,	2.2,	NULL,
	ai_walk,	4.2,	NULL,
	ai_walk,	5.6,	NULL,
	ai_walk,	3.3,	NULL,
	ai_walk,	2.4,	NULL,
	ai_walk,	0.9,	NULL,
	ai_walk,	0,		NULL
};
mmove_t insane_move_walk_normal = {INSANE_FRAME_walk27, INSANE_FRAME_walk39, insane_frames_walk_normal, insane_walk};
mmove_t insane_move_run_normal = {INSANE_FRAME_walk27, INSANE_FRAME_walk39, insane_frames_walk_normal, insane_run};

mframe_t insane_frames_walk_insane [] =
{
	ai_walk,	0,		insane_scream,		// walk 1
	ai_walk,	3.4,	NULL,		// walk 2
	ai_walk,	3.6,	NULL,		// 3
	ai_walk,	2.9,	NULL,		// 4
	ai_walk,	2.2,	NULL,		// 5
	ai_walk,	2.6,	NULL,		// 6
	ai_walk,	0,		NULL,		// 7
	ai_walk,	0.7,	NULL,		// 8
	ai_walk,	4.8,	NULL,		// 9
	ai_walk,	5.3,	NULL,		// 10
	ai_walk,	1.1,	NULL,		// 11
	ai_walk,	2,		NULL,		// 12
	ai_walk,	0.5,	NULL,		// 13
	ai_walk,	0,		NULL,		// 14
	ai_walk,	0,		NULL,		// 15
	ai_walk,	4.9,	NULL,		// 16
	ai_walk,	6.7,	NULL,		// 17
	ai_walk,	3.8,	NULL,		// 18
	ai_walk,	2,		NULL,		// 19
	ai_walk,	0.2,	NULL,		// 20
	ai_walk,	0,		NULL,		// 21
	ai_walk,	3.4,	NULL,		// 22
	ai_walk,	6.4,	NULL,		// 23
	ai_walk,	5,		NULL,		// 24
	ai_walk,	1.8,	NULL,		// 25
	ai_walk,	0,		NULL		// 26
};
mmove_t insane_move_walk_insane = {INSANE_FRAME_walk1, INSANE_FRAME_walk26, insane_frames_walk_insane, insane_walk};
mmove_t insane_move_run_insane = {INSANE_FRAME_walk1, INSANE_FRAME_walk26, insane_frames_walk_insane, insane_run};

mframe_t insane_frames_stand_pain [] =
{
	ai_move,	0,		NULL,
	ai_move,	0,		NULL,
	ai_move,	0,		NULL,
	ai_move,	0,		NULL,
	ai_move,	0,		NULL,
	ai_move,	0,		NULL,
	ai_move,	0,		NULL,
	ai_move,	0,		NULL,
	ai_move,	0,		NULL,
	ai_move,	0,		NULL,
	ai_move,	0,		NULL
};
mmove_t insane_move_stand_pain = {INSANE_FRAME_st_pain2, INSANE_FRAME_st_pain12, insane_frames_stand_pain, insane_run};

mframe_t insane_frames_stand_death [] =
{
	ai_move,	0,		NULL,
	ai_move,	0,		NULL,
	ai_move,	0,		NULL,
	ai_move,	0,		NULL,
	ai_move,	0,		NULL,
	ai_move,	0,		NULL,
	ai_move,	0,		NULL,
	ai_move,	0,		NULL,
	ai_move,	0,		NULL,
	ai_move,	0,		NULL,
	ai_move,	0,		NULL,
	ai_move,	0,		NULL,
	ai_move,	0,		NULL,
	ai_move,	0,		NULL,
	ai_move,	0,		NULL,
	ai_move,	0,		NULL,
	ai_move,	0,		NULL
};
mmove_t insane_move_stand_death = {INSANE_FRAME_st_death2, INSANE_FRAME_st_death18, insane_frames_stand_death, insane_dead};

mframe_t insane_frames_crawl [] =
{
	ai_walk,	0,		insane_scream,
	ai_walk,	1.5,	NULL,
	ai_walk,	2.1,	NULL,
	ai_walk,	3.6,	NULL,
	ai_walk,	2,		NULL,
	ai_walk,	0.9,	NULL,
	ai_walk,	3,		NULL,
	ai_walk,	3.4,	NULL,
	ai_walk,	2.4,	NULL
};
mmove_t insane_move_crawl = {INSANE_FRAME_crawl1, INSANE_FRAME_crawl9, insane_frames_crawl, NULL};
mmove_t insane_move_runcrawl = {INSANE_FRAME_crawl1, INSANE_FRAME_crawl9, insane_frames_crawl, NULL};

mframe_t insane_frames_crawl_pain [] =
{
	ai_move,	0,		NULL,
	ai_move,	0,		NULL,
	ai_move,	0,		NULL,
	ai_move,	0,		NULL,
	ai_move,	0,		NULL,
	ai_move,	0,		NULL,
	ai_move,	0,		NULL,
	ai_move,	0,		NULL,
	ai_move,	0,		NULL
};
mmove_t insane_move_crawl_pain = {INSANE_FRAME_cr_pain2, INSANE_FRAME_cr_pain10, insane_frames_crawl_pain, insane_run};

mframe_t insane_frames_crawl_death [] =
{
	ai_move,	0,		NULL,
	ai_move,	0,		NULL,
	ai_move,	0,		NULL,
	ai_move,	0,		NULL,
	ai_move,	0,		NULL,
	ai_move,	0,		NULL,
	ai_move,	0,		NULL
};
mmove_t insane_move_crawl_death = {INSANE_FRAME_cr_death10, INSANE_FRAME_cr_death16, insane_frames_crawl_death, insane_dead};

mframe_t insane_frames_cross [] =
{
	ai_move,	0,		insane_moan,
	ai_move,	0,		NULL,
	ai_move,	0,		NULL,
	ai_move,	0,		NULL,
	ai_move,	0,		NULL,
	ai_move,	0,		NULL,
	ai_move,	0,		NULL,
	ai_move,	0,		NULL,
	ai_move,	0,		NULL,
	ai_move,	0,		NULL,
	ai_move,	0,		NULL,
	ai_move,	0,		NULL,
	ai_move,	0,		NULL,
	ai_move,	0,		NULL,
	ai_move,	0,		NULL
};
mmove_t insane_move_cross = {INSANE_FRAME_cross1, INSANE_FRAME_cross15, insane_frames_cross, insane_cross};

mframe_t insane_frames_struggle_cross [] =
{
	ai_move,	0,		insane_scream,
	ai_move,	0,		NULL,
	ai_move,	0,		NULL,
	ai_move,	0,		NULL,
	ai_move,	0,		NULL,
	ai_move,	0,		NULL,
	ai_move,	0,		NULL,
	ai_move,	0,		NULL,
	ai_move,	0,		NULL,
	ai_move,	0,		NULL,
	ai_move,	0,		NULL,
	ai_move,	0,		NULL,
	ai_move,	0,		NULL,
	ai_move,	0,		NULL,
	ai_move,	0,		NULL
};
mmove_t insane_move_struggle_cross = {INSANE_FRAME_cross16, INSANE_FRAME_cross30, insane_frames_struggle_cross, insane_cross};

void insane_cross (edict_t *self)
{
	if (random() < 0.8)
		self->monsterinfo.currentmove = &insane_move_cross;
	else
		self->monsterinfo.currentmove = &insane_move_struggle_cross;
}

void insane_walk (edict_t *self)
{
	if ( self->spawnflags & 16 )			// Hold Ground?
		if (self->s.frame == INSANE_FRAME_cr_pain10)
		{
			self->monsterinfo.currentmove = &insane_move_down;
			return;
		}
	if (self->spawnflags & 4)
		self->monsterinfo.currentmove = &insane_move_crawl;
	else
		if (random() <= 0.5)
			self->monsterinfo.currentmove = &insane_move_walk_normal;
		else
			self->monsterinfo.currentmove = &insane_move_walk_insane;
}

void insane_run (edict_t *self)
{
	if ( self->spawnflags & 16 )			// Hold Ground?
		if (self->s.frame == INSANE_FRAME_cr_pain10)
		{
			self->monsterinfo.currentmove = &insane_move_down;
			return;
		}
	if (self->spawnflags & 4)				// Crawling?
		self->monsterinfo.currentmove = &insane_move_runcrawl;
	else
		if (random() <= 0.5)				// Else, mix it up
			self->monsterinfo.currentmove = &insane_move_run_normal;
		else
			self->monsterinfo.currentmove = &insane_move_run_insane;
}


void insane_pain (edict_t *self, edict_t *other, float kick, int damage)
{
	int	l,r;

//	if (self->health < (self->max_health / 2))
//		self->s.skinnum = 1;

	if (level.time < self->pain_debounce_time)
		return;

	self->pain_debounce_time = level.time + 3;

	r = 1 + (rand()&1);
	if (self->health < 25)
		l = 25;
	else if (self->health < 50)
		l = 50;
	else if (self->health < 75)
		l = 75;
	else
		l = 100;
	gi.sound (self, CHAN_VOICE, gi.soundindex (va("player/male/pain%i_%i.wav", l, r)), 1, ATTN_IDLE, 0);

	if (skill->value == 3)
		return;		// no pain anims in nightmare

	// Don't go into pain frames if crucified.
	if (self->spawnflags & 8)
	{
		self->monsterinfo.currentmove = &insane_move_struggle_cross;
		return;
	}

	if  ( ((self->s.frame >= INSANE_FRAME_crawl1) && (self->s.frame <= INSANE_FRAME_crawl9)) || ((self->s.frame >= INSANE_FRAME_stand99) && (self->s.frame <= INSANE_FRAME_stand160)) )
	{
		self->monsterinfo.currentmove = &insane_move_crawl_pain;
	}
	else
		self->monsterinfo.currentmove = &insane_move_stand_pain;

}

void insane_onground (edict_t *self)
{
	self->monsterinfo.currentmove = &insane_move_down;
}

void insane_checkdown (edict_t *self)
{
//	if ( (self->s.frame == INSANE_FRAME_stand94) || (self->s.frame == INSANE_FRAME_stand65) )
	if (self->spawnflags & 32)				// Always stand
		return;
	if (random() < 0.3)
		if (random() < 0.5)
			self->monsterinfo.currentmove = &insane_move_uptodown;
		else
			self->monsterinfo.currentmove = &insane_move_jumpdown;
}

void insane_checkup (edict_t *self)
{
	// If Hold_Ground and Crawl are set
	if ( (self->spawnflags & 4) && (self->spawnflags & 16) )
		return;
	if (random() < 0.5)
		self->monsterinfo.currentmove = &insane_move_downtoup;

}

void insane_stand (edict_t *self)
{
	if (self->spawnflags & 8)			// If crucified
	{
		self->monsterinfo.currentmove = &insane_move_cross;
		self->monsterinfo.aiflags |= AI_STAND_GROUND;
	}
	// If Hold_Ground and Crawl are set
	else if ( (self->spawnflags & 4) && (self->spawnflags & 16) )
		self->monsterinfo.currentmove = &insane_move_down;
	else
		if (random() < 0.5)
			self->monsterinfo.currentmove = &insane_move_stand_normal;
		else
			self->monsterinfo.currentmove = &insane_move_stand_insane;
}

void insane_dead (edict_t *self)
{
	if (self->spawnflags & 8)
	{
		self->flags |= FL_FLY;
	}
	else
	{
		VectorSet (self->mins, -16, -16, -24);
		VectorSet (self->maxs, 16, 16, -8);
		self->movetype = MOVETYPE_TOSS;
	}
	self->svflags |= SVF_DEADMONSTER;
	self->nextthink = 0;
	gi.linkentity (self);
}


void insane_die (edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point)
{
	int		n;

	if (self->health <= self->gib_health)
	{
		gi.sound (self, CHAN_VOICE, gi.soundindex ("misc/udeath.wav"), 1, ATTN_IDLE, 0);
		if(net_compatibility->value || !net_fixoverflow->value)
		{
			for (n= 0; n < 2; n++)
				ThrowGib (self, "models/objects/gibs/bone/tris.md2", damage, GIB_ORGANIC);
			for (n= 0; n < 4; n++)
				ThrowGib (self, "models/objects/gibs/sm_meat/tris.md2", damage, GIB_ORGANIC);
			ThrowHead (self, "models/objects/gibs/head2/tris.md2", damage, GIB_ORGANIC);
			self->deadflag = DEAD_DEAD;
		}
		else
		{
			NetThrowMonsterGibsExt (self, GIBS_INSANE, damage);
			G_FreeEdict (self);
		}
		return;
	}

	if (self->deadflag == DEAD_DEAD)
		return;

	gi.sound (self, CHAN_VOICE, gi.soundindex(va("player/male/death%i.wav", (rand()%4)+1)), 1, ATTN_IDLE, 0);

	self->deadflag = DEAD_DEAD;
	self->takedamage = DAMAGE_YES;

	if (self->spawnflags & 8)
	{
		insane_dead (self);
	}
	else
	{
		if ( ((self->s.frame >= INSANE_FRAME_crawl1) && (self->s.frame <= INSANE_FRAME_crawl9)) || ((self->s.frame >= INSANE_FRAME_stand99) && (self->s.frame <= INSANE_FRAME_stand160)) )
			self->monsterinfo.currentmove = &insane_move_crawl_death;
		else
			self->monsterinfo.currentmove = &insane_move_stand_death;
	}
}


/*QUAKED misc_insane (1 .5 0) (-16 -16 -24) (16 16 32) Ambush Trigger_Spawn CRAWL CRUCIFIED STAND_GROUND ALWAYS_STAND
*/
void SP_misc_insane (edict_t *self)
{
//	static int skin = 0;	//@@

	if (deathmatch->value)
	{
		G_FreeEdict (self);
		return;
	}

	insane_sound_fist = gi.soundindex ("insane/insane11.wav");
	insane_sound_shake = gi.soundindex ("insane/insane5.wav");
	insane_sound_moan = gi.soundindex ("insane/insane7.wav");
	insane_sound_scream[0] = gi.soundindex ("insane/insane1.wav");
	insane_sound_scream[1] = gi.soundindex ("insane/insane2.wav");
	insane_sound_scream[2] = gi.soundindex ("insane/insane3.wav");
	insane_sound_scream[3] = gi.soundindex ("insane/insane4.wav");
	insane_sound_scream[4] = gi.soundindex ("insane/insane6.wav");
	insane_sound_scream[5] = gi.soundindex ("insane/insane8.wav");
	insane_sound_scream[6] = gi.soundindex ("insane/insane9.wav");
	insane_sound_scream[7] = gi.soundindex ("insane/insane10.wav");

	self->movetype = MOVETYPE_STEP;
	self->solid = SOLID_BBOX;
	self->s.modelindex = gi.modelindex("models/monsters/insane/tris.md2");

	VectorSet (self->mins, -16, -16, -24);
	VectorSet (self->maxs, 16, 16, 32);

	self->health = 100;
	self->gib_health = -50;
	self->mass = 300;

	self->pain = insane_pain;
	self->die = insane_die;

	self->monsterinfo.stand = insane_stand;
	self->monsterinfo.walk = insane_walk;
	self->monsterinfo.run = insane_run;
	self->monsterinfo.dodge = NULL;
	self->monsterinfo.attack = NULL;
	self->monsterinfo.melee = NULL;
	self->monsterinfo.sight = NULL;
	self->monsterinfo.aiflags |= AI_GOOD_GUY;

//@@
//	self->s.skinnum = skin;
//	skin++;
//	if (skin > 12)
//		skin = 0;

	gi.linkentity (self);

	if (self->spawnflags & 16)				// Stand Ground
		self->monsterinfo.aiflags |= AI_STAND_GROUND;

	self->monsterinfo.currentmove = &insane_move_stand_normal;

	self->monsterinfo.scale = INSANE_MODEL_SCALE;

	if (self->spawnflags & 8)					// Crucified ?
	{
		VectorSet (self->mins, -16, 0, 0);
		VectorSet (self->maxs, 16, 8, 32);
		self->flags |= FL_NO_KNOCKBACK;
		flymonster_start (self, true);
	}
	else
	{
		walkmonster_start (self, true);
		self->s.skinnum = rand()%3;
	}
}

/*
==============================================================================

end of insane

==============================================================================
*/


/*
==============================================================================

floater

==============================================================================
*/

void floater_sight (edict_t *self, edict_t *other)
{
	gi.sound (self, CHAN_VOICE, floater_sound_sight, 1, ATTN_NORM, 0);
}

void floater_idle (edict_t *self)
{
	gi.sound (self, CHAN_VOICE, floater_sound_idle, 1, ATTN_IDLE, 0);
}


void floater_dead (edict_t *self);
void floater_die (edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point);
void floater_run (edict_t *self);
void floater_wham (edict_t *self);
void floater_zap (edict_t *self);


void floater_fire_blaster (edict_t *self)
{
	vec3_t	start;
	vec3_t	forward, right;
	vec3_t	end;
	vec3_t	dir;
	int		effect;

	if ((self->s.frame == FLOATER_FRAME_attak104) || (self->s.frame == FLOATER_FRAME_attak107))
		effect = EF_HYPERBLASTER;
	else
		effect = 0;
	AngleVectors (self->s.angles, forward, right, NULL);
	G_ProjectSource (self->s.origin, &monster_flash_offset[MZ2_FLOAT_BLASTER_1*3], forward, right, start);

	VectorCopy (self->enemy->s.origin, end);
	end[2] += self->enemy->viewheight;
	VectorSubtract (end, start, dir);

	monster_fire_blaster (self, start, dir, 1, 1000, MZ2_FLOAT_BLASTER_1, effect);
}


mframe_t floater_frames_stand1 [] =
{
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL
};
mmove_t	floater_move_stand1 = {FLOATER_FRAME_stand101, FLOATER_FRAME_stand152, floater_frames_stand1, NULL};

mframe_t floater_frames_stand2 [] =
{
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL
};
mmove_t	floater_move_stand2 = {FLOATER_FRAME_stand201, FLOATER_FRAME_stand252, floater_frames_stand2, NULL};

void floater_stand (edict_t *self)
{
	if (random() <= 0.5)
		self->monsterinfo.currentmove = &floater_move_stand1;
	else
		self->monsterinfo.currentmove = &floater_move_stand2;
}

mframe_t floater_frames_activate [] =
{
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL
};
mmove_t floater_move_activate = {FLOATER_FRAME_actvat01, FLOATER_FRAME_actvat31, floater_frames_activate, NULL};

mframe_t floater_frames_attack1 [] =
{
	ai_charge,	0,	NULL,			// Blaster attack
	ai_charge,	0,	NULL,
	ai_charge,	0,	NULL,
	ai_charge,	0,	floater_fire_blaster,			// BOOM (0, -25.8, 32.5)	-- LOOP Starts
	ai_charge,	0,	floater_fire_blaster,
	ai_charge,	0,	floater_fire_blaster,
	ai_charge,	0,	floater_fire_blaster,
	ai_charge,	0,	floater_fire_blaster,
	ai_charge,	0,	floater_fire_blaster,
	ai_charge,	0,	floater_fire_blaster,
	ai_charge,	0,	NULL,
	ai_charge,	0,	NULL,
	ai_charge,	0,	NULL,
	ai_charge,	0,	NULL			//							-- LOOP Ends
};
mmove_t floater_move_attack1 = {FLOATER_FRAME_attak101, FLOATER_FRAME_attak114, floater_frames_attack1, floater_run};

mframe_t floater_frames_attack2 [] =
{
	ai_charge,	0,	NULL,			// Claws
	ai_charge,	0,	NULL,
	ai_charge,	0,	NULL,
	ai_charge,	0,	NULL,
	ai_charge,	0,	NULL,
	ai_charge,	0,	NULL,
	ai_charge,	0,	NULL,
	ai_charge,	0,	NULL,
	ai_charge,	0,	NULL,
	ai_charge,	0,	NULL,
	ai_charge,	0,	NULL,
	ai_charge,	0,	floater_wham,			// WHAM (0, -45, 29.6)		-- LOOP Starts
	ai_charge,	0,	NULL,
	ai_charge,	0,	NULL,
	ai_charge,	0,	NULL,
	ai_charge,	0,	NULL,
	ai_charge,	0,	NULL,
	ai_charge,	0,	NULL,
	ai_charge,	0,	NULL,			//							-- LOOP Ends
	ai_charge,	0,	NULL,
	ai_charge,	0,	NULL,
	ai_charge,	0,	NULL,
	ai_charge,	0,	NULL,
	ai_charge,	0,	NULL,
	ai_charge,	0,	NULL
};
mmove_t floater_move_attack2 = {FLOATER_FRAME_attak201, FLOATER_FRAME_attak225, floater_frames_attack2, floater_run};

mframe_t floater_frames_attack3 [] =
{
	ai_charge,	0,	NULL,
	ai_charge,	0,	NULL,
	ai_charge,	0,	NULL,
	ai_charge,	0,	NULL,
	ai_charge,	0,	NULL,
	ai_charge,	0,	NULL,
	ai_charge,	0,	NULL,
	ai_charge,	0,	NULL,
	ai_charge,	0,	floater_zap,		//								-- LOOP Starts
	ai_charge,	0,	NULL,
	ai_charge,	0,	NULL,
	ai_charge,	0,	NULL,
	ai_charge,	0,	NULL,
	ai_charge,	0,	NULL,
	ai_charge,	0,	NULL,
	ai_charge,	0,	NULL,
	ai_charge,	0,	NULL,
	ai_charge,	0,	NULL,
	ai_charge,	0,	NULL,
	ai_charge,	0,	NULL,
	ai_charge,	0,	NULL,
	ai_charge,	0,	NULL,
	ai_charge,	0,	NULL,
	ai_charge,	0,	NULL,
	ai_charge,	0,	NULL,
	ai_charge,	0,	NULL,
	ai_charge,	0,	NULL,
	ai_charge,	0,	NULL,
	ai_charge,	0,	NULL,		//								-- LOOP Ends
	ai_charge,	0,	NULL,
	ai_charge,	0,	NULL,
	ai_charge,	0,	NULL,
	ai_charge,	0,	NULL,
	ai_charge,	0,	NULL
};
mmove_t floater_move_attack3 = {FLOATER_FRAME_attak301, FLOATER_FRAME_attak334, floater_frames_attack3, floater_run};

mframe_t floater_frames_death [] =
{
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL
};
mmove_t floater_move_death = {FLOATER_FRAME_death01, FLOATER_FRAME_death13, floater_frames_death, floater_dead};

mframe_t floater_frames_pain1 [] =
{
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL
};
mmove_t floater_move_pain1 = {FLOATER_FRAME_pain101, FLOATER_FRAME_pain107, floater_frames_pain1, floater_run};

mframe_t floater_frames_pain2 [] =
{
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL
};
mmove_t floater_move_pain2 = {FLOATER_FRAME_pain201, FLOATER_FRAME_pain208, floater_frames_pain2, floater_run};

mframe_t floater_frames_pain3 [] =
{
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL
};
mmove_t floater_move_pain3 = {FLOATER_FRAME_pain301, FLOATER_FRAME_pain312, floater_frames_pain3, floater_run};

mframe_t floater_frames_walk [] =
{
	ai_walk, 5, NULL,
	ai_walk, 5, NULL,
	ai_walk, 5, NULL,
	ai_walk, 5, NULL,
	ai_walk, 5, NULL,
	ai_walk, 5, NULL,
	ai_walk, 5, NULL,
	ai_walk, 5, NULL,
	ai_walk, 5, NULL,
	ai_walk, 5, NULL,
	ai_walk, 5, NULL,
	ai_walk, 5, NULL,
	ai_walk, 5, NULL,
	ai_walk, 5, NULL,
	ai_walk, 5, NULL,
	ai_walk, 5, NULL,
	ai_walk, 5, NULL,
	ai_walk, 5, NULL,
	ai_walk, 5, NULL,
	ai_walk, 5, NULL,
	ai_walk, 5, NULL,
	ai_walk, 5, NULL,
	ai_walk, 5, NULL,
	ai_walk, 5, NULL,
	ai_walk, 5, NULL,
	ai_walk, 5, NULL,
	ai_walk, 5, NULL,
	ai_walk, 5, NULL,
	ai_walk, 5, NULL,
	ai_walk, 5, NULL,
	ai_walk, 5, NULL,
	ai_walk, 5, NULL,
	ai_walk, 5, NULL,
	ai_walk, 5, NULL,
	ai_walk, 5, NULL,
	ai_walk, 5, NULL,
	ai_walk, 5, NULL,
	ai_walk, 5, NULL,
	ai_walk, 5, NULL,
	ai_walk, 5, NULL,
	ai_walk, 5, NULL,
	ai_walk, 5, NULL,
	ai_walk, 5, NULL,
	ai_walk, 5, NULL,
	ai_walk, 5, NULL,
	ai_walk, 5, NULL,
	ai_walk, 5, NULL,
	ai_walk, 5, NULL,
	ai_walk, 5, NULL,
	ai_walk, 5, NULL,
	ai_walk, 5, NULL,
	ai_walk, 5, NULL
};
mmove_t	floater_move_walk = {FLOATER_FRAME_stand101, FLOATER_FRAME_stand152, floater_frames_walk, NULL};

mframe_t floater_frames_run [] =
{
	ai_run, 13, NULL,
	ai_run, 13, NULL,
	ai_run, 13, NULL,
	ai_run, 13, NULL,
	ai_run, 13, NULL,
	ai_run, 13, NULL,
	ai_run, 13, NULL,
	ai_run, 13, NULL,
	ai_run, 13, NULL,
	ai_run, 13, NULL,
	ai_run, 13, NULL,
	ai_run, 13, NULL,
	ai_run, 13, NULL,
	ai_run, 13, NULL,
	ai_run, 13, NULL,
	ai_run, 13, NULL,
	ai_run, 13, NULL,
	ai_run, 13, NULL,
	ai_run, 13, NULL,
	ai_run, 13, NULL,
	ai_run, 13, NULL,
	ai_run, 13, NULL,
	ai_run, 13, NULL,
	ai_run, 13, NULL,
	ai_run, 13, NULL,
	ai_run, 13, NULL,
	ai_run, 13, NULL,
	ai_run, 13, NULL,
	ai_run, 13, NULL,
	ai_run, 13, NULL,
	ai_run, 13, NULL,
	ai_run, 13, NULL,
	ai_run, 13, NULL,
	ai_run, 13, NULL,
	ai_run, 13, NULL,
	ai_run, 13, NULL,
	ai_run, 13, NULL,
	ai_run, 13, NULL,
	ai_run, 13, NULL,
	ai_run, 13, NULL,
	ai_run, 13, NULL,
	ai_run, 13, NULL,
	ai_run, 13, NULL,
	ai_run, 13, NULL,
	ai_run, 13, NULL,
	ai_run, 13, NULL,
	ai_run, 13, NULL,
	ai_run, 13, NULL,
	ai_run, 13, NULL,
	ai_run, 13, NULL,
	ai_run, 13, NULL,
	ai_run, 13, NULL
};
mmove_t	floater_move_run = {FLOATER_FRAME_stand101, FLOATER_FRAME_stand152, floater_frames_run, NULL};

void floater_run (edict_t *self)
{
	if (self->monsterinfo.aiflags & AI_STAND_GROUND)
		self->monsterinfo.currentmove = &floater_move_stand1;
	else
		self->monsterinfo.currentmove = &floater_move_run;
}

void floater_walk (edict_t *self)
{
	self->monsterinfo.currentmove = &floater_move_walk;
}

void floater_wham (edict_t *self)
{
	static	vec3_t	aim = {MELEE_DISTANCE, 0, 0};
	gi.sound (self, CHAN_WEAPON, floater_sound_attack3, 1, ATTN_NORM, 0);
	fire_hit (self, aim, 5 + rand() % 6, -50);
}

void floater_zap (edict_t *self)
{
	vec3_t	forward, right;
	vec3_t	origin;
	vec3_t	dir;
	vec3_t	offset;

	VectorSubtract (self->enemy->s.origin, self->s.origin, dir);

	AngleVectors (self->s.angles, forward, right, NULL);
	//FIXME use a flash and replace these two lines with the commented one
	VectorSet (offset, 18.5, -0.9, 10);
	G_ProjectSource (self->s.origin, offset, forward, right, origin);
//	G_ProjectSource (self->s.origin, monster_flash_offset[flash_number], forward, right, origin);

	gi.sound (self, CHAN_WEAPON, floater_sound_attack2, 1, ATTN_NORM, 0);

	//FIXME use the flash, Luke
	bool large_map = LargeMapCheck(origin);
	if (large_map)
		gi.WriteByte (svc_temp_entity_ext);
	else
		gi.WriteByte (svc_temp_entity);
	gi.WriteByte (TE_SPLASH);
	gi.WriteByte (32);
	gi.WritePosition (origin, large_map);
	gi.WriteDir (dir);
	gi.WriteByte (1);	//sparks
	gi.multicast (origin, MULTICAST_PVS);

	T_Damage (self->enemy, self, self, dir, self->enemy->s.origin, vec3_origin, 5 + rand() % 6, -10, DAMAGE_ENERGY, MOD_UNKNOWN);
}

void floater_attack(edict_t *self)
{
	self->monsterinfo.currentmove = &floater_move_attack1;
}


void floater_melee(edict_t *self)
{
	if (random() < 0.5)
		self->monsterinfo.currentmove = &floater_move_attack3;
	else
		self->monsterinfo.currentmove = &floater_move_attack2;
}


void floater_pain (edict_t *self, edict_t *other, float kick, int damage)
{
	int		n;

	if (self->health < (self->max_health / 2))
		self->s.skinnum = 1;

	if (level.time < self->pain_debounce_time)
		return;

	self->pain_debounce_time = level.time + 3;
	if (skill->value == 3)
		return;		// no pain anims in nightmare

	n = (rand() + 1) % 3;
	if (n == 0)
	{
		gi.sound (self, CHAN_VOICE, floater_sound_pain1, 1, ATTN_NORM, 0);
		self->monsterinfo.currentmove = &floater_move_pain1;
	}
	else
	{
		gi.sound (self, CHAN_VOICE, floater_sound_pain2, 1, ATTN_NORM, 0);
		self->monsterinfo.currentmove = &floater_move_pain2;
	}
}

void floater_dead (edict_t *self)
{
	VectorSet (self->mins, -16, -16, -24);
	VectorSet (self->maxs, 16, 16, -8);
	self->movetype = MOVETYPE_TOSS;
	self->svflags |= SVF_DEADMONSTER;
	self->nextthink = 0;
	gi.linkentity (self);

///	monster_reborn(self);
}

void floater_die (edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point)
{
	gi.sound (self, CHAN_VOICE, floater_sound_death1, 1, ATTN_NORM, 0);

//// Berserker: пусть floater разбрасывает gibs при взрыве
	if(net_compatibility->value || !net_fixoverflow->value)
	{
		int	n;
		for (n= 0; n < 2; n++)
			ThrowGib (self, "models/objects/gibs/bone/tris.md2", 1000, GIB_ORGANIC);
		for (n= 0; n < 2; n++)
			ThrowGib (self, "models/objects/gibs/sm_meat/tris.md2", 1000, GIB_ORGANIC);
	}
	else
	{
//		self->s.origin[2] += 16;	// приподымем немного, чтоб джибсы не пропадали под полом
		NetThrowMonsterGibs (self, GIBS_FLOATER, 1000);
//		self->s.origin[2] -= 16;	// вернём взад
	}
////

	BecomeExplosion1(self);
}

/*QUAKED monster_floater (1 .5 0) (-16 -16 -24) (16 16 32) Ambush Trigger_Spawn Sight
*/
void SP_monster_floater (edict_t *self)
{
	if (deathmatch->value)
	{
		G_FreeEdict (self);
		return;
	}

	floater_sound_attack2 = gi.soundindex ("floater/fltatck2.wav");
	floater_sound_attack3 = gi.soundindex ("floater/fltatck3.wav");
	floater_sound_death1 = gi.soundindex ("floater/fltdeth1.wav");
	floater_sound_idle = gi.soundindex ("floater/fltidle1.wav");
	floater_sound_pain1 = gi.soundindex ("floater/fltpain1.wav");
	floater_sound_pain2 = gi.soundindex ("floater/fltpain2.wav");
	floater_sound_sight = gi.soundindex ("floater/fltsght1.wav");

	gi.soundindex ("floater/fltatck1.wav");

	self->s.sound = gi.soundindex ("floater/fltsrch1.wav");

	self->movetype = MOVETYPE_STEP;
	self->solid = SOLID_BBOX;
	self->s.modelindex = gi.modelindex ("models/monsters/float/tris.md2");
	VectorSet (self->mins, -24, -24, -24);
	VectorSet (self->maxs, 24, 24, 32);

	self->health = 200;
	self->gib_health = -80;
	self->mass = 300;

	self->pain = floater_pain;
	self->die = floater_die;

	self->monsterinfo.stand = floater_stand;
	self->monsterinfo.walk = floater_walk;
	self->monsterinfo.run = floater_run;
//	self->monsterinfo.dodge = floater_dodge;
	self->monsterinfo.attack = floater_attack;
	self->monsterinfo.melee = floater_melee;
	self->monsterinfo.sight = floater_sight;
	self->monsterinfo.idle = floater_idle;

	gi.linkentity (self);

	if (random() <= 0.5)
		self->monsterinfo.currentmove = &floater_move_stand1;
	else
		self->monsterinfo.currentmove = &floater_move_stand2;

	self->monsterinfo.scale = FLOATER_MODEL_SCALE;

	flymonster_start (self, false);
}

/*
==============================================================================

end of floater

==============================================================================
*/

/*
==============================================================================
flyQbe
==============================================================================
*/
void flyQbe_stand (edict_t *self);
void flyQbe_attack (edict_t *self);

mframe_t flyQbe_frames_attack_charge[] =
{
	ai_charge,	8,	flyQbe_attack
};
mmove_t	flyQbe_move_attack_charge = {0, 0, flyQbe_frames_attack_charge, NULL};

mframe_t flyQbe_frames_attack_walk[] =
{
	ai_run/*ai_walk*/,	12,	flyQbe_attack
};
mmove_t	flyQbe_move_attack_walk = {0, 0, flyQbe_frames_attack_walk, NULL};

mframe_t flyQbe_frames_stand[] =
{
	ai_stand,	2,	flyQbe_stand
};
mmove_t	flyQbe_move_stand = {0, 0, flyQbe_frames_stand, NULL};

void flyQbe_none (edict_t *self)
{}

void flyQbe_none_sight (edict_t *self, edict_t *other)
{}

#define	NUM_TELEPORT_EFFECT_FRAMES	18		// До 71/4=17.75 кадра на эффект телепортера
void teleport_effect_think(edict_t *self)
{
	if (self->owner)
		VectorCopy (self->owner->s.origin, self->s.origin);
	self->s.frame++;
	if (self->s.frame>NUM_TELEPORT_EFFECT_FRAMES)
		G_FreeEdict(self);
	else
		self->nextthink = level.time;
}

void flyqbe_teleport_effect(edict_t *self, bool enter)
{
	edict_t	*tele = G_Spawn();

	VectorCopy (self->s.origin, tele->s.origin);
	tele->movetype = MOVETYPE_NONE;
	tele->clipmask = 0;
	tele->solid = SOLID_NOT;
	VectorClear (tele->mins);
	VectorClear (tele->maxs);
	if (enter)
	{
		tele->s.modelindex = flyQbe_teleport;
		tele->s.renderfx = RF_ANIM_ONE_CYCLE;	// применять ТОЛЬКО для Berserker-протокола
	}
	else
	{
		tele->s.modelindex = flyQbe_tele_out;
		tele->s.renderfx = RF_LIGHT|RF_ANIM_ONE_CYCLE;	// применять ТОЛЬКО для Berserker-протокола
	}
	tele->s.frame = 0;
	tele->think = teleport_effect_think;
	tele->nextthink = level.time;
	if (enter)
	{
		tele->s.sound = flyqbe_teleport_sound;
		tele->owner = self;
	}
	else
	{
		tele->s.sound = flyqbe_exit_sound;
		tele->owner = NULL;
	}
	gi.linkentity (tele);

	// send muzzle flash
	gi.WriteByte (svc_muzzleflash);
	gi.WriteShort (tele-g_edicts);
	if (enter)
		gi.WriteByte (MZ_NUKE1);
	else
		gi.WriteByte (MZ_NUKE2);
	gi.multicast (tele->s.origin, MULTICAST_PVS);
}

void flyQbe_die (edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point)
{
	num_flyqbes--;
	func_explosive_explode(self, self, self, 100, vec3_origin);
	BecomeExplosion1(self);
}

void flyQbe_exit (edict_t *self)
{	// деактивация
	num_flyqbes--;
	flyqbe_teleport_effect(self, false);
	G_FreeEdict (self);
}

void flyQbe_stand (edict_t *self)
{
	if (level.time >= self->monsterinfo.idle_time-0.3)	// little hack
		flyQbe_exit(self);

///	if (random()<0.7)
	{
		self->velocity[0] = random()*2-1;
		self->velocity[1] = random()*2-1;
		self->velocity[2] = random()*2;
		VectorNormalize(self->velocity);
		VectorScale(self->velocity, 1000*FRAMETIME*random(), self->velocity);
	}

	if (!self->enemy)
		return;

	if (self->enemy->inuse && self->enemy->client && self->enemy->health > 0)
	{	// Переход в боевой режим
		self->monsterinfo.idle_time = level.time + 8 + 4*random();	// Через это время деактивируется (если не было атаки)
		self->monsterinfo.currentmove = &flyQbe_move_attack_walk;
	}
}


void flyQbe_attack (edict_t *self)
{
	bool	barrel = false;
	float	m;
	vec3_t	org, dir;
	vec3_t	vec;
	trace_t	tr;

	if (level.time >= self->monsterinfo.idle_time-0.3)	// little hack
		flyQbe_exit(self);

	if (!self->enemy)
	{
///		if (random()<0.7)
		{
			self->velocity[0] = random()*2-1;
			self->velocity[1] = random()*2-1;
			self->velocity[2] = random()*2;
			VectorNormalize(self->velocity);
			VectorScale(self->velocity, 1000*FRAMETIME*random(), self->velocity);
		}
		return;
	}

	if (!self->enemy->inuse || (self->enemy->client && self->enemy->health <= 0))
	{	// Переход в мирный режим
		self->monsterinfo.aiflags |= AI_GOOD_GUY;	// игнорировать клиентов, стать "доброй" мухой
		self->monsterinfo.idle_time = level.time + 3 + 2*random();	// Через это время деактивируется (если не было атаки)
		self->monsterinfo.currentmove = &flyQbe_move_stand;
		return;
	}

	if (VectorCompare(self->pos2, vec3_origin))	// для первого раза мухе полюбому бэкапим координаты жертвы, чтоб не пряталась
		VectorCopy(self->enemy->s.origin, self->pos2);	// pos2 is not using for monsters, so will be use it for backup enemy->origin

	tr = gi.trace (self->s.origin, NULL, NULL, self->enemy->s.origin, self, MASK_OPAQUE);
	if (tr.fraction<1)	// Между мухой и жертвой - стена, яд или лава.
	{	// муха не видит жертву!
		VectorSubtract(self->pos2, self->s.origin, vec);
		VectorNormalize(vec);
		VectorScale(vec, 1800*FRAMETIME, vec);	// ускорим муху чтоб побыстрее она опять увидела жертву
		VectorAdd(self->velocity, vec, self->velocity);
///		if (random()<0.5)
///			self->velocity[2] += 1600*FRAMETIME;
		return;
	}

	// муха видит жертву!
	VectorCopy(self->enemy->s.origin, self->pos2);	// pos2 is not using for monsters, so will be use it for backup enemy->origin
	if (!(self->monsterinfo.aiflags & AI_GOOD_GUY))
		if (random()<0.5)
		{	// иногда корректируем полет мухи к клиенту
			VectorSubtract(self->pos2, self->s.origin, vec);
			VectorNormalize(vec);
			VectorScale(vec, 800*random()*FRAMETIME, vec);
			VectorAdd(self->velocity, vec, self->velocity);
			if (random()<0.4)
				self->velocity[2] += 1000*FRAMETIME;
		}

	if (level.time >= self->delay)
	{
		if (random()<=0.85)
			self->delay = level.time + 0.3;				// стрелять через каждую четверть секунды
		else
			self->delay = level.time + 1 + random();	// пауза отдыха

		tr = gi.trace (self->s.origin, NULL, NULL, self->enemy->s.origin, self, MASK_SHOT);
		if (tr.fraction < 1)
		{
			if (strcmp(tr.ent->classname, "misc_explobox") && strcmp(tr.ent->classname, "func_explosive"))
			{
				if (!tr.ent->takedamage)
					return;
				if (!(tr.ent->svflags & SVF_MONSTER) && (!tr.ent->client))
					return;
				if (tr.ent != self->enemy)
					return;			// Чтоб не могли прикрыться чужим телом
			}
			else
				barrel = true;
		}
		else
			tr.ent = self->enemy;

		self->monsterinfo.idle_time = level.time + 12;	// Через это время деактивируется (если не было атаки и муха видела жертву)

		if(self->enemy->movetype == MOVETYPE_NOCLIP)
			return;

		if (tr.ent)
		{
			org[0] = tr.ent->s.origin[0];
			org[1] = tr.ent->s.origin[1];
			org[2] = tr.ent->s.origin[2]-12;	// чуть ниже...
			VectorSubtract (org, self->s.origin, dir);
			float dst = VectorNormalize(dir);
			if (dst>=512)
				return;		// too far
			int dmg;
			if (barrel)
			{
				dmg = 9999;
				m = 0;
			}
			else
			{
				if (tr.ent->client)
				{
					m = 32;
					if (dst <= m)	// chainsaw
						dmg = 2;
					else
						dmg = 1;
				}
				else
				{
					m = 12;
					dmg = 1;
				}
			}
			if (dst<=m)
			{	// chainsaw
				gi.sound (self, CHAN_WEAPON, flyQbe_sound_attack2, 1, ATTN_NORM, 0);
				T_Damage (tr.ent, self, self, dir, org, vec3_origin, dmg, 8, DAMAGE_RADIUS, MOD_UNKNOWN);
				self->delay = level.time + 0.25;		// пилить без отдыха
			}
			else
			{	// green laser
				gi.sound (self, CHAN_WEAPON, flyQbe_sound_attack, 1, ATTN_NORM, 0);

				bool large_map = LargeMapCheck(self->s.origin) | LargeMapCheck(org);
				if (large_map)
					gi.WriteByte (svc_temp_entity_ext);
				else
					gi.WriteByte (svc_temp_entity);
				gi.WriteByte (TE_BFG_LASER);
				gi.WritePosition (self->s.origin, large_map);
				gi.WritePosition (org, large_map);
				gi.multicast (self->s.origin, MULTICAST_PHS);

				T_Damage (tr.ent, self, self, dir, org, vec3_origin, dmg, 5, DAMAGE_ENERGY, MOD_TARGET_LASER);
			}
		}
	}
}

void flyQbe_pain (edict_t *self, edict_t *other, float kick, int damage)
{
	if (other->client)								// если муху обидел клиент,
	{
		float m;
		self->monsterinfo.aiflags &= ~AI_GOOD_GUY;	// то стать "злой" мухой
		self->enemy = other;						// и сразу охотиться на клиента
		self->monsterinfo.currentmove = &flyQbe_move_attack_walk;
		if (random()<0.5)
			m=2000*FRAMETIME;
		else if (random()<0.8)
			m=-2000*FRAMETIME;
		else
			m=0;
		self->velocity[2] += m;
		return;
	}

	if ((other->svflags & SVF_MONSTER))
	{
		self->enemy = other;
		return;
	}
}

void SP_monster_flyQbe (edict_t *fly)
{
	vec3_t	org, mins, maxs, dir;
	trace_t	tr;

	if (deathmatch->value)
		return;

	if (num_flyqbes>=MAX_FLYQBES)	// Не более MAX_FLYQBES кубмух на карте
		return;

	VectorSet (mins, -4, -4, -4);
	VectorSet (maxs, 4, 4, 4);

	dir[0] = random()*2-1;
	dir[1] = random()*2-1;
	dir[2] = random();			// Только выше трупа...
	if (VectorNormalize(dir))
	{
		VectorMA (fly->s.origin, 512, dir, org);
		tr = gi.trace (fly->s.origin, mins, maxs, org, NULL, MASK_SOLID);
		VectorMA (tr.endpos, -16, dir, org);
		// check new position
		tr = gi.trace (org, mins, maxs, org, NULL, MASK_SOLID);
		if (tr.startsolid)
			return;
	}
	else
		return;

	edict_t	*self = G_Spawn();
	self->enemy = fly;
	self->s.origin[0] = org[0];
	self->s.origin[1] = org[1];
	self->s.origin[2] = org[2];
	KillBox (self);

	flyqbe_teleport_effect(self, true);

	self->s.sound = flyQbe_sound_fly;

	self->movetype = MOVETYPE_STEP;
	self->solid = SOLID_BBOX;
	self->s.modelindex = flyQbe_model;
	VectorCopy (mins, self->mins);
	VectorCopy (maxs, self->maxs);

	switch ((int)skill->value)
	{
		case 0:
			self->health = 10;
			break;
		case 1:
			self->health = 20;
			break;
		case 2:
			self->health = 40;
			break;
		default:
			self->health = 80;
	}

	self->mass = 50;

	self->pain = flyQbe_pain;
	self->die = flyQbe_die;
	self->monsterinfo.stand = flyQbe_none;
	self->monsterinfo.walk = flyQbe_none;
	self->monsterinfo.run = flyQbe_none;
	self->monsterinfo.attack = flyQbe_none;
	self->monsterinfo.melee = flyQbe_none;
	self->monsterinfo.sight = flyQbe_none_sight;
	self->monsterinfo.idle = flyQbe_none;
	self->monsterinfo.idle_time = level.time + 8 + 4*random();	// Через это время деактивируется (если не было атаки)
	self->monsterinfo.currentmove = &flyQbe_move_attack_charge;
	self->monsterinfo.scale = 1;
	self->monsterinfo.aiflags = AI_GOOD_GUY;	// игнорировать клиентов, быть "доброй" мухой
	VectorClear(self->pos2);

	// начнет действовать через это время:
	self->delay = level.time + 4 + (2*random());

	gi.linkentity (self);
	flymonster_start (self, true);
	num_flyqbes++;
}
/*
==============================================================================
end of flyQbe
==============================================================================
*/


/*
==============================================================================

chick (bitch)

==============================================================================
*/

void chick_stand (edict_t *self);
void chick_run (edict_t *self);
void chick_reslash(edict_t *self);
void chick_rerocket(edict_t *self);
void chick_attack1(edict_t *self);


void ChickMoan (edict_t *self)
{
	if (random() < 0.5)
		gi.sound (self, CHAN_VOICE, chick_sound_idle1, 1, ATTN_IDLE, 0);
	else
		gi.sound (self, CHAN_VOICE, chick_sound_idle2, 1, ATTN_IDLE, 0);
}

mframe_t chick_frames_fidget [] =
{
	ai_stand, 0,  NULL,
	ai_stand, 0,  NULL,
	ai_stand, 0,  NULL,
	ai_stand, 0,  NULL,
	ai_stand, 0,  NULL,
	ai_stand, 0,  NULL,
	ai_stand, 0,  NULL,
	ai_stand, 0,  NULL,
	ai_stand, 0,  ChickMoan,
	ai_stand, 0,  NULL,
	ai_stand, 0,  NULL,
	ai_stand, 0,  NULL,
	ai_stand, 0,  NULL,
	ai_stand, 0,  NULL,
	ai_stand, 0,  NULL,
	ai_stand, 0,  NULL,
	ai_stand, 0,  NULL,
	ai_stand, 0,  NULL,
	ai_stand, 0,  NULL,
	ai_stand, 0,  NULL,
	ai_stand, 0,  NULL,
	ai_stand, 0,  NULL,
	ai_stand, 0,  NULL,
	ai_stand, 0,  NULL,
	ai_stand, 0,  NULL,
	ai_stand, 0,  NULL,
	ai_stand, 0,  NULL,
	ai_stand, 0,  NULL,
	ai_stand, 0,  NULL,
	ai_stand, 0,  NULL
};
mmove_t chick_move_fidget = {BITCH_FRAME_stand201, BITCH_FRAME_stand230, chick_frames_fidget, chick_stand};

void chick_fidget (edict_t *self)
{
	if (self->monsterinfo.aiflags & AI_STAND_GROUND)
		return;
	if (random() <= 0.3)
		self->monsterinfo.currentmove = &chick_move_fidget;
}

mframe_t chick_frames_stand [] =
{
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, chick_fidget,

};
mmove_t chick_move_stand = {BITCH_FRAME_stand101, BITCH_FRAME_stand130, chick_frames_stand, NULL};

void chick_stand (edict_t *self)
{
	self->monsterinfo.currentmove = &chick_move_stand;
}

mframe_t chick_frames_start_run [] =
{
	ai_run, 1,  NULL,
	ai_run, 0,  NULL,
	ai_run, 0,	 NULL,
	ai_run, -1, NULL,
	ai_run, -1, NULL,
	ai_run, 0,  NULL,
	ai_run, 1,  NULL,
	ai_run, 3,  NULL,
	ai_run, 6,	 NULL,
	ai_run, 3,	 NULL
};
mmove_t chick_move_start_run = {BITCH_FRAME_walk01, BITCH_FRAME_walk10, chick_frames_start_run, chick_run};

mframe_t chick_frames_run [] =
{
	ai_run, 6,	NULL,
	ai_run, 8,  NULL,
	ai_run, 13, NULL,
	ai_run, 5,  NULL,
	ai_run, 7,  NULL,
	ai_run, 4,  NULL,
	ai_run, 11, NULL,
	ai_run, 5,  NULL,
	ai_run, 9,  NULL,
	ai_run, 7,  NULL

};

mmove_t chick_move_run = {BITCH_FRAME_walk11, BITCH_FRAME_walk20, chick_frames_run, NULL};

mframe_t chick_frames_walk [] =
{
	ai_walk, 6,	 NULL,
	ai_walk, 8,  NULL,
	ai_walk, 13, NULL,
	ai_walk, 5,  NULL,
	ai_walk, 7,  NULL,
	ai_walk, 4,  NULL,
	ai_walk, 11, NULL,
	ai_walk, 5,  NULL,
	ai_walk, 9,  NULL,
	ai_walk, 7,  NULL
};

mmove_t chick_move_walk = {BITCH_FRAME_walk11, BITCH_FRAME_walk20, chick_frames_walk, NULL};

void chick_walk (edict_t *self)
{
	self->monsterinfo.currentmove = &chick_move_walk;
}

void chick_run (edict_t *self)
{
	if (self->monsterinfo.aiflags & AI_STAND_GROUND)
	{
		self->monsterinfo.currentmove = &chick_move_stand;
		return;
	}

	if (self->monsterinfo.currentmove == &chick_move_walk ||
		self->monsterinfo.currentmove == &chick_move_start_run)
	{
		self->monsterinfo.currentmove = &chick_move_run;
	}
	else
	{
		self->monsterinfo.currentmove = &chick_move_start_run;
	}
}

mframe_t chick_frames_pain1 [] =
{
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL
};
mmove_t chick_move_pain1 = {BITCH_FRAME_pain101, BITCH_FRAME_pain105, chick_frames_pain1, chick_run};

mframe_t chick_frames_pain2 [] =
{
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL
};
mmove_t chick_move_pain2 = {BITCH_FRAME_pain201, BITCH_FRAME_pain205, chick_frames_pain2, chick_run};

mframe_t chick_frames_pain3 [] =
{
	ai_move, 0,		NULL,
	ai_move, 0,		NULL,
	ai_move, -6,	NULL,
	ai_move, 3,		NULL,
	ai_move, 11,	NULL,
	ai_move, 3,		NULL,
	ai_move, 0,		NULL,
	ai_move, 0,		NULL,
	ai_move, 4,		NULL,
	ai_move, 1,		NULL,
	ai_move, 0,		NULL,
	ai_move, -3,	NULL,
	ai_move, -4,	NULL,
	ai_move, 5,		NULL,
	ai_move, 7,		NULL,
	ai_move, -2,	NULL,
	ai_move, 3,		NULL,
	ai_move, -5,	NULL,
	ai_move, -2,	NULL,
	ai_move, -8,	NULL,
	ai_move, 2,		NULL
};
mmove_t chick_move_pain3 = {BITCH_FRAME_pain301, BITCH_FRAME_pain321, chick_frames_pain3, chick_run};

void chick_pain (edict_t *self, edict_t *other, float kick, int damage)
{
	float	r;

	if (self->health < (self->max_health / 2))
		self->s.skinnum = 1;

	if (level.time < self->pain_debounce_time)
		return;

	self->pain_debounce_time = level.time + 3;

	r = random();
	if (r < 0.33)
		gi.sound (self, CHAN_VOICE, chick_sound_pain1, 1, ATTN_NORM, 0);
	else if (r < 0.66)
		gi.sound (self, CHAN_VOICE, chick_sound_pain2, 1, ATTN_NORM, 0);
	else
		gi.sound (self, CHAN_VOICE, chick_sound_pain3, 1, ATTN_NORM, 0);

	if (skill->value == 3)
		return;		// no pain anims in nightmare

	if (damage <= 10)
		self->monsterinfo.currentmove = &chick_move_pain1;
	else if (damage <= 25)
		self->monsterinfo.currentmove = &chick_move_pain2;
	else
		self->monsterinfo.currentmove = &chick_move_pain3;
}

void chick_dead (edict_t *self)
{
	VectorSet (self->mins, -16, -16, 0);
	VectorSet (self->maxs, 16, 16, 16);
	self->movetype = MOVETYPE_TOSS;
	self->svflags |= SVF_DEADMONSTER;
	self->nextthink = 0;
	gi.linkentity (self);

	M_FlyCheck (self);

	monster_reborn(self);
}

mframe_t chick_frames_death2 [] =
{
	ai_move, -6, NULL,
	ai_move, 0,  NULL,
	ai_move, -1,  NULL,
	ai_move, -5, NULL,
	ai_move, 0, NULL,
	ai_move, -1,  NULL,
	ai_move, -2,  NULL,
	ai_move, 1,  NULL,
	ai_move, 10, NULL,
	ai_move, 2,  NULL,
	ai_move, 3,  NULL,
	ai_move, 1,  NULL,
	ai_move, 2, NULL,
	ai_move, 0,  NULL,
	ai_move, 3,  NULL,
	ai_move, 3,  NULL,
	ai_move, 1,  NULL,
	ai_move, -3,  NULL,
	ai_move, -5, NULL,
	ai_move, 4, NULL,
	ai_move, 15, NULL,
	ai_move, 14, NULL,
	ai_move, 1, NULL
};
mmove_t chick_move_death2 = {BITCH_FRAME_death201, BITCH_FRAME_death223, chick_frames_death2, chick_dead};

mframe_t chick_frames_death1 [] =
{
	ai_move, 0,  NULL,
	ai_move, 0,  NULL,
	ai_move, -7, NULL,
	ai_move, 4,  NULL,
	ai_move, 11, NULL,
	ai_move, 0,  NULL,
	ai_move, 0,  NULL,
	ai_move, 0,  NULL,
	ai_move, 0,  NULL,
	ai_move, 0,  NULL,
	ai_move, 0,  NULL,
	ai_move, 0,  NULL

};
mmove_t chick_move_death1 = {BITCH_FRAME_death101, BITCH_FRAME_death112, chick_frames_death1, chick_dead};

void chick_die (edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point)
{
	int		n;

// check for gib
	if (self->health <= self->gib_health)
	{
		gi.sound (self, CHAN_VOICE, gi.soundindex ("misc/udeath.wav"), 1, ATTN_NORM, 0);
		if(net_compatibility->value || !net_fixoverflow->value)
		{
			for (n= 0; n < 2; n++)
				ThrowGib (self, "models/objects/gibs/bone/tris.md2", damage, GIB_ORGANIC);
			for (n= 0; n < 4; n++)
				ThrowGib (self, "models/objects/gibs/sm_meat/tris.md2", damage, GIB_ORGANIC);
			ThrowHead (self, "models/objects/gibs/head2/tris.md2", damage, GIB_ORGANIC);
			self->deadflag = DEAD_DEAD;
		}
		else
		{
			NetThrowMonsterGibs (self, GIBS_CHICK, damage);
			G_FreeEdict (self);
		}
		return;
	}

	if (self->deadflag == DEAD_DEAD)
		return;

// regular death
	self->deadflag = DEAD_DEAD;
	self->takedamage = DAMAGE_YES;

	n = rand() % 2;
	if (n == 0)
	{
		self->monsterinfo.currentmove = &chick_move_death1;
		gi.sound (self, CHAN_VOICE, chick_sound_death1, 1, ATTN_NORM, 0);
	}
	else
	{
		self->monsterinfo.currentmove = &chick_move_death2;
		gi.sound (self, CHAN_VOICE, chick_sound_death2, 1, ATTN_NORM, 0);
	}
}


void chick_duck_down (edict_t *self)
{
	if (self->monsterinfo.aiflags & AI_DUCKED)
		return;
	self->monsterinfo.aiflags |= AI_DUCKED;
	self->maxs[2] -= 32;
	self->takedamage = DAMAGE_YES;
	self->monsterinfo.pausetime = level.time + 1;
	gi.linkentity (self);
}

void chick_duck_hold (edict_t *self)
{
	if (level.time >= self->monsterinfo.pausetime)
		self->monsterinfo.aiflags &= ~AI_HOLD_FRAME;
	else
		self->monsterinfo.aiflags |= AI_HOLD_FRAME;
}

void chick_duck_up (edict_t *self)
{
	self->monsterinfo.aiflags &= ~AI_DUCKED;
	self->maxs[2] += 32;
	self->takedamage = DAMAGE_AIM;
	gi.linkentity (self);
}

mframe_t chick_frames_duck [] =
{
	ai_move, 0, chick_duck_down,
	ai_move, 1, NULL,
	ai_move, 4, chick_duck_hold,
	ai_move, -4,  NULL,
	ai_move, -5,  chick_duck_up,
	ai_move, 3, NULL,
	ai_move, 1,  NULL
};
mmove_t chick_move_duck = {BITCH_FRAME_duck01, BITCH_FRAME_duck07, chick_frames_duck, chick_run};

void chick_dodge (edict_t *self, edict_t *attacker, float eta)
{
	if (random() > 0.25)
		return;

	if (!self->enemy)
		self->enemy = attacker;

	self->monsterinfo.currentmove = &chick_move_duck;
}

void ChickSlash (edict_t *self)
{
	vec3_t	aim;

	VectorSet (aim, MELEE_DISTANCE, self->mins[0], 10);
	gi.sound (self, CHAN_WEAPON, chick_sound_melee_swing, 1, ATTN_NORM, 0);
	fire_hit (self, aim, (10 + (rand() %6)), 100);
}


void ChickRocket (edict_t *self)
{
	vec3_t	forward, right;
	vec3_t	start;
	vec3_t	dir;
	vec3_t	vec;

	AngleVectors (self->s.angles, forward, right, NULL);
	G_ProjectSource (self->s.origin, &monster_flash_offset[MZ2_CHICK_ROCKET_1*3], forward, right, start);

	VectorCopy (self->enemy->s.origin, vec);
	vec[2] += self->enemy->viewheight;
	VectorSubtract (vec, start, dir);
	VectorNormalize (dir);

	monster_fire_rocket (self, start, dir, 50, 500, MZ2_CHICK_ROCKET_1);
}

void Chick_PreAttack1 (edict_t *self)
{
	gi.sound (self, CHAN_VOICE, chick_sound_missile_prelaunch, 1, ATTN_NORM, 0);
}

void ChickReload (edict_t *self)
{
	gi.sound (self, CHAN_VOICE, chick_sound_missile_reload, 1, ATTN_NORM, 0);
}


mframe_t chick_frames_start_attack1 [] =
{
	ai_charge, 0,	Chick_PreAttack1,
	ai_charge, 0,	NULL,
	ai_charge, 0,	NULL,
	ai_charge, 4,	NULL,
	ai_charge, 0,	NULL,
	ai_charge, -3,  NULL,
	ai_charge, 3,	NULL,
	ai_charge, 5,	NULL,
	ai_charge, 7,	NULL,
	ai_charge, 0,	NULL,
	ai_charge, 0,	NULL,
	ai_charge, 0,	NULL,
	ai_charge, 0,	chick_attack1
};
mmove_t chick_move_start_attack1 = {BITCH_FRAME_attak101, BITCH_FRAME_attak113, chick_frames_start_attack1, NULL};


mframe_t chick_frames_attack1 [] =
{
	ai_charge, 19,	ChickRocket,
	ai_charge, -6,	NULL,
	ai_charge, -5,	NULL,
	ai_charge, -2,	NULL,
	ai_charge, -7,	NULL,
	ai_charge, 0,	NULL,
	ai_charge, 1,	NULL,
	ai_charge, 10,	ChickReload,
	ai_charge, 4,	NULL,
	ai_charge, 5,	NULL,
	ai_charge, 6,	NULL,
	ai_charge, 6,	NULL,
	ai_charge, 4,	NULL,
	ai_charge, 3,	chick_rerocket

};
mmove_t chick_move_attack1 = {BITCH_FRAME_attak114, BITCH_FRAME_attak127, chick_frames_attack1, NULL};

mframe_t chick_frames_end_attack1 [] =
{
	ai_charge, -3,	NULL,
	ai_charge, 0,	NULL,
	ai_charge, -6,	NULL,
	ai_charge, -4,	NULL,
	ai_charge, -2,  NULL
};
mmove_t chick_move_end_attack1 = {BITCH_FRAME_attak128, BITCH_FRAME_attak132, chick_frames_end_attack1, chick_run};

void chick_rerocket(edict_t *self)
{
	if (self->enemy->health > 0)
	{
		if (range (self, self->enemy) > RANGE_MELEE)
			if ( visible (self, self->enemy) )
				if (random() <= 0.6)
				{
					self->monsterinfo.currentmove = &chick_move_attack1;
					return;
				}
	}
	self->monsterinfo.currentmove = &chick_move_end_attack1;
}

void chick_attack1(edict_t *self)
{
	self->monsterinfo.currentmove = &chick_move_attack1;
}

mframe_t chick_frames_slash [] =
{
	ai_charge, 1,	NULL,
	ai_charge, 7,	ChickSlash,
	ai_charge, -7,	NULL,
	ai_charge, 1,	NULL,
	ai_charge, -1,	NULL,
	ai_charge, 1,	NULL,
	ai_charge, 0,	NULL,
	ai_charge, 1,	NULL,
	ai_charge, -2,	chick_reslash
};
mmove_t chick_move_slash = {BITCH_FRAME_attak204, BITCH_FRAME_attak212, chick_frames_slash, NULL};

mframe_t chick_frames_end_slash [] =
{
	ai_charge, -6,	NULL,
	ai_charge, -1,	NULL,
	ai_charge, -6,	NULL,
	ai_charge, 0,	NULL
};
mmove_t chick_move_end_slash = {BITCH_FRAME_attak213, BITCH_FRAME_attak216, chick_frames_end_slash, chick_run};


void chick_reslash(edict_t *self)
{
	if (self->enemy->health > 0)
	{
		if (range (self, self->enemy) == RANGE_MELEE)
			if (random() <= 0.9)
			{
				self->monsterinfo.currentmove = &chick_move_slash;
				return;
			}
			else
			{
				self->monsterinfo.currentmove = &chick_move_end_slash;
				return;
			}
	}
	self->monsterinfo.currentmove = &chick_move_end_slash;
}

void chick_slash(edict_t *self)
{
	self->monsterinfo.currentmove = &chick_move_slash;
}


mframe_t chick_frames_start_slash [] =
{
	ai_charge, 1,	NULL,
	ai_charge, 8,	NULL,
	ai_charge, 3,	NULL
};
mmove_t chick_move_start_slash = {BITCH_FRAME_attak201, BITCH_FRAME_attak203, chick_frames_start_slash, chick_slash};



void chick_melee(edict_t *self)
{
	self->monsterinfo.currentmove = &chick_move_start_slash;
}


void chick_attack(edict_t *self)
{
	self->monsterinfo.currentmove = &chick_move_start_attack1;
}

void chick_sight(edict_t *self, edict_t *other)
{
	gi.sound (self, CHAN_VOICE, chick_sound_sight, 1, ATTN_NORM, 0);
}

/*QUAKED monster_chick (1 .5 0) (-16 -16 -24) (16 16 32) Ambush Trigger_Spawn Sight
*/
void SP_monster_chick (edict_t *self)
{
	if (deathmatch->value)
	{
		G_FreeEdict (self);
		return;
	}

	chick_sound_missile_prelaunch	= gi.soundindex ("chick/chkatck1.wav");
	chick_sound_missile_launch	= gi.soundindex ("chick/chkatck2.wav");
	chick_sound_melee_swing		= gi.soundindex ("chick/chkatck3.wav");
	chick_sound_melee_hit			= gi.soundindex ("chick/chkatck4.wav");
	chick_sound_missile_reload	= gi.soundindex ("chick/chkatck5.wav");
	chick_sound_death1			= gi.soundindex ("chick/chkdeth1.wav");
	chick_sound_death2			= gi.soundindex ("chick/chkdeth2.wav");
	chick_sound_fall_down			= gi.soundindex ("chick/chkfall1.wav");
	chick_sound_idle1				= gi.soundindex ("chick/chkidle1.wav");
	chick_sound_idle2				= gi.soundindex ("chick/chkidle2.wav");
	chick_sound_pain1				= gi.soundindex ("chick/chkpain1.wav");
	chick_sound_pain2				= gi.soundindex ("chick/chkpain2.wav");
	chick_sound_pain3				= gi.soundindex ("chick/chkpain3.wav");
	chick_sound_sight				= gi.soundindex ("chick/chksght1.wav");
	chick_sound_search			= gi.soundindex ("chick/chksrch1.wav");

	self->movetype = MOVETYPE_STEP;
	self->solid = SOLID_BBOX;
	self->s.modelindex = gi.modelindex ("models/monsters/bitch/tris.md2");
	VectorSet (self->mins, -16, -16, 0);
	VectorSet (self->maxs, 16, 16, 56);

	self->health = 175;
	self->gib_health = -70;
	self->mass = 200;

	self->pain = chick_pain;
	self->die = chick_die;

	self->monsterinfo.stand = chick_stand;
	self->monsterinfo.walk = chick_walk;
	self->monsterinfo.run = chick_run;
	self->monsterinfo.dodge = chick_dodge;
	self->monsterinfo.attack = chick_attack;
	self->monsterinfo.melee = chick_melee;
	self->monsterinfo.sight = chick_sight;

	gi.linkentity (self);

	self->monsterinfo.currentmove = &chick_move_stand;
	self->monsterinfo.scale = BITCH_MODEL_SCALE;

	walkmonster_start (self, false);
}

/*
==============================================================================

end of chick (bitch)

==============================================================================
*/



/*
==============================================================================

FLIPPER

==============================================================================
*/

void flipper_stand (edict_t *self);

mframe_t flipper_frames_stand [] =
{
	ai_stand, 0, NULL
};

mmove_t	flipper_move_stand = {FLIPPER_FRAME_flphor01, FLIPPER_FRAME_flphor01, flipper_frames_stand, NULL};

void flipper_stand (edict_t *self)
{
		self->monsterinfo.currentmove = &flipper_move_stand;
}

#define FLIPPER_RUN_SPEED	24

mframe_t flipper_frames_run [] =
{
	ai_run, FLIPPER_RUN_SPEED, NULL,	// 6
	ai_run, FLIPPER_RUN_SPEED, NULL,
	ai_run, FLIPPER_RUN_SPEED, NULL,
	ai_run, FLIPPER_RUN_SPEED, NULL,
	ai_run, FLIPPER_RUN_SPEED, NULL,	// 10

	ai_run, FLIPPER_RUN_SPEED, NULL,
	ai_run, FLIPPER_RUN_SPEED, NULL,
	ai_run, FLIPPER_RUN_SPEED, NULL,
	ai_run, FLIPPER_RUN_SPEED, NULL,
	ai_run, FLIPPER_RUN_SPEED, NULL,
	ai_run, FLIPPER_RUN_SPEED, NULL,
	ai_run, FLIPPER_RUN_SPEED, NULL,
	ai_run, FLIPPER_RUN_SPEED, NULL,
	ai_run, FLIPPER_RUN_SPEED, NULL,
	ai_run, FLIPPER_RUN_SPEED, NULL,	// 20

	ai_run, FLIPPER_RUN_SPEED, NULL,
	ai_run, FLIPPER_RUN_SPEED, NULL,
	ai_run, FLIPPER_RUN_SPEED, NULL,
	ai_run, FLIPPER_RUN_SPEED, NULL,
	ai_run, FLIPPER_RUN_SPEED, NULL,
	ai_run, FLIPPER_RUN_SPEED, NULL,
	ai_run, FLIPPER_RUN_SPEED, NULL,
	ai_run, FLIPPER_RUN_SPEED, NULL,
	ai_run, FLIPPER_RUN_SPEED, NULL		// 29
};
mmove_t flipper_move_run_loop = {FLIPPER_FRAME_flpver06, FLIPPER_FRAME_flpver29, flipper_frames_run, NULL};

void flipper_run_loop (edict_t *self)
{
	self->monsterinfo.currentmove = &flipper_move_run_loop;
}

mframe_t flipper_frames_run_start [] =
{
	ai_run, 8, NULL,
	ai_run, 8, NULL,
	ai_run, 8, NULL,
	ai_run, 8, NULL,
	ai_run, 8, NULL,
	ai_run, 8, NULL
};
mmove_t flipper_move_run_start = {FLIPPER_FRAME_flpver01, FLIPPER_FRAME_flpver06, flipper_frames_run_start, flipper_run_loop};

void flipper_run (edict_t *self)
{
	self->monsterinfo.currentmove = &flipper_move_run_start;
}

/* Standard Swimming */
mframe_t flipper_frames_walk [] =
{
	ai_walk, 4, NULL,
	ai_walk, 4, NULL,
	ai_walk, 4, NULL,
	ai_walk, 4, NULL,
	ai_walk, 4, NULL,
	ai_walk, 4, NULL,
	ai_walk, 4, NULL,
	ai_walk, 4, NULL,
	ai_walk, 4, NULL,
	ai_walk, 4, NULL,
	ai_walk, 4, NULL,
	ai_walk, 4, NULL,
	ai_walk, 4, NULL,
	ai_walk, 4, NULL,
	ai_walk, 4, NULL,
	ai_walk, 4, NULL,
	ai_walk, 4, NULL,
	ai_walk, 4, NULL,
	ai_walk, 4, NULL,
	ai_walk, 4, NULL,
	ai_walk, 4, NULL,
	ai_walk, 4, NULL,
	ai_walk, 4, NULL,
	ai_walk, 4, NULL
};
mmove_t flipper_move_walk = {FLIPPER_FRAME_flphor01, FLIPPER_FRAME_flphor24, flipper_frames_walk, NULL};

void flipper_walk (edict_t *self)
{
	self->monsterinfo.currentmove = &flipper_move_walk;
}

mframe_t flipper_frames_start_run [] =
{
	ai_run, 8, NULL,
	ai_run, 8, NULL,
	ai_run, 8, NULL,
	ai_run, 8, NULL,
	ai_run, 8, flipper_run
};
mmove_t flipper_move_start_run = {FLIPPER_FRAME_flphor01, FLIPPER_FRAME_flphor05, flipper_frames_start_run, NULL};

void flipper_start_run (edict_t *self)
{
	self->monsterinfo.currentmove = &flipper_move_start_run;
}

mframe_t flipper_frames_pain2 [] =
{
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0,	NULL,
	ai_move, 0,	NULL,
	ai_move, 0, NULL
};
mmove_t flipper_move_pain2 = {FLIPPER_FRAME_flppn101, FLIPPER_FRAME_flppn105, flipper_frames_pain2, flipper_run};

mframe_t flipper_frames_pain1 [] =
{
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0,	NULL,
	ai_move, 0,	NULL,
	ai_move, 0, NULL
};
mmove_t flipper_move_pain1 = {FLIPPER_FRAME_flppn201, FLIPPER_FRAME_flppn205, flipper_frames_pain1, flipper_run};

void flipper_bite (edict_t *self)
{
	vec3_t	aim;

	VectorSet (aim, MELEE_DISTANCE, 0, 0);
	fire_hit (self, aim, 5, 0);
}

void flipper_preattack (edict_t *self)
{
	gi.sound (self, CHAN_WEAPON, flipper_sound_chomp, 1, ATTN_NORM, 0);
}

mframe_t flipper_frames_attack [] =
{
	ai_charge, 0,	flipper_preattack,
	ai_charge, 0,	NULL,
	ai_charge, 0,	NULL,
	ai_charge, 0,	NULL,
	ai_charge, 0,	NULL,
	ai_charge, 0,	NULL,
	ai_charge, 0,	NULL,
	ai_charge, 0,	NULL,
	ai_charge, 0,	NULL,
	ai_charge, 0,	NULL,
	ai_charge, 0,	NULL,
	ai_charge, 0,	NULL,
	ai_charge, 0,	NULL,
	ai_charge, 0,	flipper_bite,
	ai_charge, 0,	NULL,
	ai_charge, 0,	NULL,
	ai_charge, 0,	NULL,
	ai_charge, 0,	NULL,
	ai_charge, 0,	flipper_bite,
	ai_charge, 0,	NULL
};
mmove_t flipper_move_attack = {FLIPPER_FRAME_flpbit01, FLIPPER_FRAME_flpbit20, flipper_frames_attack, flipper_run};

void flipper_melee(edict_t *self)
{
	self->monsterinfo.currentmove = &flipper_move_attack;
}

void flipper_pain (edict_t *self, edict_t *other, float kick, int damage)
{
	int		n;

	if (self->health < (self->max_health / 2))
		self->s.skinnum = 1;

	if (level.time < self->pain_debounce_time)
		return;

	self->pain_debounce_time = level.time + 3;

	if (skill->value == 3)
		return;		// no pain anims in nightmare

	n = (rand() + 1) % 2;
	if (n == 0)
	{
		gi.sound (self, CHAN_VOICE, flipper_sound_pain1, 1, ATTN_NORM, 0);
		self->monsterinfo.currentmove = &flipper_move_pain1;
	}
	else
	{
		gi.sound (self, CHAN_VOICE, flipper_sound_pain2, 1, ATTN_NORM, 0);
		self->monsterinfo.currentmove = &flipper_move_pain2;
	}
}

void flipper_dead (edict_t *self)
{
	VectorSet (self->mins, -16, -16, -24);
	VectorSet (self->maxs, 16, 16, -8);
	self->movetype = MOVETYPE_TOSS;
	self->svflags |= SVF_DEADMONSTER;
	self->nextthink = 0;
	gi.linkentity (self);

	monster_reborn(self);
}

mframe_t flipper_frames_death [] =
{
	ai_move, 0,	 NULL,
	ai_move, 0,	 NULL,
	ai_move, 0,	 NULL,
	ai_move, 0,	 NULL,
	ai_move, 0,	 NULL,
	ai_move, 0,	 NULL,
	ai_move, 0,	 NULL,
	ai_move, 0,	 NULL,
	ai_move, 0,	 NULL,
	ai_move, 0,	 NULL,

	ai_move, 0,	 NULL,
	ai_move, 0,	 NULL,
	ai_move, 0,	 NULL,
	ai_move, 0,	 NULL,
	ai_move, 0,	 NULL,
	ai_move, 0,	 NULL,
	ai_move, 0,	 NULL,
	ai_move, 0,	 NULL,
	ai_move, 0,	 NULL,
	ai_move, 0,	 NULL,

	ai_move, 0,	 NULL,
	ai_move, 0,	 NULL,
	ai_move, 0,	 NULL,
	ai_move, 0,	 NULL,
	ai_move, 0,	 NULL,
	ai_move, 0,	 NULL,
	ai_move, 0,	 NULL,
	ai_move, 0,	 NULL,
	ai_move, 0,	 NULL,
	ai_move, 0,	 NULL,

	ai_move, 0,	 NULL,
	ai_move, 0,	 NULL,
	ai_move, 0,	 NULL,
	ai_move, 0,	 NULL,
	ai_move, 0,	 NULL,
	ai_move, 0,	 NULL,
	ai_move, 0,	 NULL,
	ai_move, 0,	 NULL,
	ai_move, 0,	 NULL,
	ai_move, 0,	 NULL,

	ai_move, 0,	 NULL,
	ai_move, 0,	 NULL,
	ai_move, 0,	 NULL,
	ai_move, 0,	 NULL,
	ai_move, 0,	 NULL,
	ai_move, 0,	 NULL,
	ai_move, 0,	 NULL,
	ai_move, 0,	 NULL,
	ai_move, 0,	 NULL,
	ai_move, 0,	 NULL,

	ai_move, 0,	 NULL,
	ai_move, 0,	 NULL,
	ai_move, 0,	 NULL,
	ai_move, 0,	 NULL,
	ai_move, 0,	 NULL,
	ai_move, 0,	 NULL
};
mmove_t flipper_move_death = {FLIPPER_FRAME_flpdth01, FLIPPER_FRAME_flpdth56, flipper_frames_death, flipper_dead};

void flipper_sight (edict_t *self, edict_t *other)
{
	gi.sound (self, CHAN_VOICE, flipper_sound_sight, 1, ATTN_NORM, 0);
}

void flipper_die (edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point)
{
	int		n;

// check for gib
	if (self->health <= self->gib_health)
	{
		gi.sound (self, CHAN_VOICE, gi.soundindex ("misc/udeath.wav"), 1, ATTN_NORM, 0);
		if(net_compatibility->value || !net_fixoverflow->value)
		{
			for (n= 0; n < 2; n++)
				ThrowGib (self, "models/objects/gibs/bone/tris.md2", damage, GIB_ORGANIC);
			for (n= 0; n < 2; n++)
				ThrowGib (self, "models/objects/gibs/sm_meat/tris.md2", damage, GIB_ORGANIC);
			ThrowHead (self, "models/objects/gibs/sm_meat/tris.md2", damage, GIB_ORGANIC);
			self->deadflag = DEAD_DEAD;
		}
		else
		{
			NetThrowMonsterGibs (self, GIBS_FLIPPER, damage);
			G_FreeEdict (self);
		}
		return;
	}

	if (self->deadflag == DEAD_DEAD)
		return;

// regular death
	gi.sound (self, CHAN_VOICE, flipper_sound_death, 1, ATTN_NORM, 0);
	self->deadflag = DEAD_DEAD;
	self->takedamage = DAMAGE_YES;
	self->monsterinfo.currentmove = &flipper_move_death;
}

/*QUAKED monster_flipper (1 .5 0) (-16 -16 -24) (16 16 32) Ambush Trigger_Spawn Sight
*/
void SP_monster_flipper (edict_t *self)
{
	if (deathmatch->value)
	{
		G_FreeEdict (self);
		return;
	}

	flipper_sound_pain1		= gi.soundindex ("flipper/flppain1.wav");
	flipper_sound_pain2		= gi.soundindex ("flipper/flppain2.wav");
	flipper_sound_death		= gi.soundindex ("flipper/flpdeth1.wav");
	flipper_sound_chomp		= gi.soundindex ("flipper/flpatck1.wav");
	flipper_sound_attack	= gi.soundindex ("flipper/flpatck2.wav");
	flipper_sound_idle		= gi.soundindex ("flipper/flpidle1.wav");
	flipper_sound_search	= gi.soundindex ("flipper/flpsrch1.wav");
	flipper_sound_sight		= gi.soundindex ("flipper/flpsght1.wav");

	self->movetype = MOVETYPE_STEP;
	self->solid = SOLID_BBOX;
	self->s.modelindex = gi.modelindex ("models/monsters/flipper/tris.md2");
	VectorSet (self->mins, -16, -16, 0);
	VectorSet (self->maxs, 16, 16, 32);

	self->health = 50;
	self->gib_health = -30;
	self->mass = 100;

	self->pain = flipper_pain;
	self->die = flipper_die;

	self->monsterinfo.stand = flipper_stand;
	self->monsterinfo.walk = flipper_walk;
	self->monsterinfo.run = flipper_start_run;
	self->monsterinfo.melee = flipper_melee;
	self->monsterinfo.sight = flipper_sight;

	gi.linkentity (self);

	self->monsterinfo.currentmove = &flipper_move_stand;
	self->monsterinfo.scale = FLIPPER_MODEL_SCALE;

	swimmonster_start (self);
}

/*
==============================================================================

END of FLIPPER

==============================================================================
*/



/*
==============================================================================

parasite

==============================================================================
*/

void parasite_stand (edict_t *self);
void parasite_start_run (edict_t *self);
void parasite_run (edict_t *self);
void parasite_walk (edict_t *self);
void parasite_start_walk (edict_t *self);
void parasite_end_fidget (edict_t *self);
void parasite_do_fidget (edict_t *self);
void parasite_refidget (edict_t *self);


void parasite_launch (edict_t *self)
{
	gi.sound (self, CHAN_WEAPON, parasite_sound_launch, 1, ATTN_NORM, 0);
}

void parasite_reel_in (edict_t *self)
{
	gi.sound (self, CHAN_WEAPON, parasite_sound_reelin, 1, ATTN_NORM, 0);
}

void parasite_sight (edict_t *self, edict_t *other)
{
	gi.sound (self, CHAN_WEAPON, parasite_sound_sight, 1, ATTN_NORM, 0);
}

void parasite_tap (edict_t *self)
{
	gi.sound (self, CHAN_WEAPON, parasite_sound_tap, 1, ATTN_IDLE, 0);
}

void parasite_scratch (edict_t *self)
{
	gi.sound (self, CHAN_WEAPON, parasite_sound_scratch, 1, ATTN_IDLE, 0);
}

void parasite_search (edict_t *self)
{
	gi.sound (self, CHAN_WEAPON, parasite_sound_search, 1, ATTN_IDLE, 0);
}


mframe_t parasite_frames_start_fidget [] =
{
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL
};
mmove_t parasite_move_start_fidget = {PARASITE_FRAME_stand18, PARASITE_FRAME_stand21, parasite_frames_start_fidget, parasite_do_fidget};

mframe_t parasite_frames_fidget [] =
{
	ai_stand, 0, parasite_scratch,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, parasite_scratch,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL
};
mmove_t parasite_move_fidget = {PARASITE_FRAME_stand22, PARASITE_FRAME_stand27, parasite_frames_fidget, parasite_refidget};

mframe_t parasite_frames_end_fidget [] =
{
	ai_stand, 0, parasite_scratch,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL
};
mmove_t parasite_move_end_fidget = {PARASITE_FRAME_stand28, PARASITE_FRAME_stand35, parasite_frames_end_fidget, parasite_stand};

void parasite_end_fidget (edict_t *self)
{
	self->monsterinfo.currentmove = &parasite_move_end_fidget;
}

void parasite_do_fidget (edict_t *self)
{
	self->monsterinfo.currentmove = &parasite_move_fidget;
}

void parasite_refidget (edict_t *self)
{
	if (random() <= 0.8)
		self->monsterinfo.currentmove = &parasite_move_fidget;
	else
		self->monsterinfo.currentmove = &parasite_move_end_fidget;
}

void parasite_idle (edict_t *self)
{
	self->monsterinfo.currentmove = &parasite_move_start_fidget;
}


mframe_t parasite_frames_stand [] =
{
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, parasite_tap,
	ai_stand, 0, NULL,
	ai_stand, 0, parasite_tap,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, parasite_tap,
	ai_stand, 0, NULL,
	ai_stand, 0, parasite_tap,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, parasite_tap,
	ai_stand, 0, NULL,
	ai_stand, 0, parasite_tap
};
mmove_t	parasite_move_stand = {PARASITE_FRAME_stand01, PARASITE_FRAME_stand17, parasite_frames_stand, parasite_stand};

void parasite_stand (edict_t *self)
{
	self->monsterinfo.currentmove = &parasite_move_stand;
}


mframe_t parasite_frames_run [] =
{
	ai_run, 30, NULL,
	ai_run, 30, NULL,
	ai_run, 22, NULL,
	ai_run, 19, NULL,
	ai_run, 24, NULL,
	ai_run, 28, NULL,
	ai_run, 25, NULL
};
mmove_t parasite_move_run = {PARASITE_FRAME_run03, PARASITE_FRAME_run09, parasite_frames_run, NULL};

mframe_t parasite_frames_start_run [] =
{
	ai_run, 0,	NULL,
	ai_run, 30, NULL,
};
mmove_t parasite_move_start_run = {PARASITE_FRAME_run01, PARASITE_FRAME_run02, parasite_frames_start_run, parasite_run};

mframe_t parasite_frames_stop_run [] =
{
	ai_run, 20, NULL,
	ai_run, 20,	NULL,
	ai_run, 12, NULL,
	ai_run, 10, NULL,
	ai_run, 0,  NULL,
	ai_run, 0,  NULL
};
mmove_t parasite_move_stop_run = {PARASITE_FRAME_run10, PARASITE_FRAME_run15, parasite_frames_stop_run, NULL};

void parasite_start_run (edict_t *self)
{
	if (self->monsterinfo.aiflags & AI_STAND_GROUND)
		self->monsterinfo.currentmove = &parasite_move_stand;
	else
		self->monsterinfo.currentmove = &parasite_move_start_run;
}

void parasite_run (edict_t *self)
{
	if (self->monsterinfo.aiflags & AI_STAND_GROUND)
		self->monsterinfo.currentmove = &parasite_move_stand;
	else
		self->monsterinfo.currentmove = &parasite_move_run;
}


mframe_t parasite_frames_walk [] =
{
	ai_walk, 30, NULL,
	ai_walk, 30, NULL,
	ai_walk, 22, NULL,
	ai_walk, 19, NULL,
	ai_walk, 24, NULL,
	ai_walk, 28, NULL,
	ai_walk, 25, NULL
};
mmove_t parasite_move_walk = {PARASITE_FRAME_run03, PARASITE_FRAME_run09, parasite_frames_walk, parasite_walk};

mframe_t parasite_frames_start_walk [] =
{
	ai_walk, 0,	NULL,
	ai_walk, 30, parasite_walk
};
mmove_t parasite_move_start_walk = {PARASITE_FRAME_run01, PARASITE_FRAME_run02, parasite_frames_start_walk, NULL};

mframe_t parasite_frames_stop_walk [] =
{
	ai_walk, 20, NULL,
	ai_walk, 20,	NULL,
	ai_walk, 12, NULL,
	ai_walk, 10, NULL,
	ai_walk, 0,  NULL,
	ai_walk, 0,  NULL
};
mmove_t parasite_move_stop_walk = {PARASITE_FRAME_run10, PARASITE_FRAME_run15, parasite_frames_stop_walk, NULL};

void parasite_start_walk (edict_t *self)
{
	self->monsterinfo.currentmove = &parasite_move_start_walk;
}

void parasite_walk (edict_t *self)
{
	self->monsterinfo.currentmove = &parasite_move_walk;
}


mframe_t parasite_frames_pain1 [] =
{
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0,	NULL,
	ai_move, 0,	NULL,
	ai_move, 0,	NULL,
	ai_move, 0,	NULL,
	ai_move, 6,	NULL,
	ai_move, 16, NULL,
	ai_move, -6, NULL,
	ai_move, -7, NULL,
	ai_move, 0, NULL
};
mmove_t parasite_move_pain1 = {PARASITE_FRAME_pain101, PARASITE_FRAME_pain111, parasite_frames_pain1, parasite_start_run};

void parasite_pain (edict_t *self, edict_t *other, float kick, int damage)
{
	if (self->health < (self->max_health / 2))
		self->s.skinnum = 1;

	if (level.time < self->pain_debounce_time)
		return;

	self->pain_debounce_time = level.time + 3;

	if (skill->value == 3)
		return;		// no pain anims in nightmare

	if (random() < 0.5)
		gi.sound (self, CHAN_VOICE, parasite_sound_pain1, 1, ATTN_NORM, 0);
	else
		gi.sound (self, CHAN_VOICE, parasite_sound_pain2, 1, ATTN_NORM, 0);

	self->monsterinfo.currentmove = &parasite_move_pain1;
}


static bool parasite_drain_attack_ok (vec3_t start, vec3_t end)
{
	vec3_t	dir, angles;

	// check for max distance
	VectorSubtract (start, end, dir);
	if (VectorLength(dir) > 256)
		return false;

	// check for min/max pitch
	vectoangles (dir, angles);
	if (angles[0] < -180)
		angles[0] += 360;
	if (fabs(angles[0]) > 30)
		return false;

	return true;
}

void parasite_drain_attack (edict_t *self)
{
	vec3_t	offset, start, f, r, end, dir;
	trace_t	tr;
	int damage;

	AngleVectors (self->s.angles, f, r, NULL);
	VectorSet (offset, 24, 0, 6);
	G_ProjectSource (self->s.origin, offset, f, r, start);

	VectorCopy (self->enemy->s.origin, end);
	if (!parasite_drain_attack_ok(start, end))
	{
		end[2] = self->enemy->s.origin[2] + self->enemy->maxs[2] - 8;
		if (!parasite_drain_attack_ok(start, end))
		{
			end[2] = self->enemy->s.origin[2] + self->enemy->mins[2] + 8;
			if (!parasite_drain_attack_ok(start, end))
				return;
		}
	}
	VectorCopy (self->enemy->s.origin, end);

	tr = gi.trace (start, NULL, NULL, end, self, MASK_SHOT);
	if (tr.ent != self->enemy)
		return;

	if (self->s.frame == PARASITE_FRAME_drain03)
	{
		damage = 5;
		gi.sound (self->enemy, CHAN_AUTO, parasite_sound_impact, 1, ATTN_NORM, 0);
	}
	else
	{
		if (self->s.frame == PARASITE_FRAME_drain04)
			gi.sound (self, CHAN_WEAPON, parasite_sound_suck, 1, ATTN_NORM, 0);
		damage = 2;
	}

	bool large_map = LargeMapCheck(start) | LargeMapCheck(end);
	if (large_map)
		gi.WriteByte (svc_temp_entity_ext);
	else
		gi.WriteByte (svc_temp_entity);
	gi.WriteByte (TE_PARASITE_ATTACK);
	gi.WriteShort (self - g_edicts);
	gi.WritePosition (start, large_map);
	gi.WritePosition (end, large_map);
	gi.multicast (self->s.origin, MULTICAST_PVS);

	VectorSubtract (start, end, dir);
	T_Damage (self->enemy, self, self, dir, self->enemy->s.origin, vec3_origin, damage, 0, DAMAGE_NO_KNOCKBACK, MOD_UNKNOWN);
}

mframe_t parasite_frames_drain [] =
{
	ai_charge, 0,	parasite_launch,
	ai_charge, 0,	NULL,
	ai_charge, 15,	parasite_drain_attack,			// Target hits
	ai_charge, 0,	parasite_drain_attack,			// drain
	ai_charge, 0,	parasite_drain_attack,			// drain
	ai_charge, 0,	parasite_drain_attack,			// drain
	ai_charge, 0,	parasite_drain_attack,			// drain
	ai_charge, -2,  parasite_drain_attack,			// drain
	ai_charge, -2,	parasite_drain_attack,			// drain
	ai_charge, -3,	parasite_drain_attack,			// drain
	ai_charge, -2,	parasite_drain_attack,			// drain
	ai_charge, 0,	parasite_drain_attack,			// drain
	ai_charge, -1,  parasite_drain_attack,			// drain
	ai_charge, 0,	parasite_reel_in,				// let go
	ai_charge, -2,	NULL,
	ai_charge, -2,	NULL,
	ai_charge, -3,	NULL,
	ai_charge, 0,	NULL
};
mmove_t parasite_move_drain = {PARASITE_FRAME_drain01, PARASITE_FRAME_drain18, parasite_frames_drain, parasite_start_run};


mframe_t parasite_frames_break [] =
{
	ai_charge, 0,	NULL,
	ai_charge, -3,	NULL,
	ai_charge, 1,	NULL,
	ai_charge, 2,	NULL,
	ai_charge, -3,	NULL,
	ai_charge, 1,	NULL,
	ai_charge, 1,	NULL,
	ai_charge, 3,	NULL,
	ai_charge, 0,	NULL,
	ai_charge, -18,	NULL,
	ai_charge, 3,	NULL,
	ai_charge, 9,	NULL,
	ai_charge, 6,	NULL,
	ai_charge, 0,	NULL,
	ai_charge, -18,	NULL,
	ai_charge, 0,	NULL,
	ai_charge, 8,	NULL,
	ai_charge, 9,	NULL,
	ai_charge, 0,	NULL,
	ai_charge, -18,	NULL,
	ai_charge, 0,	NULL,
	ai_charge, 0,	NULL,		// airborne
	ai_charge, 0,	NULL,		// airborne
	ai_charge, 0,	NULL,		// slides
	ai_charge, 0,	NULL,		// slides
	ai_charge, 0,	NULL,		// slides
	ai_charge, 0,	NULL,		// slides
	ai_charge, 4,	NULL,
	ai_charge, 11,	NULL,
	ai_charge, -2,	NULL,
	ai_charge, -5,	NULL,
	ai_charge, 1,	NULL
};
mmove_t parasite_move_break = {PARASITE_FRAME_break01, PARASITE_FRAME_break32, parasite_frames_break, parasite_start_run};

/*
===
Break Stuff Ends
===
*/

void parasite_attack (edict_t *self)
{
//	if (random() <= 0.2)
//		self->monsterinfo.currentmove = &parasite_move_break;
//	else
		self->monsterinfo.currentmove = &parasite_move_drain;
}



/*
===
Death Stuff Starts
===
*/

void parasite_dead (edict_t *self)
{
	VectorSet (self->mins, -16, -16, -24);
	VectorSet (self->maxs, 16, 16, -8);
	self->movetype = MOVETYPE_TOSS;
	self->svflags |= SVF_DEADMONSTER;
	self->nextthink = 0;
	gi.linkentity (self);

	M_FlyCheck (self);

	monster_reborn(self);
}

mframe_t parasite_frames_death [] =
{
	ai_move, 0,	 NULL,
	ai_move, 0,	 NULL,
	ai_move, 0,	 NULL,
	ai_move, 0,	 NULL,
	ai_move, 0,	 NULL,
	ai_move, 0,	 NULL,
	ai_move, 0,	 NULL
};
mmove_t parasite_move_death = {PARASITE_FRAME_death101, PARASITE_FRAME_death107, parasite_frames_death, parasite_dead};

void parasite_die (edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point)
{
	int		n;

// check for gib
	if (self->health <= self->gib_health)
	{
		gi.sound (self, CHAN_VOICE, gi.soundindex ("misc/udeath.wav"), 1, ATTN_NORM, 0);
		if(net_compatibility->value || !net_fixoverflow->value)
		{
			for (n= 0; n < 2; n++)
				ThrowGib (self, "models/objects/gibs/bone/tris.md2", damage, GIB_ORGANIC);
			for (n= 0; n < 4; n++)
				ThrowGib (self, "models/objects/gibs/sm_meat/tris.md2", damage, GIB_ORGANIC);
			ThrowHead (self, "models/objects/gibs/head2/tris.md2", damage, GIB_ORGANIC);
			self->deadflag = DEAD_DEAD;
		}
		else
		{
			NetThrowMonsterGibs (self, GIBS_PARASITE, damage);
			G_FreeEdict (self);
		}
		return;
	}

	if (self->deadflag == DEAD_DEAD)
		return;

// regular death
	gi.sound (self, CHAN_VOICE, parasite_sound_die, 1, ATTN_NORM, 0);
	self->deadflag = DEAD_DEAD;
	self->takedamage = DAMAGE_YES;
	self->monsterinfo.currentmove = &parasite_move_death;
}

/*
===
End Death Stuff
===
*/

/*QUAKED monster_parasite (1 .5 0) (-16 -16 -24) (16 16 32) Ambush Trigger_Spawn Sight
*/
void SP_monster_parasite (edict_t *self)
{
	if (deathmatch->value)
	{
		G_FreeEdict (self);
		return;
	}

	parasite_sound_pain1 = gi.soundindex ("parasite/parpain1.wav");
	parasite_sound_pain2 = gi.soundindex ("parasite/parpain2.wav");
	parasite_sound_die = gi.soundindex ("parasite/pardeth1.wav");
	parasite_sound_launch = gi.soundindex("parasite/paratck1.wav");
	parasite_sound_impact = gi.soundindex("parasite/paratck2.wav");
	parasite_sound_suck = gi.soundindex("parasite/paratck3.wav");
	parasite_sound_reelin = gi.soundindex("parasite/paratck4.wav");
	parasite_sound_sight = gi.soundindex("parasite/parsght1.wav");
	parasite_sound_tap = gi.soundindex("parasite/paridle1.wav");
	parasite_sound_scratch = gi.soundindex("parasite/paridle2.wav");
	parasite_sound_search = gi.soundindex("parasite/parsrch1.wav");

	self->s.modelindex = gi.modelindex ("models/monsters/parasite/tris.md2");
	VectorSet (self->mins, -16, -16, -24);
	VectorSet (self->maxs, 16, 16, 24);
	self->movetype = MOVETYPE_STEP;
	self->solid = SOLID_BBOX;

	self->health = 175;
	self->gib_health = -50;
	self->mass = 250;

	self->pain = parasite_pain;
	self->die = parasite_die;

	self->monsterinfo.stand = parasite_stand;
	self->monsterinfo.walk = parasite_start_walk;
	self->monsterinfo.run = parasite_start_run;
	self->monsterinfo.attack = parasite_attack;
	self->monsterinfo.sight = parasite_sight;
	self->monsterinfo.idle = parasite_idle;

	gi.linkentity (self);

	self->monsterinfo.currentmove = &parasite_move_stand;
	self->monsterinfo.scale = PARASITE_MODEL_SCALE;

	walkmonster_start (self, false);
}

/*
==============================================================================

end of parasite

==============================================================================
*/


/*
==============================================================================

TANK

==============================================================================
*/

void tank_refire_rocket (edict_t *self);
void tank_doattack_rocket (edict_t *self);
void tank_reattack_blaster (edict_t *self);

//
// misc
//

void tank_sight (edict_t *self, edict_t *other)
{
	gi.sound (self, CHAN_VOICE, tank_sound_sight, 1, ATTN_NORM, 0);
}


void tank_footstep (edict_t *self)
{
	gi.sound (self, CHAN_BODY, tank_sound_step, 1, ATTN_NORM, 0);
}

void tank_thud (edict_t *self)
{
	gi.sound (self, CHAN_BODY, tank_sound_thud, 1, ATTN_NORM, 0);
}

void tank_windup (edict_t *self)
{
	gi.sound (self, CHAN_WEAPON, tank_sound_windup, 1, ATTN_NORM, 0);
}

void tank_idle (edict_t *self)
{
	gi.sound (self, CHAN_VOICE, tank_sound_idle, 1, ATTN_IDLE, 0);
}


//
// stand
//

mframe_t tank_frames_stand []=
{
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL
};
mmove_t	tank_move_stand = {TANK_FRAME_stand01, TANK_FRAME_stand30, tank_frames_stand, NULL};

void tank_stand (edict_t *self)
{
	self->monsterinfo.currentmove = &tank_move_stand;
}


//
// walk
//

void tank_walk (edict_t *self);

mframe_t tank_frames_start_walk [] =
{
	ai_walk,  0, NULL,
	ai_walk,  6, NULL,
	ai_walk,  6, NULL,
	ai_walk, 11, tank_footstep
};
mmove_t	tank_move_start_walk = {TANK_FRAME_walk01, TANK_FRAME_walk04, tank_frames_start_walk, tank_walk};

mframe_t tank_frames_walk [] =
{
	ai_walk, 4,	NULL,
	ai_walk, 5,	NULL,
	ai_walk, 3,	NULL,
	ai_walk, 2,	NULL,
	ai_walk, 5,	NULL,
	ai_walk, 5,	NULL,
	ai_walk, 4,	NULL,
	ai_walk, 4,	tank_footstep,
	ai_walk, 3,	NULL,
	ai_walk, 5,	NULL,
	ai_walk, 4,	NULL,
	ai_walk, 5,	NULL,
	ai_walk, 7,	NULL,
	ai_walk, 7,	NULL,
	ai_walk, 6,	NULL,
	ai_walk, 6,	tank_footstep
};
mmove_t	tank_move_walk = {TANK_FRAME_walk05, TANK_FRAME_walk20, tank_frames_walk, NULL};

mframe_t tank_frames_stop_walk [] =
{
	ai_walk,  3, NULL,
	ai_walk,  3, NULL,
	ai_walk,  2, NULL,
	ai_walk,  2, NULL,
	ai_walk,  4, tank_footstep
};
mmove_t	tank_move_stop_walk = {TANK_FRAME_walk21, TANK_FRAME_walk25, tank_frames_stop_walk, tank_stand};

void tank_walk (edict_t *self)
{
		self->monsterinfo.currentmove = &tank_move_walk;
}


//
// run
//

void tank_run (edict_t *self);

mframe_t tank_frames_start_run [] =
{
	ai_run,  0, NULL,
	ai_run,  6, NULL,
	ai_run,  6, NULL,
	ai_run, 11, tank_footstep
};
mmove_t	tank_move_start_run = {TANK_FRAME_walk01, TANK_FRAME_walk04, tank_frames_start_run, tank_run};

mframe_t tank_frames_run [] =
{
	ai_run, 4,	NULL,
	ai_run, 5,	NULL,
	ai_run, 3,	NULL,
	ai_run, 2,	NULL,
	ai_run, 5,	NULL,
	ai_run, 5,	NULL,
	ai_run, 4,	NULL,
	ai_run, 4,	tank_footstep,
	ai_run, 3,	NULL,
	ai_run, 5,	NULL,
	ai_run, 4,	NULL,
	ai_run, 5,	NULL,
	ai_run, 7,	NULL,
	ai_run, 7,	NULL,
	ai_run, 6,	NULL,
	ai_run, 6,	tank_footstep
};
mmove_t	tank_move_run = {TANK_FRAME_walk05, TANK_FRAME_walk20, tank_frames_run, NULL};

mframe_t tank_frames_stop_run [] =
{
	ai_run,  3, NULL,
	ai_run,  3, NULL,
	ai_run,  2, NULL,
	ai_run,  2, NULL,
	ai_run,  4, tank_footstep
};
mmove_t	tank_move_stop_run = {TANK_FRAME_walk21, TANK_FRAME_walk25, tank_frames_stop_run, tank_walk};

void tank_run (edict_t *self)
{
	if (self->enemy && self->enemy->client)
		self->monsterinfo.aiflags |= AI_BRUTAL;
	else
		self->monsterinfo.aiflags &= ~AI_BRUTAL;

	if (self->monsterinfo.aiflags & AI_STAND_GROUND)
	{
		self->monsterinfo.currentmove = &tank_move_stand;
		return;
	}

	if (self->monsterinfo.currentmove == &tank_move_walk ||
		self->monsterinfo.currentmove == &tank_move_start_run)
	{
		self->monsterinfo.currentmove = &tank_move_run;
	}
	else
	{
		self->monsterinfo.currentmove = &tank_move_start_run;
	}
}

//
// pain
//

mframe_t tank_frames_pain1 [] =
{
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL
};
mmove_t tank_move_pain1 = {TANK_FRAME_pain101, TANK_FRAME_pain104, tank_frames_pain1, tank_run};

mframe_t tank_frames_pain2 [] =
{
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL
};
mmove_t tank_move_pain2 = {TANK_FRAME_pain201, TANK_FRAME_pain205, tank_frames_pain2, tank_run};

mframe_t tank_frames_pain3 [] =
{
	ai_move, -7, NULL,
	ai_move, 0,  NULL,
	ai_move, 0,  NULL,
	ai_move, 0,  NULL,
	ai_move, 2,  NULL,
	ai_move, 0,  NULL,
	ai_move, 0,  NULL,
	ai_move, 3,  NULL,
	ai_move, 0,  NULL,
	ai_move, 2,  NULL,
	ai_move, 0,  NULL,
	ai_move, 0,  NULL,
	ai_move, 0,  NULL,
	ai_move, 0,  NULL,
	ai_move, 0,  NULL,
	ai_move, 0,  tank_footstep
};
mmove_t	tank_move_pain3 = {TANK_FRAME_pain301, TANK_FRAME_pain316, tank_frames_pain3, tank_run};


void tank_pain (edict_t *self, edict_t *other, float kick, int damage)
{
	if (self->health < (self->max_health / 2))
			self->s.skinnum |= 1;

	if (damage <= 10)
		return;

	if (level.time < self->pain_debounce_time)
			return;

	if (damage <= 30)
		if (random() > 0.2)
			return;

	// If hard or nightmare, don't go into pain while attacking
	if ( skill->value >= 2)
	{
		if ( (self->s.frame >= TANK_FRAME_attak301) && (self->s.frame <= TANK_FRAME_attak330) )
			return;
		if ( (self->s.frame >= TANK_FRAME_attak101) && (self->s.frame <= TANK_FRAME_attak116) )
			return;
	}

	self->pain_debounce_time = level.time + 3;
	gi.sound (self, CHAN_VOICE, tank_sound_pain, 1, ATTN_NORM, 0);

	if (skill->value == 3)
		return;		// no pain anims in nightmare

	if (damage <= 30)
		self->monsterinfo.currentmove = &tank_move_pain1;
	else if (damage <= 60)
		self->monsterinfo.currentmove = &tank_move_pain2;
	else
		self->monsterinfo.currentmove = &tank_move_pain3;
};


//
// attacks
//

void TankBlaster (edict_t *self)
{
	vec3_t	forward, right;
	vec3_t	start;
	vec3_t	end;
	vec3_t	dir;
	int		flash_number;

	if (self->s.frame == TANK_FRAME_attak110)
		flash_number = MZ2_TANK_BLASTER_1;
	else if (self->s.frame == TANK_FRAME_attak113)
		flash_number = MZ2_TANK_BLASTER_2;
	else // (self->s.frame == TANK_FRAME_attak116)
		flash_number = MZ2_TANK_BLASTER_3;

	AngleVectors (self->s.angles, forward, right, NULL);
	G_ProjectSource (self->s.origin, &monster_flash_offset[flash_number*3], forward, right, start);

	VectorCopy (self->enemy->s.origin, end);
	end[2] += self->enemy->viewheight;
	VectorSubtract (end, start, dir);

	monster_fire_blaster (self, start, dir, 30, 800, flash_number, EF_BLASTER);
}

void TankStrike (edict_t *self)
{
	gi.sound (self, CHAN_WEAPON, tank_sound_strike, 1, ATTN_NORM, 0);
}

void TankRocket (edict_t *self)
{
	vec3_t	forward, right;
	vec3_t	start;
	vec3_t	dir;
	vec3_t	vec;
	int		flash_number;

	if (self->s.frame == TANK_FRAME_attak324)
		flash_number = MZ2_TANK_ROCKET_1;
	else if (self->s.frame == TANK_FRAME_attak327)
		flash_number = MZ2_TANK_ROCKET_2;
	else // (self->s.frame == TANK_FRAME_attak330)
		flash_number = MZ2_TANK_ROCKET_3;

	AngleVectors (self->s.angles, forward, right, NULL);
	G_ProjectSource (self->s.origin, &monster_flash_offset[flash_number*3], forward, right, start);

	VectorCopy (self->enemy->s.origin, vec);
	vec[2] += self->enemy->viewheight;
	VectorSubtract (vec, start, dir);
	VectorNormalize (dir);

	monster_fire_rocket (self, start, dir, 50, 550, flash_number);
}

void TankMachineGun (edict_t *self)
{
	vec3_t	dir;
	vec3_t	vec;
	vec3_t	start;
	vec3_t	forward, right;
	int		flash_number;

	flash_number = MZ2_TANK_MACHINEGUN_1 + (self->s.frame - TANK_FRAME_attak406);

	AngleVectors (self->s.angles, forward, right, NULL);
	G_ProjectSource (self->s.origin, &monster_flash_offset[flash_number*3], forward, right, start);

	if (self->enemy)
	{
		VectorCopy (self->enemy->s.origin, vec);
		vec[2] += self->enemy->viewheight;
		VectorSubtract (vec, start, vec);
		vectoangles (vec, vec);
		dir[0] = vec[0];
	}
	else
	{
		dir[0] = 0;
	}
	if (self->s.frame <= TANK_FRAME_attak415)
		dir[1] = self->s.angles[1] - 8 * (self->s.frame - TANK_FRAME_attak411);
	else
		dir[1] = self->s.angles[1] + 8 * (self->s.frame - TANK_FRAME_attak419);
	dir[2] = 0;

	AngleVectors (dir, forward, NULL, NULL);

	monster_fire_bullet (self, start, forward, 20, 4, DEFAULT_BULLET_HSPREAD, DEFAULT_BULLET_VSPREAD, flash_number);
}


mframe_t tank_frames_attack_blast [] =
{
	ai_charge, 0,	NULL,
	ai_charge, 0,	NULL,
	ai_charge, 0,	NULL,
	ai_charge, 0,	NULL,
	ai_charge, -1,	NULL,
	ai_charge, -2,	NULL,
	ai_charge, -1,	NULL,
	ai_charge, -1,	NULL,
	ai_charge, 0,	NULL,
	ai_charge, 0,	TankBlaster,		// 10
	ai_charge, 0,	NULL,
	ai_charge, 0,	NULL,
	ai_charge, 0,	TankBlaster,
	ai_charge, 0,	NULL,
	ai_charge, 0,	NULL,
	ai_charge, 0,	TankBlaster			// 16
};
mmove_t tank_move_attack_blast = {TANK_FRAME_attak101, TANK_FRAME_attak116, tank_frames_attack_blast, tank_reattack_blaster};

mframe_t tank_frames_reattack_blast [] =
{
	ai_charge, 0,	NULL,
	ai_charge, 0,	NULL,
	ai_charge, 0,	TankBlaster,
	ai_charge, 0,	NULL,
	ai_charge, 0,	NULL,
	ai_charge, 0,	TankBlaster			// 16
};
mmove_t tank_move_reattack_blast = {TANK_FRAME_attak111, TANK_FRAME_attak116, tank_frames_reattack_blast, tank_reattack_blaster};

mframe_t tank_frames_attack_post_blast [] =
{
	ai_move, 0,		NULL,				// 17
	ai_move, 0,		NULL,
	ai_move, 2,		NULL,
	ai_move, 3,		NULL,
	ai_move, 2,		NULL,
	ai_move, -2,	tank_footstep		// 22
};
mmove_t tank_move_attack_post_blast = {TANK_FRAME_attak117, TANK_FRAME_attak122, tank_frames_attack_post_blast, tank_run};

void tank_reattack_blaster (edict_t *self)
{
	if (skill->value >= 2)
		if (visible (self, self->enemy))
			if (self->enemy->health > 0)
				if (random() <= 0.6)
				{
					self->monsterinfo.currentmove = &tank_move_reattack_blast;
					return;
				}
	self->monsterinfo.currentmove = &tank_move_attack_post_blast;
}


void tank_poststrike (edict_t *self)
{
	self->enemy = NULL;
	tank_run (self);
}

mframe_t tank_frames_attack_strike [] =
{
	ai_move, 3,   NULL,
	ai_move, 2,   NULL,
	ai_move, 2,   NULL,
	ai_move, 1,   NULL,
	ai_move, 6,   NULL,
	ai_move, 7,   NULL,
	ai_move, 9,   tank_footstep,
	ai_move, 2,   NULL,
	ai_move, 1,   NULL,
	ai_move, 2,   NULL,
	ai_move, 2,   tank_footstep,
	ai_move, 2,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, -2,  NULL,
	ai_move, -2,  NULL,
	ai_move, 0,   tank_windup,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   TankStrike,
	ai_move, 0,   NULL,
	ai_move, -1,  NULL,
	ai_move, -1,  NULL,
	ai_move, -1,  NULL,
	ai_move, -1,  NULL,
	ai_move, -1,  NULL,
	ai_move, -3,  NULL,
	ai_move, -10, NULL,
	ai_move, -10, NULL,
	ai_move, -2,  NULL,
	ai_move, -3,  NULL,
	ai_move, -2,  tank_footstep
};
mmove_t tank_move_attack_strike = {TANK_FRAME_attak201, TANK_FRAME_attak238, tank_frames_attack_strike, tank_poststrike};

mframe_t tank_frames_attack_pre_rocket [] =
{
	ai_charge, 0,  NULL,
	ai_charge, 0,  NULL,
	ai_charge, 0,  NULL,
	ai_charge, 0,  NULL,
	ai_charge, 0,  NULL,
	ai_charge, 0,  NULL,
	ai_charge, 0,  NULL,
	ai_charge, 0,  NULL,
	ai_charge, 0,  NULL,
	ai_charge, 0,  NULL,			// 10

	ai_charge, 0,  NULL,
	ai_charge, 1,  NULL,
	ai_charge, 2,  NULL,
	ai_charge, 7,  NULL,
	ai_charge, 7,  NULL,
	ai_charge, 7,  tank_footstep,
	ai_charge, 0,  NULL,
	ai_charge, 0,  NULL,
	ai_charge, 0,  NULL,
	ai_charge, 0,  NULL,			// 20

	ai_charge, -3, NULL
};
mmove_t tank_move_attack_pre_rocket = {TANK_FRAME_attak301, TANK_FRAME_attak321, tank_frames_attack_pre_rocket, tank_doattack_rocket};

mframe_t tank_frames_attack_fire_rocket [] =
{
	ai_charge, -3, NULL,			// Loop Start	22
	ai_charge, 0,  NULL,
	ai_charge, 0,  TankRocket,		// 24
	ai_charge, 0,  NULL,
	ai_charge, 0,  NULL,
	ai_charge, 0,  TankRocket,
	ai_charge, 0,  NULL,
	ai_charge, 0,  NULL,
	ai_charge, -1, TankRocket		// 30	Loop End
};
mmove_t tank_move_attack_fire_rocket = {TANK_FRAME_attak322, TANK_FRAME_attak330, tank_frames_attack_fire_rocket, tank_refire_rocket};

mframe_t tank_frames_attack_post_rocket [] =
{
	ai_charge, 0,  NULL,			// 31
	ai_charge, -1, NULL,
	ai_charge, -1, NULL,
	ai_charge, 0,  NULL,
	ai_charge, 2,  NULL,
	ai_charge, 3,  NULL,
	ai_charge, 4,  NULL,
	ai_charge, 2,  NULL,
	ai_charge, 0,  NULL,
	ai_charge, 0,  NULL,			// 40

	ai_charge, 0,  NULL,
	ai_charge, -9, NULL,
	ai_charge, -8, NULL,
	ai_charge, -7, NULL,
	ai_charge, -1, NULL,
	ai_charge, -1, tank_footstep,
	ai_charge, 0,  NULL,
	ai_charge, 0,  NULL,
	ai_charge, 0,  NULL,
	ai_charge, 0,  NULL,			// 50

	ai_charge, 0,  NULL,
	ai_charge, 0,  NULL,
	ai_charge, 0,  NULL
};
mmove_t tank_move_attack_post_rocket = {TANK_FRAME_attak331, TANK_FRAME_attak353, tank_frames_attack_post_rocket, tank_run};

mframe_t tank_frames_attack_chain [] =
{
	ai_charge, 0, NULL,
	ai_charge, 0, NULL,
	ai_charge, 0, NULL,
	ai_charge, 0, NULL,
	ai_charge, 0, NULL,
	NULL,      0, TankMachineGun,
	NULL,      0, TankMachineGun,
	NULL,      0, TankMachineGun,
	NULL,      0, TankMachineGun,
	NULL,      0, TankMachineGun,
	NULL,      0, TankMachineGun,
	NULL,      0, TankMachineGun,
	NULL,      0, TankMachineGun,
	NULL,      0, TankMachineGun,
	NULL,      0, TankMachineGun,
	NULL,      0, TankMachineGun,
	NULL,      0, TankMachineGun,
	NULL,      0, TankMachineGun,
	NULL,      0, TankMachineGun,
	NULL,      0, TankMachineGun,
	NULL,      0, TankMachineGun,
	NULL,      0, TankMachineGun,
	NULL,      0, TankMachineGun,
	NULL,      0, TankMachineGun,
	ai_charge, 0, NULL,
	ai_charge, 0, NULL,
	ai_charge, 0, NULL,
	ai_charge, 0, NULL,
	ai_charge, 0, NULL
};
mmove_t tank_move_attack_chain = {TANK_FRAME_attak401, TANK_FRAME_attak429, tank_frames_attack_chain, tank_run};

void tank_refire_rocket (edict_t *self)
{
	// Only on hard or nightmare
	if ( skill->value >= 2 )
		if (self->enemy->health > 0)
			if (visible(self, self->enemy) )
				if (random() <= 0.4)
				{
					self->monsterinfo.currentmove = &tank_move_attack_fire_rocket;
					return;
				}
	self->monsterinfo.currentmove = &tank_move_attack_post_rocket;
}

void tank_doattack_rocket (edict_t *self)
{
	self->monsterinfo.currentmove = &tank_move_attack_fire_rocket;
}

void tank_attack(edict_t *self)
{
	vec3_t	vec;
	float	range;
	float	r;

	if (self->enemy->health < 0)
	{
		self->monsterinfo.currentmove = &tank_move_attack_strike;
		self->monsterinfo.aiflags &= ~AI_BRUTAL;
		return;
	}

	VectorSubtract (self->enemy->s.origin, self->s.origin, vec);
	range = VectorLength (vec);

	r = random();

	if (range <= 125)
	{
		if (r < 0.4)
			self->monsterinfo.currentmove = &tank_move_attack_chain;
		else
			self->monsterinfo.currentmove = &tank_move_attack_blast;
	}
	else if (range <= 250)
	{
		if (r < 0.5)
			self->monsterinfo.currentmove = &tank_move_attack_chain;
		else
			self->monsterinfo.currentmove = &tank_move_attack_blast;
	}
	else
	{
		if (r < 0.33)
			self->monsterinfo.currentmove = &tank_move_attack_chain;
		else if (r < 0.66)
		{
			self->monsterinfo.currentmove = &tank_move_attack_pre_rocket;
			self->pain_debounce_time = level.time + 5.0;	// no pain for a while
		}
		else
			self->monsterinfo.currentmove = &tank_move_attack_blast;
	}
}


//
// death
//

void tank_dead (edict_t *self)
{
	VectorSet (self->mins, -16, -16, -16);
	VectorSet (self->maxs, 16, 16, -0);
	self->movetype = MOVETYPE_TOSS;
	self->svflags |= SVF_DEADMONSTER;
	self->nextthink = 0;
	gi.linkentity (self);

///	M_FlyCheck (self);	/// Танк железный, его мухи грызть не любят

	monster_reborn(self);
}

mframe_t tank_frames_death1 [] =
{
	ai_move, -7,  NULL,
	ai_move, -2,  NULL,
	ai_move, -2,  NULL,
	ai_move, 1,   NULL,
	ai_move, 3,   NULL,
	ai_move, 6,   NULL,
	ai_move, 1,   NULL,
	ai_move, 1,   NULL,
	ai_move, 2,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, -2,  NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, -3,  NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, -4,  NULL,
	ai_move, -6,  NULL,
	ai_move, -4,  NULL,
	ai_move, -5,  NULL,
	ai_move, -7,  NULL,
	ai_move, -15, tank_thud,
	ai_move, -5,  NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, 0,   NULL
};
mmove_t	tank_move_death = {TANK_FRAME_death101, TANK_FRAME_death132, tank_frames_death1, tank_dead};

void tank_die (edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point)
{
	int		n;

// check for gib
	if (self->health <= self->gib_health)
	{
		gi.sound (self, CHAN_VOICE, gi.soundindex ("misc/udeath.wav"), 1, ATTN_NORM, 0);
		if(net_compatibility->value || !net_fixoverflow->value)
		{
			for (n= 0; n < 1 /*4*/; n++)
				ThrowGib (self, "models/objects/gibs/sm_meat/tris.md2", damage, GIB_ORGANIC);
			for (n= 0; n < 4; n++)
				ThrowGib (self, "models/objects/gibs/sm_metal/tris.md2", damage, GIB_METALLIC);
			ThrowGib (self, "models/objects/gibs/chest/tris.md2", damage, GIB_ORGANIC);
			ThrowHead (self, "models/objects/gibs/gear/tris.md2", damage, GIB_METALLIC);
			self->deadflag = DEAD_DEAD;
		}
		else
		{
			NetThrowMonsterGibsExt (self, GIBS_TANK, damage);
			G_FreeEdict (self);
		}
		return;
	}

	if (self->deadflag == DEAD_DEAD)
		return;

// regular death
	gi.sound (self, CHAN_VOICE, tank_sound_die, 1, ATTN_NORM, 0);
	self->deadflag = DEAD_DEAD;
	self->takedamage = DAMAGE_YES;

	self->monsterinfo.currentmove = &tank_move_death;

}


//
// monster_tank
//

/*QUAKED monster_tank (1 .5 0) (-32 -32 -16) (32 32 72) Ambush Trigger_Spawn Sight
*/
/*QUAKED monster_tank_commander (1 .5 0) (-32 -32 -16) (32 32 72) Ambush Trigger_Spawn Sight
*/
void SP_monster_tank (edict_t *self)
{
	if (deathmatch->value)
	{
		G_FreeEdict (self);
		return;
	}

	self->s.modelindex = gi.modelindex ("models/monsters/tank/tris.md2");
	VectorSet (self->mins, -32, -32, -16);
	VectorSet (self->maxs, 32, 32, 72);
	self->movetype = MOVETYPE_STEP;
	self->solid = SOLID_BBOX;

	tank_sound_pain = gi.soundindex ("tank/tnkpain2.wav");
	tank_sound_thud = gi.soundindex ("tank/tnkdeth2.wav");
	tank_sound_idle = gi.soundindex ("tank/tnkidle1.wav");
	tank_sound_die = gi.soundindex ("tank/death.wav");
	tank_sound_step = gi.soundindex ("tank/step.wav");
	tank_sound_windup = gi.soundindex ("tank/tnkatck4.wav");
	tank_sound_strike = gi.soundindex ("tank/tnkatck5.wav");
	tank_sound_sight = gi.soundindex ("tank/sight1.wav");

	gi.soundindex ("tank/tnkatck1.wav");
	gi.soundindex ("tank/tnkatk2a.wav");
	gi.soundindex ("tank/tnkatk2b.wav");
	gi.soundindex ("tank/tnkatk2c.wav");
	gi.soundindex ("tank/tnkatk2d.wav");
	gi.soundindex ("tank/tnkatk2e.wav");
	gi.soundindex ("tank/tnkatck3.wav");

	if (strcmp(self->classname, "monster_tank_commander") == 0)
	{
		self->health = 1000;
		self->gib_health = -225;
	}
	else
	{
		self->health = 750;
		self->gib_health = -200;
	}

	self->mass = 500;

	self->pain = tank_pain;
	self->die = tank_die;
	self->monsterinfo.stand = tank_stand;
	self->monsterinfo.walk = tank_walk;
	self->monsterinfo.run = tank_run;
	self->monsterinfo.dodge = NULL;
	self->monsterinfo.attack = tank_attack;
	self->monsterinfo.melee = NULL;
	self->monsterinfo.sight = tank_sight;
	self->monsterinfo.idle = tank_idle;

	gi.linkentity (self);

	self->monsterinfo.currentmove = &tank_move_stand;
	self->monsterinfo.scale = TANK_MODEL_SCALE;

	walkmonster_start(self, false);

	if (strcmp(self->classname, "monster_tank_commander") == 0)
		self->s.skinnum = 2;
}

/*
==============================================================================

END of TANK

==============================================================================
*/


/*
==============================================================================

MEDIC

==============================================================================
*/

edict_t *medic_FindDeadMonster (edict_t *self)
{
	edict_t	*ent = NULL;
	edict_t	*best = NULL;

	while ((ent = findradius(ent, self->s.origin, 1024)) != NULL)
	{
		if (ent == self)
			continue;
		if (!(ent->svflags & SVF_MONSTER))
			continue;
		if (ent->monsterinfo.aiflags & AI_GOOD_GUY)
			continue;
		if (ent->owner)
			continue;
		if (ent->health > 0)
			continue;
		if (ent->nextthink)
			continue;
		if (!visible(self, ent))
			continue;
		if (!best)
		{
			best = ent;
			continue;
		}
		if (ent->max_health <= best->max_health)
			continue;
		best = ent;
	}

	return best;
}

void medic_idle (edict_t *self)
{
	edict_t	*ent;

	gi.sound (self, CHAN_VOICE, medic_sound_idle1, 1, ATTN_IDLE, 0);

	ent = medic_FindDeadMonster(self);
	if (ent)
	{
		self->enemy = ent;
		self->enemy->owner = self;
		self->monsterinfo.aiflags |= AI_MEDIC;
		FoundTarget (self, NULL);
	}
}

void medic_search (edict_t *self)
{
	edict_t	*ent;

	gi.sound (self, CHAN_VOICE, medic_sound_search, 1, ATTN_IDLE, 0);

	if (!self->oldenemy)
	{
		ent = medic_FindDeadMonster(self);
		if (ent)
		{
			self->oldenemy = self->enemy;
			self->enemy = ent;
			self->enemy->owner = self;
			self->monsterinfo.aiflags |= AI_MEDIC;
			FoundTarget (self, NULL);
		}
	}
}

void medic_sight (edict_t *self, edict_t *other)
{
	gi.sound (self, CHAN_VOICE, medic_sound_sight, 1, ATTN_NORM, 0);
}


mframe_t medic_frames_stand [] =
{
	ai_stand, 0, medic_idle,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,

};
mmove_t medic_move_stand = {MEDIC_FRAME_wait1, MEDIC_FRAME_wait90, medic_frames_stand, NULL};

void medic_stand (edict_t *self)
{
	self->monsterinfo.currentmove = &medic_move_stand;
}


mframe_t medic_frames_walk [] =
{
	ai_walk, 6.2,	NULL,
	ai_walk, 18.1,  NULL,
	ai_walk, 1,		NULL,
	ai_walk, 9,		NULL,
	ai_walk, 10,	NULL,
	ai_walk, 9,		NULL,
	ai_walk, 11,	NULL,
	ai_walk, 11.6,  NULL,
	ai_walk, 2,		NULL,
	ai_walk, 9.9,	NULL,
	ai_walk, 14,	NULL,
	ai_walk, 9.3,	NULL
};
mmove_t medic_move_walk = {MEDIC_FRAME_walk1, MEDIC_FRAME_walk12, medic_frames_walk, NULL};

void medic_walk (edict_t *self)
{
	self->monsterinfo.currentmove = &medic_move_walk;
}


mframe_t medic_frames_run [] =
{
	ai_run, 18,		NULL,
	ai_run, 22.5,	NULL,
	ai_run, 25.4,	NULL,
	ai_run, 23.4,	NULL,
	ai_run, 24,		NULL,
	ai_run, 35.6,	NULL

};
mmove_t medic_move_run = {MEDIC_FRAME_run1, MEDIC_FRAME_run6, medic_frames_run, NULL};

void medic_run (edict_t *self)
{
	if (!(self->monsterinfo.aiflags & AI_MEDIC))
	{
		edict_t	*ent;

		ent = medic_FindDeadMonster(self);
		if (ent)
		{
			self->oldenemy = self->enemy;
			self->enemy = ent;
			self->enemy->owner = self;
			self->monsterinfo.aiflags |= AI_MEDIC;
			FoundTarget (self, NULL);
			return;
		}
	}

	if (self->monsterinfo.aiflags & AI_STAND_GROUND)
		self->monsterinfo.currentmove = &medic_move_stand;
	else
		self->monsterinfo.currentmove = &medic_move_run;
}


mframe_t medic_frames_pain1 [] =
{
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL
};
mmove_t medic_move_pain1 = {MEDIC_FRAME_paina1, MEDIC_FRAME_paina8, medic_frames_pain1, medic_run};

mframe_t medic_frames_pain2 [] =
{
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL
};
mmove_t medic_move_pain2 = {MEDIC_FRAME_painb1, MEDIC_FRAME_painb15, medic_frames_pain2, medic_run};

void medic_pain (edict_t *self, edict_t *other, float kick, int damage)
{
	if (self->health < (self->max_health / 2))
		self->s.skinnum = 1;

	if (level.time < self->pain_debounce_time)
		return;

	self->pain_debounce_time = level.time + 3;

	if (skill->value == 3)
		return;		// no pain anims in nightmare

	if (random() < 0.5)
	{
		self->monsterinfo.currentmove = &medic_move_pain1;
		gi.sound (self, CHAN_VOICE, medic_sound_pain1, 1, ATTN_NORM, 0);
	}
	else
	{
		self->monsterinfo.currentmove = &medic_move_pain2;
		gi.sound (self, CHAN_VOICE, medic_sound_pain2, 1, ATTN_NORM, 0);
	}
}

void medic_fire_blaster (edict_t *self)
{
	vec3_t	start;
	vec3_t	forward, right;
	vec3_t	end;
	vec3_t	dir;
	int		effect;

	if ((self->s.frame == MEDIC_FRAME_attack9) || (self->s.frame == MEDIC_FRAME_attack12))
		effect = EF_BLASTER;
	else if ((self->s.frame == MEDIC_FRAME_attack19) || (self->s.frame == MEDIC_FRAME_attack22) || (self->s.frame == MEDIC_FRAME_attack25) || (self->s.frame == MEDIC_FRAME_attack28))
		effect = EF_HYPERBLASTER;
	else
		effect = 0;

	AngleVectors (self->s.angles, forward, right, NULL);
	G_ProjectSource (self->s.origin, &monster_flash_offset[MZ2_MEDIC_BLASTER_1*3], forward, right, start);

	VectorCopy (self->enemy->s.origin, end);
	end[2] += self->enemy->viewheight;
	VectorSubtract (end, start, dir);

	monster_fire_blaster (self, start, dir, 2, 1000, MZ2_MEDIC_BLASTER_1, effect);
}


void medic_dead (edict_t *self)
{
	VectorSet (self->mins, -16, -16, -24);
	VectorSet (self->maxs, 16, 16, -8);
	self->movetype = MOVETYPE_TOSS;
	self->svflags |= SVF_DEADMONSTER;
	self->nextthink = 0;
	gi.linkentity (self);

	M_FlyCheck (self);

	monster_reborn(self);
}

mframe_t medic_frames_death [] =
{
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL,
	ai_move, 0, NULL
};
mmove_t medic_move_death = {MEDIC_FRAME_death1, MEDIC_FRAME_death30, medic_frames_death, medic_dead};

void medic_die (edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point)
{
	int		n;

	// if we had a pending patient, free him up for another medic
	if ((self->enemy) && (self->enemy->owner == self))
		self->enemy->owner = NULL;

// check for gib
	if (self->health <= self->gib_health)
	{
		gi.sound (self, CHAN_VOICE, gi.soundindex ("misc/udeath.wav"), 1, ATTN_NORM, 0);
		if(net_compatibility->value || !net_fixoverflow->value)
		{
			for (n= 0; n < 2; n++)
				ThrowGib (self, "models/objects/gibs/bone/tris.md2", damage, GIB_ORGANIC);
			for (n= 0; n < 4; n++)
				ThrowGib (self, "models/objects/gibs/sm_meat/tris.md2", damage, GIB_ORGANIC);
			ThrowHead (self, "models/objects/gibs/head2/tris.md2", damage, GIB_ORGANIC);
			self->deadflag = DEAD_DEAD;
		}
		else
		{
			NetThrowMonsterGibs (self, GIBS_MEDIC, damage);
			G_FreeEdict (self);
		}
		return;
	}

	if (self->deadflag == DEAD_DEAD)
		return;

// regular death
	gi.sound (self, CHAN_VOICE, medic_sound_die, 1, ATTN_NORM, 0);
	self->deadflag = DEAD_DEAD;
	self->takedamage = DAMAGE_YES;

	self->monsterinfo.currentmove = &medic_move_death;
}


void medic_duck_down (edict_t *self)
{
	if (self->monsterinfo.aiflags & AI_DUCKED)
		return;
	self->monsterinfo.aiflags |= AI_DUCKED;
	self->maxs[2] -= 32;
	self->takedamage = DAMAGE_YES;
	self->monsterinfo.pausetime = level.time + 1;
	gi.linkentity (self);
}

void medic_duck_hold (edict_t *self)
{
	if (level.time >= self->monsterinfo.pausetime)
		self->monsterinfo.aiflags &= ~AI_HOLD_FRAME;
	else
		self->monsterinfo.aiflags |= AI_HOLD_FRAME;
}

void medic_duck_up (edict_t *self)
{
	self->monsterinfo.aiflags &= ~AI_DUCKED;
	self->maxs[2] += 32;
	self->takedamage = DAMAGE_AIM;
	gi.linkentity (self);
}

mframe_t medic_frames_duck [] =
{
	ai_move, -1,	NULL,
	ai_move, -1,	NULL,
	ai_move, -1,	medic_duck_down,
	ai_move, -1,	medic_duck_hold,
	ai_move, -1,	NULL,
	ai_move, -1,	NULL,
	ai_move, -1,	medic_duck_up,
	ai_move, -1,	NULL,
	ai_move, -1,	NULL,
	ai_move, -1,	NULL,
	ai_move, -1,	NULL,
	ai_move, -1,	NULL,
	ai_move, -1,	NULL,
	ai_move, -1,	NULL,
	ai_move, -1,	NULL,
	ai_move, -1,	NULL
};
mmove_t medic_move_duck = {MEDIC_FRAME_duck1, MEDIC_FRAME_duck16, medic_frames_duck, medic_run};

void medic_dodge (edict_t *self, edict_t *attacker, float eta)
{
	if (random() > 0.25)
		return;

	if (!self->enemy)
		self->enemy = attacker;

	self->monsterinfo.currentmove = &medic_move_duck;
}

mframe_t medic_frames_attackHyperBlaster [] =
{
	ai_charge, 0,	NULL,
	ai_charge, 0,	NULL,
	ai_charge, 0,	NULL,
	ai_charge, 0,	NULL,
	ai_charge, 0,	medic_fire_blaster,
	ai_charge, 0,	medic_fire_blaster,
	ai_charge, 0,	medic_fire_blaster,
	ai_charge, 0,	medic_fire_blaster,
	ai_charge, 0,	medic_fire_blaster,
	ai_charge, 0,	medic_fire_blaster,
	ai_charge, 0,	medic_fire_blaster,
	ai_charge, 0,	medic_fire_blaster,
	ai_charge, 0,	medic_fire_blaster,
	ai_charge, 0,	medic_fire_blaster,
	ai_charge, 0,	medic_fire_blaster,
	ai_charge, 0,	medic_fire_blaster
};
mmove_t medic_move_attackHyperBlaster = {MEDIC_FRAME_attack15, MEDIC_FRAME_attack30, medic_frames_attackHyperBlaster, medic_run};


void medic_continue (edict_t *self)
{
	if (visible (self, self->enemy) )
		if (random() <= 0.95)
			self->monsterinfo.currentmove = &medic_move_attackHyperBlaster;
}


mframe_t medic_frames_attackBlaster [] =
{
	ai_charge, 0,	NULL,
	ai_charge, 5,	NULL,
	ai_charge, 5,	NULL,
	ai_charge, 3,	NULL,
	ai_charge, 2,	NULL,
	ai_charge, 0,	NULL,
	ai_charge, 0,	NULL,
	ai_charge, 0,	NULL,
	ai_charge, 0,	medic_fire_blaster,
	ai_charge, 0,	NULL,
	ai_charge, 0,	NULL,
	ai_charge, 0,	medic_fire_blaster,
	ai_charge, 0,	NULL,
	ai_charge, 0,	medic_continue	// Change to medic_continue... Else, go to frame 32
};
mmove_t medic_move_attackBlaster = {MEDIC_FRAME_attack1, MEDIC_FRAME_attack14, medic_frames_attackBlaster, medic_run};


void medic_hook_launch (edict_t *self)
{
	gi.sound (self, CHAN_WEAPON, medic_sound_hook_launch, 1, ATTN_NORM, 0);
}

void ED_CallSpawn (edict_t *ent);

static vec3_t	medic_cable_offsets[] =
{
	45.0,  -9.2, 15.5,
	48.4,  -9.7, 15.2,
	47.8,  -9.8, 15.8,
	47.3,  -9.3, 14.3,
	45.4, -10.1, 13.1,
	41.9, -12.7, 12.0,
	37.8, -15.8, 11.2,
	34.3, -18.4, 10.7,
	32.7, -19.7, 10.4,
	32.7, -19.7, 10.4
};

void medic_cable_attack (edict_t *self)
{
	vec3_t	offset, start, end, f, r;
	trace_t	tr;
	vec3_t	dir, angles;
	float	distance;

	if (!self->enemy->inuse)
		return;

	AngleVectors (self->s.angles, f, r, NULL);
	VectorCopy (medic_cable_offsets[self->s.frame - MEDIC_FRAME_attack42], offset);
	G_ProjectSource (self->s.origin, offset, f, r, start);

	// check for max distance
	VectorSubtract (start, self->enemy->s.origin, dir);
	distance = VectorLength(dir);
	if (distance > 256)
		return;

	// check for min/max pitch
	vectoangles (dir, angles);
	if (angles[0] < -180)
		angles[0] += 360;
	if (fabs(angles[0]) > 45)
		return;

	tr = gi.trace (start, NULL, NULL, self->enemy->s.origin, self, MASK_SHOT);
	if (tr.fraction != 1.0 && tr.ent != self->enemy)
		return;

	if (self->s.frame == MEDIC_FRAME_attack43)
	{
		gi.sound (self->enemy, CHAN_AUTO, medic_sound_hook_hit, 1, ATTN_NORM, 0);
		self->enemy->monsterinfo.aiflags |= AI_RESURRECTING;
	}
	else if (self->s.frame == MEDIC_FRAME_attack50)
	{
		self->enemy->spawnflags = 0;
		self->enemy->monsterinfo.aiflags = 0;
		self->enemy->target = NULL;
		self->enemy->targetname = NULL;
		self->enemy->combattarget = NULL;
		self->enemy->deathtarget = NULL;
		self->enemy->owner = self;
		ED_CallSpawn (self->enemy);
		self->enemy->owner = NULL;
		if (self->enemy->think)
		{
			self->enemy->nextthink = level.time;
			self->enemy->think (self->enemy);
		}
		self->enemy->monsterinfo.aiflags |= AI_RESURRECTING;
		if (self->oldenemy && self->oldenemy->client)
		{
			self->enemy->enemy = self->oldenemy;
			FoundTarget (self->enemy, NULL);
		}
	}
	else
	{
		if (self->s.frame == MEDIC_FRAME_attack44)
			gi.sound (self, CHAN_WEAPON, medic_sound_hook_heal, 1, ATTN_NORM, 0);
	}

	// adjust start for beam origin being in middle of a segment
	VectorMA (start, 8, f, start);

	// adjust end z for end spot since the monster is currently dead
	VectorCopy (self->enemy->s.origin, end);
	end[2] = self->enemy->absmin[2] + self->enemy->size[2] / 2;

	bool large_map = LargeMapCheck(start) | LargeMapCheck(end);
	if (large_map)
		gi.WriteByte (svc_temp_entity_ext);
	else
		gi.WriteByte (svc_temp_entity);
	gi.WriteByte (TE_MEDIC_CABLE_ATTACK);
	gi.WriteShort (self - g_edicts);
	gi.WritePosition (start, large_map);
	gi.WritePosition (end, large_map);
	gi.multicast (self->s.origin, MULTICAST_PVS);
}

void medic_hook_retract (edict_t *self)
{
	gi.sound (self, CHAN_WEAPON, medic_sound_hook_retract, 1, ATTN_NORM, 0);
	self->enemy->monsterinfo.aiflags &= ~AI_RESURRECTING;
}

mframe_t medic_frames_attackCable [] =
{
	ai_move, 2,		NULL,
	ai_move, 3,		NULL,
	ai_move, 5,		NULL,
	ai_move, 4.4,	NULL,
	ai_charge, 4.7,	NULL,
	ai_charge, 5,	NULL,
	ai_charge, 6,	NULL,
	ai_charge, 4,	NULL,
	ai_charge, 0,	NULL,
	ai_move, 0,		medic_hook_launch,
	ai_move, 0,		medic_cable_attack,
	ai_move, 0,		medic_cable_attack,
	ai_move, 0,		medic_cable_attack,
	ai_move, 0,		medic_cable_attack,
	ai_move, 0,		medic_cable_attack,
	ai_move, 0,		medic_cable_attack,
	ai_move, 0,		medic_cable_attack,
	ai_move, 0,		medic_cable_attack,
	ai_move, 0,		medic_cable_attack,
	ai_move, -15,	medic_hook_retract,
	ai_move, -1.5,	NULL,
	ai_move, -1.2,	NULL,
	ai_move, -3,	NULL,
	ai_move, -2,	NULL,
	ai_move, 0.3,	NULL,
	ai_move, 0.7,	NULL,
	ai_move, 1.2,	NULL,
	ai_move, 1.3,	NULL
};
mmove_t medic_move_attackCable = {MEDIC_FRAME_attack33, MEDIC_FRAME_attack60, medic_frames_attackCable, medic_run};


void medic_attack(edict_t *self)
{
	if (self->monsterinfo.aiflags & AI_MEDIC)
		self->monsterinfo.currentmove = &medic_move_attackCable;
	else
		self->monsterinfo.currentmove = &medic_move_attackBlaster;
}

bool medic_checkattack (edict_t *self)
{
	if (self->monsterinfo.aiflags & AI_MEDIC)
	{
		medic_attack(self);
		return true;
	}

	return M_CheckAttack (self);
}


/*QUAKED monster_medic (1 .5 0) (-16 -16 -24) (16 16 32) Ambush Trigger_Spawn Sight
*/
void SP_monster_medic (edict_t *self)
{
	if (deathmatch->value)
	{
		G_FreeEdict (self);
		return;
	}

	medic_sound_idle1 = gi.soundindex ("medic/idle.wav");
	medic_sound_pain1 = gi.soundindex ("medic/medpain1.wav");
	medic_sound_pain2 = gi.soundindex ("medic/medpain2.wav");
	medic_sound_die = gi.soundindex ("medic/meddeth1.wav");
	medic_sound_sight = gi.soundindex ("medic/medsght1.wav");
	medic_sound_search = gi.soundindex ("medic/medsrch1.wav");
	medic_sound_hook_launch = gi.soundindex ("medic/medatck2.wav");
	medic_sound_hook_hit = gi.soundindex ("medic/medatck3.wav");
	medic_sound_hook_heal = gi.soundindex ("medic/medatck4.wav");
	medic_sound_hook_retract = gi.soundindex ("medic/medatck5.wav");

	gi.soundindex ("medic/medatck1.wav");

	self->movetype = MOVETYPE_STEP;
	self->solid = SOLID_BBOX;
	self->s.modelindex = gi.modelindex ("models/monsters/medic/tris.md2");
	VectorSet (self->mins, -24, -24, -24);
	VectorSet (self->maxs, 24, 24, 32);

	self->health = 300;
	self->gib_health = -130;
	self->mass = 400;

	self->pain = medic_pain;
	self->die = medic_die;

	self->monsterinfo.stand = medic_stand;
	self->monsterinfo.walk = medic_walk;
	self->monsterinfo.run = medic_run;
	self->monsterinfo.dodge = medic_dodge;
	self->monsterinfo.attack = medic_attack;
	self->monsterinfo.melee = NULL;
	self->monsterinfo.sight = medic_sight;
	self->monsterinfo.idle = medic_idle;
	self->monsterinfo.search = medic_search;
	self->monsterinfo.checkattack = medic_checkattack;

	gi.linkentity (self);

	self->monsterinfo.currentmove = &medic_move_stand;
	self->monsterinfo.scale = MEDIC_MODEL_SCALE;

	walkmonster_start (self, false);
}

/*
==============================================================================

END of MEDIC

==============================================================================
*/



/*
==============================================================================

flyer

==============================================================================
*/

static int	nextmove;			// Used for start/stop frames

void flyer_check_melee(edict_t *self);
void flyer_loop_melee (edict_t *self);
void flyer_melee (edict_t *self);
void flyer_setstart (edict_t *self);
void flyer_stand (edict_t *self);
void flyer_nextmove (edict_t *self);


void flyer_sight (edict_t *self, edict_t *other)
{
	gi.sound (self, CHAN_VOICE, flyer_sound_sight, 1, ATTN_NORM, 0);
}

void flyer_idle (edict_t *self)
{
	gi.sound (self, CHAN_VOICE, flyer_sound_idle, 1, ATTN_IDLE, 0);
}

void flyer_pop_blades (edict_t *self)
{
	gi.sound (self, CHAN_VOICE, flyer_sound_sproing, 1, ATTN_NORM, 0);
}


mframe_t flyer_frames_stand [] =
{
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL
};
mmove_t	flyer_move_stand = {FLYER_FRAME_stand01, FLYER_FRAME_stand45, flyer_frames_stand, NULL};


mframe_t flyer_frames_walk [] =
{
	ai_walk, 5, NULL,
	ai_walk, 5, NULL,
	ai_walk, 5, NULL,
	ai_walk, 5, NULL,
	ai_walk, 5, NULL,
	ai_walk, 5, NULL,
	ai_walk, 5, NULL,
	ai_walk, 5, NULL,
	ai_walk, 5, NULL,
	ai_walk, 5, NULL,
	ai_walk, 5, NULL,
	ai_walk, 5, NULL,
	ai_walk, 5, NULL,
	ai_walk, 5, NULL,
	ai_walk, 5, NULL,
	ai_walk, 5, NULL,
	ai_walk, 5, NULL,
	ai_walk, 5, NULL,
	ai_walk, 5, NULL,
	ai_walk, 5, NULL,
	ai_walk, 5, NULL,
	ai_walk, 5, NULL,
	ai_walk, 5, NULL,
	ai_walk, 5, NULL,
	ai_walk, 5, NULL,
	ai_walk, 5, NULL,
	ai_walk, 5, NULL,
	ai_walk, 5, NULL,
	ai_walk, 5, NULL,
	ai_walk, 5, NULL,
	ai_walk, 5, NULL,
	ai_walk, 5, NULL,
	ai_walk, 5, NULL,
	ai_walk, 5, NULL,
	ai_walk, 5, NULL,
	ai_walk, 5, NULL,
	ai_walk, 5, NULL,
	ai_walk, 5, NULL,
	ai_walk, 5, NULL,
	ai_walk, 5, NULL,
	ai_walk, 5, NULL,
	ai_walk, 5, NULL,
	ai_walk, 5, NULL,
	ai_walk, 5, NULL,
	ai_walk, 5, NULL
};
mmove_t	flyer_move_walk = {FLYER_FRAME_stand01, FLYER_FRAME_stand45, flyer_frames_walk, NULL};

mframe_t flyer_frames_run [] =
{
	ai_run, 10, NULL,
	ai_run, 10, NULL,
	ai_run, 10, NULL,
	ai_run, 10, NULL,
	ai_run, 10, NULL,
	ai_run, 10, NULL,
	ai_run, 10, NULL,
	ai_run, 10, NULL,
	ai_run, 10, NULL,
	ai_run, 10, NULL,
	ai_run, 10, NULL,
	ai_run, 10, NULL,
	ai_run, 10, NULL,
	ai_run, 10, NULL,
	ai_run, 10, NULL,
	ai_run, 10, NULL,
	ai_run, 10, NULL,
	ai_run, 10, NULL,
	ai_run, 10, NULL,
	ai_run, 10, NULL,
	ai_run, 10, NULL,
	ai_run, 10, NULL,
	ai_run, 10, NULL,
	ai_run, 10, NULL,
	ai_run, 10, NULL,
	ai_run, 10, NULL,
	ai_run, 10, NULL,
	ai_run, 10, NULL,
	ai_run, 10, NULL,
	ai_run, 10, NULL,
	ai_run, 10, NULL,
	ai_run, 10, NULL,
	ai_run, 10, NULL,
	ai_run, 10, NULL,
	ai_run, 10, NULL,
	ai_run, 10, NULL,
	ai_run, 10, NULL,
	ai_run, 10, NULL,
	ai_run, 10, NULL,
	ai_run, 10, NULL,
	ai_run, 10, NULL,
	ai_run, 10, NULL,
	ai_run, 10, NULL,
	ai_run, 10, NULL,
	ai_run, 10, NULL
};
mmove_t	flyer_move_run = {FLYER_FRAME_stand01, FLYER_FRAME_stand45, flyer_frames_run, NULL};

void flyer_run (edict_t *self)
{
	if (self->monsterinfo.aiflags & AI_STAND_GROUND)
		self->monsterinfo.currentmove = &flyer_move_stand;
	else
		self->monsterinfo.currentmove = &flyer_move_run;
}

void flyer_walk (edict_t *self)
{
	self->monsterinfo.currentmove = &flyer_move_walk;
}

void flyer_stand (edict_t *self)
{
		self->monsterinfo.currentmove = &flyer_move_stand;
}

mframe_t flyer_frames_start [] =
{
		ai_move, 0,	NULL,
		ai_move, 0,	NULL,
		ai_move, 0,	NULL,
		ai_move, 0,	NULL,
		ai_move, 0,	NULL,
		ai_move, 0,	flyer_nextmove
};
mmove_t flyer_move_start = {FLYER_FRAME_start01, FLYER_FRAME_start06, flyer_frames_start, NULL};

mframe_t flyer_frames_stop [] =
{
		ai_move, 0,	NULL,
		ai_move, 0,	NULL,
		ai_move, 0,	NULL,
		ai_move, 0,	NULL,
		ai_move, 0,	NULL,
		ai_move, 0,	NULL,
		ai_move, 0,	flyer_nextmove
};
mmove_t flyer_move_stop = {FLYER_FRAME_stop01, FLYER_FRAME_stop07, flyer_frames_stop, NULL};

void flyer_stop (edict_t *self)
{
		self->monsterinfo.currentmove = &flyer_move_stop;
}

void flyer_start (edict_t *self)
{
		self->monsterinfo.currentmove = &flyer_move_start;
}


mframe_t flyer_frames_rollright [] =
{
		ai_move, 0, NULL,
		ai_move, 0, NULL,
		ai_move, 0, NULL,
		ai_move, 0, NULL,
		ai_move, 0, NULL,
		ai_move, 0, NULL,
		ai_move, 0, NULL,
		ai_move, 0, NULL,
		ai_move, 0, NULL
};
mmove_t flyer_move_rollright = {FLYER_FRAME_rollr01, FLYER_FRAME_rollr09, flyer_frames_rollright, NULL};

mframe_t flyer_frames_rollleft [] =
{
		ai_move, 0, NULL,
		ai_move, 0, NULL,
		ai_move, 0, NULL,
		ai_move, 0, NULL,
		ai_move, 0, NULL,
		ai_move, 0, NULL,
		ai_move, 0, NULL,
		ai_move, 0, NULL,
		ai_move, 0, NULL
};
mmove_t flyer_move_rollleft = {FLYER_FRAME_rollf01, FLYER_FRAME_rollf09, flyer_frames_rollleft, NULL};

mframe_t flyer_frames_pain3 [] =
{
		ai_move, 0, NULL,
		ai_move, 0, NULL,
		ai_move, 0, NULL,
		ai_move, 0, NULL
};
mmove_t flyer_move_pain3 = {FLYER_FRAME_pain301, FLYER_FRAME_pain304, flyer_frames_pain3, flyer_run};

mframe_t flyer_frames_pain2 [] =
{
		ai_move, 0, NULL,
		ai_move, 0, NULL,
		ai_move, 0, NULL,
		ai_move, 0, NULL
};
mmove_t flyer_move_pain2 = {FLYER_FRAME_pain201, FLYER_FRAME_pain204, flyer_frames_pain2, flyer_run};

mframe_t flyer_frames_pain1 [] =
{
		ai_move, 0, NULL,
		ai_move, 0, NULL,
		ai_move, 0, NULL,
		ai_move, 0, NULL,
		ai_move, 0, NULL,
		ai_move, 0, NULL,
		ai_move, 0, NULL,
		ai_move, 0, NULL,
		ai_move, 0, NULL
};
mmove_t flyer_move_pain1 = {FLYER_FRAME_pain101, FLYER_FRAME_pain109, flyer_frames_pain1, flyer_run};

mframe_t flyer_frames_defense [] =
{
		ai_move, 0, NULL,
		ai_move, 0, NULL,
		ai_move, 0, NULL,		// Hold this frame
		ai_move, 0, NULL,
		ai_move, 0, NULL,
		ai_move, 0, NULL
};
mmove_t flyer_move_defense = {FLYER_FRAME_defens01, FLYER_FRAME_defens06, flyer_frames_defense, NULL};

mframe_t flyer_frames_bankright [] =
{
		ai_move, 0, NULL,
		ai_move, 0, NULL,
		ai_move, 0, NULL,
		ai_move, 0, NULL,
		ai_move, 0, NULL,
		ai_move, 0, NULL,
		ai_move, 0, NULL
};
mmove_t flyer_move_bankright = {FLYER_FRAME_bankr01, FLYER_FRAME_bankr07, flyer_frames_bankright, NULL};

mframe_t flyer_frames_bankleft [] =
{
		ai_move, 0, NULL,
		ai_move, 0, NULL,
		ai_move, 0, NULL,
		ai_move, 0, NULL,
		ai_move, 0, NULL,
		ai_move, 0, NULL,
		ai_move, 0, NULL
};
mmove_t flyer_move_bankleft = {FLYER_FRAME_bankl01, FLYER_FRAME_bankl07, flyer_frames_bankleft, NULL};


void flyer_fire (edict_t *self, int flash_number)
{
	vec3_t	start;
	vec3_t	forward, right;
	vec3_t	end;
	vec3_t	dir;
	int		effect;

	if ((self->s.frame == FLYER_FRAME_attak204) || (self->s.frame == FLYER_FRAME_attak207) || (self->s.frame == FLYER_FRAME_attak210))
		effect = EF_HYPERBLASTER;
	else
		effect = 0;
	AngleVectors (self->s.angles, forward, right, NULL);
	G_ProjectSource (self->s.origin, &monster_flash_offset[flash_number*3], forward, right, start);

	VectorCopy (self->enemy->s.origin, end);
	end[2] += self->enemy->viewheight;
	VectorSubtract (end, start, dir);

	monster_fire_blaster (self, start, dir, 1, 1000, flash_number, effect);
}

void flyer_fireleft (edict_t *self)
{
	flyer_fire (self, MZ2_FLYER_BLASTER_1);
}

void flyer_fireright (edict_t *self)
{
	flyer_fire (self, MZ2_FLYER_BLASTER_2);
}


mframe_t flyer_frames_attack2 [] =
{
		ai_charge, 0, NULL,
		ai_charge, 0, NULL,
		ai_charge, 0, NULL,
		ai_charge, -10, flyer_fireleft,			// left gun
		ai_charge, -10, flyer_fireright,		// right gun
		ai_charge, -10, flyer_fireleft,			// left gun
		ai_charge, -10, flyer_fireright,		// right gun
		ai_charge, -10, flyer_fireleft,			// left gun
		ai_charge, -10, flyer_fireright,		// right gun
		ai_charge, -10, flyer_fireleft,			// left gun
		ai_charge, -10, flyer_fireright,		// right gun
		ai_charge, 0, NULL,
		ai_charge, 0, NULL,
		ai_charge, 0, NULL,
		ai_charge, 0, NULL,
		ai_charge, 0, NULL,
		ai_charge, 0, NULL
};
mmove_t flyer_move_attack2 = {FLYER_FRAME_attak201, FLYER_FRAME_attak217, flyer_frames_attack2, flyer_run};


void flyer_slash_left (edict_t *self)
{
	vec3_t	aim;

	VectorSet (aim, MELEE_DISTANCE, self->mins[0], 0);
	fire_hit (self, aim, 5, 0);
	gi.sound (self, CHAN_WEAPON, flyer_sound_slash, 1, ATTN_NORM, 0);
}

void flyer_slash_right (edict_t *self)
{
	vec3_t	aim;

	VectorSet (aim, MELEE_DISTANCE, self->maxs[0], 0);
	fire_hit (self, aim, 5, 0);
	gi.sound (self, CHAN_WEAPON, flyer_sound_slash, 1, ATTN_NORM, 0);
}

mframe_t flyer_frames_start_melee [] =
{
		ai_charge, 0, flyer_pop_blades,
		ai_charge, 0, NULL,
		ai_charge, 0, NULL,
		ai_charge, 0, NULL,
		ai_charge, 0, NULL,
		ai_charge, 0, NULL
};
mmove_t flyer_move_start_melee = {FLYER_FRAME_attak101, FLYER_FRAME_attak106, flyer_frames_start_melee, flyer_loop_melee};

mframe_t flyer_frames_end_melee [] =
{
		ai_charge, 0, NULL,
		ai_charge, 0, NULL,
		ai_charge, 0, NULL
};
mmove_t flyer_move_end_melee = {FLYER_FRAME_attak119, FLYER_FRAME_attak121, flyer_frames_end_melee, flyer_run};


mframe_t flyer_frames_loop_melee [] =
{
		ai_charge, 0, NULL,		// Loop Start
		ai_charge, 0, NULL,
		ai_charge, 0, flyer_slash_left,		// Left Wing Strike
		ai_charge, 0, NULL,
		ai_charge, 0, NULL,
		ai_charge, 0, NULL,
		ai_charge, 0, NULL,
		ai_charge, 0, flyer_slash_right,	// Right Wing Strike
		ai_charge, 0, NULL,
		ai_charge, 0, NULL,
		ai_charge, 0, NULL,
		ai_charge, 0, NULL		// Loop Ends

};
mmove_t flyer_move_loop_melee = {FLYER_FRAME_attak107, FLYER_FRAME_attak118, flyer_frames_loop_melee, flyer_check_melee};

void flyer_loop_melee (edict_t *self)
{
/*	if (random() <= 0.5)
		self->monsterinfo.currentmove = &flyer_move_attack1;
	else */
	self->monsterinfo.currentmove = &flyer_move_loop_melee;
}



void flyer_attack (edict_t *self)
{
/*	if (random() <= 0.5)
		self->monsterinfo.currentmove = &flyer_move_attack1;
	else */
	self->monsterinfo.currentmove = &flyer_move_attack2;
}

void flyer_setstart (edict_t *self)
{
	nextmove = ACTION_run;
	self->monsterinfo.currentmove = &flyer_move_start;
}

void flyer_nextmove (edict_t *self)
{
	if (nextmove == ACTION_attack1)
		self->monsterinfo.currentmove = &flyer_move_start_melee;
	else if (nextmove == ACTION_attack2)
		self->monsterinfo.currentmove = &flyer_move_attack2;
	else if (nextmove == ACTION_run)
		self->monsterinfo.currentmove = &flyer_move_run;
}

void flyer_melee (edict_t *self)
{
//	flyer.nextmove = ACTION_attack1;
//	self->monsterinfo.currentmove = &flyer_move_stop;
	self->monsterinfo.currentmove = &flyer_move_start_melee;
}

void flyer_check_melee(edict_t *self)
{
	if (range (self, self->enemy) == RANGE_MELEE)
		if (random() <= 0.8)
			self->monsterinfo.currentmove = &flyer_move_loop_melee;
		else
			self->monsterinfo.currentmove = &flyer_move_end_melee;
	else
		self->monsterinfo.currentmove = &flyer_move_end_melee;
}

void flyer_pain (edict_t *self, edict_t *other, float kick, int damage)
{
	int		n;

	if (self->health < (self->max_health / 2))
		self->s.skinnum = 1;

	if (level.time < self->pain_debounce_time)
		return;

	self->pain_debounce_time = level.time + 3;
	if (skill->value == 3)
		return;		// no pain anims in nightmare

	n = rand() % 3;
	if (n == 0)
	{
		gi.sound (self, CHAN_VOICE, flyer_sound_pain1, 1, ATTN_NORM, 0);
		self->monsterinfo.currentmove = &flyer_move_pain1;
	}
	else if (n == 1)
	{
		gi.sound (self, CHAN_VOICE, flyer_sound_pain2, 1, ATTN_NORM, 0);
		self->monsterinfo.currentmove = &flyer_move_pain2;
	}
	else
	{
		gi.sound (self, CHAN_VOICE, flyer_sound_pain1, 1, ATTN_NORM, 0);
		self->monsterinfo.currentmove = &flyer_move_pain3;
	}
}


void flyer_die(edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point)
{
	gi.sound (self, CHAN_VOICE, flyer_sound_die, 1, ATTN_NORM, 0);

//// Berserker: пусть flyer разбрасывает gibs при взрыве
	if(net_compatibility->value || !net_fixoverflow->value)
	{
		int	n;
		for (n= 0; n < 2; n++)
			ThrowGib (self, "models/objects/gibs/bone/tris.md2", 1000, GIB_ORGANIC);
		for (n= 0; n < 2; n++)
			ThrowGib (self, "models/objects/gibs/sm_meat/tris.md2", 1000, GIB_ORGANIC);
	}
	else
	{
//		self->s.origin[2] += 16;	// приподымем немного, чтоб джибсы не пропадали под полом
		NetThrowMonsterGibs (self, GIBS_FLYER, 1000);
//		self->s.origin[2] -= 16;	// вернём взад
	}
////

	BecomeExplosion1(self);
}


/*QUAKED monster_flyer (1 .5 0) (-16 -16 -24) (16 16 32) Ambush Trigger_Spawn Sight
*/
void SP_monster_flyer (edict_t *self)
{
	if (deathmatch->value)
	{
		G_FreeEdict (self);
		return;
	}

	// fix a map bug in jail5.bsp
	if (!_stricmp(level.mapname, "jail5") && (self->s.origin[2] == -104))
	{
		self->targetname = self->target;
		self->target = NULL;
	}

	flyer_sound_sight = gi.soundindex ("flyer/flysght1.wav");
	flyer_sound_idle = gi.soundindex ("flyer/flysrch1.wav");
	flyer_sound_pain1 = gi.soundindex ("flyer/flypain1.wav");
	flyer_sound_pain2 = gi.soundindex ("flyer/flypain2.wav");
	flyer_sound_slash = gi.soundindex ("flyer/flyatck2.wav");
	flyer_sound_sproing = gi.soundindex ("flyer/flyatck1.wav");
	flyer_sound_die = gi.soundindex ("flyer/flydeth1.wav");

	gi.soundindex ("flyer/flyatck3.wav");

	self->s.modelindex = gi.modelindex ("models/monsters/flyer/tris.md2");
	VectorSet (self->mins, -16, -16, -24);
	VectorSet (self->maxs, 16, 16, 32);
	self->movetype = MOVETYPE_STEP;
	self->solid = SOLID_BBOX;

	self->s.sound = gi.soundindex ("flyer/flyidle1.wav");

	self->health = 50;
	self->mass = 50;

	self->pain = flyer_pain;
	self->die = flyer_die;

	self->monsterinfo.stand = flyer_stand;
	self->monsterinfo.walk = flyer_walk;
	self->monsterinfo.run = flyer_run;
	self->monsterinfo.attack = flyer_attack;
	self->monsterinfo.melee = flyer_melee;
	self->monsterinfo.sight = flyer_sight;
	self->monsterinfo.idle = flyer_idle;

	gi.linkentity (self);

	self->monsterinfo.currentmove = &flyer_move_stand;
	self->monsterinfo.scale = FLYER_MODEL_SCALE;

	flymonster_start (self, false);
}

/*
==============================================================================

end of flyer

==============================================================================
*/


/*
==============================================================================

brain

==============================================================================
*/

void brain_sight (edict_t *self, edict_t *other)
{
	gi.sound (self, CHAN_VOICE, brain_sound_sight, 1, ATTN_NORM, 0);
}

void brain_search (edict_t *self)
{
	gi.sound (self, CHAN_VOICE, brain_sound_search, 1, ATTN_NORM, 0);
}


void brain_run (edict_t *self);
void brain_dead (edict_t *self);


//
// STAND
//

mframe_t brain_frames_stand [] =
{
	ai_stand,	0,	NULL,
	ai_stand,	0,	NULL,
	ai_stand,	0,	NULL,
	ai_stand,	0,	NULL,
	ai_stand,	0,	NULL,
	ai_stand,	0,	NULL,
	ai_stand,	0,	NULL,
	ai_stand,	0,	NULL,
	ai_stand,	0,	NULL,
	ai_stand,	0,	NULL,

	ai_stand,	0,	NULL,
	ai_stand,	0,	NULL,
	ai_stand,	0,	NULL,
	ai_stand,	0,	NULL,
	ai_stand,	0,	NULL,
	ai_stand,	0,	NULL,
	ai_stand,	0,	NULL,
	ai_stand,	0,	NULL,
	ai_stand,	0,	NULL,
	ai_stand,	0,	NULL,

	ai_stand,	0,	NULL,
	ai_stand,	0,	NULL,
	ai_stand,	0,	NULL,
	ai_stand,	0,	NULL,
	ai_stand,	0,	NULL,
	ai_stand,	0,	NULL,
	ai_stand,	0,	NULL,
	ai_stand,	0,	NULL,
	ai_stand,	0,	NULL,
	ai_stand,	0,	NULL
};
mmove_t brain_move_stand = {BRAIN_FRAME_stand01, BRAIN_FRAME_stand30, brain_frames_stand, NULL};

void brain_stand (edict_t *self)
{
	self->monsterinfo.currentmove = &brain_move_stand;
}


//
// IDLE
//

mframe_t brain_frames_idle [] =
{
	ai_stand,	0,	NULL,
	ai_stand,	0,	NULL,
	ai_stand,	0,	NULL,
	ai_stand,	0,	NULL,
	ai_stand,	0,	NULL,
	ai_stand,	0,	NULL,
	ai_stand,	0,	NULL,
	ai_stand,	0,	NULL,
	ai_stand,	0,	NULL,
	ai_stand,	0,	NULL,

	ai_stand,	0,	NULL,
	ai_stand,	0,	NULL,
	ai_stand,	0,	NULL,
	ai_stand,	0,	NULL,
	ai_stand,	0,	NULL,
	ai_stand,	0,	NULL,
	ai_stand,	0,	NULL,
	ai_stand,	0,	NULL,
	ai_stand,	0,	NULL,
	ai_stand,	0,	NULL,

	ai_stand,	0,	NULL,
	ai_stand,	0,	NULL,
	ai_stand,	0,	NULL,
	ai_stand,	0,	NULL,
	ai_stand,	0,	NULL,
	ai_stand,	0,	NULL,
	ai_stand,	0,	NULL,
	ai_stand,	0,	NULL,
	ai_stand,	0,	NULL,
	ai_stand,	0,	NULL
};
mmove_t brain_move_idle = {BRAIN_FRAME_stand31, BRAIN_FRAME_stand60, brain_frames_idle, brain_stand};

void brain_idle (edict_t *self)
{
	gi.sound (self, CHAN_AUTO, brain_sound_idle3, 1, ATTN_IDLE, 0);
	self->monsterinfo.currentmove = &brain_move_idle;
}


//
// WALK
//
mframe_t brain_frames_walk1 [] =
{
	ai_walk,	7,	NULL,
	ai_walk,	2,	NULL,
	ai_walk,	3,	NULL,
	ai_walk,	3,	NULL,
	ai_walk,	1,	NULL,
	ai_walk,	0,	NULL,
	ai_walk,	0,	NULL,
	ai_walk,	9,	NULL,
	ai_walk,	-4,	NULL,
	ai_walk,	-1,	NULL,
	ai_walk,	2,	NULL
};
mmove_t brain_move_walk1 = {BRAIN_FRAME_walk101, BRAIN_FRAME_walk111, brain_frames_walk1, NULL};

// walk2 is FUBAR, do not use
#if 0
void brain_walk2_cycle (edict_t *self)
{
	if (random() > 0.1)
		self->monsterinfo.nextframe = BRAIN_FRAME_walk220;
}

mframe_t brain_frames_walk2 [] =
{
	ai_walk,	3,	NULL,
	ai_walk,	-2,	NULL,
	ai_walk,	-4,	NULL,
	ai_walk,	-3,	NULL,
	ai_walk,	0,	NULL,
	ai_walk,	1,	NULL,
	ai_walk,	12,	NULL,
	ai_walk,	0,	NULL,
	ai_walk,	-3,	NULL,
	ai_walk,	0,	NULL,

	ai_walk,	-2,	NULL,
	ai_walk,	0,	NULL,
	ai_walk,	0,	NULL,
	ai_walk,	1,	NULL,
	ai_walk,	0,	NULL,
	ai_walk,	0,	NULL,
	ai_walk,	0,	NULL,
	ai_walk,	0,	NULL,
	ai_walk,	0,	NULL,
	ai_walk,	10,	NULL,		// Cycle Start

	ai_walk,	-1,	NULL,
	ai_walk,	7,	NULL,
	ai_walk,	0,	NULL,
	ai_walk,	3,	NULL,
	ai_walk,	-3,	NULL,
	ai_walk,	2,	NULL,
	ai_walk,	4,	NULL,
	ai_walk,	-3,	NULL,
	ai_walk,	2,	NULL,
	ai_walk,	0,	NULL,

	ai_walk,	4,	brain_walk2_cycle,
	ai_walk,	-1,	NULL,
	ai_walk,	-1,	NULL,
	ai_walk,	-8,	NULL,
	ai_walk,	0,	NULL,
	ai_walk,	1,	NULL,
	ai_walk,	5,	NULL,
	ai_walk,	2,	NULL,
	ai_walk,	-1,	NULL,
	ai_walk,	-5,	NULL
};
mmove_t brain_move_walk2 = {BRAIN_FRAME_walk201, BRAIN_FRAME_walk240, brain_frames_walk2, NULL};
#endif

void brain_walk (edict_t *self)
{
//	if (random() <= 0.5)
		self->monsterinfo.currentmove = &brain_move_walk1;
//	else
//		self->monsterinfo.currentmove = &brain_move_walk2;
}



mframe_t brain_frames_defense [] =
{
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL
};
mmove_t brain_move_defense = {BRAIN_FRAME_defens01, BRAIN_FRAME_defens08, brain_frames_defense, NULL};

mframe_t brain_frames_pain3 [] =
{
	ai_move,	-2,	NULL,
	ai_move,	2,	NULL,
	ai_move,	1,	NULL,
	ai_move,	3,	NULL,
	ai_move,	0,	NULL,
	ai_move,	-4,	NULL
};
mmove_t brain_move_pain3 = {BRAIN_FRAME_pain301, BRAIN_FRAME_pain306, brain_frames_pain3, brain_run};

mframe_t brain_frames_pain2 [] =
{
	ai_move,	-2,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	3,	NULL,
	ai_move,	1,	NULL,
	ai_move,	-2,	NULL
};
mmove_t brain_move_pain2 = {BRAIN_FRAME_pain201, BRAIN_FRAME_pain208, brain_frames_pain2, brain_run};

mframe_t brain_frames_pain1 [] =
{
	ai_move,	-6,	NULL,
	ai_move,	-2,	NULL,
	ai_move,	-6,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	2,	NULL,
	ai_move,	0,	NULL,
	ai_move,	2,	NULL,
	ai_move,	1,	NULL,
	ai_move,	7,	NULL,
	ai_move,	0,	NULL,
	ai_move,	3,	NULL,
	ai_move,	-1,	NULL
};
mmove_t brain_move_pain1 = {BRAIN_FRAME_pain101, BRAIN_FRAME_pain121, brain_frames_pain1, brain_run};


//
// DUCK
//

void brain_duck_down (edict_t *self)
{
	if (self->monsterinfo.aiflags & AI_DUCKED)
		return;
	self->monsterinfo.aiflags |= AI_DUCKED;
	self->maxs[2] -= 32;
	self->takedamage = DAMAGE_YES;
	gi.linkentity (self);
}

void brain_duck_hold (edict_t *self)
{
	if (level.time >= self->monsterinfo.pausetime)
		self->monsterinfo.aiflags &= ~AI_HOLD_FRAME;
	else
		self->monsterinfo.aiflags |= AI_HOLD_FRAME;
}

void brain_duck_up (edict_t *self)
{
	self->monsterinfo.aiflags &= ~AI_DUCKED;
	self->maxs[2] += 32;
	self->takedamage = DAMAGE_AIM;
	gi.linkentity (self);
}

mframe_t brain_frames_duck [] =
{
	ai_move,	0,	NULL,
	ai_move,	-2,	brain_duck_down,
	ai_move,	17,	brain_duck_hold,
	ai_move,	-3,	NULL,
	ai_move,	-1,	brain_duck_up,
	ai_move,	-5,	NULL,
	ai_move,	-6,	NULL,
	ai_move,	-6,	NULL
};
mmove_t brain_move_duck = {BRAIN_FRAME_duck01, BRAIN_FRAME_duck08, brain_frames_duck, brain_run};

void brain_dodge (edict_t *self, edict_t *attacker, float eta)
{
	if (random() > 0.25)
		return;

	if (!self->enemy)
		self->enemy = attacker;

	self->monsterinfo.pausetime = level.time + eta + 0.5;
	self->monsterinfo.currentmove = &brain_move_duck;
}


mframe_t brain_frames_death2 [] =
{
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	9,	NULL,
	ai_move,	0,	NULL
};
mmove_t brain_move_death2 = {BRAIN_FRAME_death201, BRAIN_FRAME_death205, brain_frames_death2, brain_dead};

mframe_t brain_frames_death1 [] =
{
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	-2,	NULL,
	ai_move,	9,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL
};
mmove_t brain_move_death1 = {BRAIN_FRAME_death101, BRAIN_FRAME_death118, brain_frames_death1, brain_dead};


//
// MELEE
//

void brain_swing_right (edict_t *self)
{
	gi.sound (self, CHAN_BODY, brain_sound_melee1, 1, ATTN_NORM, 0);
}

void brain_hit_right (edict_t *self)
{
	vec3_t	aim;

	VectorSet (aim, MELEE_DISTANCE, self->maxs[0], 8);
	if (fire_hit (self, aim, (15 + (rand() %5)), 40))
		gi.sound (self, CHAN_WEAPON, brain_sound_melee3, 1, ATTN_NORM, 0);
}

void brain_swing_left (edict_t *self)
{
	gi.sound (self, CHAN_BODY, brain_sound_melee2, 1, ATTN_NORM, 0);
}

void brain_hit_left (edict_t *self)
{
	vec3_t	aim;

	VectorSet (aim, MELEE_DISTANCE, self->mins[0], 8);
	if (fire_hit (self, aim, (15 + (rand() %5)), 40))
		gi.sound (self, CHAN_WEAPON, brain_sound_melee3, 1, ATTN_NORM, 0);
}

mframe_t brain_frames_attack1 [] =
{
	ai_charge,	8,	NULL,
	ai_charge,	3,	NULL,
	ai_charge,	5,	NULL,
	ai_charge,	0,	NULL,
	ai_charge,	-3,	brain_swing_right,
	ai_charge,	0,	NULL,
	ai_charge,	-5,	NULL,
	ai_charge,	-7,	brain_hit_right,
	ai_charge,	0,	NULL,
	ai_charge,	6,	brain_swing_left,
	ai_charge,	1,	NULL,
	ai_charge,	2,	brain_hit_left,
	ai_charge,	-3,	NULL,
	ai_charge,	6,	NULL,
	ai_charge,	-1,	NULL,
	ai_charge,	-3,	NULL,
	ai_charge,	2,	NULL,
	ai_charge,	-11,NULL
};
mmove_t brain_move_attack1 = {BRAIN_FRAME_attak101, BRAIN_FRAME_attak118, brain_frames_attack1, brain_run};

void brain_chest_open (edict_t *self)
{
	self->spawnflags &= ~65536;
	self->monsterinfo.power_armor_type = POWER_ARMOR_NONE;
	gi.sound (self, CHAN_BODY, brain_sound_chest_open, 1, ATTN_NORM, 0);
}

void brain_tentacle_attack (edict_t *self)
{
	vec3_t	aim;

	VectorSet (aim, MELEE_DISTANCE, 0, 8);
	if (fire_hit (self, aim, (10 + (rand() %5)), -600) && skill->value > 0)
		self->spawnflags |= 65536;
	gi.sound (self, CHAN_WEAPON, brain_sound_tentacles_retract, 1, ATTN_NORM, 0);
}

void brain_chest_closed (edict_t *self)
{
	self->monsterinfo.power_armor_type = POWER_ARMOR_SCREEN;
	if (self->spawnflags & 65536)
	{
		self->spawnflags &= ~65536;
		self->monsterinfo.currentmove = &brain_move_attack1;
	}
}

mframe_t brain_frames_attack2 [] =
{
	ai_charge,	5,	NULL,
	ai_charge,	-4,	NULL,
	ai_charge,	-4,	NULL,
	ai_charge,	-3,	NULL,
	ai_charge,	0,	brain_chest_open,
	ai_charge,	0,	NULL,
	ai_charge,	13,	brain_tentacle_attack,
	ai_charge,	0,	NULL,
	ai_charge,	2,	NULL,
	ai_charge,	0,	NULL,
	ai_charge,	-9,	brain_chest_closed,
	ai_charge,	0,	NULL,
	ai_charge,	4,	NULL,
	ai_charge,	3,	NULL,
	ai_charge,	2,	NULL,
	ai_charge,	-3,	NULL,
	ai_charge,	-6,	NULL
};
mmove_t brain_move_attack2 = {BRAIN_FRAME_attak201, BRAIN_FRAME_attak217, brain_frames_attack2, brain_run};

void brain_melee(edict_t *self)
{
	if (random() <= 0.5)
		self->monsterinfo.currentmove = &brain_move_attack1;
	else
		self->monsterinfo.currentmove = &brain_move_attack2;
}


//
// RUN
//

mframe_t brain_frames_run [] =
{
	ai_run,	9,	NULL,
	ai_run,	2,	NULL,
	ai_run,	3,	NULL,
	ai_run,	3,	NULL,
	ai_run,	1,	NULL,
	ai_run,	0,	NULL,
	ai_run,	0,	NULL,
	ai_run,	10,	NULL,
	ai_run,	-4,	NULL,
	ai_run,	-1,	NULL,
	ai_run,	2,	NULL
};
mmove_t brain_move_run = {BRAIN_FRAME_walk101, BRAIN_FRAME_walk111, brain_frames_run, NULL};

void brain_run (edict_t *self)
{
	self->monsterinfo.power_armor_type = POWER_ARMOR_SCREEN;
	if (self->monsterinfo.aiflags & AI_STAND_GROUND)
		self->monsterinfo.currentmove = &brain_move_stand;
	else
		self->monsterinfo.currentmove = &brain_move_run;
}


void brain_pain (edict_t *self, edict_t *other, float kick, int damage)
{
	float	r;

	if (self->health < (self->max_health / 2))
		self->s.skinnum = 1;

	if (level.time < self->pain_debounce_time)
		return;

	self->pain_debounce_time = level.time + 3;
	if (skill->value == 3)
		return;		// no pain anims in nightmare

	r = random();
	if (r < 0.33)
	{
		gi.sound (self, CHAN_VOICE, brain_sound_pain1, 1, ATTN_NORM, 0);
		self->monsterinfo.currentmove = &brain_move_pain1;
	}
	else if (r < 0.66)
	{
		gi.sound (self, CHAN_VOICE, brain_sound_pain2, 1, ATTN_NORM, 0);
		self->monsterinfo.currentmove = &brain_move_pain2;
	}
	else
	{
		gi.sound (self, CHAN_VOICE, brain_sound_pain1, 1, ATTN_NORM, 0);
		self->monsterinfo.currentmove = &brain_move_pain3;
	}
}

void brain_dead (edict_t *self)
{
	VectorSet (self->mins, -16, -16, -24);
	VectorSet (self->maxs, 16, 16, -8);
	self->movetype = MOVETYPE_TOSS;
	self->svflags |= SVF_DEADMONSTER;
	self->nextthink = 0;
	gi.linkentity (self);

	M_FlyCheck (self);

	monster_reborn(self);
}



void brain_die (edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point)
{
	int		n;

	self->s.effects = 0;
	self->monsterinfo.power_armor_type = POWER_ARMOR_NONE;

// check for gib
	if (self->health <= self->gib_health)
	{
		gi.sound (self, CHAN_VOICE, gi.soundindex ("misc/udeath.wav"), 1, ATTN_NORM, 0);
		if(net_compatibility->value || !net_fixoverflow->value)
		{
			for (n= 0; n < 2; n++)
				ThrowGib (self, "models/objects/gibs/bone/tris.md2", damage, GIB_ORGANIC);
			for (n= 0; n < 4; n++)
				ThrowGib (self, "models/objects/gibs/sm_meat/tris.md2", damage, GIB_ORGANIC);
			ThrowHead (self, "models/objects/gibs/head2/tris.md2", damage, GIB_ORGANIC);
			self->deadflag = DEAD_DEAD;
		}
		else
		{
			NetThrowMonsterGibs (self, GIBS_BRAIN, damage);
			G_FreeEdict (self);
		}
		return;
	}

	if (self->deadflag == DEAD_DEAD)
		return;

// regular death
	gi.sound (self, CHAN_VOICE, brain_sound_death, 1, ATTN_NORM, 0);
	self->deadflag = DEAD_DEAD;
	self->takedamage = DAMAGE_YES;
	if (random() <= 0.5)
		self->monsterinfo.currentmove = &brain_move_death1;
	else
		self->monsterinfo.currentmove = &brain_move_death2;
}

/*QUAKED monster_brain (1 .5 0) (-16 -16 -24) (16 16 32) Ambush Trigger_Spawn Sight
*/
void SP_monster_brain (edict_t *self)
{
	if (deathmatch->value)
	{
		G_FreeEdict (self);
		return;
	}

	brain_sound_chest_open = gi.soundindex ("brain/brnatck1.wav");
	brain_sound_tentacles_extend = gi.soundindex ("brain/brnatck2.wav");
	brain_sound_tentacles_retract = gi.soundindex ("brain/brnatck3.wav");
	brain_sound_death = gi.soundindex ("brain/brndeth1.wav");
	brain_sound_idle1 = gi.soundindex ("brain/brnidle1.wav");
	brain_sound_idle2 = gi.soundindex ("brain/brnidle2.wav");
	brain_sound_idle3 = gi.soundindex ("brain/brnlens1.wav");
	brain_sound_pain1 = gi.soundindex ("brain/brnpain1.wav");
	brain_sound_pain2 = gi.soundindex ("brain/brnpain2.wav");
	brain_sound_sight = gi.soundindex ("brain/brnsght1.wav");
	brain_sound_search = gi.soundindex ("brain/brnsrch1.wav");
	brain_sound_melee1 = gi.soundindex ("brain/melee1.wav");
	brain_sound_melee2 = gi.soundindex ("brain/melee2.wav");
	brain_sound_melee3 = gi.soundindex ("brain/melee3.wav");

	self->movetype = MOVETYPE_STEP;
	self->solid = SOLID_BBOX;
	self->s.modelindex = gi.modelindex ("models/monsters/brain/tris.md2");
	VectorSet (self->mins, -16, -16, -24);
	VectorSet (self->maxs, 16, 16, 32);

	self->health = 300;
	self->gib_health = -150;
	self->mass = 400;

	self->pain = brain_pain;
	self->die = brain_die;

	self->monsterinfo.stand = brain_stand;
	self->monsterinfo.walk = brain_walk;
	self->monsterinfo.run = brain_run;
	self->monsterinfo.dodge = brain_dodge;
//	self->monsterinfo.attack = brain_attack;
	self->monsterinfo.melee = brain_melee;
	self->monsterinfo.sight = brain_sight;
	self->monsterinfo.search = brain_search;
	self->monsterinfo.idle = brain_idle;

	self->monsterinfo.power_armor_type = POWER_ARMOR_SCREEN;
	self->monsterinfo.power_armor_power = 100;

	gi.linkentity (self);

	self->monsterinfo.currentmove = &brain_move_stand;
	self->monsterinfo.scale = BRAIN_MODEL_SCALE;

	walkmonster_start (self, false);
}

/*
==============================================================================

end of brain

==============================================================================
*/


/*
==============================================================================

hover

==============================================================================
*/

void hover_sight (edict_t *self, edict_t *other)
{
	gi.sound (self, CHAN_VOICE, hover_sound_sight, 1, ATTN_NORM, 0);
}

void hover_search (edict_t *self)
{
	if (random() < 0.5)
		gi.sound (self, CHAN_VOICE, hover_sound_search1, 1, ATTN_NORM, 0);
	else
		gi.sound (self, CHAN_VOICE, hover_sound_search2, 1, ATTN_NORM, 0);
}


void hover_run (edict_t *self);
void hover_stand (edict_t *self);
void hover_dead (edict_t *self);
void hover_attack (edict_t *self);
void hover_reattack (edict_t *self);
void hover_fire_blaster (edict_t *self);
void hover_die (edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point);

mframe_t hover_frames_stand [] =
{
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL
};
mmove_t	hover_move_stand = {HOVER_FRAME_stand01, HOVER_FRAME_stand30, hover_frames_stand, NULL};

mframe_t hover_frames_stop1 [] =
{
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL
};
mmove_t hover_move_stop1 = {HOVER_FRAME_stop101, HOVER_FRAME_stop109, hover_frames_stop1, NULL};

mframe_t hover_frames_stop2 [] =
{
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL
};
mmove_t hover_move_stop2 = {HOVER_FRAME_stop201, HOVER_FRAME_stop208, hover_frames_stop2, NULL};

mframe_t hover_frames_takeoff [] =
{
	ai_move,	0,	NULL,
	ai_move,	-2,	NULL,
	ai_move,	5,	NULL,
	ai_move,	-1,	NULL,
	ai_move,	1,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	-1,	NULL,
	ai_move,	-1,	NULL,
	ai_move,	-1,	NULL,
	ai_move,	0,	NULL,
	ai_move,	2,	NULL,
	ai_move,	2,	NULL,
	ai_move,	1,	NULL,
	ai_move,	1,	NULL,
	ai_move,	-6,	NULL,
	ai_move,	-9,	NULL,
	ai_move,	1,	NULL,
	ai_move,	0,	NULL,
	ai_move,	2,	NULL,
	ai_move,	2,	NULL,
	ai_move,	1,	NULL,
	ai_move,	1,	NULL,
	ai_move,	1,	NULL,
	ai_move,	2,	NULL,
	ai_move,	0,	NULL,
	ai_move,	2,	NULL,
	ai_move,	3,	NULL,
	ai_move,	2,	NULL,
	ai_move,	0,	NULL
};
mmove_t hover_move_takeoff = {HOVER_FRAME_takeof01, HOVER_FRAME_takeof30, hover_frames_takeoff, NULL};

mframe_t hover_frames_pain3 [] =
{
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL
};
mmove_t hover_move_pain3 = {HOVER_FRAME_pain301, HOVER_FRAME_pain309, hover_frames_pain3, hover_run};

mframe_t hover_frames_pain2 [] =
{
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL
};
mmove_t hover_move_pain2 = {HOVER_FRAME_pain201, HOVER_FRAME_pain212, hover_frames_pain2, hover_run};

mframe_t hover_frames_pain1 [] =
{
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	2,	NULL,
	ai_move,	-8,	NULL,
	ai_move,	-4,	NULL,
	ai_move,	-6,	NULL,
	ai_move,	-4,	NULL,
	ai_move,	-3,	NULL,
	ai_move,	1,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	3,	NULL,
	ai_move,	1,	NULL,
	ai_move,	0,	NULL,
	ai_move,	2,	NULL,
	ai_move,	3,	NULL,
	ai_move,	2,	NULL,
	ai_move,	7,	NULL,
	ai_move,	1,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	2,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	5,	NULL,
	ai_move,	3,	NULL,
	ai_move,	4,	NULL
};
mmove_t hover_move_pain1 = {HOVER_FRAME_pain101, HOVER_FRAME_pain128, hover_frames_pain1, hover_run};

mframe_t hover_frames_land [] =
{
	ai_move,	0,	NULL
};
mmove_t hover_move_land = {HOVER_FRAME_land01, HOVER_FRAME_land01, hover_frames_land, NULL};

mframe_t hover_frames_forward [] =
{
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL
};
mmove_t hover_move_forward = {HOVER_FRAME_forwrd01, HOVER_FRAME_forwrd35, hover_frames_forward, NULL};

mframe_t hover_frames_walk [] =
{
	ai_walk,	4,	NULL,
	ai_walk,	4,	NULL,
	ai_walk,	4,	NULL,
	ai_walk,	4,	NULL,
	ai_walk,	4,	NULL,
	ai_walk,	4,	NULL,
	ai_walk,	4,	NULL,
	ai_walk,	4,	NULL,
	ai_walk,	4,	NULL,
	ai_walk,	4,	NULL,
	ai_walk,	4,	NULL,
	ai_walk,	4,	NULL,
	ai_walk,	4,	NULL,
	ai_walk,	4,	NULL,
	ai_walk,	4,	NULL,
	ai_walk,	4,	NULL,
	ai_walk,	4,	NULL,
	ai_walk,	4,	NULL,
	ai_walk,	4,	NULL,
	ai_walk,	4,	NULL,
	ai_walk,	4,	NULL,
	ai_walk,	4,	NULL,
	ai_walk,	4,	NULL,
	ai_walk,	4,	NULL,
	ai_walk,	4,	NULL,
	ai_walk,	4,	NULL,
	ai_walk,	4,	NULL,
	ai_walk,	4,	NULL,
	ai_walk,	4,	NULL,
	ai_walk,	4,	NULL,
	ai_walk,	4,	NULL,
	ai_walk,	4,	NULL,
	ai_walk,	4,	NULL,
	ai_walk,	4,	NULL,
	ai_walk,	4,	NULL
};
mmove_t hover_move_walk = {HOVER_FRAME_forwrd01, HOVER_FRAME_forwrd35, hover_frames_walk, NULL};

mframe_t hover_frames_run [] =
{
	ai_run,	10,	NULL,
	ai_run,	10,	NULL,
	ai_run,	10,	NULL,
	ai_run,	10,	NULL,
	ai_run,	10,	NULL,
	ai_run,	10,	NULL,
	ai_run,	10,	NULL,
	ai_run,	10,	NULL,
	ai_run,	10,	NULL,
	ai_run,	10,	NULL,
	ai_run,	10,	NULL,
	ai_run,	10,	NULL,
	ai_run,	10,	NULL,
	ai_run,	10,	NULL,
	ai_run,	10,	NULL,
	ai_run,	10,	NULL,
	ai_run,	10,	NULL,
	ai_run,	10,	NULL,
	ai_run,	10,	NULL,
	ai_run,	10,	NULL,
	ai_run,	10,	NULL,
	ai_run,	10,	NULL,
	ai_run,	10,	NULL,
	ai_run,	10,	NULL,
	ai_run,	10,	NULL,
	ai_run,	10,	NULL,
	ai_run,	10,	NULL,
	ai_run,	10,	NULL,
	ai_run,	10,	NULL,
	ai_run,	10,	NULL,
	ai_run,	10,	NULL,
	ai_run,	10,	NULL,
	ai_run,	10,	NULL,
	ai_run,	10,	NULL,
	ai_run,	10,	NULL
};
mmove_t hover_move_run = {HOVER_FRAME_forwrd01, HOVER_FRAME_forwrd35, hover_frames_run, NULL};

mframe_t hover_frames_death1 [] =
{
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	-10,NULL,
	ai_move,	3,	NULL,
	ai_move,	5,	NULL,
	ai_move,	4,	NULL,
	ai_move,	7,	NULL
};
mmove_t hover_move_death1 = {HOVER_FRAME_death101, HOVER_FRAME_death111, hover_frames_death1, hover_dead};

mframe_t hover_frames_backward [] =
{
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL
};
mmove_t hover_move_backward = {HOVER_FRAME_backwd01, HOVER_FRAME_backwd24, hover_frames_backward, NULL};

mframe_t hover_frames_start_attack [] =
{
	ai_charge,	1,	NULL,
	ai_charge,	1,	NULL,
	ai_charge,	1,	NULL
};
mmove_t hover_move_start_attack = {HOVER_FRAME_attak101, HOVER_FRAME_attak103, hover_frames_start_attack, hover_attack};

mframe_t hover_frames_attack1 [] =
{
	ai_charge,	-10,	hover_fire_blaster,
	ai_charge,	-10,	hover_fire_blaster,
	ai_charge,	0,		hover_reattack,
};
mmove_t hover_move_attack1 = {HOVER_FRAME_attak104, HOVER_FRAME_attak106, hover_frames_attack1, NULL};


mframe_t hover_frames_end_attack [] =
{
	ai_charge,	1,	NULL,
	ai_charge,	1,	NULL
};
mmove_t hover_move_end_attack = {HOVER_FRAME_attak107, HOVER_FRAME_attak108, hover_frames_end_attack, hover_run};

void hover_reattack (edict_t *self)
{
	if (self->enemy->health > 0 )
		if (visible (self, self->enemy) )
			if (random() <= 0.6)
			{
				self->monsterinfo.currentmove = &hover_move_attack1;
				return;
			}
	self->monsterinfo.currentmove = &hover_move_end_attack;
}


void hover_fire_blaster (edict_t *self)
{
	vec3_t	start;
	vec3_t	forward, right;
	vec3_t	end;
	vec3_t	dir;
	int		effect;

	if (self->s.frame == HOVER_FRAME_attak104)
		effect = EF_HYPERBLASTER;
	else
		effect = 0;

	AngleVectors (self->s.angles, forward, right, NULL);
	G_ProjectSource (self->s.origin, &monster_flash_offset[MZ2_HOVER_BLASTER_1*3], forward, right, start);

	VectorCopy (self->enemy->s.origin, end);
	end[2] += self->enemy->viewheight;
	VectorSubtract (end, start, dir);

	monster_fire_blaster (self, start, dir, 1, 1000, MZ2_HOVER_BLASTER_1, effect);
}


void hover_stand (edict_t *self)
{
		self->monsterinfo.currentmove = &hover_move_stand;
}

void hover_run (edict_t *self)
{
	if (self->monsterinfo.aiflags & AI_STAND_GROUND)
		self->monsterinfo.currentmove = &hover_move_stand;
	else
		self->monsterinfo.currentmove = &hover_move_run;
}

void hover_walk (edict_t *self)
{
	self->monsterinfo.currentmove = &hover_move_walk;
}

void hover_start_attack (edict_t *self)
{
	self->monsterinfo.currentmove = &hover_move_start_attack;
}

void hover_attack(edict_t *self)
{
	self->monsterinfo.currentmove = &hover_move_attack1;
}


void hover_pain (edict_t *self, edict_t *other, float kick, int damage)
{
	if (self->health < (self->max_health / 2))
		self->s.skinnum = 1;

	if (level.time < self->pain_debounce_time)
		return;

	self->pain_debounce_time = level.time + 3;

	if (skill->value == 3)
		return;		// no pain anims in nightmare

	if (damage <= 25)
	{
		if (random() < 0.5)
		{
			gi.sound (self, CHAN_VOICE, hover_sound_pain1, 1, ATTN_NORM, 0);
			self->monsterinfo.currentmove = &hover_move_pain3;
		}
		else
		{
			gi.sound (self, CHAN_VOICE, hover_sound_pain2, 1, ATTN_NORM, 0);
			self->monsterinfo.currentmove = &hover_move_pain2;
		}
	}
	else
	{
		gi.sound (self, CHAN_VOICE, hover_sound_pain1, 1, ATTN_NORM, 0);
		self->monsterinfo.currentmove = &hover_move_pain1;
	}
}

void hover_deadthink (edict_t *self)
{
	if (!self->groundentity && level.time < self->timestamp)
	{
		self->nextthink = level.time + FRAMETIME;
		return;
	}

//// Berserker: пусть hover разбрасывает gibs при взрыве
	if(net_compatibility->value || !net_fixoverflow->value)
	{
		int	n;
		for (n= 0; n < 2; n++)
			ThrowGib (self, "models/objects/gibs/bone/tris.md2", 1000, GIB_ORGANIC);
		for (n= 0; n < 2; n++)
			ThrowGib (self, "models/objects/gibs/sm_meat/tris.md2", 1000, GIB_ORGANIC);
	}
	else
	{
		self->s.origin[2] += 16;	// приподымем немного, чтоб джибсы не пропадали под полом
		NetThrowMonsterGibs (self, GIBS_HOVER, 1000);
		self->s.origin[2] -= 16;	// вернём взад
	}
////

	BecomeExplosion1(self);
}

void hover_dead (edict_t *self)
{
	VectorSet (self->mins, -16, -16, -24);
	VectorSet (self->maxs, 16, 16, -8);
	self->movetype = MOVETYPE_TOSS;
	self->think = hover_deadthink;
	self->nextthink = level.time + FRAMETIME;
	self->timestamp = level.time + 15;
	gi.linkentity (self);

///	monster_reborn(self);
}

void hover_die (edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point)
{
	int		n;

	self->s.effects &= ~EF_FLASHLIGHT;		// заберем у монстра фонарик, ибо он разбился ))))

// check for gib
	if (self->health <= self->gib_health)
	{
		gi.sound (self, CHAN_VOICE, gi.soundindex ("misc/udeath.wav"), 1, ATTN_NORM, 0);
		if(net_compatibility->value || !net_fixoverflow->value)
		{
			for (n= 0; n < 2; n++)
				ThrowGib (self, "models/objects/gibs/bone/tris.md2", damage, GIB_ORGANIC);
			for (n= 0; n < 2; n++)
				ThrowGib (self, "models/objects/gibs/sm_meat/tris.md2", damage, GIB_ORGANIC);
			ThrowHead (self, "models/objects/gibs/sm_meat/tris.md2", damage, GIB_ORGANIC);
			self->deadflag = DEAD_DEAD;
		}
		else
		{
			NetThrowMonsterGibs (self, GIBS_HOVER, damage);
			G_FreeEdict (self);
		}
		return;
	}

	if (self->deadflag == DEAD_DEAD)
		return;

// regular death
	if (random() < 0.5)
		gi.sound (self, CHAN_VOICE, hover_sound_death1, 1, ATTN_NORM, 0);
	else
		gi.sound (self, CHAN_VOICE, hover_sound_death2, 1, ATTN_NORM, 0);
	self->deadflag = DEAD_DEAD;
	self->takedamage = DAMAGE_YES;
	self->monsterinfo.currentmove = &hover_move_death1;
}

/*QUAKED monster_hover (1 .5 0) (-16 -16 -24) (16 16 32) Ambush Trigger_Spawn Sight
*/
void SP_monster_hover (edict_t *self)
{
	if (deathmatch->value)
	{
		G_FreeEdict (self);
		return;
	}

	hover_sound_pain1 = gi.soundindex ("hover/hovpain1.wav");
	hover_sound_pain2 = gi.soundindex ("hover/hovpain2.wav");
	hover_sound_death1 = gi.soundindex ("hover/hovdeth1.wav");
	hover_sound_death2 = gi.soundindex ("hover/hovdeth2.wav");
	hover_sound_sight = gi.soundindex ("hover/hovsght1.wav");
	hover_sound_search1 = gi.soundindex ("hover/hovsrch1.wav");
	hover_sound_search2 = gi.soundindex ("hover/hovsrch2.wav");

	gi.soundindex ("hover/hovatck1.wav");

	self->s.sound = gi.soundindex ("hover/hovidle1.wav");

	if(!(net_compatibility->value))
		if ((int)dmflags->value & DF_FLASHLIGHT)	// если разрешен фонарик:
			self->s.effects = EF_FLASHLIGHT;		// дадим монстру - фонарик

	self->movetype = MOVETYPE_STEP;
	self->solid = SOLID_BBOX;
	self->s.modelindex = gi.modelindex("models/monsters/hover/tris.md2");
	VectorSet (self->mins, -24, -24, -24);
	VectorSet (self->maxs, 24, 24, 32);

	self->health = 240;
	self->gib_health = -100;
	self->mass = 150;

	self->pain = hover_pain;
	self->die = hover_die;

	self->monsterinfo.stand = hover_stand;
	self->monsterinfo.walk = hover_walk;
	self->monsterinfo.run = hover_run;
//	self->monsterinfo.dodge = hover_dodge;
	self->monsterinfo.attack = hover_start_attack;
	self->monsterinfo.sight = hover_sight;
	self->monsterinfo.search = hover_search;

	gi.linkentity (self);

	self->monsterinfo.currentmove = &hover_move_stand;
	self->monsterinfo.scale = HOVER_MODEL_SCALE;

	flymonster_start (self, false);
}

/*
==============================================================================

end of hover

==============================================================================
*/


/*
==============================================================================

mutant

==============================================================================
*/

//
// SOUNDS
//

void mutant_step (edict_t *self)
{
	int		n;
	n = (rand() + 1) % 3;
	if (n == 0)
		gi.sound (self, CHAN_VOICE, mutant_sound_step1, 1, ATTN_NORM, 0);
	else if (n == 1)
		gi.sound (self, CHAN_VOICE, mutant_sound_step2, 1, ATTN_NORM, 0);
	else
		gi.sound (self, CHAN_VOICE, mutant_sound_step3, 1, ATTN_NORM, 0);
}

void mutant_sight (edict_t *self, edict_t *other)
{
	gi.sound (self, CHAN_VOICE, mutant_sound_sight, 1, ATTN_NORM, 0);
}

void mutant_search (edict_t *self)
{
	gi.sound (self, CHAN_VOICE, mutant_sound_search, 1, ATTN_NORM, 0);
}

void mutant_swing (edict_t *self)
{
	gi.sound (self, CHAN_VOICE, mutant_sound_swing, 1, ATTN_NORM, 0);
}


//
// STAND
//

mframe_t mutant_frames_stand [] =
{
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,		// 10

	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,		// 20

	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,		// 30

	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,		// 40

	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,		// 50

	ai_stand, 0, NULL
};
mmove_t mutant_move_stand = {MUTANT_FRAME_stand101, MUTANT_FRAME_stand151, mutant_frames_stand, NULL};

void mutant_stand (edict_t *self)
{
	self->monsterinfo.currentmove = &mutant_move_stand;
}


//
// IDLE
//

void mutant_idle_loop (edict_t *self)
{
	if (random() < 0.75)
		self->monsterinfo.nextframe = MUTANT_FRAME_stand155;
}

mframe_t mutant_frames_idle [] =
{
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,					// scratch loop start
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, mutant_idle_loop,		// scratch loop end
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL
};
mmove_t mutant_move_idle = {MUTANT_FRAME_stand152, MUTANT_FRAME_stand164, mutant_frames_idle, mutant_stand};

void mutant_idle (edict_t *self)
{
	self->monsterinfo.currentmove = &mutant_move_idle;
	gi.sound (self, CHAN_VOICE, mutant_sound_idle, 1, ATTN_IDLE, 0);
}


//
// WALK
//

void mutant_walk (edict_t *self);

mframe_t mutant_frames_walk [] =
{
	ai_walk,	3,		NULL,
	ai_walk,	1,		NULL,
	ai_walk,	5,		NULL,
	ai_walk,	10,		NULL,
	ai_walk,	13,		NULL,
	ai_walk,	10,		NULL,
	ai_walk,	0,		NULL,
	ai_walk,	5,		NULL,
	ai_walk,	6,		NULL,
	ai_walk,	16,		NULL,
	ai_walk,	15,		NULL,
	ai_walk,	6,		NULL
};
mmove_t mutant_move_walk = {MUTANT_FRAME_walk05, MUTANT_FRAME_walk16, mutant_frames_walk, NULL};

void mutant_walk_loop (edict_t *self)
{
	self->monsterinfo.currentmove = &mutant_move_walk;
}

mframe_t mutant_frames_start_walk [] =
{
	ai_walk,	5,		NULL,
	ai_walk,	5,		NULL,
	ai_walk,	-2,		NULL,
	ai_walk,	1,		NULL
};
mmove_t mutant_move_start_walk = {MUTANT_FRAME_walk01, MUTANT_FRAME_walk04, mutant_frames_start_walk, mutant_walk_loop};

void mutant_walk (edict_t *self)
{
	self->monsterinfo.currentmove = &mutant_move_start_walk;
}


//
// RUN
//

mframe_t mutant_frames_run [] =
{
	ai_run,	40,		NULL,
	ai_run,	40,		mutant_step,
	ai_run,	24,		NULL,
	ai_run,	5,		mutant_step,
	ai_run,	17,		NULL,
	ai_run,	10,		NULL
};
mmove_t mutant_move_run = {MUTANT_FRAME_run03, MUTANT_FRAME_run08, mutant_frames_run, NULL};

void mutant_run (edict_t *self)
{
	if (self->monsterinfo.aiflags & AI_STAND_GROUND)
		self->monsterinfo.currentmove = &mutant_move_stand;
	else
		self->monsterinfo.currentmove = &mutant_move_run;
}


//
// MELEE
//

void mutant_hit_left (edict_t *self)
{
	vec3_t	aim;

	VectorSet (aim, MELEE_DISTANCE, self->mins[0], 8);
	if (fire_hit (self, aim, (10 + (rand() %5)), 100))
		gi.sound (self, CHAN_WEAPON, mutant_sound_hit, 1, ATTN_NORM, 0);
	else
		gi.sound (self, CHAN_WEAPON, mutant_sound_swing, 1, ATTN_NORM, 0);
}

void mutant_hit_right (edict_t *self)
{
	vec3_t	aim;

	VectorSet (aim, MELEE_DISTANCE, self->maxs[0], 8);
	if (fire_hit (self, aim, (10 + (rand() %5)), 100))
		gi.sound (self, CHAN_WEAPON, mutant_sound_hit2, 1, ATTN_NORM, 0);
	else
		gi.sound (self, CHAN_WEAPON, mutant_sound_swing, 1, ATTN_NORM, 0);
}

void mutant_check_refire (edict_t *self)
{
	if (!self->enemy || !self->enemy->inuse || self->enemy->health <= 0)
		return;

	if ( ((skill->value == 3) && (random() < 0.5)) || (range(self, self->enemy) == RANGE_MELEE) )
		self->monsterinfo.nextframe = MUTANT_FRAME_attack09;
}

mframe_t mutant_frames_attack [] =
{
	ai_charge,	0,	NULL,
	ai_charge,	0,	NULL,
	ai_charge,	0,	mutant_hit_left,
	ai_charge,	0,	NULL,
	ai_charge,	0,	NULL,
	ai_charge,	0,	mutant_hit_right,
	ai_charge,	0,	mutant_check_refire
};
mmove_t mutant_move_attack = {MUTANT_FRAME_attack09, MUTANT_FRAME_attack15, mutant_frames_attack, mutant_run};

void mutant_melee (edict_t *self)
{
	self->monsterinfo.currentmove = &mutant_move_attack;
}


//
// ATTACK
//

void mutant_jump_touch (edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf)
{
	if (self->health <= 0)
	{
		self->touch = NULL;
		return;
	}

	if (other->takedamage)
	{
		if (VectorLength(self->velocity) > 400)
		{
			vec3_t	point;
			vec3_t	normal;
			int		damage;

			VectorCopy (self->velocity, normal);
			VectorNormalize(normal);
			VectorMA (self->s.origin, self->maxs[0], normal, point);
			damage = 40 + 10 * random();
			T_Damage (other, self, self, self->velocity, point, normal, damage, damage, 0, MOD_UNKNOWN);
		}
	}

	if (!M_CheckBottom (self))
	{
		if (self->groundentity)
		{
			self->monsterinfo.nextframe = MUTANT_FRAME_attack02;
			self->touch = NULL;
		}
		return;
	}

	self->touch = NULL;
}

void mutant_jump_takeoff (edict_t *self)
{
	vec3_t	forward;

	gi.sound (self, CHAN_VOICE, mutant_sound_sight, 1, ATTN_NORM, 0);
	AngleVectors (self->s.angles, forward, NULL, NULL);
	self->s.origin[2] += 1;
	VectorScale (forward, 600, self->velocity);
	self->velocity[2] = 250;
	self->groundentity = NULL;
	self->monsterinfo.aiflags |= AI_DUCKED;
	self->monsterinfo.attack_finished = level.time + 3;
	self->touch = mutant_jump_touch;
}

void mutant_check_landing (edict_t *self)
{
	if (self->groundentity)
	{
		gi.sound (self, CHAN_WEAPON, mutant_sound_thud, 1, ATTN_NORM, 0);
		self->monsterinfo.attack_finished = 0;
		self->monsterinfo.aiflags &= ~AI_DUCKED;
		return;
	}

	if (level.time > self->monsterinfo.attack_finished)
		self->monsterinfo.nextframe = MUTANT_FRAME_attack02;
	else
		self->monsterinfo.nextframe = MUTANT_FRAME_attack05;
}

mframe_t mutant_frames_jump [] =
{
	ai_charge,	 0,	NULL,
	ai_charge,	17,	NULL,
	ai_charge,	15,	mutant_jump_takeoff,
	ai_charge,	15,	NULL,
	ai_charge,	15,	mutant_check_landing,
	ai_charge,	 0,	NULL,
	ai_charge,	 3,	NULL,
	ai_charge,	 0,	NULL
};
mmove_t mutant_move_jump = {MUTANT_FRAME_attack01, MUTANT_FRAME_attack08, mutant_frames_jump, mutant_run};

void mutant_jump (edict_t *self)
{
	self->monsterinfo.currentmove = &mutant_move_jump;
}


//
// CHECKATTACK
//

bool mutant_check_melee (edict_t *self)
{
	if (range (self, self->enemy) == RANGE_MELEE)
		return true;
	return false;
}

bool mutant_check_jump (edict_t *self)
{
	vec3_t	v;
	float	distance;

	if (self->absmin[2] > (self->enemy->absmin[2] + 0.75 * self->enemy->size[2]))
		return false;

	if (self->absmax[2] < (self->enemy->absmin[2] + 0.25 * self->enemy->size[2]))
		return false;

	v[0] = self->s.origin[0] - self->enemy->s.origin[0];
	v[1] = self->s.origin[1] - self->enemy->s.origin[1];
	v[2] = 0;
	distance = VectorLength(v);

	if (distance < 100)
		return false;
	if (distance > 100)
	{
		if (random() < 0.9)
			return false;
	}

	return true;
}

bool mutant_checkattack (edict_t *self)
{
	if (!self->enemy || self->enemy->health <= 0)
		return false;

	if (mutant_check_melee(self))
	{
		self->monsterinfo.attack_state = AS_MELEE;
		return true;
	}

	if (mutant_check_jump(self))
	{
		self->monsterinfo.attack_state = AS_MISSILE;
		// FIXME play a jump sound here
		return true;
	}

	return false;
}


//
// PAIN
//

mframe_t mutant_frames_pain1 [] =
{
	ai_move,	4,	NULL,
	ai_move,	-3,	NULL,
	ai_move,	-8,	NULL,
	ai_move,	2,	NULL,
	ai_move,	5,	NULL
};
mmove_t mutant_move_pain1 = {MUTANT_FRAME_pain101, MUTANT_FRAME_pain105, mutant_frames_pain1, mutant_run};

mframe_t mutant_frames_pain2 [] =
{
	ai_move,	-24,NULL,
	ai_move,	11,	NULL,
	ai_move,	5,	NULL,
	ai_move,	-2,	NULL,
	ai_move,	6,	NULL,
	ai_move,	4,	NULL
};
mmove_t mutant_move_pain2 = {MUTANT_FRAME_pain201, MUTANT_FRAME_pain206, mutant_frames_pain2, mutant_run};

mframe_t mutant_frames_pain3 [] =
{
	ai_move,	-22,NULL,
	ai_move,	3,	NULL,
	ai_move,	3,	NULL,
	ai_move,	2,	NULL,
	ai_move,	1,	NULL,
	ai_move,	1,	NULL,
	ai_move,	6,	NULL,
	ai_move,	3,	NULL,
	ai_move,	2,	NULL,
	ai_move,	0,	NULL,
	ai_move,	1,	NULL
};
mmove_t mutant_move_pain3 = {MUTANT_FRAME_pain301, MUTANT_FRAME_pain311, mutant_frames_pain3, mutant_run};

void mutant_pain (edict_t *self, edict_t *other, float kick, int damage)
{
	float	r;

	if (self->health < (self->max_health / 2))
		self->s.skinnum = 1;

	if (level.time < self->pain_debounce_time)
		return;

	self->pain_debounce_time = level.time + 3;

	if (skill->value == 3)
		return;		// no pain anims in nightmare

	r = random();
	if (r < 0.33)
	{
		gi.sound (self, CHAN_VOICE, mutant_sound_pain1, 1, ATTN_NORM, 0);
		self->monsterinfo.currentmove = &mutant_move_pain1;
	}
	else if (r < 0.66)
	{
		gi.sound (self, CHAN_VOICE, mutant_sound_pain2, 1, ATTN_NORM, 0);
		self->monsterinfo.currentmove = &mutant_move_pain2;
	}
	else
	{
		gi.sound (self, CHAN_VOICE, mutant_sound_pain1, 1, ATTN_NORM, 0);
		self->monsterinfo.currentmove = &mutant_move_pain3;
	}
}


//
// DEATH
//

void mutant_dead (edict_t *self)
{
	VectorSet (self->mins, -16, -16, -24);
	VectorSet (self->maxs, 16, 16, -8);
	self->movetype = MOVETYPE_TOSS;
	self->svflags |= SVF_DEADMONSTER;
	gi.linkentity (self);

	M_FlyCheck (self);

	monster_reborn(self);
}

mframe_t mutant_frames_death1 [] =
{
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL
};
mmove_t mutant_move_death1 = {MUTANT_FRAME_death101, MUTANT_FRAME_death109, mutant_frames_death1, mutant_dead};

mframe_t mutant_frames_death2 [] =
{
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL
};
mmove_t mutant_move_death2 = {MUTANT_FRAME_death201, MUTANT_FRAME_death210, mutant_frames_death2, mutant_dead};

void mutant_die (edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point)
{
	int		n;

	if (self->health <= self->gib_health)
	{
		gi.sound (self, CHAN_VOICE, gi.soundindex ("misc/udeath.wav"), 1, ATTN_NORM, 0);
		if(net_compatibility->value || !net_fixoverflow->value)
		{
			for (n= 0; n < 2; n++)
				ThrowGib (self, "models/objects/gibs/bone/tris.md2", damage, GIB_ORGANIC);
			for (n= 0; n < 4; n++)
				ThrowGib (self, "models/objects/gibs/sm_meat/tris.md2", damage, GIB_ORGANIC);
			ThrowHead (self, "models/objects/gibs/head2/tris.md2", damage, GIB_ORGANIC);
			self->deadflag = DEAD_DEAD;
		}
		else
		{
			NetThrowMonsterGibs (self, GIBS_MUTANT, damage);
			G_FreeEdict (self);
		}
		return;
	}

	if (self->deadflag == DEAD_DEAD)
		return;

	gi.sound (self, CHAN_VOICE, mutant_sound_death, 1, ATTN_NORM, 0);
	self->deadflag = DEAD_DEAD;
	self->takedamage = DAMAGE_YES;
	self->s.skinnum = 1;

	if (random() < 0.5)
		self->monsterinfo.currentmove = &mutant_move_death1;
	else
		self->monsterinfo.currentmove = &mutant_move_death2;
}


//
// SPAWN
//

/*QUAKED monster_mutant (1 .5 0) (-32 -32 -24) (32 32 32) Ambush Trigger_Spawn Sight
*/
void SP_monster_mutant (edict_t *self)
{
	if (deathmatch->value)
	{
		G_FreeEdict (self);
		return;
	}

	mutant_sound_swing = gi.soundindex ("mutant/mutatck1.wav");
	mutant_sound_hit = gi.soundindex ("mutant/mutatck2.wav");
	mutant_sound_hit2 = gi.soundindex ("mutant/mutatck3.wav");
	mutant_sound_death = gi.soundindex ("mutant/mutdeth1.wav");
	mutant_sound_idle = gi.soundindex ("mutant/mutidle1.wav");
	mutant_sound_pain1 = gi.soundindex ("mutant/mutpain1.wav");
	mutant_sound_pain2 = gi.soundindex ("mutant/mutpain2.wav");
	mutant_sound_sight = gi.soundindex ("mutant/mutsght1.wav");
	mutant_sound_search = gi.soundindex ("mutant/mutsrch1.wav");
	mutant_sound_step1 = gi.soundindex ("mutant/step1.wav");
	mutant_sound_step2 = gi.soundindex ("mutant/step2.wav");
	mutant_sound_step3 = gi.soundindex ("mutant/step3.wav");
	mutant_sound_thud = gi.soundindex ("mutant/thud1.wav");

	self->movetype = MOVETYPE_STEP;
	self->solid = SOLID_BBOX;
	self->s.modelindex = gi.modelindex ("models/monsters/mutant/tris.md2");
	VectorSet (self->mins, -32, -32, -24);
	VectorSet (self->maxs, 32, 32, 48);

	self->health = 300;
	self->gib_health = -120;
	self->mass = 300;

	self->pain = mutant_pain;
	self->die = mutant_die;

	self->monsterinfo.stand = mutant_stand;
	self->monsterinfo.walk = mutant_walk;
	self->monsterinfo.run = mutant_run;
	self->monsterinfo.dodge = NULL;
	self->monsterinfo.attack = mutant_jump;
	self->monsterinfo.melee = mutant_melee;
	self->monsterinfo.sight = mutant_sight;
	self->monsterinfo.search = mutant_search;
	self->monsterinfo.idle = mutant_idle;
	self->monsterinfo.checkattack = mutant_checkattack;

	gi.linkentity (self);

	self->monsterinfo.currentmove = &mutant_move_stand;
	self->monsterinfo.scale = MUTANT_MODEL_SCALE;

	walkmonster_start (self, false);
}

/*
==============================================================================

end of mutant

==============================================================================
*/


/*
==============================================================================

boss2

==============================================================================
*/

void boss2_search (edict_t *self)
{
	if (random() < 0.5)
		gi.sound (self, CHAN_VOICE, boss2_sound_search1, 1, ATTN_NONE, 0);
}

void boss2_run (edict_t *self);
void boss2_stand (edict_t *self);
void boss2_dead (edict_t *self);
void boss2_attack (edict_t *self);
void boss2_attack_mg (edict_t *self);
void boss2_reattack_mg (edict_t *self);
void boss2_die (edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point);

void Boss2Rocket (edict_t *self)
{
	vec3_t	forward, right;
	vec3_t	start;
	vec3_t	dir;
	vec3_t	vec;

	AngleVectors (self->s.angles, forward, right, NULL);

//1
	G_ProjectSource (self->s.origin, &monster_flash_offset[MZ2_BOSS2_ROCKET_1*3], forward, right, start);
	VectorCopy (self->enemy->s.origin, vec);
	vec[2] += self->enemy->viewheight;
	VectorSubtract (vec, start, dir);
	VectorNormalize (dir);
	monster_fire_rocket (self, start, dir, 50, 500, MZ2_BOSS2_ROCKET_1);

//2
	G_ProjectSource (self->s.origin, &monster_flash_offset[MZ2_BOSS2_ROCKET_2*3], forward, right, start);
	VectorCopy (self->enemy->s.origin, vec);
	vec[2] += self->enemy->viewheight;
	VectorSubtract (vec, start, dir);
	VectorNormalize (dir);
	monster_fire_rocket (self, start, dir, 50, 500, MZ2_BOSS2_ROCKET_2);

//3
	G_ProjectSource (self->s.origin, &monster_flash_offset[MZ2_BOSS2_ROCKET_3*3], forward, right, start);
	VectorCopy (self->enemy->s.origin, vec);
	vec[2] += self->enemy->viewheight;
	VectorSubtract (vec, start, dir);
	VectorNormalize (dir);
	monster_fire_rocket (self, start, dir, 50, 500, MZ2_BOSS2_ROCKET_3);

//4
	G_ProjectSource (self->s.origin, &monster_flash_offset[MZ2_BOSS2_ROCKET_4*3], forward, right, start);
	VectorCopy (self->enemy->s.origin, vec);
	vec[2] += self->enemy->viewheight;
	VectorSubtract (vec, start, dir);
	VectorNormalize (dir);
	monster_fire_rocket (self, start, dir, 50, 500, MZ2_BOSS2_ROCKET_4);
}

void boss2_firebullet_right (edict_t *self)
{
	vec3_t	forward, right, target;
	vec3_t	start;

	AngleVectors (self->s.angles, forward, right, NULL);
	G_ProjectSource (self->s.origin, &monster_flash_offset[MZ2_BOSS2_MACHINEGUN_R1*3], forward, right, start);

	VectorMA (self->enemy->s.origin, -0.2, self->enemy->velocity, target);
	target[2] += self->enemy->viewheight;
	VectorSubtract (target, start, forward);
	VectorNormalize (forward);

	monster_fire_bullet (self, start, forward, 6, 4, DEFAULT_BULLET_HSPREAD, DEFAULT_BULLET_VSPREAD, MZ2_BOSS2_MACHINEGUN_R1);
}

void boss2_firebullet_left (edict_t *self)
{
	vec3_t	forward, right, target;
	vec3_t	start;

	AngleVectors (self->s.angles, forward, right, NULL);
	G_ProjectSource (self->s.origin, &monster_flash_offset[MZ2_BOSS2_MACHINEGUN_L1*3], forward, right, start);

	VectorMA (self->enemy->s.origin, -0.2, self->enemy->velocity, target);

	target[2] += self->enemy->viewheight;
	VectorSubtract (target, start, forward);
	VectorNormalize (forward);

	monster_fire_bullet (self, start, forward, 6, 4, DEFAULT_BULLET_HSPREAD, DEFAULT_BULLET_VSPREAD, MZ2_BOSS2_MACHINEGUN_L1);
}

void Boss2MachineGun (edict_t *self)
{
/*	vec3_t	forward, right;
	vec3_t	start;
	vec3_t	dir;
	vec3_t	vec;
	int		flash_number;

	AngleVectors (self->s.angles, forward, right, NULL);

	flash_number = MZ2_BOSS2_MACHINEGUN_1 + (self->s.frame - BOSS2_FRAME_attack10);
	G_ProjectSource (self->s.origin, monster_flash_offset[flash_number], forward, right, start);

	VectorCopy (self->enemy->s.origin, vec);
	vec[2] += self->enemy->viewheight;
	VectorSubtract (vec, start, dir);
	VectorNormalize (dir);
	monster_fire_bullet (self, start, dir, 3, 4, DEFAULT_BULLET_HSPREAD, DEFAULT_BULLET_VSPREAD, flash_number);
*/
	boss2_firebullet_left(self);
	boss2_firebullet_right(self);
}


mframe_t boss2_frames_stand [] =
{
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL
};
mmove_t	boss2_move_stand = {BOSS2_FRAME_stand30, BOSS2_FRAME_stand50, boss2_frames_stand, NULL};

mframe_t boss2_frames_fidget [] =
{
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL
};
mmove_t boss2_move_fidget = {BOSS2_FRAME_stand1, BOSS2_FRAME_stand30, boss2_frames_fidget, NULL};

mframe_t boss2_frames_walk [] =
{
	ai_walk,	8,	NULL,
	ai_walk,	8,	NULL,
	ai_walk,	8,	NULL,
	ai_walk,	8,	NULL,
	ai_walk,	8,	NULL,
	ai_walk,	8,	NULL,
	ai_walk,	8,	NULL,
	ai_walk,	8,	NULL,
	ai_walk,	8,	NULL,
	ai_walk,	8,	NULL,
	ai_walk,	8,	NULL,
	ai_walk,	8,	NULL,
	ai_walk,	8,	NULL,
	ai_walk,	8,	NULL,
	ai_walk,	8,	NULL,
	ai_walk,	8,	NULL,
	ai_walk,	8,	NULL,
	ai_walk,	8,	NULL,
	ai_walk,	8,	NULL,
	ai_walk,	8,	NULL
};
mmove_t boss2_move_walk = {BOSS2_FRAME_walk1, BOSS2_FRAME_walk20, boss2_frames_walk, NULL};


mframe_t boss2_frames_run [] =
{
	ai_run,	8,	NULL,
	ai_run,	8,	NULL,
	ai_run,	8,	NULL,
	ai_run,	8,	NULL,
	ai_run,	8,	NULL,
	ai_run,	8,	NULL,
	ai_run,	8,	NULL,
	ai_run,	8,	NULL,
	ai_run,	8,	NULL,
	ai_run,	8,	NULL,
	ai_run,	8,	NULL,
	ai_run,	8,	NULL,
	ai_run,	8,	NULL,
	ai_run,	8,	NULL,
	ai_run,	8,	NULL,
	ai_run,	8,	NULL,
	ai_run,	8,	NULL,
	ai_run,	8,	NULL,
	ai_run,	8,	NULL,
	ai_run,	8,	NULL
};
mmove_t boss2_move_run = {BOSS2_FRAME_walk1, BOSS2_FRAME_walk20, boss2_frames_run, NULL};

mframe_t boss2_frames_attack_pre_mg [] =
{
	ai_charge,	1,	NULL,
	ai_charge,	1,	NULL,
	ai_charge,	1,	NULL,
	ai_charge,	1,	NULL,
	ai_charge,	1,	NULL,
	ai_charge,	1,	NULL,
	ai_charge,	1,	NULL,
	ai_charge,	1,	NULL,
	ai_charge,	1,	boss2_attack_mg
};
mmove_t boss2_move_attack_pre_mg = {BOSS2_FRAME_attack1, BOSS2_FRAME_attack9, boss2_frames_attack_pre_mg, NULL};


// Loop this
mframe_t boss2_frames_attack_mg [] =
{
	ai_charge,	1,	Boss2MachineGun,
	ai_charge,	1,	Boss2MachineGun,
	ai_charge,	1,	Boss2MachineGun,
	ai_charge,	1,	Boss2MachineGun,
	ai_charge,	1,	Boss2MachineGun,
	ai_charge,	1,	boss2_reattack_mg
};
mmove_t boss2_move_attack_mg = {BOSS2_FRAME_attack10, BOSS2_FRAME_attack15, boss2_frames_attack_mg, NULL};

mframe_t boss2_frames_attack_post_mg [] =
{
	ai_charge,	1,	NULL,
	ai_charge,	1,	NULL,
	ai_charge,	1,	NULL,
	ai_charge,	1,	NULL
};
mmove_t boss2_move_attack_post_mg = {BOSS2_FRAME_attack16, BOSS2_FRAME_attack19, boss2_frames_attack_post_mg, boss2_run};

mframe_t boss2_frames_attack_rocket [] =
{
	ai_charge,	1,	NULL,
	ai_charge,	1,	NULL,
	ai_charge,	1,	NULL,
	ai_charge,	1,	NULL,
	ai_charge,	1,	NULL,
	ai_charge,	1,	NULL,
	ai_charge,	1,	NULL,
	ai_charge,	1,	NULL,
	ai_charge,	1,	NULL,
	ai_charge,	1,	NULL,
	ai_charge,	1,	NULL,
	ai_charge,	1,	NULL,
	ai_move,	-20,	Boss2Rocket,
	ai_charge,	1,	NULL,
	ai_charge,	1,	NULL,
	ai_charge,	1,	NULL,
	ai_charge,	1,	NULL,
	ai_charge,	1,	NULL,
	ai_charge,	1,	NULL,
	ai_charge,	1,	NULL,
	ai_charge,	1,	NULL
};
mmove_t boss2_move_attack_rocket = {BOSS2_FRAME_attack20, BOSS2_FRAME_attack40, boss2_frames_attack_rocket, boss2_run};

mframe_t boss2_frames_pain_heavy [] =
{
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL
};
mmove_t boss2_move_pain_heavy = {BOSS2_FRAME_pain2, BOSS2_FRAME_pain19, boss2_frames_pain_heavy, boss2_run};

mframe_t boss2_frames_pain_light [] =
{
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL
};
mmove_t boss2_move_pain_light = {BOSS2_FRAME_pain20, BOSS2_FRAME_pain23, boss2_frames_pain_light, boss2_run};

mframe_t boss2_frames_death [] =
{
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	NULL,
	ai_move,	0,	BossExplode
};
mmove_t boss2_move_death = {BOSS2_FRAME_death2, BOSS2_FRAME_death50, boss2_frames_death, boss2_dead};

void boss2_stand (edict_t *self)
{
		self->monsterinfo.currentmove = &boss2_move_stand;
}

void boss2_run (edict_t *self)
{
	if (self->monsterinfo.aiflags & AI_STAND_GROUND)
		self->monsterinfo.currentmove = &boss2_move_stand;
	else
		self->monsterinfo.currentmove = &boss2_move_run;
}

void boss2_walk (edict_t *self)
{
	self->monsterinfo.currentmove = &boss2_move_walk;
}

void boss2_attack (edict_t *self)
{
	vec3_t	vec;
	float	range;

	VectorSubtract (self->enemy->s.origin, self->s.origin, vec);
	range = VectorLength (vec);

	if (range <= 125)
	{
		self->monsterinfo.currentmove = &boss2_move_attack_pre_mg;
	}
	else
	{
		if (random() <= 0.6)
			self->monsterinfo.currentmove = &boss2_move_attack_pre_mg;
		else
			self->monsterinfo.currentmove = &boss2_move_attack_rocket;
	}
}

void boss2_attack_mg (edict_t *self)
{
	self->monsterinfo.currentmove = &boss2_move_attack_mg;
}

void boss2_reattack_mg (edict_t *self)
{
	if ( infront(self, self->enemy) )
		if (random() <= 0.7)
			self->monsterinfo.currentmove = &boss2_move_attack_mg;
		else
			self->monsterinfo.currentmove = &boss2_move_attack_post_mg;
	else
		self->monsterinfo.currentmove = &boss2_move_attack_post_mg;
}


void boss2_pain (edict_t *self, edict_t *other, float kick, int damage)
{
	if (self->health < (self->max_health / 2))
		self->s.skinnum = 1;

	if (level.time < self->pain_debounce_time)
		return;

	self->pain_debounce_time = level.time + 3;
// American wanted these at no attenuation
	if (damage < 10)
	{
		gi.sound (self, CHAN_VOICE, boss2_sound_pain3, 1, ATTN_NONE, 0);
		self->monsterinfo.currentmove = &boss2_move_pain_light;
	}
	else if (damage < 30)
	{
		gi.sound (self, CHAN_VOICE, boss2_sound_pain1, 1, ATTN_NONE, 0);
		self->monsterinfo.currentmove = &boss2_move_pain_light;
	}
	else
	{
		gi.sound (self, CHAN_VOICE, boss2_sound_pain2, 1, ATTN_NONE, 0);
		self->monsterinfo.currentmove = &boss2_move_pain_heavy;
	}
}

void boss2_dead (edict_t *self)
{
	VectorSet (self->mins, -56, -56, 0);
	VectorSet (self->maxs, 56, 56, 80);
	self->movetype = MOVETYPE_TOSS;
	self->svflags |= SVF_DEADMONSTER;
	self->nextthink = 0;
	gi.linkentity (self);
}

void boss2_die (edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point)
{
	gi.sound (self, CHAN_VOICE, boss2_sound_death, 1, ATTN_NONE, 0);
	self->deadflag = DEAD_DEAD;
	self->takedamage = DAMAGE_NO;
	self->count = 0;
	self->monsterinfo.currentmove = &boss2_move_death;
#if 0
	int		n;

	self->s.sound = 0;
	// check for gib
	if (self->health <= self->gib_health)
	{
		gi.sound (self, CHAN_VOICE, gi.soundindex ("misc/udeath.wav"), 1, ATTN_NORM, 0);
		if(net_compatibility->value || !net_fixoverflow->value)
		{
			for (n= 0; n < 2; n++)
				ThrowGib (self, "models/objects/gibs/bone/tris.md2", damage, GIB_ORGANIC);
			for (n= 0; n < 4; n++)
				ThrowGib (self, "models/objects/gibs/sm_meat/tris.md2", damage, GIB_ORGANIC);
			ThrowHead (self, "models/objects/gibs/head2/tris.md2", damage, GIB_ORGANIC);
			self->deadflag = DEAD_DEAD;
		}
		else
		{
			NetThrowGibs (self, GIBS_2BONES_4MEAT_HEAD2, damage);
			G_FreeEdict (self);
		}
		return;
	}

	if (self->deadflag == DEAD_DEAD)
		return;

	self->deadflag = DEAD_DEAD;
	self->takedamage = DAMAGE_YES;
	self->monsterinfo.currentmove = &boss2_move_death;
#endif
}

bool Boss2_CheckAttack (edict_t *self)
{
	vec3_t	spot1, spot2;
	vec3_t	temp;
	float	chance;
	trace_t	tr;
//	bool	enemy_infront;
	int		enemy_range;
	float	enemy_yaw;

	if (self->enemy->health > 0)
	{
	// see if any entities are in the way of the shot
		VectorCopy (self->s.origin, spot1);
		spot1[2] += self->viewheight;
		VectorCopy (self->enemy->s.origin, spot2);
		spot2[2] += self->enemy->viewheight;

		tr = gi.trace (spot1, NULL, NULL, spot2, self, CONTENTS_SOLID|CONTENTS_MONSTER|CONTENTS_SLIME|CONTENTS_LAVA);

		// do we have a clear shot?
		if (tr.ent != self->enemy)
			return false;
	}

//	enemy_infront = infront(self, self->enemy);
	enemy_range = range(self, self->enemy);
	VectorSubtract (self->enemy->s.origin, self->s.origin, temp);
	enemy_yaw = vectoyaw(temp);

	self->ideal_yaw = enemy_yaw;


	// melee attack
	if (enemy_range == RANGE_MELEE)
	{
		if (self->monsterinfo.melee)
			self->monsterinfo.attack_state = AS_MELEE;
		else
			self->monsterinfo.attack_state = AS_MISSILE;
		return true;
	}

// missile attack
	if (!self->monsterinfo.attack)
		return false;

	if (level.time < self->monsterinfo.attack_finished)
		return false;

	if (enemy_range == RANGE_FAR)
		return false;

	if (self->monsterinfo.aiflags & AI_STAND_GROUND)
	{
		chance = 0.4;
	}
	else if (enemy_range == RANGE_MELEE)
	{
		chance = 0.8;
	}
	else if (enemy_range == RANGE_NEAR)
	{
		chance = 0.8;
	}
	else if (enemy_range == RANGE_MID)
	{
		chance = 0.8;
	}
	else
	{
		return false;
	}

	if (random () < chance)
	{
		self->monsterinfo.attack_state = AS_MISSILE;
		self->monsterinfo.attack_finished = level.time + 2*random();
		return true;
	}

	if (self->flags & FL_FLY)
	{
		if (random() < 0.3)
			self->monsterinfo.attack_state = AS_SLIDING;
		else
			self->monsterinfo.attack_state = AS_STRAIGHT;
	}

	return false;
}



/*QUAKED monster_boss2 (1 .5 0) (-56 -56 0) (56 56 80) Ambush Trigger_Spawn Sight
*/
void SP_monster_boss2 (edict_t *self)
{
	if (deathmatch->value)
	{
		G_FreeEdict (self);
		return;
	}

	boss2_sound_pain1 = gi.soundindex ("bosshovr/bhvpain1.wav");
	boss2_sound_pain2 = gi.soundindex ("bosshovr/bhvpain2.wav");
	boss2_sound_pain3 = gi.soundindex ("bosshovr/bhvpain3.wav");
	boss2_sound_death = gi.soundindex ("bosshovr/bhvdeth1.wav");
	boss2_sound_search1 = gi.soundindex ("bosshovr/bhvunqv1.wav");

	self->s.sound = gi.soundindex ("bosshovr/bhvengn1.wav");

	self->movetype = MOVETYPE_STEP;
	self->solid = SOLID_BBOX;
	self->s.modelindex = gi.modelindex ("models/monsters/boss2/tris.md2");
	VectorSet (self->mins, -56, -56, 0);
	VectorSet (self->maxs, 56, 56, 80);

	self->health = 2000;
	self->gib_health = -200;
	self->mass = 1000;

	self->flags |= FL_IMMUNE_LASER;

	self->pain = boss2_pain;
	self->die = boss2_die;

	self->monsterinfo.stand = boss2_stand;
	self->monsterinfo.walk = boss2_walk;
	self->monsterinfo.run = boss2_run;
	self->monsterinfo.attack = boss2_attack;
	self->monsterinfo.search = boss2_search;
	self->monsterinfo.checkattack = Boss2_CheckAttack;
	gi.linkentity (self);

	self->monsterinfo.currentmove = &boss2_move_stand;
	self->monsterinfo.scale = BOSS2_MODEL_SCALE;

	flymonster_start (self, true);
}

/*
==============================================================================

end of boss2

==============================================================================
*/


/*
==============================================================================

boss3

==============================================================================
*/

void Use_Boss3 (edict_t *ent, edict_t *other, edict_t *activator)
{
	bool large_map = LargeMapCheck(ent->s.origin);
	if (large_map)
		gi.WriteByte (svc_temp_entity_ext);
	else
		gi.WriteByte (svc_temp_entity);
	gi.WriteByte (TE_BOSSTPORT);
	gi.WritePosition (ent->s.origin, large_map);
	gi.multicast (ent->s.origin, MULTICAST_PVS);
	G_FreeEdict (ent);
}

void Think_Boss3Stand (edict_t *ent)
{
	if (ent->s.frame == MAKRON_FRAME_stand260)
		ent->s.frame = MAKRON_FRAME_stand201;
	else
		ent->s.frame++;
	ent->nextthink = level.time + FRAMETIME;
}

/*QUAKED monster_boss3_stand (1 .5 0) (-32 -32 0) (32 32 90)

Just stands and cycles in one place until targeted, then teleports away.
*/
void SP_monster_boss3_stand (edict_t *self)
{
	if (deathmatch->value)
	{
		G_FreeEdict (self);
		return;
	}

	self->movetype = MOVETYPE_STEP;
	self->solid = SOLID_BBOX;
	self->model = "models/monsters/boss3/rider/tris.md2";
	self->s.modelindex = gi.modelindex (self->model);
	self->s.frame = MAKRON_FRAME_stand201;

	gi.soundindex ("misc/bigtele.wav");

	VectorSet (self->mins, -32, -32, 0);
	VectorSet (self->maxs, 32, 32, 90);

	self->use = Use_Boss3;
	self->think = Think_Boss3Stand;
	self->nextthink = level.time + FRAMETIME;
	gi.linkentity (self);
}

/*
==============================================================================

end of boss3

==============================================================================
*/



///////////  Turret //////////////////////////
// g_turret.c

void AnglesNormalize(vec3_t vec)
{
	while(vec[0] > 360)
		vec[0] -= 360;
	while(vec[0] < 0)
		vec[0] += 360;
	while(vec[1] > 360)
		vec[1] -= 360;
	while(vec[1] < 0)
		vec[1] += 360;
}

float SnapToEights(float x)
{
	x *= 8.0;
	if (x > 0.0)
		x += 0.5;
	else
		x -= 0.5;
	return 0.125 * (int)x;
}


void turret_blocked(edict_t *self, edict_t *other)
{
	edict_t	*attacker;

	if (other->takedamage)
	{
		if (self->teammaster->owner)
			attacker = self->teammaster->owner;
		else
			attacker = self->teammaster;
		T_Damage (other, self, attacker, vec3_origin, other->s.origin, vec3_origin, self->teammaster->dmg, 10, 0, MOD_CRUSH);
	}
}

/*QUAKED turret_breach (0 0 0) ?
This portion of the turret can change both pitch and yaw.
The model  should be made with a flat pitch.
It (and the associated base) need to be oriented towards 0.
Use "angle" to set the starting angle.

"speed"		default 50
"dmg"		default 10
"angle"		point this forward
"target"	point this at an info_notnull at the muzzle tip
"minpitch"	min acceptable pitch angle : default -30
"maxpitch"	max acceptable pitch angle : default 30
"minyaw"	min acceptable yaw angle   : default 0
"maxyaw"	max acceptable yaw angle   : default 360
*/

void turret_breach_fire (edict_t *self)
{
	vec3_t	f, r, u;
	vec3_t	start;
	int		damage;

	AngleVectors (self->s.angles, f, r, u);
	VectorMA (self->s.origin, self->move_origin[0], f, start);
	VectorMA (start, self->move_origin[1], r, start);
	VectorMA (start, self->move_origin[2], u, start);

	damage = 100 + random() * 50;
	fire_rocket (self->teammaster->owner, start, f, damage, 550 + 50 * skill->value, 150, damage);
	gi.positioned_sound (start, self, CHAN_WEAPON, gi.soundindex("weapons/rocklf1a.wav"), 1, ATTN_NORM, 0);
}

void turret_breach_think (edict_t *self)
{
	edict_t	*ent;
	vec3_t	current_angles;
	vec3_t	delta;

	VectorCopy (self->s.angles, current_angles);
	AnglesNormalize(current_angles);

	AnglesNormalize(self->move_angles);
	if (self->move_angles[PITCH] > 180)
		self->move_angles[PITCH] -= 360;

	// clamp angles to mins & maxs
	if (self->move_angles[PITCH] > self->pos1[PITCH])
		self->move_angles[PITCH] = self->pos1[PITCH];
	else if (self->move_angles[PITCH] < self->pos2[PITCH])
		self->move_angles[PITCH] = self->pos2[PITCH];

	if ((self->move_angles[YAW] < self->pos1[YAW]) || (self->move_angles[YAW] > self->pos2[YAW]))
	{
		float	dmin, dmax;

		dmin = fabs(self->pos1[YAW] - self->move_angles[YAW]);
		if (dmin < -180)
			dmin += 360;
		else if (dmin > 180)
			dmin -= 360;
		dmax = fabs(self->pos2[YAW] - self->move_angles[YAW]);
		if (dmax < -180)
			dmax += 360;
		else if (dmax > 180)
			dmax -= 360;
		if (fabs(dmin) < fabs(dmax))
			self->move_angles[YAW] = self->pos1[YAW];
		else
			self->move_angles[YAW] = self->pos2[YAW];
	}

	VectorSubtract (self->move_angles, current_angles, delta);
	if (delta[0] < -180)
		delta[0] += 360;
	else if (delta[0] > 180)
		delta[0] -= 360;
	if (delta[1] < -180)
		delta[1] += 360;
	else if (delta[1] > 180)
		delta[1] -= 360;
	delta[2] = 0;

	if (delta[0] > self->speed * FRAMETIME)
		delta[0] = self->speed * FRAMETIME;
	if (delta[0] < -1 * self->speed * FRAMETIME)
		delta[0] = -1 * self->speed * FRAMETIME;
	if (delta[1] > self->speed * FRAMETIME)
		delta[1] = self->speed * FRAMETIME;
	if (delta[1] < -1 * self->speed * FRAMETIME)
		delta[1] = -1 * self->speed * FRAMETIME;

	VectorScale (delta, 1.0/FRAMETIME, self->avelocity);

	self->nextthink = level.time + FRAMETIME;

	for (ent = self->teammaster; ent; ent = ent->teamchain)
		ent->avelocity[1] = self->avelocity[1];

	// if we have adriver, adjust his velocities
	if (self->owner)
	{
		float	angle;
		float	target_z;
		float	diff;
		float	s, c;
		vec3_t	target;
		vec3_t	dir;

		// angular is easy, just copy ours
		self->owner->avelocity[0] = self->avelocity[0];
		self->owner->avelocity[1] = self->avelocity[1];

		// x & y
		angle = self->s.angles[1] + self->owner->move_origin[1];
		angle *= (M_PI*2 / 360);
		SinCos(angle, &s, &c);
		target[0] = SnapToEights(self->s.origin[0] + c * self->owner->move_origin[0]);
		target[1] = SnapToEights(self->s.origin[1] + s * self->owner->move_origin[0]);
		target[2] = self->owner->s.origin[2];

		VectorSubtract (target, self->owner->s.origin, dir);
		self->owner->velocity[0] = dir[0] * 1.0 / FRAMETIME;
		self->owner->velocity[1] = dir[1] * 1.0 / FRAMETIME;

		// z
		angle = self->s.angles[PITCH] * (M_PI*2 / 360);
		target_z = SnapToEights(self->s.origin[2] + self->owner->move_origin[0] * tan(angle) + self->owner->move_origin[2]);

		diff = target_z - self->owner->s.origin[2];
		self->owner->velocity[2] = diff * 1.0 / FRAMETIME;

		if (self->spawnflags & 65536)
		{
			turret_breach_fire (self);
			self->spawnflags &= ~65536;
		}
	}
}

void turret_breach_finish_init (edict_t *self)
{
	// get and save info for muzzle location
	if (!self->target)
	{
		gi.dprintf("%s at %s needs a target\n", self->classname, vtos(self->s.origin));
	}
	else
	{
		self->target_ent = G_PickTarget (self->target, NULL);
		VectorSubtract (self->target_ent->s.origin, self->s.origin, self->move_origin);
		G_FreeEdict(self->target_ent);
	}

	self->teammaster->dmg = self->dmg;
	self->think = turret_breach_think;
	self->think (self);
}

void SP_turret_breach (edict_t *self)
{
	self->solid = SOLID_BSP;
	self->movetype = MOVETYPE_PUSH;
	gi.setmodel (self, self->model);

	if (!self->speed)
		self->speed = 50;
	if (!self->dmg)
		self->dmg = 10;

	if (!st.minpitch)
		st.minpitch = -30;
	if (!st.maxpitch)
		st.maxpitch = 30;
	if (!st.maxyaw)
		st.maxyaw = 360;

	self->pos1[PITCH] = -1 * st.minpitch;
	self->pos1[YAW]   = st.minyaw;
	self->pos2[PITCH] = -1 * st.maxpitch;
	self->pos2[YAW]   = st.maxyaw;

	self->ideal_yaw = self->s.angles[YAW];
	self->move_angles[YAW] = self->ideal_yaw;

	self->blocked = turret_blocked;

	self->think = turret_breach_finish_init;
	self->nextthink = level.time + FRAMETIME;
	gi.linkentity (self);
}


/*QUAKED turret_base (0 0 0) ?
This portion of the turret changes yaw only.
MUST be teamed with a turret_breach.
*/

void SP_turret_base (edict_t *self)
{
	self->solid = SOLID_BSP;
	self->movetype = MOVETYPE_PUSH;
	gi.setmodel (self, self->model);
	self->blocked = turret_blocked;
	gi.linkentity (self);
}


/*QUAKED turret_driver (1 .5 0) (-16 -16 -24) (16 16 32)
Must NOT be on the team with the rest of the turret parts.
Instead it must target the turret_breach.
*/

void turret_driver_die (edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point)
{
	edict_t	*ent;

	// level the gun
	self->target_ent->move_angles[0] = 0;

	// remove the driver from the end of them team chain
	for (ent = self->target_ent->teammaster; ent->teamchain != self; ent = ent->teamchain)
		;
	ent->teamchain = NULL;
	self->teammaster = NULL;
	self->flags &= ~FL_TEAMSLAVE;

	self->target_ent->owner = NULL;
	self->target_ent->teammaster->owner = NULL;

	infantry_die (self, inflictor, attacker, damage, NULL);
}

void turret_driver_think (edict_t *self)
{
	vec3_t	target;
	vec3_t	dir;
	float	reaction_time;

	self->nextthink = level.time + FRAMETIME;

	if (self->enemy && (!self->enemy->inuse || self->enemy->health <= 0))
		self->enemy = NULL;

	if (!self->enemy)
	{
		if (!FindTarget (self))
			return;
		self->monsterinfo.trail_time = level.time;
		self->monsterinfo.aiflags &= ~AI_LOST_SIGHT;
	}
	else
	{
		if (visible (self, self->enemy))
		{
			if (self->monsterinfo.aiflags & AI_LOST_SIGHT)
			{
				self->monsterinfo.trail_time = level.time;
				self->monsterinfo.aiflags &= ~AI_LOST_SIGHT;
			}
		}
		else
		{
			self->monsterinfo.aiflags |= AI_LOST_SIGHT;
			return;
		}
	}

	// let the turret know where we want it to aim
	VectorCopy (self->enemy->s.origin, target);
	target[2] += self->enemy->viewheight;
	VectorSubtract (target, self->target_ent->s.origin, dir);
	vectoangles (dir, self->target_ent->move_angles);

	// decide if we should shoot
	if (level.time < self->monsterinfo.attack_finished)
		return;

	reaction_time = (3 - skill->value) * 1.0;
	if ((level.time - self->monsterinfo.trail_time) < reaction_time)
		return;

	self->monsterinfo.attack_finished = level.time + reaction_time + 1.0;
	//FIXME how do we really want to pass this along?
	self->target_ent->spawnflags |= 65536;
}

void turret_driver_link (edict_t *self)
{
	vec3_t	vec;
	edict_t	*ent;

	self->think = turret_driver_think;
	self->nextthink = level.time + FRAMETIME;

	self->target_ent = G_PickTarget (self->target, NULL);
	self->target_ent->owner = self;
	self->target_ent->teammaster->owner = self;
	VectorCopy (self->target_ent->s.angles, self->s.angles);

	vec[0] = self->target_ent->s.origin[0] - self->s.origin[0];
	vec[1] = self->target_ent->s.origin[1] - self->s.origin[1];
	vec[2] = 0;
	self->move_origin[0] = VectorLength(vec);

	VectorSubtract (self->s.origin, self->target_ent->s.origin, vec);
	vectoangles (vec, vec);
	AnglesNormalize(vec);
	self->move_origin[1] = vec[1];

	self->move_origin[2] = self->s.origin[2] - self->target_ent->s.origin[2];

	// add the driver to the end of them team chain
	for (ent = self->target_ent->teammaster; ent->teamchain; ent = ent->teamchain)
		;
	ent->teamchain = self;
	self->teammaster = self->target_ent->teammaster;
	self->flags |= FL_TEAMSLAVE;
}

void SP_turret_driver (edict_t *self)
{
	if (deathmatch->value)
	{
		G_FreeEdict (self);
		return;
	}

	self->movetype = MOVETYPE_PUSH;
	self->solid = SOLID_BBOX;
	self->s.modelindex = gi.modelindex("models/monsters/infantry/tris.md2");
	VectorSet (self->mins, -16, -16, -24);
	VectorSet (self->maxs, 16, 16, 32);

	self->health = 100;
	self->gib_health = 0;
	self->mass = 200;
	self->viewheight = 24;

	self->die = turret_driver_die;
	self->monsterinfo.stand = infantry_stand;

	self->flags |= FL_NO_KNOCKBACK;

	level.total_monsters++;

	self->svflags |= SVF_MONSTER;
	self->s.renderfx |= RF_FRAMELERP;
	self->takedamage = DAMAGE_AIM;
	self->use = monster_use;
	self->clipmask = MASK_MONSTERSOLID;
	VectorCopy (self->s.origin, self->s.old_origin);
	self->monsterinfo.aiflags |= AI_STAND_GROUND|AI_DUCKED;

	if (st.item)
	{
		self->item = FindItemByClassname (st.item);
		if (!self->item)
			gi.dprintf("%s at %s has bad item: %s\n", self->classname, vtos(self->s.origin), st.item);
	}

	self->think = turret_driver_link;
	self->nextthink = level.time + FRAMETIME;

	gi.linkentity (self);
}
/////////// END of TURRET /////////////////////////////


/*
==============================================================================

ACTOR

==============================================================================
*/

#define	MAX_ACTOR_NAMES		8
char *actor_names[MAX_ACTOR_NAMES] =
{
	"Hellrot",
	"Tokay",
	"Killme",
	"Disruptor",
	"Adrianator",
	"Rambear",
	"Titus",
	"Bitterman"
};


mframe_t actor_frames_stand [] =
{
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,

	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,

	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,

	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL,
	ai_stand, 0, NULL
};
mmove_t actor_move_stand = {ACTOR_FRAME_stand101, ACTOR_FRAME_stand140, actor_frames_stand, NULL};

void actor_stand (edict_t *self)
{
	self->monsterinfo.currentmove = &actor_move_stand;

	// randomize on startup
	if (level.time < 1.0)
		self->s.frame = self->monsterinfo.currentmove->firstframe + (rand() % (self->monsterinfo.currentmove->lastframe - self->monsterinfo.currentmove->firstframe + 1));
}


mframe_t actor_frames_walk [] =
{
	ai_walk, 0,  NULL,
	ai_walk, 6,  NULL,
	ai_walk, 10, NULL,
	ai_walk, 3,  NULL,
	ai_walk, 2,  NULL,
	ai_walk, 7,  NULL,
	ai_walk, 10, NULL,
	ai_walk, 1,  NULL,
	ai_walk, 4,  NULL,
	ai_walk, 0,  NULL,
	ai_walk, 0,  NULL
};
mmove_t actor_move_walk = {ACTOR_FRAME_walk01, ACTOR_FRAME_walk08, actor_frames_walk, NULL};

void actor_walk (edict_t *self)
{
	self->monsterinfo.currentmove = &actor_move_walk;
}


mframe_t actor_frames_run [] =
{
	ai_run, 4,  NULL,
	ai_run, 15, NULL,
	ai_run, 15, NULL,
	ai_run, 8,  NULL,
	ai_run, 20, NULL,
	ai_run, 15, NULL,
	ai_run, 8,  NULL,
	ai_run, 17, NULL,
	ai_run, 12, NULL,
	ai_run, -2, NULL,
	ai_run, -2, NULL,
	ai_run, -1, NULL
};
mmove_t actor_move_run = {ACTOR_FRAME_run02, ACTOR_FRAME_run07, actor_frames_run, NULL};

void actor_run (edict_t *self)
{
	if ((level.time < self->pain_debounce_time) && (!self->enemy))
	{
		if (self->movetarget)
			actor_walk(self);
		else
			actor_stand(self);
		return;
	}

	if (self->monsterinfo.aiflags & AI_STAND_GROUND)
	{
		actor_stand(self);
		return;
	}

	self->monsterinfo.currentmove = &actor_move_run;
}


mframe_t actor_frames_pain1 [] =
{
	ai_move, -5, NULL,
	ai_move, 4,  NULL,
	ai_move, 1,  NULL
};
mmove_t actor_move_pain1 = {ACTOR_FRAME_pain101, ACTOR_FRAME_pain103, actor_frames_pain1, actor_run};

mframe_t actor_frames_pain2 [] =
{
	ai_move, -4, NULL,
	ai_move, 4,  NULL,
	ai_move, 0,  NULL
};
mmove_t actor_move_pain2 = {ACTOR_FRAME_pain201, ACTOR_FRAME_pain203, actor_frames_pain2, actor_run};

mframe_t actor_frames_pain3 [] =
{
	ai_move, -1, NULL,
	ai_move, 1,  NULL,
	ai_move, 0,  NULL
};
mmove_t actor_move_pain3 = {ACTOR_FRAME_pain301, ACTOR_FRAME_pain303, actor_frames_pain3, actor_run};

mframe_t actor_frames_flipoff [] =
{
	ai_turn, 0,  NULL,
	ai_turn, 0,  NULL,
	ai_turn, 0,  NULL,
	ai_turn, 0,  NULL,
	ai_turn, 0,  NULL,
	ai_turn, 0,  NULL,
	ai_turn, 0,  NULL,
	ai_turn, 0,  NULL,
	ai_turn, 0,  NULL,
	ai_turn, 0,  NULL,
	ai_turn, 0,  NULL,
	ai_turn, 0,  NULL,
	ai_turn, 0,  NULL,
	ai_turn, 0,  NULL
};
mmove_t actor_move_flipoff = {ACTOR_FRAME_flip01, ACTOR_FRAME_flip14, actor_frames_flipoff, actor_run};

mframe_t actor_frames_taunt [] =
{
	ai_turn, 0,  NULL,
	ai_turn, 0,  NULL,
	ai_turn, 0,  NULL,
	ai_turn, 0,  NULL,
	ai_turn, 0,  NULL,
	ai_turn, 0,  NULL,
	ai_turn, 0,  NULL,
	ai_turn, 0,  NULL,
	ai_turn, 0,  NULL,
	ai_turn, 0,  NULL,
	ai_turn, 0,  NULL,
	ai_turn, 0,  NULL,
	ai_turn, 0,  NULL,
	ai_turn, 0,  NULL,
	ai_turn, 0,  NULL,
	ai_turn, 0,  NULL,
	ai_turn, 0,  NULL
};
mmove_t actor_move_taunt = {ACTOR_FRAME_taunt01, ACTOR_FRAME_taunt17, actor_frames_taunt, actor_run};

char *messages[] =
{
	"Watch it",
	"#$@*&",
	"Idiot",
	"Check your targets"
};

void actor_pain (edict_t *self, edict_t *other, float kick, int damage)
{
	int		n;

	if (self->health < (self->max_health / 2))
		self->s.skinnum = 1;

	if (level.time < self->pain_debounce_time)
		return;

	self->pain_debounce_time = level.time + 3;
//	gi.sound (self, CHAN_VOICE, actor.sound_pain, 1, ATTN_NORM, 0);

	if ((other->client) && (random() < 0.4))
	{
		vec3_t	v;
		char	*name;

		VectorSubtract (other->s.origin, self->s.origin, v);
		self->ideal_yaw = vectoyaw (v);
		if (random() < 0.5)
			self->monsterinfo.currentmove = &actor_move_flipoff;
		else
			self->monsterinfo.currentmove = &actor_move_taunt;
		name = actor_names[(self - g_edicts)%MAX_ACTOR_NAMES];
		gi.cprintf (other, PRINT_CHAT, "%s: %s!\n", name, messages[rand()%3]);
		return;
	}

	n = rand() % 3;
	if (n == 0)
		self->monsterinfo.currentmove = &actor_move_pain1;
	else if (n == 1)
		self->monsterinfo.currentmove = &actor_move_pain2;
	else
		self->monsterinfo.currentmove = &actor_move_pain3;
}


void actorMachineGun (edict_t *self)
{
	vec3_t	start, target;
	vec3_t	forward, right;

	AngleVectors (self->s.angles, forward, right, NULL);
	G_ProjectSource (self->s.origin, &monster_flash_offset[MZ2_ACTOR_MACHINEGUN_1], forward, right, start);
	if (self->enemy)
	{
		if (self->enemy->health > 0)
		{
			VectorMA (self->enemy->s.origin, -0.2, self->enemy->velocity, target);
			target[2] += self->enemy->viewheight;
		}
		else
		{
			VectorCopy (self->enemy->absmin, target);
			target[2] += (self->enemy->size[2] / 2);
		}
		VectorSubtract (target, start, forward);
		VectorNormalize (forward);
	}
	else
	{
		AngleVectors (self->s.angles, forward, NULL, NULL);
	}
	monster_fire_bullet (self, start, forward, 3, 4, DEFAULT_BULLET_HSPREAD, DEFAULT_BULLET_VSPREAD, MZ2_ACTOR_MACHINEGUN_1);
}

void actor_fire (edict_t *self)
{
	actorMachineGun (self);

	if (level.time >= self->monsterinfo.pausetime)
		self->monsterinfo.aiflags &= ~AI_HOLD_FRAME;
	else
		self->monsterinfo.aiflags |= AI_HOLD_FRAME;
}

mframe_t actor_frames_attack [] =
{
	ai_charge, -2,  actor_fire,
	ai_charge, -2,  NULL,
	ai_charge, 3,   NULL,
	ai_charge, 2,   NULL
};
mmove_t actor_move_attack = {ACTOR_FRAME_attak01, ACTOR_FRAME_attak04, actor_frames_attack, actor_run};

void actor_dead (edict_t *self)
{
	VectorSet (self->mins, -16, -16, -24);
	VectorSet (self->maxs, 16, 16, -8);
	self->movetype = MOVETYPE_TOSS;
	self->svflags |= SVF_DEADMONSTER;
	self->nextthink = 0;
	gi.linkentity (self);

	M_FlyCheck (self);
}

mframe_t actor_frames_death1 [] =
{
	ai_move, 0,   NULL,
	ai_move, 0,   NULL,
	ai_move, -13, NULL,
	ai_move, 14,  NULL,
	ai_move, 3,   NULL,
	ai_move, -2,  NULL,
	ai_move, 1,   NULL
};
mmove_t actor_move_death1 = {ACTOR_FRAME_death101, ACTOR_FRAME_death107, actor_frames_death1, actor_dead};

mframe_t actor_frames_death2 [] =
{
	ai_move, 0,   NULL,
	ai_move, 7,   NULL,
	ai_move, -6,  NULL,
	ai_move, -5,  NULL,
	ai_move, 1,   NULL,
	ai_move, 0,   NULL,
	ai_move, -1,  NULL,
	ai_move, -2,  NULL,
	ai_move, -1,  NULL,
	ai_move, -9,  NULL,
	ai_move, -13, NULL,
	ai_move, -13, NULL,
	ai_move, 0,   NULL
};
mmove_t actor_move_death2 = {ACTOR_FRAME_death201, ACTOR_FRAME_death213, actor_frames_death2, actor_dead};

void actor_die (edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point)
{
	int		n;

// check for gib
	if (self->health <= -80)
	{
//		gi.sound (self, CHAN_VOICE, actor.sound_gib, 1, ATTN_NORM, 0);
		if(net_compatibility->value || !net_fixoverflow->value)
		{
			for (n= 0; n < 2; n++)
				ThrowGib (self, "models/objects/gibs/bone/tris.md2", damage, GIB_ORGANIC);
			for (n= 0; n < 4; n++)
				ThrowGib (self, "models/objects/gibs/sm_meat/tris.md2", damage, GIB_ORGANIC);
			ThrowHead (self, "models/objects/gibs/head2/tris.md2", damage, GIB_ORGANIC);
			self->deadflag = DEAD_DEAD;
		}
		else
		{
			NetThrowMonsterGibs (self, GIBS_2BONES_4MEAT_HEAD2, damage);
			G_FreeEdict (self);
		}
		return;
	}

	if (self->deadflag == DEAD_DEAD)
		return;

// regular death
//	gi.sound (self, CHAN_VOICE, actor.sound_die, 1, ATTN_NORM, 0);
	self->deadflag = DEAD_DEAD;
	self->takedamage = DAMAGE_YES;

	n = rand() % 2;
	if (n == 0)
		self->monsterinfo.currentmove = &actor_move_death1;
	else
		self->monsterinfo.currentmove = &actor_move_death2;
}


void actor_attack(edict_t *self)
{
	int		n;

	self->monsterinfo.currentmove = &actor_move_attack;
	n = (rand() & 15) + 3 + 7;
	self->monsterinfo.pausetime = level.time + n * FRAMETIME;
}


void actor_use (edict_t *self, edict_t *other, edict_t *activator)
{
	vec3_t		v;

	self->goalentity = self->movetarget = G_PickTarget(self->target, NULL);
	if ((!self->movetarget) || (strcmp(self->movetarget->classname, "target_actor") != 0))
	{
		gi.dprintf ("%s has bad target %s at %s\n", self->classname, self->target, vtos(self->s.origin));
		self->target = NULL;
		self->monsterinfo.pausetime = 100000000;
		self->monsterinfo.stand (self);
		return;
	}

	VectorSubtract (self->goalentity->s.origin, self->s.origin, v);
	self->ideal_yaw = self->s.angles[YAW] = vectoyaw(v);
	self->monsterinfo.walk (self);
	self->target = NULL;
}


/*QUAKED misc_actor (1 .5 0) (-16 -16 -24) (16 16 32)
*/

void SP_misc_actor (edict_t *self)
{
	if (deathmatch->value)
	{
		G_FreeEdict (self);
		return;
	}

	if (!self->targetname)
	{
		gi.dprintf("untargeted %s at %s\n", self->classname, vtos(self->s.origin));
		G_FreeEdict (self);
		return;
	}

	if (!self->target)
	{
		gi.dprintf("%s with no target at %s\n", self->classname, vtos(self->s.origin));
		G_FreeEdict (self);
		return;
	}

	self->movetype = MOVETYPE_STEP;
	self->solid = SOLID_BBOX;
	self->s.modelindex = gi.modelindex("players/male/tris.md2");
	VectorSet (self->mins, -16, -16, -24);
	VectorSet (self->maxs, 16, 16, 32);

	if (!self->health)
		self->health = 100;
	self->mass = 200;

	self->pain = actor_pain;
	self->die = actor_die;

	self->monsterinfo.stand = actor_stand;
	self->monsterinfo.walk = actor_walk;
	self->monsterinfo.run = actor_run;
	self->monsterinfo.attack = actor_attack;
	self->monsterinfo.melee = NULL;
	self->monsterinfo.sight = NULL;

	self->monsterinfo.aiflags |= AI_GOOD_GUY;

	gi.linkentity (self);

	self->monsterinfo.currentmove = &actor_move_stand;
	self->monsterinfo.scale = ACTOR_MODEL_SCALE;

	walkmonster_start (self, true);

	// actors always start in a dormant state, they *must* be used to get going
	self->use = actor_use;
}


/*QUAKED target_actor (.5 .3 0) (-8 -8 -8) (8 8 8) JUMP SHOOT ATTACK x HOLD BRUTAL
JUMP			jump in set direction upon reaching this target
SHOOT			take a single shot at the pathtarget
ATTACK			attack pathtarget until it or actor is dead

"target"		next target_actor
"pathtarget"	target of any action to be taken at this point
"wait"			amount of time actor should pause at this point
"message"		actor will "say" this to the player

for JUMP only:
"speed"			speed thrown forward (default 200)
"height"		speed thrown upwards (default 200)
*/

void target_actor_touch (edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf)
{
	vec3_t	v;

	if (other->movetarget != self)
		return;

	if (other->enemy)
		return;

	other->goalentity = other->movetarget = NULL;

	if (self->message)
	{
		int		n;
		edict_t	*ent;

		for (n = 1; n <= game.maxclients; n++)
		{
			ent = &g_edicts[n];
			if (!ent->inuse)
				continue;
			gi.cprintf (ent, PRINT_CHAT, "%s: %s\n", actor_names[(other - g_edicts)%MAX_ACTOR_NAMES], self->message);
		}
	}

	if (self->spawnflags & 1)		//jump
	{
		other->velocity[0] = self->movedir[0] * self->speed;
		other->velocity[1] = self->movedir[1] * self->speed;

		if (other->groundentity)
		{
			other->groundentity = NULL;
			other->velocity[2] = self->movedir[2];
			gi.sound(other, CHAN_VOICE, gi.soundindex("player/male/jump1.wav"), 1, ATTN_NORM, 0);
		}
	}

	if (self->spawnflags & 2)	//shoot
	{
	}
	else if (self->spawnflags & 4)	//attack
	{
		other->enemy = G_PickTarget(self->pathtarget, NULL);
		if (other->enemy)
		{
			other->goalentity = other->enemy;
			if (self->spawnflags & 32)
				other->monsterinfo.aiflags |= AI_BRUTAL;
			if (self->spawnflags & 16)
			{
				other->monsterinfo.aiflags |= AI_STAND_GROUND;
				actor_stand (other);
			}
			else
			{
				actor_run (other);
			}
		}
	}

	if (!(self->spawnflags & 6) && (self->pathtarget))
	{
		char *savetarget;

		savetarget = self->target;
		self->target = self->pathtarget;
		G_UseTargets (self, other, NULL);
		self->target = savetarget;
	}

	other->movetarget = G_PickTarget(self->target, NULL);

	if (!other->goalentity)
		other->goalentity = other->movetarget;

	if (!other->movetarget && !other->enemy)
	{
		other->monsterinfo.pausetime = level.time + 100000000;
		other->monsterinfo.stand (other);
	}
	else if (other->movetarget == other->goalentity)
	{
		VectorSubtract (other->movetarget->s.origin, other->s.origin, v);
		other->ideal_yaw = vectoyaw (v);
	}
}

void SP_target_actor (edict_t *self)
{
	if (!self->targetname)
		gi.dprintf ("%s with no targetname at %s\n", self->classname, vtos(self->s.origin));

	self->solid = SOLID_TRIGGER;
	self->touch = target_actor_touch;
	VectorSet (self->mins, -8, -8, -8);
	VectorSet (self->maxs, 8, 8, 8);
	self->svflags = SVF_NOCLIENT;

	if (self->spawnflags & 1)
	{
		if (!self->speed)
			self->speed = 200;
		if (!st.height)
			st.height = 200;
		if (self->s.angles[YAW] == 0)
			self->s.angles[YAW] = 360;
		G_SetMovedir (self->s.angles, self->movedir);
		self->movedir[2] = st.height;
	}

	gi.linkentity (self);
}
/////////// END of ACTOR /////////////////////////////




//// for Berserker's savegame
void	*savefuncs[] = {
	actor_attack,
	actor_dead,
	actor_die,
	actor_fire,
	actor_pain,
	actor_run,
	actor_stand,
	actor_use,
	actor_walk,
	ai_charge,
	ai_move,
	ai_run,
	ai_stand,
	ai_turn,
	ai_walk,
	AngleMove_Begin,
	AngleMove_Done,
	AngleMove_Final,
	barrel_delay,
	barrel_explode,
	barrel_touch,
	berserk_attack_club,
	berserk_attack_spike,
	berserk_dead,
	berserk_die,
	berserk_fidget,
	berserk_melee,
	berserk_pain,
	berserk_run,
	berserk_search,
	berserk_sight,
	berserk_stand,
	berserk_strike,
	berserk_swing,
	berserk_walk,
	bfg_explode,
	bfg_think,
	bfg_touch,
	blaster_touch,
	body_die,
	boss2_attack,
	boss2_attack_mg,
	Boss2_CheckAttack,
	boss2_dead,
	boss2_die,
	boss2_pain,
	boss2_reattack_mg,
	boss2_run,
	boss2_search,
	boss2_stand,
	boss2_walk,
	Boss2MachineGun,
	Boss2Rocket,
	BossExplode,
	brain_chest_closed,
	brain_chest_open,
	brain_dead,
	brain_die,
	brain_dodge,
	brain_duck_down,
	brain_duck_hold,
	brain_duck_up,
	brain_hit_left,
	brain_hit_right,
	brain_idle,
	brain_melee,
	brain_pain,
	brain_run,
	brain_search,
	brain_sight,
	brain_stand,
	brain_swing_left,
	brain_swing_right,
	brain_tentacle_attack,
	brain_walk,
	button_done,
	button_killed,
	button_return,
	button_touch,
	button_use,
	button_wait,
	chick_attack,
	chick_attack1,
	chick_dead,
	chick_die,
	chick_dodge,
	chick_duck_down,
	chick_duck_hold,
	chick_duck_up,
	chick_fidget,
	chick_melee,
	chick_pain,
	Chick_PreAttack1,
	chick_rerocket,
	chick_reslash,
	chick_run,
	chick_sight,
	chick_slash,
	chick_stand,
	chick_walk,
	ChickMoan,
	ChickReload,
	ChickRocket,
	ChickSlash,
	commander_body_drop,
	commander_body_think,
	commander_body_use,
	debris_die,
	door_blocked,
	door_go_down,
	door_hit_bottom,
	door_hit_top,
	door_killed,
	door_secret_blocked,
	door_secret_die,
	door_secret_done,
	door_secret_move1,
	door_secret_move2,
	door_secret_move3,
	door_secret_move4,
	door_secret_move5,
	door_secret_move6,
	door_secret_use,
	door_touch,
	door_use,
	DoRespawn,
	Drop_Ammo,
	Drop_General,
	drop_make_touchable,
	Drop_PowerArmor,
	drop_temp_touch,
	Drop_Weapon,
	droptofloor,
	flipper_bite,
	flipper_dead,
	flipper_die,
	flipper_melee,
	flipper_pain,
	flipper_preattack,
	flipper_run,
	flipper_run_loop,
	flipper_sight,
	flipper_stand,
	flipper_start_run,
	flipper_walk,
	floater_attack,
	floater_dead,
	floater_die,
	floater_fire_blaster,
	floater_idle,
	floater_melee,
	floater_pain,
	floater_run,
	floater_sight,
	floater_stand,
	floater_walk,
	floater_wham,
	floater_zap,
	flyer_attack,
	flyer_check_melee,
	flyer_die,
	flyer_fireleft,
	flyer_fireright,
	flyer_idle,
	flyer_loop_melee,
	flyer_melee,
	flyer_nextmove,
	flyer_pain,
	flyer_pop_blades,
	flyer_run,
	flyer_sight,
	flyer_slash_left,
	flyer_slash_right,
	flyer_stand,
	flyer_walk,
	flymonster_start_go,
	flyQbe_attack,
	flyQbe_die,
	flyQbe_none,
	flyQbe_none_sight,
	flyQbe_pain,
	flyQbe_stand,
	func_clock_think,
	func_clock_use,
	func_conveyor_use,
	func_explosive_explode,
	func_explosive_spawn,
	func_explosive_use,
	func_object_release,
	func_object_touch,
	func_object_use,
	func_timer_think,
	func_timer_use,
	func_train_find,
	func_wall_use,
	G_FreeEdict,
	G_FreeEdictExt,
	GaldiatorMelee,
	gib_die,
	gib_think,
	gib_touch,
	gladiator_attack,
	gladiator_cleaver_swing,
	gladiator_dead,
	gladiator_die,
	gladiator_idle,
	gladiator_melee,
	gladiator_pain,
	gladiator_run,
	gladiator_search,
	gladiator_sight,
	gladiator_stand,
	gladiator_walk,
	GladiatorGun,
	Grenade_Explode,
	Grenade_Touch,
	gunner_attack,
	gunner_dead,
	gunner_die,
	gunner_dodge,
	gunner_duck_down,
	gunner_duck_hold,
	gunner_duck_up,
	gunner_fidget,
	gunner_fire_chain,
	gunner_idlesound,
	gunner_opengun,
	gunner_pain,
	gunner_refire_chain,
	gunner_run,
	gunner_search,
	gunner_sight,
	gunner_stand,
	gunner_walk,
	GunnerGrenade,
	hover_attack,
	hover_dead,
	hover_deadthink,
	hover_die,
	hover_fire_blaster,
	hover_pain,
	hover_reattack,
	hover_run,
	hover_search,
	hover_sight,
	hover_stand,
	hover_start_attack,
	hover_walk,
	hurt_touch,
	hurt_use,
	infantry_attack,
	infantry_cock_gun,
	infantry_dead,
	infantry_die,
	infantry_dodge,
	infantry_duck_down,
	infantry_duck_hold,
	infantry_duck_up,
	infantry_fidget,
	infantry_fire,
	infantry_pain,
	infantry_run,
	infantry_sight,
	infantry_smack,
	infantry_stand,
	infantry_swing,
	infantry_walk,
	InfantryMachineGun,
	insane_checkdown,
	insane_checkup,
	insane_cross,
	insane_dead,
	insane_die,
	insane_fist,
	insane_moan,
	insane_onground,
	insane_pain,
	insane_run,
	insane_scream,
	insane_shake,
	insane_stand,
	insane_walk,
	jorg_attack,
	jorg_attack1,
	Jorg_CheckAttack,
	jorg_dead,
	jorg_die,
	jorg_firebullet,
	jorg_idle,
	jorg_pain,
	jorg_reattack1,
	jorg_run,
	jorg_search,
	jorg_stand,
	jorg_step_left,
	jorg_step_right,
	jorg_walk,
	jorgBFG,
	light_use,
	M_CheckAttack,
	M_droptofloor,
	M_FliesOff,
	M_FliesOn,
	M_FlyQbeOn,
	makron_attack,
	makron_brainsplorch,
	Makron_CheckAttack,
	makron_dead,
	makron_die,
	makron_hit,
	makron_pain,
	makron_popup,
	makron_prerailgun,
	makron_run,
	makron_sight,
	makron_stand,
	makron_step_left,
	makron_step_right,
	makron_taunt,
	makron_torso_think,
	makron_walk,
	makronBFG,
	MakronHyperblaster,
	MakronRailgun,
	MakronSaveloc,
	MakronSpawn,
	MakronToss,
	medic_attack,
	medic_cable_attack,
	medic_checkattack,
	medic_continue,
	medic_dead,
	medic_die,
	medic_dodge,
	medic_duck_down,
	medic_duck_hold,
	medic_duck_up,
	medic_fire_blaster,
	medic_hook_launch,
	medic_idle,
	medic_pain,
	medic_run,
	medic_search,
	medic_sight,
	medic_stand,
	medic_walk,
	MegaHealth_think,
	misc_banner_think,
	misc_blackhole_think,
	misc_blackhole_use,
	misc_deadsoldier_die,
	misc_easterchick_think,
	misc_easterchick2_think,
	misc_eastertank_think,
	misc_satellite_dish_think,
	misc_satellite_dish_use,
	misc_strogg_ship_use,
	misc_viper_bomb_prethink,
	misc_viper_bomb_touch,
	misc_viper_bomb_use,
	misc_viper_use,
	model_think,
	model_use,
	monster_think,
	monster_triggered_spawn,
	monster_triggered_spawn_use,
	monster_use,
	Move_Begin,
	Move_Done,
	Move_Final,
	multi_wait,
	mutant_check_landing,
	mutant_check_refire,
	mutant_checkattack,
	mutant_dead,
	mutant_die,
	mutant_hit_left,
	mutant_hit_right,
	mutant_idle,
	mutant_idle_loop,
	mutant_jump,
	mutant_jump_takeoff,
	mutant_jump_touch,
	mutant_melee,
	mutant_pain,
	mutant_run,
	mutant_search,
	mutant_sight,
	mutant_stand,
	mutant_step,
	mutant_walk,
	mutant_walk_loop,
	parasite_attack,
	parasite_dead,
	parasite_die,
	parasite_do_fidget,
	parasite_drain_attack,
	parasite_idle,
	parasite_launch,
	parasite_pain,
	parasite_reel_in,
	parasite_refidget,
	parasite_run,
	parasite_scratch,
	parasite_sight,
	parasite_stand,
	parasite_start_run,
	parasite_start_walk,
	parasite_tap,
	parasite_walk,
	path_corner_touch,
	Pickup_Adrenaline,
	Pickup_Ammo,
	Pickup_AncientHead,
	Pickup_Armor,
	Pickup_Bandolier,
	Pickup_Health,
	Pickup_Key,
	Pickup_Pack,
	Pickup_PowerArmor,
	Pickup_Powerup,
	Pickup_Weapon,
	plat_blocked,
	plat_go_down,
	plat_hit_bottom,
	plat_hit_top,
	player_die,
	player_pain,
	point_combat_touch,
	rocket_touch,
	rotating_blocked,
	rotating_touch,
	rotating_use,
	soldier_attack,
	soldier_attack1_refire1,
	soldier_attack1_refire2,
	soldier_attack2_refire1,
	soldier_attack2_refire2,
	soldier_attack3_refire,
	soldier_attack6_refire,
	soldier_cock,
	soldier_dead,
	soldier_die,
	soldier_dodge,
	soldier_duck_down,
	soldier_duck_hold,
	soldier_duck_up,
	soldier_fire1,
	soldier_fire2,
	soldier_fire3,
	soldier_fire4,
	soldier_fire6,
	soldier_fire7,
	soldier_fire8,
	soldier_idle,
	soldier_pain,
	soldier_run,
	soldier_sight,
	soldier_stand,
	soldier_walk,
	soldier_walk1_random,
	SP_monster_flyQbe,
	SP_monster_flyQbe1,
	SP_monster_flyQbe2,
	supertank_attack,
	supertank_dead,
	supertank_die,
	supertank_pain,
	supertank_reattack1,
	supertank_run,
	supertank_search,
	supertank_stand,
	supertank_walk,
	supertankMachineGun,
	supertankRocket,
	swimmonster_start_go,
	tank_attack,
	tank_dead,
	tank_die,
	tank_doattack_rocket,
	tank_footstep,
	tank_idle,
	tank_pain,
	tank_poststrike,
	tank_reattack_blaster,
	tank_refire_rocket,
	tank_run,
	tank_sight,
	tank_stand,
	tank_thud,
	tank_walk,
	tank_windup,
	TankBlaster,
	TankMachineGun,
	TankRocket,
	TankStrike,
	target_actor_touch,
	target_crosslevel_target_think,
	target_damage_use,
	target_earthquake_think,
	target_earthquake_use,
	target_explosion_explode,
	target_kill_use,
	target_laser_start,
	target_laser_think,
	target_laser_use,
	target_lightramp_think,
	target_lightramp_use,
	target_string_use,
	teleport_effect_think,
	teleporter_touch,
	teleporter_use,
	Think_AccelMove,
	Think_Boss3Stand,
	Think_CalcMoveSpeed,
	Think_Delay,
	Think_SpawnDoorTrigger,
	Touch_DoorTrigger,
	Touch_Item,
	Touch_Multi,
	Touch_Plat_Center,
	train_blocked,
	train_next,
	train_use,
	train_wait,
	TreadSound,
	trigger_counter_use,
	trigger_crosslevel_trigger_use,
	trigger_elevator_init,
	trigger_elevator_use,
	trigger_enable,
	trigger_gravity_touch,
	trigger_key_use,
	trigger_monsterjump_touch,
	trigger_push_touch,
	trigger_relay_use,
	turret_blocked,
	turret_breach_finish_init,
	turret_breach_think,
	turret_driver_die,
	turret_driver_link,
	turret_driver_think,
	Use_Areaportal,
	Use_Boss3,
	Use_Breather,
	Use_Envirosuit,
	Use_Invisibility,
	Use_Invulnerability,
	Use_Item,
	use_killbox,
	Use_Multi,
	Use_Plat,
	Use_PowerArmor,
	Use_Quad,
	Use_Silencer,
	use_target_blaster,
	use_target_changelevel,
	use_target_explosion,
	Use_Target_Give,
	use_target_goal,
	Use_Target_Help,
	use_target_secret,
	use_target_spawner,
	Use_Target_Speaker,
	use_target_splash,
	Use_Target_Tent,
	Use_Weapon,
	walkmonster_start_go,
	Weapon_BFG,
	Weapon_Blaster,
	Weapon_Chaingun,
	Weapon_Grenade,
	Weapon_GrenadeLauncher,
	Weapon_HyperBlaster,
	Weapon_Machinegun,
	Weapon_Railgun,
	Weapon_RocketLauncher,
	Weapon_Shotgun,
	Weapon_SuperShotgun,
	monster_respawn
};

int FunctionToIndex(void *func)
{
	int i;
	int size = sizeof(savefuncs)/sizeof(void*);
	for (i=0; i<size; i++)
		if (savefuncs[i] == func)
			return i;
	gi.error("FunctionToIndex: unknown function\n");
	return 0;		// shut up compiler :(
}


void *IndexToFunction(int index)
{
	int size = sizeof(savefuncs)/sizeof(void*);
	if (index < size)
		return savefuncs[index];
	gi.error("IndexToFunction: unknown index\n");
	return NULL;		// shut up compiler :(
}


void	*savemmoves[] = {
	&actor_move_attack,
	&actor_move_death1,
	&actor_move_death2,
	&actor_move_flipoff,
	&actor_move_pain1,
	&actor_move_pain2,
	&actor_move_pain3,
	&actor_move_run,
	&actor_move_stand,
	&actor_move_taunt,
	&actor_move_walk,
	&berserk_move_attack_club,
	&berserk_move_attack_spike,
	&berserk_move_attack_strike,
	&berserk_move_death1,
	&berserk_move_death2,
	&berserk_move_pain1,
	&berserk_move_pain2,
	&berserk_move_run1,
	&berserk_move_stand,
	&berserk_move_stand_fidget,
	&berserk_move_walk,
	&boss2_move_attack_mg,
	&boss2_move_attack_post_mg,
	&boss2_move_attack_pre_mg,
	&boss2_move_attack_rocket,
	&boss2_move_death,
	&boss2_move_fidget,
	&boss2_move_pain_heavy,
	&boss2_move_pain_light,
	&boss2_move_run,
	&boss2_move_stand,
	&boss2_move_walk,
	&brain_move_attack1,
	&brain_move_attack2,
	&brain_move_death1,
	&brain_move_death2,
	&brain_move_defense,
	&brain_move_duck,
	&brain_move_idle,
	&brain_move_pain1,
	&brain_move_pain2,
	&brain_move_pain3,
	&brain_move_run,
	&brain_move_stand,
	&brain_move_walk1,
	&chick_move_attack1,
	&chick_move_death1,
	&chick_move_death2,
	&chick_move_duck,
	&chick_move_end_attack1,
	&chick_move_end_slash,
	&chick_move_fidget,
	&chick_move_pain1,
	&chick_move_pain2,
	&chick_move_pain3,
	&chick_move_run,
	&chick_move_slash,
	&chick_move_stand,
	&chick_move_start_attack1,
	&chick_move_start_run,
	&chick_move_start_slash,
	&chick_move_walk,
	&flipper_move_attack,
	&flipper_move_death,
	&flipper_move_pain1,
	&flipper_move_pain2,
	&flipper_move_run_loop,
	&flipper_move_run_start,
	&flipper_move_stand,
	&flipper_move_start_run,
	&flipper_move_walk,
	&floater_move_activate,
	&floater_move_attack1,
	&floater_move_attack2,
	&floater_move_attack3,
	&floater_move_death,
	&floater_move_pain1,
	&floater_move_pain2,
	&floater_move_pain3,
	&floater_move_run,
	&floater_move_stand1,
	&floater_move_stand2,
	&floater_move_walk,
	&flyer_move_attack2,
	&flyer_move_bankleft,
	&flyer_move_bankright,
	&flyer_move_defense,
	&flyer_move_end_melee,
	&flyer_move_loop_melee,
	&flyer_move_pain1,
	&flyer_move_pain2,
	&flyer_move_pain3,
	&flyer_move_rollleft,
	&flyer_move_rollright,
	&flyer_move_run,
	&flyer_move_stand,
	&flyer_move_start,
	&flyer_move_start_melee,
	&flyer_move_stop,
	&flyer_move_walk,
	&flyQbe_move_attack_charge,
	&flyQbe_move_attack_walk,
	&flyQbe_move_stand,
	&gladiator_move_attack_gun,
	&gladiator_move_attack_melee,
	&gladiator_move_death,
	&gladiator_move_pain,
	&gladiator_move_pain_air,
	&gladiator_move_run,
	&gladiator_move_stand,
	&gladiator_move_walk,
	&gunner_move_attack_chain,
	&gunner_move_attack_grenade,
	&gunner_move_death,
	&gunner_move_duck,
	&gunner_move_endfire_chain,
	&gunner_move_fidget,
	&gunner_move_fire_chain,
	&gunner_move_pain1,
	&gunner_move_pain2,
	&gunner_move_pain3,
	&gunner_move_run,
	&gunner_move_runandshoot,
	&gunner_move_stand,
	&gunner_move_walk,
	&hover_move_attack1,
	&hover_move_backward,
	&hover_move_death1,
	&hover_move_end_attack,
	&hover_move_forward,
	&hover_move_land,
	&hover_move_pain1,
	&hover_move_pain2,
	&hover_move_pain3,
	&hover_move_run,
	&hover_move_stand,
	&hover_move_start_attack,
	&hover_move_stop1,
	&hover_move_stop2,
	&hover_move_takeoff,
	&hover_move_walk,
	&infantry_move_attack1,
	&infantry_move_attack2,
	&infantry_move_death1,
	&infantry_move_death2,
	&infantry_move_death3,
	&infantry_move_duck,
	&infantry_move_fidget,
	&infantry_move_pain1,
	&infantry_move_pain2,
	&infantry_move_run,
	&infantry_move_stand,
	&infantry_move_walk,
	&insane_move_crawl,
	&insane_move_crawl_death,
	&insane_move_crawl_pain,
	&insane_move_cross,
	&insane_move_down,
	&insane_move_downtoup,
	&insane_move_jumpdown,
	&insane_move_run_insane,
	&insane_move_run_normal,
	&insane_move_runcrawl,
	&insane_move_stand_death,
	&insane_move_stand_insane,
	&insane_move_stand_normal,
	&insane_move_stand_pain,
	&insane_move_struggle_cross,
	&insane_move_uptodown,
	&insane_move_walk_insane,
	&insane_move_walk_normal,
	&jorg_move_attack1,
	&jorg_move_attack2,
	&jorg_move_death,
	&jorg_move_end_attack1,
	&jorg_move_end_walk,
	&jorg_move_pain1,
	&jorg_move_pain2,
	&jorg_move_pain3,
	&jorg_move_run,
	&jorg_move_stand,
	&jorg_move_start_attack1,
	&jorg_move_start_walk,
	&jorg_move_walk,
	&makron_move_attack3,
	&makron_move_attack4,
	&makron_move_attack5,
	&makron_move_death2,
	&makron_move_death3,
	&makron_move_pain4,
	&makron_move_pain5,
	&makron_move_pain6,
	&makron_move_run,
	&makron_move_sight,
	&makron_move_stand,
	&makron_move_walk,
	&medic_move_attackBlaster,
	&medic_move_attackCable,
	&medic_move_attackHyperBlaster,
	&medic_move_death,
	&medic_move_duck,
	&medic_move_pain1,
	&medic_move_pain2,
	&medic_move_run,
	&medic_move_stand,
	&medic_move_walk,
	&mutant_move_attack,
	&mutant_move_death1,
	&mutant_move_death2,
	&mutant_move_idle,
	&mutant_move_jump,
	&mutant_move_pain1,
	&mutant_move_pain2,
	&mutant_move_pain3,
	&mutant_move_run,
	&mutant_move_stand,
	&mutant_move_start_walk,
	&mutant_move_walk,
	&parasite_move_break,
	&parasite_move_death,
	&parasite_move_drain,
	&parasite_move_end_fidget,
	&parasite_move_fidget,
	&parasite_move_pain1,
	&parasite_move_run,
	&parasite_move_stand,
	&parasite_move_start_fidget,
	&parasite_move_start_run,
	&parasite_move_start_walk,
	&parasite_move_stop_run,
	&parasite_move_stop_walk,
	&parasite_move_walk,
	&soldier_move_attack1,
	&soldier_move_attack2,
	&soldier_move_attack3,
	&soldier_move_attack4,
	&soldier_move_attack6,
	&soldier_move_death1,
	&soldier_move_death2,
	&soldier_move_death3,
	&soldier_move_death4,
	&soldier_move_death5,
	&soldier_move_death6,
	&soldier_move_duck,
	&soldier_move_pain1,
	&soldier_move_pain2,
	&soldier_move_pain3,
	&soldier_move_pain4,
	&soldier_move_run,
	&soldier_move_stand1,
	&soldier_move_stand3,
	&soldier_move_start_run,
	&soldier_move_walk1,
	&soldier_move_walk2,
	&supertank_move_attack1,
	&supertank_move_attack2,
	&supertank_move_attack3,
	&supertank_move_attack4,
	&supertank_move_backward,
	&supertank_move_death,
	&supertank_move_end_attack1,
	&supertank_move_forward,
	&supertank_move_pain1,
	&supertank_move_pain2,
	&supertank_move_pain3,
	&supertank_move_run,
	&supertank_move_stand,
	&supertank_move_turn_left,
	&supertank_move_turn_right,
	&tank_move_attack_blast,
	&tank_move_attack_chain,
	&tank_move_attack_fire_rocket,
	&tank_move_attack_post_blast,
	&tank_move_attack_post_rocket,
	&tank_move_attack_pre_rocket,
	&tank_move_attack_strike,
	&tank_move_death,
	&tank_move_pain1,
	&tank_move_pain2,
	&tank_move_pain3,
	&tank_move_reattack_blast,
	&tank_move_run,
	&tank_move_stand,
	&tank_move_start_run,
	&tank_move_start_walk,
	&tank_move_stop_run,
	&tank_move_stop_walk,
	&tank_move_walk
};

int MmoveToIndex(void *func)
{
	int i;
	int size = sizeof(savemmoves)/sizeof(void*);
	for (i=0; i<size; i++)
		if (savemmoves[i] == func)
			return i;
	gi.error("MmoveToIndex: unknown mmove\n");
	return 0;		// shut up compiler :(
}


void *IndexToMmove(int index)
{
	int size = sizeof(savemmoves)/sizeof(void*);
	if (index < size)
		return savemmoves[index];
	gi.error("IndexToMmove: unknown index\n");
	return NULL;		// shut up compiler :(
}
