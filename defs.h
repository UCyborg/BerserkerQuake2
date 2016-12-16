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

#ifdef __linux__
#define _GNU_SOURCE
#endif
#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#include <io.h>
#else
#include <errno.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pwd.h>
#include <sched.h>
#endif
#include <time.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <GL/gl.h>
#include <GL/glext.h>
#include <png.h>
#include <SDL2/SDL.h>
#include "unpak.h"
#include "m_frames.h"
#include <jpeglib.h>
#include <vorbis/vorbisfile.h>
#include "compat.h"

#define GL_ANY_SAMPLES_PASSED	0x8C2F

#define	MAX_MODELVIEW_CACHE	4096	// Количество полупрозрачных полигонов браш-моделей карты, которые двигаются (не больше 65536 !!!)

#define	FORCE_DYNAMICCONE			// Будем считать вращающиеся статичные источники света с _cone - динамичными! WHAT FASTER???
#define FLARELIGHT_RADIUS	1024	// Расстояние действия фонарика


#define Q2toBERS_Detalization	1		// 2  Фтоппку нах!  Пусть у новых текстур будет обычная детализация, надоели неудобства с маппингом в радианте!

#define	FPS_UPDATE_TIME		0.5f		// Обновлять показания FPS каждые 0.5 секунды
#define	FPS_UPDATE_FREQ		2			//		1 / FPS_UPDATE_TIME

#define	VEL_UPDATE_TIME		0.1f		// Обновлять показания VELOCITY каждую 0.1 секунды


#define FP_BITS(fp) (* (DWORD *) &(fp))

typedef union FastSqrtUnion
{
	float f;
	unsigned i;
} FastSqrtUnion;


// angle indexes
#define	PITCH				0		// up / down
#define	YAW					1		// left / right
#define	ROLL				2		// fall over

#ifndef M_PI
#define M_PI		3.14159265358979323846
#endif

typedef struct sizebuf_s
{
	bool	allowoverflow;	// if false, do a Com_Error
	bool	overflowed;		// set to true if the buffer size failed
	byte	*data;
	int		maxsize;
	int		cursize;
	int		readcount;
} sizebuf_t;

typedef void (*xcommand_t) (void);

typedef struct cmd_function_s
{
	struct cmd_function_s	*next;
	char					*name;
	unsigned				hash;
	xcommand_t				function;
} cmd_function_t;

typedef struct cvar_s
{
	char		*name;
	char		*string;
	char		*latched_string;	// for CVAR_LATCH vars
	char		*vid_latched_string;// for CVAR_VID_LATCH vars
	int			flags;
	bool		modified;			// set each time the cvar is changed
	float		value;
	struct cvar_s *next;
	char		*defaultString;
	bool		need_send;
	unsigned	hash;
	const char	*help;		// r1ch
} cvar_t;



#define	MAX_QPATH			64		// max length of a berserker game pathname

#ifdef _WIN32
#define MAX_OSPATH			256		// max length of a filesystem pathname (same as MAX_PATH)
#else
#define MAX_OSPATH			4096	// max length of a filesystem pathname
#endif

typedef struct
{
	char		name[MAX_QPATH];
	unsigned	hash;
	int			filepos, filelen;
} packfile_t;

typedef struct pack_s
{
	char	filename[MAX_OSPATH];
	FILE	*handle;
	bool	isPK2;
	// for PAK only
	int			numfiles;
	packfile_t	*files;
} pack_t;

typedef struct searchpath_s
{
	char	filename[MAX_OSPATH];
	pack_t	*pack;		// only one of filename / pack will be used
	bool	disabled;
	struct searchpath_s *next;
} searchpath_t;


/*
========================================================================
The .pak files are just a linear collapse of a directory tree
========================================================================
*/

#define IDPAKHEADER		(('K'<<24)+('C'<<16)+('A'<<8)+'P')

typedef struct
{
	char	name[56];
	int		filepos, filelen;
} dpackfile_t;

typedef struct
{
	int		ident;		// == IDPAKHEADER
	int		dirofs;
	int		dirlen;
} dpackheader_t;

#define	MAX_FILES_IN_PACK	4096



typedef struct edict_s edict_t;


#define	MAX_STRING_CHARS	2048	//1024	// max length of a string passed to Cmd_TokenizeString
/// Fixed bug: обрезание строки при парсинге...
#define	MAX_STRING_TOKENS	512		/// 80		// max tokens resulting from Cmd_TokenizeString
#define	MAX_TOKEN_CHARS		512		///128		// max length of an individual token

#define	MAX_NUM_ARGVS	128

#define	MAXCMDLINE	256

typedef enum keynum_e
{
	//
	// these are the key numbers that should be passed to Key_Event
	//
	K_TAB			= 9,
	K_ENTER			= 13,
	K_ESCAPE		= 27,
	K_SPACE			= 32,

	// normal keys should be passed as lowercased ascii
	K_BACKSPACE		= 127,
	K_UPARROW,
	K_DOWNARROW,
	K_LEFTARROW,
	K_RIGHTARROW,

	K_ALT,
	K_CTRL,
	K_SHIFT,
	K_F1,
	K_F2,
	K_F3,
	K_F4,
	K_F5,
	K_F6,
	K_F7,
	K_F8,
	K_F9,
	K_F10,
	K_F11,
	K_F12,
	K_INS,
	K_DEL,
	K_PGDN,
	K_PGUP,
	K_HOME,
	K_END,

	K_KP_HOME		= 160,
	K_KP_UPARROW,
	K_KP_PGUP,
	K_KP_LEFTARROW,
	K_KP_5,
	K_KP_RIGHTARROW,
	K_KP_END,
	K_KP_DOWNARROW,
	K_KP_PGDN,
	K_KP_ENTER,
	K_KP_INS,
	K_KP_DEL,
	K_KP_SLASH,
	K_KP_MINUS,
	K_KP_PLUS,

	//
	// mouse buttons generate virtual keys
	//
	K_MOUSE1		= 200,
	K_MOUSE3,
	K_MOUSE2,
	K_MOUSE4,
	K_MOUSE5,
	K_MOUSE6,
	K_MOUSE7,
	K_MOUSE8,
	K_MOUSE9,
	K_MOUSE10,
	K_MOUSE11,
	K_MOUSE12,
	K_MOUSE13,
	K_MOUSE14,
	K_MOUSE15,
	K_MOUSE16,

	//
	// joystick buttons
	//
	K_JOY1,
	K_JOY2,
	K_JOY3,
	K_JOY4,

	//
	// aux keys are for multi-buttoned joysticks to generate so they can use
	// the normal binding process
	//
	K_AUX1,
	K_AUX2,
	K_AUX3,
	K_AUX4,
	K_AUX5,
	K_AUX6,
	K_AUX7,
	K_AUX8,
	K_AUX9,
	K_AUX10,
	K_AUX11,
	K_AUX12,
	K_AUX13,
	K_AUX14,
	K_AUX15,
	K_AUX16,
	K_AUX17,
	K_AUX18,
	K_AUX19,
	K_AUX20,
	K_AUX21,
	K_AUX22,
	K_AUX23,
	K_AUX24,
	K_AUX25,
	K_AUX26,
	K_AUX27,
	K_AUX28,
	K_AUX29,
	K_AUX30,
	K_AUX31,
	K_AUX32,

	K_MWHEELDOWN,
	K_MWHEELUP,

	K_PAUSE			= 255
} keynum_t;


typedef struct
{
	char	*name;
	int		keynum;
} keyname_t;


typedef enum
{
	ca_uninitialized,
	ca_disconnected, 	// not talking to a server
	ca_connecting,		// sending request packets to the server
	ca_connected,		// netchan_t established, waiting for svc_serverdata
	ca_active			// game views should be displayed
} connstate_t;

typedef enum {key_game, key_console, key_message, key_menu} keydest_t;

#define	PORT_ANY	-1

#define	MAX_MSGLEN		1400		// max length of a message
#define	PACKET_HEADER	10			// two ints and a short

typedef enum {NA_LOOPBACK, NA_BROADCAST, NA_IP} netadrtype_t;

typedef enum {NS_CLIENT, NS_SERVER} netsrc_t;

typedef struct
{
	netadrtype_t	type;
	byte	ip[4];
	WORD	port;
} netadr_t;

typedef struct
{
	bool	fatal_error;

	netsrc_t	sock;

	int			dropped;			// between last packet and previous

	int			last_received;		// for timeouts
	int			last_sent;			// for retransmits

	netadr_t	remote_address;
	int			qport;				// qport value to write when transmitting

// sequencing variables
	int			incoming_sequence;
	int			incoming_acknowledged;
	int			incoming_reliable_acknowledged;	// single bit

	int			incoming_reliable_sequence;		// single bit, maintained local

	int			outgoing_sequence;
	int			reliable_sequence;			// single bit
	int			last_reliable_sequence;		// sequence number of last send

// reliable staging and holding areas
	sizebuf_t	message;		// writing buffer to send to server
	byte		message_buf[MAX_MSGLEN-16];		// leave space for header

// message is copied to this buffer when it is first transfered
	int			reliable_length;
	byte		reliable_buf[MAX_MSGLEN-16];	// unacked reliable message
} netchan_t;


typedef enum {
	dl_none,
	dl_model,
	dl_sound,
	dl_skin,
	dl_single
} dltype_t;		// download type

typedef struct
{
	connstate_t	state;
	keydest_t	key_dest;

///	int			framecount;		// not used?
	int			realtime;			// always increasing, no clamping, etc
	float		netFrameTime;		// seconds since last packet frame
	float		renderFrameTime;	// seconds since last refresh frame

// screen rendering information
	float		disable_screen;		// showing loading plaque between levels
									// if time gets > 30 seconds ahead, break it
	int			disable_servercount;	// when we receive a frame and cl.servercount
									// > cls.disable_servercount, clear disable_screen

// connection information
	char		servername[MAX_OSPATH];	// name of server from original connect
	float		connect_time;		// for connection retransmits

	int			port;				// a 16 bit value that allows berserker servers
									// to work around address translating routers
	netchan_t	netchan;
	int			serverProtocol;		// in case we are doing some kind of version hack

	int			challenge;			// from the server to use for connecting

	bool		forcePacket;		// forces a packet to be sent the next frame

	FILE		*download;			// file transfer from server
	char		downloadtempname[MAX_OSPATH];
	char		downloadname[MAX_OSPATH];
///	int			downloadnumber;
	dltype_t	downloadtype;
	int			downloadpercent;

// demo recording info must be here, so it isn't cleared on level change
	bool	demorecording;
	bool	demowaiting;	// don't record until a non-delta message is received
	FILE	*demofile;
	int		demolength;
} client_static_t;


#define	ALIAS_LOOP_COUNT	16

#define	MAX_ALIAS_NAME	32

typedef struct cmdalias_s
{
	struct cmdalias_s	*next;
	char				name[MAX_ALIAS_NAME];
	char				*value;
	unsigned			hash;
} cmdalias_t;

// this is set each time a CVAR_USERINFO variable is changed
// so that the client knows to send it to the server
#define	CVAR_ARCHIVE	1	// set to cause it to be saved to vars.rc
#define	CVAR_USERINFO	2	// added to userinfo  when changed
#define	CVAR_SERVERINFO	4	// added to serverinfo when changed
#define	CVAR_NOSET		8	// don't allow change from console at all,
							// but can be set from the command line
#define	CVAR_LATCH		16	// save changes until server restart
#define	CVAR_VID_LATCH	32	// save changes until videosystem restart

#define	WINDOW_CLASS_NAME	"BerserkerQ2"
#define	BASEDIRNAME		"baseq2"

#define	NUM_CON_TIMES 4

#define		CON_TEXTSIZE	32768

#define Q_COLOR_ESCAPE	'^'
#define Q_IsColorString(p)	( p && *(p) == Q_COLOR_ESCAPE && *((p)+1) && *((p)+1) != Q_COLOR_ESCAPE )

#define COLOR_BLACK		'0'
#define COLOR_RED		'1'
#define COLOR_GREEN		'2'
#define COLOR_YELLOW	'3'
#define COLOR_BLUE		'4'
#define COLOR_CYAN		'5'
#define COLOR_MAGENTA	'6'
#define COLOR_WHITE		'7'
#define ColorIndex(c)	( ( (c) - '0' ) & 7 )

typedef struct
{
	bool	initialized;
	char	text[CON_TEXTSIZE];
	char	color[CON_TEXTSIZE];	// text color
	int		current;		// line where next message will be printed
	int		x;				// offset in current line for next print
	int		display;		// bottom of console displays this line
///	int		ormask;			// high bit mask for colored characters
	int 	linewidth;		// characters across screen
	int		totallines;		// total lines in console scrollback
	float	cursorspeed;
	int		vislines;
	float	times[NUM_CON_TIMES];	// cls.realtime time the line was generated
								// for transparent notify lines
} console_t;


#define	MAX_VERTEX_CACHES	4096

typedef enum {
	VBO_STATIC,
	VBO_DYNAMIC
} vertCacheMode_t;

typedef enum {
	VBO_STORE_ANY,
	VBO_STORE_XYZ,
	VBO_STORE_NORMAL,
	VBO_STORE_BINORMAL,
	VBO_STORE_TANGENT
} vertStoreMode_t;

typedef struct vertCache_s
{
	struct vertCache_s	*prev;
	struct vertCache_s	*next;

	vertCacheMode_t		mode;

	int					size;

	void				*pointer;

	vertStoreMode_t		store;
	struct model_s		*mod;
	int					frame;
	int					oldframe;
	float				backlerp;
	float				angles[3];
	float				origin[3];
	int					mesh;		// 0 for md2

	unsigned			id;
} vertCache_t;

typedef struct {
	vertCache_t		*freeVertCache;
	vertCache_t		activeVertCache;
	vertCache_t		vertCacheList[MAX_VERTEX_CACHES];
} vertCacheManager_t;

#define MD3_ALIAS_VERSION	15
///#define MD3_ALIAS_MAX_LODS	4

#define	MD3_MAX_TRIANGLES	8192	// per mesh
#define MD3_MAX_VERTS		4096	// per mesh
//#define MD3_MAX_SHADERS		256		// per mesh
#define MD3_MAX_FRAMES		1024	// per model
#define	MD3_MAX_MESHES		32		// per model
#define MD3_MAX_TAGS		16		// per frame
#define MD3_MAX_PATH		MAX_QPATH	// 64
#define	MD3_MAX_SKINS		32		// per mesh

typedef float vec3_t[3];
typedef float vec4_t[4];

#define	MAX_MAP_LEAFS		65536
typedef struct entity_s
{
	struct model_s		*model;			// opaque type outside refresh
	float				angles[3];

	/*
	** most recent data
	*/
	float				origin[3];		// also used as RF_BEAM's "from"
	int					frame;			// also used as RF_BEAM's diameter

	/*
	** previous data for lerping
	*/
	float				oldorigin[3];	// also used as RF_BEAM's "to"
	int					oldframe;

	/*
	** misc
	*/
	float	backlerp;				// 0.0 = current, 1.0 = old
	int		skinnum;				// also used as RF_BEAM's palette index

	float	alpha;					// ignore if RF_TRANSLUCENT isn't set

	struct image_s	*skin;			// NULL for inline skin
	struct image_s	*bump;
	struct image_s	*light;
	vertCache_t		*vbo_xyz[MD3_MAX_MESHES];
///	vertCache_t		*vbo_lightp[MD3_MAX_MESHES];
///	vertCache_t		*vbo_tsh[MD3_MAX_MESHES];
///	vertCache_t		*vbo_fx[MD3_MAX_MESHES];
	vertCache_t		*vbo_normals[MD3_MAX_MESHES];
	vertCache_t		*vbo_tangents[MD3_MAX_MESHES];
	vertCache_t		*vbo_binormals[MD3_MAX_MESHES];
	int		flags;
	int		ownerkey;
	float	minmax[6];				// bbox for current frame of alias model (in world coordinates!)
	int		framecount;				// для кэширования расчетов vis
	byte	vis[MAX_MAP_LEAFS/8];
	vec3_t	color_shell;
	float	dist;					// для сортировки по дальности от источника света (для r_noselfshadow=2)
									// и по дальности от камеры (для ambient render)
	bool	minmax_ready;			// true - frustrum's minmax вычислены
	bool	sky_model;
	struct alink_s	*alink;
} entity_t;


typedef enum
{
	part_simple,		// Никаких эффектов, old style particles
	part_blood,			// Партикли марают стены кровью при прикосновении
	part_smoke,			// Партикли при попадании в жидкость заменяются пузырями (не применять для падающих партиклей!!!)
	part_bigsmoke,		// то же самое, что и part_smoke (только покрупнее)
	part_splash,		// Партикли-брызги: при падении в жидкость - изчезают.
	part_bubble,		// Партикли-пузыри; при попадании в воздух - изчезают
	part_fast_blood,	// Партикли марают стены кровью при прикосновении (и быстро сохнут)
part_end_of_list_for_emitters,		// ****** после этого типа НЕЛЬЗЯ использовать в эмиттерах ********
	part_fly,
	part_beam,
	part_rail_beam,
	part_spiral,
	part_wake,
	part_plume,
	part_tracer,
	part_sparks,
	part_greenblood
} parttype_t;


typedef struct
{
	vec3_t			origin;
	byte			color_r;
	byte			color_g;
	byte			color_b;
	parttype_t		type;
	float			alpha;
	vec3_t			length;		// for part_rail_beam / part_beam / part_spiral
	int				size;		// for part_rail_beam / part_beam / part_spiral
} particle_t;



#define	ICON_WIDTH	24
#define	ICON_HEIGHT	24
#define	CHAR_WIDTH	16
#define	ICON_SPACE	8
#define STAT_MINUS		10	// num frame for '-' stats digit

typedef enum {rend_all, rend_mirror, rend_no_mirror} rend_t;

typedef struct
{
	vec3_t		origin;
	vec3_t		color;
	float/*vec3_t*/		radius;			// динамический свет всегда шаровидный
	int			ownerkey;
	int			filtercube_start;
	int			filtercube_end;
	float		framerate;
	float		_cone;
	vec3_t		angles;
	byte		style;
	byte		cl_style;				// локальный лайт-стиль (для создания мигающих лайтов, которые можно выключать) - итог равен умножению style на cl_style
	rend_t		render_to;
} dlight_t;

///#define	DLIGHT_CUTOFF	16	///64		/// FIXME: Make configurable...


typedef struct
{
	float		rgb[3];			// 0.0 - 2.0
	float		white;			// highest of rgb
} lightstyle_t;


typedef struct
{
	int			x, y, width, height;// in virtual screen coordinates
	float		fov_x, fov_y;
	float		vieworg[3];
	float		viewangles[3];
	float		blend[4];			// rgba 0-1 full screen blend
	float		time;				// time is uesed to auto animate
	int			rdflags;			// RDF_UNDERWATER, etc

	byte		*areabits;			// if not NULL, only areas with set bits will be drawn

	int			num_entities;
	entity_t	*entities;

	int			num_dlights;
	dlight_t	*dlights;

	int			num_particles;
	particle_t	*particles;
} refdef_t;

#define	ERR_FATAL			0		// exit the entire game with a popup window
#define	ERR_DROP			1		// print to console and disconnect from game
#define	ERR_DISCONNECT		2		// don't kill server

#define	CMD_BACKUP		64			// allow a lot of command backups for very fast systems

#define DEG2RAD( a ) ( a * M_PI ) / 180.0F
#define RAD2DEG( a ) ( ( (a) * 180.0f ) / M_PI )

// pmove_state_t is the information necessary for client side movement
// prediction
typedef enum
{
	// can accelerate and turn
	PM_NORMAL,
	PM_SPECTATOR,
	// no acceleration or turning
	PM_DEAD,
	PM_GIB,		// different bounding box
	PM_FREEZE
} pmtype_t;

// this structure needs to be communicated bit-accurate
// from the server to the client to guarantee that
// prediction stays in sync, so no floats are used.
// if any part of the game code modifies this struct, it
// will result in a prediction error of some degree.
typedef struct
{
	pmtype_t	pm_type;
///	short		origin[3];		// 12.3
	int			origin[3];		// 20.3	(LARGE_MAP_SIZE support)
	short		velocity[3];	// 12.3
	byte		pm_flags;		// ducked, jump_held, etc
	byte		pm_time;		// each unit = 8 ms
	short		gravity;
	short		delta_angles[3];	// add to command angles to get view direction
									// changed by spawns, rotating objects, and teleporters
} pmove_state_t;

// LARGE_MAP_SIZE support
#define BIT_23	0x00800000
#define UPRBITS	0xFF000000

// player_state->stats[] indexes
#define STAT_HEALTH_ICON		0
#define	STAT_HEALTH				1
#define	STAT_AMMO_ICON			2
#define	STAT_AMMO				3
#define	STAT_ARMOR_ICON			4
#define	STAT_ARMOR				5
#define	STAT_SELECTED_ICON		6
#define	STAT_PICKUP_ICON		7
#define	STAT_PICKUP_STRING		8
#define	STAT_TIMER_ICON			9
#define	STAT_TIMER				10
#define	STAT_HELPICON			11
#define	STAT_SELECTED_ITEM		12
#define	STAT_LAYOUTS			13
#define	STAT_FRAGS				14
#define	STAT_FLASHES			15		// cleared each frame, 1 = health, 2 = armor
#define STAT_CHASE				16
#define STAT_SPECTATOR			17

#define	MAX_STATS				32
#define	UPDATE_BACKUP			16	// copies of entity_state_t to keep buffered
									// must be power of two
#define	UPDATE_MASK		(UPDATE_BACKUP-1)

// player_state_t is the information needed in addition to pmove_state_t
// to rendered a view.  There will only be 10 player_state_t sent each second,
// but the number of pmove_state_t changes will be reletive to client
// frame rates
typedef struct
{
	pmove_state_t	pmove;		// for prediction

	// these fields do not need to be communicated bit-precise

	vec3_t		viewangles;		// for fixed views
	vec3_t		viewoffset;		// add to pmovestate->origin
	vec3_t		kick_angles;	// add to view direction to get render angles
								// set by weapon kicks, pain effects, etc

	vec3_t		gunangles;
	vec3_t		gunoffset;
	int			gunindex;
	int			gunframe;

	float		oldviewangle;	// Berserker: for detecting client's rotate

	float		blend[4];		// rgba full screen effect

	float		fov;			// horizontal field of view

	int			rdflags;		// refdef flags

	short		stats[MAX_STATS];		// fast status bar updates
} player_state_t;

#define	MAX_MAP_AREAS		256

typedef struct
{
	bool			valid;			// cleared if delta parsing was invalid
	int				serverframe;
	int				servertime;		// server time the message is valid for (in msec)
	int				deltaframe;
	byte			areabits[MAX_MAP_AREAS/8];		// portalarea visibility bits
	player_state_t	playerstate;
	int				num_entities;
	int				parse_entities;		// index (not anded off) into cl_parse_entities[]
} frame_t;

#define	MAX_PARSE_ENTITIES	1024
#define MAX_CLIENTWEAPONMODELS		16

typedef struct
{
	char	name[MAX_QPATH];
	char	cinfo[MAX_QPATH];
	struct	image_s	*skin;
	struct	image_s	*bump;
	struct	image_s	*light;
	struct	image_s	*icon;
	char	iconname[MAX_QPATH];
	struct	model_s	*model;
	struct	model_s	*weaponmodel[MAX_CLIENTWEAPONMODELS];
} clientinfo_t;


//
// per-level limits
//
#define	MAX_CLIENTS			256		// absolute limit
#define MAX_GENERAL			(MAX_CLIENTS*2)	// general config strings
#define	MAX_EDICTS			1024	// must change protocol to increase more
#define	MAX_ITEMS			256
#define	MAX_LIGHTSTYLES		256
// for Q2 protocol
#define	MAX_MODELS_Q2		256		// these are sent over the net as bytes
#define	MAX_SOUNDS_Q2		256		// so they cannot be blindly increased
#define	MAX_IMAGES_Q2		256
// for Berserker protocol
#define	MAX_MODELS_BERS		1024	// можно легко увеличивать до 65536, используются 16-бит
#define	MAX_SOUNDS_BERS		2048
#define	MAX_IMAGES_BERS		1024

//
// config strings are a general means of communication from
// the server to all connected clients.
// Each config string can be at most MAX_QPATH characters.
//
#define	CS_NAME				0
///#define	CS_CDTRACK			1
	#define	CS_TRACK_AMBIENT	1	// В режиме совместимости равен:	[CD Track]
									// В режиме BersQ2 протокола равен:	[CD Track] [Ambient_R Ambient_G Ambient_B]
#define	CS_SKY				2
#define	CS_SKYAXIS			3		// %f %f %f format
#define	CS_SKYROTATE		4
#define	CS_STATUSBAR		5		// display program string

#define CS_AIRACCEL			29		// air acceleration control
#define	CS_MAXCLIENTS		30
#define	CS_MAPCHECKSUM		31		// for catching cheater maps

#define	CS_MODELS			32
// for Q2 protocol
#define	CS_SOUNDS_Q2			(CS_MODELS+MAX_MODELS_Q2)
#define	CS_IMAGES_Q2			(CS_SOUNDS_Q2+MAX_SOUNDS_Q2)
#define	CS_LIGHTS_Q2			(CS_IMAGES_Q2+MAX_IMAGES_Q2)
#define	CS_ITEMS_Q2				(CS_LIGHTS_Q2+MAX_LIGHTSTYLES)
#define	CS_PLAYERSKINS_Q2		(CS_ITEMS_Q2+MAX_ITEMS)
#define CS_GENERAL_Q2			(CS_PLAYERSKINS_Q2+MAX_CLIENTS)
#define	MAX_CONFIGSTRINGS_Q2	(CS_GENERAL_Q2+MAX_GENERAL)
#define ENV_CNT_Q2 (CS_PLAYERSKINS_Q2 + MAX_CLIENTS * PLAYER_MULT)	// ENV_CNT is map load, ENV_CNT+1 is first env map
#define TEXTURE_CNT_Q2 (ENV_CNT_Q2 + 6 + 1)	// 6 SKY textures
// for Berserker protocol
#define	CS_SOUNDS_BERS			(CS_MODELS+MAX_MODELS_BERS)
#define	CS_IMAGES_BERS			(CS_SOUNDS_BERS+MAX_SOUNDS_BERS)
#define	CS_LIGHTS_BERS			(CS_IMAGES_BERS+MAX_IMAGES_BERS)
#define	CS_ITEMS_BERS			(CS_LIGHTS_BERS+MAX_LIGHTSTYLES)
#define	CS_PLAYERSKINS_BERS		(CS_ITEMS_BERS+MAX_ITEMS)
#define CS_GENERAL_BERS			(CS_PLAYERSKINS_BERS+MAX_CLIENTS)
#define	MAX_CONFIGSTRINGS_BERS	(CS_GENERAL_BERS+MAX_GENERAL)
#define ENV_CNT_BERS (CS_PLAYERSKINS_BERS + MAX_CLIENTS * PLAYER_MULT)	// ENV_CNT is map load, ENV_CNT+1 is first env map
#define TEXTURE_CNT_BERS (ENV_CNT_BERS + 6 + 1)	// 6 SKY textures

#define PLAYER_MULT 5


typedef struct
{
	int		length;
	float	val;			///value[3];	Berserker: в Ку2 не используются цветные лайтстили... упраздним вектор
	float	map[MAX_QPATH];
} clightstyle_t;


// usercmd_t is sent to the server each client frame
typedef struct usercmd_s
{
	byte	msec;
	byte	buttons;
	short	angles[3];
	short	forwardmove, sidemove, upmove;
	byte	impulse;		// remove?
	byte	lightlevel;
} usercmd_t;


#define	MAX_DLIGHTS		128	/// 32 - тупое ограничение убрано!
#define	MAX_CLENTITIES	1024	/// 256	///128
#define	MAX_ENTITIES	MAX_CLENTITIES + 128
#define	MAX_PARTICLES	8192	/// 4096
#define	MAX_LIGHTSTYLES_OVERRIDE	32

#define MAX_DECAL_VERTICES	128
#define MAX_DECAL_TRIANGLES	64
#define MAX_DECALS			4096


typedef struct
{
	int			timeoutcount;

	int			timedemo_frames;
	int			timedemo_start;

	bool		refresh_prepped;	// false if on new level or new ref dll
	bool		sound_prepped;		// ambient sounds can start
	bool		force_refdef;		// vid has changed, so we can't use a paused refdef

	int			parse_entities;		// index (not anded off) into cl_parse_entities[]

	usercmd_t	cmd;
	usercmd_t	cmds[CMD_BACKUP];	// each mesage will send several old cmds
	int			cmd_time[CMD_BACKUP];	// time sent, for calculating pings
//	short		predicted_origins[CMD_BACKUP][3];	// for debug comparing against server
	int			predicted_origins[CMD_BACKUP][3];	// for debug comparing against server (LARGE_MAP_SIZE support)

	float		predicted_step;				// for stair up smoothing
	unsigned	predicted_step_time;

	vec3_t		predicted_origin;	// generated by CL_PredictMovement
	vec3_t		predicted_angles;
	vec3_t		prediction_error;

	frame_t		frame;				// received from server
	int			surpressCount;		// number of messages rate supressed
	frame_t		frames[UPDATE_BACKUP];

	// the client maintains its own idea of view angles, which are
	// sent to the server each frame.  It is cleared to 0 upon entering each level.
	// the server sends a delta each frame which is added to the locally
	// tracked view angles to account for standing on rotating objects,
	// and teleport direction changes
	vec3_t		viewangles;

	int			time;			// this is the time value that the client
								// is rendering at.  always <= cls.realtime
	int			initial_server_frame;

	float		lerpfrac;		// between oldframe and frame

	refdef_t	refdef;

	vec3_t		v_forward, v_right, v_up;	// set when refdef.angles is set

	//
	// transient data from server
	//
	char		layout[1024];		// general 2D overlay
	int			inventory[MAX_ITEMS];

	//
	// non-gameserver infornamtion
	// FIXME: move this cinematic stuff into the cin_t structure
	FILE		*cinematic_file;
	int			cinematictime;		// cls.realtime for first cinematic frame
	int			cinematicframe;
	char		cinematicpalette[768];
	bool		cinematicpalette_active;
	bool		need_palette;		// berserker: for PCX images only

	// server state information
	bool		attractloop;		// running the attract loop, any key will menu
	int			servercount;		// server identification for prespawns
	char		gamedir[MAX_QPATH];
	int			playernum;

	char		configstrings[MAX_CONFIGSTRINGS_BERS][MAX_QPATH];

	//
	// locally derived information from server state
	//
	struct model_s	*model_draw[MAX_MODELS_BERS];
	struct cmodel_s	*model_clip[MAX_MODELS_BERS];

	struct sfx_s	*sound_precache[MAX_SOUNDS_BERS];
	struct image_s	*image_precache[MAX_IMAGES_BERS];

	clientinfo_t	clientinfo[MAX_CLIENTS];
	clientinfo_t	baseclientinfo;

	float		leveltime;			// gameTime * 1000
	int			gameTime;
///	float		new_leveltime;		/// from game.dll
///	float		old_leveltime;
} client_state_t;

typedef enum
{
	cs_free,		// can be reused for a new connection
	cs_zombie,		// client has been disconnected, but don't reuse
					// connection for a couple seconds
	cs_connected,	// has been assigned to a client_t, but not in game yet
	cs_spawned		// client is fully in game
} client_state_te;

#define VectorSubtract(a,b,c)	(c[0]=a[0]-b[0],c[1]=a[1]-b[1],c[2]=a[2]-b[2])
#define VectorAdd(a,b,c)		(c[0]=a[0]+b[0],c[1]=a[1]+b[1],c[2]=a[2]+b[2])
#define VectorCopy(a,b)			(b[0]=a[0],b[1]=a[1],b[2]=a[2])
#define VectorClear(a)			(a[0]=a[1]=a[2]=0)
#define VectorNegate(a,b)		(b[0]=-a[0],b[1]=-a[1],b[2]=-a[2])
#define VectorSet(v, x, y, z)	(v[0]=(x), v[1]=(y), v[2]=(z))


#define	MAXPRINTMSG			4096

///#define	PRINT_ALL			0
///#define	PRINT_DEVELOPER		1		// only print when "developer 1"
///#define	PRINT_ALERT			2

typedef struct
{
	int	width, height;			// coordinates from main game
} viddef_t;

typedef struct
{
	char *name;
	int	minimize, maximize;
} glmode_t;

#define NUM_GL_MODES (sizeof(modes) / sizeof (glmode_t))

typedef enum
{
	it_skin,
	it_sprite,		// GL_CLAMP
	it_wall,
	it_pic,			// GL_CLAMP
	it_sky,			// GL_CLAMP
	it_bump,
	it_light,
	it_fx		// как it_wall, но не выгружается! резидент!
} imagetype_t;

typedef enum
{
	file_nul,
	file_wal,
	file_pcx,
	file_tga,
	file_jpg,
	file_png,
	file_dds
} filetype_t;

typedef enum
{
	fx_none,
	fx_chrome,
	fx_power,
	fx_map,
	fx_distort,
	fx_style
} fx_t;

typedef struct image_s
{
	char			name[MAX_QPATH];			// game path, including extension
	unsigned		hash;
	imagetype_t		type;
	filetype_t		filetype;
	int				width, height;				// source image
	int				upload_width, upload_height;	// after power of two and picmip
	int				registration_sequence;		// 0 = free
	struct msurface_s	*texturechain;	// for sort-by-texture world drawing
	unsigned		texnum;						// gl texture binding
///	float			sl, tl, sh, th;				// 0,0 - 1,1 unless part of the scrap
///	bool			scrap;
	bool			has_alpha;
///	bool			paletted;
	vec3_t			color;						// средний цвет текстуры
	fx_t			fx;
	float			fx_s;
	float			fx_t;
	float			fx_scale_s;
	float			fx_scale_t;
	int				fx_original_size_s;
	int				fx_original_size_t;

	struct image_s	*fx_image;
	char			fx_image_name[MAX_QPATH];
	// for detailed bumpmap
	struct image_s	*fx_detail_image;
	char			fx_detail_image_name[MAX_QPATH];
	float			detail_scale;
	bool			NoDetailBump;
	bool			AlphaTest;		// Если есть альфа-канал, считаем что он для "дыр"
	bool			CastShadow;		// Если есть альфа-канал, форсирует тень.
	bool			Parallax;		// Если есть альфа-канал, считаем что он для HeightMap
	bool			Translucent33;	// Если есть альфа-канал, считаем что он для "полупрозрачности 33%"
	bool			Translucent66;	// Если есть альфа-канал, считаем что он для "полупрозрачности 66%"

	struct material_s *material;					// NULL - old algorithms
	bool			mirror;
	float			scale_s;		// для кэширования результатов FS_GetWalSizes
	float			scale_t;		// для кэширования результатов FS_GetWalSizes
///	int				framecount;		// для сортировки текстур
	float			rotate;			// для вращения текстур
} image_t;

#define	MAX_MATERIAL_HITS	4
typedef struct material_s
{
	char	name[MAX_QPATH];			// game path, including extension
	unsigned		hash;
	int		registration_sequence;		// 0 = free
	char	decal_name[MAX_QPATH];		// для отложенной загрузки картинки декаля
	char	debris_name[MAX_QPATH];		// для отложенной загрузки картинки дебриса
	image_t	*decal;						// картинка декаля при попадании в материал заряда
	struct  model_s	*debris;			// модель дебриса, которая выбрасывается при попадании в материал заряда
	char	footstep_name[MAX_QPATH];	// для отложенной загрузки звука шагов при прохождении по материалу
	char	hs_bullet_name[MAX_QPATH];	// для отложенной загрузки звука при попадании пули по материалу
	char	hs_energy_name[MAX_QPATH];	// для отложенной загрузки звука при попадании энергии по материалу
	struct  sfx_s	*footstep;			// звук шага при прохождении по материалу
	struct  sfx_s	*hs_bullet;			// звук при попадании пули по материалу
	struct  sfx_s	*hs_energy;			// звук при попадании энергии по материалу
	bool	add_blend;					// false = mul, true = add
	int		framecount;					// Для предотвращения множественного озвучивания материала при попадании заряда
	byte	count;						// для подсчёта максимального кол-ва озвучивания материала при попадании заряда
} material_t;


typedef struct
{
	const char	*renderer_string;
	const char	*vendor_string;
	const char	*version_string;
	const char	*extensions_string;
//	int			features;
	int			tmus;
	int			maxTextureCoords;		/// for ARBfp1.0
	int			maxTextureImageUnits;	/// for ARBfp1.0
	float		max_anisotropy;
	float		anisotropy_level;
	bool		nv_meminfo;
	bool		ati_meminfo;
	bool		nv_stencil_two_side;
	bool		ati_separate_stencil;
	bool		lod_bias;
	bool		anisotropic;
	bool		compression;
	bool		s3_compression;
	bool		screentexture;		// признак работоспособности механизма снятия экран.буфера в текстуру
	bool		arb_distort;		// признак того, что ARB-шейдер дисторта валидный и загружен успешно
	bool		texshaders;
	bool		vbo;
	bool		vp;					// true - vertex programs ENABLED
	bool		gl_swap_control;
	bool		gl_swap_control_tear;
	int			occlusion;
	bool		arb_multisample;
	bool		nv_multisample_hint;
	int			screenTextureSize[2];
	int			mirrorTextureSize;
	int			maxTextureSize;
} glconfig_t;


#define	GPU_MEMORY_INFO_DEDICATED_VIDMEM_NVX			0x9047
#define	GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX		0x9048
#define	GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX	0x9049
#define	GPU_MEMORY_INFO_EVICTION_COUNT_NVX				0x904A
#define	GPU_MEMORY_INFO_EVICTED_MEMORY_NVX				0x904B

#define	VBO_FREE_MEMORY_ATI								0x87FB
#define	TEXTURE_FREE_MEMORY_ATI							0x87FC
#define	RENDERBUFFER_FREE_MEMORY_ATI					0x87FD

#define	GL_TEX			1
#define	GL_TMU			2
#define	GL_TEXENV		4
#define	GL_BLEND_		8
#define	GL_COLOR_		16
#define	GL_VP			32
#define	GL_FP			64

typedef struct
{
	float	inverse_intensity;
	bool	fullscreen;
	int     prev_mode;
///	int		lightmap_textures;
	int		currenttextures[32];		// for 32 TMUs (max in OpenGL)
	int		currenttmu;
	int		texenv[32][2];
	int		lastblend_src;
	int		lastblend_dst;
	float	color[4];
	int		vp;
	int		fp;
	byte	originalRedGammaTable[256];
	byte	originalGreenGammaTable[256];
	byte	originalBlueGammaTable[256];
	GLuint	vbo_id;
///	double	clipPlane[6][4];		///// почему то если включить кэширование, клипПлейн работает неверно...
} glstate_t;

typedef struct
{
	char *name;
	int mode;
} gltmode_t;

#define NUM_GL_ALPHA_MODES (sizeof(gl_alpha_modes) / sizeof (gltmode_t))

#define NUM_GL_SOLID_MODES (sizeof(gl_solid_modes) / sizeof (gltmode_t))

typedef struct vrect_s
{
	int	x,y,width,height;
} vrect_t;

typedef struct
{
	int		x1, y1, x2, y2;
} dirty_t;

typedef struct
{
    char	manufacturer;
    char	version;
    char	encoding;
    char	bits_per_pixel;
    WORD	xmin,ymin,xmax,ymax;
    WORD	hres,vres;
    byte	palette[48];
    char	reserved;
    char	color_planes;
    WORD	bytes_per_line;
    WORD	palette_type;
    char	filler[58];
    byte	data;			// unbounded
} pcx_t;

typedef struct _TargaHeader
{
	byte 	id_length, colormap_type, image_type;
	WORD	colormap_index, colormap_length;
	byte	colormap_size;
	WORD	x_origin, y_origin, width, height;
	byte	pixel_size, attributes;
} TargaHeader;

#define	MIPLEVELS	4
typedef struct miptex_s
{
	char		name[32];
	unsigned	width, height;
	unsigned	offsets[MIPLEVELS];		// four mip maps stored
	char		animname[32];			// next frame in animation chain
	int			flags;
	int			contents;
	int			value;
} miptex_t;

///#define	VERSION		6.5		// Версия движка

// game print flags
#define	PRINT_LOW			0		// pickup messages
#define	PRINT_MEDIUM		1		// death messages
#define	PRINT_HIGH			2		// critical messages
#define	PRINT_CHAT			3		// chat messages

//
// client to server
//
enum clc_ops_e
{
	clc_bad,
	clc_nop,
	clc_move,				// [[usercmd_t]
	clc_userinfo,			// [[userinfo string]	/// В протоколе Berserker - передаем только изменения userinfo. Полный userinfo передается серверу только при "connect"
	clc_stringcmd			// [string] message
};

//
// server to client
//
enum svc_ops_e
{
	svc_bad,

	// these ops are known to the game dll
	svc_muzzleflash,
	svc_muzzleflash2,
	svc_temp_entity,
	svc_layout,
	svc_inventory,

	// the rest are private to the client and server
	svc_nop,
	svc_disconnect,
	svc_reconnect,
	svc_sound,					// <see code>
	svc_print,					// [byte] id [string] null terminated string
	svc_stufftext,				// [string] stuffed into client's console buffer, should be \n terminated
	svc_serverdata,				// [long] protocol ...
	svc_configstring,			// [short] [string]
	svc_spawnbaseline,
	svc_centerprint,			// [string] to put in center of the screen
	svc_download,				// [short] size [size bytes]
	svc_playerinfo,				// variable
	svc_packetentities,			// [...]
	svc_deltapacketentities,	// [...]
	svc_frame,
///	svc_areaportals_state,		/// Berserker: информация о текущем состоянии portalopen сервера
///	svc_areaportal				/// Berserker: информация об открытии/закрытии порталов всем клиентам
	svc_rfx,					/// Berserker: инфо о render_fx. Параметр: байт, код эффекта. Старший бит = 1 означает выключение эффекта.
	svc_temp_entity_ext			/// Berserker: подобен svc_temp_entity, но передаётся координата большой карты (LARGE_MAP_SIZE)
};


#define MAX_RFX_IMAGES	64

enum rfx_ops
{
	RFX_DISABLE_ALL,
////	RFX_BREATHER,
////	RFX_ENVIRO,
////	RFX_BURN_1_SECOND,			// Выключается сам через 1 сек.
	RFX_PAIN_07_SECOND,			// Выключается сам через 0.7 сек.
////	RFX_DROWN_1_SECOND,			// Выключается сам через 1 сек.
	RFX_PAIN_2_SECOND			// Выключается сам через 2 сек.
////	RFX_UNDERWATER
};


typedef struct
{
	int		key;				// so entities can reuse same entry
	vec3_t	color;
	vec3_t	origin;
	float	radius;
	float	die;				// stop lighting after this time
///	float	decay;				// drop this each second				Berserker: not used???
///	float	minlight;			// don't add when contributing less		Berserker: not used???
} cdlight_t;


//
// muzzle flashes / player effects
//
#define	MZ_BLASTER			0
#define MZ_MACHINEGUN		1
#define	MZ_SHOTGUN			2
#define	MZ_CHAINGUN1		3
#define	MZ_CHAINGUN2		4
#define	MZ_CHAINGUN3		5
#define	MZ_RAILGUN			6
#define	MZ_ROCKET			7
#define	MZ_GRENADE			8
#define	MZ_LOGIN			9
#define	MZ_LOGOUT			10
#define	MZ_RESPAWN			11
#define	MZ_BFG				12
#define	MZ_SSHOTGUN			13
#define	MZ_HYPERBLASTER		14
#define	MZ_ITEMRESPAWN		15
// RAFAEL
#define MZ_IONRIPPER		16
#define MZ_BLUEHYPERBLASTER 17
#define MZ_PHALANX			18
//ROGUE
#define MZ_ETF_RIFLE		30
#define MZ_UNUSED			31
#define MZ_SHOTGUN2			32
#define MZ_HEATBEAM			33
#define MZ_BLASTER2			34
#define	MZ_TRACKER			35
#define	MZ_NUKE1			36
#define	MZ_NUKE2			37
#define	MZ_NUKE4			38
#define	MZ_NUKE8			39
// Berserker
#define	MZ_MONSTER_RESPAWN	127

#define MZ_SILENCED			128		// bit flag ORed with one of the above numbers


//
// monster muzzle flashes
//
#define MZ2_TANK_BLASTER_1				1
#define MZ2_TANK_BLASTER_2				2
#define MZ2_TANK_BLASTER_3				3
#define MZ2_TANK_MACHINEGUN_1			4
#define MZ2_TANK_MACHINEGUN_2			5
#define MZ2_TANK_MACHINEGUN_3			6
#define MZ2_TANK_MACHINEGUN_4			7
#define MZ2_TANK_MACHINEGUN_5			8
#define MZ2_TANK_MACHINEGUN_6			9
#define MZ2_TANK_MACHINEGUN_7			10
#define MZ2_TANK_MACHINEGUN_8			11
#define MZ2_TANK_MACHINEGUN_9			12
#define MZ2_TANK_MACHINEGUN_10			13
#define MZ2_TANK_MACHINEGUN_11			14
#define MZ2_TANK_MACHINEGUN_12			15
#define MZ2_TANK_MACHINEGUN_13			16
#define MZ2_TANK_MACHINEGUN_14			17
#define MZ2_TANK_MACHINEGUN_15			18
#define MZ2_TANK_MACHINEGUN_16			19
#define MZ2_TANK_MACHINEGUN_17			20
#define MZ2_TANK_MACHINEGUN_18			21
#define MZ2_TANK_MACHINEGUN_19			22
#define MZ2_TANK_ROCKET_1				23
#define MZ2_TANK_ROCKET_2				24
#define MZ2_TANK_ROCKET_3				25

#define MZ2_INFANTRY_MACHINEGUN_1		26
#define MZ2_INFANTRY_MACHINEGUN_2		27
#define MZ2_INFANTRY_MACHINEGUN_3		28
#define MZ2_INFANTRY_MACHINEGUN_4		29
#define MZ2_INFANTRY_MACHINEGUN_5		30
#define MZ2_INFANTRY_MACHINEGUN_6		31
#define MZ2_INFANTRY_MACHINEGUN_7		32
#define MZ2_INFANTRY_MACHINEGUN_8		33
#define MZ2_INFANTRY_MACHINEGUN_9		34
#define MZ2_INFANTRY_MACHINEGUN_10		35
#define MZ2_INFANTRY_MACHINEGUN_11		36
#define MZ2_INFANTRY_MACHINEGUN_12		37
#define MZ2_INFANTRY_MACHINEGUN_13		38

#define MZ2_SOLDIER_BLASTER_1			39
#define MZ2_SOLDIER_BLASTER_2			40
#define MZ2_SOLDIER_SHOTGUN_1			41
#define MZ2_SOLDIER_SHOTGUN_2			42
#define MZ2_SOLDIER_MACHINEGUN_1		43
#define MZ2_SOLDIER_MACHINEGUN_2		44

#define MZ2_GUNNER_MACHINEGUN_1			45
#define MZ2_GUNNER_MACHINEGUN_2			46
#define MZ2_GUNNER_MACHINEGUN_3			47
#define MZ2_GUNNER_MACHINEGUN_4			48
#define MZ2_GUNNER_MACHINEGUN_5			49
#define MZ2_GUNNER_MACHINEGUN_6			50
#define MZ2_GUNNER_MACHINEGUN_7			51
#define MZ2_GUNNER_MACHINEGUN_8			52
#define MZ2_GUNNER_GRENADE_1			53
#define MZ2_GUNNER_GRENADE_2			54
#define MZ2_GUNNER_GRENADE_3			55
#define MZ2_GUNNER_GRENADE_4			56

#define MZ2_CHICK_ROCKET_1				57

#define MZ2_FLYER_BLASTER_1				58
#define MZ2_FLYER_BLASTER_2				59

#define MZ2_MEDIC_BLASTER_1				60

#define MZ2_GLADIATOR_RAILGUN_1			61

#define MZ2_HOVER_BLASTER_1				62

#define MZ2_ACTOR_MACHINEGUN_1			63

#define MZ2_SUPERTANK_MACHINEGUN_1		64
#define MZ2_SUPERTANK_MACHINEGUN_2		65
#define MZ2_SUPERTANK_MACHINEGUN_3		66
#define MZ2_SUPERTANK_MACHINEGUN_4		67
#define MZ2_SUPERTANK_MACHINEGUN_5		68
#define MZ2_SUPERTANK_MACHINEGUN_6		69
#define MZ2_SUPERTANK_ROCKET_1			70
#define MZ2_SUPERTANK_ROCKET_2			71
#define MZ2_SUPERTANK_ROCKET_3			72

#define MZ2_BOSS2_MACHINEGUN_L1			73
#define MZ2_BOSS2_MACHINEGUN_L2			74
#define MZ2_BOSS2_MACHINEGUN_L3			75
#define MZ2_BOSS2_MACHINEGUN_L4			76
#define MZ2_BOSS2_MACHINEGUN_L5			77
#define MZ2_BOSS2_ROCKET_1				78
#define MZ2_BOSS2_ROCKET_2				79
#define MZ2_BOSS2_ROCKET_3				80
#define MZ2_BOSS2_ROCKET_4				81

#define MZ2_FLOAT_BLASTER_1				82

#define MZ2_SOLDIER_BLASTER_3			83
#define MZ2_SOLDIER_SHOTGUN_3			84
#define MZ2_SOLDIER_MACHINEGUN_3		85
#define MZ2_SOLDIER_BLASTER_4			86
#define MZ2_SOLDIER_SHOTGUN_4			87
#define MZ2_SOLDIER_MACHINEGUN_4		88
#define MZ2_SOLDIER_BLASTER_5			89
#define MZ2_SOLDIER_SHOTGUN_5			90
#define MZ2_SOLDIER_MACHINEGUN_5		91
#define MZ2_SOLDIER_BLASTER_6			92
#define MZ2_SOLDIER_SHOTGUN_6			93
#define MZ2_SOLDIER_MACHINEGUN_6		94
#define MZ2_SOLDIER_BLASTER_7			95
#define MZ2_SOLDIER_SHOTGUN_7			96
#define MZ2_SOLDIER_MACHINEGUN_7		97
#define MZ2_SOLDIER_BLASTER_8			98
#define MZ2_SOLDIER_SHOTGUN_8			99
#define MZ2_SOLDIER_MACHINEGUN_8		100

// --- Xian shit below ---
#define	MZ2_MAKRON_BFG					101
#define MZ2_MAKRON_BLASTER_1			102
#define MZ2_MAKRON_BLASTER_2			103
#define MZ2_MAKRON_BLASTER_3			104
#define MZ2_MAKRON_BLASTER_4			105
#define MZ2_MAKRON_BLASTER_5			106
#define MZ2_MAKRON_BLASTER_6			107
#define MZ2_MAKRON_BLASTER_7			108
#define MZ2_MAKRON_BLASTER_8			109
#define MZ2_MAKRON_BLASTER_9			110
#define MZ2_MAKRON_BLASTER_10			111
#define MZ2_MAKRON_BLASTER_11			112
#define MZ2_MAKRON_BLASTER_12			113
#define MZ2_MAKRON_BLASTER_13			114
#define MZ2_MAKRON_BLASTER_14			115
#define MZ2_MAKRON_BLASTER_15			116
#define MZ2_MAKRON_BLASTER_16			117
#define MZ2_MAKRON_BLASTER_17			118
#define MZ2_MAKRON_RAILGUN_1			119
#define	MZ2_JORG_MACHINEGUN_L1			120
#define	MZ2_JORG_MACHINEGUN_L2			121
#define	MZ2_JORG_MACHINEGUN_L3			122
#define	MZ2_JORG_MACHINEGUN_L4			123
#define	MZ2_JORG_MACHINEGUN_L5			124
#define	MZ2_JORG_MACHINEGUN_L6			125
#define	MZ2_JORG_MACHINEGUN_R1			126
#define	MZ2_JORG_MACHINEGUN_R2			127
#define	MZ2_JORG_MACHINEGUN_R3			128
#define	MZ2_JORG_MACHINEGUN_R4			129
#define MZ2_JORG_MACHINEGUN_R5			130
#define	MZ2_JORG_MACHINEGUN_R6			131
#define MZ2_JORG_BFG_1					132
#define MZ2_BOSS2_MACHINEGUN_R1			133
#define MZ2_BOSS2_MACHINEGUN_R2			134
#define MZ2_BOSS2_MACHINEGUN_R3			135
#define MZ2_BOSS2_MACHINEGUN_R4			136
#define MZ2_BOSS2_MACHINEGUN_R5			137
//ROGUE
#define	MZ2_CARRIER_MACHINEGUN_L1		138
#define	MZ2_CARRIER_MACHINEGUN_R1		139
#define	MZ2_CARRIER_GRENADE				140
#define MZ2_TURRET_MACHINEGUN			141
#define MZ2_TURRET_ROCKET				142
#define MZ2_TURRET_BLASTER				143
#define MZ2_STALKER_BLASTER				144
#define MZ2_DAEDALUS_BLASTER			145
#define MZ2_MEDIC_BLASTER_2				146
#define	MZ2_CARRIER_RAILGUN				147
#define	MZ2_WIDOW_DISRUPTOR				148
#define	MZ2_WIDOW_BLASTER				149
#define	MZ2_WIDOW_RAIL					150
#define	MZ2_WIDOW_PLASMABEAM			151		// PMM - not used
#define	MZ2_CARRIER_MACHINEGUN_L2		152
#define	MZ2_CARRIER_MACHINEGUN_R2		153
#define	MZ2_WIDOW_RAIL_LEFT				154
#define	MZ2_WIDOW_RAIL_RIGHT			155
#define	MZ2_WIDOW_BLASTER_SWEEP1		156
#define	MZ2_WIDOW_BLASTER_SWEEP2		157
#define	MZ2_WIDOW_BLASTER_SWEEP3		158
#define	MZ2_WIDOW_BLASTER_SWEEP4		159
#define	MZ2_WIDOW_BLASTER_SWEEP5		160
#define	MZ2_WIDOW_BLASTER_SWEEP6		161
#define	MZ2_WIDOW_BLASTER_SWEEP7		162
#define	MZ2_WIDOW_BLASTER_SWEEP8		163
#define	MZ2_WIDOW_BLASTER_SWEEP9		164
#define	MZ2_WIDOW_BLASTER_100			165
#define	MZ2_WIDOW_BLASTER_90			166
#define	MZ2_WIDOW_BLASTER_80			167
#define	MZ2_WIDOW_BLASTER_70			168
#define	MZ2_WIDOW_BLASTER_60			169
#define	MZ2_WIDOW_BLASTER_50			170
#define	MZ2_WIDOW_BLASTER_40			171
#define	MZ2_WIDOW_BLASTER_30			172
#define	MZ2_WIDOW_BLASTER_20			173
#define	MZ2_WIDOW_BLASTER_10			174
#define	MZ2_WIDOW_BLASTER_0				175
#define	MZ2_WIDOW_BLASTER_10L			176
#define	MZ2_WIDOW_BLASTER_20L			177
#define	MZ2_WIDOW_BLASTER_30L			178
#define	MZ2_WIDOW_BLASTER_40L			179
#define	MZ2_WIDOW_BLASTER_50L			180
#define	MZ2_WIDOW_BLASTER_60L			181
#define	MZ2_WIDOW_BLASTER_70L			182
#define	MZ2_WIDOW_RUN_1					183
#define	MZ2_WIDOW_RUN_2					184
#define	MZ2_WIDOW_RUN_3					185
#define	MZ2_WIDOW_RUN_4					186
#define	MZ2_WIDOW_RUN_5					187
#define	MZ2_WIDOW_RUN_6					188
#define	MZ2_WIDOW_RUN_7					189
#define	MZ2_WIDOW_RUN_8					190
#define	MZ2_CARRIER_ROCKET_1			191
#define	MZ2_CARRIER_ROCKET_2			192
#define	MZ2_CARRIER_ROCKET_3			193
#define	MZ2_CARRIER_ROCKET_4			194
#define	MZ2_WIDOW2_BEAMER_1				195
#define	MZ2_WIDOW2_BEAMER_2				196
#define	MZ2_WIDOW2_BEAMER_3				197
#define	MZ2_WIDOW2_BEAMER_4				198
#define	MZ2_WIDOW2_BEAMER_5				199
#define	MZ2_WIDOW2_BEAM_SWEEP_1			200
#define	MZ2_WIDOW2_BEAM_SWEEP_2			201
#define	MZ2_WIDOW2_BEAM_SWEEP_3			202
#define	MZ2_WIDOW2_BEAM_SWEEP_4			203
#define	MZ2_WIDOW2_BEAM_SWEEP_5			204
#define	MZ2_WIDOW2_BEAM_SWEEP_6			205
#define	MZ2_WIDOW2_BEAM_SWEEP_7			206
#define	MZ2_WIDOW2_BEAM_SWEEP_8			207
#define	MZ2_WIDOW2_BEAM_SWEEP_9			208
#define	MZ2_WIDOW2_BEAM_SWEEP_10		209
#define	MZ2_WIDOW2_BEAM_SWEEP_11		210



#define	ANGLE2SHORT(x)	((int)((x)*65536/360) & 65535)
#define	SHORT2ANGLE(x)	((x)*(360.0/65536))

#define	MAX_LOOPBACK	4

typedef struct
{
	byte	data[MAX_MSGLEN];
	int		datalen;
} loopmsg_t;

typedef struct
{
	loopmsg_t	msgs[MAX_LOOPBACK];
	int			get, send;
} loopback_t;

loopback_t	loopbacks[2];

// entity_state_t is the information conveyed from the server
// in an update message about entities that the client will
// need to render in some way
typedef struct entity_state_s
{
	int		number;			// edict index
	vec3_t	origin;
	vec3_t	angles;
	vec3_t	old_origin;		// for lerping
	int		modelindex;
	int		modelindex2, modelindex3, modelindex4;	// weapons, CTF flags, etc
	int		frame;
	int		skinnum;
	unsigned	effects;		// PGM - we're filling it, so it needs to be unsigned
	int		renderfx;
	int		solid;			// for client side prediction, 8*(bits 0-4) is x/y radius
							// 8*(bits 5-9) is z down distance, 8(bits10-15) is z up
							// gi.linkentity sets this properly
	int		sound;			// for looping sounds, to guarantee shutoff
	int		event;			// impulse events -- muzzle flashes, footsteps, etc
							// events only go out for a single frame, they
							// are automatically cleared each frame
} entity_state_t;


typedef struct centity_s
{
	entity_state_t	baseline;		// delta from this if not from a previous frame
	entity_state_t	current;
	entity_state_t	prev;			// will always be valid, but might just be a copy of current

	int			serverframe;		// if not current, this ent isn't in the frame

	int			trailcount;			// for diminishing grenade trails
	vec3_t		lerp_origin;		// for trails (variable hz)

	int			fly_stoptime;
} centity_t;

#define	LATENCY_COUNTS		16
#define	RATE_MESSAGES		10
#define	MAX_INFO_KEY		64
#define	MAX_INFO_STRING		512

typedef struct
{
	int					areabytes;
	byte				areabits[MAX_MAP_AREAS/8];		// portalarea visibility bits
	player_state_t		ps;
	int					num_entities;
	int					first_entity;		// into the circular sv_packet_entities[]
	int					senttime;			// for ping calculations
} client_frame_t;

/*
typedef struct filelink_s
{
	struct filelink_s	*next;
	char	*from;
	int		fromlength;
	char	*to;
} filelink_t;
*/

// link_t is only used for entity area links now
typedef struct link_s
{
	struct link_s	*prev, *next;
} link_t;

#define	MAX_ENT_CLUSTERS	16

typedef enum
{
	SOLID_NOT,			// no interaction with other objects
	SOLID_TRIGGER,		// only touch when inside, after moving
	SOLID_BBOX,			// touch on edge
	SOLID_BSP			// bsp clip, touch on edge
} solid_t;


typedef struct gitem_s
{
	char		*classname;	// spawning name
	bool		(*pickup)(struct edict_s *ent, struct edict_s *other);
	void		(*use)(struct edict_s *ent, struct gitem_s *item);
	void		(*drop)(struct edict_s *ent, struct gitem_s *item);
	void		(*weaponthink)(struct edict_s *ent);
	char		*pickup_sound;
	char		*world_model;
	int			world_model_flags;
	char		*view_model;

	// client side info
	char		*icon;
	char		*pickup_name;	// for printing on pickup
	int			count_width;		// number of digits to display by icon

	int			quantity;		// for ammo how much, for weapons how much is used per shot
	char		*ammo;			// for weapons
	int			flags;			// IT_* flags

	int			weapmodel;		// weapon model index (for weapons)

	void		*info;
	int			tag;

	char		*precaches;		// string of all models, sounds, and images this item will use
} gitem_t;


typedef struct
{
	char		userinfo[MAX_INFO_STRING];
	char		netname[64];		// было 16, увеличил.
	int			hand;

	bool		connected;			// a loadgame will leave valid entities that
									// just don't have a connection yet

	// values saved and restored from edicts when changing levels
	int			health;
	int			max_health;
	int			savedFlags;

	int			selected_item;
	int			inventory[MAX_ITEMS];

	// ammo capacities
	int			max_bullets;
	int			max_shells;
	int			max_rockets;
	int			max_grenades;
	int			max_cells;
	int			max_slugs;

	gitem_t		*weapon;
	gitem_t		*lastweapon;

	int			power_cubes;	// used for tracking the cubes in coop games
	int			score;			// for calculating total unit score in coop games

	int			game_helpchanged;
	int			helpchanged;

	bool		spectator;			// client is a spectator

	BYTE		railtrailcore;
	BYTE		railtrailspiral;
	BYTE		railtrailradius;
} client_persistant_t;


// client data that stays across deathmatch respawns
typedef struct
{
	client_persistant_t	coop_respawn;	// what to set client->pers to on a respawn
	int			enterframe;			// level.framenum the client entered the game
	int			score;				// frags, etc
	vec3_t		cmd_angles;			// angles sent over in the last command

	bool		spectator;			// client is a spectator
} client_respawn_t;


typedef struct pmenuhnd_s {
	struct pmenu_s *entries;
	int cur;
	int num;
	void *arg;
} pmenuhnd_t;


typedef enum
{
	WEAPON_READY,
	WEAPON_ACTIVATING,
	WEAPON_DROPPING,
	WEAPON_FIRING
} weaponstate_t;


typedef struct client_s
{
	client_state_te	state;

	char			userinfo[MAX_INFO_STRING];		// name, etc

	int				lastframe;			// for delta compression
	usercmd_t		lastcmd;			// for filling in big drops

	int				commandMsec;		// every seconds this is reset, if user
										// commands exhaust it, assume time cheating

	int				frame_latency[LATENCY_COUNTS];
	int				ping;

	int				message_size[RATE_MESSAGES];	// used to rate drop packets
	int				rate;
	int				surpressCount;		// number of messages rate supressed

	edict_t			*edict;				// EDICT_NUM(clientnum+1)
	char			name[128];			// было 32, extracted from userinfo, high bits masked
	int				messagelevel;		// for filtering printed messages

	// The datagram is written to by sound calls, prints, temp ents, etc.
	// It can be harmlessly overflowed.
	sizebuf_t		datagram;
	byte			datagram_buf[MAX_MSGLEN];

	client_frame_t	frames[UPDATE_BACKUP];	// updates can be delta'd from here

	byte			*download;			// file being downloaded
	int				downloadsize;		// total bytes (can't use EOF because of paks)
	int				downloadcount;		// bytes sent

	int				lastmessage;		// sv.framenum when packet was last received
	int				lastconnect;

	int				challenge;			// challenge of this user, randomly generated

	netchan_t		netchan;

	unsigned		nodeltaframes;		//r1: number of consecutive nodelta frames
} client_t;


// MAX_CHALLENGES is made large to prevent a denial
// of service attack that could cycle all of them
// out before legitimate users connected
#define	MAX_CHALLENGES	1024

typedef struct
{
	netadr_t	adr;
	int			challenge;
	int			time;
} challenge_t;


typedef struct
{
	bool		initialized;				// sv_init has completed
	int			realtime;					// always increasing, no clamping, etc

	char		mapcmd[MAX_TOKEN_CHARS];	// ie: *intro.cin+base

	int			spawncount;					// incremented each server start
											// used to check late spawns

	client_t	*clients;					// [maxclients->value];
	int			num_client_entities;		// maxclients->value*UPDATE_BACKUP*MAX_PACKET_ENTITIES
	int			next_client_entities;		// next client_entity to use
	entity_state_t	*client_entities;		// [num_client_entities]

	challenge_t	challenges[MAX_CHALLENGES];	// to prevent invalid IPs from connecting

	// serverrecord values
	FILE		*demofile;
	sizebuf_t	demo_multicast;
	byte		demo_multicast_buf[MAX_MSGLEN];
} server_static_t;


typedef enum
{
	ss_dead,			// no map loaded
	ss_loading,			// spawning level edicts
	ss_game,			// actively running
	ss_cinematic,
	ss_demo,
	ss_pic
} server_state_t;


// can't just use function pointers, or dll linkage can
// mess up when qcommon is included in multiple places
short	(*_BigShort) (short l);
short	(*_LittleShort) (short l);
int		(*_BigLong) (int l);
int		(*_LittleLong) (int l);
float	(*_BigFloat) (float l);
float	(*_LittleFloat) (float l);

short	BigShort(short l){return _BigShort(l);}
short	LittleShort(short l) {return _LittleShort(l);}
int		BigLong (int l) {return _BigLong(l);}
int		LittleLong (int l) {return _LittleLong(l);}
float	BigFloat (float l) {return _BigFloat(l);}
float	LittleFloat (float l) {return _LittleFloat(l);}


typedef struct
{
	server_state_t	state;			// precache commands are only valid during load

	bool		attractloop;		// running demos for the local system only
	bool		loadgame;			// client begins should reuse existing entity

	unsigned	time;				// always sv.framenum * 100 msec
	int			framenum;

	char		name[MAX_QPATH];			// map name
	struct cmodel_s		*models[MAX_MODELS_BERS];

	char		configstrings[MAX_CONFIGSTRINGS_BERS][MAX_QPATH];
	entity_state_t	baselines[MAX_EDICTS];

	// the multicast buffer is used to send a message to a set of clients
	// it is only used to marshall data until SV_Multicast is called
	sizebuf_t	multicast;
	byte		multicast_buf[MAX_MSGLEN];

	// demo server information
	FILE		*demofile;
	bool		timedemo;		// don't time sync
} server_t;


#if defined(__GNUC__)
# if defined(__i386__)
#  define BERS_ARCH_STR		"686"
# elif defined(__x86_64__)
#  define BERS_ARCH_STR		"x86_64"
# endif
#elif defined(_WIN64)
# define BERS_ARCH_STR		"x86_64"
#elif defined(_WIN32)
# define BERS_ARCH_STR		"x86"
#else
# define BERS_ARCH_STR		"unknown"
#endif

#if defined(__linux__)
# define BERS_OS_STR		"Linux"
#elif defined(_WIN64)
# define BERS_OS_STR		"Win64"
#elif defined(_WIN32)
# define BERS_OS_STR		"Win32"
#else
# define BERS_OS_STR		"Unknown"
#endif

#ifdef NDEBUG
#define BUILDSTRING BERS_OS_STR " Release"
#else
#define BUILDSTRING BERS_OS_STR " Debug"
#endif


//
// functions exported by the game subsystem
//
typedef struct
{
	int			apiversion;

	// the init function will only be called when a game starts,
	// not each time a level is loaded.  Persistant data for clients
	// and the server can be allocated in init
	void		(*Init) (void);
	void		(*Shutdown) (void);

	// each new level entered will cause a call to SpawnEntities
	void		(*SpawnEntities) (char *mapname, char *entstring, char *spawnpoint);

	// Read/Write Game is for storing persistant cross level information
	// about the world state and the clients.
	// WriteGame is called every time a level is exited.
	// ReadGame is called on a loadgame.
	void		(*WriteGame) (char *filename, bool autosave);
	void		(*ReadGame) (char *filename);

	// ReadLevel is called after the default map information has been
	// loaded with SpawnEntities
	void		(*WriteLevel) (char *filename);
	void		(*ReadLevel) (char *filename);

	bool		(*ClientConnect) (edict_t *ent, char *userinfo);
	void		(*ClientBegin) (edict_t *ent);
	void		(*ClientUserinfoChanged) (edict_t *ent, char *userinfo);
	void		(*ClientDisconnect) (edict_t *ent);
	void		(*ClientCommand) (edict_t *ent);
	void		(*ClientThink) (edict_t *ent, usercmd_t *cmd);

	void		(*RunFrame) (float stopclock);	/// Berserker's hack: остановим игровые часы, чтоб можно было увидеть изначальное положение браш-моделей для линкования на них источников света

	// ServerCommand will be called when an "sv <command>" command is issued on the
	// server console.
	// The game can issue gi.argc() / gi.argv() commands to get the rest
	// of the parameters
	void		(*ServerCommand) (void);

///	float		(*LevelTime) (void);
	//
	// global variables shared between game and server
	//

	// The edict array is allocated in the game dll so it
	// can vary in size from one game to another.
	//
	// The size will be fixed when ge->Init() is called
	struct edict_s	*edicts;
	int			edict_size;
	int			num_edicts;		// current number, <= max_edicts
	int			max_edicts;
} game_export_t;

#define	FRAMETIME		0.1
#define FRAMETIME_MAX	0.5

#define	PORT_CLIENT				27971	/// my client port
#define	PORT_SERVER				27980	/// my server port

#define	OLD_PORT_CLIENT			27901	/// old Q2 client port
#define	OLD_PORT_SERVER			27910	/// old Q2 server port

#define	PROTOCOL_VERSION		4		/// my net protocol version
#define	OLD_PROTOCOL_VERSION	34		/// old Q2 protocol version

// dmflags->value flags
#define	DF_NO_HEALTH		0x00000001	// 1
#define	DF_NO_ITEMS			0x00000002	// 2
#define	DF_WEAPONS_STAY		0x00000004	// 4
#define	DF_NO_FALLING		0x00000008	// 8
#define	DF_INSTANT_ITEMS	0x00000010	// 16
#define	DF_SAME_LEVEL		0x00000020	// 32
#define DF_SKINTEAMS		0x00000040	// 64
#define DF_MODELTEAMS		0x00000080	// 128
#define DF_NO_FRIENDLY_FIRE	0x00000100	// 256
#define	DF_SPAWN_FARTHEST	0x00000200	// 512
#define DF_FORCE_RESPAWN	0x00000400	// 1024
#define DF_NO_ARMOR			0x00000800	// 2048
#define DF_ALLOW_EXIT		0x00001000	// 4096
#define DF_INFINITE_AMMO	0x00002000	// 8192
#define DF_QUAD_DROP		0x00004000	// 16384
#define DF_FIXED_FOV		0x00008000	// 32768
/// CTF support
#define DF_CTF_FORCEJOIN	0x00020000	// 131072
#define DF_ARMOR_PROTECT	0x00040000	// 262144
#define DF_CTF_NO_TECH		0x00080000	// 524288
/// Berserker:
#define DF_INSTAGIB			0x00400000
#define DF_NOGODLIGHT		0x00800000
#define DF_UT_DOUBLE_JUMP	0x01000000
#define DF_FLASHLIGHTCELLS	0x02000000
#define DF_SHELL			0x04000000
#define DF_HASTE			0x08000000	// 134217728
#define DF_DROP_WEAPON		0x10000000	// 268435456
#define DF_FLASHLIGHT		0x20000000	// 536870912
#define DF_FOOTPRINT		0x40000000	// 1073741824
#define DF_FASTROCKET		0x80000000	// 2147483648


typedef struct cplane_s
{
	vec3_t	normal;
	float	dist;
	byte	type;			// for fast side tests
	byte	signbits;		// signx + (signy<<1) + (signz<<1)
//	byte	pad[2];		/// Berserker: not used???
} cplane_t;

// 0-2 are axial planes
#define	PLANE_X			0
#define	PLANE_Y			1
#define	PLANE_Z			2

// 3-5 are non-axial planes snapped to the nearest
#define	PLANE_ANYX		3
#define	PLANE_ANYY		4
#define	PLANE_ANYZ		5

typedef struct
{
	int			down[2];		// key nums holding it down
	unsigned	downtime;		// msec timestamp
	unsigned	msec;			// msec down this frame
	int			state;
} kbutton_t;


#define BOX_ON_PLANE_SIDE(emins, emaxs, p)	\
	(((p)->type < 3)?						\
	(										\
		((p)->dist <= (emins)[(p)->type])?	\
			1								\
		:									\
		(									\
			((p)->dist >= (emaxs)[(p)->type])?\
				2							\
			:								\
				3							\
		)									\
	)										\
	:										\
		BoxOnPlaneSide( (emins), (emaxs), (p)))


typedef struct areanode_s
{
	int		axis;		// -1 = leaf node
	float	dist;
	struct areanode_s	*children[2];
	link_t	trigger_edicts;
	link_t	solid_edicts;
} areanode_t;

#define EDICT_NUM(n) ((edict_t *)((byte *)ge->edicts + ge->edict_size*(n)))
#define NUM_FOR_EDICT(e) ( ((byte *)(e)-(byte *)ge->edicts ) / ge->edict_size)


// user_cmd_t communication
// ms and light always sent, the others are optional
#define	CM_ANGLE1 	(1<<0)
#define	CM_ANGLE2 	(1<<1)
#define	CM_ANGLE3 	(1<<2)
#define	CM_FORWARD	(1<<3)
#define	CM_SIDE		(1<<4)
#define	CM_UP		(1<<5)
#define	CM_BUTTONS	(1<<6)
#define	CM_IMPULSE	(1<<7)

#define	MAX_STRINGCMDS	8

#define CRC_INIT_VALUE	0xffff

typedef struct
{
	char	*name;
	void	(*func) (void);
} ucmd_t;


// lower bits are stronger, and will eat weaker brushes completely
#define	CONTENTS_SOLID			1		// an eye is never valid in a solid
#define	CONTENTS_WINDOW			2		// translucent, but not watery
#define	CONTENTS_AUX			4		// Berserker: юзаю для playerclip но с возможностью простреливания сквозь полигон
#define	CONTENTS_LAVA			8
#define	CONTENTS_SLIME			16
#define	CONTENTS_WATER			32
///#define	CONTENTS_MIST			64	// in radiant	mist == fog
#define	CONTENTS_FOG			64
#define	LAST_VISIBLE_CONTENTS	64

// remaining contents are non-visible, and don't eat brushes
#define	CONTENTS_AREAPORTAL		0x8000
#define	CONTENTS_PLAYERCLIP		0x10000
#define	CONTENTS_MONSTERCLIP	0x20000

// currents can be added to any other contents, and may be mixed
#define	CONTENTS_CURRENT_0		0x40000
#define	CONTENTS_CURRENT_90		0x80000
#define	CONTENTS_CURRENT_180	0x100000
#define	CONTENTS_CURRENT_270	0x200000
#define	CONTENTS_CURRENT_UP		0x400000
#define	CONTENTS_CURRENT_DOWN	0x800000
#define	CONTENTS_ORIGIN			0x1000000	// removed before bsping an entity
#define	CONTENTS_MONSTER		0x2000000	// should never be on a brush, only in game
#define	CONTENTS_DEADMONSTER	0x4000000
#define	CONTENTS_DETAIL			0x8000000	// brushes to be added after vis leafs
#define	CONTENTS_TRANSLUCENT	0x10000000	// auto set if any surface has trans
#define	CONTENTS_LADDER			0x20000000


// content masks
#define	MASK_ALL				(-1)
#define	MASK_SOLID				(CONTENTS_SOLID|CONTENTS_WINDOW)
#define	MASK_PLAYERSOLID		(CONTENTS_SOLID|CONTENTS_PLAYERCLIP|CONTENTS_WINDOW|CONTENTS_MONSTER)
#define	MASK_DEADSOLID			(CONTENTS_SOLID|/*CONTENTS_PLAYERCLIP|*/CONTENTS_WINDOW)				/// Berserker: теперь CONTENTS_PLAYERCLIP не блокирует трупы...
#define	MASK_MONSTERSOLID		(CONTENTS_SOLID|CONTENTS_MONSTERCLIP|CONTENTS_WINDOW|CONTENTS_MONSTER)
#define	MASK_WATER				(CONTENTS_WATER|CONTENTS_LAVA|CONTENTS_SLIME)
#define	MASK_OPAQUE				(CONTENTS_SOLID|CONTENTS_SLIME|CONTENTS_LAVA)
#define	MASK_SHOT				(CONTENTS_SOLID|CONTENTS_MONSTER|CONTENTS_WINDOW|CONTENTS_DEADMONSTER)
#define MASK_CURRENT			(CONTENTS_CURRENT_0|CONTENTS_CURRENT_90|CONTENTS_CURRENT_180|CONTENTS_CURRENT_270|CONTENTS_CURRENT_UP|CONTENTS_CURRENT_DOWN)


#define	SURF_LIGHT		0x1		// value will hold the light strength
#define	SURF_SLICK		0x2		// effects game physics
#define	SURF_SKY		0x4		// don't draw, but add to skybox
#define	SURF_WARP		0x8		// turbulent water warp
#define	SURF_TRANS33	0x10
#define	SURF_TRANS66	0x20
#define	SURF_FLOWING	0x40	// scroll towards angle
#define	SURF_NODRAW		0x80	// don't bother referencing the texture

#define	SURF_FLARE		0x1000	// Berserker: служебная информация, показывающая, что поверхность светится (более 500 единиц яркости)
#define	SURF_FENCE		0x2000	// Berserker: признак "дырявой" текстуры (выставляется автоматически при наличии в текстуре альфа-канала)
								//		(не "марается" декалами и не отбрасывает тени)
								//		PS: for BrushModels ONLY!!!
#define	SURF_NEWTEX		0x4000	// Berserker: служебная информация, показывающая, что используется новая текстуа повышенной детализации
#define	SURF_IGNORE_LIGHT	0x8000	// Berserker: гасим light_surf где не требуется свечение...


// sound channels
// channel 0 never willingly overrides
// other channels (1-7) allways override a playing sound on that channel
#define	CHAN_AUTO               0
#define	CHAN_WEAPON             1
#define	CHAN_VOICE              2
#define	CHAN_ITEM               3
#define	CHAN_BODY               4
// modifier flags
#define	CHAN_NO_PHS_ADD			8	// send to all clients, not just ones in PHS (ATTN 0 will also do this)
#define	CHAN_RELIABLE			16	// send by reliable message, not datagram


// edict->svflags
#define	SVF_NOCLIENT			0x00000001	// don't send entity to clients, even if it has effects
#define	SVF_DEADMONSTER			0x00000002	// treat as CONTENTS_DEADMONSTER for collision
#define	SVF_MONSTER				0x00000004	// treat as CONTENTS_MONSTER for collision
#define SVF_PROJECTILE			0x00000008  // entity is simple projectile, used for network optimization

// player_state_t communication
#define	PS_M_TYPE			(1<<0)
#define	PS_M_ORIGIN			(1<<1)
#define	PS_M_VELOCITY		(1<<2)
#define	PS_M_TIME			(1<<3)
#define	PS_M_FLAGS			(1<<4)
#define	PS_M_GRAVITY		(1<<5)
#define	PS_M_DELTA_ANGLES	(1<<6)
#define	PS_VIEWOFFSET		(1<<7)
#define	PS_VIEWANGLES		(1<<8)
#define	PS_KICKANGLES		(1<<9)	/// Berserker: часто кикаем только по angle[0], поэтому сэкономим трафик на 1 и 2 компоненте.
#define	PS_BLEND			(1<<10)
#define	PS_FOV				(1<<11)
#define	PS_WEAPONINDEX		(1<<12)
#define	PS_WEAPONFRAME		(1<<13)
#define	PS_RDFLAGS			(1<<14)
#define	PS_EPS				(1<<15)		/// extra flags
/// extra flags
#define	EPS_STATS			(1<<0)
#define	EPS_KICKANGLES		(1<<1)
#define EPS_GUNOFFSET		(1<<2)
#define EPS_GUNANGLES		(1<<3)
#define EPS_VIEWANGLES		(1<<4)
#define EPS_M_VELOCITY		(1<<5)
#define	EPS_WEAPONINDEX16	(1<<6)		// Berserker: расширенный индекс weapon модели
#define EPS_M_ORIGIN		(1<<7)		// LARGE_MAP_SIZE support

// entity_state_t communication
// try to pack the common update flags into the first byte
#define	U_ORIGIN1	(1<<0)
#define	U_ORIGIN2	(1<<1)
#define	U_ANGLE2	(1<<2)
#define	U_ANGLE3	(1<<3)
#define	U_FRAME8	(1<<4)		// frame is a byte
#define	U_EVENT		(1<<5)
#define	U_REMOVE	(1<<6)		// REMOVE this entity, don't add it
#define	U_MOREBITS1	(1<<7)		// read one additional byte

// second byte
#define	U_NUMBER16	(1<<8)		// NUMBER8 is implicit if not set
#define	U_ORIGIN3	(1<<9)
#define	U_ANGLE1	(1<<10)
#define	U_MODEL		(1<<11)
#define U_RENDERFX8	(1<<12)		// fullbright, etc
#define	U_EFFECTS8	(1<<14)		// autorotate, trails, etc
#define	U_MOREBITS2	(1<<15)		// read one additional byte

// third byte
#define	U_SKIN8		(1<<16)
#define	U_FRAME16	(1<<17)		// frame is a short
#define	U_RENDERFX16 (1<<18)	// 8 + 16 = 32
#define	U_EFFECTS16	(1<<19)		// 8 + 16 = 32
#define	U_MODEL2	(1<<20)		// weapons, flags, etc
#define	U_MODEL3	(1<<21)
#define	U_MODEL4	(1<<22)
#define	U_MOREBITS3	(1<<23)		// read one additional byte

// fourth byte
#define	U_OLDORIGIN	(1<<24)		// FIXME: get rid of this
#define	U_SKIN16	(1<<25)
#define	U_SOUND		(1<<26)
#define U_SOLID		(1<<27)
				// Berserker's bits
#define	U_MODEL16	(1<<28)		// short modelindexes (instead byte)
#define	U_SOUND16	(1<<29)		// short soundindexes (instead byte)
#define	U_ORIGIN_LARGE		(1<<30)		// LARGE_MAP_SIZE support
#define	U_OLDORIGIN_LARGE	(1<<31)		// LARGE_MAP_SIZE support


// entity_state_t->renderfx flags
#define	RF_MINLIGHT			1		// allways have some light (viewmodel)
#define	RF_VIEWERMODEL		2		// don't draw through eyes, only mirrors
#define	RF_WEAPONMODEL		4		// only draw through eyes
#define	RF_FULLBRIGHT		8		// allways draw full intensity
#define	RF_DEPTHHACK		16		// for view weapon Z crunching
#define	RF_TRANSLUCENT		32
#define	RF_FRAMELERP		64
#define RF_BEAM				128
#define	RF_CUSTOMSKIN		256		// skin is an index in image_precache
///	#define	RF_GLOW				512		// pulse lighting for bonus items
	#define RF_ANIM_ONE_CYCLE	512		// for Berserker-protocol only!!! - анимировать на стороне клиента только один цикл
#define RF_SHELL_RED		1024	// red power-shield
#define	RF_SHELL_GREEN		2048	// green power-shield
#define RF_SHELL_BLUE		4096	// blue power-shield

//BERSERKER
#define	RF_NOSELFSHADOW		0x00002000		// Berserker: для алиас-моделей - запрет самозатенения
#define	RF_CALC_NORMALS		0x00004000		// Berserker: форсируем пересчет нормалей для MD2
//ROGUE
#define RF_IR_VISIBLE		0x00008000		// 32768
///#define	RF_SHELL_DOUBLE		0x00010000		// 65536
///#define	RF_SHELL_HALF_DAM	0x00020000
#define RF_USE_DISGUISE		0x00040000
//BERSERKER
#define RF_NOCASTSHADOW		0x00080000	// no cast shadow (used in model_t.flags !!!)
#define RF_CHROME			0x00100000	// chromemap
#define RF_POWER_RED		0x00200000	// red power-shield (like RF_SHELL_RED, but w/o shell)
#define RF_POWER_GREEN		0x00400000	// green power-shield (like RF_SHELL_GREEN, but w/o shell)
#define RF_POWER_BLUE		0x00800000	// blue power-shield (like RF_SHELL_BLUE, but w/o shell)
#define RF_LIGHT			0x01000000	// Свечение entity (цвет вычисляется от SHELL/POWER/иначе white)
#define RF_NOAUTOBUMP		0x02000000	// Не создавать бамп-текстуру для модели, если нет "родного" бампа...
#define RF_SMOOTHVECS		0x04000000	/// Сглаживать все 3 нормали всех треугольников модели (MD2 only!!!)
									/// Такой режим дает более качественный рендеринг, НО при условии, что в модели нет резких углов!
#define RF_NOUSE			0x08000000	/// Не использовать модель в lighting-render code (for internal use only!!!)
#define RF_DISTORT			0x10000000	/// Модель имеет distort эффект (для монстров-невидимок и игроков в invisible-режиме)
#define RF_ANIMFRAME		0x20000000	/// Модель меняет фреймы автоматически (модели-спрайты ВСЕГДА анимационные!)
#define RF_DISTORT_SKIN		0x40000000	/// MD3-модель имеет хотя бы один меш с дисторт-шкуркой (применяется для раннего отбрасывания не-дисторт MD3-моделей)
///#define RF_COLOR_SHELL		0x80000000	// no need more...



#define MAX_LOCAL_SERVERS 8

// entity_state_t->event values
// ertity events are for effects that take place reletive
// to an existing entities origin.  Very network efficient.
// All muzzle flashes really should be converted to events...
typedef enum
{
	EV_NONE,
	EV_ITEM_RESPAWN,
	EV_FOOTSTEP,				// просто топот, без следов
	EV_FALLSHORT,
	EV_FALL,
	EV_FALLFAR,
	EV_PLAYER_TELEPORT,
	EV_OTHER_TELEPORT,
/// Berserker
	EV_FOOTSTEP_LEFT_PRINT,		// топот, следы
	EV_FOOTSTEP_RIGHT_PRINT,
	EV_FOOTSTEP_DOUBLE_PRINT,	// топот от двух шагов, следы
	EV_FOOTSTEP2,				// Тихие шаги, без следов
	EV_FOOTSTEP2_LEFT_PRINT,	// тихий топот, следы
	EV_FOOTSTEP2_RIGHT_PRINT,
	EV_FOOTSTEP_DOUBLE,			// Топот от двух шагов
	EV_FALLSHORT_PRINT,
	EV_FALL_PRINT,
	EV_FALLFAR_PRINT
} entity_event_t;

// pmove->pm_flags
#define	PMF_DUCKED			1
#define	PMF_JUMP_HELD		2
#define	PMF_ON_GROUND		4
#define	PMF_TIME_WATERJUMP	8	// pm_time is waterjump
#define	PMF_TIME_LAND		16	// pm_time is time before rejump
#define	PMF_TIME_TELEPORT	32	// pm_time is non-moving time
#define PMF_NO_PREDICTION	64	// temporarily disables prediction (used for grappling hook)
#define PMF_DOUBLE_JUMP		128

//
// button bits
//
#define	BUTTON_ATTACK		1
#define	BUTTON_USE			2
#define BUTTON_FLASHLIGHT	64		// flashlight
#define	BUTTON_ANY			128			// any key whatsoever


typedef struct csurface_s
{
	char	name[32];	/// было 16
	int		flags;
	int		value;
	byte	color_r;
	byte	color_g;
	byte	color_b;
	byte	color_r1;
	byte	color_g1;
	byte	color_b1;
} csurface_t;


// a trace is returned when a box is swept through the world
typedef struct
{
	bool		allsolid;	// if true, plane is not valid
	bool		startsolid;	// if true, the initial point was in a solid area
	float		fraction;	// time completed, 1.0 = didn't hit anything
	vec3_t		endpos;		// final position
	cplane_t	plane;		// surface normal at impact
	csurface_t	*surface;	// surface hit
	int			contents;	// contents on other side of surface hit
	struct edict_s	*ent;		// not set by CM_*() functions
} trace_t;


#define	MAXTOUCH	32

typedef struct
{
	// state (in / out)
	pmove_state_t	s;

	// command (in)
	usercmd_t		cmd;
	bool			snapinitial;	// if s has been changed outside pmove

	// results (out)
	int			numtouch;
	struct edict_s	*touchents[MAXTOUCH];

	vec3_t		viewangles;			// clamped
	float		viewheight;

	vec3_t		mins, maxs;			// bounding box size

	struct edict_s	*groundentity;
	int			watertype;
	int			waterlevel;

	// callbacks to test the world
	trace_t		(*trace) (vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, int mask);
	int			(*pointcontents) (vec3_t point);
} pmove_t;


typedef struct vidmode_s
{
///	const char *description;		// not used
	int         width, height;
///	int         mode;				// not used
} vidmode_t;


#define MAX_VID_NUM_MODES	64
///#define VID_NUM_MODES ( sizeof( vid_modes ) / sizeof( vid_modes[0] ) )

#define	WINDOW_STYLE	(WS_OVERLAPPED|WS_BORDER|WS_CAPTION|WS_VISIBLE)

typedef enum
{
	rserr_ok,
	rserr_invalid_fullscreen,
	rserr_invalid_mode,
	rserr_unknown
} rserr_t;


#define	TEXNUM_IMAGES		1


typedef struct
{
	int 		length;
	int 		loopstart;
///	int 		speed;			// not needed, because converted on load?
	int 		width;
	int 		stereo;
	byte		data[1];		// variable sized
} sfxcache_t;

typedef struct sfx_s
{
	char 		name[MAX_QPATH];
	int			registration_sequence;
	sfxcache_t	*cache;
	char 		*truename;
	unsigned	hash;			// hash for name
	unsigned	lockMSecs;		// кол-во миллисекунд, заданных для блокировки звучания (ставить для предотвращения какофонии в местах где слишком много источников такого звука)
	int			unLockTime;		// Время (Sys_Milliseconds() + lockMSecs) по достижении которого будет разрешено воспроизвести данный звук
} sfx_t;


#define		MAX_SFX_Q2		(MAX_SOUNDS_Q2*2)
#define		MAX_SFX_BERS	(MAX_SOUNDS_BERS*2)

typedef struct
{
	int			rate;
	int			width;
	int			channels;
	int			loopstart;
	int			samples;
	int			dataofs;		// chunk starts this many bytes from file start
	int			format;		// from Q3
} wavinfo_t;

typedef struct
{
	int			channels;
	int			samples;				// mono samples in buffer
	int			samplepos;				// in mono samples
	int			samplebits;
	int			speed;
	byte		*buffer;
} dma_t;

// a playsound_t will be generated by each call to S_StartSound,
// when the mixer reaches playsound->begin, the playsound will
// be assigned to a channel
typedef struct playsound_s
{
	struct playsound_s	*prev, *next;
	sfx_t		*sfx;
	float		volume;
	float		attenuation;
	int			entnum;
	int			entchannel;
	bool		fixed_origin;	// use origin field instead of entnum's origin
	vec3_t		origin;
	unsigned	begin;			// begin on this sample
} playsound_t;

#define		MAX_PLAYSOUNDS	128

typedef struct
{
	sfx_t		*sfx;			// sfx number
	int			leftvol;		// 0-255 volume
	int			rightvol;		// 0-255 volume
	int			end;			// end time in global paintsamples
	int 		pos;			// sample position in sfx
	int			looping;		// where to loop, -1 = no looping OBSOLETE?
	int			entnum;			// to allow overriding a specific sound
	int			entchannel;		//
	vec3_t		origin;			// only use if fixed_origin is set
	float		dist_mult;		// distance multiplier (attenuation/clipK)
	int			master_vol;		// 0-255 master volume
	bool		fixed_origin;	// use origin instead of fetching entnum's origin
	bool		autosound;		// from an entity->sound, cleared each frame
} channel_t;

#define	MAX_CHANNELS			32
#define	WAV_BUFFERS				64
#define	WAV_BUFFER_SIZE			0x0400
#define	WAV_MASK				0x3F
#define SECONDARY_BUFFER_SIZE	0x10000


// a sound without an ent or pos will be a local only sound
#define	SND_VOLUME		(1<<0)		// a byte
#define	SND_ATTENUATION	(1<<1)		// a byte
#define	SND_POS			(1<<2)		// three coordinates
#define	SND_ENT			(1<<3)		// a short 0-2: channel, 3-12: entity
#define	SND_OFFSET		(1<<4)		// a byte, msec offset from frame start
#define	SND_INDEX16		(1<<5)		// Berserker: read short instead byte for sound index
#define	SND_LARGE_POS	(1<<6)		// LARGE_MAP_SIZE

#define DEFAULT_SOUND_PACKET_VOLUME	1.0
#define DEFAULT_SOUND_PACKET_ATTENUATION 1.0

// only begin attenuating sound volumes when outside the FULLVOLUME range
#define		SOUND_FULLVOLUME	80
#define		SOUND_LOOPATTENUATE	0.003

typedef struct
{
	int			left;
	int			right;
} portable_samplepair_t;

#define	PAINTBUFFER_SIZE	4096	/// 2048
#define	MAX_RAW_SAMPLES		16384	/// 8192

// directory searching
#define SFF_ARCH    0x01
#define SFF_HIDDEN  0x02
#define SFF_RDONLY  0x04
#define SFF_SUBDIR  0x08
#define SFF_SYSTEM  0x10

// sound attenuation values
#define	ATTN_NONE               0	// full volume the entire level
#define	ATTN_NORM               1
#define	ATTN_IDLE               2
#define	ATTN_STATIC             3	// diminish very rapidly with distance

typedef enum {RD_NONE, RD_CLIENT, RD_PACKET} redirect_t;
#define	SV_OUTPUTBUF_LENGTH	(MAX_MSGLEN - 16)

#define	MAX_MENU_DEPTH	8

typedef struct
{
	void	(*draw) (void);
	const char *(*key) (int k);
} menulayer_t;

#define NUM_CURSOR_FRAMES 15
#define	MAIN_ITEMS	6

typedef struct _tag_menuframework
{
	int x, y;
	int	cursor;

	int	nitems;
	int nslots;
	void *items[64];

	const char *statusbar;

	void (*cursordraw)( struct _tag_menuframework *m );

} menuframework_s;

typedef struct
{
	int type;
	const char *name;
	int x, y;
	menuframework_s *parent;
	int cursor_offset;
	int	localdata[4];
	unsigned flags;

	const char *statusbar;

	void (*callback)( void *self );
	void (*statusbarfunc)( void *self );
	void (*ownerdraw)( void *self );
	void (*cursordraw)( void *self );
} menucommon_s;

typedef struct
{
	menucommon_s generic;
} menuaction_s;

typedef struct
{
	menucommon_s generic;
} menuseparator_s;

#define MTYPE_SLIDER		0
///#define MTYPE_LIST			1		// NOT USED ?
#define MTYPE_ACTION		2
#define MTYPE_SPINCONTROL	3
#define MTYPE_SEPARATOR  	4
#define MTYPE_FIELD			5

#define QMF_LEFT_JUSTIFY	0x00000001
#define QMF_GRAYED			0x00000002
#define QMF_NUMBERSONLY		0x00000004

#define MAXMENUITEMS	64

typedef struct
{
	menucommon_s generic;

	int curvalue;

	const char **itemnames;
} menulist_s;

typedef struct
{
	menucommon_s generic;

	char		buffer[80];
	int			cursor;
	int			length;
	int			visible_length;
	int			visible_offset;
} menufield_s;

#define RCOLUMN_OFFSET  16
#define LCOLUMN_OFFSET -16
#define SLIDER_RANGE	10

typedef struct
{
	menucommon_s generic;

	float minvalue;
	float maxvalue;
	float curvalue;

	float range;
} menuslider_s;


#define	MAX_SAVEGAMES	15

#define	MAX_DEMOS			38
#define	MAX_DEMO_NAMELEN	18

#define	MAX_MAPS			256
#define	MAX_MAP_NAMELEN		MAX_QPATH-1

#define	MAX_MODS			46
#define	MAX_MOD_NAMELEN		18

#define NUM_ADDRESSBOOK_ENTRIES 9
#define	NO_SERVER_STRING		"<no server>"

// destination class for gi.multicast()
typedef enum
{
MULTICAST_ALL,
MULTICAST_PHS,
MULTICAST_PVS,
MULTICAST_ALL_R,
MULTICAST_PHS_R,
MULTICAST_PVS_R
} multicast_t;


// 1/32 epsilon to keep floating point happy
#define	DIST_EPSILON	(0.03125)

typedef struct
{
	vec3_t		boxmins, boxmaxs;// enclose the test object along entire move
	float		*mins, *maxs;	// size of the moving object
	vec3_t		mins2, maxs2;	// size when clipping against mosnters
	float		*start, *end;
	trace_t		trace;
	edict_t		*passedict;
	int			contentmask;
} moveclip_t;


#define	AREA_DEPTH	4
#define	AREA_NODES	32


// gi.BoxEdicts() can return a list of either solid or trigger entities
// FIXME: eliminate AREA_ distinction?
#define	AREA_SOLID		1
#define	AREA_TRIGGERS	2

// (type *)STRUCT_FROM_LINK(link_t *link, type, member)
// ent = STRUCT_FROM_LINK(link,entity_t,order)
// FIXME: remove this mess!
#define	STRUCT_FROM_LINK(l,t,m) ((t *)((byte *)l - (byte *)&(((t *)NULL)->m)))

#define	EDICT_FROM_AREA(l) STRUCT_FROM_LINK(l,edict_t,area)

// upper design bounds
// leaffaces, leafbrushes, planes, and verts are still bounded by
// 16 bit short limits
#define	MAX_MAP_MODELS		1024
#define	MAX_MAP_BRUSHES		16384	// was 8192, LARGE_MAP_SIZE support
// #define	MAX_MAP_ENTITIES	2048	// unused
#define	MAX_MAP_ENTSTRING	0x80000	// was 0x40000, LARGE_MAP_SIZE support
#define	MAX_MAP_TEXINFO		8192

///#define	MAX_MAP_AREAS		256
#define	MAX_MAP_AREAPORTALS	1024
#define	MAX_MAP_PLANES		65536
#define	MAX_MAP_NODES		65536
#define	MAX_MAP_BRUSHSIDES	65536
//#define	MAX_MAP_LEAFS		65536	/// defined above
#define	MAX_MAP_VERTS		65536
#define	MAX_MAP_FACES		65536
#define	MAX_MAP_LEAFFACES	65536
#define	MAX_MAP_LEAFBRUSHES 65536
#define	MAX_MAP_PORTALS		65536
#define	MAX_MAP_EDGES		128000
#define	MAX_MAP_SURFEDGES	256000
#define	MAX_MAP_LIGHTING	0x200000
#define	MAX_MAP_VISIBILITY	0x100000


typedef struct cmodel_s
{
	vec3_t		mins, maxs;
	vec3_t		origin;		// for sounds or lights
	int			headnode;
} cmodel_t;

typedef struct mapsurface_s  // used internally due to name len probs //ZOID
{
	csurface_t	c;
	char		rname[32];
} mapsurface_t;

typedef struct
{
	cplane_t	*plane;
	int			children[2];		// negative numbers are leafs
} cnode_t;

typedef struct
{
	cplane_t	*plane;
	mapsurface_t	*surface;
} cbrushside_t;

typedef struct
{
	int		contents;
	int		cluster;
	int		area;
	WORD	firstleafbrush;
	WORD	numleafbrushes;
} cleaf_t;

typedef struct
{
	int			contents;
	int			numsides;
	int			firstbrushside;
	int			checkcount;		// to avoid repeated testings
} cbrush_t;

typedef struct
{
	int		numareaportals;
	int		firstareaportal;
	int		floodnum;			// if two areas have equal floodnums, they are connected
	int		floodvalid;
} carea_t;

// the visibility lump consists of a header with a count, then
// byte offsets for the PVS and PHS of each cluster, then the raw
// compressed bit vectors
#define	DVIS_PVS	0
#define	DVIS_PHS	1

typedef struct
{
	int			numclusters;
	int			bitofs[8][2];	// bitofs[numclusters][2]
} dvis_t;

// each area has a list of portals that lead into other areas
// when portals are closed, other areas may not be visible or
// hearable even if the vis info says that it should be
typedef struct
{
	int		portalnum;
	int		otherarea;
} dareaportal_t;


typedef struct
{
	vec3_t		origin;			// full float precision
	vec3_t		velocity;		// full float precision

	vec3_t		forward, right, up;
	float		frametime;


	csurface_t	*groundsurface;
	cplane_t	groundplane;
	int			groundcontents;

	vec3_t		previous_origin;
	bool		ladder;
} pml_t;


#define	STEPSIZE	18

#define NUMVERTEXNORMALS	162

#define	GAME_API_VERSION	1	// Версия game.dll	/// 3 in Q2

#define DISTORT_WEAPON

// entity_state_t->effects
// Effects are things handled on the client side (lights, particles, frame animations)
// that happen constantly on the given entity.
// An entity that has effects will be sent to the client
// even if it has a zero index model.
#define	EF_ROTATE			0x00000001		// rotate (bonus items)
#define	EF_GIB				0x00000002		// leave a trail
	// Berserker
	#define	EF_INVIS			0x00000004
#define	EF_BLASTER			0x00000008		// redlight + trail
#define	EF_ROCKET			0x00000010		// redlight + trail
#define	EF_GRENADE			0x00000020
#define	EF_HYPERBLASTER		0x00000040
#define	EF_BFG				0x00000080
#define EF_COLOR_SHELL		0x00000100
#define EF_POWERSCREEN		0x00000200
#define	EF_ANIM01			0x00000400		// automatically cycle between frames 0 and 1 at 2 hz
#define	EF_ANIM23			0x00000800		// automatically cycle between frames 2 and 3 at 2 hz
#define EF_ANIM_ALL			0x00001000		// automatically cycle through all frames at 2hz
#define EF_ANIM_ALLFAST		0x00002000		// automatically cycle through all frames at 10hz
#define	EF_FLIES			0x00004000
#define	EF_QUAD				0x00008000
#define	EF_PENT				0x00010000
#define	EF_TELEPORTER		0x00020000		// particle fountain
#define EF_FLAG1			0x00040000
#define EF_FLAG2			0x00080000
//BERSERKER
#define EF_FLASHLIGHT		0x00100000/// confilcted!!!	do not use with old q2!!!
#define EF_FLASHLIGHT_NOENT	0x00200000/// confilcted!!!	do not use with old q2!!!
// RAFAEL
#define EF_IONRIPPER		0x00100000/// conflicted!!! do not use with bers@q2!!!
#define EF_GREENGIB			0x00200000/// conflicted!!! do not use with bers@q2!!!
#define	EF_BLUEHYPERBLASTER 0x00400000
#define EF_SPINNINGLIGHTS	0x00800000
#define EF_PLASMA			0x01000000
#define EF_TRAP				0x02000000
//ROGUE
#define EF_TRACKER			0x04000000
#define	EF_DOUBLE			0x08000000
#define	EF_SPHERETRANS		0x10000000
#define EF_TAGTRAIL			0x20000000
#define EF_HALF_DAMAGE		0x40000000
#define EF_TRACKERTRAIL		0x80000000


#define SPLASH_UNKNOWN		0
#define SPLASH_SPARKS		1
#define SPLASH_BLUE_WATER	2
#define SPLASH_BROWN_WATER	3
#define SPLASH_SLIME		4
#define	SPLASH_LAVA			5
#define SPLASH_BLOOD		6


#define POWERSUIT_SCALE			1.0		///2.0F
#define POWERSUIT_SCALE_WEAPON	0.5		/// HACK: for weapon models

#define BACKFACE_EPSILON	0.01

typedef enum
{
	ex_free, ex_explosion, ex_misc, ex_flash, ex_mflash, ex_poly, ex_poly2
} exptype_t;


typedef struct
{
	exptype_t	type;
	entity_t	ent;

	int			frames;
	float		light;
	vec3_t		lightcolor;
	float		start;
	int			baseframe;
	vec3_t		light_origin;	/// Это поле позволяет развести модель и источник света.
} explosion_t;


typedef enum
{
	dt_mul,
	dt_add
} dtype_t;

typedef struct defDecal_s
{
	vec3_t	center;
	vec3_t	normal;
	image_t	*texture;
	float	size;
	float	angle;
	dtype_t	dtype;
	byte	style;
	byte	cl_style;
	vec3_t	offset;			// for decaling bmodels obly
} defDecal_t;

/// Если поменял тут, не забудь сменить в WriteClientFile и CL_ReadClientFile !!!
typedef struct decal_s
{
	float		angle;
	struct decal_s	*next;
	float		die;
	float		lifetime;	// seconds
///	dtype_t		type;
///	int			texture;	// texture object of particle
///	vec3_t		origin;		// чисто для релайтера
///	vec3_t		dir;		// чисто для релайтера
	int			size;							// чисто для сохранения клиентского состояния
	byte		shot;							// чисто для сохранения клиентского состояния
	char		decalTextureName[MAX_QPATH];	// чисто для сохранения клиентского состояния
	dtype_t		decalType;						// чисто для сохранения клиентского состояния
	vec3_t		center;							// чисто для сохранения клиентского состояния
	vec3_t		normal;							// чисто для сохранения клиентского состояния
	//geometry of decal
	int	vertexCount, triangleCount;
	vec3_t		vertexArray[MAX_DECAL_VERTICES];
	float		texcoordArray[MAX_DECAL_VERTICES][2];
	byte		triangleArray[MAX_DECAL_TRIANGLES][3];
	image_t		*images[MAX_DECAL_TRIANGLES];
	struct  msurface_s	*surfs[MAX_DECAL_TRIANGLES];
	dtype_t		types[MAX_DECAL_TRIANGLES];
	int			model_id;	// if NULL - bsp decal (using firstmodelsurface as model_id)
	byte		style;
	byte		cl_style;
	defDecal_t	*defDecal;
} decal_t;

// temp entity events
//
// Temp entity events are for things that happen
// at a location seperate from any existing entity.
// Temporary entity messages are explicitly constructed
// and broadcast.
typedef enum
{
	TE_GUNSHOT,
	TE_BLOOD,
	TE_BLASTER,
	TE_RAILTRAIL,
	TE_SHOTGUN,
	TE_EXPLOSION1,
	TE_EXPLOSION2,
	TE_ROCKET_EXPLOSION,
	TE_GRENADE_EXPLOSION,
	TE_SPARKS,
	TE_SPLASH,
	TE_BUBBLETRAIL_WATER,
	TE_SCREEN_SPARKS,
	TE_SHIELD_SPARKS,
	TE_BULLET_SPARKS,
	TE_LASER_SPARKS,
	TE_PARASITE_ATTACK,
	TE_ROCKET_EXPLOSION_WATER,
	TE_GRENADE_EXPLOSION_WATER,
	TE_MEDIC_CABLE_ATTACK,
	TE_BFG_EXPLOSION,
	TE_BFG_BIGEXPLOSION,
	TE_BOSSTPORT,			// used as '22' in a map, so DON'T RENUMBER!!!
	TE_BFG_LASER,

//// This is't the Q2 standard temp entities!
	TE_GRAPPLE_CABLE,
	TE_WELDING_SPARKS,
	TE_GREENBLOOD,
	TE_BLUEHYPERBLASTER,
	TE_PLASMA_EXPLOSION,
	TE_TUNNEL_SPARKS,
//ROGUE
	TE_BLASTER2,
	TE_RAILTRAIL2,
	TE_FLAME,
	TE_LIGHTNING,
	TE_DEBUGTRAIL,
	TE_PLAIN_EXPLOSION,
	TE_FLASHLIGHT,
	TE_FORCEWALL,
	TE_HEATBEAM,
	TE_MONSTER_HEATBEAM,
	TE_STEAM,
	TE_BUBBLETRAIL2,
	TE_MOREBLOOD,
	TE_HEATBEAM_SPARKS,
	TE_HEATBEAM_STEAM,
	TE_CHAINFIST_SMOKE,
	TE_ELECTRIC_SPARKS,
	TE_TRACKER_EXPLOSION,
	TE_TELEPORT_EFFECT,
	TE_DBALL_GOAL,
	TE_WIDOWBEAMOUT,
	TE_NUKEBLAST,
	TE_WIDOWSPLASH,
	TE_EXPLOSION1_BIG,
	TE_EXPLOSION1_NP,
	TE_FLECHETTE,
//BERSERKER
	TE_BUBBLETRAIL_LAVA,
	TE_BUBBLETRAIL_SLIME,
	TE_RAILTRAIL_BERS_NEW,
	TE_MODEL_EXPLOSION,
	TE_EXPLOSION_NUM,
	TE_EXPLOSION_NUM_GLASS,
	TE_EXPLOSION_ROCKET,

	TE_GIBS_ARM,				// GIBS_ARM			!!! TE_GIBS_ARM должна быть первой в списке TE_GIBS_* !!!
	TE_GIBS_LEG,				// GIBS_LEG
	TE_GIBS_HEAD,				// GIBS_HEAD
	TE_GIBS_MAKRON,				// GIBS_MAKRON
	TE_GIBS_GLADIATOR,			// GIBS_GLADIATOR
	TE_GIBS_INFANTRY,			// GIBS_INFANTRY
	TE_GIBS_FLIPPER,			// GIBS_FLIPPER
	TE_GIBS_MUTANT,				// GIBS_MUTANT
	TE_GIBS_HOVER,				// GIBS_HOVER
	TE_GIBS_GUNNER,				// GIBS_GUNNER
	TE_GIBS_FLOATER,			// GIBS_FLOATER
	TE_GIBS_FLYER,				// GIBS_FLYER
	TE_GIBS_SOLDIER,			// GIBS_SOLDIER
	TE_GIBS_TANK,				// GIBS_TANK
	TE_GIBS_INSANE,				// GIBS_INSANE
	TE_GIBS_DEADSOLDIER,		// GIBS_DEADSOLDIER
	TE_GIBS_BARREL_DEBRIS,		// GIBS_BARREL_DEBRIS
	TE_GIBS_PLAYER,				// GIBS_PLAYER
	TE_GIBS_BRAIN,				// GIBS_BRAIN
	TE_GIBS_PARASITE,			// GIBS_PARASITE
	TE_GIBS_MEDIC,				// GIBS_MEDIC
	TE_GIBS_BERSERK,			// GIBS_BERSERK
	TE_GIBS_CHICK,				// GIBS_CHICK
	TE_GIBS_JORG,				// GIBS_JORG
	TE_GIBS_BOSS2,				// GIBS_BOSS2
	TE_GIBS_SUPERTANK,			// GIBS_SUPERTANK
	TE_GIBS_2BONES_4MEAT_HEAD2	// GIBS_2BONES_4MEAT_HEAD2
} temp_event_t;

typedef enum
{
	GIBS_ARM,
	GIBS_LEG,
	GIBS_HEAD,
	GIBS_MAKRON,
	GIBS_GLADIATOR,
	GIBS_INFANTRY,
	GIBS_FLIPPER,
	GIBS_MUTANT,
	GIBS_HOVER,
	GIBS_GUNNER,
	GIBS_FLOATER,
	GIBS_FLYER,
	GIBS_SOLDIER,
	GIBS_TANK,
	GIBS_INSANE,
	GIBS_DEADSOLDIER,
	GIBS_BARREL_DEBRIS,
	GIBS_PLAYER,
	GIBS_BRAIN,
	GIBS_PARASITE,
	GIBS_MEDIC,
	GIBS_BERSERK,
	GIBS_CHICK,
	GIBS_JORG,
	GIBS_BOSS2,
	GIBS_SUPERTANK,
	GIBS_2BONES_4MEAT_HEAD2		// generic human gibs (used for actor)
} gibs_t;

#define	CLMOD_SFX_NONE			0
#define	CLMOD_SFX_MSHELL		1
#define	CLMOD_SFX_SSHELL		2
#define	CLMOD_SFX_GLASS			3
#define	CLMOD_SFX_FLESH			4
#define	CLMOD_SFX_METAL			5
#define	CLMOD_SFX_GEAR			6

#define	CLMOD_EF_NONE			0
#define	CLMOD_EF_GIB			1
#define	CLMOD_EF_GREENGIB		2
#define	CLMOD_EF_SMOKE			3



//ROGUE
typedef struct cl_sustain
{
	int			id;
	int			type;
	int			endtime;
	int			nextthink;
	int			thinkinterval;
	vec3_t		org;
	vec3_t		dir;
	int			color;
	int			count;
	int			magnitude;
	void		(*think)(struct cl_sustain *self);
} cl_sustain_t;

#define MAX_SUSTAINS		32


// PMM
#define INSTANT_PARTICLE	-10000.0


/// Если поменял тут, не забудь сменить в WriteClientFile и CL_ReadClientFile !!!
typedef struct cparticle_s
{
	struct cparticle_s	*next;
	float		time;
	vec3_t		org;
	vec3_t		lastOrg;
	vec3_t		vel;
	vec3_t		accel;
	vec3_t		length;
	int			size;
	byte		color_r0;
	byte		color_g0;
	byte		color_b0;
	byte		color_r1;
	byte		color_g1;
	byte		color_b1;
	parttype_t	type;
	float		alpha;
	float		alphavel;
	int			flags;
} cparticle_t;


#define	PARTICLE_GRAVITY	40
#define	MAX_EXPLOSIONS		32
#define	MAX_LASERS			32
typedef struct
{
	entity_t	ent;
	int			endtime;
} laser_t;

#define	MAX_BEAMS	32
typedef struct
{
	int		entity;
	int		dest_entity;
	struct model_s	*model;
	int		endtime;
	vec3_t	offset;
	vec3_t	start, end;
} beam_t;


typedef struct
{
	char	*name;
	char	*value;
	cvar_t	*var;
} cheatvar_t;

typedef enum {mod_bad, mod_brush, mod_sprite, mod_sprite2, mod_alias, mod_alias_md3 } modtype_t;

typedef struct
{
	vec3_t		mins, maxs;
	vec3_t		origin;		// for sounds or lights
	float		radius;
	int			headnode;
	int			visleafs;		// not including the solid leaf 0
	int			firstface, numfaces;
} mmodel_t;

typedef struct
{
	vec3_t		position;
} mvertex_t;

typedef struct
{
	WORD		v[2];
	unsigned	cachededgeoffset;
} medge_t;

typedef struct mtexinfo_s
{
	float		vecs[2][4];
	int			flags;
	int			numframes;
	struct mtexinfo_s	*next;		// animation chain
	image_t		*image;
	image_t		*bump;
	image_t		*light;
} mtexinfo_t;

#define	VERTEXSIZE	16		/// (10 = 7 + 3(normal) + 3(s-vector) + 3(t-vector))


typedef struct glpoly_s
{
	struct	glpoly_s	*next;
//	struct	glpoly_s	*chain;		// Berserker: not used?
	int		numverts;
	int		lightTimestamp;
///	bool	light;					// принадлежность к SURF_LIGHT	(no more used...)
	struct	glpoly_s	**neighbours;
	float	verts[4][VERTEXSIZE];	// variable sized (x/y/z s1 t1 s2 t2 normal_x/y/z)
} glpoly_t;



#define		MAX_FOG_AREAS	64	// Максимальное количество туманных областей на карте

typedef struct
{
	vec3_t		color;			// цвет тумана
	vec3_t		origin;			// for bbox
	vec3_t		origin2;		// for bbox
	float		density;
	float		minmax[6];
	bool		ed;				// туманный объект, HACK, from Editor
} fog_t;


typedef struct msurface_s
{
	int			visframe;		// should be drawn when node is crossed
	int			drawframe;		// berserker

	cplane_t	*plane;
	int			flags;

	int			firstedge;	// look up in model->surfedges[], negative numbers
	int			numedges;	// are backwards edges

	short		texturemins[2];
	short		extents[2];

	int			light_s, light_t;	// gl lightmap coordinates

	glpoly_t	*polys;				// multiple if warped
	struct	msurface_s	*texturechain;
///	struct	msurface_s	*shadowchain;
	struct	msurface_s	*fogchain;
	struct	msurface_s	*uwchain;
	struct	msurface_s	*distortchain;
	struct	msurface_s	*mirrorchain;
///	struct	msurface_s	*scenechain;
	fog_t				*fog;

	mtexinfo_t	*texinfo;

	int			lightmaptexturenum;
	byte		*samples;		// [numstyles*surfsize]

	short		mvm_cache_idx;	/// Berserker: uses for cache trans brushes
	short		mvm_cache_idx2;	/// Berserker: uses for cache distorts brushes
	short		mvm_cache_idx3;	/// Berserker: uses for cache caustics brushes
/// Berserker: uses for light flares
	entity_t	*ent;			/// NULL for worldmodel
	vec3_t		center;
	unsigned	scale_s, scale_t;
	vec3_t		mins, maxs;
	vec3_t		polygonOffset;		// offset for decals
///	// lighting info
///	int			dlightframe;
///	int			dlightbits;		// 32 метки, показывающие, какие DLights касаются данной поверхности (MAX_DLIGHTS = 32)
} msurface_t;


#define	MAX_MATERIALS		64
#define	MAX_GLTEXTURES		8192	///1024				не превышать 65536, т.к. сортировщик сурфов выделяет под номер текстуры 16-бит.
#define	TEXNUM_LIGHTMAPS	MAX_GLTEXTURES	///1024
#define LIGHTMAP_BYTES		4
///#define	MAX_LIGHTMAPS	8		/// 128		/// Berserker: не нужен этот лимит, т.к. стены с лайтмапой рисуем в один проход!
   #define	MAX_LIGHTMAPS	256					/// Berserker: всё же оставим этот лимит, чтобы при сортировке сурфов в батчи соблюдать лимит в 8 бит.
#define	LIGHTMAP_SIZE	1024	/// 256			// GL_MAX_TEXTURE_SIZE	1024 for Radeon 7x00, Radeon 8500, i810 or better, Riva TNT
												//						2048 for GeForce2MX
												//						4096 for GeForce3, 4, FX


typedef struct
{
	int	current_lightmap_texture;
	int	allocated[LIGHTMAP_SIZE];
} gllightmapstate_t;

typedef struct mleaf_s
{
// common with node
	int			contents;		// wil be a negative contents number
	int			visframe;		// node needs to be traversed if current

	float		minmaxs[6];		// for bounding box culling

	struct mnode_s	*parent;

// leaf specific
	int			cluster;
	int			area;

	msurface_t	**firstmarksurface;
	int			nummarksurfaces;
} mleaf_t;


typedef struct mnode_s
{
// common with leaf
	int			contents;		// -1, to differentiate from leafs
	int			visframe;		// node needs to be traversed if current

	float		minmaxs[6];		// for bounding box culling

	struct mnode_s	*parent;

// node specific
	cplane_t	*plane;
	struct mnode_s	*children[2];

	WORD		firstsurface;
	WORD		numsurfaces;
} mnode_t;

#define	MAX_TRIANGLES	4096
#define MAX_VERTS		2048
///#define MAX_FRAMES		512		not used
#define MAX_MD2SKINS	32
#define	MAX_SKINNAME	64

#define MAX_DISPLAYNAME 16
#define MAX_PLAYERMODELS 1024


// planes (x&~1) and (x&~1)+1 are always opposites
typedef struct
{
	float	normal[3];
	float	dist;
	int		type;		// PLANE_X - PLANE_ANYZ ?remove? trivial to regenerate
} dplane_t;

typedef struct
{
	short	s;
	short	t;
} dstvert_t;

typedef struct
{
	float s;
	float t;
} fstvert_t;


typedef struct mtriangle_s
{
	int		neighbours[3];
} neighbours_t;


typedef struct
{
	int			mesh;				// Номер меша модели, в которой указана точка vert. O - для MD2. -1 - если источник не определен.
	unsigned	tri;				// Номер полигона, к которому "привязываем" источник. От него считаем нормаль для создания подпространства, в котором уже будем двигать лайт (с помощью origin).
	vec3_t		color;				// Цвет источника света
	byte		style;				// Стиль мигания
	byte		cl_style;			// Клиентский стиль мигания
	float		radius;				// Радиус источника (всегда шаровидный!)
	int			filtercube_start;	// Начальный кадр светофильтра
	int			filtercube_end;		// Конечный кадр светофильтра
	float		framerate;			// Скорость анимации светофильтра
	float		_cone;				// Конус действия источника
	float		distance;			// Расстояние от центральной точки треугольника tri до источника света
	vec3_t		angles;				// angles of the cubemap filter
	vec3_t		rspeed;				// rotation speed of cube map
	unsigned	skinbits;			// MD2 ONLY: Номера шкур, для которых действует этот лайт. Т.к. MAX_MD2SKINS=32, то зададим это битовой маской.
	int			frame_start;		// Начальный кадр действия лайта
	int			frame_end;			// Конечный кадр действия лайта
} modlight_t;

typedef struct
{
	int			mesh;				// Номер меша модели, в которой указана точка vert. O - для MD2. -1 - если источник не определен.
	unsigned	tri;				// Номер полигона, к которому "привязываем" источник. От него считаем нормаль для создания подпространства, в котором уже будем двигать лайт (с помощью origin).
	float		distance;			// Расстояние от центральной точки треугольника tri до источника
	unsigned	skinbits;			// MD2 ONLY: Номера шкур, для которых действует этот источник. Т.к. MAX_MD2SKINS=32, то зададим это битовой маской.
	byte		style;				// Стиль мигания
	byte		cl_style;			// Клиентский стиль мигания
	float		cone;				// Конус действия источника
	parttype_t	emit;					// (emit."filter")
	float		vel;					// скорость движения
	char		gravity;
	vec3_t		startcolor;				// если startcolor[0]!=0, то color override!
	vec3_t		endcolor;
	float		alphavel;
	int			numemits;
	int			frame_start;		// Начальный кадр действия эмиттера
	int			frame_end;			// Конечный кадр действия эмиттера
} modemit_t;

#define		MAX_MODEL_LIGHTS		32
#define		MAX_MODEL_EMITS			16

typedef struct model_s
{
	char		name[MAX_QPATH];
	unsigned	hash;

	int			registration_sequence;

	modtype_t	type;
	int			numframes;

	int			flags;				// Berserker: not used in Q2, but used in Berserker
	byte		hit_sound_index;	// for debris/gibs only
	byte		trail_index;		// for debris/gibs only
//
// volume occupied by the model graphics
//
	vec3_t		mins, maxs;
	float		radius;

//
// solid volume for clipping (not used)
//
///	bool		clipbox;
///	vec3_t		clipmins, clipmaxs;

///	Berserker: using for 3d hud
	vec3_t		center;

//
// brush model
//
	int			firstmodelsurface, nummodelsurfaces;
	int			lightmap;		// only for submodels

	int			numsubmodels;
	mmodel_t	*submodels;

	int			numplanes;
	cplane_t	*planes;

	int			numleafs;		// number of visible leafs, not counting 0
	mleaf_t		*leafs;

	int			numvertexes;
	mvertex_t	*vertexes;

	int			numedges;
	medge_t		*edges;

	int			numnodes;
	int			firstnode;
	mnode_t		*nodes;

	int			numtexinfo;
	mtexinfo_t	*texinfo;

	int			numsurfaces;
	msurface_t	*surfaces;

	int			numsurfedges;
	int			*surfedges;

	int			nummarksurfaces;
	msurface_t	**marksurfaces;

	dvis_t		*vis;

	byte		*lightdata;

	// for alias models and skins (MD2 and Sprites only!)
	image_t		*skins[MAX_MD2SKINS];
	image_t		*bumps[MAX_MD2SKINS];
	image_t		*lights[MAX_MD2SKINS];
	byte		*normals;
	byte		*binormals;
	byte		*tangents;
	neighbours_t *triangles;
	float		*st;			// using if vbo disabled (malloc, no hunk)

	modlight_t	mod_lights[MAX_MODEL_LIGHTS];
	modemit_t	mod_emits[MAX_MODEL_EMITS];

	WORD		max_meshes;	// для вывода на экран
	WORD		max_tris;	// для вывода на экран

/*
	bool		geo_changed_;
	int			framecount_;
	int			old_frame_;
	float		old_backlerp_;
	vec3_t		old_angles_;
	char		old_nobump_;
	unsigned	vbo_st;
	unsigned	vbo_xyz;
	unsigned	vbo_lightp;
	unsigned	vbo_tsh;
	unsigned	vbo_fx;
	unsigned	vbo_normals;
	unsigned	vbo_tangents;
	unsigned	vbo_binormals;
*/
vertCache_t	*vbo_st;

	bool		md3_has_solid_mesh;		// только для md3 и ase
	bool		md3_has_trans_mesh;		// только для md3 и ase
	float		scale;					// for md2, md3, ase only
	bool		invert;					// for md2, md3, ase only

	int			extradatasize;
	void		*extradata;
} model_t;


/// Если поменял тут, не забудь сменить в WriteClientFile и CL_ReadClientFile !!!
typedef struct clentity_s
{
	struct clentity_s	*next;
	float		time;
	vec3_t		org;
	vec3_t		lastOrg;
	vec3_t		vel;
	vec3_t		accel;
	model_t		*model;
	vec3_t		ang;
	vec3_t		avel;
	float		alpha;
	float		alphavel;
	byte		flags;
	byte		trailcount;
	int			type;
	image_t		*skin;			// not NULL for player's custom skin
	image_t		*bump;
	image_t		*light;
} clentity_t;


#define	MAX_MOD_KNOWN	512

#define IDALIASHEADER		(('2'<<24)+('P'<<16)+('D'<<8)+'I')

// the glcmd format:
// a positive integer starts a tristrip command, followed by that many
// vertex structures.
// a negative integer starts a trifan command, followed by -x vertexes
// a zero indicates the end of the command list.
// a vertex consists of a floating point s, a floating point t,
// and an integer vertex index.
typedef struct
{
	int			ident;
	int			version;

	int			skinwidth;
	int			skinheight;
	int			framesize;		// byte size of each frame

	int			num_skins;
	int			num_xyz;
	int			num_st;			// greater than num_xyz for seams
	int			num_tris;
	int			num_glcmds;		// dwords in strip/fan command list
	int			num_frames;

	int			ofs_skins;		// each skin is a MAX_SKINNAME string
	int			ofs_st;			// byte offset from start for stverts	(not used)
	int			ofs_tris;		// offset for dtriangles
	int			ofs_frames;		// offset for first frame
	int			ofs_glcmds;
	int			ofs_end;		// end of file

} dmdl_t;


typedef struct
{
	short	index_xyz[3];
	short	index_st[3];
} dtriangle_t;

typedef struct
{
	byte	v[3];			// scaled byte to fit in frame mins/maxs
	byte	lightnormalindex;
} dtrivertx_t;

typedef struct
{
	float		scale[3];	// multiply byte verts by this
	float		translate[3];	// then add this
	char		name[16];	// frame name from grabbing			НЕ ИСПОЛЬЗУЕТСЯ, НО УБИРАТЬ НЕЛЬЗЯ, ЭТО MD2 СТАНДАРТ
	dtrivertx_t	verts[1];	// variable sized
} daliasframe_t;

#define ALIAS_VERSION		8
///#define	MAX_LBM_HEIGHT		480

#define IDBSPHEADER	(('P'<<24)+('S'<<16)+('B'<<8)+'I')	// little-endian "IBSP"
#define BSPVERSION	38

typedef struct
{
	int		fileofs, filelen;
} lump_t;

#define	LUMP_ENTITIES		0
#define	LUMP_PLANES			1
#define	LUMP_VERTEXES		2
#define	LUMP_VISIBILITY		3
#define	LUMP_NODES			4
#define	LUMP_TEXINFO		5
#define	LUMP_FACES			6
#define	LUMP_LIGHTING		7
#define	LUMP_LEAFS			8
#define	LUMP_LEAFFACES		9
#define	LUMP_LEAFBRUSHES	10
#define	LUMP_EDGES			11
#define	LUMP_SURFEDGES		12
#define	LUMP_MODELS			13
#define	LUMP_BRUSHES		14
#define	LUMP_BRUSHSIDES		15
#define	LUMP_POP			16
#define	LUMP_AREAS			17
#define	LUMP_AREAPORTALS	18
#define	HEADER_LUMPS		19

typedef struct
{
	int			ident;
	int			version;
	lump_t		lumps[HEADER_LUMPS];
} dheader_t;

typedef struct
{
	float	point[3];
} dvertex_t;

// note that edge 0 is never used, because negative edge nums are used for
// counterclockwise use of the edge in a face
typedef struct
{
	WORD	v[2];		// vertex numbers
} dedge_t;

typedef struct texinfo_s
{
	float		vecs[2][4];		// [s/t][xyz offset]
	int			flags;			// miptex flags + overrides
	int			value;			// light emission, etc
	char		texture[32];	// texturename.ext (textures/*.*)
	int			nexttexinfo;	// for animations, -1 = end of chain
} texinfo_t;

typedef struct
{
	WORD		planenum;
	short		side;

	int			firstedge;		// we must support > 64k edges
	short		numedges;
	short		texinfo;

// lighting info
#define	MAXLIGHTMAPS	4				/// Berserker FIXME: убрать в будущем, а щас - поддержка Q2-карт
	byte		styles[MAXLIGHTMAPS];	/// Berserker FIXME: убрать в будущем, а щас - поддержка Q2-карт
	int			lightofs;		// start of [numstyles*surfsize] samples
} dface_t;

typedef struct
{
	int			firstside;
	int			numsides;
	int			contents;
} dbrush_t;

typedef struct
{
	WORD	planenum;		// facing out of the leaf
	short	texinfo;
} dbrushside_t;

typedef struct
{
	int		numareaportals;
	int		firstareaportal;
} darea_t;

#define	SURF_PLANEBACK		2
#define	SURF_DRAWSKY		4
#define SURF_DRAWTURB		0x10
#define	SURF_TRANSBRUSH		0x20
#define	SURF_DRAWFOG		0x40
#define	SURF_UNDERWATER		0x80
#define	SURF_DISTORTBRUSH	0x100
#define	SURF_CAUSTICBRUSH	0x200


// если будете уменьшать, то форсировать пересчет BSP кэша!!!
#define	SUBDIVIDE_SIZE	4096	/// was 64


typedef struct
{
	int		contents;			// OR of all brushes (not needed?)

	short	cluster;
	short	area;

	short	mins[3];			// for frustum culling
	short	maxs[3];

	WORD	firstleafface;
	WORD	numleaffaces;

	WORD	firstleafbrush;
	WORD	numleafbrushes;
} dleaf_t;

typedef struct
{
	int		planenum;
	int		children[2];	// negative numbers are -(leafs+1), not nodes
	short	mins[3];		// for frustom culling
	short	maxs[3];
	WORD	firstface;
	WORD	numfaces;	// counting both sides
} dnode_t;

typedef struct
{
	float		mins[3], maxs[3];
	float		origin[3];		// for sounds or lights
	int			headnode;
	int			firstface, numfaces;	// submodels just draw faces
										// without walking the bsp tree
} dmodel_t;


#define IDSPRITEHEADER	(('2'<<24)+('S'<<16)+('D'<<8)+'I')	// little-endian "IDS2"
#define SPRITE_VERSION	2
#define IDSPRITE2HEADER	(('3'<<24)+('S'<<16)+('D'<<8)+'I')	// little-endian "IDS3"

typedef struct
{
	int		width, height;
	int		origin_x, origin_y;		// raster coordinates inside pic
	char	name[MAX_SKINNAME];		// name of pcx file
} dsprframe_t;

typedef struct
{
	int			ident;
	int			version;
	int			numframes;
	dsprframe_t	frames[1];			// variable sized
} dsprite_t;


typedef struct
{
	int		width, height;
	int		origin_x, origin_y;		// raster coordinates inside pic
	char	name[MAX_SKINNAME];		// name of skin file
	image_t	*skin;
} dspr2frame_t;

typedef struct
{
	int				numframes;
	bool			skipDiffuse;
	bool			directed;
	float			directedScale;
	float			timescale;
	dspr2frame_t	frames[1];		// variable sized
} dsprite2_t;


typedef struct screenrect_s
{
	int coords[4];
	struct screenrect_s *next;
} screenrect_t;


typedef union lightcmd_u
{
  int	asInt;
  float	asFloat;
  void	*asVoid;
} lightcmd_t;


typedef struct
{
	float		radius;			// max(radiuses)
	vec3_t		radiuses;
	vec3_t		origin;
	vec3_t		color;
	float		_cone;						// 0 - цельная сфера, иначе: тангенс половины угла конуса (для отсечения действия источника света).
	byte		style;
	byte		cl_style;				// локальный лайт-стиль (для создания мигающих лайтов, которые можно выключать) - итог равен умножению style на cl_style
	bool		visible;
	bool		isStatic;
	bool		sphere;
	screenrect_t	scizz;
	byte		vis[MAX_MAP_LEAFS/8];	//redone pvs for light, only poly's in nodes
	int			area;
	lightcmd_t	*volumeCmds;			//gl commands to draw the shadow volume
	lightcmd_t	*lightCmds;				//gl commands to draw the cap/lighted volumes
	bool		has_parallax_surf;		// true - если в lightCmds есть параллакс-surf
	int			ownerkey;
///	int			filtercube;				//texture object of this light's cubemap filter (if <0 - texnum...)
	int			filtercube_start;		//texture object of this light's cubemap filter (if <0 - texnum...)
	int			filtercube_end;			//texture object of this light's cubemap filter (if <0 - texnum...)
	float		framerate;
	vec3_t		angles;					//angles of the cubemap filter
	vec3_t		rspeed;					//rotation speed of cube map;
	vec3_t		mins;
	vec3_t		maxs;
	vec3_t		mins_cone;
	vec3_t		maxs_cone;
	cplane_t	frust[4];
	vec3_t		styled_color;
	char		label[MAX_QPATH];
	char		targetname[MAX_QPATH];
	bool		has_lightedsurfs;		// for static world only!
///	bool		has_shadowvolume;		// for static world only!
	bool		noshadow;				// no world shadows
	bool		noshadow2;				// no entity shadows
	bool		nobump;					// no bump, no specular
	bool		start_off;
	vertCache_t	*sh_vbo_id;				// идентификатор shadow volume vbo
	unsigned	sh_vbo_xyz;
///	bool		use;					// признак использования объекта (так можно запрещать использование в игре - применяется в задаче релайта)
	bool		flareLight;				// признак того, что родитель = Flare
	int			framecount;
	GLuint		occID;
	bool		directVisibled;			// "прямая видимость", не делаем occlusion-test
} shadowlight_t;


typedef struct
{
	// special messages
	void	(*bprintf) (int printlevel, char *fmt, ...);
	void	(*dprintf) (char *fmt, ...);
	void	(*cprintf) (edict_t *ent, int printlevel, char *fmt, ...);
	void	(*centerprintf) (edict_t *ent, char *fmt, ...);
	void	(*sound) (edict_t *ent, int channel, int soundindex, float volume, float attenuation, float timeofs);
	void	(*positioned_sound) (vec3_t origin, edict_t *ent, int channel, int soundinedex, float volume, float attenuation, float timeofs);

	// config strings hold all the index strings, the lightstyles,
	// and misc data like the sky definition and cdtrack.
	// All of the current configstrings are sent to clients when
	// they connect, and changes are sent to all connected clients.
	void	(*configstring) (int num, char *string);

	void	(*error) (char *fmt, ...);

	/// Berserker: lightstyles override
	char	*(*lightstyle) (int num);

	// the *index functions create configstrings and some internal server state
	int		(*modelindex) (char *name);
	int		(*soundindex) (char *name);
	int		(*imageindex) (char *name);

	void	(*setmodel) (edict_t *ent, char *name);

	// collision detection
	trace_t	(*trace) (vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, edict_t *passent, int contentmask);
	int		(*pointcontents) (vec3_t point);
	bool	(*inPVS) (vec3_t p1, vec3_t p2);
	bool	(*inPHS) (vec3_t p1, vec3_t p2);
	void	(*SetAreaPortalState) (int portalnum, bool open);
	bool	(*AreasConnected) (int area1, int area2);

	// an entity will never be sent to a client or used for collision
	// if it is not passed to linkentity.  If the size, position, or
	// solidity changes, it must be relinked.
	void	(*linkentity) (edict_t *ent);
	void	(*unlinkentity) (edict_t *ent);		// call before removing an interactive edict
	int		(*BoxEdicts) (vec3_t mins, vec3_t maxs, edict_t **list,	int maxcount, int areatype);
	void	(*Pmove) (pmove_t *pmove);		// player movement code common with client prediction

	// network messaging
	void	(*multicast) (vec3_t origin, multicast_t to);
	void	(*unicast) (edict_t *ent, bool reliable);
	void	(*WriteChar) (int c);
	void	(*WriteByte) (int c);
	void	(*WriteShort) (int c);
	void	(*WriteLong) (int c);
	void	(*WriteFloat) (float f);
	void	(*WriteString) (char *s);
	void	(*WritePosition) (vec3_t pos, bool large_map);	// some fractional bits
	void	(*WriteDir) (vec3_t pos);		// single byte encoded, very coarse
	void	(*WriteAngle) (float f);

	// managed memory allocation
	void	*(*TagMalloc) (int size, int tag);
	void	(*TagFree) (void *block);
	void	(*FreeTags) (int tag);

	// console variable interaction
	cvar_t	*(*cvar) (char *var_name, char *value, int flags);
	cvar_t	*(*cvar_set) (char *var_name, char *value);
	cvar_t	*(*cvar_forceset) (char *var_name, char *value);

	// ClientCommand and ServerCommand parameter access
	int		(*argc) (void);
	char	*(*argv) (int n);
	char	*(*args) (void);	// concatenation of all argv >= 1

	// add commands to the server console as if they were typed in
	// for map changing, etc
	void	(*AddCommandString) (char *text);

///	void	(*DebugGraph) (float value, int color);
} game_import_t;

// player_state_t->refdef flags		(Передаётся по сети байт! Значения не больше 128 !!!)
#define	RDF_UNDERWATER		1		// warp the screen as apropriate
#define RDF_NOWORLDMODEL	2		// used for player configuration screen
//ROGUE
#define	RDF_IRGOGGLES		4
///#define RDF_UVGOGGLES		8	/// berserker: not used???
//Berserker
#define RDF_COMBATTRACK		8		/// if set ON, client will play battle sound track
#define	RDF_MASK			16
#define	RDF_BURN			32
#define	RDF_DROWN			64
/// Berserker: если не хватает байта для эффектов, можно заюзать канал rfx_ops !!! Пример: pain эффекты.
#define	RDF_BREATHER		0x40000000		/// berserker: game dll side bite
#define	RDF_ENVIRO			0x80000000		/// berserker: game dll side bite
#define	RDF_NOCLEAR			0x80000000		/// berserker: client/render side bit


#define	MAX_CLIP_VERTS	64
#define	ON_EPSILON		0.1			// point on plane side epsilon

#define	SIDE_FRONT	0
#define	SIDE_BACK	1
#define	SIDE_ON		2

/* MD4 context. */
typedef unsigned int UINT4;
typedef struct
{
	UINT4	state[4];				/* state (ABCD) */
	UINT4	count[2];				/* number of bits, modulo 2^64 (lsb first) */
	byte	buffer[64]; 			/* input buffer */
} MD4_CTX;

/* POINTER defines a generic pointer type */
typedef byte *POINTER;

/* Constants for MD4Transform routine.  */
#define S11 3
#define S12 7
#define S13 11
#define S14 19
#define S21 3
#define S22 5
#define S23 9
#define S24 13
#define S31 3
#define S32 9
#define S33 11
#define S34 15

/* F, G and H are basic MD4 functions. */
#define F(x, y, z) (((x) & (y)) | ((~x) & (z)))
#define G(x, y, z) (((x) & (y)) | ((x) & (z)) | ((y) & (z)))
#define H(x, y, z) ((x) ^ (y) ^ (z))

/* ROTATE_LEFT rotates x left n bits. */
#define ROTATE_LEFT(x, n) (((x) << (n)) | ((x) >> (32-(n))))

/* FF, GG and HH are transformations for rounds 1, 2 and 3 */
/* Rotation is separate from addition to prevent recomputation */
#define FF(a, b, c, d, x, s) {(a) += F ((b), (c), (d)) + (x); (a) = ROTATE_LEFT ((a), (s));}
#define GG(a, b, c, d, x, s) {(a) += G ((b), (c), (d)) + (x) + (UINT4)0x5a827999; (a) = ROTATE_LEFT ((a), (s));}
#define HH(a, b, c, d, x, s) {(a) += H ((b), (c), (d)) + (x) + (UINT4)0x6ed9eba1; (a) = ROTATE_LEFT ((a), (s));}


#ifndef GL_VERSION_1_3
#define GL_TEXTURE0		GL_TEXTURE0_ARB
#define GL_TEXTURE1		GL_TEXTURE1_ARB
#define GL_TEXTURE2		GL_TEXTURE2_ARB
#define GL_TEXTURE3		GL_TEXTURE3_ARB
#define GL_TEXTURE4		GL_TEXTURE4_ARB
#define GL_TEXTURE5		GL_TEXTURE5_ARB
#define GL_TEXTURE6		GL_TEXTURE6_ARB
#define GL_TEXTURE7		GL_TEXTURE7_ARB
#define GL_TEXTURE8		GL_TEXTURE8_ARB
#define GL_TEXTURE9		GL_TEXTURE9_ARB
#define GL_TEXTURE10	GL_TEXTURE10_ARB
#define GL_TEXTURE11	GL_TEXTURE11_ARB
#define GL_TEXTURE12	GL_TEXTURE12_ARB
#define GL_TEXTURE13	GL_TEXTURE13_ARB
#define GL_TEXTURE14	GL_TEXTURE14_ARB
#define GL_TEXTURE15	GL_TEXTURE15_ARB
#define GL_TEXTURE16	GL_TEXTURE16_ARB
#define GL_TEXTURE17	GL_TEXTURE17_ARB
#define GL_TEXTURE18	GL_TEXTURE18_ARB
#define GL_TEXTURE19	GL_TEXTURE19_ARB
#define GL_TEXTURE20	GL_TEXTURE20_ARB
#define GL_TEXTURE21	GL_TEXTURE21_ARB
#define GL_TEXTURE22	GL_TEXTURE22_ARB
#define GL_TEXTURE23	GL_TEXTURE23_ARB
#define GL_TEXTURE24	GL_TEXTURE24_ARB
#define GL_TEXTURE25	GL_TEXTURE25_ARB
#define GL_TEXTURE26	GL_TEXTURE26_ARB
#define GL_TEXTURE27	GL_TEXTURE27_ARB
#define GL_TEXTURE28	GL_TEXTURE28_ARB
#define GL_TEXTURE29	GL_TEXTURE29_ARB
#define GL_TEXTURE30	GL_TEXTURE30_ARB
#define GL_TEXTURE31	GL_TEXTURE31_ARB
#endif


#define	MAXSHADOWLIGHTS		1024	//256 //Maximum number of (client side) lights in a map				/// FIXME: уменьшить для спец.карт для данного движка!
#define MAXUSEDSHADOWLIGHS	128		//64  //Maximum number of lights that can be used in a single frame	/// FIXME: уменьшить для спец.карт для данного движка!

#define MAX_VOLUME_COMMANDS	524288	// 131072	//Thats 2.0 (0.5) meg storage for commands, insane
#define MAX_LIGHT_COMMANDS	65536	// 0.25 meg for light commands
#define MAX_VBO_XYZs		524288	// 65536

typedef struct svnode_s
{
	dplane_t		*splitplane;
	struct svnode_s	*children[2];
} svnode_t;

///#define MAX_PLANE_POOL 10240
///#define MAX_NODE_POOL 10240

#define MAX_POLY_VERT		128

//temporaly storage for polygons that use an edge
typedef struct
{
	byte		used;		//how many polygons use this edge
	glpoly_t	*poly[2];	//pointer to the polygons who use this edge
} temp_connect_t;


#define	MAX_GLOBAL_FILTERS	128
#define	MAX_FILTERS		256	///99
					// 1 - FlashLight (reserved for flashlight!!!)
					// 2 - Disco
					// 3 - Fan
					// 4 - Fenced Light
					// 5 - Light Spot (Round)
					// 6 - Window spot
					// 7 - Lamp
					// etc...


#define SAFE_GET_PROC( func, type, name)     \
   func = (type) SDL_GL_GetProcAddress( name)

typedef struct
{
	bool	restart_sound;
	int		s_rate;
	int		s_width;
	int		s_channels;

	int		width;
	int		height;
	byte	*pic;
	byte	*pic_pending;

	// order 1 huffman stuff
	int		*hnodes1;	// [256][256][2];
	int		numhnodes1[256];

	int		h_used[512];
	int		h_count[512];
} cinematics_t;

typedef struct
{
	byte	*data;
	int		count;
} cblock_t;

typedef struct img_s
{
	byte	*pixels;
	int		width;
	int		height;
} img_t;



#define		ENT_UNKNOWN			0
#define		ENT_LIGHT			1
#define		ENT_SMALL_SUN		2
#define		ENT_SUN				3
#define		ENT_BIG_SUN			4
#define		ENT_EMIT			5
#define		ENT_DECAL			6
#define		ENT_FOG				7
#define		ENT_ALIASMODEL		8
#define		ENT_BRUSHMODEL		9
#define		ENT_LIGHTSTYLE		10
#define		ENT_OPAQUE			11
#define		ENT_IGNORED_FLARE	12
#define		ENT_CL_LIGHTSTYLE	13
#define		ENT_FOG_HACK		14
#define		ENT_NODRAW			15


#define	MAX_EMITS	256

typedef struct
{
	int			index;
	vec3_t		origin;		// (emit."origin")
	vec3_t		dir;		// вычисляется из emit."angles" (всего используется 2 первых угла)
	vec3_t		rspeed;		// (emit."speed"), а последний параметр X:
	byte		style;		// Использует систему lightstyles: определяет интенсивность выброса партиклей (emit."style")
	byte		cl_style;				// локальный лайт-стиль (для создания мигающих эмиттеров, которые можно выключать) - итог равен умножению style на cl_style
	parttype_t	emit;		// (emit."filter")
	float		cone;
	float		vel;		// скорость движения из emit."angles[2]" - последний угол
	vec3_t		origin0;	// (emit."origin") - Н.У.
	vec3_t		dir0;		// вычисляется из emit."angles" (всего используется 2 первых угла) - Н.У.
	char		label[MAX_QPATH];
	int			framecount;
	int			area;
	float		angles[2];		// для хранения углов, чтоб потом записать в релайт.
	vec3_t		startcolor;		// если startcolor[0]!=0, то color override!
	vec3_t		endcolor;
	float		alphavel;
	char		gravity;
} emit_t;


// edict->spawnflags
// these are set with checkboxes on each entity in the map editor
#define	SPAWNFLAG_NOT_EASY			0x00000100
#define	SPAWNFLAG_NOT_MEDIUM		0x00000200
#define	SPAWNFLAG_NOT_HARD			0x00000400
#define	SPAWNFLAG_NOT_DEATHMATCH	0x00000800
#define	SPAWNFLAG_NOT_COOP			0x00001000

#define MAX_FOV		135

#define SKY_MIN		1.0/512
#define SKY_MAX		511.0/512

///#define	R_TEXTURE_MAX_LOD	-0.7


/*
========================================================================
.MD3 model file format
========================================================================
*/

#define IDMD3HEADER		(('3'<<24)+('P'<<16)+('D'<<8)+'I')


#ifndef M_TWOPI
#define M_TWOPI		6.28318530717958647692
#endif

// vertex scales
#define	MD3_XYZ_SCALE		(1.0/64)

typedef struct
{
	float			st[2];
} dmd3coord_t;

typedef struct
{
	short			point[3];
	short			norm;
} dmd3vertex_t;

typedef struct
{
	vec3_t			point;
	vec3_t			normal;
} admd3vertex_t;

typedef struct
{
    vec3_t			mins;
	vec3_t			maxs;
    vec3_t			translate;
    float			radius;
    char			creator[16];
} dmd3frame_t;

typedef struct
{
	vec3_t			origin;
	float			axis[3][3];
} dorientation_t;

typedef struct
{
	char			name[MD3_MAX_PATH];		// tag name
	dorientation_t	orient;
} dmd3tag_t;

typedef struct
{
	char			name[MD3_MAX_PATH];
	int				unused;					// shader
} dmd3skin_t;

typedef struct
{
    char			id[4];

    char			name[MD3_MAX_PATH];

	int				flags;

    int				num_frames;
    int				num_skins;
    int				num_verts;
    int				num_tris;

    int				ofs_tris;
    int				ofs_skins;
    int				ofs_tcs;
    int				ofs_verts;

    int				meshsize;
} dmd3mesh_t;

typedef struct
{
    int				id;
    int				version;

    char			filename[MD3_MAX_PATH];

	int				flags;

    int				num_frames;
    int				num_tags;
    int				num_meshes;
    int				num_skins;

    int				ofs_frames;
    int				ofs_tags;
    int				ofs_meshes;
    int				ofs_end;
} dmd3_t;


////////// MD3 Support ///////////////////////

typedef struct maliascoord_s
{
	float		st[2];
} maliascoord_t;

typedef struct maliasvertex_s
{
	vec3_t			point;
	byte			normal;
	byte			tangent;
	byte			binormal;
} maliasvertex_t;

typedef struct
{
    vec3_t			mins, maxs;
    vec3_t			translate;
    float			radius;
} maliasframe_t;

///typedef struct
///{
///	char			name[MD3_MAX_PATH];
///	dorientation_t	orient;
///} maliastag_t;

///typedef struct
///{
///	char			name[MD3_MAX_PATH];
///	int				shader;
///} maliasskin_t;

typedef struct
{
	int				num_verts;
	char			name[MD3_MAX_PATH];
	maliasvertex_t	*vertexes;
	maliascoord_t	*stcoords;

	int				num_tris;
	WORD			*indexes;
	neighbours_t	*triangles;

	int				num_skins;
///	maliasskin_t	*skins;
	image_t			*img_skins[MD3_MAX_SKINS];
	image_t			*img_bumps[MD3_MAX_SKINS];
	image_t			*img_lights[MD3_MAX_SKINS];
/*
	bool			geo_changed;
	int				framecount;
	int				old_frame;
	float			old_backlerp;
	vec3_t			old_angles;
	char			old_nobump;
	unsigned		vbo_st;
	unsigned		vbo_xyz;
	unsigned		vbo_lightp;
	unsigned		vbo_tsh;
	unsigned		vbo_fx;
	unsigned		vbo_normals;
	unsigned		vbo_tangents;
	unsigned		vbo_binormals;
*/
	vertCache_t	*vbo_st;
} maliasmesh_t;

typedef struct maliasmodel_s
{
	int				num_frames;
	maliasframe_t	*frames;

///	int				num_tags;
///	maliastag_t		*tags;

	int				num_meshes;
	maliasmesh_t	*meshes;

///	int				num_skins;
///	maliasskin_t	*skins;
} maliasmodel_t;

///////////////////////////////////////////////////////////

typedef struct alink_s
{
	int			index;
	vec3_t		origin;
	vec3_t		angles;
	char		model[MAX_QPATH];
	char		label[MAX_QPATH];
	char		sound[MAX_QPATH];		// для редактора (сохраняем имя звука для cl_savelights_f)
	sfx_t		*noise;
	model_t		*mdl;
	int			areas[9];
	byte		vis[MAX_MAP_LEAFS/8];	//redone pvs for model, only poly's in nodes
	bool		nouse;
	vec3_t		real_origin;			// for linked model origin caching
	bool		animframe;
	int			frame;
	int			frame_end;
	unsigned	skinnum;				// for md2 only!
} alink_t;

typedef struct blink_s
{
	int			index;
	vec3_t		origin;
	char		label[MAX_QPATH];
	mmodel_t	*mdl;
	bool		nodraw;
} blink_t;

typedef struct llink_s
{
	int		index;
	float	radius;
	vec3_t	origin;
	vec3_t	color;
	float	_cone;					// 0 - цельная сфера, иначе: тангенс половины угла конуса (для отсечения действия источника света).
	byte	style;
	byte	cl_style;				// локальный лайт-стиль (для создания мигающих лайтов, которые можно выключать) - итог равен умножению style на cl_style
///	int		filtercube;				//texture object of this light's cubemap filter (if <0 - texnum...)
	int		filtercube_start;		//texture object of this light's cubemap filter (if <0 - texnum...)
	int		filtercube_end;			//texture object of this light's cubemap filter (if <0 - texnum...)
	float	framerate;
	vec3_t	angles;					//angles of the cubemap filter
	char	label[MAX_QPATH];
///	bool	use;					// признак использования объекта (так можно запрещать использование в игре - применяется в задаче релайта)
/// FIXME: пока мы НЕ поддерживаем нижеуказанные возможности для линкованного света (модифицировать V_AddLight!)
///	vec3_t	rspeed;					//rotation speed of cube map;
///	char	targetname[MAX_QPATH];
///	bool	start_off;
} llink_t;


#define MAX_PACK_FILES		1024
#define MAX_HUDMODEL_FILES	256


#define Vec_RangeCap(x,minv,maxv) \
do { \
	if ((x)[0] > (maxv)) (x)[0] = (maxv); else if ((x)[0] < (minv)) x[0] = (minv); \
	if ((x)[1] > (maxv)) (x)[1] = (maxv); else if ((x)[1] < (minv)) x[1] = (minv); \
	if ((x)[2] > (maxv)) (x)[2] = (maxv); else if ((x)[2] < (minv)) x[2] = (minv); \
} while(0)

/*
typedef enum
{
	PRE_READ,									// prefetch assuming that buffer is used for reading only
	PRE_WRITE,									// prefetch assuming that buffer is used for writing only
	PRE_READ_WRITE								// prefetch assuming that buffer is used for both reading and writing
} e_prefetch;
*/


#define	PARTICLE_BOUNCE		1
#define	PARTICLE_FRICTION	2
#define	PARTICLE_ROTATE		4
#define	PARTICLE_STOPED		8
#define	PARTICLE_STRETCH	16
#define	PARTICLE_BURNING	32


typedef struct
{
    byte *Buffer;
    size_t Pos;
} TPngFileBuffer;


#define	LSTAGE_STAGES	8

const char *lstage_names[] =
{
	"Map",
	"Pictures",
	"Models",
	"Images",
	"Clients",
	"Sky",
	"Lights",
	"Fogs"
};


///// Berserker: Заменил 2048 на 4096, т.к. есть видеорежим 2560x1600 и он валил программу.
///// Thanx to -BFG10K-, who discovered this bug!
/// Размер 4096 (2048) выбран потому, что максимальный видеорежим = 4096 (2048) точек.
/// Следовательно screenTexture может иметь размер до 4096 (2048) точек.
#define		MAX_TEXTURE_SIZE	4096	/// 2048


#define		MAX_FLARES			1024

typedef struct flare_s
{
	msurface_t	*surf;
	float		size;
} flare_t;

#define	r_predator_intensity	0.9


#define	MAX_MIRRORS	8
#define	MIRROR_SIZE	128

typedef struct mirror_s
{
	image_t		*texture;				// Object we should render to and use for drawing
	int			lockframe, updateframe;	// Index of frame when it was last updated
	cplane_t	plane;					// Plane equatation of mirror face
	msurface_t	*chain;					// Chain of mirror poly's
	float		alpha;					// Интенсивность зеркала в зависимости от расстояния
	bool		planeback;				// Плоскость-зеркало SURF_PLANEBACK
	int			occID;
	bool		occResult;
} mirror_t;

typedef struct fs_cache_s
{
	char		name[MAX_QPATH];
	int			len;				// -1 - not exist
	int			pak;				// -1 - not from pak.
	unsigned	hash;
} fs_cache_t;

#define		MAX_PAKS	100
#define		MAX_FILES	131072	//65536

#define		MAX_SPEC_TEXTURES	11

typedef int	cmp_t (const void *, const void *);


/* dds definition */
typedef enum
{
	DDS_PF_ARGB8888,
	DDS_PF_DXT1,
	DDS_PF_DXT2,
	DDS_PF_DXT3,
	DDS_PF_DXT4,
	DDS_PF_DXT5,
	DDS_PF_UNKNOWN
}
ddsPF_t;


/* 16bpp stuff */
#define DDS_LOW_5		0x001F;
#define DDS_MID_6		0x07E0;
#define DDS_HIGH_5		0xF800;
#define DDS_MID_555		0x03E0;
#define DDS_HI_555		0x7C00;


/* structures */
typedef struct ddsColorKey_s
{
	unsigned int    colorSpaceLowValue;
	unsigned int    colorSpaceHighValue;
}
ddsColorKey_t;


typedef struct ddsCaps_s
{
	unsigned int    caps1;
	unsigned int    caps2;
	unsigned int    caps3;
	unsigned int    caps4;
}
ddsCaps_t;


typedef struct ddsMultiSampleCaps_s
{
	unsigned short  flipMSTypes;
	unsigned short  bltMSTypes;
}
ddsMultiSampleCaps_t;


typedef struct ddsPixelFormat_s
{
	unsigned int    size;
	unsigned int    flags;
	unsigned int    fourCC;
	union
	{
		unsigned int    rgbBitCount;
		unsigned int    yuvBitCount;
		unsigned int    zBufferBitDepth;
		unsigned int    alphaBitDepth;
		unsigned int    luminanceBitCount;
		unsigned int    bumpBitCount;
		unsigned int    privateFormatBitCount;
	};
	union
	{
		unsigned int    rBitMask;
		unsigned int    yBitMask;
		unsigned int    stencilBitDepth;
		unsigned int    luminanceBitMask;
		unsigned int    bumpDuBitMask;
		unsigned int    operations;
	};
	union
	{
		unsigned int    gBitMask;
		unsigned int    uBitMask;
		unsigned int    zBitMask;
		unsigned int    bumpDvBitMask;
		ddsMultiSampleCaps_t multiSampleCaps;
	};
	union
	{
		unsigned int    bBitMask;
		unsigned int    vBitMask;
		unsigned int    stencilBitMask;
		unsigned int    bumpLuminanceBitMask;
	};
	union
	{
		unsigned int    rgbAlphaBitMask;
		unsigned int    yuvAlphaBitMask;
		unsigned int    luminanceAlphaBitMask;
		unsigned int    rgbZBitMask;
		unsigned int    yuvZBitMask;
	};
}
ddsPixelFormat_t;


typedef struct ddsBuffer_s
{
	/* magic: 'dds ' */
	char            magic[4];

	/* directdraw surface */
	unsigned int    size;
	unsigned int    flags;
	unsigned int    height;
	unsigned int    width;
	union
	{
		int             pitch;
		unsigned int    linearSize;
	};
	unsigned int    backBufferCount;
	union
	{
		unsigned int    mipMapCount;
		unsigned int    refreshRate;
		unsigned int    srcVBHandle;
	};
	unsigned int    alphaBitDepth;
	unsigned int    reserved;
	void           *surface;
	union
	{
		ddsColorKey_t   ckDestOverlay;
		unsigned int    emptyFaceColor;
	};
	ddsColorKey_t   ckDestBlt;
	ddsColorKey_t   ckSrcOverlay;
	ddsColorKey_t   ckSrcBlt;
	union
	{
		ddsPixelFormat_t pixelFormat;
		unsigned int    fvf;
	};
	ddsCaps_t       ddsCaps;
	unsigned int    textureStage;

	/* data (Varying size) */
	unsigned char   data[4];
}
ddsBuffer_t;


typedef struct ddsColorBlock_s
{
	unsigned short  colors[2];
	unsigned char   row[4];
}
ddsColorBlock_t;


typedef struct ddsAlphaBlockExplicit_s
{
	unsigned short  row[4];
}
ddsAlphaBlockExplicit_t;


typedef struct ddsAlphaBlock3BitLinear_s
{
	unsigned char   alpha0;
	unsigned char   alpha1;
	unsigned char   stuff[6];
}
ddsAlphaBlock3BitLinear_t;


typedef struct ddsColor_s
{
	unsigned char   r, g, b, a;
}
ddsColor_t;


typedef struct
{
	WORD		type;
	WORD		occResult;
	int			framecount;
	unsigned	hash_model_name;		// MD2/3: Com_HashKey(mod->name);  BRUSH: firstmodelsurfase
	unsigned	hash_model_origin;		// Com_PosHash(mod->origin)
}
occResult_t;

#if 1
// defines from KMQuake2
#define SCREEN_WIDTH	640.0f
#define SCREEN_HEIGHT	480.0f
#define STANDARD_ASPECT_RATIO	((float)SCREEN_WIDTH/(float)SCREEN_HEIGHT)
#endif

// joystick defines and variables
// where should defines be moved?
#define JOY_ABSOLUTE_AXIS	0x00000000		// control like a joystick
#define JOY_RELATIVE_AXIS	0x00000010		// control like a mouse, spinner, trackball
#define	JOY_MAX_AXES		6				// X, Y, Z, R, U, V
#define JOY_AXIS_X			0
#define JOY_AXIS_Y			1
#define JOY_AXIS_Z			2
#define JOY_AXIS_R			3
#define JOY_AXIS_U			4
#define JOY_AXIS_V			5

#define random()	((rand () & 0x7fff) / ((float)0x7fff))
#define crandom()	(2.0 * (random() - 0.5))

// memory tags to allow dynamic memory to be cleaned up
#define	TAG_GAME	765		// clear when unloading the dll
#define	TAG_LEVEL	766		// clear when loading a new level



#define ED_LIGHT	1
#define ED_EMIT		2
#define ED_DECAL	4
#define ED_FOG		8
#define ED_MODEL	16
#define ED_BRUSH	32
#define ED_AMBIENT	64
#define ED_MODIFIED	128
