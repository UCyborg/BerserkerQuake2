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


struct gclient_s
{
	player_state_t	ps;		// communicated by server to clients
	int				ping;
	// the game dll can add anything it wants after
	// this point in the structure
};


struct edict_s
{
	entity_state_t	s;
	struct gclient_s	*client;
	bool		inuse;
	int			linkcount;

	// FIXME: move these fields to a server private sv_entity_t
	link_t		area;				// linked to a division node or leaf

	int			num_clusters;		// if -1, use headnode instead
	int			clusternums[MAX_ENT_CLUSTERS];
	int			headnode;			// unused if num_clusters != -1
	int			areanum, areanum2;

	//================================
	int			svflags;			// SVF_NOCLIENT, SVF_DEADMONSTER, SVF_MONSTER, etc
	vec3_t		mins, maxs;
	vec3_t		absmin, absmax, size;
	solid_t		solid;
	int			clipmask;
	edict_t		*owner;

	// the game dll can add anything it wants after
	// this point in the structure
};


clightstyle_t	sv_lightstyle[MAX_LIGHTSTYLES];
clightstyle_t	cl_lightstyle[MAX_LIGHTSTYLES];


void*	game_library;

int	ActiveApp;


int			r_framecount;	// used for dlight push checking
int			occ_framecount;	// like r_framecount, but count real frames, w/o mirrors

cmd_function_t	*cmd_functions;		// possible commands to execute
cvar_t			*cvar_vars;

bool	Minimized;

int			com_argc;
char		*com_argv[MAX_NUM_ARGVS+1];

int			cmd_argc;
char		*cmd_argv[MAX_STRING_TOKENS];
char		*cmd_null_string = "";
char		cmd_args[MAX_STRING_CHARS];

char	key_lines[32][MAXCMDLINE];
int		history_line=0;
int		edit_line=0;
int		key_linepos;
bool	consolekeys[256];	// if true, can't be rebound while in console
bool	menubound[256];	// if true, can't be rebound while in menu
int		keyshift[256];		// key to map to if shift held down in console
char	*keybindings[256];
int		anykeydown;
bool	keydown[256];
int		key_repeats[256];	// if > 1, it is autorepeating
int		shift_down=false;

sizebuf_t	cmd_text;
byte		cmd_text_buf[32768];	/// 8192	Fixed bug: ограничение на командный буфер в 8 КБ
char		defer_text_buf[32768];	/// 8192

keyname_t keynames[] =
{
	{"TAB", K_TAB},
	{"ENTER", K_ENTER},
	{"ESCAPE", K_ESCAPE},
	{"SPACE", K_SPACE},
	{"BACKSPACE", K_BACKSPACE},
	{"UPARROW", K_UPARROW},
	{"DOWNARROW", K_DOWNARROW},
	{"LEFTARROW", K_LEFTARROW},
	{"RIGHTARROW", K_RIGHTARROW},

	{"ALT", K_ALT},
	{"CTRL", K_CTRL},
	{"SHIFT", K_SHIFT},

	{"F1", K_F1},
	{"F2", K_F2},
	{"F3", K_F3},
	{"F4", K_F4},
	{"F5", K_F5},
	{"F6", K_F6},
	{"F7", K_F7},
	{"F8", K_F8},
	{"F9", K_F9},
	{"F10", K_F10},
	{"F11", K_F11},
	{"F12", K_F12},

	{"INS", K_INS},
	{"DEL", K_DEL},
	{"PGDN", K_PGDN},
	{"PGUP", K_PGUP},
	{"HOME", K_HOME},
	{"END", K_END},

	{"MOUSE1", K_MOUSE1},
	{"MOUSE2", K_MOUSE2},
	{"MOUSE3", K_MOUSE3},
	/// added by Willow: new mouse support
	{"MOUSE4", K_MOUSE4},
	{"MOUSE5", K_MOUSE5},
	// UCyborg: more mouse buttons!
	{"MOUSE6", K_MOUSE6},
	{"MOUSE7", K_MOUSE7},
	{"MOUSE8", K_MOUSE8},
	{"MOUSE9", K_MOUSE9},
	{"MOUSE10", K_MOUSE10},
	{"MOUSE11", K_MOUSE11},
	{"MOUSE12", K_MOUSE12},
	{"MOUSE13", K_MOUSE13},
	{"MOUSE14", K_MOUSE14},
	{"MOUSE15", K_MOUSE15},
	{"MOUSE16", K_MOUSE16},

	{"JOY1", K_JOY1},
	{"JOY2", K_JOY2},
	{"JOY3", K_JOY3},
	{"JOY4", K_JOY4},

	{"AUX1", K_AUX1},
	{"AUX2", K_AUX2},
	{"AUX3", K_AUX3},
	{"AUX4", K_AUX4},
	{"AUX5", K_AUX5},
	{"AUX6", K_AUX6},
	{"AUX7", K_AUX7},
	{"AUX8", K_AUX8},
	{"AUX9", K_AUX9},
	{"AUX10", K_AUX10},
	{"AUX11", K_AUX11},
	{"AUX12", K_AUX12},
	{"AUX13", K_AUX13},
	{"AUX14", K_AUX14},
	{"AUX15", K_AUX15},
	{"AUX16", K_AUX16},
	{"AUX17", K_AUX17},
	{"AUX18", K_AUX18},
	{"AUX19", K_AUX19},
	{"AUX20", K_AUX20},
	{"AUX21", K_AUX21},
	{"AUX22", K_AUX22},
	{"AUX23", K_AUX23},
	{"AUX24", K_AUX24},
	{"AUX25", K_AUX25},
	{"AUX26", K_AUX26},
	{"AUX27", K_AUX27},
	{"AUX28", K_AUX28},
	{"AUX29", K_AUX29},
	{"AUX30", K_AUX30},
	{"AUX31", K_AUX31},
	{"AUX32", K_AUX32},

	{"KP_HOME",			K_KP_HOME },
	{"KP_UPARROW",		K_KP_UPARROW },
	{"KP_PGUP",			K_KP_PGUP },
	{"KP_LEFTARROW",	K_KP_LEFTARROW },
	{"KP_5",			K_KP_5 },
	{"KP_RIGHTARROW",	K_KP_RIGHTARROW },
	{"KP_END",			K_KP_END },
	{"KP_DOWNARROW",	K_KP_DOWNARROW },
	{"KP_PGDN",			K_KP_PGDN },
	{"KP_ENTER",		K_KP_ENTER },
	{"KP_INS",			K_KP_INS },
	{"KP_DEL",			K_KP_DEL },
	{"KP_SLASH",		K_KP_SLASH },
	{"KP_MINUS",		K_KP_MINUS },
	{"KP_PLUS",			K_KP_PLUS },

	{"MWHEELUP", K_MWHEELUP },
	{"MWHEELDOWN", K_MWHEELDOWN },

	{"PAUSE", K_PAUSE},

	{"SEMICOLON", ';'},	// because a raw semicolon separates commands

	{NULL,0}
};

client_static_t	cls;

int		alias_count;		// for detecting runaway loops

int		com_token_len;
char	com_token[MAX_TOKEN_CHARS];

cmdalias_t	*cmd_alias;

int		server_state;

bool	userinfo_modified;

searchpath_t	*fs_searchpaths;
searchpath_t	*fs_base_searchpaths;	// without gamedirs
char	fs_gamedir[MAX_OSPATH];

int		rd_target;
char	*rd_buffer;
int		rd_buffersize;
void	(*rd_flush)(int target, char *buffer);

console_t	con;

bool	vid_restart;
bool	mlooking;

gllightmapstate_t	gl_lms;
float s_blocklights[128*128*4]; //Knightmare-  was [34*34*3], supports max chop size of 2048?

vec3_t	amb;
vec3_t	sky_origin;
vec3_t	sky_viewcenter;
float	sky_angle;
float	sky_rotate;
float	sky_scale;
bool	sky_world;
int		sky_area;
bool	drawing_sky_world;


cvar_t	*cv_reset;
cvar_t	*s_music;
cvar_t	*s_musicVolume;
cvar_t	*sys_username;
cvar_t	*scr_drawclock;
cvar_t	*scr_draw2d;
cvar_t	*r_screenshot_png_quality;
cvar_t	*r_screenshot_jpg_quality;
///cvar_t	*calc_lni;
cvar_t	*rfx_mask;
cvar_t	*rfx_mask_mp;
cvar_t	*rfx_burning;
cvar_t	*rfx_drowning;
cvar_t	*rfx_pain;
cvar_t	*rfx_underwater;
cvar_t	*r_nolights;
cvar_t	*r_fpsThreshold;
cvar_t	*r_decals;
cvar_t	*r_faststencil;
cvar_t	*r_vbo;
cvar_t	*r_vp;
cvar_t	*r_occlusion;
cvar_t	*r_occlusion_scale;
cvar_t	*r_normcube_size;
cvar_t	*r_diffuse_modifier;
cvar_t	*r_specular_modifier;
cvar_t	*r_diffuse_compression;
cvar_t	*r_bump_compression;
cvar_t	*r_anisotropy;
///cvar_t	*r_texture_lod;
cvar_t	*r_speeds;
cvar_t	*scr_drawspeed;
cvar_t	*scr_viewpos;
cvar_t	*r_showtris;
cvar_t	*r_showtexture;
unsigned	hash_showtexture, null_hashkey;
cvar_t	*r_drawsun;
cvar_t	*r_flares;
cvar_t	*r_flares_segs;
cvar_t	*r_flares_intensity;
///cvar_t	*r_DepthBoundsTest;
cvar_t	*r_mirrors;
cvar_t	*r_mirror_models;
cvar_t	*r_mirror_frameskip;
cvar_t	*r_mirror_distance;
cvar_t	*r_mirror_blend;
///cvar_t	*r_mirror_trans;
cvar_t	*r_mirror_size;
///cvar_t	*r_mirror_clip;
cvar_t	*r_caustics;
cvar_t	*r_bumpscale;
cvar_t	*r_specularscale;
cvar_t	*r_autobump;
cvar_t	*r_attenpower;
cvar_t	*r_sprite_expl;
cvar_t	*developer;
cvar_t	*cl_timedemo;
cvar_t	*fs_cddir;
cvar_t	*fs_basedir;
cvar_t	*fs_pure;
cvar_t	*r_flashblend;
cvar_t	*fs_gamedirvar;
cvar_t	*r_noshells;
cvar_t	*r_mode;
cvar_t	*r_bloodlifetime;
cvar_t	*r_bulletMarkLifeTime;
cvar_t	*r_laserMarkLifeTime;
cvar_t	*r_railMarkLifeTime;
cvar_t	*r_explosionMarkLifeTime;
cvar_t	*r_bfgMarkLifeTime;

cvar_t	*r_shader;
cvar_t	*r_drawbuffer;
cvar_t	*r_texturemode;
cvar_t	*r_texturealphamode;
cvar_t	*r_texturesolidmode;
cvar_t	*r_swapinterval;
cvar_t	*r_texfx;
cvar_t	*r_itemfx;
///cvar_t	*r_finish;
cvar_t	*scr_fps;
cvar_t	*scr_fps_min;
cvar_t	*scr_fps_max;
cvar_t	*scr_gametime;
cvar_t	*scr_texture;
cvar_t	*scr_viewsize;
cvar_t	*scr_conspeed;
cvar_t	*r_round_down;
cvar_t	*r_picmip;
cvar_t	*r_picmip_bump;
cvar_t	*cl_paused;
cvar_t	*scr_showpause;
cvar_t	*scr_centertime;
cvar_t	*scr_mapshots;
cvar_t	*con_notifytime;
cvar_t	*con_transparency;
cvar_t	*qport;
cvar_t	*noudp;
cvar_t	*maxclients;
cvar_t	*timeout;
cvar_t	*zombietime;
cvar_t	*sv_allownodelta;
cvar_t	*sv_locked;
cvar_t	*sv_showclamp;
cvar_t	*sv_stopclock;
cvar_t	*sv_autoSavePeriod;
cvar_t	*sv_autoSaveTimer;
cvar_t	*sv_paused;
cvar_t	*sv_timedemo;
cvar_t	*sv_enforcetime;
cvar_t	*dedicated;
cvar_t	*net_compatibility;
cvar_t	*hunk_model;
cvar_t	*hunk_sprite;
cvar_t	*hunk_map;
cvar_t	*sv_flyqbe;
cvar_t	*net_zsize;
cvar_t	*net_fixoverflow;
cvar_t	*logfile_active;	// 1 = buffer log, 2 = flush after each print
cvar_t	*host_speeds;
cvar_t	*net_traffic;
cvar_t	*fs_cached;
cvar_t	*timescale;
cvar_t	*fixedtime;
cvar_t	*rcon_password;		// password for remote server commands
cvar_t	*hostname;
cvar_t	*vid_xpos;
cvar_t	*vid_ypos;
cvar_t	*r_fullscreen;
cvar_t	*vid_hz;
cvar_t	*r_multiSamples;
cvar_t	*r_nvMultisampleFilterHint;
cvar_t	*vid_ignorehwgamma;
cvar_t	*vid_gamma;
cvar_t	*vid_bright;
cvar_t	*vid_contrast;
cvar_t	*r_gamma;
cvar_t	*r_intensity;
cvar_t	*r_modulate;
cvar_t	*r_overbright;
cvar_t	*hud_overbright;
///cvar_t	*r_cull;
cvar_t	*crosshair;
cvar_t	*scr_crosshair_scale;
cvar_t	*cl_nodelta;
cvar_t	*adr[NUM_ADDRESSBOOK_ENTRIES];

cvar_t	*dl_shell;
cvar_t	*dl_rocket;
cvar_t	*dl_blaster;
cvar_t	*dl_hyperblaster;
cvar_t	*dl_bfg;
cvar_t	*dl_explosion;
cvar_t	*dl_muzzle;

cvar_t	*cl_add_blend;
cvar_t	*cl_add_lights;
cvar_t	*cl_add_particles;
cvar_t	*cl_add_entities;
cvar_t	*cl_gun;
cvar_t	*cl_handicap;
cvar_t	*cl_footsteps;
cvar_t	*cl_noskins;
///cvar_t	*cl_autoskins;
cvar_t	*cl_predict;
cvar_t	*cl_maxfps;
cvar_t	*con_maxfps;
cvar_t	*cl_async;
cvar_t	*net_maxfps;
cvar_t	*r_maxfps;
cvar_t	*cl_sleep;

cvar_t	*cl_forcemymodel;
bool	cl_forcemymodel_modified;
char	mymodel[MAX_OSPATH];

cvar_t	*cl_upspeed;
cvar_t	*cl_forwardspeed;
cvar_t	*cl_sidespeed;
cvar_t	*cl_yawspeed;
cvar_t	*cl_pitchspeed;
cvar_t	*cl_anglespeedkey;

cvar_t	*cl_run;
cvar_t	*freelook;
cvar_t	*lookspring;
cvar_t	*lookstrafe;

cvar_t  *sensitivity;
cvar_t	*m_pitch;
cvar_t	*m_inversion;
cvar_t	*in_joystick;

cvar_t	*cl_showmiss;
cvar_t	*cl_showclamp;
cvar_t	*cl_timeout;

cvar_t	*rcon_client_password;
cvar_t	*rcon_address;

cvar_t	*info_password;
cvar_t	*info_spectator;
cvar_t	*name;
cvar_t	*skin;
cvar_t	*railtrailcore;
cvar_t	*railtrailspiral;
cvar_t	*railtrailradius;
cvar_t	*rate;
cvar_t	*msg;
cvar_t	*hand;
cvar_t	*fov;
cvar_t	*zoomfov;
cvar_t	*zoomspeed;
cvar_t	*gender;					/// тут пол игрока, в норм. условиях д.б. male, female или none
char	gender_model[MAX_QPATH];		/// тут храним модель игрока
model_t	*gender_mdl;
image_t	*gender_skin;
image_t	*gender_bump;
image_t	*gender_light;
///cvar_t	*gender_auto;
cvar_t	*cl_vwep;
cvar_t	*cl_fovweap;
cvar_t	*cl_thirdPerson;;
cvar_t	*cl_thirdPersonAngle;
cvar_t	*cl_thirdPersonRange;
cvar_t	*cl_cameraOrbit;
cvar_t	*v_centermove;
cvar_t	*v_centerspeed;
cvar_t	*sv_reconnect_limit;
cvar_t	*s_volume;
cvar_t	*s_testsound;
cvar_t	*s_loadas8bit;
cvar_t	*s_khz;
cvar_t	*s_show;
cvar_t	*s_mixahead;
cvar_t	*s_mute_losefocus;
///cvar_t	*s_wavonly;
cvar_t	*s_initsound;
///cvar_t	*intensity;
cvar_t	*map_noareas;
cvar_t	*map_nochecklit;
///cvar_t	*r_novis;
cvar_t	*r_lockpvs;
///cvar_t	*r_nocull;
cvar_t	*r_drawentities;
cvar_t	*r_maxDistance;
cvar_t	*r_brassTimeScale;
cvar_t	*r_debrisTimeScale;
////cvar_t	*gl_modulate;
///cvar_t	*r_lerpmodels;
cvar_t	*r_shadows;
cvar_t	*r_mirror_shadows;
cvar_t	*r_teshadows;
cvar_t	*dl_shadows;
cvar_t	*r_worldshadows;
cvar_t	*r_playershadow;
cvar_t	*r_override;
cvar_t	*r_parallax;
cvar_t	*r_detailed_bump;
cvar_t	*r_detailed_bump_default;
cvar_t	*r_noselfshadows;
cvar_t	*r_editor;
cvar_t	*r_simple;
cvar_t	*r_personal_gibs;
cvar_t	*r_md3;
cvar_t	*r_crosshair_image;
cvar_t	*r_editor_layoutstring;
cvar_t	*r_bloom;
cvar_t	*r_showbloom;
cvar_t	*r_modlights;
cvar_t	*r_modemits;
cvar_t	*r_offsetFactor;
cvar_t	*r_offsetUnits;
cvar_t	*r_offset;
cvar_t	*demofov;
cvar_t	*r_sp3;
cvar_t	*r_smoothangle;
cvar_t	*scr_3dhud;
cvar_t	*scr_3dhud_intensity;
cvar_t	*scr_bighud;
cvar_t	*r_relight;
////cvar_t	*r_predator;	// changed to sv_predator
///cvar_t	*r_predator_r;
///cvar_t	*r_predator_g;
///cvar_t	*r_predator_b;
cvar_t	*r_distort;
cvar_t	*r_distort_distance;
cvar_t	*mtr_decal;
cvar_t	*mtr_footstep;
cvar_t	*mtr_debris;
cvar_t	*mtr_hitsound;

cvar_t	*r_particle_min_size;
cvar_t	*r_particle_max_size;
cvar_t	*r_particle_size;
cvar_t	*r_particle_att_a;
cvar_t	*r_particle_att_b;
cvar_t	*r_particle_att_c;
cvar_t	*r_ext_pointparameters;

///cvar_t	*r_polyblend;
cvar_t	*sv_airaccelerate;
cvar_t	*sv_noreload;			// don't reload level state when reentering

cvar_t	*allow_download;
cvar_t	*allow_download_players;
cvar_t	*allow_download_models;
cvar_t	*allow_download_sounds;
cvar_t	*allow_download_maps;

///cvar_t	*r_ext_draw_range_elements;

cvar_t	*p_rocket;
cvar_t	*p_grenade;
cvar_t	*p_gib;
cvar_t	*p_shotgun;
cvar_t	*p_machine;
cvar_t	*p_explosion;
cvar_t	*p_spawn;
cvar_t	*p_blood;
cvar_t	*p_splash;
cvar_t	*p_sparks;
cvar_t	*p_tracer;
cvar_t	*p_weapfire;

// цвет крови
cvar_t	*p_blood_r;
cvar_t	*p_blood_g;
cvar_t	*p_blood_b;
// цвет зелёной крови
cvar_t	*p_greenblood_r;
cvar_t	*p_greenblood_g;
cvar_t	*p_greenblood_b;
// цвет дыма от попадания в стену или взрыва
cvar_t	*p_smoke_r;
cvar_t	*p_smoke_g;
cvar_t	*p_smoke_b;
// цвет дыма от выстрела или полёта ракеты
cvar_t	*p_shot_r;
cvar_t	*p_shot_g;
cvar_t	*p_shot_b;

int colortable[4] = {2*8,13*8,21*8,18*8};


char q2cd_letter[3]="-:";


//ROGUE
cl_sustain_t	cl_sustains[MAX_SUSTAINS];
//ROGUE


float		v_blend[4];			// final blending color

int		r_visframecount;	// bumped when going to a new PVS
byte	mod_novis[MAX_MAP_LEAFS/8];
refdef_t	r_newrefdef;
vec3_t	modelorg;		// relative to viewpoint
entity_t	*currententity;
float	skymins[2][6], skymaxs[2][6];

vec3_t	skyclip[6] = {
	{1,1,0},
	{1,-1,0},
	{0,-1,1},
	{0,1,1},
	{1,0,1},
	{-1,0,1}
};
int	c_sky;

// 1 = s, 2 = t, 3 = 2048
int	st_to_vec[6][3] =
{
	{3,-1,2},
	{-3,1,2},

	{1,3,2},
	{-1,-3,2},

	{-2,-1,3},		// 0 degrees yaw, look straight up
	{2,-1,-3}		// look straight down

//	{-1,2,3},
//	{1,2,-3}
};

// s = [0]/[2], t = [1]/[2]
int	vec_to_st[6][3] =
{
	{-2,3,1},
	{2,3,-1},

	{1,3,2},
	{-1,3,-2},

	{-2,-1,3},
	{-2,1,-3}

//	{-1,2,3},
//	{1,2,-3}
};

int			transbrush = 0;
float		r_mvm_cache[MAX_MODELVIEW_CACHE][16+3];
int			causticbrush = 0;
float		r_mvm_cache3[MAX_MODELVIEW_CACHE][16+3];
int			distortbrush = 0;
float		r_mvm_cache2[MAX_MODELVIEW_CACHE][16];
msurface_t	*r_alpha_surfaces = NULL;
msurface_t	*r_fog_surfaces = NULL;
msurface_t	*r_uw_surfaces = NULL;
msurface_t	*r_distort_surfaces = NULL;
///msurface_t	*r_temp_surfaces;
///msurface_t	*r_scene_surfaces = NULL;
int			num_scene_surfaces;
msurface_t	*scene_surfaces[MAX_MAP_FACES];


char	skyname[MAX_QPATH];
float	skyrotate;
vec3_t	skyaxis;
image_t	*sky_images[6];
// 3dstudio environment map names
char	*suf[6] = {"rt", "bk", "lf", "ft", "up", "dn"};
int	skytexorder[6] = {0,2,1,3,4,5};

char	*lsuf[6] = {"ft", "bk", "lf", "rt", "up", "dn"};
unsigned	trans[MAX_TEXTURE_SIZE*MAX_TEXTURE_SIZE];
unsigned	scaled[MAX_TEXTURE_SIZE*MAX_TEXTURE_SIZE];


zipfile_t pf;
char *zipdata;
int file_from_pak;
int file_from_pk2;
///filelink_t	*fs_links;
zipfile_t	ZipCache[MAX_PAKS];


char	console_text[256];
int		console_textlen;

#ifdef _WIN32
HANDLE	hinput, houtput;
#endif

FILE	*logfile;

// used to restore mouse cursor position when switching from relative mouse mode
#if SDL_VERSION_ATLEAST(2,0,4)
int		mouse_x;
int		mouse_y;
#endif


bool	reflib_active = false;

enum _ControlList
{
	AxisNada = 0, AxisForward, AxisLook, AxisSide, AxisTurn, AxisUp
};

DWORD	dwAxisMap[JOY_MAX_AXES];
DWORD	dwControlMap[JOY_MAX_AXES];
Sint16	dwRawValue[JOY_MAX_AXES];

// none of these cvars are saved over a session
// this means that advanced controller configuration needs to be executed
// each time.  this avoids any problems with getting back to a default usage
// or when changing from one controller to another.  this way at least something
// works.
cvar_t	*joy_name;
cvar_t	*joy_advanced;
cvar_t	*joy_advaxisx;
cvar_t	*joy_advaxisy;
cvar_t	*joy_advaxisz;
cvar_t	*joy_advaxisr;
cvar_t	*joy_advaxisu;
cvar_t	*joy_advaxisv;
cvar_t	*joy_forwardthreshold;
cvar_t	*joy_sidethreshold;
cvar_t	*joy_pitchthreshold;
cvar_t	*joy_yawthreshold;
cvar_t	*joy_forwardsensitivity;
cvar_t	*joy_sidesensitivity;
cvar_t	*joy_pitchsensitivity;
cvar_t	*joy_yawsensitivity;
cvar_t	*joy_upthreshold;
cvar_t	*joy_upsensitivity;

bool	joy_advancedinit;
DWORD	joy_oldbuttonstate, joy_oldhatstate;

SDL_Joystick	*joy;
int			joy_numbuttons;
int			joy_numhats;

vec3_t vec3_origin = {0,0,0};

vec3_t	r_origin;
vec3_t	vup;
vec3_t	vpn;
vec3_t	vright;

client_state_t	cl;

void	(*m_drawfunc) (void);
const char *(*m_keyfunc) (int key);

bool		chat_team;
char		chat_buffer[MAXCMDLINE];
int			chat_bufferlen = 0;

bool	scr_initialized;		// ready to draw

SDL_Window	*hWnd;			// handle to window
SDL_GLContext hGLRC;		// handle to GL rendering context

viddef_t	vid;

glmode_t modes[] = {
	{"GL_NEAREST", GL_NEAREST, GL_NEAREST},
	{"GL_LINEAR", GL_LINEAR, GL_LINEAR},
	{"GL_NEAREST_MIPMAP_NEAREST", GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST},
	{"GL_LINEAR_MIPMAP_NEAREST", GL_LINEAR_MIPMAP_NEAREST, GL_LINEAR},
	{"GL_NEAREST_MIPMAP_LINEAR", GL_NEAREST_MIPMAP_LINEAR, GL_NEAREST},
	{"GL_LINEAR_MIPMAP_LINEAR", GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR}
};

image_t	gltextures[MAX_GLTEXTURES];
int		numgltextures;

material_t	materials[MAX_MATERIALS];
int		nummaterials;

glconfig_t  gl_config;
glstate_t   gl_state;


///////////////////////////////////////////////////
gltmode_t gl_alpha_modes[] = {
	{"default", 4},
	{"GL_RGBA", GL_RGBA},
	{"GL_RGBA8", GL_RGBA8},
	{"GL_RGB5_A1", GL_RGB5_A1},
	{"GL_RGBA4", GL_RGBA4},
	{"GL_RGBA2", GL_RGBA2},
};

const char *alphas[] =
{
	"default",
	"GL_RGBA",
	"GL_RGBA8",
	"GL_RGB5_A1",
	"GL_RGBA4",
	"GL_RGBA2",
	0
};


gltmode_t gl_solid_modes[] = {
	{"default", 3},
	{"GL_RGB", GL_RGB},
	{"GL_RGB8", GL_RGB8},
	{"GL_RGB5", GL_RGB5},
	{"GL_RGB4", GL_RGB4},
	{"GL_R3_G3_B2", GL_R3_G3_B2},
};

const char *solids[] =
{
	"default",
	"GL_RGB",
	"GL_RGB8",
	"GL_RGB5",
	"GL_RGB4",
	"GL_R3_G3_B2",
	0
};
///////////////////////////////////////////////////


int	gl_tex_solid_format = 3;
int	gl_tex_alpha_format = 4;

int	gl_filter_min = GL_LINEAR_MIPMAP_NEAREST;
int	gl_filter_max = GL_LINEAR;

PFNGLGENBUFFERSARBPROC					glGenBuffersARB = NULL;
PFNGLBINDBUFFERARBPROC					glBindBufferARB = NULL;
PFNGLBUFFERDATAARBPROC					glBufferDataARB = NULL;
///PFNGLMAPBUFFERARBPROC					glMapBufferARB = NULL;
///PFNGLUNMAPBUFFERARBPROC					glUnmapBufferARB = NULL;

PFNGLDELETEBUFFERSARBPROC				glDeleteBuffersARB = NULL;
PFNGLMULTITEXCOORD1FARBPROC				pglMultiTexCoord1fARB = NULL;
PFNGLMULTITEXCOORD2FARBPROC				pglMultiTexCoord2fARB = NULL;
PFNGLMULTITEXCOORD3FVARBPROC			pglMultiTexCoord3fvARB = NULL;
PFNGLACTIVETEXTUREARBPROC				pglActiveTextureARB = NULL;
PFNGLCLIENTACTIVETEXTUREARBPROC			pglClientActiveTextureARB = NULL;
PFNGLCOMBINERPARAMETERINVPROC			glCombinerParameteriNV = NULL;
PFNGLCOMBINERINPUTNVPROC				glCombinerInputNV = NULL;
PFNGLCOMBINEROUTPUTNVPROC				glCombinerOutputNV = NULL;
PFNGLFINALCOMBINERINPUTNVPROC			glFinalCombinerInputNV = NULL;
PFNGLCOMBINERPARAMETERFVNVPROC			glCombinerParameterfvNV = NULL;
PFNGLTEXIMAGE3DEXTPROC					glTexImage3DEXT = NULL;
///PFNGLLOCKARRAYSEXTPROC				glLockArraysEXT = NULL;
///PFNGLUNLOCKARRAYSEXTPROC				glUnlockArraysEXT = NULL;
PFNGLPOINTPARAMETERFEXTPROC				glPointParameterfEXT = NULL;
PFNGLPOINTPARAMETERFVEXTPROC			glPointParameterfvEXT = NULL;
PFNGLBLENDCOLORPROC						pglBlendColor = NULL;

PFNGLGENFRAGMENTSHADERSATIPROC			glGenFragmentShadersATI = NULL;
PFNGLDELETEFRAGMENTSHADERATIPROC		glDeleteFragmentShaderATI = NULL;
PFNGLBINDFRAGMENTSHADERATIPROC			glBindFragmentShaderATI = NULL;
PFNGLBEGINFRAGMENTSHADERATIPROC			glBeginFragmentShaderATI = NULL;
PFNGLSETFRAGMENTSHADERCONSTANTATIPROC	glSetFragmentShaderConstantATI = NULL;
PFNGLSAMPLEMAPATIPROC					glSampleMapATI = NULL;
PFNGLCOLORFRAGMENTOP2ATIPROC			glColorFragmentOp2ATI = NULL;
PFNGLALPHAFRAGMENTOP1ATIPROC			glAlphaFragmentOp1ATI = NULL;
PFNGLALPHAFRAGMENTOP2ATIPROC			glAlphaFragmentOp2ATI = NULL;
PFNGLALPHAFRAGMENTOP3ATIPROC			glAlphaFragmentOp3ATI = NULL;
PFNGLENDFRAGMENTSHADERATIPROC 			glEndFragmentShaderATI = NULL;
///PFNGLGENVERTEXSHADERSEXTPROC			glGenVertexShadersEXT = NULL;
///PFNGLBEGINVERTEXSHADEREXTPROC			glBeginVertexShaderEXT = NULL;
///PFNGLENDVERTEXSHADEREXTPROC				glEndVertexShaderEXT = NULL;
///PFNGLBINDVERTEXSHADEREXTPROC			glBindVertexShaderEXT = NULL;
///PFNGLBINDPARAMETEREXTPROC				glBindParameterEXT = NULL;
///PFNGLBINDTEXTUREUNITPARAMETEREXTPROC	glBindTextureUnitParameterEXT = NULL;
///PFNGLGENSYMBOLSEXTPROC					glGenSymbolsEXT = NULL;
///PFNGLSHADEROP1EXTPROC					glShaderOp1EXT = NULL;
///PFNGLSHADEROP2EXTPROC					glShaderOp2EXT = NULL;
///PFNGLEXTRACTCOMPONENTEXTPROC			glExtractComponentEXT = NULL;

PFNGLSTENCILOPSEPARATEATIPROC			glStencilOpSeparateATI = NULL;
PFNGLSTENCILFUNCSEPARATEATIPROC			glStencilFuncSeparateATI = NULL;
PFNGLACTIVESTENCILFACEEXTPROC			glActiveStencilFaceEXT = NULL;
///PFNGLDRAWRANGEELEMENTSEXTPROC			glDrawRangeElementsEXT = NULL;
///PFNGLDEPTHBOUNDSEXTPROC					glDepthBoundsEXT = NULL;


PFNGLPROGRAMSTRINGARBPROC				glProgramStringARB = NULL;
PFNGLBINDPROGRAMARBPROC					glBindProgramARB = NULL;
PFNGLDELETEPROGRAMSARBPROC				glDeleteProgramsARB = NULL;
PFNGLGENPROGRAMSARBPROC					glGenProgramsARB = NULL;
PFNGLPROGRAMENVPARAMETER4DARBPROC		glProgramEnvParameter4dARB = NULL;
PFNGLPROGRAMENVPARAMETER4DVARBPROC		glProgramEnvParameter4dvARB = NULL;
PFNGLPROGRAMENVPARAMETER4FARBPROC		glProgramEnvParameter4fARB = NULL;
PFNGLPROGRAMENVPARAMETER4FVARBPROC		glProgramEnvParameter4fvARB = NULL;
PFNGLPROGRAMLOCALPARAMETER4DARBPROC		glProgramLocalParameter4dARB = NULL;
PFNGLPROGRAMLOCALPARAMETER4DVARBPROC	glProgramLocalParameter4dvARB = NULL;
PFNGLPROGRAMLOCALPARAMETER4FARBPROC		glProgramLocalParameter4fARB = NULL;
PFNGLPROGRAMLOCALPARAMETER4FVARBPROC	glProgramLocalParameter4fvARB = NULL;
PFNGLGETPROGRAMENVPARAMETERDVARBPROC	glGetProgramEnvParameterdvARB = NULL;
PFNGLGETPROGRAMENVPARAMETERFVARBPROC	glGetProgramEnvParameterfvARB = NULL;
PFNGLGETPROGRAMLOCALPARAMETERDVARBPROC	glGetProgramLocalParameterdvARB = NULL;
PFNGLGETPROGRAMLOCALPARAMETERFVARBPROC	glGetProgramLocalParameterfvARB = NULL;
PFNGLGETPROGRAMIVARBPROC				glGetProgramivARB = NULL;
PFNGLGETPROGRAMSTRINGARBPROC			glGetProgramStringARB = NULL;
PFNGLISPROGRAMARBPROC					glIsProgramARB = NULL;

PFNGLVERTEXATTRIB1DARBPROC				glVertexAttrib1dARB = NULL;
PFNGLVERTEXATTRIB1DVARBPROC				glVertexAttrib1dvARB = NULL;
PFNGLVERTEXATTRIB1FARBPROC				glVertexAttrib1fARB = NULL;
PFNGLVERTEXATTRIB1FVARBPROC				glVertexAttrib1fvARB = NULL;
PFNGLVERTEXATTRIB1SARBPROC				glVertexAttrib1sARB = NULL;
PFNGLVERTEXATTRIB1SVARBPROC				glVertexAttrib1svARB = NULL;
PFNGLVERTEXATTRIB2DARBPROC				glVertexAttrib2dARB = NULL;
PFNGLVERTEXATTRIB2DVARBPROC				glVertexAttrib2dvARB = NULL;
PFNGLVERTEXATTRIB2FARBPROC				glVertexAttrib2fARB = NULL;
PFNGLVERTEXATTRIB2FVARBPROC				glVertexAttrib2fvARB = NULL;
PFNGLVERTEXATTRIB2SARBPROC				glVertexAttrib2sARB = NULL;
PFNGLVERTEXATTRIB2SVARBPROC				glVertexAttrib2svARB = NULL;
PFNGLVERTEXATTRIB3DARBPROC				glVertexAttrib3dARB = NULL;
PFNGLVERTEXATTRIB3DVARBPROC				glVertexAttrib3dvARB = NULL;
PFNGLVERTEXATTRIB3FARBPROC				glVertexAttrib3fARB = NULL;
PFNGLVERTEXATTRIB3FVARBPROC				glVertexAttrib3fvARB = NULL;
PFNGLVERTEXATTRIB3SARBPROC				glVertexAttrib3sARB = NULL;
PFNGLVERTEXATTRIB3SVARBPROC				glVertexAttrib3svARB = NULL;
PFNGLVERTEXATTRIB4NBVARBPROC			glVertexAttrib4NbvARB = NULL;
PFNGLVERTEXATTRIB4NIVARBPROC			glVertexAttrib4NivARB = NULL;
PFNGLVERTEXATTRIB4NSVARBPROC			glVertexAttrib4NsvARB = NULL;
PFNGLVERTEXATTRIB4NUBARBPROC			glVertexAttrib4NubARB = NULL;
PFNGLVERTEXATTRIB4NUBVARBPROC			glVertexAttrib4NubvARB = NULL;
PFNGLVERTEXATTRIB4NUIVARBPROC			glVertexAttrib4NuivARB = NULL;
PFNGLVERTEXATTRIB4NUSVARBPROC			glVertexAttrib4NusvARB = NULL;
PFNGLVERTEXATTRIB4BVARBPROC				glVertexAttrib4bvARB = NULL;
PFNGLVERTEXATTRIB4DARBPROC				glVertexAttrib4dARB = NULL;
PFNGLVERTEXATTRIB4DVARBPROC				glVertexAttrib4dvARB = NULL;
PFNGLVERTEXATTRIB4FARBPROC				glVertexAttrib4fARB = NULL;
PFNGLVERTEXATTRIB4FVARBPROC				glVertexAttrib4fvARB = NULL;
PFNGLVERTEXATTRIB4IVARBPROC				glVertexAttrib4ivARB = NULL;
PFNGLVERTEXATTRIB4SARBPROC				glVertexAttrib4sARB = NULL;
PFNGLVERTEXATTRIB4SVARBPROC				glVertexAttrib4svARB = NULL;
PFNGLVERTEXATTRIB4UBVARBPROC			glVertexAttrib4ubvARB = NULL;
PFNGLVERTEXATTRIB4UIVARBPROC			glVertexAttrib4uivARB = NULL;
PFNGLVERTEXATTRIB4USVARBPROC			glVertexAttrib4usvARB = NULL;
PFNGLVERTEXATTRIBPOINTERARBPROC			glVertexAttribPointerARB = NULL;
PFNGLENABLEVERTEXATTRIBARRAYARBPROC		glEnableVertexAttribArrayARB = NULL;
PFNGLDISABLEVERTEXATTRIBARRAYARBPROC	glDisableVertexAttribArrayARB = NULL;
PFNGLGETVERTEXATTRIBDVARBPROC			glGetVertexAttribdvARB = NULL;
PFNGLGETVERTEXATTRIBFVARBPROC			glGetVertexAttribfvARB = NULL;
PFNGLGETVERTEXATTRIBIVARBPROC			glGetVertexAttribivARB = NULL;
PFNGLGETVERTEXATTRIBPOINTERVARBPROC		glGetVertexAttribPointervARB = NULL;

PFNGLGENQUERIESARBPROC					glGenQueriesARB = NULL;
PFNGLDELETEQUERIESARBPROC				glDeleteQueriesARB = NULL;
PFNGLISQUERYARBPROC						glIsQueryARB = NULL;
PFNGLBEGINQUERYARBPROC					glBeginQueryARB = NULL;
PFNGLENDQUERYARBPROC					glEndQueryARB = NULL;
PFNGLGETQUERYIVARBPROC					glGetQueryivARB = NULL;
PFNGLGETQUERYOBJECTIVARBPROC			glGetQueryObjectivARB = NULL;
PFNGLGETQUERYOBJECTUIVARBPROC			glGetQueryObjectuivARB = NULL;




vrect_t		scr_vrect;		// position of render window on screen

viddef_t	viddef;				// global video state

dirty_t		scr_dirty, scr_old_dirty[2];

float		scr_con_current;	// aproaches scr_conlines at scr_conspeed
float		scr_conlines;		// 0.0 to 1.0 lines of console to display

int		registration_sequence;

// host_speeds times
int		time_before_game;
int		time_after_game;
int		time_before_ref;
int		time_after_ref;
int		time_before_ref_ambient;
int		time_before_ref_light;
int		time_after_ref_light;

image_t	*draw_chars;
image_t	*draw_conback;
image_t	*draw_conback_bump;
image_t	*draw_default;
image_t	*draw_mapshot = NULL;
char	saved_shot_dir[16];			// NULL, 'quick', 'save*'

int			ip_sockets[2] = { -1, -1 };

centity_t	cl_entities[MAX_EDICTS];

jmp_buf abortframe;					// an ERR_DROP/DISCONNECT occured, exit the entire frame

netadr_t	net_from;
sizebuf_t	net_message;
byte		net_message_buffer[MAX_MSGLEN];

server_static_t	svs;				// persistant server info
server_t		sv;					// local server

client_t	*sv_client;			// current client

edict_t		*sv_player;


game_export_t	*ge;

cplane_t	frustum[4];
float		frustumPlanes[6][4];

float	r_world_matrix[16];
float	r_project_matrix[16];
float   clip[16];
//float	mvpMatrix[16];
int 	r_viewport[4];
float	r_texture_matrix[16];
float	mir_project_matrix[16];

unsigned	sys_frame_time;
unsigned	frame_msec;
unsigned	old_sys_frame_time;

kbutton_t	in_klook;
kbutton_t	in_left, in_right, in_forward, in_back;
kbutton_t	in_lookup, in_lookdown, in_moveleft, in_moveright;
kbutton_t	in_strafe, in_speed, in_use, in_attack, in_flashlight, in_zoom;
kbutton_t	in_up, in_down;

int			in_impulse;
float		fov_delta;
int			fov_time;

static byte chktbl[1024] = {
0x84, 0x47, 0x51, 0xc1, 0x93, 0x22, 0x21, 0x24, 0x2f, 0x66, 0x60, 0x4d, 0xb0, 0x7c, 0xda,
0x88, 0x54, 0x15, 0x2b, 0xc6, 0x6c, 0x89, 0xc5, 0x9d, 0x48, 0xee, 0xe6, 0x8a, 0xb5, 0xf4,
0xcb, 0xfb, 0xf1, 0x0c, 0x2e, 0xa0, 0xd7, 0xc9, 0x1f, 0xd6, 0x06, 0x9a, 0x09, 0x41, 0x54,
0x67, 0x46, 0xc7, 0x74, 0xe3, 0xc8, 0xb6, 0x5d, 0xa6, 0x36, 0xc4, 0xab, 0x2c, 0x7e, 0x85,
0xa8, 0xa4, 0xa6, 0x4d, 0x96, 0x19, 0x19, 0x9a, 0xcc, 0xd8, 0xac, 0x39, 0x5e, 0x3c, 0xf2,
0xf5, 0x5a, 0x72, 0xe5, 0xa9, 0xd1, 0xb3, 0x23, 0x82, 0x6f, 0x29, 0xcb, 0xd1, 0xcc, 0x71,
0xfb, 0xea, 0x92, 0xeb, 0x1c, 0xca, 0x4c, 0x70, 0xfe, 0x4d, 0xc9, 0x67, 0x43, 0x47, 0x94,
0xb9, 0x47, 0xbc, 0x3f, 0x01, 0xab, 0x7b, 0xa6, 0xe2, 0x76, 0xef, 0x5a, 0x7a, 0x29, 0x0b,
0x51, 0x54, 0x67, 0xd8, 0x1c, 0x14, 0x3e, 0x29, 0xec, 0xe9, 0x2d, 0x48, 0x67, 0xff, 0xed,
0x54, 0x4f, 0x48, 0xc0, 0xaa, 0x61, 0xf7, 0x78, 0x12, 0x03, 0x7a, 0x9e, 0x8b, 0xcf, 0x83,
0x7b, 0xae, 0xca, 0x7b, 0xd9, 0xe9, 0x53, 0x2a, 0xeb, 0xd2, 0xd8, 0xcd, 0xa3, 0x10, 0x25,
0x78, 0x5a, 0xb5, 0x23, 0x06, 0x93, 0xb7, 0x84, 0xd2, 0xbd, 0x96, 0x75, 0xa5, 0x5e, 0xcf,
0x4e, 0xe9, 0x50, 0xa1, 0xe6, 0x9d, 0xb1, 0xe3, 0x85, 0x66, 0x28, 0x4e, 0x43, 0xdc, 0x6e,
0xbb, 0x33, 0x9e, 0xf3, 0x0d, 0x00, 0xc1, 0xcf, 0x67, 0x34, 0x06, 0x7c, 0x71, 0xe3, 0x63,
0xb7, 0xb7, 0xdf, 0x92, 0xc4, 0xc2, 0x25, 0x5c, 0xff, 0xc3, 0x6e, 0xfc, 0xaa, 0x1e, 0x2a,
0x48, 0x11, 0x1c, 0x36, 0x68, 0x78, 0x86, 0x79, 0x30, 0xc3, 0xd6, 0xde, 0xbc, 0x3a, 0x2a,
0x6d, 0x1e, 0x46, 0xdd, 0xe0, 0x80, 0x1e, 0x44, 0x3b, 0x6f, 0xaf, 0x31, 0xda, 0xa2, 0xbd,
0x77, 0x06, 0x56, 0xc0, 0xb7, 0x92, 0x4b, 0x37, 0xc0, 0xfc, 0xc2, 0xd5, 0xfb, 0xa8, 0xda,
0xf5, 0x57, 0xa8, 0x18, 0xc0, 0xdf, 0xe7, 0xaa, 0x2a, 0xe0, 0x7c, 0x6f, 0x77, 0xb1, 0x26,
0xba, 0xf9, 0x2e, 0x1d, 0x16, 0xcb, 0xb8, 0xa2, 0x44, 0xd5, 0x2f, 0x1a, 0x79, 0x74, 0x87,
0x4b, 0x00, 0xc9, 0x4a, 0x3a, 0x65, 0x8f, 0xe6, 0x5d, 0xe5, 0x0a, 0x77, 0xd8, 0x1a, 0x14,
0x41, 0x75, 0xb1, 0xe2, 0x50, 0x2c, 0x93, 0x38, 0x2b, 0x6d, 0xf3, 0xf6, 0xdb, 0x1f, 0xcd,
0xff, 0x14, 0x70, 0xe7, 0x16, 0xe8, 0x3d, 0xf0, 0xe3, 0xbc, 0x5e, 0xb6, 0x3f, 0xcc, 0x81,
0x24, 0x67, 0xf3, 0x97, 0x3b, 0xfe, 0x3a, 0x96, 0x85, 0xdf, 0xe4, 0x6e, 0x3c, 0x85, 0x05,
0x0e, 0xa3, 0x2b, 0x07, 0xc8, 0xbf, 0xe5, 0x13, 0x82, 0x62, 0x08, 0x61, 0x69, 0x4b, 0x47,
0x62, 0x73, 0x44, 0x64, 0x8e, 0xe2, 0x91, 0xa6, 0x9a, 0xb7, 0xe9, 0x04, 0xb6, 0x54, 0x0c,
0xc5, 0xa9, 0x47, 0xa6, 0xc9, 0x08, 0xfe, 0x4e, 0xa6, 0xcc, 0x8a, 0x5b, 0x90, 0x6f, 0x2b,
0x3f, 0xb6, 0x0a, 0x96, 0xc0, 0x78, 0x58, 0x3c, 0x76, 0x6d, 0x94, 0x1a, 0xe4, 0x4e, 0xb8,
0x38, 0xbb, 0xf5, 0xeb, 0x29, 0xd8, 0xb0, 0xf3, 0x15, 0x1e, 0x99, 0x96, 0x3c, 0x5d, 0x63,
0xd5, 0xb1, 0xad, 0x52, 0xb8, 0x55, 0x70, 0x75, 0x3e, 0x1a, 0xd5, 0xda, 0xf6, 0x7a, 0x48,
0x7d, 0x44, 0x41, 0xf9, 0x11, 0xce, 0xd7, 0xca, 0xa5, 0x3d, 0x7a, 0x79, 0x7e, 0x7d, 0x25,
0x1b, 0x77, 0xbc, 0xf7, 0xc7, 0x0f, 0x84, 0x95, 0x10, 0x92, 0x67, 0x15, 0x11, 0x5a, 0x5e,
0x41, 0x66, 0x0f, 0x38, 0x03, 0xb2, 0xf1, 0x5d, 0xf8, 0xab, 0xc0, 0x02, 0x76, 0x84, 0x28,
0xf4, 0x9d, 0x56, 0x46, 0x60, 0x20, 0xdb, 0x68, 0xa7, 0xbb, 0xee, 0xac, 0x15, 0x01, 0x2f,
0x20, 0x09, 0xdb, 0xc0, 0x16, 0xa1, 0x89, 0xf9, 0x94, 0x59, 0x00, 0xc1, 0x76, 0xbf, 0xc1,
0x4d, 0x5d, 0x2d, 0xa9, 0x85, 0x2c, 0xd6, 0xd3, 0x14, 0xcc, 0x02, 0xc3, 0xc2, 0xfa, 0x6b,
0xb7, 0xa6, 0xef, 0xdd, 0x12, 0x26, 0xa4, 0x63, 0xe3, 0x62, 0xbd, 0x56, 0x8a, 0x52, 0x2b,
0xb9, 0xdf, 0x09, 0xbc, 0x0e, 0x97, 0xa9, 0xb0, 0x82, 0x46, 0x08, 0xd5, 0x1a, 0x8e, 0x1b,
0xa7, 0x90, 0x98, 0xb9, 0xbb, 0x3c, 0x17, 0x9a, 0xf2, 0x82, 0xba, 0x64, 0x0a, 0x7f, 0xca,
0x5a, 0x8c, 0x7c, 0xd3, 0x79, 0x09, 0x5b, 0x26, 0xbb, 0xbd, 0x25, 0xdf, 0x3d, 0x6f, 0x9a,
0x8f, 0xee, 0x21, 0x66, 0xb0, 0x8d, 0x84, 0x4c, 0x91, 0x45, 0xd4, 0x77, 0x4f, 0xb3, 0x8c,
0xbc, 0xa8, 0x99, 0xaa, 0x19, 0x53, 0x7c, 0x02, 0x87, 0xbb, 0x0b, 0x7c, 0x1a, 0x2d, 0xdf,
0x48, 0x44, 0x06, 0xd6, 0x7d, 0x0c, 0x2d, 0x35, 0x76, 0xae, 0xc4, 0x5f, 0x71, 0x85, 0x97,
0xc4, 0x3d, 0xef, 0x52, 0xbe, 0x00, 0xe4, 0xcd, 0x49, 0xd1, 0xd1, 0x1c, 0x3c, 0xd0, 0x1c,
0x42, 0xaf, 0xd4, 0xbd, 0x58, 0x34, 0x07, 0x32, 0xee, 0xb9, 0xb5, 0xea, 0xff, 0xd7, 0x8c,
0x0d, 0x2e, 0x2f, 0xaf, 0x87, 0xbb, 0xe6, 0x52, 0x71, 0x22, 0xf5, 0x25, 0x17, 0xa1, 0x82,
0x04, 0xc2, 0x4a, 0xbd, 0x57, 0xc6, 0xab, 0xc8, 0x35, 0x0c, 0x3c, 0xd9, 0xc2, 0x43, 0xdb,
0x27, 0x92, 0xcf, 0xb8, 0x25, 0x60, 0xfa, 0x21, 0x3b, 0x04, 0x52, 0xc8, 0x96, 0xba, 0x74,
0xe3, 0x67, 0x3e, 0x8e, 0x8d, 0x61, 0x90, 0x92, 0x59, 0xb6, 0x1a, 0x1c, 0x5e, 0x21, 0xc1,
0x65, 0xe5, 0xa6, 0x34, 0x05, 0x6f, 0xc5, 0x60, 0xb1, 0x83, 0xc1, 0xd5, 0xd5, 0xed, 0xd9,
0xc7, 0x11, 0x7b, 0x49, 0x7a, 0xf9, 0xf9, 0x84, 0x47, 0x9b, 0xe2, 0xa5, 0x82, 0xe0, 0xc2,
0x88, 0xd0, 0xb2, 0x58, 0x88, 0x7f, 0x45, 0x09, 0x67, 0x74, 0x61, 0xbf, 0xe6, 0x40, 0xe2,
0x9d, 0xc2, 0x47, 0x05, 0x89, 0xed, 0xcb, 0xbb, 0xb7, 0x27, 0xe7, 0xdc, 0x7a, 0xfd, 0xbf,
0xa8, 0xd0, 0xaa, 0x10, 0x39, 0x3c, 0x20, 0xf0, 0xd3, 0x6e, 0xb1, 0x72, 0xf8, 0xe6, 0x0f,
0xef, 0x37, 0xe5, 0x09, 0x33, 0x5a, 0x83, 0x43, 0x80, 0x4f, 0x65, 0x2f, 0x7c, 0x8c, 0x6a,
0xa0, 0x82, 0x0c, 0xd4, 0xd4, 0xfa, 0x81, 0x60, 0x3d, 0xdf, 0x06, 0xf1, 0x5f, 0x08, 0x0d,
0x6d, 0x43, 0xf2, 0xe3, 0x11, 0x7d, 0x80, 0x32, 0xc5, 0xfb, 0xc5, 0xd9, 0x27, 0xec, 0xc6,
0x4e, 0x65, 0x27, 0x76, 0x87, 0xa6, 0xee, 0xee, 0xd7, 0x8b, 0xd1, 0xa0, 0x5c, 0xb0, 0x42,
0x13, 0x0e, 0x95, 0x4a, 0xf2, 0x06, 0xc6, 0x43, 0x33, 0xf4, 0xc7, 0xf8, 0xe7, 0x1f, 0xdd,
0xe4, 0x46, 0x4a, 0x70, 0x39, 0x6c, 0xd0, 0xed, 0xca, 0xbe, 0x60, 0x3b, 0xd1, 0x7b, 0x57,
0x48, 0xe5, 0x3a, 0x79, 0xc1, 0x69, 0x33, 0x53, 0x1b, 0x80, 0xb8, 0x91, 0x7d, 0xb4, 0xf6,
0x17, 0x1a, 0x1d, 0x5a, 0x32, 0xd6, 0xcc, 0x71, 0x29, 0x3f, 0x28, 0xbb, 0xf3, 0x5e, 0x71,
0xb8, 0x43, 0xaf, 0xf8, 0xb9, 0x64, 0xef, 0xc4, 0xa5, 0x6c, 0x08, 0x53, 0xc7, 0x00, 0x10,
0x39, 0x4f, 0xdd, 0xe4, 0xb6, 0x19, 0x27, 0xfb, 0xb8, 0xf5, 0x32, 0x73, 0xe5, 0xcb, 0x32
};

static WORD crctable[256] =
{
	0x0000,	0x1021,	0x2042,	0x3063,	0x4084,	0x50a5,	0x60c6,	0x70e7,
	0x8108,	0x9129,	0xa14a,	0xb16b,	0xc18c,	0xd1ad,	0xe1ce,	0xf1ef,
	0x1231,	0x0210,	0x3273,	0x2252,	0x52b5,	0x4294,	0x72f7,	0x62d6,
	0x9339,	0x8318,	0xb37b,	0xa35a,	0xd3bd,	0xc39c,	0xf3ff,	0xe3de,
	0x2462,	0x3443,	0x0420,	0x1401,	0x64e6,	0x74c7,	0x44a4,	0x5485,
	0xa56a,	0xb54b,	0x8528,	0x9509,	0xe5ee,	0xf5cf,	0xc5ac,	0xd58d,
	0x3653,	0x2672,	0x1611,	0x0630,	0x76d7,	0x66f6,	0x5695,	0x46b4,
	0xb75b,	0xa77a,	0x9719,	0x8738,	0xf7df,	0xe7fe,	0xd79d,	0xc7bc,
	0x48c4,	0x58e5,	0x6886,	0x78a7,	0x0840,	0x1861,	0x2802,	0x3823,
	0xc9cc,	0xd9ed,	0xe98e,	0xf9af,	0x8948,	0x9969,	0xa90a,	0xb92b,
	0x5af5,	0x4ad4,	0x7ab7,	0x6a96,	0x1a71,	0x0a50,	0x3a33,	0x2a12,
	0xdbfd,	0xcbdc,	0xfbbf,	0xeb9e,	0x9b79,	0x8b58,	0xbb3b,	0xab1a,
	0x6ca6,	0x7c87,	0x4ce4,	0x5cc5,	0x2c22,	0x3c03,	0x0c60,	0x1c41,
	0xedae,	0xfd8f,	0xcdec,	0xddcd,	0xad2a,	0xbd0b,	0x8d68,	0x9d49,
	0x7e97,	0x6eb6,	0x5ed5,	0x4ef4,	0x3e13,	0x2e32,	0x1e51,	0x0e70,
	0xff9f,	0xefbe,	0xdfdd,	0xcffc,	0xbf1b,	0xaf3a,	0x9f59,	0x8f78,
	0x9188,	0x81a9,	0xb1ca,	0xa1eb,	0xd10c,	0xc12d,	0xf14e,	0xe16f,
	0x1080,	0x00a1,	0x30c2,	0x20e3,	0x5004,	0x4025,	0x7046,	0x6067,
	0x83b9,	0x9398,	0xa3fb,	0xb3da,	0xc33d,	0xd31c,	0xe37f,	0xf35e,
	0x02b1,	0x1290,	0x22f3,	0x32d2,	0x4235,	0x5214,	0x6277,	0x7256,
	0xb5ea,	0xa5cb,	0x95a8,	0x8589,	0xf56e,	0xe54f,	0xd52c,	0xc50d,
	0x34e2,	0x24c3,	0x14a0,	0x0481,	0x7466,	0x6447,	0x5424,	0x4405,
	0xa7db,	0xb7fa,	0x8799,	0x97b8,	0xe75f,	0xf77e,	0xc71d,	0xd73c,
	0x26d3,	0x36f2,	0x0691,	0x16b0,	0x6657,	0x7676,	0x4615,	0x5634,
	0xd94c,	0xc96d,	0xf90e,	0xe92f,	0x99c8,	0x89e9,	0xb98a,	0xa9ab,
	0x5844,	0x4865,	0x7806,	0x6827,	0x18c0,	0x08e1,	0x3882,	0x28a3,
	0xcb7d,	0xdb5c,	0xeb3f,	0xfb1e,	0x8bf9,	0x9bd8,	0xabbb,	0xbb9a,
	0x4a75,	0x5a54,	0x6a37,	0x7a16,	0x0af1,	0x1ad0,	0x2ab3,	0x3a92,
	0xfd2e,	0xed0f,	0xdd6c,	0xcd4d,	0xbdaa,	0xad8b,	0x9de8,	0x8dc9,
	0x7c26,	0x6c07,	0x5c64,	0x4c45,	0x3ca2,	0x2c83,	0x1ce0,	0x0cc1,
	0xef1f,	0xff3e,	0xcf5d,	0xdf7c,	0xaf9b,	0xbfba,	0x8fd9,	0x9ff8,
	0x6e17,	0x7e36,	0x4e55,	0x5e74,	0x2e93,	0x3eb2,	0x0ed1,	0x1ef0
};


static byte PADDING[64] = {0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

int		m_num_servers;
// user readable information
static char local_server_names[MAX_LOCAL_SERVERS][80];
// network address
static netadr_t local_server_netadr[MAX_LOCAL_SERVERS];

char		scr_centerstring[1024];
///float		scr_centertime_start;	// for slow victory printing
float		scr_centertime_off;
int			scr_center_lines;
int			scr_erase_center;

entity_state_t	cl_parse_entities[MAX_PARSE_ENTITIES];

char	*sb_nums[2][11] =
{
	{"num_0.tga", "num_1.tga", "num_2.tga", "num_3.tga", "num_4.tga", "num_5.tga",
	"num_6.tga", "num_7.tga", "num_8.tga", "num_9.tga", "num_minus.tga"},
	{"anum_0.tga", "anum_1.tga", "anum_2.tga", "anum_3.tga", "anum_4.tga", "anum_5.tga",
	"anum_6.tga", "anum_7.tga", "anum_8.tga", "anum_9.tga", "anum_minus.tga"}
};

char		crosshair_pic[MAX_QPATH];
int			crosshair_width, crosshair_height;


unsigned	num_pains;
image_t	*glfx_pain[MAX_RFX_IMAGES];
unsigned	num_burns;
image_t	*glfx_burn[MAX_RFX_IMAGES];
image_t	*glfx_mask;
image_t	*glfx_maskenv;
image_t	*glfx_underwater;
image_t	*glfx_drown;


image_t		*r_notexture;
image_t		*r_particletexture;
image_t		*r_defaultbump;
image_t		*r_nodetailbump;
image_t		*r_defaultbump_detail;
image_t		*r_dark;
image_t		*r_dark2;
image_t		*r_white;
image_t		*r_screenTexture;
image_t		*r_bloomTexture;
#define		MAX_CAUSTICS		32
image_t		*r_caustic[MAX_CAUSTICS];
image_t		*normcube_texture_object;
image_t		*atten3d_texture_object;
image_t		*dst_texture;
image_t		*dst_texture_water;
image_t		*env_texture_2d_r;
image_t		*env_texture_2d_g;
image_t		*env_texture_2d_b;
image_t		*env_texture_2d_rg;
image_t		*env_texture_2d_rb;
image_t		*env_texture_2d_gb;
image_t		*env_texture_2d_rgb;
image_t		*chrome_texture_2d;
image_t		*env_texture_2d_world;
image_t		*chrome_texture_2d_world;
image_t		*bullet_mrk_object;
image_t		*laser_mrk_object;
image_t		*blood_mrk_object[8];
image_t		*greenblood_mrk_object[8];
image_t		*foot_mrk_object[5];
image_t		*explosion_mrk_object;
image_t		*bfg_explosion_mrk_object;
image_t		*rail_mrk_object[8];
image_t		*sun_object;
image_t		*sun1_object;
image_t		*sun2_object;
image_t		*smoke_object;
image_t		*blood_object;
image_t		*bubble_object;
image_t		*splat_object;
image_t		*waterwake_object;
image_t		*waterplume_object;
image_t		*tracer_object;
image_t		*fly_object;
image_t		*filtercube_texture_object[MAX_FILTERS];
image_t		*decal_texture_object[MAX_FILTERS];


int			_VID_NUM_MODES;
vidmode_t	vid_modes[MAX_VID_NUM_MODES];
char		_resolutions[MAX_VID_NUM_MODES+1][12];
const char	*resolutions[MAX_VID_NUM_MODES+1];


int			sound_started = 0;
int			num_sfx;
sfx_t		known_sfx[MAX_SFX_BERS];
int			s_registration_sequence;
bool		s_registering;

byte	*data_p;
byte 	*iff_end;
byte 	*last_chunk;
byte 	*iff_data;
int 	iff_chunk_len;

dma_t	dma;

playsound_t	s_playsounds[MAX_PLAYSOUNDS];
playsound_t	s_freeplays;
playsound_t	s_pendingplays;
int	s_beginofs;
int paintedtime; 	// sample PAIRS
int	soundtime;		// sample PAIRS
int	s_rawend;
byte		*s_backgroundFile;
int			s_backgroundFileLength;		// for convert OGG -> WAV
unsigned	s_backgroundFile_offset, s_backgroundFile_Start, s_backgroundFile_offset_offset;
int			s_backgroundSamples;
char		s_backgroundLoop[MAX_QPATH];
char		s_playingFile[MAX_QPATH];
wavinfo_t	s_backgroundInfo;
bool		s_IntroEQULoop;		// true if intro == loop
bool		credits_backgroundFile = false;
//глобальные переменные для буферизованой подгрузки ogg
byte			*ogg_file_buffer = NULL;
unsigned		ogg_file_buffer_size = 0;
unsigned		ogg_file_buffer_pos = 0;

channel_t   channels[MAX_CHANNELS];

int		snd_scaletable[32][256];

vec3_t		listener_origin;
vec3_t		listener_forward;
vec3_t		listener_right;
vec3_t		listener_up;

int 	*snd_p, snd_linear_count, snd_vol;
short	*snd_out;
portable_samplepair_t	paintbuffer[PAINTBUFFER_SIZE];
portable_samplepair_t	s_rawsamples[MAX_RAW_SAMPLES];

int		scr_draw_loading;

char	sv_outputbuf[SV_OUTPUTBUF_LENGTH];


menulayer_t	m_layers[MAX_MENU_DEPTH+1];
int		m_menudepth;
bool	m_entersound;		// play after drawing a frame, so caching
							// won't disrupt the sound

static int	m_main_cursor;

char *menu_in_sound		= "misc/menu1.wav";
char *menu_move_sound	= "misc/menu2.wav";
char *menu_out_sound	= "misc/menu3.wav";

menuframework_s	s_game_menu;
menuaction_s		s_easy_game_action;
menuaction_s		s_medium_game_action;
menuaction_s		s_hard_game_action;
menuaction_s		s_nm_game_action;
menuseparator_s		s_blankline;
menuaction_s		s_sm_game_action;
menuaction_s		s_smod_game_action;
menuaction_s		s_game_options_action;
menuaction_s		s_load_quickgame_action;
menuaction_s		s_load_game_action;
menuaction_s		s_save_game_action;
menuaction_s		s_credits_action;

int	m_game_cursor;

menuframework_s	s_loadquickgame_menu;
menuframework_s	s_savegame_menu;
menuframework_s	s_loadgame_menu;
menuaction_s	s_load_quickgame_actions[MAX_SAVEGAMES];
menuaction_s	s_loadgame_actions[MAX_SAVEGAMES];
menuaction_s	s_savegame_actions[MAX_SAVEGAMES];
char			m_savestrings[MAX_SAVEGAMES][32];
bool			m_savevalid[MAX_SAVEGAMES];
///unsigned		m_saveshots[MAX_SAVEGAMES];
char			m_savemapnames[MAX_SAVEGAMES][32];

menuframework_s	s_demos_menu;
menuaction_s	s_demos_actions[MAX_DEMOS];
char			m_demos[MAX_DEMOS][MAX_QPATH+1];
char			*m_demosvalid[MAX_DEMOS];
char			m_demos_short[MAX_DEMOS][MAX_DEMO_NAMELEN+1];

menuframework_s	s_mods_menu;
menuaction_s	s_mods_actions[MAX_MODS];
char			m_mod_names[MAX_MODS][MAX_MOD_NAMELEN+1];

menuseparator_s	s_maps_title;
menulist_s		s_maps_list;
menuseparator_s	s_skill_title;
menulist_s		s_skill_box;
char			**map_names;
int				num_maps = 0;
menuframework_s	s_maps_menu;
char			m_maps[MAX_MAPS][MAX_MAP_NAMELEN+1];
bool			m_mapsvalid[MAX_MAPS];

int credits_start_time;
const char **credits;
const char *creditsIndex[256];
char *creditsBuffer = NULL;
const char *berscredits[] =
{
	"+Berserker @ Quake2",
	"by Berserker (tm)",
	"(C) 2000-2015",
	"Russia, Chelyabinsk"
	"",
	"+PROGRAMMING",
	"Serge \"Berserker\" Borodulin",
	"berserker.quakegate.ru",
	"",
	"+New mouse code",
	"\"Willow\"",
	"",
	"+R1Q2 dedicated server",
	"r1ch.net",
	"+ANTICHEAT",
	"antiche.at",
	"Richard \"R1CH\" Stanway",
	"",
	"+RETEXTURE",
	"\"Berserker\"",
	"\"Alexander \"ATex\" Betliy\"",
	"\"GT-Agressor\"",
	"\"Just Jim\"",
	"www-personal.umich.edu/~jimw",
	"",
	"+RELIGHT",
	"\"Berserker\"",
	"",
	"+HI-POLY MODELS",
	"Arthur \"Turic\" Galactionov",
	"CTPEJIOK22.deviantart.com",
	"",
	"+BETA TESTING",
	"Serge \"V_2540\" Shabrov",
	"\"D_R_\"",
	"and other people",
	"",
	"+SOUND TRACKS",
	"\"tigrik2017\"",
	"+CREDITS SOUND TRACK",
	"Alexander \"ATex\" Betliy",
	"Sergey Dovzhenko",
	"Ukraine, Odessa"
	"",
	"+SPECIAL THANKS:",
	"+WEB HOSTING",
	"www.quakegate.ru",
	"[RWG]DOOMer",
	"and",
	"+HARDWARE:",
	"+ATI HD5830, PSU Zalman 600W",
	"+CPU Q9450, M/B P45, RAM 4GB HyperX",
	"+ViewSonic VX2255wmb 22\" HD LCD",
	"Serge \"V_2540\" Shabrov",
	"Russia, Magnitogorsk"
	"",
	"",
	"",
	"+QUAKE II BY ID SOFTWARE",
	"",
	"+PROGRAMMING",
	"John Carmack",
	"John Cash",
	"Brian Hook",
	"",
	"+ART",
	"Adrian Carmack",
	"Kevin Cloud",
	"Paul Steed",
	"",
	"+LEVEL DESIGN",
	"Tim Willits",
	"American McGee",
	"Christian Antkow",
	"Paul Jaquays",
	"Brandon James",
	"",
	"+BIZ",
	"Todd Hollenshead",
	"Barrett (Bear) Alexander",
	"Donna Jackson",
	"",
	"",
	"+SPECIAL THANKS",
	"Ben Donges for beta testing",
	"",
	"",
	"",
	"",
	"+ADDITIONAL SUPPORT",
	"",
	"+LINUX PORT AND CTF",
	"Dave \"Zoid\" Kirsch",
	"",
	"+CINEMATIC SEQUENCES",
	"Ending Cinematic by Blur Studio - ",
	"Venice, CA",
	"",
	"Environment models for Introduction",
	"Cinematic by Karl Dolgener",
	"",
	"Assistance with environment design",
	"by Cliff Iwai",
	"",
	"+SOUND EFFECTS AND MUSIC",
	"Sound Design by Soundelux Media Labs.",
	"Music Composed and Produced by",
	"Soundelux Media Labs.  Special thanks",
	"to Bill Brown, Tom Ozanich, Brian",
	"Celano, Jeff Eisner, and The Soundelux",
	"Players.",
	"",
	"\"Level Music\" by Sonic Mayhem",
	"www.sonicmayhem.com",
	"",
	"\"Quake II Theme Song\"",
	"(C) 1997 Rob Zombie. All Rights",
	"Reserved.",
	"",
	"Track 10 (\"Climb\") by Jer Sypult",
	"",
	"Voice of computers by",
	"Carly Staehlin-Taylor",
	"",
	"+THANKS TO ACTIVISION",
	"+IN PARTICULAR:",
	"",
	"John Tam",
	"Steve Rosenthal",
	"Marty Stratton",
	"Henk Hartong",
	"",
	"Quake II(tm) (C)1997 Id Software, Inc.",
	"All Rights Reserved.  Distributed by",
	"Activision, Inc. under license.",
	"Quake II(tm), the Id Software name,",
	"the \"Q II\"(tm) logo and id(tm)",
	"logo are trademarks of Id Software,",
	"Inc. Activision(R) is a registered",
	"trademark of Activision, Inc. All",
	"other trademarks and trade names are",
	"properties of their respective owners.",
	0
};


menuframework_s	s_multiplayer_menu;
menuaction_s		s_join_network_server_action;
menuaction_s		s_start_network_server_action;
menuaction_s		s_start_mod_server_action;
menuaction_s		s_disconnect_action;
menulist_s			s_killserver_action;
menuaction_s		s_player_setup_action;

menuframework_s	s_joinserver_menu;
menuseparator_s	s_joinserver_server_title;
menuaction_s		s_joinserver_search_action;
menuaction_s		s_joinserver_address_book_action;
menuaction_s		s_joinserver_server_actions[MAX_LOCAL_SERVERS];

menuframework_s	s_addressbook_menu;
menufield_s		s_addressbook_fields[NUM_ADDRESSBOOK_ENTRIES];

menuframework_s s_startserver_menu;
char **mapnames;
int	  nummaps = 0;
menuaction_s	s_startserver_start_action;
menuaction_s	s_startserver_dmoptions_action;
menuseparator_s	s_timelimit_title;
menufield_s	s_timelimit_field;
menuseparator_s	s_fraglimit_title;
menufield_s	s_fraglimit_field;
menuseparator_s	s_maxclients_title;
menufield_s	s_maxclients_field;
menuseparator_s	s_hostname_title;
menuseparator_s	s_mapname_title;
menufield_s	s_hostname_field;
menufield_s	s_mapname_field;
menulist_s	s_startmap_list;
menuseparator_s	s_rules_title;
menulist_s	s_rules_box;

char **paknames;
int	  numpaks = 0;

char **paknames_[2];
int	  numpaks_[2];

///char dmoptions_statusbar[128];

menuframework_s s_dmoptions_menu;

menulist_s	s_friendlyfire_box;
menulist_s	s_falls_box;
menulist_s	s_weapons_stay_box;
menulist_s	s_instant_powerups_box;
menulist_s	s_powerups_box;
menulist_s	s_health_box;
menulist_s	s_spawn_farthest_box;
menulist_s	s_teamplay_box;
menulist_s	s_samelevel_box;
menulist_s	s_force_respawn_box;
menulist_s	s_armor_box;
menulist_s	s_allow_exit_box;
menulist_s	s_infinite_ammo_box;
menulist_s	s_fixed_fov_box;
menulist_s	s_quad_drop_box;
menulist_s	s_drop_weapon_box;
menulist_s	s_flashlight_box;
menulist_s	s_flashlightcells_box;
menulist_s	s_footprint_box;
menulist_s	s_fastrocket_box;
menulist_s	s_haste_box;
menulist_s	s_shell_box;
menulist_s	s_doublejump_box;
menulist_s	s_godlight_box;
menulist_s	s_instagib_box;


menuframework_s	s_player_config_menu;
menufield_s		s_player_name_field;
menulist_s		s_player_model_box;
menulist_s		s_player_skin_box;
menulist_s		s_player_handedness_box;
menulist_s		s_player_rate_box;
menulist_s		s_player_handicap_box;
menulist_s		s_player_core_box;
menulist_s		s_player_spiral_box;
menufield_s		s_player_radius_field;
menuseparator_s	s_player_skin_title;
menuseparator_s	s_player_model_title;
menuseparator_s	s_player_core_title;
menuseparator_s	s_player_spiral_title;
menuseparator_s	s_player_radius_title;
menuseparator_s	s_player_hand_title;
menuseparator_s	s_player_rate_title;
menuseparator_s	s_player_handicap_title;
menuaction_s	s_player_download_action;
menuseparator_s	s_player_pose_title;
menulist_s		s_player_pose_box;
menulist_s		s_player_rot_box;


#define	SV_UNLIMITED_RATE	9999999
int rate_tbl[] = { 2500, 3200, 5000, 10000, 25000, SV_UNLIMITED_RATE };
const char *rate_names[] = { "28.8 Modem", "33.6 Modem", "Single ISDN",	"Dual ISDN/Cable", "T1/LAN", "Unlimited", 0 };

int handicap_tbl[] = { 5, 10, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 65, 70, 75, 80, 85, 90, 95, 100 };
const char *handicap_names[] = { "5", "10", "15", "20", "25", "30", "35", "40", "45", "50", "55", "60", "65", "70", "75", "80", "85", "90", "95", "none", 0 };

int	pose = 8;//0;
const char *pose_names[] = { "stand", "run", "attack", "pain1", "pain2", "pain3", "jump", "flip", "salute", "taunt", "wave", "point", "crouch stand", "crouch walk", "crouch attack", "crouch pain", 0 };
int	pose_start[] = { PLAYER_FRAME_stand01, PLAYER_FRAME_run1, PLAYER_FRAME_attack1, PLAYER_FRAME_pain101, PLAYER_FRAME_pain201, PLAYER_FRAME_pain301, PLAYER_FRAME_jump1, PLAYER_FRAME_flip01, PLAYER_FRAME_salute01, PLAYER_FRAME_taunt01, PLAYER_FRAME_wave01, PLAYER_FRAME_point01, PLAYER_FRAME_crstnd01, PLAYER_FRAME_crwalk1, PLAYER_FRAME_crattak1, PLAYER_FRAME_crpain1 };
int	pose_end[] = { PLAYER_FRAME_stand40, PLAYER_FRAME_run6, PLAYER_FRAME_attack8, PLAYER_FRAME_pain104, PLAYER_FRAME_pain204, PLAYER_FRAME_pain304, PLAYER_FRAME_jump6, PLAYER_FRAME_flip12, PLAYER_FRAME_salute11, PLAYER_FRAME_taunt17, PLAYER_FRAME_wave11, PLAYER_FRAME_point12, PLAYER_FRAME_crstnd19, PLAYER_FRAME_crwalk6, PLAYER_FRAME_crattak9, PLAYER_FRAME_crpain4 };
int	pose_rot = 7;
const char *pose_rot_names[] = { "0 degrees", "30 degrees", "60 degrees", "90 degrees", "120 degrees", "150 degrees", "180 degrees", "210 degrees", "240 degrees", "270 degrees", "300 degrees", "330 degrees", "auto rotate", 0 };
int	pose_rot_angle[] = { 0, 30, 60, 90, 120, 150, 180, 210, 240, 270, 300, 330, -1 };


menuframework_s	s_options_menu;
menuaction_s		s_options_defaults_action;
menuaction_s		s_options_customize_options_action;
menuaction_s		s_options_customize2_options_action;
menuslider_s		s_options_sensitivity_slider;
menuslider_s		s_options_zoomspeed_slider;
menulist_s			s_options_freelook_box;
menulist_s			s_options_joystick_box;
menulist_s			s_options_alwaysrun_box;
menulist_s			s_options_invertmouse_box;
menulist_s			s_options_lookspring_box;
menulist_s			s_options_lookstrafe_box;
menulist_s			s_options_crosshair_box;
menuslider_s		s_options_sfxvolume_slider;
menuslider_s		s_options_cdvolume_slider;
menulist_s			s_options_cdvolume_box;
menulist_s			s_options_quality_list;
menulist_s			s_options_mutefocus_list;
menulist_s			s_options_onoff_list;
menuaction_s		s_keys_help_computer_action;


menuframework_s		s_downloadoptions_menu;
menuseparator_s		s_download_title;
menulist_s			s_allow_download_box;
menulist_s			s_allow_download_maps_box;
menulist_s			s_allow_download_models_box;
menulist_s			s_allow_download_players_box;
menulist_s			s_allow_download_sounds_box;


char *bindnames[][2] =
{
{"+attack", 		"attack"},
{"+forward", 		"walk forward"},
{"+back", 			"backpedal"},
{"+left", 			"turn left"},
{"+right", 			"turn right"},
{"+speed", 			"run"},
{"+moveleft", 		"step left"},
{"+moveright", 		"step right"},
{"+strafe", 		"sidestep"},
{"+lookup", 		"look up"},
{"+lookdown", 		"look down"},
{"centerview", 		"center view"},
{"+mlook", 			"mouse look"},
{"+klook", 			"keyboard look"},
{"+moveup",			"up / jump"},
{"+movedown",		"down / crouch"},
{"+flashlight",		"flashlight"},
{"+zoom",			"zoom"},
{"inven",			"inventory"},
{"invuse",			"use item"},
{"invdrop",			"drop item"},
{"invprev",			"prev item"},
{"invnext",			"next item"},
{"cmd help", 		"help computer" },
{"screenshot",		"screenshot"},
{ 0, 0 }
};

char *bindnames2[][2] =
{
{"use Blaster", 		"Blaster"},
{"use Shotgun", 		"Shotgun"},
{"use Super Shotgun",	"Super Shotgun"},
{"use Machinegun", 		"Machinegun"},
{"use Chaingun", 		"Chaingun"},
{"use Grenades",		"Grenades"},
{"use Grenade Launcher","Grenade Launcher"},
{"use Rocket Launcher",	"Rocket Launcher"},
{"use HyperBlaster", 	"HyperBlaster"},
{"use Railgun", 		"Railgun"},
{"use BFG10K", 			"BFG10K"},
{"weapprev", 			"prev.weapon"},
{"weapnext", 			"next weapon"},
{"use Quad Damage", 	"Quad Damage"},
{"use Invulnerability", "Invulnerability"},
{"use Rebreather", 		"Rebreather"},
{"use Environment Suit","Environment Suit"},
{"use Silencer",		"Silencer"},
{"use Power Shield",	"Power Shield"},
{"use Invisibility",	"Invisibility"},
{"messagemode",			"Chat"},
{"echo Quick Saving...; wait; save quick",	"Quick Saving"},
{"echo Quick Loading...; wait; load quick",	"Quick Loading"},
{"menu_quit",			"Quit"},
{ 0, 0 }
};

int		bind_grab;
menuframework_s	s_keys_menu;
menuaction_s		s_keys_attack_action;
menuaction_s		s_keys_walk_forward_action;
menuaction_s		s_keys_backpedal_action;
menuaction_s		s_keys_turn_left_action;
menuaction_s		s_keys_turn_right_action;
menuaction_s		s_keys_run_action;
menuaction_s		s_keys_step_left_action;
menuaction_s		s_keys_step_right_action;
menuaction_s		s_keys_sidestep_action;
menuaction_s		s_keys_look_up_action;
menuaction_s		s_keys_look_down_action;
menuaction_s		s_keys_center_view_action;
menuaction_s		s_keys_mouse_look_action;
menuaction_s		s_keys_keyboard_look_action;
menuaction_s		s_keys_move_up_action;
menuaction_s		s_keys_move_down_action;
menuaction_s		s_keys_flashlight_action;
menuaction_s		s_keys_zoom_action;
menuaction_s		s_keys_inventory_action;
menuaction_s		s_keys_inv_use_action;
menuaction_s		s_keys_inv_drop_action;
menuaction_s		s_keys_inv_prev_action;
menuaction_s		s_keys_inv_next_action;
menuaction_s		s_keys_screenshot_action;

menuframework_s	s_keys2_menu;
menuaction_s		s_keys2_blaster_action;
menuaction_s		s_keys2_shotgun_action;
menuaction_s		s_keys2_sshotgun_action;
menuaction_s		s_keys2_machinegun_action;
menuaction_s		s_keys2_chaingun_action;
menuaction_s		s_keys2_grenades_action;
menuaction_s		s_keys2_glauncher_action;
menuaction_s		s_keys2_rlauncher_action;
menuaction_s		s_keys2_hyperblaster_action;
menuaction_s		s_keys2_railgun_action;
menuaction_s		s_keys2_bfg10k_action;
menuaction_s		s_keys2_weapprev_action;
menuaction_s		s_keys2_weapnext_action;
menuaction_s		s_keys2_quaddamage_action;
menuaction_s		s_keys2_invulnerability_action;
menuaction_s		s_keys2_rebreather_action;
menuaction_s		s_keys2_envsuit_action;
menuaction_s		s_keys2_silencer_action;
menuaction_s		s_keys2_powershield_action;
menuaction_s		s_keys2_invisibility_action;
menuaction_s		s_keys2_messagemode_action;
menuaction_s		s_keys2_qsaving_action;
menuaction_s		s_keys2_qloading_action;
menuaction_s		s_keys2_quit_action;


menulist_s			s_mode_list;
menulist_s			s_filter_list;
menulist_s			s_solid_list;
menulist_s			s_alpha_list;
menulist_s			s_shader_list;
menulist_s			s_simple;
///menuslider_s		s_screensize_slider;
menuframework_s		s_opengl_menu;
menuslider_s		s_gamma_slider;
menuslider_s		s_bright_slider;
menuslider_s		s_contrast_slider;
menulist_s  		s_fs_box;
///menuaction_s		s_cancel_action;
menuslider_s		s_tq_slider;
menuslider_s		s_lmscale_slider;
menulist_s			s_bmscale_slider;
menulist_s			s_finish_box;
menulist_s			s_nvhint_box;
menulist_s			s_compression_box;
menulist_s			s_bump_compression_box;
menuslider_s		s_anisotropy_slider;
menuslider_s		s_antialias_slider;
menuaction_s		s_video_options_action;
menuaction_s		s_screen_infos_action;


///menuslider_s		s_brightness_slider_;
///menuslider_s		s_anisotropy_slider_;
///menuslider_s		s_antialias_slider_;
///menulist_s  		s_fs_box_;

menuframework_s		s_gameoptions_menu;
menuslider_s		s_flyqbe_slider;
menuslider_s		s_predator_slider;
menuslider_s		s_monsterRespawn;
menulist_s			s_fixoverflow;
menulist_s			s_footsteps;
menuseparator_s		s_knockback_title;
menufield_s			s_knockback_field;
menuseparator_s		s_gibs_title;
menufield_s			s_gibs_field;
menuseparator_s		s_distance_title;
menufield_s			s_distance_field;
menulist_s			s_r_brassTimeScale;
menulist_s			s_r_debrisTimeScale;
menulist_s			s_bloodLifeTime;
menulist_s			s_bulletMarkLifeTime;
menulist_s			s_laserMarkLifeTime;
menulist_s			s_explosionMarkLifeTime;
menulist_s			s_railMarkLifeTime;
menulist_s			s_bfgMarkLifeTime;


menuframework_s		s_scrinfo_menu;
menulist_s			s_3dhud;
menulist_s			s_bighud;
menulist_s			s_drawclock;
menulist_s			s_fps;
menulist_s			s_viewpos;
menulist_s			s_mapshots;
menulist_s			s_showtexture;
menulist_s			s_gametime;
menulist_s			s_drawspeed;

menuframework_s		s_mirroroptions_menu;
menulist_s			s_r_mirrors;
menulist_s			s_r_mirror_frameskip;
menulist_s			s_r_mirror_size;
menulist_s			s_r_mirror_models;
menuseparator_s		s_r_mirror_distance_title;
menufield_s			s_r_mirror_distance_field;
menulist_s			s_r_bloom;
menulist_s			s_r_nolights;
menulist_s			s_r_drawsun;
menulist_s			s_r_decals;
menulist_s			s_r_caustics;
menulist_s			s_r_flares;
menulist_s			s_r_noshells;
menulist_s			s_r_shadows;
menulist_s			s_playershadow;
menulist_s			s_dl_shadows;
menulist_s			s_r_mirror_shadows;
menulist_s			s_r_teshadows;
menulist_s			s_r_personal_gibs;



bool no_jmp = false;		// if TRUE - вместо ERR_DROP/DISCONNECT применить ERR_FATAL.
							// it is patch Q2 error: если возникает ошибка ERR_DROP/DISCONNECT
							// во время выполнения MainWndProc, то longjmp срабатывает некорректно!!!

/*
const char *texdepth[] =
{
	"[16-bit]",
	"[32-bit]",
	0
};
*/

const char *coreColorNames[] =
{
	" -",
	"blue",
	"green",
	"cian",
	"red",
	"purp.",
	"yell.",
	"white",
	0
};

const char *spiralColorNames[] =
{
	"rand.",
	"blue",
	"green",
	"cian",
	"red",
	"purp.",
	"yell.",
	"white",
	0
};


const char *handedness[] =
{
	"right",
	"left",
	"center",
	0
};

const char *sun_names[] =
{
	"disabled",
	"with no flares",
	"with flares",
	0
};

const char *bloom_names[] =
{
	"disable",
	"easy",
	"middle",
	"strong",
	0
};

const char *MirrorSize_names[] =
{
	"low",
	"middle",
	"full",
	0
};

const char *MirrorRefresh_names[] =
{
	"60 fps",
	"55 fps",
	"50 fps",
	"45 fps",
	"40 fps",
	"35 fps",
	"30 fps",
	"25 fps",
	"20 fps",
	"15 fps",
	"every frame",
	"skip 1 frame",
	"skip 2 frames",
	"skip 4 frames",
	0
};

const char *ShadowsMirror_names[] =
{
	"BSP only",
	"BSP and brushes",
	"all",
	0
};

const char *Shadows_names[] =
{
	"disabled",
	"BSP only",
	"all",
	0
};

const char *EnableDisable_names[] =
{
	"disabled",
	"enabled",
	0
};

/*
const char *showtexture_names[] =
{
	"no",
	"Name,Color,Normal",
	"Name,Color,Normal,Mtr,ColorMap",
	"Name,Color,Normal,FX,FxMap",
	0
};
*/

const char *yesno_names[] =
{
	"no",
	"yes",
	0
};

const char *fps_names[] =
{
	"disabled",
	"normal",
	"extra",
	0
};

const char *vsync_names[] =
{
	"no",
	"yes",
	"adaptive",
	0
};

const char *hint_names[] =
{
	"fastest",
	"nicest",
	0
};

const char *Clock_names[] =
{
	"off",
	"24 hours",
	"12 hours",
	0
};

const char *lifetime_names[] =
{
	"Very Quickly",
	"Quickly",
	"Normally",
	"Long",
	"Very long",
	"Eternally",
	0
};

const char *lifetime_names2[] =
{
	"-",
	"Quickly",
	"Normally",
	"Long",
	"Very long",
	"Eternally",
	0
};

const char *MusicTrack_names[] =
{
	"disabled",
	"Tracks only",
	"Situations tracks",
	0
};

const char *onoff_names[] =
{
	"off",
	"on",
	0
};


const char *compr_types[] =
{
	"off",
	"DXT1",
	"DXT3",
	"DXT5",
	"ARB",
	0
};


const char *quality_items[] =
{
	"low",
	"medium",
	"high",
	"highest",
	0
};


const char *dm_coop_names[] =
{
	"d.match",
	"coop",
	0
};


const char *skill_names[] =
{
	"  easy",
	" medium",
	"  hard",
	"nightmare",
	0
};


#define	MAX_CROSSHAIR	9			// Не больше 9 !!! Иначе код поменять...
const char *crosshair_names[] =
{
	"none",
	"dot",
	"dot in circle",
	"dot in circle2",
	"dot in filled circle",
	"angle",
	"cross",
	"cross2",
	"cross in circle",
	"dot in cross",
	0
};


const char *filters[] =
{
	"GL_NEAREST",
	"GL_LINEAR",
	"GL_NEAREST_MIPMAP_NEAREST",
	"GL_LINEAR_MIPMAP_NEAREST",
	"GL_NEAREST_MIPMAP_LINEAR",
	"GL_LINEAR_MIPMAP_LINEAR",
	0
};


///////////////////////////////////////
#define MAX_SHADERS		7

typedef enum
{
	SHADER_GENERIC4 = 0,
	SHADER_GENERIC6,
	SHADER_NVIDIA4,
	SHADER_ATI4,
	SHADER_ATI6,
	SHADER_ARB4,
	SHADER_ARB6
} shadertype;

const char *shaders[] =
{
	"Generic, 4 TMUs",
	"Generic, 6 TMUs",
	"GeForce, 4 TMUs",
	"Radeon, 4 TMUs (fast)",
	"Radeon, 6 TMUs",
	"ARBfp1.0, 4 TMUs (fast)",
	"ARBfp1.0, 6 TMUs",
	0
};
///////////////////////////////////////

bool	need_gun_test;				// false если дисторт не работает
bool	need_gun_optimisation;		// true - если рядом оказался distort-объект
int		gun_pass;					// 1 - отдельный спец.прогон для оружия

trace_t	trace_trace;
int		trace_contents;
///int		c_traces, c_brush_traces;
vec3_t	trace_start, trace_end;
vec3_t	trace_mins, trace_maxs;
bool	trace_ispoint;		// optimized case
vec3_t	trace_extents;

areanode_t	sv_areanodes[AREA_NODES];
int			sv_numareanodes;
int			area_type;

float	*area_mins, *area_maxs;
int		area_count, area_maxcount;
edict_t	**area_list;

int		c_pointcontents;


int			checkcount;

char		map_name[MAX_QPATH];

int			numbrushsides;
cbrushside_t map_brushsides[MAX_MAP_BRUSHSIDES];

int			numtexinfo;
mapsurface_t	map_surfaces[MAX_MAP_TEXINFO];

int			numplanes;
cplane_t	map_planes[MAX_MAP_PLANES+6];		// extra for box hull

int			numnodes;
cnode_t		map_nodes[MAX_MAP_NODES+6];		// extra for box hull

int			numleafs = 1;	// allow leaf funcs to be called without a map
cleaf_t		map_leafs[MAX_MAP_LEAFS];
int			emptyleaf, solidleaf;

int			numleafbrushes;
WORD		map_leafbrushes[MAX_MAP_LEAFBRUSHES];

int			numcmodels;
cmodel_t	map_cmodels[MAX_MAP_MODELS];

int			numbrushes;
cbrush_t	map_brushes[MAX_MAP_BRUSHES];

int			numvisibility;
byte		map_visibility[MAX_MAP_VISIBILITY];
dvis_t		*map_vis = (dvis_t *)map_visibility;

int			numentitychars;
char		map_entitystring[MAX_MAP_ENTSTRING];

int			numareas = 1;
carea_t		map_areas[MAX_MAP_AREAS];

int			numareaportals;
dareaportal_t map_areaportals[MAX_MAP_AREAPORTALS];

int			numclusters = 1;

mapsurface_t	nullsurface;

int			floodvalid;

bool	portalopen[MAX_MAP_AREAPORTALS];

///bool	areaportals_from_server = false;

#define NUMVERTEXNORMALS	162

float	r_avertexnormals[NUMVERTEXNORMALS][3] =
{
#include "anorms.h"
};

#define TURBSCALE (256.0 / (2 * M_PI))
// speed up sin calculations - Ed
float	r_turbsin[] =
{
	#include "warpsin.h"
};


typedef struct
{
	int		nskins;
	char	**skindisplaynames;
	char	displayname[MAX_DISPLAYNAME];
	char	directory[MAX_QPATH];
} playermodelinfo_s;

playermodelinfo_s s_pmi[MAX_PLAYERMODELS];


vec3_t			pointcolor;
///cplane_t		*lightplane;		// used as shadow plane
///vec3_t			lightspot;

cplane_t	*box_planes;
int			box_headnode;
cbrush_t	*box_brush;
cleaf_t		*box_leaf;

pmove_t		*pm;
pml_t		pml;

// movement parameters
float	pm_stopspeed = 100;
float	pm_maxspeed = 300;
float	pm_duckspeed = 100;
float	pm_accelerate = 10;
float	pm_airaccelerate = 0;
float	pm_wateraccelerate = 10;
float	pm_friction = 6;
float	pm_waterfriction = 1;
float	pm_waterspeed = 400;

cparticle_t	*active_particles, *free_particles;
cparticle_t	particles[MAX_PARTICLES];

clentity_t	*active_clentities, *free_clentities;
clentity_t	clentities[MAX_CLENTITIES];

explosion_t	cl_explosions[MAX_EXPLOSIONS];
decal_t		decals[MAX_DECALS];
decal_t		*active_decals, *free_decals;

bool		decals_needs_spawn;
int			num_deferred_decals;
defDecal_t	deferred_decals[MAX_DECALS+1];

sfx_t	*cl_sfx_lava;
sfx_t	*cl_sfx_watershell;
sfx_t	*cl_sfx_mbrass;
sfx_t	*cl_sfx_sbrass;
sfx_t	*cl_sfx_debris;
sfx_t	*cl_sfx_debris_glass;
sfx_t	*cl_sfx_debris_metal;
sfx_t	*cl_sfx_debris_flesh;
sfx_t	*cl_sfx_ric1;
sfx_t	*cl_sfx_ric2;
sfx_t	*cl_sfx_ric3;
sfx_t	*cl_sfx_lashit;
sfx_t	*cl_sfx_spark5;
sfx_t	*cl_sfx_spark6;
sfx_t	*cl_sfx_spark7;
sfx_t	*cl_sfx_railg;
sfx_t	*cl_sfx_rockexp;
sfx_t	*cl_sfx_grenexp;
sfx_t	*cl_sfx_watrexp;


model_t	*cl_mod_explode;		// Модель вспышки бластера (когда он попадает в стену)
model_t	*cl_mod_parasite_segment;
model_t	*cl_mod_flash;
model_t	*cl_mod_grapple_cable;
model_t	*cl_mod_bfg_explo;
model_t	*cl_mod_powerscreen;
model_t	*cl_mod_explo4;
model_t	*cl_mod_smoke;				// not used anymore?
model_t	*cl_mod_parasite_tip;		// not used anymore?
model_t	*cl_mod_exp;
model_t	*cl_mod_rlexp;
//ROGUE
sfx_t	*cl_sfx_lightning;
sfx_t	*cl_sfx_disrexp;
model_t	*cl_mod_lightning;
model_t	*cl_mod_heatbeam;
model_t	*cl_mod_monster_heatbeam;
model_t	*cl_mod_explo4_big;
//Berserker
model_t *cl_mod_laserbolt;
model_t *cl_mod_beam;
model_t	*cl_mod_mshell;
model_t	*cl_mod_sshell;
model_t	*cl_mod_debris1;
model_t	*cl_mod_debris2;
model_t	*cl_mod_debris3;
model_t	*cl_mod_debris1_glass;
model_t	*cl_mod_debris2_glass;
model_t	*cl_mod_debris3_glass;
model_t	*cl_mod_head2;
model_t	*cl_mod_chest;
model_t	*cl_mod_meat;
model_t	*cl_mod_bone;
model_t	*cl_mod_gear;
model_t	*cl_mod_metal;
model_t	*cl_mod_arm;
model_t	*cl_mod_leg;
model_t	*cl_mod_head;
model_t	*cl_mod_vbfg[2];
model_t	*cl_mod_vblast[2];
model_t	*cl_mod_vchain[2];
model_t	*cl_mod_vhandgr[2];
model_t	*cl_mod_vhyperb[2];
model_t	*cl_mod_vlaunch[2];
model_t	*cl_mod_vmachn[2];
model_t	*cl_mod_vrail[2];
model_t	*cl_mod_vrocket[2];
model_t	*cl_mod_vshotg[2];
model_t	*cl_mod_vshotg2[2];

model_t	*cl_disguise_model;
model_t *cl_disguise_skin;
model_t *cl_disguise_bump;
model_t *cl_disguise_light;

model_t	*cl_mod_gladiator_g1;
model_t	*cl_mod_gladiator_g2;
model_t	*cl_mod_gladiator_g3;
model_t	*cl_mod_gladiator_g4;
model_t	*cl_mod_gladiator_g5;
model_t	*cl_mod_gladiator_g6;
model_t	*cl_mod_gladiator_g7;

model_t	*cl_mod_infantry_g1;
model_t	*cl_mod_infantry_g2;
model_t	*cl_mod_infantry_g3;
model_t	*cl_mod_infantry_g4;
model_t	*cl_mod_infantry_g5;
model_t	*cl_mod_infantry_g6;
model_t	*cl_mod_infantry_g7;

model_t	*cl_mod_flipper_g1;
model_t	*cl_mod_flipper_g2;
model_t	*cl_mod_flipper_g3;
model_t	*cl_mod_flipper_g4;

model_t	*cl_mod_mutant_g1;
model_t	*cl_mod_mutant_g2;
model_t	*cl_mod_mutant_g3;
model_t	*cl_mod_mutant_g4;
model_t	*cl_mod_mutant_g5;
model_t	*cl_mod_mutant_g6;

model_t	*cl_mod_hover_g1;
model_t	*cl_mod_hover_g2;
model_t	*cl_mod_hover_g3;
model_t	*cl_mod_hover_g4;
model_t	*cl_mod_hover_g5;
model_t	*cl_mod_hover_g6;

model_t	*cl_mod_gunner_g1;
model_t	*cl_mod_gunner_g2;
model_t	*cl_mod_gunner_g3;
model_t	*cl_mod_gunner_g4;
model_t	*cl_mod_gunner_g5;
model_t	*cl_mod_gunner_g6;
model_t	*cl_mod_gunner_g7;
model_t	*cl_mod_gunner_g8;
model_t	*cl_mod_gunner_g9;

model_t	*cl_mod_floater_g1;
model_t	*cl_mod_floater_g2;
model_t	*cl_mod_floater_g3;
model_t	*cl_mod_floater_g4;
model_t	*cl_mod_floater_g5;
model_t	*cl_mod_floater_g6;

model_t	*cl_mod_flyer_g1;
model_t	*cl_mod_flyer_g2;
model_t	*cl_mod_flyer_g3;
model_t	*cl_mod_flyer_g4;

model_t	*cl_mod_soldier_g1;
model_t	*cl_mod_soldier_g2;
model_t	*cl_mod_soldier_g3;
model_t	*cl_mod_soldier_g4;
model_t	*cl_mod_soldier_g5;
model_t	*cl_mod_soldier_g6;
model_t	*cl_mod_soldier_g7;
model_t	*cl_mod_soldier_g8;
model_t	*cl_mod_soldier_g9;

model_t	*cl_mod_tank_g1;
model_t	*cl_mod_tank_g2;
model_t	*cl_mod_tank_g3;
model_t	*cl_mod_tank_g4;
model_t	*cl_mod_tank_g5;
model_t	*cl_mod_tank_g6;
model_t	*cl_mod_tank_g7;
model_t	*cl_mod_tank_g8;
model_t	*cl_mod_tank_g9;

model_t	*cl_mod_insane_g1;
model_t	*cl_mod_insane_g2;
model_t	*cl_mod_insane_g3;
model_t	*cl_mod_insane_g4;
model_t	*cl_mod_insane_g5;
model_t	*cl_mod_insane_g6;
model_t	*cl_mod_insane_g7;
model_t	*cl_mod_insane_g8;

model_t	*cl_mod_barrel_g1;
model_t	*cl_mod_barrel_g2;
model_t	*cl_mod_barrel_g3;
model_t	*cl_mod_barrel_g4;

model_t	*cl_mod_male_g1;
model_t	*cl_mod_male_g2;
model_t	*cl_mod_male_g3;
model_t	*cl_mod_male_g4;
model_t	*cl_mod_male_g5;
model_t	*cl_mod_male_g6;
model_t	*cl_mod_male_g7;
model_t	*cl_mod_male_g8;

model_t	*cl_mod_female_g1;
model_t	*cl_mod_female_g2;
model_t	*cl_mod_female_g3;
model_t	*cl_mod_female_g4;
model_t	*cl_mod_female_g5;
model_t	*cl_mod_female_g6;
model_t	*cl_mod_female_g7;
model_t	*cl_mod_female_g8;
model_t	*cl_mod_female_g9;

model_t	*cl_mod_cyborg_g1;
model_t	*cl_mod_cyborg_g2;
model_t	*cl_mod_cyborg_g3;
model_t	*cl_mod_cyborg_g4;
model_t	*cl_mod_cyborg_g5;
model_t	*cl_mod_cyborg_g6;
model_t	*cl_mod_cyborg_g7;
model_t	*cl_mod_cyborg_g8;
model_t	*cl_mod_cyborg_g9;
model_t	*cl_mod_cyborg_g10;

model_t	*cl_mod_brain_g1;
model_t	*cl_mod_brain_g2;
model_t	*cl_mod_brain_g3;
model_t	*cl_mod_brain_g4;
model_t	*cl_mod_brain_g5;
model_t	*cl_mod_brain_g6;
model_t	*cl_mod_brain_g7;
model_t	*cl_mod_brain_g8;
model_t	*cl_mod_brain_g9;
model_t	*cl_mod_brain_g10;

model_t	*cl_mod_parasite_g1;
model_t	*cl_mod_parasite_g2;
model_t	*cl_mod_parasite_g3;
model_t	*cl_mod_parasite_g4;
model_t	*cl_mod_parasite_g5;
model_t	*cl_mod_parasite_g6;
model_t	*cl_mod_parasite_g7;

model_t	*cl_mod_medic_g1;
model_t	*cl_mod_medic_g2;
model_t	*cl_mod_medic_g3;
model_t	*cl_mod_medic_g4;
model_t	*cl_mod_medic_g5;
model_t	*cl_mod_medic_g6;
model_t	*cl_mod_medic_g7;
model_t	*cl_mod_medic_g8;
model_t	*cl_mod_medic_g9;
model_t	*cl_mod_medic_g10;

model_t	*cl_mod_berserk_g1;
model_t	*cl_mod_berserk_g2;
model_t	*cl_mod_berserk_g3;
model_t	*cl_mod_berserk_g4;
model_t	*cl_mod_berserk_g5;
model_t	*cl_mod_berserk_g6;
model_t	*cl_mod_berserk_g7;
model_t	*cl_mod_berserk_g8;

model_t	*cl_mod_chick_g1;
model_t	*cl_mod_chick_g2;
model_t	*cl_mod_chick_g3;
model_t	*cl_mod_chick_g4;
model_t	*cl_mod_chick_g5;
model_t	*cl_mod_chick_g6;
model_t	*cl_mod_chick_g7;
model_t	*cl_mod_chick_g8;
model_t	*cl_mod_chick_g9;

model_t	*cl_mod_jorg_g1;
model_t	*cl_mod_jorg_g2;
model_t	*cl_mod_jorg_g3;
model_t	*cl_mod_jorg_g4;
model_t	*cl_mod_jorg_g5;
model_t	*cl_mod_jorg_g6;
model_t	*cl_mod_jorg_g7;
model_t	*cl_mod_jorg_g8;
model_t	*cl_mod_jorg_g9;
model_t	*cl_mod_jorg_g10;
model_t	*cl_mod_jorg_g11;

model_t	*cl_mod_boss2_g1;
model_t	*cl_mod_boss2_g2;
model_t	*cl_mod_boss2_g3;
model_t	*cl_mod_boss2_g4;
model_t	*cl_mod_boss2_g5;
model_t	*cl_mod_boss2_g6;
model_t	*cl_mod_boss2_g7;
model_t	*cl_mod_boss2_g8;
model_t	*cl_mod_boss2_g9;
model_t	*cl_mod_boss2_g10;
model_t	*cl_mod_boss2_g11;
model_t	*cl_mod_boss2_g12;
model_t	*cl_mod_boss2_g13;

model_t	*cl_mod_supertank_g1;
model_t	*cl_mod_supertank_g2;
model_t	*cl_mod_supertank_g3;
model_t	*cl_mod_supertank_g4;
model_t	*cl_mod_supertank_g5;
model_t	*cl_mod_supertank_g6;
model_t	*cl_mod_supertank_g7;


laser_t		cl_lasers[MAX_LASERS];
beam_t		cl_beams[MAX_BEAMS];
//PMM - added this for player-linked beams.  Currently only used by the plasma beam
beam_t		cl_playerbeams[MAX_BEAMS];

cheatvar_t	cheatvars[] = {
	{"timescale", "1"},
	{"timedemo", "0"},
///	{"r_fullbright", "0"},
	{"paused", "0"},
	{"fixedtime", "0"},
	{"r_showtris", "0"},
	{"r_showtexture", "0"},
	{NULL, NULL}
};

unsigned	c_brush_polys, c_alias_polys, c_light_brush_polys, c_light_alias_polys, c_mirrors, c_lights, c_no_light_ents, c_no_lights, c_world_batches, c_world_batches_lighted, c_no_mirrors;
unsigned	oc_light_brush_polys, oc_light_alias_polys, oc_lights, oc_no_light_ents, oc_no_lights, oc_no_mirrors;


byte	d_8to24table[768];
bool	q2_palette_initialized = false;

int		numcheatvars;

int		r_viewcluster, r_viewcluster2, r_oldviewcluster, r_oldviewcluster2;

model_t	mod_known[MAX_MOD_KNOWN];
int		mod_numknown;

model_t	*r_worldmodel;

// the inline * models from the current map are kept seperate
model_t	mod_inline[MAX_MOD_KNOWN];

model_t	*loadmodel;
int		modfilelen;

byte	*membase;
int		hunkmaxsize = 0;
int		cursize;
char	hunk_name[MAX_OSPATH];
int		hunkcount;
int		hunk_total_size = 0;

byte	*cmod_base;
byte	*mod_base;

model_t		*currentmodel;

msurface_t	*warpface;

int		num_cl_weaponmodels;
char	cl_weaponmodels[MAX_CLIENTWEAPONMODELS][MAX_QPATH];

///vec3_t	shadevector;
float	shadelight[3];

int precache_check; // for autodownload of precache items
int precache_spawncount;
int precache_tex;
int precache_model_skin;
byte *precache_model; // used for skin checking in alias models

int				gun_frame;
model_t	*gun_model;


///int	r_dlightframecount;

int			r_numdlights;
dlight_t	r_dlights[MAX_DLIGHTS];


int			r_numentities;
entity_t	r_entities[MAX_ENTITIES];
GLuint		r_entities_occID[MAX_ENTITIES];
int			r_entities_occUSE[MAX_ENTITIES];
occResult_t	occResults[MAX_ENTITIES];



int			r_numparticles;
particle_t	r_particles[MAX_PARTICLES];

static vec3_t avelocities [NUMVERTEXNORMALS];

static char *s_pmnames[MAX_PLAYERMODELS];
static int s_numplayermodels;

sfx_t	*cl_sfx_footsteps[4];

cdlight_t		cl_dlights[MAX_DLIGHTS];

int			lastofs;

unsigned	iFastSqrtTable[0x10000];

int		trickframe;

byte	decompressed[MAX_MAP_LEAFS/8];

dplane_t leftPlane, rightPlane, bottomPlane, topPlane, backPlane, frontPlane;

unsigned	fragment_shaders = 0xffffffff;
unsigned	fragment_program_nofilter = 0xffffffff;
unsigned	fragment_program_filter = 0xffffffff;
unsigned	fragment_program_filter4 = 0xffffffff;
unsigned	arbdistort_program = 0xffffffff;
unsigned	fragment_program_ambient = 0xffffffff;
unsigned	fragment_program_nofilter6_detail = 0xffffffff;
unsigned	fragment_program_filter6_detail = 0xffffffff;
unsigned	vertex_program_nolightfilter = 0xffffffff;
unsigned	vertex_program_lightfilter6 = 0xffffffff;
unsigned	vertex_program_lightfilter4 = 0xffffffff;
unsigned	vertex_program_lightfilter6_parallax = 0xffffffff;
unsigned	vertex_program_nolightfilter_parallax = 0xffffffff;
unsigned	fragment_program_filter_parallax = 0xffffffff;
unsigned	fragment_program_nofilter_parallax = 0xffffffff;
unsigned	vertex_program_ambient_parallax = 0xffffffff;
unsigned	fragment_program_ambient_parallax = 0xffffffff;
unsigned	vertex_program_nobump_parallax = 0xffffffff;
unsigned	fragment_program_nobump_parallax = 0xffffffff;



shadowlight_t	shadowlights[MAXSHADOWLIGHTS];
shadowlight_t	*usedshadowlights[MAXUSEDSHADOWLIGHS];
int				numStaticShadowLights;		// Общее количество статичных источников света
int				numShadowLights;		// Общее количество ВСЕХ источников света (статичных + динамичных)
int				numUsedShadowLights;		// Общее количество ВСЕХ источников света (статичных + динамичных), которые попали в кадр и отбрасывают тени
shadowlight_t	*currentshadowlight;
byte			viewvis[MAX_MAP_LEAFS/8];
///int				viewarea;
byte			viewvis__[MAX_MAP_LEAFS/8];
screenrect_t	*recList;					//first rectangle of the list
screenrect_t	totalRect;					//rectangle that holds all rectangles in the list
///msurface_t		*shadowchain;				//linked list of polygons that are shadowed
int				r_lightTimestamp;
int				numVolumeCmds;
int				numLightCmds;
lightcmd_t		volumeCmdsBuff[MAX_VOLUME_COMMANDS+128]; //Hack protect against slight overflows
lightcmd_t		lightCmdsBuff[MAX_LIGHT_COMMANDS+128];
vec3_t			vbo_shadow[MAX_VBO_XYZs];
///int				svBsp_NumKeptPolys;
svnode_t		*currentlightroot;
dplane_t		PlanePool[MAX_MAP_PLANES*3];
int				planesusedpool;
svnode_t		NodePool[MAX_MAP_NODES*3];
int				nodesusedpool;
int				cl_numlightvisedicts;
entity_t		*cl_lightvisedicts[MAX_ENTITIES];
temp_connect_t	*tempEdges;
vec3_t			extrudedVerts[MD3_MAX_VERTS];
vec3_t			trinormals[MD3_MAX_TRIANGLES];
byte			viss[MD3_MAX_TRIANGLES];

int				TrisCounter;
float			TrisVerts[36 * MD3_MAX_TRIANGLES];

#define	MAX_BATCH_SURFS		21845				// (floor)65536/3
vec3_t			wVertexArray[MAX_BATCH_SURFS];
float			wTexArray[MAX_BATCH_SURFS][2];
float			wLMArray[MAX_BATCH_SURFS][2];
vec3_t			nTexArray[MAX_BATCH_SURFS];
vec3_t			tTexArray[MAX_BATCH_SURFS];
vec3_t			bTexArray[MAX_BATCH_SURFS];
float			EnvArray[MAX_BATCH_SURFS][2];
vec3_t			EnvArray3[MAX_BATCH_SURFS];
WORD			indexArray[MAX_BATCH_SURFS*3];
bool	batch_env;
bool	batch_power;
bool	batch_map;
bool	batch_style;
bool	batch_used_fx;
float	batch_fx_s;
float	batch_fx_t;
float	batch_fx_scale_s;
float	batch_fx_scale_t;

vec3_t			sun_origin;
float			sun_size;
float			sun_x, sun_y;///, sun_z;
bool			draw_sun, mir_draw_sun;
float			last_draw_sun;
float			last_draw_sun_mirror;
int				sun_time = 0;
int				sun_time_mirror = 0;

vec3_t	g_color_table[8] =
	{
	{0.2, 0.2, 0.2},
	{1.0, 0.2, 0.2},
	{0.2, 1.0, 0.2},
	{1.0, 1.0, 0.2},
	{0.3, 0.3, 1.0},
	{0.3, 1.0, 1.0},
	{1.0, 0.3, 1.0},
	{1.0, 1.0, 1.0},
	};

float			lastframetime;
int				fps_count;
int				lastfps;
unsigned		fps;
unsigned		fps_noshow = 0;
unsigned		fps_nocalc = 0;
bool			fps_refreshed = false;

vec3_t			vcache[MAX_MAP_TEXINFO * MAX_POLY_VERT];
unsigned		icache[MAX_MAP_TEXINFO * MAX_POLY_VERT];

shadowlight_t	m_light;

cinematics_t	cin;

int				sshot_counter = 0;

int				numEmits;
emit_t			emits[MAX_EMITS];

BYTE			**img_rowptr;
BYTE			*img_bmpbits;

vec3_t			dec_normal, dec_normal_bak, dec_origin, dec_origin_bak;
vec3_t			dec_tangent, dec_binormal;
float			dec_radius, dec_half_radius;

int				numFogs;
fog_t			fog_infos[MAX_FOG_AREAS];

Uint16			gammaRamp[256*3];
byte			intensitytable[256];
byte			gammatable[256];

int				gl_edge_type;

int				sys_time;
int				cmd_wait;

float	max_speed, extraspeed;
float	display_speed;
int		last_sys_time, prev_sys_time;
vec3_t	old_orgs;
vec3_t	cur_orgs;
bool	last_cvar_drawspeed = true;
bool	weaponmodel_lighted;
vec3_t	shell_color;

vec3_t	tempVertexArray[MD3_MAX_VERTS];


alink_t			amdl_list [MAX_ENTITIES * 16];
blink_t			bmdl_list [MAX_ENTITIES];
llink_t			lmdl_list [MAX_ENTITIES * 16];
int				aliasmodel_counter;
int				brushmodel_counter;
int				lightmodel_counter;


byte	r_q2pal[] =
{
	#include "q2pal.h"
};

vec3_t		world_ambient_light;

float		texcoordArray_global[6 * MAX_PARTICLES];
float		texcoordArray_global2[6 * MAX_PARTICLES];


#define		ZHEADERLEN	2
#define		ZBUFFLEN	2048
byte		compressed_frame[ZBUFFLEN+ZHEADERLEN];
byte		buff_out[ZBUFFLEN];

unsigned	net_saved_in = 0;
unsigned	net_saved_out = 0;
unsigned	net_incoming = 0;
unsigned	net_outgoing = 0;
unsigned	znet_incoming = 0;
unsigned	znet_outgoing = 0;


char		mapname[32];
char		mapshot[44];	/// 32 + 11 байт на "levelshots/" + NULL
unsigned	loading_stage;
char		*loading_string;


GLfloat s___[4] = {1.0, 0.0, 0.0, 0.0};
GLfloat t___[4] = {0.0, 1.0, 0.0, 0.0};
GLfloat r___[4] = {0.0, 0.0, 1.0, 0.0};

float		args_distort[4] = {0.05f,0,0,0.04f};


bool			le_mode = false;	// true - linked, false - static lights
shadowlight_t	*curlight = NULL;
llink_t			*curlink = NULL;
shadowlight_t	light_clipboard;
llink_t			link_clipboard;
emit_t			emit_clipboard;
defDecal_t		decal_clipboard;
alink_t			model_clipboard;
mmodel_t		*curbrush = NULL;
vec3_t			curbrush_offset;
int				curbrushnum = 0;
emit_t			*curemit = NULL;
defDecal_t		*curdecal = NULL;
alink_t			*curmodel = NULL;
fog_t			*curfog = NULL;
bool			trace_emit = false;
bool			trace_model = false;
bool			trace_decal = false;
bool			silent_decal = false;
bool			silent_model = false;
bool			trace_light = false;
mtexinfo_t		*curtexinfo = NULL;

int				curmesh;
int				curtri;

bool			distort_initialised;

int				num_flares;
flare_t			flares[MAX_FLARES];

bool			hud_player_test;
float			hud_player_scale;
bool			hud_player_exist;
int				hud_player_framecount;
WORD			hud_player_origin[2];
model_t			*hud_player_model;
image_t			*hud_player_skin;
image_t			*hud_player_bump;
image_t			*hud_player_light;

bool			hud_model_test[MAX_IMAGES_BERS];
bool			hud_model_exist[MAX_IMAGES_BERS];
int				hud_model_framecount[MAX_IMAGES_BERS];
BYTE			hud_model_counter[MAX_IMAGES_BERS];
WORD			hud_model_origin[MAX_IMAGES_BERS][16][2];	// до 16 одновременных показов в хаде одной модели
float			hud_model_scale[MAX_IMAGES_BERS][16];
model_t			*hud_models[MAX_IMAGES_BERS];
image_t			*hud_skins[MAX_IMAGES_BERS];
image_t			*hud_bumps[MAX_IMAGES_BERS];
image_t			*hud_lights[MAX_IMAGES_BERS];

float			smooth_cosine;


model_t*	old_wmdl;
///float		old_r_finish;
int			old_fps_count;
int			old_r_framecount, old_occ_framecount;
vec3_t		old_vpn, old_vright, old_vup;
vec3_t		old_r_origin;
cplane_t	old_frustum[4];




bool		r_mirror;
mirror_t	mirrors[MAX_MIRRORS];
mirror_t	*mirror_plane;
///cplane_t	*mirror_plane;
///cplane_t	mirror_far_plane;
int			mirror_clipside;
float		inv_mirror_distance;
float		blend_mirror;
float		blend_mirror1;


char		old_skin[MAX_QPATH+1];


fs_cache_t	fs_cache[MAX_FILES];
int			fs_cache_number = 0;
int			last_zip_number;

bool		clock_started = false;
int			start_clock;

bool		light_used, shadow_used;

float		(*Sqrt)(float);
///float		(*Sin)(float);

unsigned	hashes[MAX_SPEC_TEXTURES];
unsigned	hashLaserBolt[3];

char		lightstyles[MAX_LIGHTSTYLES_OVERRIDE][MAX_QPATH];
char		cl_lightstyles[MAX_LIGHTSTYLES][MAX_QPATH];

bool		caching__, caching_calc;

bool		ignore_cvars = false;

////bool		r_using_underwater = false;
////bool		r_using_breather = false;
////bool		r_using_enviro = false;
int			r_using_burning = 0;
int			r_using_pain = 0;
int			r_using_pain_frame;
int			r_using_drown = 0;


bool						vc_initialised = false;
bool						need_free_vbo;
static vertCacheManager_t	vcm;


#define OLD_TRACK_DISABLED	0
#define OLD_TRACK_UNKNOWN	1
#define OLD_TRACK_RELAX		2
#define OLD_TRACK_COMBAT	3
char	TrackIsCombat_old;
char	trackName_Relax[MAX_OSPATH];
char	trackName_RelaxToCombat[MAX_OSPATH];
char	trackName_Combat[MAX_OSPATH];
char	trackName_CombatToRelax[MAX_OSPATH];
unsigned	trackRelax_offset;
unsigned	trackCombat_offset;


void Mod_LoadSubmodels (lump_t *l);
void CalcSurfaceExtents (msurface_t *s);
void GL_BeginBuildingLightmaps ();
void GL_CreateSurfaceLightmap (msurface_t *surf);
void GL_EndBuildingLightmaps ();
void GL_SubdivideSurface (msurface_t *fa);
void SetupSurfaceConnectivity(msurface_t *surf);
void CL_SetLightstyle (int i);
void DrawString (int x, int y, char *s);
void DrawAltString (int x, int y, char *s);
void DrawMixedString (int x, int y, char *s, int pos);
char *Cvar_VariableString (char *var_name);
void MakeFrustum4Light(bool ingame);
char *CM_EntityString ();
void R_CalcAliasFrameLerp (dmdl_t *paliashdr);
void R_MarkShadowCasting (shadowlight_t *light, mnode_t *node);
void R_ClearSLights();
void Draw_Pic (int x, int y, char *pic);
void Draw_Pic16 (int x, int y, char *pic);
void SCR_TouchPics ();
void Render_Frame (refdef_t *fd, bool bloom);
float CalcFov (float fov_x, float width, float height);
void Con_ToggleConsole_f ();
void Com_Error (int code, char *fmt, ...);
cvar_t *Cvar_Get (char *var_name, char *var_value, int flags);
void Cvar_SetValue (char *var_name, float value);
cvar_t *Cvar_Set (char *var_name, char *value);
void Com_Printf (char *fmt, ...);
void Cmd_AddCommand (char *cmd_name, xcommand_t function);
void Sys_Error (char *error, ...);
void GL_TextureMode( char *string );
void GL_TextureAlphaMode( char *string );
void GL_TextureSolidMode( char *string );
void GL_UpdateSwapInterval();
image_t *GL_LoadPic (char *name, char *name0, byte *pic, int width, int height, imagetype_t type, int bits, bool no_compress, unsigned i_hash);
image_t	*GL_FindImage (char *name, imagetype_t type, bool exactly, image_t *tex, bool autobump, int override);
void S_Shutdown();
void S_StopAllSounds();
void S_Init ();
sfxcache_t *S_LoadSound (sfx_t *s);
sfx_t *S_RegisterSound (char *name, unsigned lockTime);
void Sys_SendKeyEvents ();
void S_StartLocalSound (char *sound);
void Menu_AddItem( menuframework_s *menu, void *item );
void Menu_Draw( menuframework_s *menu );
const char *Default_MenuKey( menuframework_s *m, int key );
void Menu_AdjustCursor( menuframework_s *m, int dir );
void M_PopMenu ();
void Menu_Center( menuframework_s *menu );
float ClampCvar( float min, float max, float value );
void M_Menu_Main_f ();
bool SV_RateDrop (client_t *c);
bool SV_SendClientDatagram (client_t *client);
void SetSky (char *name, float rotate, vec3_t axis);
void SV_ShutdownGameProgs ();
void R_BeginRegistration (char *model);
struct model_s *R_RegisterModel (char *name, float scale, bool invert);
void CL_LoadClientinfo (clientinfo_t *ci, char *s);
void CL_PrepRefresh ();
void AngleVectors (vec3_t angles, vec3_t forward, vec3_t right, vec3_t up);
float VectorNormalize (vec3_t v);
void VectorScale (vec3_t in, float scale, vec3_t out);
float VectorLength(vec3_t v);
image_t *R_RegisterSkin (char *name);
image_t *R_RegisterBump (char *name, image_t *tex, model_t *mod);
image_t *R_RegisterLight (char *name);
char *COM_Parse (char **data_p);
void LoadPCX (char *filename, byte **pic, byte **palette, int *width, int *height);
void Com_DPrintf (char *fmt, ...);
void GL_RenderBrushPoly (msurface_t *fa, vec3_t org, bool tris);
void KillZipCache(bool);
void LoadJPG (char *name, byte **pic, int *width, int *height);
void CrossProduct (vec3_t v1, vec3_t v2, vec3_t cross);
void LoadTGA (char *name, byte **pic, int *width, int *height);
void LoadDDS (char *name, byte **pic, int *width, int *height);
trace_t	CM_BoxTrace (vec3_t start, vec3_t end, vec3_t mins, vec3_t maxs, int headnode, int brushmask);
void R_SpawnDecal(vec3_t center, vec3_t normal, image_t *texture, float minsize, float maxsize, float minlifetime, float maxlifetime, float minangle, float maxangle, dtype_t dtype, bool shot, byte style, byte cl_style, defDecal_t *defdec);
int	CL_PMpointcontents (vec3_t point);
int	CL_PMpointcontents2 (vec3_t point, model_t *ignore);
cvar_t *Cvar_ForceSet (char *var_name, char *value);
void Cvar_ForceSetValue (char *var_name, float value);
void Menu_DrawShaderStatusBar( void* unused );
void Menu_DrawTFilterStatusBar( void* unused );
void Menu_DrawAnisoStatusBar( void* unused );
void Menu_DrawAaStatusBar( void* unused );
void VID_MenuDraw ();
int Cmd_Argc ();
char *Cmd_Argv (int arg);
trace_t SV_Trace (vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, edict_t *passedict, int contentmask);
int CM_PointLeafnum (vec3_t p);
int	CM_LeafArea (int leafnum);
float Cvar_VariableValue (char *var_name);
void S_StartSound(vec3_t origin, int entnum, int entchannel, sfx_t *sfx, float fvol, float attenuation, float timeofs);
void CL_ParticleEffect3 (vec3_t org, vec3_t dir, byte color_r, byte color_g, byte color_b, int count, parttype_t type);
void LoadPNG (char *name, byte **pic, int *width, int *height, int *bits);
trace_t		CL_PMTraceWorld (vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, int mask);
trace_t		CM_TransformedBoxTrace (vec3_t start, vec3_t end, vec3_t mins, vec3_t maxs, int headnode, int brushmask, vec3_t origin, vec3_t angles);
int FS_LoadFile (char *path, void **buffer);
void R_RenderView (refdef_t *fd, bool mirror);
void R_SetGL2D ();
void R_InitMirrorChains();
void R_AddMirror(msurface_t *surf);
bool Ent_MirrorClip(float *mins, float *maxs);
void R_LightForPoint (vec3_t point, vec3_t color);
void Create_Demosstrings (int mask);
void CreateShadowVBO();
void TraceDecal();
bool R_MarkAliasLeaves(alink_t *alias);
bool CM_AreasConnected (int area1, int area2);
bool HasSharedLeafs(byte *v1, byte *v2);
byte *Mod_ClusterPVS (int cluster, model_t *model);
mleaf_t *Mod_PointInLeaf (vec3_t p, model_t *model);
unsigned Com_HashKey (const char *string);
bool ComputeBSPLight();
void AddPointToBounds (vec3_t v, vec3_t mins, vec3_t maxs);
void ClearBounds (vec3_t mins, vec3_t maxs);
bool FS_GetWalSizes(char *walname, unsigned *wal_s, unsigned *wal_t);
bool FS_GetPCXSizes(char *picname, unsigned *pic_s, unsigned *pic_t);
void GL_BuildPolygonFromSurface(msurface_t *fa);
void	FS_CreatePath (char *path);
void GL_CheckOcclusion();
void GL_SelectMirrorSize(bool);
void S_RegisterMatSounds(material_t *material);
void CL_ClearClEntities ();
void SelectMapFunc( void *data );
void SelectModFunc( void *data );
void FreeMapNames();
void FreeMapNames2();
model_t *Mod_ForName (char *name, bool crash, float scale, bool invert, bool cache);
material_t	*R_FindMaterial(char *name);
void CL_Sparks (vec3_t org, vec3_t dir);
void CL_WaterSplash (vec3_t org, vec3_t dir, byte color_r, byte color_g, byte color_b);
void CL_ParticleEffect2 (vec3_t org, vec3_t dir, byte color_r, byte color_g, byte color_b, int count, parttype_t type);
void CL_BlasterParticles (vec3_t org, vec3_t dir, byte color_r, byte color_g, byte color_b, int count);
explosion_t *CL_AllocExplosion ();
void CL_RailTrail (vec3_t start, vec3_t end, byte core, byte spiral, byte radius);
void CL_ExplosionParticles (vec3_t org);
void CL_BFGExplosionParticles (vec3_t org, int cnt);
trace_t	CL_PMTrace (vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, int mask);
void CL_ParseLaser (int colors, bool large_map);
void CL_BubbleTrail (vec3_t start, vec3_t end, int type);
void CL_ParseBeam (struct model_s *model, bool large_map);
void CL_BigTeleportParticles (vec3_t org);
void CL_ParseBeam2 (struct model_s *model, bool large_map);
int CL_ParseLightning (struct model_s *model, bool large_map);
void CL_DebugTrail (vec3_t start, vec3_t end);
void CL_Flashlight (int ent, vec3_t pos);
void CL_ForceWall (vec3_t start, vec3_t end, int color);
void CL_ParsePlayerBeam (model_t *model, bool large_map);
void CL_ParseSteam (bool large_map);
void CL_BubbleTrail2 (vec3_t start, vec3_t end, int dist);
void CL_ParticleSteamEffect (vec3_t org, vec3_t dir, int color, int count, int magnitude);
void CL_ColorFlash (vec3_t pos, int ent, int intensity, float r, float g, float b);
void CL_ColorExplosionParticles (vec3_t org, int color, int run);
void CL_TeleportParticles (vec3_t org);
void CL_ParseWidow (bool large_map);
void CL_WidowSplash (vec3_t org);
void CL_ParseNuke (bool large_map);
void SV_Frame (int msec);
void CL_Frame (int msec);
int CM_WriteAreaBits (byte *buffer, int area);
void Mod_LoadSpriteModel (model_t *mod, void *buffer, bool cache);
void Mod_LoadSprite2Model(model_t *mod, void *buffer, bool cache);
void Mod_LoadBrushModel(model_t *mod, void *buffer);
int findneighbour(int index, int edgei, int numtris, neighbours_t *triangles, dtriangle_t *tris);
void VecsForTris(float *v0, float *v1, float *v2, float *st0, float *st1, float *st2, vec3_t Tangent, vec3_t Binormal);
void Draw_PicScaled (int x, int y, float scale_x, float scale_y, char *pic);
void R_ClearMirrorChains(bool full_clear);
void CL_Clear3DHud();
void CL_RegisterTEntModels ();
void R_EndRegistration ();
void CL_ABIndex();
bool R_CalcSLight();
void R_MarkFogSurfs ();
void CL_SplashSparks (vec3_t org, vec3_t dir, byte r0, byte g0, byte b0, byte r1, byte g1, byte b1, int count);
byte *CM_ClusterPVS (int cluster);
int CM_LeafCluster (int leafnum);
int	CM_BoxLeafnums (vec3_t mins, vec3_t maxs, int *list, int listsize, int *topnode);
void S_SpatializeOrigin (vec3_t origin, float master_vol, float dist_mult, int *left_vol, int *right_vol);
channel_t *S_PickChannel(int entnum, int entchannel);
void CL_AddBubbles (float num);
void CheckEntityFrame(dmdl_t *paliashdr);
void CheckEntityFrameMD3(maliasmodel_t *paliashdr);
void R_AddModelLight();
void CL_ParticleEffect (vec3_t org, vec3_t dir, byte color_r, byte color_g, byte color_b, int count, parttype_t type, int flag);
void CL_EmitParticles (vec3_t org, vec3_t dir, byte color_r0, byte color_g0, byte color_b0, byte color_r1, byte color_g1, byte color_b1, int count, parttype_t type, float gravity, float vel, float spread, int flags, float alphavel);
int Q_strcasecmp (char *s1, char *s2);
void	Cmd_RemoveCommand (char *cmd_name);
void SCR_FinishCinematic ();
void CL_Snd_Restart_f ();
void Z_Free (void *ptr);
void FS_Read (void *buffer, int len, FILE *f);
void *Z_Malloc (int size, bool crash);
void S_StopBackgroundTrack();
void Com_sprintf (char *dest, int size, char *fmt, ...);
void SCR_EndLoadingPlaque ();
int FS_FOpenFile (char *filename, FILE **file, bool test, int zip_start);
int Sys_Milliseconds ();
void SCR_BeginLoadingPlaque ();
unsigned MSG_ReadByte (sizebuf_t *msg_read);
void MSG_WriteChar (sizebuf_t *sb, int c);
void MSG_WriteByte (sizebuf_t *sb, unsigned c);
void SZ_Print (sizebuf_t *buf, char *data);
char	*va(char *format, ...);
void Com_sprintf (char *dest, int size, char *fmt, ...);
void GL_ResampleTexture (unsigned *in, int inwidth, int inheight, unsigned *out,  int outwidth, int outheight, bool normalMap);
void Keys_MenuInit();
void Keys_MenuDraw ();
const char *Keys_MenuKey( int key );
const char *Keys_MenuKey2( int key );
void M_PushMenu ( void (*draw) (void), const char *(*key) (int k) );
void Keys2_MenuInit();
void Keys2_MenuDraw ();
void ToggleSoundFunc( void *unused );
void ToggleMuteFocusFunc( void *unused );
void UpdateVolumeFunc( void *unused );
void UpdateSoundQualityFunc( void *unused );
void MouseSpeedFunc( void *unused );
void ZoomSpeedFunc( void *unused );
void AlwaysRunFunc( void *unused );
void InvertMouseFunc( void *unused );
void LookspringFunc( void *unused );
void LookstrafeFunc( void *unused );
void FreeLookFunc( void *unused );
void CrosshairFunc( void *unused );
void M_Banner( char *name );
void CL_ParseDelta (entity_state_t *from, entity_state_t *to, int number, int bits);
void Adjust320to640(int *x0, int *y0, int *x1, int *y1);
char *FS_NextPath (char *prevpath);
char **FS_ListFiles( char *findname, int *numfiles, unsigned musthave, unsigned canthave );
char *Sys_FindFirst (char *path, unsigned musthave, unsigned canthave );
void Sys_FindClose ();
bool LightEditor_Enabled(bool);
void CL_Paste_f ();
void CL_FixUpGender();
void Mod_LoadFaces (lump_t *l);
void Mod_CalcSurfColors();
void S_StartBackgroundTrack(const char *intro, const char *loop, unsigned offset);
void M_Credits_MenuDraw();
char *FS_Gamedir ();
bool R_MarkEntityLeaves(entity_t *ent, byte *fatvis);
void Menu_DrawStatusBar( const char *string );
void DMOptionsFunc( void *self );
void Cl_UpdateEditorCvars (byte mask);
void CL_ReadClientFile (bool temp);
void R_GrabScreen(char*);
void R_FreePic (char *name);
void Cbuf_InsertText (char *text);
int Q_strncasecmp (char *s1, char *s2, int n);
bool R_FreeClEntities();
bool R_FreeDecal();
void InitModLightsEmits(model_t *mod);
void	ParseMFX(model_t *mod, char *s);
void HACK_RecalcVertsLightNormalIdx (dmdl_t *pheader, bool invert);
void Mod_InvertNormal(dmdl_t *pheader);
void TangentForTrimd3(WORD *index, maliasvertex_t *vertices, maliascoord_t *texcos, vec3_t Tangent, vec3_t Binormal);
void R_BuildTriangleNeighbors ( neighbours_t *neighbors, WORD *indexes, int numtris );
int MSG_ReadChar (sizebuf_t *msg_read);
int MSG_ReadLong (sizebuf_t *msg_read);
int MSG_ReadShort (sizebuf_t *msg_read);
float MSG_ReadAngle16 (sizebuf_t *msg_read);
int MSG_ReadPMCoordNew (sizebuf_t *msg_read);
void MSG_WritePMCoordNew (sizebuf_t *sb, int in);
char *Info_ValueForKey (char *s, char *key);
void MSG_WriteShort (sizebuf_t *sb, int c);
void MSG_WriteLong (sizebuf_t *sb, int c);
void MSG_WriteAngle16 (sizebuf_t *sb, float f);
char	*NET_AdrToString (netadr_t a);
void SV_DropClient (client_t *drop);
int StringIsWhitespace (char *name);
void M_LoadMapNames();
void	FS_LoadPureLists();
void	FS_CutNonPures();
void ModCallback( void *self );
void Cbuf_Execute ();
void CL_LogoutEffect (vec3_t org, int type);
void MSG_ReadData (sizebuf_t *msg_read, void *data, int len);
void CL_ParsePlayerstate (frame_t *oldframe, frame_t *newframe);
void CL_ParsePacketEntities (frame_t *oldframe, frame_t *newframe);
void CL_FireEntityEvents (frame_t *frame);
void CL_CheckPredictionError ();
void Draw_GetPicSize (int *w, int *h, char *pic);
void M_DrawTextBox (int x, int y, int width, int lines);
void R_SaveRenderState();
void R_RestoreRenderState();
void Draw_PicSized(int x, int y, float sx, float sy, char *pic);
void KeyUp (kbutton_t *b);
void BeginFrame();
void End_Frame();
void Info_SetValueForKey (char *s, char *key, char *value);
unsigned Com_BlockChecksum (void *buffer, int length);
void Mod_LoadAliasASEModel(model_t *mod, void *buffer, float scale, bool invert, bool cache);
bool Mod_LoadAliasModel(model_t *mod, void *buffer, float scale, bool invert, int smoothType, bool cache);
void Mod_LoadAliasMD3Model(model_t *mod, void *buffer, float scale, bool invert, bool cache);
void Menu_DrawStringColored( int x, int y, const char *string );


bool	enable_to_change_readonly_cvars = false;

byte	needsToLoadClientEntities = 0;		// 0 - no, 1 - from save, 2 - vid_restart (temp)
bool	effectsInited = false;
char	needsToDeferredShot[16];


byte	_r, _g, _b;
int		bubble_think;
bool	render_disable;
int		emit_time;
bool	draw_emits;
char	do_bloom;		// -1 - блум запрещён, 0 - делать грабинг экрана, 1 - делать блум
unsigned	bloom_size;
unsigned	max_bloom_size;	// for reserve texture size


static float Diamond8x[8][8] =
{
	0.0f, 0.0f, 0.0f, 0.1f, 0.1f, 0.0f, 0.0f, 0.0f,
	0.0f, 0.0f, 0.2f, 0.3f, 0.3f, 0.2f, 0.0f, 0.0f,
	0.0f, 0.2f, 0.4f, 0.6f, 0.6f, 0.4f, 0.2f, 0.0f,
	0.1f, 0.3f, 0.6f, 0.9f, 0.9f, 0.6f, 0.3f, 0.1f,
	0.1f, 0.3f, 0.6f, 0.9f, 0.9f, 0.6f, 0.3f, 0.1f,
	0.0f, 0.2f, 0.4f, 0.6f, 0.6f, 0.4f, 0.2f, 0.0f,
	0.0f, 0.0f, 0.2f, 0.3f, 0.3f, 0.2f, 0.0f, 0.0f,
	0.0f, 0.0f, 0.0f, 0.1f, 0.1f, 0.0f, 0.0f, 0.0f
};

#include "m_flash.h"

char	editor_layoutstring[8192];

char	r1q2_title[128];
///char	r1q2_down_title[128];


int pausedTime = 0;
int oldClTime = 0;
int clTime = 0;
int oldGameTime = 0;
int gameTime = 0;
int deltaTime = 0;

// status bar head
float		headYaw;
float		headEndPitch;
float		headEndYaw;
int			headEndTime;
float		headStartPitch;
float		headStartYaw;
int			headStartTime;

model_t		*currentPlayerWeapon;

char *fragmentprogram_nofilter6_detail_ptr = NULL;
char fragmentprogram_nofilter6_detail[] =
"!!ARBfp1.0\n"
"OPTION ARB_precision_hint_fastest;\n"
"\n"
"#\n"
"# bump mapping with per-pixel diffuse and specular lighting and detailed bump\n"
"#\n"
"# on entry:\n"
"#\n"
"#	fragment.texcoord[0]	- texture coordinates\n"
"#	fragment.texcoord[1]	- light vector (for diffuse bump)\n"
"#	fragment.texcoord[2]	- light falloff coordinates\n"
"#	fragment.texcoord[3]	- halfangle light vector (for specular bump)\n"
"#\n"
"#	program.local[0]	- detailed bump scale\n"
"#	program.local[1]	- light color\n"
"#	program.env[0]		- is the diffuse modifier\n"
"#	program.env[1]		- is the specular modifier\n"
"#\n"
"#	tex0	2D		normal map\n"
"#	tex1	2D		color map\n"
"#	tex2	3D		light falloff\n"
"#	tex5	2D		detailed normal map\n"
"#\n"
"\n"
"TEMP light, specular, localNormal, localDetailNormal, R0, R1, R2, color;\n"
"\n"
"# attenuation\n"
"TEX R0, fragment.texcoord[2], texture[2], 3D;\n"
"\n"
"# get the diffuse map and constant diffuse factor\n"
"TEX R1, fragment.texcoord[0], texture[1], 2D;\n"
"MUL color.xyz, R1, program.env[0];\n"
"\n"
"# perform the specular bump mapping\n"
"DP3 specular.w, fragment.texcoord[3], fragment.texcoord[3];\n"
"RSQ specular.w, specular.w;\n"
"MUL specular.xyz, specular.w, fragment.texcoord[3];\n"
"\n"
"# perform the diffuse and detailed bump mapping\n"
"TEX localNormal, fragment.texcoord[0], texture[0], 2D;\n"
///	"MAD localNormal.xyz, localNormal, 2, -1;\n"
	"ADD localNormal.xyz, localNormal, -0.5;\n"
	"DP3 R2.w, localNormal, localNormal;\n"
	"RSQ R2.w, R2.w;\n"
	"MUL localNormal.xyz, R2.w, localNormal;\n"
"MUL R2, program.local[0], fragment.texcoord[0];\n"
"TEX localDetailNormal, R2, texture[5], 2D;\n"
///"MAD localDetailNormal.xyz, localDetailNormal, 2, -1;\n"
   "MAD localDetailNormal.xyz, localDetailNormal, {2,2,0,0}, {-1,-1,0,0};\n"	/// Berserker: detailbump fix
"# scale and add (normal and detailed normal)\n"
"MAD localDetailNormal.xyz, localDetailNormal, R1.w, localNormal;\n"
"MUL localNormal.a, localNormal.a, localDetailNormal.a;\n"
"# normalize result vector\n"
"DP3 localDetailNormal.w, localDetailNormal, localDetailNormal;\n"
"RSQ localDetailNormal.w, localDetailNormal.w;\n"
"MUL localNormal.xyz, localDetailNormal.w, localDetailNormal;\n"
"# normalize light vector\n"
"DP3 R1.w, fragment.texcoord[1], fragment.texcoord[1];\n"
"RSQ R1.w, R1.w;\n"
"MUL R1.xyz, R1.w, fragment.texcoord[1];\n"
"DP3 light, localNormal, R1;\n"
"\n"
"# modulate by the light falloff\n"
"MUL light, light, R0;\n"
"\n"
"# perform the specular bump mapping\n"
"DP3 specular, specular, localNormal;\n"
"\n"
"# perform a dependent table read for the specular falloff\n"
"MAD_SAT R1, specular, 16, -15;\n"
"MUL R1, R1, program.env[1];\n"
"\n"
"# modulate by the specular map\n"
"MAD color, R1, localNormal.a, color;\n"
"\n"
"MUL color, program.local[1], color;\n"
"MUL result.color, light, color;\n"
"\n"
"END\n\x0";


char *fragmentprogram_filter6_detail_ptr = NULL;
char fragmentprogram_filter6_detail[] =
"!!ARBfp1.0\n"
"OPTION ARB_precision_hint_fastest;\n"
"\n"
"#\n"
"# bump mapping with per-pixel diffuse and specular lighting (using cubemap light filter) and detailed bump\n"
"#\n"
"# on entry:\n"
"#\n"
"#	fragment.texcoord[0]	- texture coordinates\n"
"#	fragment.texcoord[1]	- light vector (for diffuse bump)\n"
"#	fragment.texcoord[2]	- light falloff coordinates\n"
"#	fragment.texcoord[3]	- cubemap light filter coordinates\n"
"#	fragment.texcoord[4]	- halfangle light vector (for specular bump)\n"
"#\n"
"#	program.local[0]	- detailed bump scale\n"
"#	program.local[1]	- light color\n"
"#	program.env[0]		- is the diffuse modifier\n"
"#	program.env[1]		- is the specular modifier\n"
"#\n"
"#	tex0	2D		normal map\n"
"#	tex1	2D		color map\n"
"#	tex2	3D		light falloff\n"
"#	tex3	CUBE		light filter\n"
"#	tex5	2D		detailed normal map\n"
"#\n"
"\n"
"TEMP light, specular, localNormal, localDetailNormal, R0, R1, R2, color;\n"
"\n"
"# Attenuation\n"
"TEX R0, fragment.texcoord[2], texture[2], 3D;\n"
"\n"
"# get the diffuse map and constant diffuse factor\n"
"TEX R1, fragment.texcoord[0], texture[1], 2D;\n"
"MUL color.xyz, R1, program.env[0];\n"
"\n"
"# perform the specular bump mapping\n"
"DP3 specular.w, fragment.texcoord[4], fragment.texcoord[4];\n"
"RSQ specular.w, specular.w;\n"
"MUL specular.xyz, specular.w, fragment.texcoord[4];\n"
"\n"
"# perform the diffuse and detailed bump mapping\n"
"TEX localNormal, fragment.texcoord[0], texture[0], 2D;\n"
///	"MAD localNormal.xyz, localNormal, 2, -1;\n"
	"ADD localNormal.xyz, localNormal, -0.5;\n"
	"DP3 R2.w, localNormal, localNormal;\n"
	"RSQ R2.w, R2.w;\n"
	"MUL localNormal.xyz, R2.w, localNormal;\n"
"MUL R2, program.local[0], fragment.texcoord[0];\n"
"TEX localDetailNormal, R2, texture[5], 2D;\n"
///"MAD localDetailNormal.xyz, localDetailNormal, 2, -1;\n"
   "MAD localDetailNormal.xyz, localDetailNormal, {2,2,0,0}, {-1,-1,0,0};\n"	/// Berserker: detailbump fix
"# scale and add (normal and detailed normal)\n"
"MAD localDetailNormal.xyz, localDetailNormal, R1.w, localNormal;\n"
"MUL localNormal.a, localNormal.a, localDetailNormal.a;\n"
"# normalize result vector\n"
"DP3 localDetailNormal.w, localDetailNormal, localDetailNormal;\n"
"RSQ localDetailNormal.w, localDetailNormal.w;\n"
"MUL localNormal.xyz, localDetailNormal.w, localDetailNormal;\n"
"# normalize light vector\n"
"DP3 R1.w, fragment.texcoord[1], fragment.texcoord[1];\n"
"RSQ R1.w, R1.w;\n"
"MUL R1.xyz, R1.w, fragment.texcoord[1];\n"
"DP3 light, localNormal, R1;\n"
"\n"
"# modulate by the light projection\n"
"TEX R1, fragment.texcoord[3], texture[3], CUBE;\n"
"MUL light, light, R1;\n"
"\n"
"# modulate by the light falloff\n"
"MUL light, light, R0;\n"
"\n"
"# perform the specular bump mapping\n"
"DP3 specular, specular, localNormal;\n"
"\n"
"# perform a dependent table read for the specular falloff\n"
"MAD_SAT R1, specular, 16, -15;\n"
"MUL R1, R1, program.env[1];\n"
"\n"
"# modulate by the specular map\n"
"MAD color, R1, localNormal.a, color;\n"
"\n"
"MUL color, program.local[1], color;\n"
"MUL result.color, light, color;\n"
"\n"
"END\n\x0";


char *fragmentprogram_nofilter_ptr = NULL;
char fragmentprogram_nofilter[] =
"!!ARBfp1.0\n"
"OPTION ARB_precision_hint_fastest;\n"
"\n"
"#\n"
"# bump mapping with per-pixel diffuse and specular lighting\n"
"#\n"
"# on entry:\n"
"#\n"
"#	fragment.texcoord[0]	- texture coordinates\n"
"#	fragment.texcoord[1]	- light vector (for diffuse bump)\n"
"#	fragment.texcoord[2]	- light falloff coordinates\n"
"#	fragment.texcoord[3]	- halfangle light vector (for specular bump)\n"
"#\n"
"#	program.local[1]	- light color\n"
"#	program.env[0]		- is the diffuse modifier\n"
"#	program.env[1]		- is the specular modifier\n"
"#\n"
"#	tex0	2D		normal map\n"
"#	tex1	2D		color map\n"
"#	tex2	3D		light falloff\n"
"#\n"
"\n"
"TEMP light, specular, localNormal, R0, R1, color;\n"
"\n"
"# attenuation\n"
"TEX R0, fragment.texcoord[2], texture[2], 3D;\n"
"\n"
"# get the diffuse map and constant diffuse factor\n"
"TEX R1, fragment.texcoord[0], texture[1], 2D;\n"
"MUL color.xyz, R1, program.env[0];\n"
"\n"
"# perform the specular bump mapping\n"
"DP3 specular.w, fragment.texcoord[3], fragment.texcoord[3];\n"
"RSQ specular.w, specular.w;\n"
"MUL specular.xyz, specular.w, fragment.texcoord[3];\n"
"\n"
"# perform the diffuse bump mapping\n"
"TEX localNormal, fragment.texcoord[0], texture[0], 2D;\n"
///	"MAD localNormal.xyz, localNormal, 2, -1;\n"
	"ADD localNormal.xyz, localNormal, -0.5;\n"
	"DP3 R1.w, localNormal, localNormal;\n"
	"RSQ R1.w, R1.w;\n"
	"MUL localNormal.xyz, R1.w, localNormal;\n"
"# normalize light vector\n"
"DP3 R1.w, fragment.texcoord[1], fragment.texcoord[1];\n"
"RSQ R1.w, R1.w;\n"
"MUL R1.xyz, R1.w, fragment.texcoord[1];\n"
"DP3 light, localNormal, R1;\n"
"\n"
"# modulate by the light falloff\n"
"MUL light, light, R0;\n"
"\n"
"# perform the specular bump mapping\n"
"DP3 specular, specular, localNormal;\n"
"\n"
"# perform a dependent table read for the specular falloff\n"
"MAD_SAT R1, specular, 16, -15;\n"
"MUL R1, R1, program.env[1];\n"
"\n"
"# modulate by the specular map\n"
"MAD color, R1, localNormal.a, color;\n"
"\n"
"MUL color, program.local[1], color;\n"
"MUL result.color, light, color;\n"
"\n"
"END\n\x0";


char *fragmentprogram_filter_ptr = NULL;
char fragmentprogram_filter[] =
"!!ARBfp1.0\n"
"OPTION ARB_precision_hint_fastest;\n"
"\n"
"#\n"
"# bump mapping with per-pixel diffuse and specular lighting (using cubemap light filter)\n"
"#\n"
"# on entry:\n"
"#\n"
"#	fragment.texcoord[0]	- texture coordinates\n"
"#	fragment.texcoord[1]	- light vector (for diffuse bump)\n"
"#	fragment.texcoord[2]	- light falloff coordinates\n"
"#	fragment.texcoord[3]	- cubemap light filter coordinates\n"
"#	fragment.texcoord[4]	- halfangle light vector (for specular bump)\n"
"#\n"
"#	program.local[1]	- light color\n"
"#	program.env[0]		- is the diffuse modifier\n"
"#	program.env[1]		- is the specular modifier\n"
"#\n"
"#	tex0	2D		normal map\n"
"#	tex1	2D		color map\n"
"#	tex2	3D		light falloff\n"
"#	tex3	CUBE		light filter\n"
"#\n"
"\n"
"TEMP light, specular, localNormal, R0, R1, color;\n"
"\n"
"# Attenuation\n"
"TEX R0, fragment.texcoord[2], texture[2], 3D;\n"
"\n"
"# get the diffuse map and constant diffuse factor\n"
"TEX R1, fragment.texcoord[0], texture[1], 2D;\n"
"MUL color.xyz, R1, program.env[0];\n"
"\n"
"# perform the specular bump mapping\n"
"DP3 specular.w, fragment.texcoord[4], fragment.texcoord[4];\n"
"RSQ specular.w, specular.w;\n"
"MUL specular.xyz, specular.w, fragment.texcoord[4];\n"
"\n"
"# perform the diffuse bump mapping\n"
"TEX localNormal, fragment.texcoord[0], texture[0], 2D;\n"
///	"MAD localNormal.xyz, localNormal, 2, -1;\n"
	"ADD localNormal.xyz, localNormal, -0.5;\n"
	"DP3 R1.w, localNormal, localNormal;\n"
	"RSQ R1.w, R1.w;\n"
	"MUL localNormal.xyz, R1.w, localNormal;\n"
"# normalize light vector\n"
"DP3 R1.w, fragment.texcoord[1], fragment.texcoord[1];\n"
"RSQ R1.w, R1.w;\n"
"MUL R1.xyz, R1.w, fragment.texcoord[1];\n"
"DP3 light, localNormal, R1;\n"
"\n"
"# modulate by the light projection\n"
"TEX R1, fragment.texcoord[3], texture[3], CUBE;\n"
"MUL light, light, R1;\n"
"\n"
"# modulate by the light falloff\n"
"MUL light, light, R0;\n"
"\n"
"# perform the specular bump mapping\n"
"DP3 specular, specular, localNormal;\n"
"\n"
"# perform a dependent table read for the specular falloff\n"
"MAD_SAT R1, specular, 16, -15;\n"
"MUL R1, R1, program.env[1];\n"
"\n"
"# modulate by the specular map\n"
"MAD color, R1, localNormal.a, color;\n"
"\n"
"MUL color, program.local[1], color;\n"
"MUL result.color, light, color;\n"
"\n"
"END\n\x0";


char *fragmentprogram_filter4_ptr = NULL;
char fragmentprogram_filter4[] =
"!!ARBfp1.0\n"
"OPTION ARB_precision_hint_fastest;\n"
"\n"
"#\n"
"# bump mapping with per-pixel diffuse lighting (using cubemap light filter), 4 TMUs version\n"
"#\n"
"# on entry:\n"
"#\n"
"#	fragment.texcoord[0]	- texture coordinates\n"
"#	fragment.texcoord[1]	- light vector (for diffuse bump)\n"
"#	fragment.texcoord[2]	- light falloff coordinates\n"
"#	fragment.texcoord[3]	- cubemap light filter coordinates\n"
"#\n"
"#	program.local[1]	- light color\n"
"#	program.env[0]		- is the diffuse modifier\n"
"#	program.env[1]		- is the specular modifier\n"
"#\n"
"#	tex0	2D		normal map\n"
"#	tex1	2D		color map\n"
"#	tex2	3D		light falloff\n"
"#	tex3	CUBE		light filter\n"
"#\n"
"\n"
"TEMP light, specular, localNormal, R0, R1, color;\n"
"\n"
"# Attenuation\n"
"TEX R0, fragment.texcoord[2], texture[2], 3D;\n"
"\n"
"# get the diffuse map and constant diffuse factor\n"
"TEX R1, fragment.texcoord[0], texture[1], 2D;\n"
"MUL color.xyz, R1, program.env[0];\n"
"\n"
"# perform the diffuse bump mapping\n"
"TEX localNormal, fragment.texcoord[0], texture[0], 2D;\n"
///	"MAD localNormal.xyz, localNormal, 2, -1;\n"
	"ADD localNormal.xyz, localNormal, -0.5;\n"
	"DP3 R1.w, localNormal, localNormal;\n"
	"RSQ R1.w, R1.w;\n"
	"MUL localNormal.xyz, R1.w, localNormal;\n"
"# normalize light vector\n"
"DP3 R1.w, fragment.texcoord[1], fragment.texcoord[1];\n"
"RSQ R1.w, R1.w;\n"
"MUL R1.xyz, R1.w, fragment.texcoord[1];\n"
"DP3 light, localNormal, R1;\n"
"\n"
"# modulate by the light projection\n"
"TEX R1, fragment.texcoord[3], texture[3], CUBE;\n"
"MOV specular, light;\n"
"MUL light, light, R1;\n"
"\n"
"# modulate by the light falloff\n"
"MUL light, light, R0;\n"
"\n"
"# perform a dependent table read for the specular falloff\n"
"MAD_SAT R1, specular, 16, -15;\n"
"MUL R1, R1, program.env[1];\n"
"\n"
"# modulate by the specular map\n"
"MAD color, R1, localNormal.a, color;\n"
"\n"
"MUL color, program.local[1], color;\n"
"MUL result.color, light, color;\n"
"\n"
"END\n\x0";

char *arbdistort_ptr = NULL;
char arbdistort[] =
"!!ARBfp1.0\n"
"OPTION ARB_precision_hint_fastest;\n"
"TEMP R0, R1;\n"
"TEX R0, fragment.texcoord[0], texture[0], 2D;\n"
"TEX R1, fragment.texcoord[2], texture[2], 2D;\n"	/// Тех2.Alpha используется как маска - для обрезания эффекта и/или для затухания
"CMP R1, program.env[3], 1, R1;\n"	// если program.env[3]={0,0,0,-1}, то игнорируется затухание
///"MAD R0, R0, 2, -1;\n"		// Так вроде правильнее, но тогда distort-зеркалах неприятный боковой артефакт
   "ADD R0, R0, R0;\n"
"MUL R0.xyzw, R0, R1.wwww;\n"
"ADD R0, R0, fragment.texcoord[1];\n"
"RCP R1, R0.w;\n"
"MUL R0, R0, R1;\n"
"SUB R1, program.env[2], R0;\n"
"CMP R0, R1, program.env[2], R0;\n"
"TEX R0, R0, texture[1], 2D;\n"
"MUL result.color, R0, fragment.color;\n"
"END\n\x0";


char *fragmentprogram_ambient_ptr = NULL;
char fragmentprogram_ambient[] =
"!!ARBfp1.0\n"
"OPTION ARB_precision_hint_fastest;\n"
"\n"
"#\n"
"# no bump mapping\n"
"#\n"
"# on entry:\n"
"#\n"
"#	fragment.texcoord[0]	- texture coordinates\n"
"#	fragment.texcoord[2]	- light falloff coordinates\n"
"#\n"
"#	program.local[1]	- light color\n"
"#	program.env[0]		- is the diffuse modifier\n"
"#\n"
"#	tex1	2D		color map\n"
"#	tex2	3D		light falloff\n"
"#\n"
"\n"
"TEMP R0, color;\n"
"\n"
"# attenuation\n"
"TEX R0, fragment.texcoord[2], texture[2], 3D;\n"
"\n"
"# get the diffuse map and constant diffuse factor\n"
"TEX color, fragment.texcoord[0], texture[1], 2D;\n"
"MUL color.xyz, color, program.env[0];\n"
"\n"
"# modulate by the light falloff\n"
"MUL color, color, R0;\n"
"\n"
"MUL result.color, color, program.local[1];\n"
"\n"
"END\n\x0";


char *vertexprogram_nolightfilter_ptr = NULL;
char vertexprogram_nolightfilter[] =
"!!ARBvp1.0\n"
"OPTION ARB_position_invariant;\n"
"\n"
"# for ARB4, ARB6 w/o lightfilters\n"
"# input:\n"
"# vertex.position	- vertexArray	float[3]\n"
"# vertex.texcoord[0]	- stArray	float[2]\n"
"# vertex.texcoord[1]	- binormalArray	float[3]\n"
"# vertex.texcoord[2]	- normalArray	float[3]\n"
"# vertex.texcoord[3]	- tangentArray	float[3]\n"
"# program.local[0]	- light origin	float[3]\n"
"# state.matrix.texture[0] - texture matrix for texture rotation\n"
"# state.matrix.texture[2] - texture matrix for 3D light falloff\n"
"#\n"
"# output:\n"
"# result.texcoord[0]	- stArray	float[2]\n"
"# result.texcoord[1]	- LightP	float[3]\n"
"# result.texcoord[2]	- vertexArray	float[3]\n"
"# result.texcoord[3]	- tsH		float[3]\n"
"\n"
"TEMP	R0, R1, R2;\n"
"PARAM	mvit[4] = {state.matrix.modelview.invtrans};\n"
"\n"
"# store stArray for colormap and bumpmap\n"
"MOV	R0.xy, vertex.texcoord[0];\n"
"MOV	R0.zw, {0, 1};\n"
"DP4	result.texcoord[0].x, R0, state.matrix.texture[0].row[0];\n"
"DP4	result.texcoord[0].y, R0, state.matrix.texture[0].row[1];\n"
"\n"
"# store vertexArray for 3D light falloff\n"
"MOV	R0.xyz, vertex.position;\n"
"MOV	R0.w, 1;\n"
"DP4	result.texcoord[2].x, R0, state.matrix.texture[2].row[0];\n"
"DP4	result.texcoord[2].y, R0, state.matrix.texture[2].row[1];\n"
"DP4	result.texcoord[2].z, R0, state.matrix.texture[2].row[2];\n"
"\n"
"# LightP\n"
"SUB	R0, program.local[0], vertex.position;\n"
"DP3	R1.x, R0, vertex.texcoord[3];\n"
"DP3	R1.y, R0, vertex.texcoord[1];\n"
"DP3	R1.z, R0, vertex.texcoord[2];\n"
"MOV	R1.w, 1;\n"
"DP4	result.texcoord[1].x, R1, state.matrix.texture[0].row[0];\n"
"DP4	result.texcoord[1].y, R1, state.matrix.texture[0].row[1];\n"
"DP4	result.texcoord[1].z, R1, state.matrix.texture[0].row[2];\n"
"\n"
"# tsH\n"
"SUB	R0, mvit[3], vertex.position;\n"
"DP3	R2.x, R0, vertex.texcoord[3];\n"
"DP3	R2.y, R0, vertex.texcoord[1];\n"
"DP3	R2.z, R0, vertex.texcoord[2];\n"
"MOV	R2.w, 1;\n"
"ADD	R0, R1, R2;\n"
"DP4	result.texcoord[3].x, R0, state.matrix.texture[0].row[0];\n"
"DP4	result.texcoord[3].y, R0, state.matrix.texture[0].row[1];\n"
"DP4	result.texcoord[3].z, R0, state.matrix.texture[0].row[2];\n"
"\n"
"END\n\x0";


char *vertexprogram_lightfilter6_ptr = NULL;
char vertexprogram_lightfilter6[] =
"!!ARBvp1.0\n"
"OPTION ARB_position_invariant;\n"
"\n"
"# for ARB6 with lightfilters\n"
"# input:\n"
"# vertex.position	- vertexArray	float[3]\n"
"# vertex.texcoord[0]	- stArray	float[2]\n"
"# vertex.texcoord[1]	- binormalArray	float[3]\n"
"# vertex.texcoord[2]	- normalArray	float[3]\n"
"# vertex.texcoord[3]	- tangentArray	float[3]\n"
"# program.local[0]	- light origin	float[3]\n"
"# state.matrix.texture[0] - texture matrix for texture rotation\n"
"# state.matrix.texture[2] - texture matrix for 3D light falloff\n"
"# state.matrix.texture[3] - texture matrix for CubeMap light filter\n"
"#\n"
"# output:\n"
"# result.texcoord[0]	- stArray	float[2]\n"
"# result.texcoord[1]	- LightP	float[3]\n"
"# result.texcoord[2]	- vertexArray	float[3]\n"
"# result.texcoord[3]	- vertexArray	float[3]\n"
"# result.texcoord[4]	- tsH		float[3]\n"
"\n"
"TEMP	R0, R1, R2;\n"
"PARAM	mvit[4] = {state.matrix.modelview.invtrans};\n"
"\n"
"# store stArray for colormap and bumpmap\n"
"MOV	R0.xy, vertex.texcoord[0];\n"
"MOV	R0.zw, {0, 1};\n"
"DP4	result.texcoord[0].x, R0, state.matrix.texture[0].row[0];\n"
"DP4	result.texcoord[0].y, R0, state.matrix.texture[0].row[1];\n"
"\n"
"# store vertexArray for 3D light falloff\n"
"MOV	R0.xyz, vertex.position;\n"
"MOV	R0.w, 1;\n"
"DP4	result.texcoord[2].x, R0, state.matrix.texture[2].row[0];\n"
"DP4	result.texcoord[2].y, R0, state.matrix.texture[2].row[1];\n"
"DP4	result.texcoord[2].z, R0, state.matrix.texture[2].row[2];\n"
"\n"
"# store vertexArray for CubeMap light filter\n"
"DP4	result.texcoord[3].x, R0, state.matrix.texture[3].row[0];\n"
"DP4	result.texcoord[3].y, R0, state.matrix.texture[3].row[1];\n"
"DP4	result.texcoord[3].z, R0, state.matrix.texture[3].row[2];\n"
"\n"
"# LightP\n"
"SUB	R0, program.local[0], vertex.position;\n"
"DP3	R1.x, R0, vertex.texcoord[3];\n"
"DP3	R1.y, R0, vertex.texcoord[1];\n"
"DP3	R1.z, R0, vertex.texcoord[2];\n"
"MOV	R1.w, 1;\n"
"DP4	result.texcoord[1].x, R1, state.matrix.texture[0].row[0];\n"
"DP4	result.texcoord[1].y, R1, state.matrix.texture[0].row[1];\n"
"DP4	result.texcoord[1].z, R1, state.matrix.texture[0].row[2];\n"
"\n"
"# tsH\n"
"SUB	R0, mvit[3], vertex.position;\n"
"DP3	R2.x, R0, vertex.texcoord[3];\n"
"DP3	R2.y, R0, vertex.texcoord[1];\n"
"DP3	R2.z, R0, vertex.texcoord[2];\n"
"MOV	R2.w, 1;\n"
"ADD	R0, R1, R2;\n"
"DP4	result.texcoord[4].x, R0, state.matrix.texture[0].row[0];\n"
"DP4	result.texcoord[4].y, R0, state.matrix.texture[0].row[1];\n"
"DP4	result.texcoord[4].z, R0, state.matrix.texture[0].row[2];\n"
"\n"
"END\n\x0";


char *vertexprogram_lightfilter4_ptr = NULL;
char vertexprogram_lightfilter4[] =
"!!ARBvp1.0\n"
"OPTION ARB_position_invariant;\n"
"\n"
"# for ARB4 with lightfilters\n"
"# input:\n"
"# vertex.position	- vertexArray	float[3]\n"
"# vertex.texcoord[0]	- stArray	float[2]\n"
"# vertex.texcoord[1]	- binormalArray	float[3]\n"
"# vertex.texcoord[2]	- normalArray	float[3]\n"
"# vertex.texcoord[3]	- tangentArray	float[3]\n"
"# program.local[0]	- light origin	float[3]\n"
"# state.matrix.texture[0] - texture matrix for texture rotation\n"
"# state.matrix.texture[2] - texture matrix for 3D light falloff\n"
"# state.matrix.texture[3] - texture matrix for CubeMap light filter\n"
"#\n"
"# output:\n"
"# result.texcoord[0]	- stArray	float[2]\n"
"# result.texcoord[1]	- LightP	float[3]\n"
"# result.texcoord[2]	- vertexArray	float[3]\n"
"# result.texcoord[3]	- vertexArray	float[3]\n"
"\n"
"TEMP	R0, R1;\n"
"\n"
"# store stArray for colormap and bumpmap\n"
"MOV	R0.xy, vertex.texcoord[0];\n"
"MOV	R0.zw, {0, 1};\n"
"DP4	result.texcoord[0].x, R0, state.matrix.texture[0].row[0];\n"
"DP4	result.texcoord[0].y, R0, state.matrix.texture[0].row[1];\n"
"\n"
"# store vertexArray for 3D light falloff\n"
"MOV	R0.xyz, vertex.position;\n"
"MOV	R0.w, 1;\n"
"DP4	result.texcoord[2].x, R0, state.matrix.texture[2].row[0];\n"
"DP4	result.texcoord[2].y, R0, state.matrix.texture[2].row[1];\n"
"DP4	result.texcoord[2].z, R0, state.matrix.texture[2].row[2];\n"
"\n"
"# store vertexArray for CubeMap light filter\n"
"DP4	result.texcoord[3].x, R0, state.matrix.texture[3].row[0];\n"
"DP4	result.texcoord[3].y, R0, state.matrix.texture[3].row[1];\n"
"DP4	result.texcoord[3].z, R0, state.matrix.texture[3].row[2];\n"
"\n"
"# LightP\n"
"SUB	R0, program.local[0], vertex.position;\n"
"DP3	R1.x, R0, vertex.texcoord[3];\n"
"DP3	R1.y, R0, vertex.texcoord[1];\n"
"DP3	R1.z, R0, vertex.texcoord[2];\n"
"MOV	R1.w, 1;\n"
"DP4	result.texcoord[1].x, R1, state.matrix.texture[0].row[0];\n"
"DP4	result.texcoord[1].y, R1, state.matrix.texture[0].row[1];\n"
"DP4	result.texcoord[1].z, R1, state.matrix.texture[0].row[2];\n"
"\n"
"END\n\x0";


char *vertexprogram_lightfilter6_parallax_ptr = NULL;
char vertexprogram_lightfilter6_parallax[] =
"!!ARBvp1.0\n"
"OPTION ARB_position_invariant;\n"
"\n"
"# for ARB6 with lightfilters and parallax\n"
"# input:\n"
"# vertex.position	- vertexArray	float[3]\n"
"# vertex.texcoord[0]	- stArray	float[2]\n"
"# vertex.texcoord[1]	- binormalArray	float[3]\n"
"# vertex.texcoord[2]	- normalArray	float[3]\n"
"# vertex.texcoord[3]	- tangentArray	float[3]\n"
"# program.local[0]	- light origin	float[3]\n"
"# state.matrix.texture[2] - texture matrix for 3D light falloff\n"
"# state.matrix.texture[3] - texture matrix for CubeMap light filter\n"
"#\n"
"# output:\n"
"# result.texcoord[0]	- stArray	float[2]\n"
"# result.texcoord[1]	- LightP	float[3]\n"
"# result.texcoord[2]	- vertexArray	float[3]\n"
"# result.texcoord[3]	- vertexArray	float[3]\n"
"# result.texcoord[4]	- tsH		float[3]\n"
"# result.texcoord[5]	- eyeVec	float[3]\n"
"\n"
"TEMP	R0, R1, R2;\n"
"PARAM	mvit[4] = {state.matrix.modelview.invtrans};\n"
"\n"
"# store stArray for colormap and bumpmap (identity texture matrix)\n"
"MOV	result.texcoord[0], vertex.texcoord[0];\n"
"\n"
"# store vertexArray for 3D light falloff\n"
"MOV	R0.xyz, vertex.position;\n"
"MOV	R0.w, 1;\n"
"DP4	result.texcoord[2].x, R0, state.matrix.texture[2].row[0];\n"
"DP4	result.texcoord[2].y, R0, state.matrix.texture[2].row[1];\n"
"DP4	result.texcoord[2].z, R0, state.matrix.texture[2].row[2];\n"
"\n"
"# store vertexArray for CubeMap light filter\n"
"DP4	result.texcoord[3].x, R0, state.matrix.texture[3].row[0];\n"
"DP4	result.texcoord[3].y, R0, state.matrix.texture[3].row[1];\n"
"DP4	result.texcoord[3].z, R0, state.matrix.texture[3].row[2];\n"
"\n"
"# LightP\n"
"SUB	R0, program.local[0], vertex.position;\n"
"DP3	R1.x, R0, vertex.texcoord[3];\n"
"DP3	R1.y, R0, vertex.texcoord[1];\n"
"DP3	R1.z, R0, vertex.texcoord[2];\n"
"MOV	R1.w, 1;\n"
"MOV	result.texcoord[1], R1;\n"
"\n"
"# tsH\n"
"SUB	R0, mvit[3], vertex.position;\n"
"DP3	R2.x, R0, vertex.texcoord[3];\n"
"DP3	R2.y, R0, vertex.texcoord[1];\n"
"DP3	R2.z, R0, vertex.texcoord[2];\n"
"MOV	R2.w, 1;\n"
"MOV	result.texcoord[5], R2;\n"
"ADD	result.texcoord[4], R1, R2;\n"
"\n"
"END\n\x0";


char *vertexprogram_nolightfilter_parallax_ptr = NULL;
char vertexprogram_nolightfilter_parallax[] =
"!!ARBvp1.0\n"
"OPTION ARB_position_invariant;\n"
"\n"
"# for ARB6 w/o lightfilters, with parallax\n"
"# input:\n"
"# vertex.position	- vertexArray	float[3]\n"
"# vertex.texcoord[0]	- stArray	float[2]\n"
"# vertex.texcoord[1]	- binormalArray	float[3]\n"
"# vertex.texcoord[2]	- normalArray	float[3]\n"
"# vertex.texcoord[3]	- tangentArray	float[3]\n"
"# program.local[0]	- light origin	float[3]\n"
"# state.matrix.texture[2] - texture matrix for 3D light falloff\n"
"#\n"
"# output:\n"
"# result.texcoord[0]	- stArray	float[2]\n"
"# result.texcoord[1]	- LightP	float[3]\n"
"# result.texcoord[2]	- vertexArray	float[3]\n"
"# result.texcoord[3]	- tsH		float[3]\n"
"# result.texcoord[5]	- eyeVec	float[3]\n"
"\n"
"TEMP	R0, R1, R2;\n"
"PARAM	mvit[4] = {state.matrix.modelview.invtrans};\n"
"\n"
"# store stArray for colormap and bumpmap (identity texture matrix)\n"
"MOV	result.texcoord[0], vertex.texcoord[0];\n"
"\n"
"# store vertexArray for 3D light falloff\n"
"MOV	R0.xyz, vertex.position;\n"
"MOV	R0.w, 1;\n"
"DP4	result.texcoord[2].x, R0, state.matrix.texture[2].row[0];\n"
"DP4	result.texcoord[2].y, R0, state.matrix.texture[2].row[1];\n"
"DP4	result.texcoord[2].z, R0, state.matrix.texture[2].row[2];\n"
"\n"
"# LightP\n"
"SUB	R0, program.local[0], vertex.position;\n"
"DP3	R1.x, R0, vertex.texcoord[3];\n"
"DP3	R1.y, R0, vertex.texcoord[1];\n"
"DP3	R1.z, R0, vertex.texcoord[2];\n"
"MOV	R1.w, 1;\n"
"MOV	result.texcoord[1], R1;\n"
"\n"
"# tsH\n"
"SUB	R0, mvit[3], vertex.position;\n"
"DP3	R2.x, R0, vertex.texcoord[3];\n"
"DP3	R2.y, R0, vertex.texcoord[1];\n"
"DP3	R2.z, R0, vertex.texcoord[2];\n"
"MOV	R2.w, 1;\n"
"MOV	result.texcoord[5], R2;\n"
"ADD	result.texcoord[3], R1, R2;\n"
"\n"
"END\n\x0";


char *fragmentprogram_filter_parallax_ptr = NULL;
char fragmentprogram_filter_parallax[] =
"!!ARBfp1.0\n"
"OPTION ARB_precision_hint_fastest;\n"
"\n"
"#\n"
"# bump mapping with per-pixel diffuse and specular lighting (using cubemap light filter) and parallax\n"
"#\n"
"# on entry:\n"
"#\n"
"#	fragment.texcoord[0]	- texture coordinates\n"
"#	fragment.texcoord[1]	- light vector (for diffuse bump)\n"
"#	fragment.texcoord[2]	- light falloff coordinates\n"
"#	fragment.texcoord[3]	- cubemap light filter coordinates\n"
"#	fragment.texcoord[4]	- halfangle light vector (for specular bump)\n"
"#	fragment.texcoord[5]	- eyeVec (for parallax)\n"
"#\n"
"#	program.local[1]	- light color\n"
"#	program.env[0]		- is the diffuse modifier\n"
"#	program.env[1]		- is the specular modifier\n"
"#\n"
"#	tex0	2D		normal map\n"
"#	tex1	2D		color map\n"
"#	tex2	3D		light falloff\n"
"#	tex3	CUBE		light filter\n"
"#\n"
"\n"
"TEMP light, specular, localNormal, R0, R1, color, height, Toffset;\n"
"\n"
"# Attenuation\n"
"TEX R0, fragment.texcoord[2], texture[2], 3D;\n"
"\n"
"# perform the parallax bump mapping\n"
"DP3 R1.w, fragment.texcoord[5], fragment.texcoord[5];\n"
"RSQ R1.w, R1.w;\n"
"MUL R1.xyz, R1.w, fragment.texcoord[5];\n"
"\n"
"# calculate offset mapping coordinates\n"
"TEX height.w, fragment.texcoord[0], texture[1], 2D;\n"
"MAD height.w, height.w, 0.04, -0.02;\n"
"MAD Toffset, height.w, R1, fragment.texcoord[0];\n"
"\n"
"# perform the specular bump mapping\n"
"DP3 specular.w, fragment.texcoord[4], fragment.texcoord[4];\n"
"RSQ specular.w, specular.w;\n"
"MUL specular.xyz, specular.w, fragment.texcoord[4];\n"
"\n"
"# get the diffuse map and constant diffuse factor\n"
"TEX R1, Toffset, texture[1], 2D;\n"
"MUL color.xyz, R1, program.env[0];\n"
"\n"
"# perform the diffuse bump mapping\n"
"TEX localNormal, Toffset, texture[0], 2D;\n"
///	"MAD localNormal.xyz, localNormal, 2, -1;\n"
	"ADD localNormal.xyz, localNormal, -0.5;\n"
	"DP3 R1.w, localNormal, localNormal;\n"
	"RSQ R1.w, R1.w;\n"
	"MUL localNormal.xyz, R1.w, localNormal;\n"
"# normalize light vector\n"
"DP3 R1.w, fragment.texcoord[1], fragment.texcoord[1];\n"
"RSQ R1.w, R1.w;\n"
"MUL R1.xyz, R1.w, fragment.texcoord[1];\n"
"DP3 light, localNormal, R1;\n"
"\n"
"# modulate by the light projection\n"
"TEX R1, fragment.texcoord[3], texture[3], CUBE;\n"
"MUL light, light, R1;\n"
"\n"
"# modulate by the light falloff\n"
"MUL light, light, R0;\n"
"\n"
"# perform the specular bump mapping\n"
"DP3 specular, specular, localNormal;\n"
"\n"
"# perform a dependent table read for the specular falloff\n"
"MAD_SAT R1, specular, 16, -15;\n"
"MUL R1, R1, program.env[1];\n"
"\n"
"# modulate by the specular map\n"
"MAD color, R1, localNormal.a, color;\n"
"\n"
"MUL color, program.local[1], color;\n"
"MUL result.color, light, color;\n"
"\n"
"END\n\x0";


char *fragmentprogram_nofilter_parallax_ptr = NULL;
char fragmentprogram_nofilter_parallax[] =
"!!ARBfp1.0\n"
"OPTION ARB_precision_hint_fastest;\n"
"\n"
"#\n"
"# bump mapping with per-pixel diffuse and specular lighting and parallax\n"
"#\n"
"# on entry:\n"
"#\n"
"#	fragment.texcoord[0]	- texture coordinates\n"
"#	fragment.texcoord[1]	- light vector (for diffuse bump)\n"
"#	fragment.texcoord[2]	- light falloff coordinates\n"
"#	fragment.texcoord[3]	- halfangle light vector (for specular bump)\n"
"#	fragment.texcoord[5]	- eyeVec (for parallax)\n"
"#\n"
"#	program.local[1]	- light color\n"
"#	program.env[0]		- is the diffuse modifier\n"
"#	program.env[1]		- is the specular modifier\n"
"#\n"
"#	tex0	2D		normal map\n"
"#	tex1	2D		color map\n"
"#	tex2	3D		light falloff\n"
"#\n"
"\n"
"TEMP light, specular, localNormal, R0, R1, color, height, Toffset;\n"
"\n"
"# attenuation\n"
"TEX R0, fragment.texcoord[2], texture[2], 3D;\n"
"\n"
"# perform the parallax bump mapping\n"
"DP3 R1.w, fragment.texcoord[5], fragment.texcoord[5];\n"
"RSQ R1.w, R1.w;\n"
"MUL R1.xyz, R1.w, fragment.texcoord[5];\n"
"\n"
"# calculate offset mapping coordinates\n"
"TEX height.w, fragment.texcoord[0], texture[1], 2D;\n"
"MAD height.w, height.w, 0.04, -0.02;\n"
"MAD Toffset, height.w, R1, fragment.texcoord[0];\n"
"\n"
"# perform the specular bump mapping\n"
"DP3 specular.w, fragment.texcoord[3], fragment.texcoord[3];\n"
"RSQ specular.w, specular.w;\n"
"MUL specular.xyz, specular.w, fragment.texcoord[3];\n"
"\n"
"# get the diffuse map and constant diffuse factor\n"
"TEX R1, Toffset, texture[1], 2D;\n"
"MUL color.xyz, R1, program.env[0];\n"
"\n"
"# perform the diffuse bump mapping\n"
"TEX localNormal, Toffset, texture[0], 2D;\n"
///	"MAD localNormal.xyz, localNormal, 2, -1;\n"
	"ADD localNormal.xyz, localNormal, -0.5;\n"
	"DP3 R1.w, localNormal, localNormal;\n"
	"RSQ R1.w, R1.w;\n"
	"MUL localNormal.xyz, R1.w, localNormal;\n"
"# normalize light vector\n"
"DP3 R1.w, fragment.texcoord[1], fragment.texcoord[1];\n"
"RSQ R1.w, R1.w;\n"
"MUL R1.xyz, R1.w, fragment.texcoord[1];\n"
"DP3 light, localNormal, R1;\n"
"\n"
"# modulate by the light falloff\n"
"MUL light, light, R0;\n"
"\n"
"# perform the specular bump mapping\n"
"DP3 specular, specular, localNormal;\n"
"\n"
"# perform a dependent table read for the specular falloff\n"
"MAD_SAT R1, specular, 16, -15;\n"
"MUL R1, R1, program.env[1];\n"
"\n"
"# modulate by the specular map\n"
"MAD color, R1, localNormal.a, color;\n"
"\n"
"MUL color, program.local[1], color;\n"
"MUL result.color, light, color;\n"
"\n"
"END\n\x0";


char *vertexprogram_ambient_parallax_ptr = NULL;
char vertexprogram_ambient_parallax[] =
"!!ARBvp1.0\n"
"OPTION ARB_position_invariant;\n"
"\n"
"# for ARB6 with ambient and parallax\n"
"# input:\n"
"# vertex.position	- vertexArray		float[3]\n"
"# vertex.texcoord[0]	- stArray.colorMap	float[2]\n"
"# vertex.texcoord[1]	- stArray.lightMap	float[2]\n"
"# vertex.texcoord[2]	- binormalArray		float[3]\n"
"# vertex.texcoord[3]	- stArray.FX		float[2]\n"
"# vertex.texcoord[4]	- normalArray		float[3]\n"
"# vertex.texcoord[5]	- tangentArray		float[3]\n"
"#\n"
"# output:\n"
"# result.texcoord[0]	- stArray.colorMap	float[2]\n"
"# result.texcoord[1]	- stArray.lightMap	float[2]\n"
"# result.texcoord[2]	- eyeVec			float[3]\n"
"# result.texcoord[3]	- stArray.FX		float[2]\n"
"\n"
"TEMP	R0;\n"
"PARAM	mvit[4] = {state.matrix.modelview.invtrans};\n"
"\n"
"MOV	result.texcoord[0], vertex.texcoord[0];\n"
"MOV	result.texcoord[1], vertex.texcoord[1];\n"
"MOV	result.texcoord[3], vertex.texcoord[3];\n"
"MOV	result.color, vertex.color;\n"
"\n"
"SUB	R0, mvit[3], vertex.position;\n"
"DP3	result.texcoord[2].x, R0, vertex.texcoord[5];\n"
"DP3	result.texcoord[2].y, R0, vertex.texcoord[2];\n"
"DP3	result.texcoord[2].z, R0, vertex.texcoord[4];\n"
"\n"
"END\n\x0";


char *fragmentprogram_ambient_parallax_ptr = NULL;
char fragmentprogram_ambient_parallax[] =
"!!ARBfp1.0\n"
"OPTION ARB_precision_hint_fastest;\n"
"\n"
"#\n"
"# on entry:\n"
"#\n"
"#	fragment.texcoord[0]	- stArray.colorMap\n"
"#	fragment.texcoord[1]	- stArray.lightMap\n"
"#	fragment.texcoord[2]	- eyeVec (for parallax)\n"
"#	fragment.texcoord[3]	- stArray.FX\n"
"#\n"
"#	fragment.color		- color\n"
"#\n"
"#	tex0	2D		colorMap\n"
"#	tex1	2D		lightMap\n"
"#	tex2	2D		lightTexture\n"
"#	tex3	2D		FX Texture\n"
"#\n"
"\n"
"TEMP R0, R1, R2, color, height, Toffset;\n"
"\n"
"# perform the parallax\n"
"DP3 R0.w, fragment.texcoord[2], fragment.texcoord[2];\n"
"RSQ R0.w, R0.w;\n"
"MUL R0.xyz, R0.w, fragment.texcoord[2];\n"
"# calculate offset mapping coordinates\n"
"TEX height.w, fragment.texcoord[0], texture[0], 2D;\n"
"MAD height.w, height.w, 0.04, -0.02;\n"
"MAD Toffset, height.w, R0, fragment.texcoord[0];\n"
"\n"
"# get the colorMap\n"
"TEX color, Toffset, texture[0], 2D;\n"
"MUL color, fragment.color, color;\n"
"\n"
"# get the lightMap\n"
"TEX R0, fragment.texcoord[1], texture[1], 2D;\n"
"MUL color, R0, color;\n"
"\n"
"# get the lightTexture\n"
"TEX R0, Toffset, texture[2], 2D;\n"
"ADD color, R0, color;\n"
"\n"
"# get the FX Texture\n"
"TEX R1, fragment.texcoord[3], texture[3], 2D;\n"
"LRP result.color, R0.a, R1, color;\n"
"\n"
"END\n\x0";


char *vertexprogram_nobump_parallax_ptr = NULL;
char vertexprogram_nobump_parallax[] =
"!!ARBvp1.0\n"
"OPTION ARB_position_invariant;\n"
"\n"
"# for ARB6 with noBumpLight and parallax\n"
"# input:\n"
"# vertex.position	- vertexArray		float[3]\n"
"# vertex.texcoord[0]	- stArray		float[2]\n"
"# vertex.texcoord[1]	- binormalArray		float[3]\n"
"# vertex.texcoord[2]	- normalArray		float[3]\n"
"# vertex.texcoord[3]	- tangentArray		float[3]\n"
"# state.matrix.texture[2] - texture matrix for 3D light falloff\n"
"#\n"
"# output:\n"
"# result.texcoord[0]	- stArray	float[2]\n"
"# result.texcoord[2]	- light falloff	float[3]\n"
"# result.texcoord[5]	- eyeVec	float[3]\n"
"\n"
"TEMP	R0;\n"
"PARAM	mvit[4] = {state.matrix.modelview.invtrans};\n"
"\n"
"MOV	result.texcoord[0], vertex.texcoord[0];\n"
"\n"
"# store vertexArray for 3D light falloff\n"
"MOV	R0.xyz, vertex.position;\n"
"MOV	R0.w, 1;\n"
"DP4	result.texcoord[2].x, R0, state.matrix.texture[2].row[0];\n"
"DP4	result.texcoord[2].y, R0, state.matrix.texture[2].row[1];\n"
"DP4	result.texcoord[2].z, R0, state.matrix.texture[2].row[2];\n"
"\n"
"SUB	R0, mvit[3], vertex.position;\n"
"DP3	result.texcoord[5].x, R0, vertex.texcoord[3];\n"
"DP3	result.texcoord[5].y, R0, vertex.texcoord[1];\n"
"DP3	result.texcoord[5].z, R0, vertex.texcoord[2];\n"
"\n"
"END\n\x0";


char *fragmentprogram_nobump_parallax_ptr = NULL;
char fragmentprogram_nobump_parallax[] =
"!!ARBfp1.0\n"
"OPTION ARB_precision_hint_fastest;\n"
"\n"
"#\n"
"# on entry:\n"
"#\n"
"#	fragment.texcoord[0]	- stArray\n"
"#	fragment.texcoord[2]	- light falloff\n"
"#	fragment.texcoord[5]	- eyeVec (for parallax)\n"
"#\n"
"#	program.local[1]	- light color\n"
"#	program.env[0]		- diffuse modifier\n"
"#\n"
"#	tex1	2D		colorMap\n"
"#	tex2	3D		light falloff\n"
"#\n"
"\n"
"TEMP R0, R1, color, height, Toffset;\n"
"\n"
"# Attenuation\n"
"TEX R1, fragment.texcoord[2], texture[2], 3D;\n"
"\n"
"# perform the parallax\n"
"DP3 R0.w, fragment.texcoord[5], fragment.texcoord[5];\n"
"RSQ R0.w, R0.w;\n"
"MUL R0.xyz, R0.w, fragment.texcoord[5];\n"
"# calculate offset mapping coordinates\n"
"TEX height.w, fragment.texcoord[0], texture[1], 2D;\n"
"MAD height.w, height.w, 0.04, -0.02;\n"
"MAD Toffset, height.w, R0, fragment.texcoord[0];\n"
"\n"
"# get the colorMap\n"
"TEX color, Toffset, texture[1], 2D;\n"
"# modulate by Light Color\n"
"MUL color, program.local[1], color;\n"
"# modulate by diffuse modifier\n"
"MUL color.xyz, program.env[0], color;\n"
"# modulate by the light falloff\n"
"MUL result.color, R1, color;\n"
"\n"
"END\n\x0";
