/*
 * LAST STAND - the simulation: the gunslinger, six weapons, the horde, the bosses, the loot and the mess.
 */
#include "game_int.h"
#include "gfx.h"

#include <string.h>
#include <stdio.h>

void hurt_player(Game *g, float dmg, float fx, float fy);
static uint32_t srng = 0x51ED270Bu;
static float frand01_static(void) { srng ^= srng << 13; srng ^= srng >> 17; srng ^= srng << 5; return (float)(srng >> 8) / 16777216.0f; }

/* ------------------------------------------------------------------ */
/* the bestiary                                                         */
/* ------------------------------------------------------------------ */
/*                 name               ai        hp    spd   r     dmg  $   mass  body        body2       eye         blood */
const EnemyDef EDEF[A_COUNT] = {
    /* 1 DUST GULCH */
    { "SKELETON BANDIT", AI_WALK,  22,   54,  11,  10,  1,  1.0f, 0xE8DCC0FFu, 0x8A3A2AFFu, 0xFF4030FFu, 0x6A0C0CFFu },
    { "BUZZARD",         AI_FLY,   12,  104,   9,   7,  1,  0.5f, 0x5A3A28FFu, 0xD8A0A0FFu, 0xFFD040FFu, 0x8A0A0AFFu },
    { "CACTUS GOLEM",    AI_TANK, 130,   34,  18,  18,  4,  4.0f, 0x4E8A3AFFu, 0x2E5A26FFu, 0xFFE070FFu, 0x5A7A20FFu },
    { "BONE SLINGER",    AI_SHOOT, 30,   46,  11,   9,  2,  1.0f, 0xE8DCC0FFu, 0xD86A2AFFu, 0xFF4030FFu, 0x6A0C0CFFu },
    /* 2 ROTWOOD BAYOU */
    { "BOG TOAD",        AI_HOP,   26,   70,  12,  10,  1,  1.2f, 0x4A7A3AFFu, 0x2C4A24FFu, 0xFFD030FFu, 0x7A1A0CFFu },
    { "SWAMP CROW",      AI_FLY,   14,  125,   9,   7,  1,  0.5f, 0x22302AFFu, 0x3A6A4AFFu, 0x60FF90FFu, 0x7A0A0AFFu },
    { "MOSS BRUTE",      AI_TANK, 150,   32,  19,  20,  4,  4.5f, 0x5A6A48FFu, 0x34442CFFu, 0x8CFF60FFu, 0x4A6A1AFFu },
    { "PLAGUE LANTERN",  AI_SHOOT, 32,   44,  11,  10,  2,  1.0f, 0x3A2A5AFFu, 0x1A1424FFu, 0x40FF80FFu, 0x6A0A0AFFu },
    /* 3 GRAVEWATCH */
    { "ROTTING ZOMBIE",  AI_WALK,  30,   48,  11,  12,  1,  1.2f, 0x7A9A6AFFu, 0x3A4A6AFFu, 0xFF3020FFu, 0x4A0A06FFu },
    { "BONE SWARM",      AI_FAST,  12,  120,   8,   7,  1,  0.5f, 0xE8E4D8FFu, 0x6A6460FFu, 0xFF3020FFu, 0x5A0A0AFFu },
    { "COFFIN KNIGHT",   AI_TANK, 170,   36,  18,  20,  5,  5.0f, 0x3A3A44FFu, 0x5A5A68FFu, 0xFF2020FFu, 0x4A0606FFu },
    { "GRAVE WRAITH",    AI_SHOOT, 34,   58,  11,  10,  2,  0.8f, 0xE8F0F4FFu, 0xA8C0D0FFu, 0x40E8FFFFu, 0x5AB0C0FFu },
    /* 4 ASHEN HOLLOW */
    { "CHARRED HUSK",    AI_WALK,  34,   56,  11,  12,  1,  1.2f, 0x2A2220FFu, 0xFF7020FFu, 0xFFA020FFu, 0x5A0A04FFu },
    { "FIRE IMP",        AI_FAST,  14,  128,   9,   8,  1,  0.5f, 0xD83020FFu, 0xFFB030FFu, 0xFFF080FFu, 0x9A1004FFu },
    { "ASH HULK",        AI_TANK, 200,   34,  20,  24,  5,  5.0f, 0x4A4442FFu, 0xFF5018FFu, 0xFF8020FFu, 0x5A0806FFu },
    { "PLAGUE DOCTOR",   AI_SHOOT, 38,   46,  11,  11,  2,  1.0f, 0x1E1A20FFu, 0xE8E0D0FFu, 0x70FF40FFu, 0x5A0A0AFFu },
    /* 5 HELLMOUTH */
    { "SHADOW CRAWLER",  AI_WALK,  40,   62,  12,  13,  1,  1.2f, 0x140E18FFu, 0x3A1A30FFu, 0xFF2040FFu, 0x3A0420FFu },
    { "EYE WISP",        AI_FLY,   18,  130,   9,   9,  1,  0.5f, 0xF0E8E8FFu, 0x9A30FFFFu, 0xFF2030FFu, 0x9A0A1AFFu },
    { "FLESH HULK",      AI_TANK, 230,   36,  21,  26,  6,  5.0f, 0xB04A5AFFu, 0x6A1A2AFFu, 0xFFE040FFu, 0x9A0A14FFu },
    { "VOID PRIEST",     AI_SHOOT, 42,   48,  12,  12,  3,  1.0f, 0x2A1240FFu, 0xB040FFFFu, 0xFF40FFFFu, 0x5A0A3AFFu },
    /* bosses */
    { "THE BONE SHERIFF",   AI_BOSS, 3400, 60, 40, 22, 120, 40.0f, 0xEDE2C8FFu, 0x7A4A2AFFu, 0xFF4020FFu, 0x6A0C0CFFu },
    { "THE BOG MOTHER",     AI_BOSS, 3800, 50, 48, 24, 160, 50.0f, 0x4E7E3CFFu, 0xD8C080FFu, 0xFFD020FFu, 0x6A1A0CFFu },
    { "THE UNDERTAKER",     AI_BOSS, 4200, 56, 42, 26, 200, 50.0f, 0x24222AFFu, 0xD8D4CCFFu, 0xFF2020FFu, 0x4A0606FFu },
    { "THE PLAGUE PROPHET", AI_BOSS, 4600, 54, 46, 28, 240, 50.0f, 0x2A2030FFu, 0xEDE4D2FFu, 0x70FF40FFu, 0x5A0A0AFFu },
    { "THE MAW",            AI_BOSS, 5200, 40, 54, 30, 300, 80.0f, 0x7A1A2AFFu, 0xF0E8E0FFu, 0xFF3020FFu, 0x9A0A14FFu },
};

/* ------------------------------------------------------------------ */
/* small helpers                                                        */
/* ------------------------------------------------------------------ */
static inline float dist2(float ax, float ay, float bx, float by) { float dx = ax - bx, dy = ay - by; return dx * dx + dy * dy; }

static float diff_index(const Game *g) { return (float)(g->biome * NWAVES + (g->wave < NWAVES ? g->wave : NWAVES - 1)); }
static float hp_mul(const Game *g) { float d = diff_index(g); return 1.0f + 0.12f * d + 0.0032f * d * d; }
static float edmg_mul(const Game *g) { return 0.75f + 0.04f * diff_index(g); }
static float bounty_mul(const Game *g) { return 1.0f + 0.3f * (float)g->biome + 0.03f * (float)g->wave; }

void add_shake(Game *g, float amt) { g->shake = clampf(g->shake + amt, 0.0f, 1.0f); }

Particle *add_particle(Game *g, int kind, float x, float y, float vx, float vy, float life, float size, uint32_t c0, uint32_t c1, int add)
{
    static Particle dummy;
    Particle *p;
    /* crowded: thin out the purely decorative bits */
    if (g->npt > MAX_PT * 3 / 4 && (kind == PT_SPARK || kind == PT_SMOKE || kind == PT_DUST || kind == PT_EMBER || kind == PT_GOO) && frand01(g) < 0.5f) return &dummy;
    if (g->npt < MAX_PT) p = &g->pt[g->npt++];
    else p = &g->pt[irand(g, MAX_PT)];             /* full: overwrite a random one */
    p->x = x; p->y = y; p->vx = vx; p->vy = vy; p->life = life; p->max = life; p->size = size;
    p->rot = frand01(g) * TAU_F; p->vr = frands(g) * 8.0f; p->z = 0; p->vz = 0;
    p->c0 = c0; p->c1 = c1; p->kind = (uint8_t)kind; p->add = (uint8_t)add;
    return p;
}

Boom *add_boom(Game *g, float x, float y, float r, int big, int kind)
{
    Boom *b = NULL;
    for (int i = 0; i < MAX_BOOM; i++) if (g->boom[i].t >= g->boom[i].dur) { b = &g->boom[i]; break; }
    if (!b) b = &g->boom[irand(g, MAX_BOOM)];
    memset(b, 0, sizeof *b);
    b->x = x; b->y = y; b->r = r; b->t = 0; b->dur = big ? 0.75f : 0.5f; b->big = (uint8_t)big; b->kind = (uint8_t)kind;
    return b;
}

void add_number(Game *g, float x, float y, int val, int crit, int kind)
{
    Number *n = &g->num[g->nnum_next];
    g->nnum_next = (g->nnum_next + 1) % MAX_NUM;
    n->x = x + frands(g) * 6.0f; n->y = y - 8.0f; n->t = 0; n->val = val; n->crit = (uint8_t)crit; n->kind = (uint8_t)kind; n->txt = NULL;
}

void add_text(Game *g, float x, float y, const char *txt)
{
    Number *n = &g->num[g->nnum_next];
    g->nnum_next = (g->nnum_next + 1) % MAX_NUM;
    n->x = x; n->y = y; n->t = 0; n->val = 0; n->crit = 1; n->kind = 2; n->txt = txt;
}

static void add_decal(Game *g, float x, float y, float r, uint32_t c, int kind)
{
    Decal *d = &g->dec[g->ndec_next];
    g->ndec_next = (g->ndec_next + 1) % MAX_DECAL;
    d->x = x; d->y = y; d->r = r; d->rot = frand01(g) * TAU_F; d->life = kind == DC_SPOT ? 30.0f : 45.0f; d->c = c; d->kind = (uint8_t)kind; d->age = 0;
}

/* ------------------------------------------------------------------ */
/* gore                                                                 */
/* ------------------------------------------------------------------ */
static uint32_t blood_of(int type) { return EDEF[type].blood; }

/* what each monster sounds like going down, and grumbling on its way in */
static int death_voice(int type)
{
    switch (type) {
    case A_BUZZARD: case A_CROW: return SFX_CAW;
    case A_TOAD: return SFX_CROAK;
    case A_ZOMBIE: case A_CACTUS: return frand01_static() < 0.5f ? SFX_GROAN : SFX_GROAN2;
    case A_MOSS: case A_HULK: case A_FLESH: return SFX_GROWL;
    case A_WRAITH: case A_PRIEST: return SFX_WAIL;
    case A_HUSK: case A_CRAWLER: return SFX_HISS;
    case A_EYE: return SFX_GURGLE;
    case A_IMP: case A_BONES: return SFX_SHRIEK;
    default: return SFX_SCREAM;
    }
}
static int chatter_voice(int type)
{
    switch (type) {
    case A_BUZZARD: case A_CROW: return SFX_CAW;
    case A_TOAD: return SFX_CROAK;
    case A_ZOMBIE: return SFX_GROAN;
    case A_CACTUS: case A_MOSS: case A_HULK: case A_FLESH: return SFX_GROWL;
    case A_WRAITH: case A_PRIEST: return SFX_WAIL;
    case A_HUSK: case A_CRAWLER: case A_LANTERN: case A_DOCTOR: return SFX_HISS;
    case A_EYE: return SFX_GURGLE;
    case A_IMP: case A_BONES: return SFX_SHRIEK;
    case A_COFFIN: return SFX_CHAINS;
    default: return SFX_SNARL;
    }
}

/* droplets flying out along (kx, ky); they arc, land and stain the ground */
static void blood_spray(Game *g, float x, float y, float kx, float ky, int n, uint32_t c, float force)
{
    for (int k = 0; k < n; k++) {
        float a = atan2f(ky, kx) + frands(g) * (kx == 0 && ky == 0 ? 3.14f : 0.7f), s = (50.0f + frand01(g) * 170.0f) * force;
        Particle *q = add_particle(g, PT_BLOOD, x + frands(g) * 3, y + frands(g) * 3, cosf(a) * s, sinf(a) * s * 0.7f, 2.0f, 1.2f + frand01(g) * 1.6f, c, col_mix(c, 0x000000FFu, 0.4f), 0);
        q->z = 6.0f + frand01(g) * 8.0f; q->vz = 40.0f + frand01(g) * 120.0f * force;
    }
    add_particle(g, PT_MIST, x, y - 6, kx * 40.0f, ky * 40.0f - 10.0f, 0.35f + frand01(g) * 0.2f, 5.0f + force * 4.0f, col_a(c, 0.55f), c & 0xFFFFFF00u, 0);
}

static void add_corpse(Game *g, const Enemy *e, float kx, float ky)
{
    Corpse *c = &g->corpse[g->ncorpse_next];
    g->ncorpse_next = (g->ncorpse_next + 1) % MAX_CORPSE;
    c->x = e->x + kx * 8.0f; c->y = e->y + ky * 6.0f + e->r * 0.4f; c->s = art_scale(e);
    c->face = e->face;
    float side = (kx * e->face) > 0 ? 1.0f : -1.0f;               /* falls the way it was hit */
    c->rot = side * (1.25f + frand01(g) * 0.4f) * (e->face < 0 ? -1.0f : 1.0f);
    c->life = EDEF[e->type].ai == AI_BOSS ? 60.0f : 26.0f;
    c->seed = (uint32_t)(e->x * 13.0f + e->y * 7.0f);
    c->type = e->type; c->alive = 1; c->headless = 0;
}

static void lens_blood(Game *g, int n)
{
    for (int k = 0; k < n; k++) {
        Lens *l = &g->lens[g->nlens_next];
        g->nlens_next = (g->nlens_next + 1) % MAX_LENS;
        float a = frand01(g) * TAU_F;
        l->x = VW * 0.5f + cosf(a) * VW * (0.25f + frand01(g) * 0.22f);
        l->y = VH * 0.5f + sinf(a) * VH * (0.2f + frand01(g) * 0.25f);
        l->r = 14.0f + frand01(g) * 34.0f; l->t = 0; l->drip = 0; l->seed = g->rng;
    }
}

static void add_fire(Game *g, float x, float y, float r, float life, float dps, int enemy)
{
    Fire *f = NULL;
    float best = 1e9f;
    for (int i = 0; i < MAX_FIRE; i++) {
        if (g->fire[i].life <= 0) { f = &g->fire[i]; break; }
        if (g->fire[i].life < best) { best = g->fire[i].life; f = &g->fire[i]; }
    }
    f->x = x; f->y = y; f->r = r; f->life = life; f->dps = dps; f->enemy = (uint8_t)enemy;
}

static Beam *add_beam(Game *g, int kind, float x0, float y0, float x1, float y1, float life, float w)
{
    Beam *b = NULL;
    for (int i = 0; i < MAX_BEAM; i++) if (g->beam[i].life <= 0) { b = &g->beam[i]; break; }
    if (!b) b = &g->beam[irand(g, MAX_BEAM)];
    b->kind = (uint8_t)kind; b->x0 = x0; b->y0 = y0; b->x1 = x1; b->y1 = y1; b->life = life; b->max = life; b->w = w;
    b->seed = g->rng ^ (uint32_t)(x0 * 13.0f + y1 * 7.0f);
    return b;
}

void announce(Game *g, const char *txt, uint32_t col, int voice)
{
    g->announce = txt; g->announce_t = 0; g->announce_col = col;
    if (voice >= 0) sfx_ui(g, voice);
}

/* ------------------------------------------------------------------ */
/* the spatial grid (rebuilt every frame around the player)             */
/* ------------------------------------------------------------------ */
static void grid_build(Game *g)
{
    g->grid_x0 = g->p.x - GRID_CELL * (float)GRID_W * 0.5f;
    g->grid_y0 = g->p.y - GRID_CELL * (float)GRID_H * 0.5f;
    memset(g->grid, 0xFF, sizeof g->grid);
    for (int i = 0; i < g->nen; i++) {
        Enemy *e = &g->en[i];
        if (!e->alive) continue;
        int cx = (int)((e->x - g->grid_x0) / GRID_CELL), cy = (int)((e->y - g->grid_y0) / GRID_CELL);
        if (cx < 0 || cy < 0 || cx >= GRID_W || cy >= GRID_H) { e->next = -2; continue; }
        e->next = g->grid[cy][cx];
        g->grid[cy][cx] = (int16_t)i;
    }
}

/* calls fn for every live enemy whose centre is within r (+ its radius) of (x,y) */
#define FOR_NEAR(g, X, Y, R, IDX)                                                                     \
    for (int _cy0 = (int)(((Y) - (R) - 24.0f - (g)->grid_y0) / GRID_CELL), _cy1 = (int)(((Y) + (R) + 24.0f - (g)->grid_y0) / GRID_CELL), \
             _cx0 = (int)(((X) - (R) - 24.0f - (g)->grid_x0) / GRID_CELL), _cx1 = (int)(((X) + (R) + 24.0f - (g)->grid_x0) / GRID_CELL), \
             _cy = (_cy0 < 0 ? 0 : _cy0); _cy <= (_cy1 >= GRID_H ? GRID_H - 1 : _cy1); _cy++)     \
        for (int _cx = (_cx0 < 0 ? 0 : _cx0); _cx <= (_cx1 >= GRID_W ? GRID_W - 1 : _cx1); _cx++)   \
            for (int IDX = (g)->grid[_cy][_cx]; IDX >= 0; IDX = (g)->en[IDX].next)

int find_nearest(Game *g, float x, float y, float maxd)
{
    int best = -1;
    float bd = maxd * maxd;
    for (int i = 0; i < g->nen; i++) {
        const Enemy *e = &g->en[i];
        if (!e->alive || e->state == 250) continue;
        float d = dist2(x, y, e->x, e->y);
        if (d < bd) { bd = d; best = i; }
    }
    return best;
}

/* ------------------------------------------------------------------ */
/* weapons: numbers                                                     */
/* ------------------------------------------------------------------ */
float player_damage_mul(const Game *g)
{
    float m = 1.0f + 0.12f * (float)g->p.up[UP_POWER] + 0.08f * (float)g->sv.chr_lvl[g->p.character];
    if (g->p.frenzy > 0) m *= 1.25f;
    return m;
}

/* seconds between shots of weapon w at the current level and upgrades */
float weapon_rate(const Game *g, int w)
{
    int L = g->p.wlvl[w] ? g->p.wlvl[w] : 1;
    float iv;
    switch (w) {
    case W_REVOLVER: iv = 0.17f - 0.013f * (float)(L - 1); break;
    case W_SCATTER:  iv = 0.78f - 0.07f * (float)(L - 1); break;
    case W_GATLING:  iv = 0.075f - 0.0055f * (float)((L < 6 ? L : 6) - 1); break;
    case W_DYNAMITE: iv = 1.05f - 0.09f * (float)(L - 1); break;
    case W_TESLA:    iv = 0.44f - 0.035f * (float)(L - 1); break;
    default:         iv = 1.35f - 0.11f * (float)(L - 1); break;
    }
    iv /= 1.0f + 0.09f * (float)g->p.up[UP_RATE];
    if (g->p.frenzy > 0 || g->p.stand_t > 0) iv *= 0.6f;
    return iv;
}

static int roll_crit(Game *g)
{
    float c = 0.06f + 0.035f * (float)g->p.up[UP_LUCK] + (g->p.character == CH_DRIFTER ? 0.08f : 0.0f);
    return frand01(g) < c;
}

static void spawn_coins(Game *g, float x, float y, int value);
static void kill_enemy(Game *g, int i, float kx, float ky);

/* damage an enemy; returns 1 if it died */
static int hurt_enemy(Game *g, int i, float dmg, int crit, float kx, float ky, float kb)
{
    Enemy *e = &g->en[i];
    if (!e->alive || e->state == 250) return 0;
    if (crit) dmg *= 2.0f;
    if ((e->flags & EF_SHIELD) && (kx != 0 || ky != 0)) {
        /* coffin knights: the lid takes most of a hit from the front */
        float fx = g->p.x - e->x, fy = g->p.y - e->y, l = sqrtf(fx * fx + fy * fy) + 1e-3f;
        if ((-kx * fx - ky * fy) / l > 0.5f) { dmg *= 0.35f; if (g->snd_cool[SFX_SHIELD] <= 0) sfx(g, SFX_SHIELD, e->x, e->y, 0.6f, 0.1f); }
    }
    if ((e->flags & EF_PHASE) && fmodf(e->t, 4.0f) > 3.0f) dmg *= 0.3f;       /* wraiths fading out */
    dmg *= e->dmgmul;
    e->hp -= dmg;
    e->flash = 0.09f;
    e->hurt = 0.3f;
    {
        int bl = 1 + (int)(dmg / 12.0f);
        if (bl > 5) bl = 5;
        if (g->npt < MAX_PT * 3 / 4 || frand01(g) < 0.3f) blood_spray(g, e->x, e->y - e->r * 0.3f, kx, ky, bl, blood_of(e->type), 1.0f);
        if (e->hp - dmg > 0) {
            int w = (int)((1.0f - (e->hp - dmg) / e->maxhp) * 4.6f);
            if (w > 4) w = 4;
            if (w > e->wounds) e->wounds = (uint8_t)w;
        }
    }
    float km = kb / (EDEF[e->type].mass * (e->elite ? 2.5f : 1.0f));
    if (EDEF[e->type].ai == AI_BOSS) km *= 0.1f;
    e->vx += kx * km; e->vy += ky * km;
    if (dmg >= 1.0f && (crit ? frand01(g) < 0.7f : frand01(g) < 0.22f)) add_number(g, e->x, e->y - e->r, (int)(dmg + 0.5f), crit, 0);
    if (g->snd_cool[SFX_HIT] <= 0) {
        static const int H[3] = { SFX_HIT, SFX_HIT2, SFX_HIT3 };
        sfx(g, H[irand(g, 3)], e->x, e->y, 0.5f, 0.25f);
        g->snd_cool[SFX_HIT] = 0.03f;
    }
    if (dmg > 60.0f && g->snd_cool[SFX_SPRAY] <= 0) { sfx(g, SFX_SPRAY, e->x, e->y, 0.6f, 0.2f); g->snd_cool[SFX_SPRAY] = 0.15f; }
    if (EDEF[e->type].ai == AI_BOSS && g->bosshurt_t <= 0) { g->bosshurt_t = 1.4f + frand01(g); sfx(g, frand01(g) < 0.5f ? SFX_GROWL : SFX_SNARL, e->x, e->y, 0.9f, 0.1f); }
    if (e->hp <= 0) { kill_enemy(g, i, kx, ky); return 1; }
    return 0;
}

/* ------------------------------------------------------------------ */
/* explosions                                                           */
/* ------------------------------------------------------------------ */
static void explode(Game *g, float x, float y, float rad, float dmg, int big, int kind, int hurts_player)
{
    add_boom(g, x, y, rad, big, kind);
    uint32_t c0 = kind == 1 ? 0x80FF50FFu : (kind == 2 ? 0xC060FFFFu : 0xFFD070FFu);
    uint32_t c1 = kind == 1 ? 0x206020FFu : (kind == 2 ? 0x401060FFu : 0xFF4010FFu);
    int nf = big ? 16 : 8;
    for (int k = 0; k < nf; k++) {                            /* the fireball */
        float a = frand01(g) * TAU_F, s = (0.3f + frand01(g)) * rad * 2.4f;
        add_particle(g, PT_FIRE, x, y, cosf(a) * s, sinf(a) * s, 0.35f + frand01(g) * 0.35f, rad * (0.25f + frand01(g) * 0.25f), c0, c1, 1);
    }
    int ns = big ? 14 : 7;
    for (int k = 0; k < ns; k++) {                            /* smoke rolling out */
        float a = frand01(g) * TAU_F, s = (0.2f + frand01(g)) * rad * 1.3f;
        add_particle(g, PT_SMOKE, x + cosf(a) * rad * 0.3f, y + sinf(a) * rad * 0.3f, cosf(a) * s, sinf(a) * s - 20.0f,
                     0.9f + frand01(g) * 0.9f, rad * (0.3f + frand01(g) * 0.3f), 0x3A3230C0u, 0x1A1614FFu, 0);
    }
    int nk = big ? 22 : 10;
    for (int k = 0; k < nk; k++) {                            /* sparks and chunks */
        float a = frand01(g) * TAU_F, s = 200.0f + frand01(g) * 420.0f;
        add_particle(g, PT_SPARK, x, y, cosf(a) * s, sinf(a) * s, 0.25f + frand01(g) * 0.4f, 2.5f, 0xFFF0B0FFu, c1, 1);
    }
    for (int k = 0; k < (big ? 8 : 3); k++) {
        float a = frand01(g) * TAU_F, s = 80.0f + frand01(g) * 200.0f;
        add_particle(g, PT_CHUNK, x, y, cosf(a) * s, sinf(a) * s, 0.8f + frand01(g) * 0.6f, 3.0f + frand01(g) * 3.0f, 0x2A2420FFu, 0x5A4A40FFu, 0)->vz = 120.0f + frand01(g) * 200.0f;
    }
    add_decal(g, x, y, rad * 0.75f, kind == 1 ? 0x10301888u : 0x0A080690u, 0);
    add_shake(g, big ? 0.55f : 0.22f);
    g->chaos = clampf(g->chaos + (big ? 0.08f : 0.03f), 0, 1);
    if (big) { g->flash = fmaxf(g->flash, 0.2f); g->flash_r = 1.0f; g->flash_g = 0.75f; g->flash_b = 0.4f; }
    sfx(g, big ? SFX_BOOM_BIG : SFX_BOOM, x, y, big ? 1.0f : 0.75f, 0.12f);
    if (big && g->snd_cool[SFX_TAIKO] <= 0) { sfx(g, SFX_TAIKO, x, y, 0.8f, 0.1f); g->snd_cool[SFX_TAIKO] = 0.2f; }
    if (dmg > 0 && g->snd_cool[SFX_RICOCHET] <= 0 && frand01(g) < 0.4f) { sfx(g, SFX_RICOCHET, x + frands(g) * 80, y, 0.4f, 0.3f); g->snd_cool[SFX_RICOCHET] = 0.12f; }

    if (dmg > 0) {
        FOR_NEAR(g, x, y, rad, i) {
            Enemy *e = &g->en[i];
            float d2 = dist2(x, y, e->x, e->y), rr = rad + e->r;
            if (d2 > rr * rr) continue;
            float d = sqrtf(d2) + 1e-3f, fall = 1.0f - 0.5f * clampf(d / rr, 0, 1);
            g->gibbing = 1;
            hurt_enemy(g, i, dmg * fall, roll_crit(g), (e->x - x) / d, (e->y - y) / d, big ? 520.0f : 300.0f);
            g->gibbing = 0;
        }
        /* chain reaction: powder kegs */
        for (int k = 0; k < MAX_BARREL; k++) {
            Barrel *b = &g->bar[k];
            if (b->alive && dist2(x, y, b->x, b->y) < (rad + 14) * (rad + 14)) b->hp -= dmg;
        }
    }
    if (hurts_player && g->p.inv <= 0 && dist2(x, y, g->p.x, g->p.y) < (rad * 0.8f) * (rad * 0.8f)) hurt_player(g, 8.0f * edmg_mul(g), x, y);
}

/* ------------------------------------------------------------------ */
/* the player taking a hit                                              */
/* ------------------------------------------------------------------ */
static void last_stand(Game *g)
{
    Player *p = &g->p;
    p->stands--;
    p->hp = 1;
    p->stand_t = 5.0f;
    p->inv = 5.0f;
    g->slowmo = 0.9f;
    g->red = 1.0f;
    g->zoom_punch = 0.12f;
    announce(g, "LAST STAND!", 0xFF3A20FFu, SFX_V_LASTSTAND);
    sfx_ui(g, SFX_LASTSTAND);
    add_shake(g, 1.0f);
    /* the shockwave: everything near goes flying */
    add_boom(g, p->x, p->y, 260.0f, 1, 3);
    for (int i = 0; i < g->nen; i++) {
        Enemy *e = &g->en[i];
        if (!e->alive) continue;
        float d = sqrtf(dist2(p->x, p->y, e->x, e->y)) + 1e-3f;
        if (d < 300.0f) hurt_enemy(g, i, 60.0f * hp_mul(g) * 0.5f, 1, (e->x - p->x) / d, (e->y - p->y) / d, 900.0f);
    }
    for (int i = 0; i < g->neb; i++) g->eb[i].alive = 0;
    for (int k = 0; k < 40; k++) {
        float a = (float)k / 40.0f * TAU_F;
        add_particle(g, PT_SPARK, p->x, p->y, cosf(a) * 600.0f, sinf(a) * 600.0f, 0.5f, 3.0f, 0xFFE0A0FFu, 0xFF2010FFu, 1);
    }
}

float dbg_hurt[4]; int dbg_hurt_src;
void hurt_player(Game *g, float dmg, float fx, float fy)
{
    Player *p = &g->p;
    if (p->inv > 0 || p->dash_t > 0 || g->phase == PH_BOSSDEAD || g->phase == PH_WAVEEND) return;
    if (g->bot == 2) dmg *= 0.25f;   /* test harness: the screenshot bot bleeds but doesn't die */
    dbg_hurt[dbg_hurt_src & 3] += dmg;
    dmg *= 1.0f - 0.08f * (float)p->up[UP_ARMOR] - (p->character == CH_GRANNY ? 0.15f : 0.0f);
    p->hp -= dmg;
    if (g->bot == 2 && p->hp < p->maxhp * 0.3f) p->hp = p->maxhp * 0.3f;
    p->inv = 0.7f;
    p->hurt_t = 0.3f;
    float dx = p->x - fx, dy = p->y - fy, l = sqrtf(dx * dx + dy * dy) + 1e-3f;
    p->vx += dx / l * 260.0f; p->vy += dy / l * 260.0f;
    add_shake(g, 0.3f);
    g->red = fmaxf(g->red, 0.55f);
    g->combo = 0;
    sfx(g, SFX_HURT, p->x, p->y, 1.0f, 0.1f);
    blood_spray(g, p->x, p->y - 6, p->x - fx, p->y - fy, 14, 0x8A0A0AFFu, 1.3f);
    add_decal(g, p->x, p->y + 6, 9.0f, 0x6A0606B0u, DC_POOL);
    lens_blood(g, 1 + (dmg > 15.0f) + (p->hp < p->maxhp * 0.35f));
    if (p->hp <= 0) {
        if (p->stands > 0) last_stand(g);
        else p->hp = 0;
    }
}

/* ------------------------------------------------------------------ */
/* loot                                                                 */
/* ------------------------------------------------------------------ */
static Pickup *new_pickup(Game *g, int kind, float x, float y, int val)
{
    Pickup *p = NULL;
    if (g->npu < MAX_PU) p = &g->pu[g->npu++];
    else {
        /* full: fold the value into a random coin instead of losing it */
        Pickup *q = &g->pu[irand(g, MAX_PU)];
        if (q->kind <= PU_GEM && kind <= PU_GEM) { q->val += val; return q; }
        p = q;
    }
    memset(p, 0, sizeof *p);
    float a = frand01(g) * TAU_F, s = 40.0f + frand01(g) * 110.0f;
    p->x = x; p->y = y; p->vx = cosf(a) * s; p->vy = sinf(a) * s; p->kind = (uint8_t)kind; p->val = val; p->alive = 1;
    return p;
}

static void spawn_coins(Game *g, float x, float y, int value)
{
    value = (int)((float)value * 0.42f * (1.0f + 0.1f * (float)g->sv.perk[PK_GREED] + 0.06f * (float)g->p.up[UP_LUCK]) + frand01(g));
    while (value > 0) {
        if (value >= 25) { new_pickup(g, PU_GEM, x, y, 25); value -= 25; }
        else if (value >= 5) { new_pickup(g, PU_GOLD, x, y, 5); value -= 5; }
        else { new_pickup(g, PU_COIN, x, y, 1); value -= 1; }
    }
}

static void gib(Game *g, const Enemy *e, float kx, float ky, int n)
{
    const EnemyDef *d = &EDEF[e->type];
    int bony = e->type == A_BANDIT || e->type == A_SLINGER || e->type == A_BONES || e->type == A_BOSS1;
    for (int k = 0; k < n; k++) {
        float a = frand01(g) * TAU_F, s = 60.0f + frand01(g) * 220.0f;
        float vx = cosf(a) * s + kx * 160.0f, vy = sinf(a) * s + ky * 160.0f;
        if (bony && (k & 1)) {
            add_particle(g, PT_BONE, e->x, e->y, vx, vy, 1.2f + frand01(g), 4.0f + frand01(g) * 3.0f, 0xEDE4D0FFu, 0x9A9080FFu, 0)->vz = 100 + frand01(g) * 160;
        } else if (k % 3 == 0) {
            Particle *q = add_particle(g, PT_GIB, e->x, e->y, vx * 0.8f, vy * 0.6f, 1.6f + frand01(g), 2.0f + frand01(g) * 2.5f, (k % 2) ? d->c0 : d->c1, d->blood, 0);
            q->z = 8; q->vz = 120 + frand01(g) * 180;
        } else {
            add_particle(g, PT_GOO, e->x, e->y, vx, vy, 0.5f + frand01(g) * 0.5f, 2.0f + frand01(g) * 3.0f, (k % 3) ? d->c0 : d->c1, d->c1, 0);
        }
    }
}

static void kill_enemy(Game *g, int i, float kx, float ky)
{
    Enemy *e = &g->en[i];
    const EnemyDef *d = &EDEF[e->type];
    e->alive = 0;
    g->p.kills++;
    g->total_kills++;
    g->combo++;
    g->combo_t = 2.5f;
    if (g->combo > g->best_combo) g->best_combo = g->combo;
    /* the pinball layer: a rising chime every 10 in the combo, a sting and a shout every 50 */
    if (g->combo % 10 == 0) {
        static const int SC[11] = { 0, 2, 4, 7, 9, 12, 14, 16, 19, 21, 24 };
        int k = (g->combo / 10 - 1) % 11;
        snd_pitch(g, SFX_CHIME, 128.0f * powf(2.0f, (float)SC[k] / 12.0f), 0.8f);
    }
    if (g->combo % 50 == 0) {
        static const char *const TXT[] = { "KILLING SPREE!", "RAMPAGE!", "SPLATTER!", "OVERKILL!", "MASSACRE!", "BLOODBATH!", "UNSTOPPABLE!", "GODLIKE!" };
        static const int VO[] = { SFX_V_SPREE, SFX_V_RAMPAGE, SFX_V_SPLATTER, SFX_V_OVERKILL, SFX_V_CARNAGE, SFX_V_BLOODBATH, SFX_V_GODLIKE, SFX_V_GODLIKE };
        int k = g->combo / 50 - 1;
        if (k > 7) k = 7;
        announce(g, TXT[k], 0xFFD040FFu, VO[k]);
        sfx_ui(g, SFX_STING);
        g->vo_t = 2.0f;
    }
    /* multi-kills: a burst of kills inside a breath */
    g->mk_count = g->mk_t > 0 ? g->mk_count + 1 : 1;
    g->mk_t = 0.45f;
    {
        static const int TH[5] = { 6, 12, 22, 36, 60 };
        static const char *const MT[5] = { "MULTI KILL!", "MEGA KILL!", "CARNAGE!", "BLOODBATH!", "GODLIKE!" };
        static const int MV[5] = { SFX_V_MULTI, SFX_V_MEGA, SFX_V_CARNAGE, SFX_V_BLOODBATH, SFX_V_GODLIKE };
        for (int k = 4; k >= 0; k--)
            if (g->mk_count == TH[k] && g->mk_level <= k) {
                g->mk_level = k + 1;
                if (g->vo_t <= 0 || k >= 2) { announce(g, MT[k], 0xFF6030FFu, MV[k]); g->vo_t = 1.5f; }
                sfx_ui(g, k >= 1 ? SFX_TAIKO : SFX_STING);
                break;
            }
    }
    g->chaos = clampf(g->chaos + 0.012f, 0, 1);

    int boss = d->ai == AI_BOSS;
    int pieces = g->gibbing || boss || frand01(g) < 0.25f;      /* blown apart, or left lying there */
    gib(g, e, kx, ky, (boss ? 60 : (e->elite ? 24 : (d->mass > 2 ? 14 : 8))) * (pieces ? 2 : 1) / 2 + 2);
    uint32_t bc = d->blood;
    blood_spray(g, e->x, e->y - e->r * 0.3f, kx, ky, (boss ? 40 : (pieces ? 16 : 9)), bc, pieces ? 1.6f : 1.1f);
    blood_spray(g, e->x, e->y - e->r * 0.3f, 0, 0, pieces ? 6 : 3, bc, 0.6f);
    add_decal(g, e->x, e->y + e->r * 0.3f, e->r * (boss ? 2.6f : (pieces ? 1.6f : 1.2f)), (bc & 0xFFFFFF00u) | 0xC8u, DC_POOL);
    if (kx != 0 || ky != 0) { add_decal(g, e->x + kx * e->r, e->y + ky * e->r, e->r * 1.4f, (bc & 0xFFFFFF00u) | 0xA0u, DC_SMEAR); g->dec[(g->ndec_next + MAX_DECAL - 1) % MAX_DECAL].rot = atan2f(ky, kx); }
    if (!pieces || boss) add_corpse(g, e, kx, ky);
    if (pieces && !boss && frand01(g) < 0.5f && d->ai != AI_FLY && e->type != A_WRAITH && e->type != A_CRAWLER && e->type != A_EYE) {
        Particle *q = add_particle(g, PT_HEAD, e->x, e->y - e->r, kx * 120 + frands(g) * 80, ky * 90 - 40, 3.0f, art_scale(e), d->c0, (uint32_t)e->type, 0);
        q->z = 14; q->vz = 200 + frand01(g) * 120; q->vr = frands(g) * 14; q->rot = e->face;
        if (g->snd_cool[SFX_HEADPOP] <= 0) { sfx(g, SFX_HEADPOP, e->x, e->y, 0.8f, 0.2f); g->snd_cool[SFX_HEADPOP] = 0.2f; }
        if (g->vo_t <= 0 && frand01(g) < 0.12f) { sfx_ui(g, SFX_V_HEADSHOT); g->vo_t = 5.0f; }
    }
    for (int k = 0; k < (boss ? 3 : 1); k++)
        add_particle(g, PT_SOUL, e->x, e->y - 6, frands(g) * 20.0f, -40.0f, 0.8f, e->r * 0.8f, d->eye, 0, 1);

    int coins = (int)((float)d->bounty * bounty_mul(g) * (e->elite ? 6.0f : 1.0f));
    spawn_coins(g, e->x, e->y, coins);
    if (e->elite) new_pickup(g, PU_CHEST, e->x, e->y, 0);
    float r = frand01(g);
    float hpneed = g->p.hp < g->p.maxhp * 0.4f ? 2.0f : 1.0f;
    if (r < 0.010f * hpneed) new_pickup(g, PU_HEALTH, e->x, e->y, 0);
    else if (r < 0.010f * hpneed + 0.0035f) new_pickup(g, PU_MAGNET, e->x, e->y, 0);
    else if (r < 0.010f * hpneed + 0.0055f) new_pickup(g, PU_NUKE, e->x, e->y, 0);
    else if (r < 0.010f * hpneed + 0.0080f) new_pickup(g, PU_FRENZY, e->x, e->y, 0);

    /* it dies loudly: a splat (or gore when it comes apart), a crunch for the bony ones, and its own last noise */
    {
        int bony = e->type == A_BANDIT || e->type == A_SLINGER || e->type == A_BONES || e->type == A_BOSS1;
        if (g->snd_cool[SFX_SPLAT] <= 0) {
            static const int SP[3] = { SFX_SPLAT, SFX_SPLAT2, SFX_SPLAT3 };
            sfx(g, pieces && g->snd_cool[SFX_GORE] <= 0 ? SFX_GORE : SP[irand(g, 3)], e->x, e->y, 0.7f, 0.25f);
            if (pieces) g->snd_cool[SFX_GORE] = 0.09f;
            g->snd_cool[SFX_SPLAT] = 0.035f;
        }
        if (bony && g->snd_cool[SFX_CRUNCH] <= 0) { sfx(g, frand01(g) < 0.5f ? SFX_CRUNCH : SFX_BONES, e->x, e->y, 0.6f, 0.3f); g->snd_cool[SFX_CRUNCH] = 0.05f; }
        if (g->snd_cool[SFX_SCREAM] <= 0) { sfx(g, death_voice(e->type), e->x, e->y, 0.55f, 0.25f); g->snd_cool[SFX_SCREAM] = 0.09f; }
    }
    if (e->flags & EF_EXPLODE) explode(g, e->x, e->y, 46.0f, 22.0f * hp_mul(g), 0, 0, 1);
    if (e->elite) { add_shake(g, 0.3f); sfx(g, SFX_BOOM, e->x, e->y, 0.7f, 0.1f); }

    if (boss) {
        g->phase = PH_BOSSDEAD;
        g->phase_t = 0;
        g->zoom_punch = 0.18f;
        g->slowmo = 2.2f;
        g->flash = 1.0f; g->flash_r = g->flash_g = g->flash_b = 1.0f;
        add_shake(g, 1.0f);
        new_pickup(g, PU_CHEST, e->x, e->y, 0);
        new_pickup(g, PU_CHEST, e->x, e->y, 0);
        sfx_ui(g, SFX_ROAR);
        g->boss = -1;
    }
}

/* ------------------------------------------------------------------ */
/* spawning                                                             */
/* ------------------------------------------------------------------ */
static int new_enemy(Game *g)
{
    for (int i = 0; i < g->nen; i++) if (!g->en[i].alive) return i;
    if (g->nen < MAX_EN) return g->nen++;
    return -1;
}

static int spawn_enemy(Game *g, int type, float x, float y, int elite)
{
    int i = new_enemy(g);
    if (i < 0) return -1;
    Enemy *e = &g->en[i];
    const EnemyDef *d = &EDEF[type];
    memset(e, 0, sizeof *e);
    e->type = (uint8_t)type; e->alive = 1; e->x = x; e->y = y; e->elite = (uint8_t)elite;
    static const float BOSS_K[NBIOMES] = { 3.6f, 4.6f, 6.2f, 7.8f, 9.4f };
    float hm = d->ai == AI_BOSS ? BOSS_K[g->biome] * hp_mul(g) : hp_mul(g);
    e->maxhp = e->hp = d->hp * hm * (elite ? 8.0f : 1.0f);
    e->r = d->radius * (elite ? 1.45f : 1.0f);
    e->face = 1; e->dmgmul = 1.0f;
    e->cd = 1.0f + frand01(g) * 2.0f;
    e->st = frand01(g);
    if (type == A_COFFIN) e->flags |= EF_SHIELD;
    if (type == A_BUZZARD || type == A_CROW || type == A_EYE || type == A_WRAITH) e->flags |= EF_FLYING;
    if (type == A_IMP) e->flags |= EF_EXPLODE;
    if (type == A_WRAITH) e->flags |= EF_PHASE;
    /* a puff where it claws its way in */
    for (int k = 0; k < 3; k++) add_particle(g, PT_DUST, x + frands(g) * 10, y + frands(g) * 6, frands(g) * 30, -20 - frand01(g) * 20, 0.6f, 6.0f, 0x403830A0u, 0, 0);
    return i;
}

static void spawn_ring_point(Game *g, float *x, float *y, float extra)
{
    float a = frand01(g) * TAU_F;
    /* lean towards where the player is heading so they run into trouble */
    float mv = sqrtf(g->p.vx * g->p.vx + g->p.vy * g->p.vy);
    if (mv > 20 && frand01(g) < 0.45f) a = atan2f(g->p.vy, g->p.vx) + frands(g) * 1.1f;
    float rx = VW * 0.36f + 45.0f + extra, ry = VH * 0.36f + 45.0f + extra;      /* just off the (zoomed) screen */
    float c = cosf(a), s = sinf(a);
    /* a point just outside the screen rectangle */
    float k = fminf(rx / (fabsf(c) + 1e-3f), ry / (fabsf(s) + 1e-3f));
    *x = g->p.x + c * k; *y = g->p.y + s * k;
}

static int pick_type(Game *g)
{
    int w = g->wave, base = g->biome * 4;
    float wt[4];
    wt[ROLE_WALK]  = 10.0f;
    wt[ROLE_FAST]  = w >= 1 ? 3.0f + 0.5f * (float)w : 0.0f;
    wt[ROLE_SHOOT] = w >= 2 ? 1.6f + 0.25f * (float)w : 0.0f;
    wt[ROLE_TANK]  = w >= 3 ? 0.8f + 0.3f * (float)w : 0.0f;
    if (g->phase == PH_BOSS) wt[ROLE_TANK] *= 0.5f;
    float sum = wt[0] + wt[1] + wt[2] + wt[3], r = frand01(g) * sum;
    for (int k = 0; k < 4; k++) { if (r < wt[k]) return base + k; r -= wt[k]; }
    return base;
}

static int alive_count(const Game *g)
{
    int n = 0;
    for (int i = 0; i < g->nen; i++) n += g->en[i].alive;
    return n;
}

static void spawn_update(Game *g, float dt)
{
    if (g->phase != PH_WAVE && g->phase != PH_BOSS) return;
    float rate = 1.5f + 0.36f * (float)g->wave + 0.5f * (float)g->biome;
    if (g->phase == PH_BOSS) rate = 1.4f + 0.4f * (float)g->biome;
    /* the first seconds of a wave ramp in */
    float wt = g->run_t - (float)g->wave * WAVE_TIME;
    if (g->phase == PH_WAVE && wt < 6.0f) rate *= 0.4f + 0.1f * wt;
    int cap = 150 + 14 * g->wave + 20 * g->biome;
    if (cap > MAX_EN - 30) cap = MAX_EN - 30;
    if (g->phase == PH_BOSS) cap = cap * 2 / 3;
    g->spawn_acc += rate * dt;
    int alive = alive_count(g);
    while (g->spawn_acc >= 1.0f) {
        g->spawn_acc -= 1.0f;
        if (alive >= cap) { g->spawn_acc = 0; break; }
        float x, y;
        spawn_ring_point(g, &x, &y, frand01(g) * 60.0f);
        int type = pick_type(g);
        int elite = g->wave >= 3 && frand01(g) < 0.0015f + 0.0004f * (float)g->wave;
        /* fast ones come in packs */
        int pack = (type % 4 == ROLE_FAST) ? 2 + irand(g, 2) + (g->biome >= 2) : 1;
        for (int k = 0; k < pack && alive < cap; k++, alive++) spawn_enemy(g, type, x + frands(g) * 30, y + frands(g) * 30, elite && k == 0);
    }
    /* hordes: a ring closes in from every side */
    g->horde_t -= dt;
    if (g->phase == PH_WAVE && g->horde_t <= 0 && g->wave >= 3) {
        g->horde_t = 26.0f + frand01(g) * 14.0f;
        int n = 22 + g->wave * 3 + g->biome * 4;
        if (alive + n > MAX_EN - 10) n = MAX_EN - 10 - alive;
        int type = g->biome * 4 + ((irand(g, 3) == 0) ? ROLE_FAST : ROLE_WALK);
        float gap = frand01(g) * TAU_F;           /* two gaps in the ring to break out through */
        for (int k = 0; k < n; k++) {
            float a = (float)k / (float)(n > 0 ? n : 1) * TAU_F;
            float da = fmodf(a - gap + TAU_F, PI_F);
            if (da < 0.38f) continue;
            spawn_enemy(g, type, g->p.x + cosf(a) * 520.0f, g->p.y + sinf(a) * 380.0f, 0);
        }
        if (n > 0) { announce(g, "HORDE!", 0xFF6040FFu, SFX_V_HORDE); sfx_ui(g, SFX_SCREECH); sfx_ui(g, SFX_TAIKO); }
    }
    /* powder kegs to shoot */
    g->barrel_t -= dt;
    if (g->barrel_t <= 0) {
        g->barrel_t = 5.0f + frand01(g) * 5.0f;
        for (int k = 0; k < MAX_BARREL; k++) {
            Barrel *b = &g->bar[k];
            if (b->alive && dist2(b->x, b->y, g->p.x, g->p.y) < 900.0f * 900.0f) continue;
            float a = frand01(g) * TAU_F, d = 180.0f + frand01(g) * 220.0f;
            b->x = g->p.x + cosf(a) * d; b->y = g->p.y + sinf(a) * d * 0.7f; b->hp = 30.0f; b->alive = 1;
            break;
        }
    }
}

static void spawn_boss(Game *g)
{
    float x = g->p.x, y = g->p.y - 330.0f;
    int i = spawn_enemy(g, A_BOSS1 + g->biome, x, y, 0);
    g->boss = i;
    if (i >= 0) {
        g->boss_hp_shown = 1.0f;
        g->en[i].cd = 2.5f;
        g->en[i].state = 0;
    }
    g->bpat = 0; g->laser_on = 0; g->boss_tell = 0;
    announce(g, BOSS_NAME[g->biome], 0xFF3020FFu, SFX_V_BOSS);
    sfx_ui(g, SFX_SIREN);
    sfx_ui(g, SFX_ROAR);
    add_shake(g, 0.8f);
    add_boom(g, x, y, 160.0f, 1, g->biome >= 3 ? 2 : 3);
}

/* ------------------------------------------------------------------ */
/* enemy projectiles                                                    */
/* ------------------------------------------------------------------ */
static void enemy_shoot(Game *g, int kind, float x, float y, float ang, float speed, float dmg)
{
    EBullet *b = NULL;
    for (int i = 0; i < g->neb; i++) if (!g->eb[i].alive) { b = &g->eb[i]; break; }
    if (!b) { if (g->neb >= MAX_EB) return; b = &g->eb[g->neb++]; }
    memset(b, 0, sizeof *b);
    b->x = x; b->y = y; b->vx = cosf(ang) * speed; b->vy = sinf(ang) * speed; b->kind = (uint8_t)kind; b->alive = 1;
    b->life = 6.0f; b->r = (kind == EB_SKULL || kind == EB_VOID) ? 9.0f : 6.5f; b->dmg = dmg;
}

static int biome_bullet(int biome)
{
    static const int K[NBIOMES] = { EB_BONE, EB_GOO, EB_SKULL, EB_POISON, EB_VOID };
    return K[biome];
}

/* ------------------------------------------------------------------ */
/* boss attacks                                                         */
/* ------------------------------------------------------------------ */
enum { BP_RING, BP_SPIRAL, BP_VOLLEY, BP_CHARGE, BP_SUMMON, BP_SLAM, BP_LASER, BP_RAIN, BP_N };
static const int BOSS_PAT[NBIOMES][6] = {
    { BP_VOLLEY, BP_CHARGE, BP_RING,   BP_SUMMON, BP_VOLLEY, BP_RING },
    { BP_SLAM,   BP_VOLLEY, BP_RAIN,   BP_SUMMON, BP_SLAM,   BP_RING },
    { BP_SPIRAL, BP_SLAM,   BP_SUMMON, BP_RING,   BP_CHARGE, BP_VOLLEY },
    { BP_SPIRAL, BP_RAIN,   BP_CHARGE, BP_VOLLEY, BP_SUMMON, BP_RING },
    { BP_LASER,  BP_SPIRAL, BP_RAIN,   BP_RING,   BP_SUMMON, BP_LASER },
};

static void boss_update(Game *g, Enemy *e, float dt)
{
    Player *p = &g->p;
    float dx = p->x - e->x, dy = p->y - e->y, d = sqrtf(dx * dx + dy * dy) + 1e-3f;
    float ang = atan2f(dy, dx);
    int enraged = e->hp < e->maxhp * 0.5f;
    float spd = EDEF[e->type].speed * (enraged ? 1.25f : 1.0f);
    int kind = biome_bullet(g->biome);
    float bd = 10.0f * edmg_mul(g);
    e->face = dx < 0 ? -1.0f : 1.0f;

    if (e->state == 0) {                          /* stalking between attacks */
        float want = d > 180.0f ? 1.0f : -0.3f;
        e->x += dx / d * spd * want * dt; e->y += dy / d * spd * want * dt;
        e->cd -= dt * (enraged ? 1.4f : 1.0f);
        if (e->cd <= 0) {
            g->bnext = BOSS_PAT[g->biome][g->bpat % 6];
            g->bpat++;
            e->state = (uint8_t)(1 + g->bnext);
            e->st = 0;
            g->spiral_a = ang;
            if (g->bnext == BP_CHARGE || g->bnext == BP_SLAM) { g->btx = p->x; g->bty = p->y; g->boss_tell = 1.0f; sfx(g, SFX_CHARGE, e->x, e->y, 0.9f, 0); }
            if (g->bnext == BP_LASER) { g->laser_a = ang - 1.2f; g->laser_v = (frand01(g) < 0.5f ? 1 : -1) * 0.9f; g->laser_on = 0; sfx(g, SFX_CHARGE, e->x, e->y, 1.0f, 0); }
        }
        return;
    }
    int pat = e->state - 1;
    e->st += dt;
    float t = e->st;
    switch (pat) {
    case BP_RING: {
        int waves = enraged ? 3 : 2;
        int k = (int)(t / 0.5f), fresh = t <= dt || (int)((t - dt) / 0.5f) != k;
        if (k < waves && fresh) {
            int n = 18 + g->biome * 2 + (enraged ? 6 : 0);
            float off = (float)k * 0.17f;
            for (int j = 0; j < n; j++) enemy_shoot(g, kind, e->x, e->y, off + (float)j / (float)n * TAU_F, 150.0f + 10.0f * (float)g->biome, bd);
            sfx(g, SFX_EORB, e->x, e->y, 0.9f, 0.1f);
            add_shake(g, 0.15f);
        }
        if (t > 0.5f * (float)waves + 0.4f) e->state = 0;
        break;
    }
    case BP_SPIRAL: {
        float dur = 3.2f;
        float iv = enraged ? 0.05f : 0.07f;
        int k0 = (int)((t - dt) / iv), k1 = (int)(t / iv);
        for (int k = k0; k < k1; k++) {
            g->spiral_a += 0.33f;
            int arms = 3 + (g->biome >= 3);
            for (int j = 0; j < arms; j++) enemy_shoot(g, kind, e->x, e->y, g->spiral_a + (float)j * TAU_F / (float)arms, 140.0f, bd);
            if (g->snd_cool[SFX_EORB] <= 0) { sfx(g, SFX_EORB, e->x, e->y, 0.5f, 0.1f); g->snd_cool[SFX_EORB] = 0.12f; }
        }
        if (t > dur) e->state = 0;
        break;
    }
    case BP_VOLLEY: {
        int volleys = enraged ? 4 : 3;
        int k = (int)(t / 0.45f);
        if (k < volleys && ((int)((t - dt) / 0.45f) != k || t <= dt)) {
            int n = 5 + 2 * (k & 1) + (g->biome >= 2 ? 2 : 0);
            for (int j = 0; j < n; j++) enemy_shoot(g, kind, e->x, e->y, ang + ((float)j - (float)(n - 1) * 0.5f) * 0.16f, 230.0f, bd);
            sfx(g, g->biome == 0 ? SFX_REVOLVER : SFX_EORB, e->x, e->y, 0.9f, 0.1f);
        }
        if (t > 0.45f * (float)volleys + 0.3f) e->state = 0;
        break;
    }
    case BP_CHARGE: {
        if (t < 0.9f) { g->boss_tell = 1.0f - t / 0.9f; break; }
        if (t < 0.9f + dt * 1.5f) {
            float ax = g->btx - e->x, ay = g->bty - e->y, l = sqrtf(ax * ax + ay * ay) + 1e-3f;
            e->mx = ax / l; e->my = ay / l;
            sfx(g, SFX_ROAR, e->x, e->y, 0.7f, 0.1f);
        }
        float sp = 720.0f;
        e->x += e->mx * sp * dt; e->y += e->my * sp * dt;
        if (frand01(g) < 0.6f) add_particle(g, PT_DUST, e->x, e->y + e->r * 0.6f, frands(g) * 60, -30, 0.7f, 14, 0x50443CA0u, 0, 0);
        if (t > 1.55f) { e->state = 0; e->cd = 1.0f; add_shake(g, 0.3f); }
        e->cd = 1.4f;
        break;
    }
    case BP_SUMMON: {
        if (t <= dt) {
            int n = 6 + g->biome + (enraged ? 4 : 0);
            for (int j = 0; j < n; j++) {
                float a = (float)j / (float)n * TAU_F;
                int ty = g->biome * 4 + (j % 3 == 0 ? ROLE_FAST : ROLE_WALK);
                spawn_enemy(g, ty, e->x + cosf(a) * 80.0f, e->y + sinf(a) * 80.0f, 0);
            }
            sfx(g, SFX_SUMMON, e->x, e->y, 1.0f, 0);
            add_boom(g, e->x, e->y, 110.0f, 0, g->biome >= 3 ? 2 : 3);
        }
        if (t > 1.2f) e->state = 0;
        break;
    }
    case BP_SLAM: {
        /* leap to where the player stood, a ring of force rolls out from the landing */
        float air = 0.9f;
        if (t < air) {
            float k = t / air;
            e->x = lerpf(e->x, g->btx, dt * 4.0f); e->y = lerpf(e->y, g->bty, dt * 4.0f);
            g->boss_tell = 1.0f - k;
            e->sub = (uint8_t)(1.0f + k * 254.0f);
        } else if (e->sub != 0) {
            e->sub = 0;
            Boom *b = add_boom(g, e->x, e->y, 420.0f, 1, 3);
            b->hurt = 1; b->dur = 1.4f; b->dmg = 14.0f * edmg_mul(g);
            sfx(g, SFX_SLAM, e->x, e->y, 1.0f, 0);
            add_shake(g, 0.8f);
            add_decal(g, e->x, e->y, e->r * 1.6f, 0x0A080690u, 2);
            for (int k = 0; k < 24; k++) { float a = (float)k / 24.0f * TAU_F; add_particle(g, PT_DUST, e->x + cosf(a) * e->r, e->y + sinf(a) * e->r, cosf(a) * 260, sinf(a) * 260, 0.8f, 16, 0x5A4C40C0u, 0, 0); }
            if (enraged) for (int j = 0; j < 14; j++) enemy_shoot(g, kind, e->x, e->y, (float)j / 14.0f * TAU_F, 170.0f, bd);
        }
        if (t > air + 0.8f) e->state = 0;
        break;
    }
    case BP_LASER: {
        float tell = 1.1f, fire = enraged ? 2.6f : 2.0f;
        if (t < tell) { g->laser_on = 0.5f; }
        else if (t < tell + fire) {
            if (g->laser_on < 1.0f) { sfx(g, SFX_LASER, e->x, e->y, 1.0f, 0); add_shake(g, 0.4f); }
            g->laser_on = 1.0f;
            g->laser_a += g->laser_v * dt * 1.1f;
            add_shake(g, 0.02f);
            /* does the beam touch the player? */
            float lx = cosf(g->laser_a), ly = sinf(g->laser_a);
            float px = p->x - e->x, py = p->y - e->y, along = px * lx + py * ly, across = fabsf(px * ly - py * lx);
            if (along > 0 && across < 22.0f) hurt_player(g, 16.0f * edmg_mul(g), e->x, e->y);
            if (frand01(g) < 0.8f) {
                float s = 60.0f + frand01(g) * 700.0f;
                add_particle(g, PT_SPARK, e->x + lx * s, e->y + ly * s, frands(g) * 120, frands(g) * 120, 0.3f, 3, 0xFFE0F0FFu, 0xFF2060FFu, 1);
            }
        } else { g->laser_on = 0; e->state = 0; }
        break;
    }
    case BP_RAIN: {
        int drops = 10 + g->biome * 2 + (enraged ? 6 : 0);
        float iv = 1.6f / (float)drops;
        int k0 = (int)((t - dt) / iv), k1 = (int)(t / iv);
        for (int k = k0; k < k1 && k < drops; k++) {
            for (int b = 0; b < MAX_BOMB; b++) if (!g->bomb[b].alive) {
                Bomb *m = &g->bomb[b];
                memset(m, 0, sizeof *m);
                m->alive = 1; m->enemy = 1;
                float a = frand01(g) * TAU_F, r = (k % 3 == 0) ? 0.0f : 40.0f + frand01(g) * 160.0f;
                m->x = p->x + p->vx * 0.6f + cosf(a) * r; m->y = p->y + p->vy * 0.6f + sinf(a) * r;
                m->z = 420.0f; m->vz = -380.0f; m->rad = 54.0f; m->dmg = 10.0f * edmg_mul(g);
                break;
            }
        }
        if (t > 2.2f) e->state = 0;
        break;
    }
    }
    if (e->state == 0) e->cd = (enraged ? 1.2f : 1.9f) - 0.1f * (float)g->biome;
}

/* ------------------------------------------------------------------ */
/* enemies                                                              */
/* ------------------------------------------------------------------ */
static void enemies_update(Game *g, float dt)
{
    Player *p = &g->p;
    int wave_end = g->phase == PH_WAVEEND || g->phase == PH_BOSSDEAD;
    for (int i = 0; i < g->nen; i++) {
        Enemy *e = &g->en[i];
        if (!e->alive) continue;
        const EnemyDef *d = &EDEF[e->type];
        e->t += dt;
        if (e->flash > 0) e->flash -= dt;
        if (e->hurt > 0) e->hurt -= dt;
        if (e->wounds >= 2) {                 /* bleeding as it goes: a trail of drops */
            e->drip -= dt * (float)e->wounds;
            if (e->drip <= 0) { e->drip = 0.5f + frand01(g) * 0.5f; add_decal(g, e->x + frands(g) * 4, e->y + e->r * 0.6f, 1.2f + frand01(g) * 1.6f, (EDEF[e->type].blood & 0xFFFFFF00u) | 0xC0u, DC_SPOT); }
        }

        /* the wave is over: everything left goes up in a cascade */
        if (wave_end && d->ai != AI_BOSS) {
            if (e->state != 250) { e->state = 250; e->st = 0.1f + frand01(g) * 1.6f; }
            e->st -= dt;
            e->flash = 0.05f;
            if (e->st <= 0) {
                e->hp = 0;
                kill_enemy(g, i, 0, -1);
                if (frand01(g) < 0.35f) add_boom(g, e->x, e->y, 34, 0, 0), sfx(g, SFX_BOOM, e->x, e->y, 0.4f, 0.3f);
            }
            continue;
        }
        if (e->burn_t > 0) {
            e->burn_t -= dt;
            e->hp -= e->burn * dt;
            if (frand01(g) < 0.25f) add_particle(g, PT_FIRE, e->x + frands(g) * e->r * 0.6f, e->y - e->r * 0.3f, frands(g) * 10, -60, 0.35f, 5, 0xFFC040FFu, 0xFF3000FFu, 1);
            if (e->hp <= 0) { kill_enemy(g, i, 0, 0); continue; }
        }
        /* knockback */
        e->x += e->vx * dt; e->y += e->vy * dt;
        float fr = expf(-9.0f * dt);
        e->vx *= fr; e->vy *= fr;
        if (e->stun > 0) { e->stun -= dt; continue; }

        if (d->ai == AI_BOSS) {
            float ox = e->x, oy = e->y;
            boss_update(g, e, dt);
            float v = sqrtf((e->x - ox) * (e->x - ox) + (e->y - oy) * (e->y - oy)) / (dt > 0 ? dt : 1);
            e->anim += v * dt * 0.12f;
            e->mvis = approach(e->mvis, clampf(v / 60.0f, 0, 1), dt * 4.0f);
            continue;
        }

        float dx = p->x - e->x, dy = p->y - e->y, dd = sqrtf(dx * dx + dy * dy) + 1e-3f;
        float ux = dx / dd, uy = dy / dd;
        float spd = d->speed * (e->elite ? 0.85f : 1.0f) * (1.0f + 0.012f * (float)(g->wave + g->biome * 3));
        float mx = ux, my = uy;
        switch (d->ai) {
        case AI_FAST: {
            float wob = sinf(e->t * 6.0f + (float)i) * 0.5f;
            mx = ux - uy * wob; my = uy + ux * wob;
            break;
        }
        case AI_SHOOT: {
            float want = 230.0f;
            float k = dd > want + 40 ? 1.0f : (dd < want - 40 ? -0.8f : 0.0f);
            float side = ((i & 1) ? 1.0f : -1.0f) * 0.6f;
            mx = ux * k - uy * side; my = uy * k + ux * side;
            e->cd -= dt;
            if (e->cd <= 0 && dd < 520.0f) {
                e->cd = 2.6f - 0.06f * (float)g->wave + frand01(g) * 1.2f;
                float a = atan2f(dy, dx);
                int kind = g->biome == 0 ? EB_BONE : (g->biome == 4 ? EB_HOMING : (g->biome == 3 ? EB_POISON : EB_ORB));
                int n = (g->wave >= 6 || e->elite) ? 3 : 1;
                for (int k2 = 0; k2 < n; k2++) enemy_shoot(g, kind, e->x, e->y - 4, a + ((float)k2 - (float)(n - 1) * 0.5f) * 0.25f, 165.0f + 8.0f * (float)g->biome, d->dmg * edmg_mul(g));
                if (g->snd_cool[SFX_EORB] <= 0) { sfx(g, g->biome == 0 ? SFX_EBONE : SFX_EORB, e->x, e->y, 0.4f, 0.2f); g->snd_cool[SFX_EORB] = 0.1f; }
            }
            break;
        }
        case AI_HOP: {
            float cyc = fmodf(e->t + e->st, 1.1f);
            float k = cyc < 0.45f ? 0.0f : (cyc < 0.85f ? 2.4f : 0.0f);
            if (cyc >= 0.45f && cyc - dt < 0.45f && g->snd_cool[SFX_HOP] <= 0 && dd < 500) { sfx(g, SFX_HOP, e->x, e->y, 0.3f, 0.3f); g->snd_cool[SFX_HOP] = 0.15f; }
            mx *= k; my *= k;
            break;
        }
        case AI_FLY: {
            /* swoop: circle a little, then dive */
            float cyc = fmodf(e->t + e->st * 3.0f, 3.0f);
            if (cyc < 1.6f) { float s = (i & 1) ? 1.0f : -1.0f; mx = ux * 0.35f - uy * s; my = uy * 0.35f + ux * s; }
            else { mx = ux * 1.5f; my = uy * 1.5f; }
            break;
        }
        case AI_TANK: default: break;
        }
        e->mx = mx; e->my = my;
        e->x += mx * spd * dt; e->y += my * spd * dt;
        {
            float v = sqrtf(mx * mx + my * my) * spd;
            e->anim += v * dt * 0.21f * (d->radius / e->r);
            e->mvis = approach(e->mvis, clampf(v / 55.0f, 0, 1), dt * 5.0f);
        }
        if (fabsf(dx) > 2) e->face = dx < 0 ? -1.0f : 1.0f;

        /* touching the player */
        float rr = e->r + 9.0f;
        if (dd < rr) {
            if (p->inv <= 0 && p->dash_t <= 0) { dbg_hurt_src = 0; hurt_player(g, d->dmg * edmg_mul(g) * (e->elite ? 1.5f : 1.0f), e->x, e->y); dbg_hurt_src = 2; }
            e->vx -= ux * 160.0f; e->vy -= uy * 160.0f;
        }
        /* too far behind: come round again from ahead */
        if (dd > 1150.0f) { float x, y; spawn_ring_point(g, &x, &y, 40.0f); e->x = x; e->y = y; }
        if (e->type == A_HUSK && frand01(g) < 0.05f) add_particle(g, PT_EMBER, e->x, e->y - 6, frands(g) * 20, -40, 0.8f, 2, 0xFF8020FFu, 0xFF2000FFu, 1);
    }

    /* separation: shove overlapping enemies apart (cheap, via the grid) */
    for (int cy = 0; cy < GRID_H; cy++)
        for (int cx = 0; cx < GRID_W; cx++)
            for (int i = g->grid[cy][cx]; i >= 0; i = g->en[i].next) {
                Enemy *a = &g->en[i];
                if (!a->alive) continue;
                for (int ny = cy; ny <= cy + 1 && ny < GRID_H; ny++)
                    for (int nx = cx - 1; nx <= cx + 1; nx++) {
                        if (nx < 0 || nx >= GRID_W || (ny == cy && nx < cx)) continue;
                        for (int j = g->grid[ny][nx]; j >= 0; j = g->en[j].next) {
                            if ((ny == cy && nx == cx) && j <= i) continue;
                            Enemy *b = &g->en[j];
                            if (!b->alive) continue;
                            if ((a->flags ^ b->flags) & EF_FLYING) continue;
                            float dx = b->x - a->x, dy = b->y - a->y, rr = (a->r + b->r) * 0.9f, d2 = dx * dx + dy * dy;
                            if (d2 >= rr * rr || d2 < 1e-4f) continue;
                            float dd = sqrtf(d2), push = (rr - dd) * 0.5f;
                            float ma = EDEF[a->type].mass * (a->elite ? 3 : 1), mb = EDEF[b->type].mass * (b->elite ? 3 : 1);
                            float ka = mb / (ma + mb), kb = ma / (ma + mb);
                            dx /= dd; dy /= dd;
                            a->x -= dx * push * ka * 1.6f; a->y -= dy * push * ka * 1.6f;
                            b->x += dx * push * kb * 1.6f; b->y += dy * push * kb * 1.6f;
                        }
                    }
            }
}

/* ------------------------------------------------------------------ */
/* firing the guns                                                      */
/* ------------------------------------------------------------------ */
static Bullet *new_bullet(Game *g)
{
    if (g->nbul < MAX_BUL) return &g->bul[g->nbul++];
    return &g->bul[irand(g, MAX_BUL)];
}

static void muzzle(Game *g, float x, float y, float ax, float ay, float size, uint32_t c)
{
    add_particle(g, PT_STAR, x, y, ax * 30.0f, ay * 30.0f, 0.06f, size, 0xFFFFE0FFu, c, 1);
    for (int k = 0; k < 2; k++)
        add_particle(g, PT_SPARK, x, y, ax * (200 + frand01(g) * 200) + frands(g) * 80, ay * (200 + frand01(g) * 200) + frands(g) * 80, 0.12f, 2.0f, 0xFFF0C0FFu, c, 1);
}

static void eject_casing(Game *g, float x, float y, float ax, float ay)
{
    float s = frand01(g) < 0.5f ? 1.0f : -1.0f;
    add_particle(g, PT_CASING, x, y, -ay * s * 90.0f + frands(g) * 30, ax * s * 90.0f - 30.0f, 0.9f, 2.0f, 0xE8B048FFu, 0xA06A20FFu, 0)->vz = 90.0f + frand01(g) * 60.0f;
}

static void fire_bullet(Game *g, int w, int kind, float x, float y, float ang, float speed, float dmg, int pierce, float life, float r, float kb)
{
    Bullet *b = new_bullet(g);
    memset(b, 0, sizeof *b);
    b->x = x; b->y = y; b->vx = cosf(ang) * speed; b->vy = sinf(ang) * speed;
    b->life = life; b->dmg = dmg; b->r = r; b->kb = kb; b->kind = (uint8_t)kind; b->pierce = (uint8_t)pierce; b->weap = (uint8_t)w;
    b->lasthit = -1; b->crit = (uint8_t)roll_crit(g);
}

static void throw_dynamite(Game *g, float x, float y, float tx, float ty, float dmg, float rad, int cluster, int big)
{
    for (int i = 0; i < MAX_BOMB; i++) {
        Bomb *m = &g->bomb[i];
        if (m->alive) continue;
        memset(m, 0, sizeof *m);
        float dx = tx - x, dy = ty - y, d = sqrtf(dx * dx + dy * dy) + 1e-3f;
        if (d > 320.0f) { dx *= 320.0f / d; dy *= 320.0f / d; d = 320.0f; }
        if (d < 70.0f) { dx *= 70.0f / d; dy *= 70.0f / d; d = 70.0f; }
        float tflight = 0.5f;
        m->x = x; m->y = y; m->vx = dx / tflight; m->vy = dy / tflight; m->z = 10.0f; m->vz = 0.5f * 900.0f * tflight;
        m->fuse = tflight + 0.12f; m->dmg = dmg; m->rad = rad; m->cluster = (uint8_t)cluster; m->alive = 1; m->big = (uint8_t)big;
        return;
    }
}

static void zap(Game *g, float x, float y, float ax, float ay, int L, float dmg)
{
    /* the first victim: in the aim cone if anything is there, else the nearest */
    int first = -1;
    float best = 1e9f;
    for (int i = 0; i < g->nen; i++) {
        Enemy *e = &g->en[i];
        if (!e->alive || e->state == 250) continue;
        float dx = e->x - x, dy = e->y - y, d2 = dx * dx + dy * dy;
        if (d2 > 330.0f * 330.0f) continue;
        float d = sqrtf(d2) + 1e-3f, cs = (dx * ax + dy * ay) / d;
        float score = d * (cs > 0.55f ? 1.0f : 2.6f);
        if (score < best) { best = score; first = i; }
    }
    float px = x + ax * 140.0f, py = y + ay * 140.0f;
    if (first < 0) {                                         /* a harmless crackle into the air */
        add_beam(g, BM_ARC, x, y, px + frands(g) * 30, py + frands(g) * 30, 0.12f, 2.0f);
        return;
    }
    int chain = 3 + L, cur = first;
    static uint8_t hit[MAX_EN];
    memset(hit, 0, sizeof hit);
    float sx = x, sy = y;
    for (int k = 0; k <= chain && cur >= 0; k++) {
        Enemy *e = &g->en[cur];
        add_beam(g, BM_ARC, sx, sy, e->x, e->y, 0.16f, k == 0 ? 3.5f : 2.5f);
        for (int s = 0; s < 3; s++) add_particle(g, PT_SPARK, e->x, e->y, frands(g) * 200, frands(g) * 200, 0.2f, 2.0f, 0xE0FFFFFFu, 0x2080FFFFu, 1);
        hit[cur] = 1;
        float ex = e->x, ey = e->y;
        float kx = ex - sx, ky = ey - sy, kl = sqrtf(kx * kx + ky * ky) + 1e-3f;
        if (!hurt_enemy(g, cur, dmg * (1.0f - 0.06f * (float)k), roll_crit(g), kx / kl, ky / kl, 60.0f)) g->en[cur].stun = fmaxf(g->en[cur].stun, 0.12f);
        sx = ex; sy = ey;
        int next = -1;
        float bd = 150.0f * 150.0f;
        FOR_NEAR(g, ex, ey, 150.0f, j) {
            if (hit[j] || !g->en[j].alive || g->en[j].state == 250) continue;
            float d2 = dist2(ex, ey, g->en[j].x, g->en[j].y);
            if (d2 < bd) { bd = d2; next = j; }
        }
        cur = next;
    }
}

static void rail(Game *g, float x, float y, float ang, float dmg, float w)
{
    float lx = cosf(ang), ly = sinf(ang), len = 780.0f;
    add_beam(g, BM_RAIL, x, y, x + lx * len, y + ly * len, 0.32f, w);
    for (int i = 0; i < g->nen; i++) {
        Enemy *e = &g->en[i];
        if (!e->alive || e->state == 250) continue;
        float px = e->x - x, py = e->y - y, along = px * lx + py * ly;
        if (along < 0 || along > len) continue;
        float across = fabsf(px * ly - py * lx);
        if (across < w * 0.5f + e->r) hurt_enemy(g, i, dmg, roll_crit(g), lx, ly, 420.0f);
    }
    for (int k = 0; k < MAX_BARREL; k++) {
        Barrel *b = &g->bar[k];
        if (!b->alive) continue;
        float px = b->x - x, py = b->y - y, along = px * lx + py * ly;
        if (along > 0 && along < len && fabsf(px * ly - py * lx) < w * 0.5f + 14) b->hp = 0;
    }
    for (int k = 0; k < 18; k++) {
        float s = frand01(g) * len;
        add_particle(g, PT_SPARK, x + lx * s, y + ly * s, -ly * frands(g) * 160 + lx * 60, lx * frands(g) * 160 + ly * 60, 0.35f, 2.5f, 0xFFE8FFFFu, 0xFF30C0FFu, 1);
    }
    add_shake(g, 0.22f);
    g->chaos = clampf(g->chaos + 0.02f, 0, 1);
}

/* fires weapon w once from (x,y) towards ang. main: the weapon in hand (louder, recoil, casings) */
static void fire_weapon(Game *g, int w, float x, float y, float ang, int main)
{
    Player *p = &g->p;
    int L = p->wlvl[w];
    float dm = player_damage_mul(g);
    float ax = cosf(ang), ay = sinf(ang);
    float vol = main ? 0.8f : 0.45f;
    switch (w) {
    case W_REVOLVER: {
        float dmg = (13.0f + 4.5f * (float)(L - 1)) * dm;
        int pierce = 1 + (L >= 3) + (L >= 5) + (L >= 7);
        float side = main ? (p->side = -p->side) : 0.0f;
        float ox = x - ay * side * 6.0f, oy = y + ax * side * 6.0f;
        fire_bullet(g, w, PB_BULLET, ox, oy, ang + frands(g) * 0.03f, 780.0f, dmg, pierce, 0.85f, 4.0f, 90.0f);
        if (L >= 4) { fire_bullet(g, w, PB_BULLET, ox, oy, ang + 0.13f, 760.0f, dmg * 0.7f, pierce, 0.7f, 3.5f, 60.0f); fire_bullet(g, w, PB_BULLET, ox, oy, ang - 0.13f, 760.0f, dmg * 0.7f, pierce, 0.7f, 3.5f, 60.0f); }
        muzzle(g, ox + ax * 10, oy + ay * 10, ax, ay, 14.0f, 0xFFA020FFu);
        eject_casing(g, ox, oy, ax, ay);
        if (g->snd_cool[SFX_REVOLVER] <= 0) { sfx(g, SFX_REVOLVER, x, y, vol, 0.12f); g->snd_cool[SFX_REVOLVER] = 0.05f; }
        if (main) p->recoil = 0.5f;
        break;
    }
    case W_SCATTER: {
        int n = 7 + 2 * ((L < 6 ? L : 6) - 1);
        float dmg = (8.0f + 2.2f * (float)(L - 1)) * dm;
        for (int k = 0; k < n; k++) {
            float a = ang + ((float)k / (float)(n - 1) - 0.5f) * 0.6f + frands(g) * 0.05f;
            fire_bullet(g, w, PB_PELLET, x + ax * 12, y + ay * 12, a, 520.0f + frand01(g) * 180.0f, dmg, 1 + (L >= 4), 0.36f + frand01(g) * 0.08f, 4.5f, 160.0f);
        }
        if (L >= 3) add_fire(g, x + ax * 150.0f, y + ay * 150.0f, 26.0f + 4.0f * (float)L, 1.6f, (5.0f + 3.0f * (float)L) * dm, 0);
        muzzle(g, x + ax * 16, y + ay * 16, ax, ay, 26.0f, 0xFF6010FFu);
        for (int k = 0; k < 8; k++) add_particle(g, PT_FIRE, x + ax * 16, y + ay * 16, ax * (200 + frand01(g) * 250) + frands(g) * 90, ay * (200 + frand01(g) * 250) + frands(g) * 90, 0.25f, 9.0f, 0xFFD060FFu, 0xFF2000FFu, 1);
        add_particle(g, PT_SMOKE, x + ax * 20, y + ay * 20, ax * 60, ay * 60 - 20, 0.8f, 12, 0x50484090u, 0x20202000u, 0);
        sfx(g, SFX_SCATTER, x, y, vol + 0.15f, 0.08f);
        if (main) { p->recoil = 1.0f; p->vx -= ax * 140.0f; p->vy -= ay * 140.0f; add_shake(g, 0.12f); }
        break;
    }
    case W_GATLING: {
        float dmg = (6.0f + 2.0f * (float)(L - 1)) * dm;
        int pierce = 1 + (L >= 4);
        int barrels = L >= 5 ? 2 : 1;
        for (int k = 0; k < barrels; k++) {
            float off = barrels > 1 ? (k ? 5.0f : -5.0f) : 0.0f;
            fire_bullet(g, w, PB_GATLING, x - ay * off, y + ax * off, ang + frands(g) * 0.1f, 920.0f, dmg, pierce, 0.7f, 3.0f, 45.0f);
        }
        muzzle(g, x + ax * 18, y + ay * 18, ax, ay, 12.0f, 0xFFD040FFu);
        eject_casing(g, x, y, ax, ay);
        if (g->snd_cool[SFX_GATLING] <= 0) { sfx(g, SFX_GATLING, x, y, vol, 0.08f); g->snd_cool[SFX_GATLING] = 0.06f; }
        if (main) { p->recoil = 0.3f; add_shake(g, 0.015f); }
        break;
    }
    case W_DYNAMITE: {
        float dmg = (42.0f + 15.0f * (float)(L - 1)) * dm;
        float rad = 64.0f + 9.0f * (float)L;
        float tx = x + ax * 220.0f, ty = y + ay * 220.0f;
        int t = p->target;
        if (t >= 0 && g->en[t].alive) { tx = g->en[t].x; ty = g->en[t].y; if (main && g->stick_aim > 0.3f) { tx = x + ax * 240.0f; ty = y + ay * 240.0f; } }
        throw_dynamite(g, x, y, tx, ty, dmg, rad, L >= 3, L >= 4);
        if (L >= 5) throw_dynamite(g, x, y, tx + frands(g) * 90, ty + frands(g) * 90, dmg * 0.8f, rad * 0.85f, 1, 0);
        sfx(g, SFX_THROW, x, y, vol, 0.1f);
        break;
    }
    case W_TESLA: {
        float dmg = (16.0f + 5.5f * (float)(L - 1)) * dm;
        zap(g, x + ax * 12, y + ay * 12, ax, ay, L, dmg);
        if (g->snd_cool[SFX_TESLA] <= 0) { sfx(g, SFX_TESLA, x, y, vol, 0.15f); g->snd_cool[SFX_TESLA] = 0.08f; }
        break;
    }
    case W_RAIL: {
        float dmg = (72.0f + 32.0f * (float)(L - 1)) * dm;
        float wd = 12.0f + 2.5f * (float)L;
        rail(g, x + ax * 14, y + ay * 14, ang, dmg, wd);
        if (L >= 3) { rail(g, x + ax * 14, y + ay * 14, ang + 0.11f, dmg * 0.6f, wd * 0.7f); rail(g, x + ax * 14, y + ay * 14, ang - 0.11f, dmg * 0.6f, wd * 0.7f); }
        if (L >= 5) { rail(g, x, y, ang + 0.24f, dmg * 0.5f, wd * 0.6f); rail(g, x, y, ang - 0.24f, dmg * 0.5f, wd * 0.6f); }
        muzzle(g, x + ax * 20, y + ay * 20, ax, ay, 30.0f, 0xFF40E0FFu);
        sfx(g, SFX_RAIL, x, y, vol + 0.2f, 0.06f);
        if (main) { p->recoil = 1.0f; p->vx -= ax * 220.0f; p->vy -= ay * 220.0f; }
        break;
    }
    }
}

/* where the backup gun in slot s floats round the player */
static void backup_pos(const Game *g, int s, float *x, float *y)
{
    float a = g->t * 1.6f + (float)s * TAU_F / 4.0f;
    *x = g->p.x + cosf(a) * 30.0f; *y = g->p.y - 6.0f + sinf(a) * 22.0f;
}

static void weapons_update(Game *g, const Input *in, float dt)
{
    Player *p = &g->p;
    int firing = (in->held & (BTN_R | BTN_ZR)) || g->sv.autofire || p->stand_t > 0;
    if (g->phase == PH_WAVEEND || g->phase == PH_BOSSDEAD) firing = 0;
    int mw = p->slot[p->sel];
    p->firing = firing && mw >= 0;
    p->shoot_t = p->firing ? 0.0f : p->shoot_t + dt;

    /* gatling spin */
    int spinning = firing && mw == W_GATLING;
    if (spinning && p->spin < 0.05f) sfx(g, SFX_SPINUP, p->x, p->y, 0.6f, 0);
    p->spin = approach(p->spin, spinning ? 1.0f : 0.0f, dt * (spinning ? 1.7f : 1.0f));

    float aim = atan2f(p->ay, p->ax);
    float hx = p->x + p->ax * 10.0f, hy = p->y - 4.0f + p->ay * 8.0f;
    for (int w = 0; w < NWEAP; w++) { if (p->wcd[w] > 0) p->wcd[w] -= dt; if (p->bcd[w] > 0) p->bcd[w] -= dt; }

    if (mw >= 0 && firing) {
        float iv = weapon_rate(g, mw);
        if (mw == W_GATLING) { if (p->spin < 0.25f) iv = 1e9f; else iv /= p->spin; }
        int guard = 0;
        while (p->wcd[mw] <= 0 && guard++ < 4) {
            fire_weapon(g, mw, hx, hy, aim, 1);
            p->wcd[mw] += iv;
        }
        if (p->wcd[mw] < -0.1f) p->wcd[mw] = 0;
    }

    /* backups: the other guns orbiting the player fire on their own at the nearest thing */
    float bmul = 0.30f + 0.08f * (float)p->up[UP_BACKUP] + (g->sv.perk[PK_ARMS] ? 0.05f * (float)g->sv.perk[PK_ARMS] : 0.0f);
    if (p->frenzy > 0 || p->stand_t > 0) bmul = 1.0f;
    int t = p->target;
    if (t >= 0 && g->phase != PH_WAVEEND && g->phase != PH_BOSSDEAD) {
        for (int s = 0; s < NSLOTS; s++) {
            int w = p->slot[s];
            if (w < 0 || (s == p->sel && firing)) continue;
            if (s == p->sel && !g->sv.autofire && p->stand_t <= 0 && p->frenzy <= 0) { /* in hand but holstered: rests */ continue; }
            if (p->bcd[w] > 0) continue;
            float bx, by;
            backup_pos(g, s, &bx, &by);
            float ex = g->en[t].x, ey = g->en[t].y;
            if (dist2(bx, by, ex, ey) > 480.0f * 480.0f) continue;
            float iv = weapon_rate(g, w) / bmul;
            if (w == W_GATLING) iv *= 1.4f;
            fire_weapon(g, w, bx, by, atan2f(ey - by, ex - bx), 0);
            p->bcd[w] = iv * (0.9f + frand01(g) * 0.2f);
        }
    }
}

/* ------------------------------------------------------------------ */
/* the player                                                           */
/* ------------------------------------------------------------------ */
static void player_update(Game *g, const Input *in, float dt)
{
    Player *p = &g->p;
    if (p->inv > 0) p->inv -= dt;
    if (p->hurt_t > 0) p->hurt_t -= dt;
    if (p->recoil > 0) p->recoil = fmaxf(0, p->recoil - dt * 6.0f);
    if (p->frenzy > 0) p->frenzy -= dt;
    if (p->magnet_t > 0) p->magnet_t -= dt;
    p->sel_t += dt;
    if (p->stand_t > 0) {
        p->stand_t -= dt;
        if (p->stand_t <= 0) { p->hp = fmaxf(p->hp, p->maxhp * 0.4f); add_text(g, p->x, p->y - 30, "BACK FROM THE GRAVE"); }
    }

    /* choose the weapon: A B X Y */
    static const uint32_t SB[NSLOTS] = { BTN_A, BTN_B, BTN_X, BTN_Y };
    for (int s = 0; s < NSLOTS; s++)
        if ((in->pressed & SB[s]) && p->slot[s] >= 0 && s != p->sel) {
            p->sel = s; p->sel_t = 0;
            sfx(g, SFX_SWAP, p->x, p->y, 0.7f, 0.05f);
            add_text(g, p->x, p->y - 34, WEAP_NAME[p->slot[s]]);
        }

    /* movement */
    float sx = in->sx, sy = in->sy;
    if (in->held & BTN_LEFT) sx = -1; if (in->held & BTN_RIGHT) sx = 1;
    if (in->held & BTN_UP) sy = -1;   if (in->held & BTN_DOWN) sy = 1;
    float m = sqrtf(sx * sx + sy * sy);
    if (m > 1) { sx /= m; sy /= m; m = 1; }
    float speed = 150.0f * (1.0f + 0.07f * (float)p->up[UP_SPEED]) * (p->character == CH_GRANNY ? 0.9f : 1.0f);
    if (p->slot[p->sel] == W_GATLING && p->spin > 0.5f) speed *= 0.8f;     /* the gatling is heavy */

    if (p->dash_cd > 0) p->dash_cd -= dt;
    if ((in->pressed & (BTN_L | BTN_ZL)) && p->dash_cd <= 0) {
        float dx = m > 0.1f ? sx : p->ax, dy = m > 0.1f ? sy : p->ay, l = sqrtf(dx * dx + dy * dy) + 1e-3f;
        p->dx = dx / l; p->dy = dy / l;
        p->dash_t = 0.2f; p->dash_cd = 1.1f;
        sfx(g, SFX_DASH, p->x, p->y, 0.8f, 0.05f);
        sfx(g, SFX_WHOOSH, p->x, p->y, 0.6f, 0.1f);
    }
    if (p->dash_t > 0) {
        p->dash_t -= dt;
        p->x += p->dx * 640.0f * dt; p->y += p->dy * 640.0f * dt;
        add_particle(g, PT_RING, p->x, p->y - 8, 0, 0, 0.25f, 1.0f, 0xFFB060A0u, 0, 1);    /* after-image */
    }
    float tvx = sx * speed, tvy = sy * speed;
    p->vx = lerpf(p->vx, tvx, clampf(dt * 14.0f, 0, 1)); p->vy = lerpf(p->vy, tvy, clampf(dt * 14.0f, 0, 1));
    p->x += p->vx * dt; p->y += p->vy * dt;
    if (m > 0.1f) {
        p->walk += dt * 9.0f * m;
        if (fmodf(p->walk, 3.14159f) < dt * 9.0f * m) add_particle(g, PT_DUST, p->x, p->y + 8, -p->vx * 0.1f, -10, 0.5f, 4.0f, 0x6050409Au, 0, 0);
    }

    /* aim: the right stick if it is held, otherwise the nearest monster */
    p->target_t -= dt;
    if (p->target_t <= 0 || p->target < 0 || !g->en[p->target].alive) {
        p->target = find_nearest(g, p->x, p->y, 420.0f);
        /* the boss is the priority unless something is right on top of us */
        if (g->boss >= 0 && g->en[g->boss].alive) {
            const Enemy *b = &g->en[g->boss];
            float db = sqrtf((b->x - p->x) * (b->x - p->x) + (b->y - p->y) * (b->y - p->y));
            float dn = p->target >= 0 ? sqrtf((g->en[p->target].x - p->x) * (g->en[p->target].x - p->x) + (g->en[p->target].y - p->y) * (g->en[p->target].y - p->y)) : 1e9f;
            if (db < 520.0f && dn > 110.0f) p->target = g->boss;
        }
        p->target_t = 0.12f;
    }
    float rm = sqrtf(in->rx * in->rx + in->ry * in->ry);
    g->stick_aim = rm;
    float tx = p->ax, ty = p->ay;
    if (rm > 0.3f) { tx = in->rx / rm; ty = in->ry / rm; }
    else if (p->target >= 0) {
        Enemy *e = &g->en[p->target];
        float dx = e->x + e->vx * 0.05f - p->x, dy = e->y - p->y, l = sqrtf(dx * dx + dy * dy) + 1e-3f;
        tx = dx / l; ty = dy / l;
    } else if (m > 0.2f) { tx = sx / m; ty = sy / m; }
    /* turn smoothly (fast) */
    float a0 = atan2f(p->ay, p->ax), a1 = atan2f(ty, tx), da = a1 - a0;
    while (da > PI_F) da -= TAU_F; while (da < -PI_F) da += TAU_F;
    a0 += da * clampf(dt * 22.0f, 0, 1);
    p->ax = cosf(a0); p->ay = sinf(a0);
    if (fabsf(p->ax) > 0.15f) p->face = p->ax < 0 ? -1.0f : 1.0f;
}

/* ------------------------------------------------------------------ */
/* projectiles, pickups, effects                                        */
/* ------------------------------------------------------------------ */
static void bullets_update(Game *g, float dt)
{
    for (int i = 0; i < g->nbul; i++) {
        Bullet *b = &g->bul[i];
        b->life -= dt;
        if (b->life <= 0 || b->pierce == 0) {
            if (b->kind == PB_PELLET && b->life <= 0 && frand01(g) < 0.3f) add_particle(g, PT_EMBER, b->x, b->y, b->vx * 0.05f, b->vy * 0.05f - 20, 0.4f, 2, 0xFFA030FFu, 0xFF2000FFu, 1);
            *b = g->bul[--g->nbul]; i--; continue;
        }
        b->x += b->vx * dt; b->y += b->vy * dt;
        float bl = sqrtf(b->vx * b->vx + b->vy * b->vy) + 1e-3f;
        FOR_NEAR(g, b->x, b->y, b->r, j) {
            Enemy *e = &g->en[j];
            if (j == b->lasthit || !e->alive || e->state == 250) continue;
            float rr = e->r + b->r;
            if (dist2(b->x, b->y, e->x, e->y) > rr * rr) continue;
            b->lasthit = (int16_t)j;
            int crit = b->crit;
            hurt_enemy(g, j, b->dmg, crit, b->vx / bl, b->vy / bl, b->kb);
            if (b->weap == W_SCATTER && e->alive) { e->burn = fmaxf(e->burn, b->dmg * 0.8f); e->burn_t = 2.0f; }
            for (int k = 0; k < 2; k++) add_particle(g, PT_SPARK, b->x, b->y, -b->vx * 0.2f + frands(g) * 140, -b->vy * 0.2f + frands(g) * 140, 0.15f, 2.0f, 0xFFF0C0FFu, 0xFF8020FFu, 1);
            if (--b->pierce == 0) goto next_bullet;
        }
        for (int k = 0; k < MAX_BARREL; k++) {
            Barrel *br = &g->bar[k];
            if (br->alive && dist2(b->x, b->y, br->x, br->y) < 16.0f * 16.0f) { br->hp -= b->dmg; b->pierce = 0; break; }
        }
    next_bullet:;
    }
}

static void ebullets_update(Game *g, float dt)
{
    Player *p = &g->p;
    for (int i = 0; i < g->neb; i++) {
        EBullet *b = &g->eb[i];
        if (!b->alive) continue;
        b->life -= dt;
        if (b->life <= 0) { b->alive = 0; continue; }
        if (b->kind == EB_HOMING && b->life > 3.0f) {
            float dx = p->x - b->x, dy = p->y - b->y, l = sqrtf(dx * dx + dy * dy) + 1e-3f;
            float sp = sqrtf(b->vx * b->vx + b->vy * b->vy);
            b->vx += dx / l * 220.0f * dt; b->vy += dy / l * 220.0f * dt;
            float ns = sqrtf(b->vx * b->vx + b->vy * b->vy) + 1e-3f;
            b->vx *= sp / ns; b->vy *= sp / ns;
        }
        b->x += b->vx * dt; b->y += b->vy * dt;
        b->rot += dt * 10.0f;
        float rr = b->r + 6.0f;
        if (dist2(b->x, b->y, p->x, p->y) < rr * rr) {
            if (p->inv <= 0 && p->dash_t <= 0) { dbg_hurt_src = 1; hurt_player(g, b->dmg, b->x, b->y); dbg_hurt_src = 2; b->alive = 0; }
        }
        if (dist2(b->x, b->y, p->x, p->y) > 1400.0f * 1400.0f) b->alive = 0;
    }
    while (g->neb > 0 && !g->eb[g->neb - 1].alive) g->neb--;
}

static void bombs_update(Game *g, float dt)
{
    for (int i = 0; i < MAX_BOMB; i++) {
        Bomb *m = &g->bomb[i];
        if (!m->alive) continue;
        if (m->enemy) {
            m->z += m->vz * dt;
            if (m->z <= 0) {
                m->alive = 0;
                int kind = g->biome == 3 ? 1 : (g->biome == 4 ? 2 : 0);
                explode(g, m->x, m->y, m->rad, 0, 0, kind, 0);
                if (dist2(m->x, m->y, g->p.x, g->p.y) < m->rad * m->rad * 0.7f) hurt_player(g, m->dmg, m->x, m->y);
                if (g->biome == 3) add_fire(g, m->x, m->y, 34.0f, 3.0f, 6.0f * edmg_mul(g), 1);
            }
            continue;
        }
        m->x += m->vx * dt; m->y += m->vy * dt;
        m->vz -= 900.0f * dt; m->z += m->vz * dt;
        if (m->z < 0) { m->z = 0; m->vz = -m->vz * 0.3f; m->vx *= 0.5f; m->vy *= 0.5f; }
        m->rot += dt * 12.0f;
        m->fuse -= dt;
        if (frand01(g) < 0.7f) add_particle(g, PT_SPARK, m->x, m->y - m->z - 6, frands(g) * 60, -40 - frand01(g) * 60, 0.2f, 1.6f, 0xFFF0A0FFu, 0xFF6000FFu, 1);
        if (m->fuse <= 0) {
            m->alive = 0;
            explode(g, m->x, m->y, m->rad, m->dmg, m->big, 0, 0);
            if (m->cluster) {
                for (int k = 0; k < 3; k++) {
                    for (int j = 0; j < MAX_BOMB; j++) if (!g->bomb[j].alive) {
                        Bomb *c = &g->bomb[j];
                        memset(c, 0, sizeof *c);
                        float a = frand01(g) * TAU_F, s = 90.0f + frand01(g) * 90.0f;
                        c->x = m->x; c->y = m->y; c->vx = cosf(a) * s; c->vy = sinf(a) * s; c->z = 5; c->vz = 260;
                        c->fuse = 0.55f + 0.1f * (float)k; c->dmg = m->dmg * 0.45f; c->rad = m->rad * 0.55f; c->alive = 1;
                        break;
                    }
                }
            }
        }
    }
}

static void barrels_update(Game *g)
{
    for (int k = 0; k < MAX_BARREL; k++) {
        Barrel *b = &g->bar[k];
        if (!b->alive || b->hp > 0) continue;
        b->alive = 0;
        explode(g, b->x, b->y, 120.0f, 80.0f * hp_mul(g), 1, 0, 0);
        sfx(g, SFX_BARREL, b->x, b->y, 1.0f, 0.1f);
        add_fire(g, b->x, b->y, 50.0f, 4.0f, 14.0f * hp_mul(g), 0);
        add_text(g, b->x, b->y - 30, "KABOOM!");
    }
}

static void nuke(Game *g)
{
    Player *p = &g->p;
    announce(g, "KABOOM!", 0xFFB030FFu, SFX_V_KABOOM);
    sfx_ui(g, SFX_NUKE);
    g->flash = 1.0f; g->flash_r = 1.0f; g->flash_g = 0.9f; g->flash_b = 0.6f;
    add_shake(g, 1.0f);
    for (int k = 0; k < 9; k++) {
        float a = frand01(g) * TAU_F, d = frand01(g) * 300.0f;
        explode(g, p->x + cosf(a) * d, p->y + sinf(a) * d * 0.6f, 90.0f, 0, 1, 0, 0);
    }
    for (int i = 0; i < g->nen; i++) {
        Enemy *e = &g->en[i];
        if (!e->alive) continue;
        if (fabsf(e->x - p->x) < VW * 0.6f && fabsf(e->y - p->y) < VH * 0.6f) {
            if (EDEF[e->type].ai == AI_BOSS) hurt_enemy(g, i, e->maxhp * 0.08f, 0, 0, 0, 0);
            else hurt_enemy(g, i, 1e7f, 0, 0, -1, 0);
        }
    }
    for (int i = 0; i < g->neb; i++) g->eb[i].alive = 0;
}

static void open_chest(Game *g, float x, float y)
{
    Player *p = &g->p;
    sfx(g, SFX_CHEST, x, y, 1.0f, 0);
    int owned[NSLOTS], n = 0;
    for (int s = 0; s < NSLOTS; s++) if (p->slot[s] >= 0 && p->wlvl[p->slot[s]] < MAX_WLVL) owned[n++] = p->slot[s];
    if (n > 0) {
        int w = owned[irand(g, n)];
        p->wlvl[w]++;
        static char txt[NWEAP][24];
        snprintf(txt[w], sizeof txt[w], "%s LV %d!", WEAP_NAME[w], p->wlvl[w]);
        add_text(g, x, y - 30, txt[w]);
    } else {
        add_text(g, x, y - 30, "BOUNTY!");
    }
    spawn_coins(g, x, y, (int)(40.0f * bounty_mul(g)));
    for (int k = 0; k < 30; k++) {
        float a = frand01(g) * TAU_F, s = 80 + frand01(g) * 300;
        add_particle(g, PT_STAR, x, y, cosf(a) * s, sinf(a) * s - 100, 0.6f + frand01(g) * 0.4f, 5, 0xFFF0A0FFu, 0xFFA020FFu, 1);
    }
    add_boom(g, x, y, 80, 0, 3);
}

static void pickups_update(Game *g, float dt)
{
    Player *p = &g->p;
    float mag = 70.0f + 25.0f * (float)p->up[UP_MAGNET];
    int vacuum = p->magnet_t > 0 || g->phase == PH_WAVEEND || g->phase == PH_BOSSDEAD;
    for (int i = 0; i < g->npu; i++) {
        Pickup *u = &g->pu[i];
        if (!u->alive) { *u = g->pu[--g->npu]; i--; continue; }
        u->t += dt;
        float dx = p->x - u->x, dy = p->y - u->y, d = sqrtf(dx * dx + dy * dy) + 1e-3f;
        int coin = u->kind <= PU_GEM;
        if ((coin && (d < mag || vacuum)) || (!coin && d < 40.0f)) u->pull = 1;
        if (u->pull && u->t > 0.25f) {
            float sp = 380.0f + u->t * 200.0f;
            u->vx = lerpf(u->vx, dx / d * sp, clampf(dt * 8.0f, 0, 1)); u->vy = lerpf(u->vy, dy / d * sp, clampf(dt * 8.0f, 0, 1));
        } else {
            float fr = expf(-4.0f * dt); u->vx *= fr; u->vy *= fr;
        }
        u->x += u->vx * dt; u->y += u->vy * dt;
        if (d < 14.0f && u->t > 0.2f) {
            u->alive = 0;
            switch (u->kind) {
            case PU_COIN: case PU_GOLD: case PU_GEM:
                p->cash += u->val; p->earned += u->val;
                if (g->snd_cool[SFX_COIN] <= 0) {
                    /* a run of coins climbs the scale, like a pinball bonus count */
                    static const int SC[15] = { 0, 2, 4, 7, 9, 12, 14, 16, 19, 21, 24, 26, 28, 31, 33 };
                    g->coin_streak = g->coin_t > 0 ? g->coin_streak + 1 : 0;
                    g->coin_t = 0.6f;
                    snd_pitch(g, u->kind == PU_GEM ? SFX_GEM : SFX_COIN, 128.0f * powf(2.0f, (float)SC[g->coin_streak % 15] / 12.0f) * 0.85f, 0.45f);
                    g->snd_cool[SFX_COIN] = 0.03f;
                }
                break;
            case PU_HEALTH:
                p->hp = fminf(p->maxhp, p->hp + p->maxhp * 0.3f);
                sfx(g, SFX_HEAL, p->x, p->y, 0.9f, 0); add_text(g, p->x, p->y - 30, "+HEALTH");
                for (int k = 0; k < 14; k++) add_particle(g, PT_STAR, p->x + frands(g) * 14, p->y, 0, -60 - frand01(g) * 60, 0.7f, 3, 0x80FF80FFu, 0x20A040FFu, 1);
                break;
            case PU_MAGNET: p->magnet_t = 3.0f; sfx(g, SFX_POWERUP, p->x, p->y, 0.9f, 0); add_text(g, p->x, p->y - 30, "MAGNET"); break;
            case PU_NUKE: nuke(g); break;
            case PU_FRENZY: p->frenzy = 9.0f; sfx(g, SFX_POWERUP, p->x, p->y, 1.0f, 0); announce(g, "FRENZY!", 0xFF40C0FFu, SFX_V_FRENZY); break;
            case PU_CHEST: open_chest(g, u->x, u->y); sfx_ui(g, SFX_JACKPOT); if (g->vo_t <= 0) { sfx_ui(g, SFX_V_JACKPOT); g->vo_t = 2; } break;
            }
        }
    }
}

static void effects_update(Game *g, float dt)
{
    for (int i = 0; i < g->npt; i++) {
        Particle *q = &g->pt[i];
        q->life -= dt;
        if (q->life <= 0) { *q = g->pt[--g->npt]; i--; continue; }
        float drag;
        switch (q->kind) {
        case PT_SMOKE: drag = 2.2f; q->vy -= 14.0f * dt; q->size += dt * 10.0f; break;
        case PT_FIRE:  drag = 5.0f; q->vy -= 50.0f * dt; break;
        case PT_EMBER: drag = 1.0f; q->vx += sinf(q->life * 9.0f + q->rot) * 30.0f * dt; break;
        case PT_SOUL:  drag = 1.5f; q->vy -= 30.0f * dt; break;
        case PT_DUST:  drag = 3.0f; q->size += dt * 8.0f; break;
        case PT_GOO:   drag = 5.0f; break;
        case PT_CASING: case PT_CHUNK: case PT_BONE:
            drag = q->z > 0 ? 0.8f : 6.0f;
            q->vz -= 600.0f * dt; q->z += q->vz * dt;
            if (q->z < 0) { q->z = 0; q->vz = -q->vz * 0.35f; if (fabsf(q->vz) < 30) q->vz = 0; q->vr *= 0.6f; }
            break;
        case PT_BLOOD:
            drag = 0.6f;
            q->vz -= 520.0f * dt; q->z += q->vz * dt;
            if (q->z <= 0) {      /* lands: a stain */
                add_decal(g, q->x, q->y, q->size * (1.0f + frand01(g)), (q->c0 & 0xFFFFFF00u) | 0xD0u, DC_SPOT);
                *q = g->pt[--g->npt]; i--; continue;
            }
            break;
        case PT_GIB: case PT_HEAD:
            drag = q->z > 0 ? 0.6f : 7.0f;
            q->vz -= 600.0f * dt; q->z += q->vz * dt;
            if (q->z < 0) {
                q->z = 0;
                if (q->vz < -60) add_decal(g, q->x, q->y, 2.0f + q->size * 0.6f, 0x5A0606C0u, DC_SPOT);
                q->vz = -q->vz * 0.3f; if (fabsf(q->vz) < 30) q->vz = 0; q->vr *= 0.5f;
            } else if (frand01(g) < 0.25f) {
                Particle *d = add_particle(g, PT_BLOOD, q->x, q->y, q->vx * 0.2f, q->vy * 0.2f, 1.0f, 1.0f, q->kind == PT_HEAD ? EDEF[q->c1 < A_COUNT ? q->c1 : 0].blood : q->c1, 0x200000FFu, 0);
                d->z = q->z; d->vz = 0;
            }
            break;
        case PT_MIST: drag = 3.0f; q->size += dt * 14.0f; break;
        case PT_RING: drag = 0; break;
        default:       drag = 3.5f; break;
        }
        float fr = expf(-drag * dt);
        q->vx *= fr; q->vy *= fr;
        q->x += q->vx * dt; q->y += q->vy * dt;
        q->rot += q->vr * dt;
    }
    for (int i = 0; i < MAX_BOOM; i++) {
        Boom *b = &g->boom[i];
        if (b->t >= b->dur) continue;
        b->t += dt;
        if (b->hurt && !b->hit) {
            /* a boss shockwave ring: jump it with a dash */
            float rr = b->r * (b->t / b->dur), d = sqrtf(dist2(b->x, b->y, g->p.x, g->p.y));
            if (fabsf(d - rr) < 16.0f && g->p.dash_t <= 0 && g->p.inv <= 0) { b->hit = 1; hurt_player(g, b->dmg, b->x, b->y); }
        }
    }
    for (int i = 0; i < MAX_BEAM; i++) if (g->beam[i].life > 0) g->beam[i].life -= dt;
    for (int i = 0; i < MAX_DECAL; i++) if (g->dec[i].life > 0) { g->dec[i].life -= dt; g->dec[i].age += dt; }
    for (int i = 0; i < MAX_CORPSE; i++) if (g->corpse[i].alive && (g->corpse[i].life -= dt) <= 0) g->corpse[i].alive = 0;
    for (int i = 0; i < MAX_LENS; i++) if (g->lens[i].r > 0) {
        Lens *l = &g->lens[i];
        l->t += dt;
        l->drip += dt * (10.0f + l->r * 0.4f) * (l->t < 1.5f ? 1.0f : 0.3f);
        if (l->t > 3.5f) l->r = 0;
    }
    for (int i = 0; i < MAX_NUM; i++) g->num[i].t += dt;
    for (int i = 0; i < MAX_FIRE; i++) {
        Fire *f = &g->fire[i];
        if (f->life <= 0) continue;
        f->life -= dt;
        if (frand01(g) < 0.5f) add_particle(g, PT_FIRE, f->x + frands(g) * f->r * 0.7f, f->y + frands(g) * f->r * 0.4f, frands(g) * 10, -50 - frand01(g) * 40, 0.4f, 6 + frand01(g) * 6,
                                            f->enemy && g->biome == 3 ? 0x90FF50FFu : 0xFFC040FFu, f->enemy && g->biome == 3 ? 0x205010FFu : 0xFF2000FFu, 1);
        if (f->enemy) {
            if (dist2(f->x, f->y, g->p.x, g->p.y) < f->r * f->r && g->p.inv <= 0) hurt_player(g, f->dps, f->x, f->y);
            continue;
        }
        FOR_NEAR(g, f->x, f->y, f->r, j) {
            Enemy *e = &g->en[j];
            if (!e->alive || dist2(f->x, f->y, e->x, e->y) > (f->r + e->r) * (f->r + e->r)) continue;
            e->burn = fmaxf(e->burn, f->dps); e->burn_t = fmaxf(e->burn_t, 1.0f);
        }
    }
    /* ambient: the biome's air */
    float cx = g->p.x, cy = g->p.y;
    switch (g->biome) {
    case 0: if (frand01(g) < 0.3f) add_particle(g, PT_DUST, cx + frands(g) * 480, cy + frands(g) * 280, 40 + frand01(g) * 30, frands(g) * 8, 3.0f, 2.0f, 0xC0A07050u, 0, 0); break;
    case 1: if (frand01(g) < 0.25f) add_particle(g, PT_EMBER, cx + frands(g) * 480, cy + frands(g) * 280, frands(g) * 10, frands(g) * 10, 2.5f, 1.8f, 0xC8FF60FFu, 0x40A020FFu, 1); break;
    case 3: if (frand01(g) < 0.7f) add_particle(g, PT_EMBER, cx + frands(g) * 480, cy - 260 + frands(g) * 200, frands(g) * 20, 40 + frand01(g) * 40, 4.0f, 1.6f, 0xFFA030FFu, 0xFF2000FFu, 1); break;
    case 4: if (frand01(g) < 0.25f) add_particle(g, PT_SOUL, cx + frands(g) * 480, cy + frands(g) * 280, frands(g) * 6, -10, 2.5f, 3.0f, 0xFF2040FFu, 0, 1); break;
    default: break;
    }
    /* storm over the graveyard */
    if (g->biome == 2 || g->biome == 4) {
        g->thunder_t -= dt;
        if (g->lightning > 0) g->lightning -= dt * 3.0f;
        if (g->thunder_t <= 0) {
            g->thunder_t = 6.0f + frand01(g) * 9.0f;
            g->lightning = 1.0f;
            sfx_ui(g, SFX_THUNDER);
        }
    }
}

/* ------------------------------------------------------------------ */
/* the lights for this frame                                            */
/* ------------------------------------------------------------------ */
static void light(Game *g, float x, float y, float r, int cr, int cg, int cb)
{
    if (g->nlight >= MAX_LIGHT) return;
    if (fabsf(x - g->camx) > VW * 0.5f + r || fabsf(y - g->camy) > VH * 0.5f + r) return;
    Light *l = &g->light[g->nlight++];
    l->x = x; l->y = y; l->r = r; l->cr = cr; l->cg = cg; l->cb = cb;
}

static void lights_build(Game *g)
{
    Player *p = &g->p;
    g->nlight = 0;
    /* the gunslinger's own glow: the lantern on the belt */
    float fl = 0.9f + 0.1f * sinf(g->t * 13.0f) * sinf(g->t * 7.3f);
    light(g, p->x, p->y, 175.0f, (int)(150 * fl), (int)(110 * fl), (int)(60 * fl));
    for (int i = 0; i < MAX_BOOM; i++) {
        const Boom *b = &g->boom[i];
        if (b->t >= b->dur) continue;
        float k = 1.0f - b->t / b->dur;
        if (b->kind == 3) { light(g, b->x, b->y, b->r * 0.6f, (int)(90 * k), (int)(70 * k), (int)(60 * k)); continue; }
        k *= k;
        if (b->kind == 1) light(g, b->x, b->y, b->r * 2.6f, (int)(50 * k), (int)(160 * k), (int)(40 * k));
        else if (b->kind == 2) light(g, b->x, b->y, b->r * 2.6f, (int)(130 * k), (int)(40 * k), (int)(170 * k));
        else light(g, b->x, b->y, b->r * 2.8f, (int)(170 * k), (int)(110 * k), (int)(45 * k));
    }
    for (int i = 0; i < MAX_BEAM; i++) {
        const Beam *b = &g->beam[i];
        if (b->life <= 0) continue;
        float k = b->life / b->max;
        float mx = (b->x0 + b->x1) * 0.5f, my = (b->y0 + b->y1) * 0.5f;
        if (b->kind == BM_RAIL) {
            for (int s = 0; s < 4; s++) { float u = (float)s / 3.0f; light(g, lerpf(b->x0, b->x1, u), lerpf(b->y0, b->y1, u), 150, (int)(220 * k), (int)(60 * k), (int)(230 * k)); }
        } else light(g, mx, my, 110, (int)(60 * k), (int)(150 * k), (int)(255 * k));
    }
    for (int i = 0; i < MAX_FIRE; i++) {
        const Fire *f = &g->fire[i];
        if (f->life <= 0) continue;
        float k = clampf(f->life, 0, 1) * (0.85f + 0.15f * sinf(g->t * 20.0f + (float)i));
        if (f->enemy && g->biome == 3) light(g, f->x, f->y, f->r * 3.0f, (int)(70 * k), (int)(200 * k), (int)(50 * k));
        else light(g, f->x, f->y, f->r * 3.2f, (int)(230 * k), (int)(120 * k), (int)(40 * k));
    }
    for (int i = 0; i < MAX_BOMB; i++) if (g->bomb[i].alive && !g->bomb[i].enemy) light(g, g->bomb[i].x, g->bomb[i].y, 70, 200, 120, 40);
    for (int i = 0; i < g->npt; i++) {
        const Particle *q = &g->pt[i];
        if (q->kind == PT_STAR && q->size > 10) light(g, q->x, q->y, q->size * 9.0f, 255, 180, 90);
        else if (q->kind == PT_STAR && (i & 3) == 0) light(g, q->x, q->y, 60, 180, 140, 60);
    }
    if (g->boss >= 0 && g->en[g->boss].alive) {
        const Enemy *e = &g->en[g->boss];
        uint32_t c = EDEF[e->type].eye;
        light(g, e->x, e->y - e->r * 0.4f, 180, (int)(c >> 24) / 2, (int)((c >> 16) & 255) / 2, (int)((c >> 8) & 255) / 2);
        if (g->laser_on > 0) {
            float lx = cosf(g->laser_a), ly = sinf(g->laser_a);
            int strong = g->laser_on >= 1.0f;
            for (int s = 1; s <= 5; s++) light(g, e->x + lx * (float)s * 130.0f, e->y + ly * (float)s * 130.0f, strong ? 170 : 70, strong ? 255 : 120, strong ? 40 : 10, strong ? 90 : 30);
        }
    }
    /* enemy shots glow - a cheap light for every other one */
    for (int i = 0; i < g->neb; i += 2) {
        const EBullet *b = &g->eb[i];
        if (!b->alive) continue;
        int cr = 60, cg = 200, cb = 255;
        if (b->kind == EB_GOO || b->kind == EB_POISON) { cr = 90; cg = 230; cb = 60; }
        else if (b->kind == EB_VOID || b->kind == EB_HOMING) { cr = 220; cg = 60; cb = 255; }
        else if (b->kind == EB_BONE) { cr = 200; cg = 170; cb = 120; }
        else if (b->kind == EB_SKULL) { cr = 120; cg = 220; cb = 255; }
        light(g, b->x, b->y, 60, cr, cg, cb);
    }
    /* bullets: every third tracer lights the ground on its way */
    for (int i = 0; i < g->nbul; i += 3) {
        const Bullet *b = &g->bul[i];
        if (b->kind == PB_PELLET) light(g, b->x, b->y, 50, 200, 100, 30);
        else light(g, b->x, b->y, 40, 160, 130, 60);
    }
    for (int k = 0; k < MAX_BARREL; k++) if (g->bar[k].alive) light(g, g->bar[k].x, g->bar[k].y - 6, 40, 90, 30, 10);
    {   /* glowing scenery: candles, lava, mushrooms, eyes */
        float wx0 = g->camx - VW * 0.5f, wy0 = g->camy - VH * 0.5f;
        int cx0 = (int)floorf(wx0 / 128.0f) - 1, cy0 = (int)floorf(wy0 / 128.0f) - 1;
        for (int j = 0; j < 7; j++)
            for (int i = 0; i < 10; i++) {
                int kind; float x, y, ox, oy, r; uint32_t c;
                if (!prop_at(g->biome, cx0 + i, cy0 + j, &kind, &x, &y) || !prop_glow(g->biome, kind, &ox, &oy, &c, &r)) continue;
                float fl = 0.8f + 0.2f * sinf(g->t * 9.0f + (float)(i + j));
                light(g, x + ox, y + oy, r * 3.2f, (int)((float)(c >> 24) * 0.6f * fl), (int)((float)((c >> 16) & 255) * 0.6f * fl), (int)((float)((c >> 8) & 255) * 0.6f * fl));
            }
    }
    if (p->stand_t > 0) light(g, p->x, p->y, 260, 255, 60, 30);
    if (p->frenzy > 0) light(g, p->x, p->y, 200, 200, 60, 200);
}

/* ------------------------------------------------------------------ */
/* run flow                                                             */
/* ------------------------------------------------------------------ */
static void clear_field(Game *g)
{
    g->nen = 0; g->nbul = 0; g->neb = 0; g->npt = 0; g->npu = 0;
    memset(g->boom, 0, sizeof g->boom);
    memset(g->num, 0, sizeof g->num);
    for (int i = 0; i < MAX_NUM; i++) g->num[i].t = 99;
    memset(g->fire, 0, sizeof g->fire);
    memset(g->beam, 0, sizeof g->beam);
    memset(g->bomb, 0, sizeof g->bomb);
    memset(g->dec, 0, sizeof g->dec);
    memset(g->corpse, 0, sizeof g->corpse);
    memset(g->lens, 0, sizeof g->lens);
    memset(g->bar, 0, sizeof g->bar);
    g->boss = -1;
    g->laser_on = 0;
}

void world_start_run(Game *g, int biome, int character)
{
    Player *p = &g->p;
    memset(p, 0, sizeof *p);
    p->character = character;
    for (int s = 0; s < NSLOTS; s++) p->slot[s] = -1;
    static const int START[NCHARS] = { W_REVOLVER, W_SCATTER, W_GATLING, W_DYNAMITE };
    p->slot[0] = (int8_t)START[character];
    p->wlvl[START[character]] = 1;
    p->sel = 0;
    p->maxhp = 120.0f * (1.0f + 0.1f * (float)g->sv.chr_lvl[character]) + (character == CH_PREACHER ? 25.0f : 0.0f);
    p->hp = p->maxhp;
    p->ax = 1; p->ay = 0; p->face = 1;
    p->stands = 1 + g->sv.perk[PK_STAND];
    p->cash = 30 + 40 * g->sv.perk[PK_CASH];
    p->cash_shown = p->cash;
    p->target = -1;
    p->side = 1;
    g->total_kills = 0;
    g->total_time = 0;
    g->best_combo = 0;
    /* starting in a deeper biome: catch up on gear a little */
    for (int b = 0; b < biome; b++) {
        p->wlvl[START[character]] = (uint8_t)(p->wlvl[START[character]] < MAX_WLVL ? p->wlvl[START[character]] + 1 : MAX_WLVL);
        p->cash += 260 + 160 * b;
    }
    world_start_biome(g, biome);
}

void world_start_biome(Game *g, int biome)
{
    clear_field(g);
    g->biome = biome;
    g->wave = 0;
    g->phase = PH_WAVE;
    g->phase_t = 0;
    g->run_t = 0;
    g->spawn_acc = 0;
    g->horde_t = 22.0f;
    g->barrel_t = 3.0f;
    g->chaos = 0;
    g->combo = 0;
    g->p.x = 0; g->p.y = 0; g->p.vx = g->p.vy = 0;
    g->p.hp = g->p.maxhp;
    g->camx = 0; g->camy = 0;
    g->wave_banner = 0;
    g->thunder_t = 4.0f;
    g->music_epoch++;
    g->music_pitch = 128 - 5 * biome;
}

void world_begin_wave(Game *g)
{
    g->phase = g->wave >= NWAVES ? PH_BOSS : PH_WAVE;
    g->phase_t = 0;
    g->wave_banner = 0;
    g->spawn_acc = 0;
    g->p.inv = 1.0f;
    if (g->phase == PH_BOSS) spawn_boss(g);
    else { sfx_ui(g, SFX_HORN); sfx_ui(g, SFX_TAIKO); }
}

void world_collect_all(Game *g)
{
    for (int i = 0; i < g->npu; i++) {
        Pickup *u = &g->pu[i];
        if (u->alive && u->kind <= PU_GEM) { g->p.cash += u->val; g->p.earned += u->val; u->alive = 0; }
        if (u->alive && u->kind == PU_CHEST) { u->alive = 0; open_chest(g, g->p.x, g->p.y); }
    }
    for (int i = 0; i < g->npu; i++) if (g->pu[i].kind <= PU_GEM) g->pu[i].alive = 0;
}

/* the constant racket underneath everything: the horde grumbling, the biome's own noises, a heart pounding
 * when you're nearly dead */
static void noise_machine(Game *g, float dt)
{
    if (g->phase == PH_WAVEEND || g->phase == PH_BOSSDEAD) return;
    g->chatter_t -= dt;
    if (g->chatter_t <= 0) {
        g->chatter_t = 0.35f + frand01(g) * 0.7f;
        for (int tries = 0; tries < 6; tries++) {
            int i = irand(g, g->nen > 0 ? g->nen : 1);
            Enemy *e = &g->en[i];
            if (!e->alive || fabsf(e->x - g->camx) > 420 || fabsf(e->y - g->camy) > 260) continue;
            sfx(g, chatter_voice(e->type), e->x, e->y, 0.3f + 0.2f * frand01(g), 0.25f);
            break;
        }
    }
    g->amb_t -= dt;
    if (g->amb_t <= 0) {
        g->amb_t = 2.0f + frand01(g) * 4.0f;
        static const int AMB[NBIOMES][4] = {
            { SFX_WIND, SFX_HOWL, SFX_CAW, SFX_WIND },
            { SFX_CROAK, SFX_BUBBLES, SFX_CAW, SFX_CROAK },
            { SFX_BELL, SFX_CHAINS, SFX_CAW, SFX_WAIL },
            { SFX_CRACKLE, SFX_SCREAM, SFX_CRACKLE, SFX_WIND },
            { SFX_WHISPER, SFX_LAUGH, SFX_SCREAM, SFX_WHISPER },
        };
        int id = AMB[g->biome][irand(g, 4)];
        float a = frand01(g) * TAU_F;
        sfx(g, id, g->camx + cosf(a) * 380.0f, g->camy + sinf(a) * 200.0f, id == SFX_SCREAM ? 0.25f : 0.55f, 0.15f);
    }
    if (g->p.hp > 0 && g->p.hp < g->p.maxhp * 0.3f) {
        g->heart_t -= dt;
        if (g->heart_t <= 0) { g->heart_t = 0.55f + g->p.hp / g->p.maxhp; sfx_ui(g, SFX_HEART); }
    }
}

void world_update(Game *g, const Input *in, float dt)
{
    Player *p = &g->p;
    grid_build(g);
    player_update(g, in, dt);
    weapons_update(g, in, dt);
    enemies_update(g, dt);
    grid_build(g);
    bullets_update(g, dt);
    ebullets_update(g, dt);
    bombs_update(g, dt);
    barrels_update(g);
    pickups_update(g, dt);
    effects_update(g, dt);
    spawn_update(g, dt);
    while (g->nen > 0 && !g->en[g->nen - 1].alive) g->nen--;

    /* the clock */
    g->phase_t += dt;
    g->wave_banner += dt;
    g->total_time += dt;
    if (g->phase == PH_WAVE) {
        g->run_t += dt;
        if (g->run_t >= (float)(g->wave + 1) * WAVE_TIME) {
            g->run_t = (float)(g->wave + 1) * WAVE_TIME;
            g->phase = PH_WAVEEND;
            g->phase_t = 0;
            for (int i = 0; i < g->neb; i++) g->eb[i].alive = 0;
            sfx_ui(g, SFX_WAVECLEAR);
            announce(g, g->wave == NWAVES - 1 ? "THE GROUND SHAKES..." : "WAVE SURVIVED!", g->wave == NWAVES - 1 ? 0xFF4030FFu : 0xFFE060FFu, g->wave == NWAVES - 1 ? -1 : SFX_V_WAVE);
        }
    } else if (g->phase == PH_BOSS) {
        g->run_t += dt;
    }
    if (g->combo_t > 0) { g->combo_t -= dt; if (g->combo_t <= 0) { if (g->combo >= 30) sfx_ui(g, SFX_COMBOBREAK); g->combo = 0; } }
    if (g->mk_t > 0) { g->mk_t -= dt; if (g->mk_t <= 0) { g->mk_count = 0; g->mk_level = 0; } }
    if (g->coin_t > 0) g->coin_t -= dt;
    if (g->vo_t > 0) g->vo_t -= dt;
    if (g->bosshurt_t > 0) g->bosshurt_t -= dt;
    noise_machine(g, dt);
    g->chaos = fmaxf(0.0f, g->chaos - dt * 0.12f);
    {   /* the screen is full of monsters: that's chaos too */
        float crowd = clampf((float)alive_count(g) / 180.0f, 0, 1) * 0.6f;
        if (g->chaos < crowd) g->chaos = lerpf(g->chaos, crowd, dt);
    }
    if (g->red > 0) g->red -= dt * 1.5f;
    if (g->zoom_punch > 0) g->zoom_punch = fmaxf(0, g->zoom_punch - dt * 0.15f);
    if (g->flash > 0) g->flash -= dt * 2.5f;
    if (g->shake > 0) g->shake = fmaxf(0, g->shake - dt * 1.6f);
    float sh = g->shake * g->shake * 16.0f;
    g->shx = sinf(g->t * 71.0f) * sh + frands(g) * sh * 0.4f;
    g->shy = cosf(g->t * 57.0f) * sh + frands(g) * sh * 0.4f;

    /* camera: follow, leaning a little into the aim */
    float tx = p->x + p->ax * 40.0f, ty = p->y + p->ay * 30.0f;
    g->camx = lerpf(g->camx, tx, clampf(dt * 6.0f, 0, 1));
    g->camy = lerpf(g->camy, ty, clampf(dt * 6.0f, 0, 1));
    if (fabsf(g->camx - p->x) > VW * 0.3f) g->camx = p->x + (g->camx > p->x ? VW * 0.3f : -VW * 0.3f);
    if (fabsf(g->camy - p->y) > VH * 0.3f) g->camy = p->y + (g->camy > p->y ? VH * 0.3f : -VH * 0.3f);
    if (p->cash_shown < p->cash) p->cash_shown += 1 + (p->cash - p->cash_shown) / 8;
    if (p->cash_shown > p->cash) p->cash_shown = p->cash;
    if (g->boss >= 0) {
        const Enemy *e = &g->en[g->boss];
        g->boss_hp_shown = approach(g->boss_hp_shown, e->alive ? e->hp / e->maxhp : 0, dt * 0.6f);
        if (g->boss_tell > 0 && e->state == 0) g->boss_tell = 0;
    }
    if (g->announce) { g->announce_t += dt; if (g->announce_t > 2.4f) g->announce = NULL; }
    lights_build(g);
}
