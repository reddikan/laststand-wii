/*
 * LAST STAND - game internals shared by the game, world, draw, art and hud modules.
 */
#ifndef GAME_INT_H
#define GAME_INT_H

#include <stdint.h>
#include <math.h>
#include "game.h"
#include "audio.h"
#include "fastmath.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#define PI_F  3.14159265f
#define TAU_F 6.28318531f

/* ------------------------------------------------------------------ */
/* the shape of a run                                                   */
/* ------------------------------------------------------------------ */
#define NBIOMES      5
#define WAVE_TIME    60.0f          /* each wave lasts a minute ... */
#define NWAVES       10             /* ... ten of them, then the boss at 10:00 */
#define NWEAP        6
#define NSLOTS       4
#define MAX_WLVL     8
#define NCHARS       4
#define NPERKS       4
#define NUPG         9              /* stat upgrades in the shop (the last one is a heal) */
#define MAX_ULVL     6

enum { W_REVOLVER, W_SCATTER, W_GATLING, W_DYNAMITE, W_TESLA, W_RAIL };
enum { UP_VITAL, UP_ARMOR, UP_SPEED, UP_MAGNET, UP_POWER, UP_RATE, UP_LUCK, UP_BACKUP, UP_HEAL };
enum { CH_DRIFTER, CH_PREACHER, CH_GRANNY, CH_DOC };
enum { PK_CASH, PK_STAND, PK_GREED, PK_ARMS };     /* permanent perks bought with souls */

enum { SC_TITLE, SC_ARMORY, SC_BIOMES, SC_INTRO, SC_PLAY, SC_SHOP, SC_PAUSE, SC_CLEAR, SC_OVER, SC_WIN, SC_HELP };

/* run phases inside SC_PLAY */
enum { PH_WAVE, PH_WAVEEND, PH_BOSS, PH_BOSSDEAD };

/* ------------------------------------------------------------------ */
/* enemies                                                              */
/* ------------------------------------------------------------------ */
enum { AI_WALK, AI_FAST, AI_TANK, AI_SHOOT, AI_HOP, AI_FLY, AI_BOSS };
enum { ROLE_WALK, ROLE_FAST, ROLE_TANK, ROLE_SHOOT };

/* art ids: the four enemies of each biome in role order, then the five bosses */
enum {
    A_BANDIT, A_BUZZARD, A_CACTUS, A_SLINGER,
    A_TOAD, A_CROW, A_MOSS, A_LANTERN,
    A_ZOMBIE, A_BONES, A_COFFIN, A_WRAITH,
    A_HUSK, A_IMP, A_HULK, A_DOCTOR,
    A_CRAWLER, A_EYE, A_FLESH, A_PRIEST,
    A_BOSS1, A_BOSS2, A_BOSS3, A_BOSS4, A_BOSS5,
    A_COUNT
};

typedef struct {
    const char *name;
    uint8_t ai;
    float   hp, speed, radius, dmg;
    int     bounty;
    float   mass;
    uint32_t c0, c1;        /* body colours (gibs and splats) */
    uint32_t eye;           /* glowing eye colour */
} EnemyDef;

extern const EnemyDef EDEF[A_COUNT];

enum { EF_SHIELD = 1, EF_FLYING = 2, EF_EXPLODE = 4, EF_PHASE = 8 };

typedef struct {
    float x, y, vx, vy;     /* vx/vy: knockback velocity (decays) */
    float hp, maxhp, r;
    float t;                /* age */
    float cd;               /* attack cooldown */
    float flash;            /* white hit flash */
    float burn;             /* burn damage per second left */
    float burn_t;
    float stun;
    float face;             /* -1 / 1 */
    float mx, my;           /* last movement direction */
    float st;               /* state timer (hops, swoops, boss attacks) */
    float dmgmul;
    int16_t next;           /* spatial grid chain */
    uint8_t type, alive, elite, state, flags, sub;
} Enemy;

/* ------------------------------------------------------------------ */
/* projectiles, effects                                                 */
/* ------------------------------------------------------------------ */
enum { PB_BULLET, PB_PELLET, PB_GATLING, PB_FRAG };

typedef struct {
    float x, y, vx, vy, life, dmg, r;
    float kb;               /* knockback */
    int16_t lasthit;
    uint8_t kind, pierce, weap, crit;
} Bullet;

enum { EB_ORB, EB_BONE, EB_GOO, EB_SKULL, EB_POISON, EB_HOMING, EB_FIRE, EB_VOID };

typedef struct {
    float x, y, vx, vy, life, r, rot;
    float dmg;
    uint8_t kind, alive;
} EBullet;

enum { PT_SPARK, PT_SMOKE, PT_FIRE, PT_CHUNK, PT_GOO, PT_CASING, PT_EMBER, PT_RING, PT_STAR, PT_BONE, PT_DUST, PT_SOUL };

typedef struct {
    float x, y, vx, vy, life, max, size, rot, vr, z, vz;
    uint32_t c0, c1;
    uint8_t kind, add;
} Particle;

typedef struct {
    float x, y, r, t, dur, dmg;
    uint8_t big, kind;      /* kind 0 fire, 1 poison, 2 void, 3 boss shockwave ring (dodge it), 4 lightning */
    uint8_t hurt, hit;      /* hurt: a boss attack that can hit the player (once: hit) */
} Boom;

enum { PU_COIN, PU_GOLD, PU_GEM, PU_HEALTH, PU_MAGNET, PU_NUKE, PU_FRENZY, PU_CHEST };

typedef struct {
    float x, y, vx, vy, t;
    int   val;
    uint8_t kind, pull, alive;
} Pickup;

typedef struct {
    float x, y, t;
    int   val;
    uint8_t crit, kind;     /* kind 0 damage, 1 cash, 2 text */
    const char *txt;
} Number;

typedef struct {
    float x, y, r, life, dps;
    uint8_t enemy;          /* 1 = hurts the player (boss / imp fire) */
} Fire;

enum { BM_ARC, BM_RAIL, BM_LASER_TELL, BM_LASER };

typedef struct {
    float x0, y0, x1, y1, life, max, w;
    uint32_t seed;
    uint8_t kind;
} Beam;

typedef struct {
    float x, y, vx, vy, z, vz, fuse, rot;
    float dmg, rad;
    uint8_t cluster, alive, big, enemy;   /* enemy: falls from the sky onto a marked spot (boss rain) */
} Bomb;

typedef struct {
    float x, y, r, rot, life;
    uint32_t c;
    uint8_t kind;           /* 0 scorch, 1 splat, 2 crater */
} Decal;

typedef struct {
    float x, y, hp;
    uint8_t alive;
} Barrel;

typedef struct { float x, y, r; int cr, cg, cb; } Light;

/* ------------------------------------------------------------------ */
/* the player                                                           */
/* ------------------------------------------------------------------ */
typedef struct {
    float x, y, vx, vy;
    float ax, ay;           /* aim direction (unit) */
    float face;             /* -1 left, 1 right */
    float hp, maxhp;
    float inv;              /* invulnerable time */
    float dash_t, dash_cd, dx, dy;
    float hurt_t;
    float walk;             /* walk cycle */
    float recoil;
    float spin;             /* gatling spin 0..1 */
    float rail_charge;
    int8_t slot[NSLOTS];    /* weapon in each button slot, -1 empty */
    int   sel;              /* selected slot */
    float sel_t;            /* time since a swap (the swap flourish) */
    uint8_t wlvl[NWEAP];    /* 0 = not owned */
    float wcd[NWEAP];       /* fire cooldowns (main hand) */
    float bcd[NWEAP];       /* backup fire cooldowns */
    float side;             /* revolver: alternating barrels */
    uint8_t up[NUPG];
    int   cash, cash_shown, earned;
    int   kills;
    float frenzy, magnet_t, stand_t;
    int   stands;           /* last stands left */
    float tx, ty;           /* auto-aim target */
    int   target;           /* enemy index or -1 */
    float target_t;
    int   character;
} Player;

/* ------------------------------------------------------------------ */
/* save                                                                 */
/* ------------------------------------------------------------------ */
typedef struct {
    uint32_t magic;
    uint8_t  ver, biomes;   /* biomes unlocked (1..5) */
    uint8_t  chr_open[NCHARS], chr_lvl[NCHARS], perk[NPERKS];
    uint8_t  music, sfx, autofire, wins;
    int32_t  souls, best_kills, best_time, runs;
    uint32_t sum;
} Save;

/* ------------------------------------------------------------------ */
/* limits                                                               */
/* ------------------------------------------------------------------ */
#define MAX_EN      320
#define MAX_BUL     520
#define MAX_EB      360
#define MAX_PT      2200
#define MAX_BOOM    48
#define MAX_PU      420
#define MAX_NUM     32
#define MAX_FIRE    40
#define MAX_BEAM    48
#define MAX_BOMB    40
#define MAX_DECAL   140
#define MAX_BARREL  10
#define MAX_LIGHT   140
#define MAX_SND     48

#define GRID_CELL   48.0f
#define GRID_W      40
#define GRID_H      40          /* the grid scrolls with the player: 1920 x 1920 around them */

typedef struct { int16_t id, pitch; float pan, vol; } SndEv;

struct Game {
    int   scene, prev_scene;
    float scene_t, t;
    uint32_t rng;
    Save  sv;
    int   storage_ok, save_pending, save_state;     /* 0 none, 1 ok, 2 failed */
    float save_msg_t;

    /* menus */
    int   menu, armory_sel, biome_sel, shop_sel, pause_sel, help_page;
    int   pick_weapon;      /* shop: weapon waiting for a button (-1) */
    float shop_flash, shop_err;
    int   char_sel;

    /* the run */
    int   biome, wave, phase;
    float run_t;            /* seconds into this biome (the 10:00 clock) */
    float phase_t;
    float spawn_acc, horde_t;
    int   boss;             /* enemy index of the boss, or -1 */
    float boss_hp_shown;
    int   total_kills;
    float total_time;
    int   souls_gained;
    float slowmo, hitstop;
    float chaos;            /* 0..1: how wild it is (music, shake) */
    float combo_t;
    int   combo, best_combo;
    float lightning_t, lightning, thunder_t;
    float wave_banner;
    float announce_t;
    const char *announce;
    uint32_t announce_col;

    Player p;
    float  camx, camy;
    float  shake, shx, shy, zoom_punch, flash, flash_r, flash_g, flash_b, red;
    float  stick_aim;
    int    shop_after;      /* 0: the next wave, 1: the next biome */
    float  over_t;
    /* boss attack state */
    int    bpat, bnext;
    float  laser_a, laser_v, laser_on, spiral_a, boss_tell;
    float  btx, bty;          /* charge / slam target */

    Enemy   en[MAX_EN];      int nen;
    Bullet  bul[MAX_BUL];    int nbul;
    EBullet eb[MAX_EB];      int neb;
    Particle pt[MAX_PT];     int npt;
    Boom    boom[MAX_BOOM];  int nboom;
    Pickup  pu[MAX_PU];      int npu;
    Number  num[MAX_NUM];    int nnum_next;
    Fire    fire[MAX_FIRE];
    Beam    beam[MAX_BEAM];
    Bomb    bomb[MAX_BOMB];
    Decal   dec[MAX_DECAL];  int ndec_next;
    Barrel  bar[MAX_BARREL];
    float   barrel_t;

    int16_t grid[GRID_H][GRID_W];
    float   grid_x0, grid_y0;

    Light   light[MAX_LIGHT]; int nlight;

    SndEv snd[MAX_SND]; int nsnd;
    float snd_cool[SFX_COUNT];
    int   music_epoch, music_pitch;
    float mus[8];

    /* the test harness drives the game with a bot */
    int   bot;
};

/* ------------------------------------------------------------------ */
/* shared helpers                                                       */
/* ------------------------------------------------------------------ */
static inline float frand01(Game *g) { g->rng ^= g->rng << 13; g->rng ^= g->rng >> 17; g->rng ^= g->rng << 5; return (float)(g->rng >> 8) / 16777216.0f; }
static inline float frands(Game *g) { return frand01(g) * 2.0f - 1.0f; }
static inline int   irand(Game *g, int n) { return (int)(frand01(g) * (float)n) % (n > 0 ? n : 1); }
static inline float clampf(float x, float a, float b) { return x < a ? a : (x > b ? b : x); }
static inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
static inline float approach(float x, float target, float d) { return x < target ? (x + d > target ? target : x + d) : (x - d < target ? target : x - d); }
static inline uint32_t hash32(uint32_t x) { x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15; x *= 0x846ca68bu; x ^= x >> 16; return x; }

/* how big an enemy is drawn: bosses loom larger than their hit circle */
static inline float art_scale(const Enemy *e) { return e->r / EDEF[e->type].radius * (EDEF[e->type].ai == AI_BOSS ? 1.5f : 1.0f); }

/* game.c */
void sfx(Game *g, int id, float x, float y, float vol, float pitch_jitter);
void sfx_ui(Game *g, int id);
void game_save_now(Game *g);
int  weapon_cost(const Game *g, int w);
int  upgrade_cost(const Game *g, int u);
float player_damage_mul(const Game *g);
extern const char *const WEAP_NAME[NWEAP];
extern const char *const WEAP_DESC[NWEAP];
extern const char *const UPG_NAME[NUPG];
extern const char *const UPG_DESC[NUPG];
extern const char *const CHAR_NAME[NCHARS];
extern const char *const CHAR_DESC[NCHARS];
extern const char *const PERK_NAME[NPERKS];
extern const char *const PERK_DESC[NPERKS];
extern const char *const BIOME_NAME[NBIOMES];
extern const char *const BIOME_SUB[NBIOMES];
extern const char *const BOSS_NAME[NBIOMES];
int  char_cost(const Game *g, int c);
int  char_lvl_cost(const Game *g, int c);
int  perk_cost(const Game *g, int k);

/* world.c */
void world_start_run(Game *g, int biome, int character);
void world_start_biome(Game *g, int biome);
void world_update(Game *g, const Input *in, float dt);
void world_begin_wave(Game *g);
void world_collect_all(Game *g);
Particle *add_particle(Game *g, int kind, float x, float y, float vx, float vy, float life, float size, uint32_t c0, uint32_t c1, int add);
Boom *add_boom(Game *g, float x, float y, float r, int big, int kind);
void add_shake(Game *g, float amt);
void add_number(Game *g, float x, float y, int val, int crit, int kind);
void add_text(Game *g, float x, float y, const char *txt);
void announce(Game *g, const char *txt, uint32_t col, int voice);
float weapon_rate(const Game *g, int w);
int  find_nearest(Game *g, float x, float y, float maxd);

/* draw.c / art.c / hud.c */
void draw_world(const Game *g);
void draw_lights_build(Game *g);
void art_enemy(const Game *g, const Enemy *e, float sx, float sy);
void art_player(float x, float y, int character, float face, float walk, float ax, float ay, float t, float hurt, float scale);
void art_weapon_icon(int w, float x, float y, float s, float ang, uint32_t tint);
void art_weapon_held(int w, float x, float y, float ang, float s, float spin, float t);
void art_upgrade_icon(int u, float x, float y, float s, float t);
void art_pickup(const Pickup *p, float x, float y, float t);
void art_boss_portrait(int biome, float x, float y, float s, float t);
void art_enemy_preview(int type, float x, float y, float s, float t);
void art_shadow(float x, float y, float rx, float ry, float a);
void art_prop(int biome, int kind, float x, float y, float t, uint32_t seed);
void art_barrel(float x, float y, float t);
int  prop_at(int biome, int cx, int cy, int *kind, float *x, float *y);
int  prop_glow(int biome, int kind, float *ox, float *oy, uint32_t *col, float *r);
void prop_glows(int biome, float wx0, float wy0, float t);
void hud_draw(const Game *g);
void hud_menus(const Game *g);

#endif
