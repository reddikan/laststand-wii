/*
 * LAST STAND - drawing the world: the biome floor and its props, the mess on the ground, the horde, the
 * gunslinger, every projectile and explosion, then the lighting pass (a coarse light grid drawn as a 2x
 * multiply over everything, so lights can over-brighten as well as darken) and the additive glow pass on top.
 */
#include "game_int.h"
#include "gfx.h"

#include <string.h>
#include <stdio.h>

static float OXs, OYs;           /* world -> screen offset this frame */
static float ZM = 1.0f;          /* the world is drawn zoomed in a little about the screen centre */
#define WORLD_ZOOM 1.42f
#define SX(x) ((x) + OXs)
#define SY(y) ((y) + OYs)

static int onscreen(float x, float y, float m) { return x > -m && x < VW + m && y > -m && y < VH + m; }

/* ------------------------------------------------------------------ */
/* biome palettes                                                        */
/* ------------------------------------------------------------------ */
typedef struct { uint32_t base, dark, light, crack; int amb[3]; } Biome;
static const Biome BIO[NBIOMES] = {
    { 0x7E6040FFu, 0x664C32FFu, 0x8E704CFFu, 0x4A3426FFu, { 104, 86, 80 } },    /* dusk desert */
    { 0x34442AFFu, 0x22302AFFu, 0x44583AFFu, 0x1E2A1EFFu, { 80, 98, 86 } },    /* swamp */
    { 0x3C3630FFu, 0x2E2A26FFu, 0x424434FFu, 0x221E1CFFu, { 64, 68, 86 } },    /* graveyard */
    { 0x2C2624FFu, 0x1E1A18FFu, 0x36302CFFu, 0x120E0EFFu, { 60, 42, 40 } },    /* burned forest */
    { 0x221824FFu, 0x180E1AFFu, 0x2E1A2CFFu, 0x3A0A14FFu, { 36, 26, 46 } },    /* hellmouth */
};

void biome_ambient(int b, int out[3]) { out[0] = BIO[b].amb[0]; out[1] = BIO[b].amb[1]; out[2] = BIO[b].amb[2]; }

/* ------------------------------------------------------------------ */
/* the floor                                                             */
/* ------------------------------------------------------------------ */
#define CELL 128.0f

static void blob(float cx, float cy, float r, uint32_t seed, uint32_t c, int n)
{
    float px = 0, py = 0, fx = 0, fy = 0;
    for (int i = 0; i <= n; i++) {
        int k = i % n;
        float a = (float)k / (float)n * TAU_F;
        float rr = r * (0.7f + 0.3f * (float)((hash32(seed + (uint32_t)k * 77u) >> 8) & 255) / 255.0f);
        float x = cx + cosf(a) * rr, y = cy + sinf(a) * rr * 0.7f;
        if (i == 0) { fx = x; fy = y; }
        else gfx_tri(cx, cy, c, px, py, c, x, y, c);
        px = x; py = y;
    }
    (void)fx; (void)fy;
}

static void draw_floor(const Game *g)
{
    const Biome *B = &BIO[g->biome];
    gfx_rect(0, 0, VW, VH, B->base);
    float wx0 = g->camx - VW * 0.5f - CELL, wy0 = g->camy - VH * 0.5f - CELL;
    int cx0 = (int)floorf(wx0 / CELL), cy0 = (int)floorf(wy0 / CELL);
    int ncx = (int)(VW / CELL) + 3, ncy = (int)(VH / CELL) + 3;
    /* patches of darker and lighter ground */
    for (int j = 0; j < ncy; j++)
        for (int i = 0; i < ncx; i++) {
            int cx = cx0 + i, cy = cy0 + j;
            uint32_t h = hash32((uint32_t)cx * 73856093u ^ (uint32_t)cy * 19349663u ^ (uint32_t)g->biome * 83492791u);
            float x = ((float)cx + 0.5f) * CELL + (float)((h >> 4) & 63) - 32.0f;
            float y = ((float)cy + 0.5f) * CELL + (float)((h >> 10) & 63) - 32.0f;
            if ((h & 3) != 0) blob(SX(x), SY(y), 50.0f + (float)((h >> 16) & 63), h, (h & 4) ? B->dark : B->light, 10);
        }
    /* cracks and roots */
    for (int j = 0; j < ncy; j++)
        for (int i = 0; i < ncx; i++) {
            int cx = cx0 + i, cy = cy0 + j;
            uint32_t h = hash32((uint32_t)cx * 2654435761u ^ (uint32_t)cy * 40503u ^ 0x5bd1e995u);
            if ((h & 7) > 2) continue;
            float x = SX(((float)cx) * CELL + (float)((h >> 8) & 127)), y = SY(((float)cy) * CELL + (float)((h >> 15) & 127));
            float a = (float)((h >> 20) & 255) / 255.0f * TAU_F;
            uint32_t col = g->biome >= 3 && (h & 8) ? (g->biome == 3 ? 0xFF5010C0u : 0x9A1020C0u) : B->crack;
            for (int s = 0; s < 3; s++) {
                float l = 14.0f + (float)((h >> (s * 3)) & 15);
                float nx = x + cosf(a) * l, ny = y + sinf(a) * l;
                gfx_line(x, y, nx, ny, 2.0f - (float)s * 0.4f, col, col);
                if (s == 1) { float b = a + 0.9f; gfx_line(nx, ny, nx + cosf(b) * 10, ny + sinf(b) * 10, 1.2f, col, col); }
                x = nx; y = ny; a += ((float)((h >> (s * 4 + 4)) & 7) - 3.5f) * 0.25f;
            }
        }
}

/* props: one per cell at most, decided by a hash, so the world is endless and the same every time */
int prop_at(int biome, int cx, int cy, int *kind, float *x, float *y)
{
    uint32_t h = hash32((uint32_t)cx * 951274213u ^ (uint32_t)cy * 1597334677u ^ (uint32_t)(biome + 1) * 3812015801u);
    if ((h & 3) == 0) return 0;
    if (cx == 0 && cy == 0) return 0;
    *kind = (int)((h >> 3) % 6);
    *x = ((float)cx + 0.15f + 0.7f * (float)((h >> 8) & 255) / 255.0f) * CELL;
    *y = ((float)cy + 0.15f + 0.7f * (float)((h >> 16) & 255) / 255.0f) * CELL;
    return 1;
}

int prop_glow(int biome, int kind, float *ox, float *oy, uint32_t *col, float *r)
{
    static const struct { int8_t b, k; float ox, oy, r; uint32_t c; } G[] = {
        { 1, 3, 0, -8, 26, 0x60E0C0FFu },
        { 2, 3, 0, -9, 30, 0xFFC040FFu },
        { 3, 0, 0, -8, 20, 0xFF6020FFu }, { 3, 2, 0, -2, 24, 0xFF7020FFu }, { 3, 4, 0, 0, 50, 0xFF6010FFu },
        { 4, 0, 0, -18, 22, 0xFF2030FFu }, { 4, 3, 0, 0, 46, 0xD02040FFu }, { 4, 4, 0, -3, 26, 0xE06090FFu },
    };
    for (unsigned i = 0; i < sizeof G / sizeof G[0]; i++)
        if (G[i].b == biome && G[i].k == kind) { *ox = G[i].ox; *oy = G[i].oy; *r = G[i].r; *col = G[i].c; return 1; }
    return 0;
}

void prop_glows(int biome, float wx0, float wy0, float t)
{
    int cx0 = (int)floorf((wx0 - CELL) / CELL), cy0 = (int)floorf((wy0 - CELL) / CELL);
    int ncx = (int)(VW / CELL) + 3, ncy = (int)(VH / CELL) + 3;
    for (int j = 0; j < ncy; j++)
        for (int i = 0; i < ncx; i++) {
            int kind; float x, y, ox, oy, r; uint32_t c;
            if (!prop_at(biome, cx0 + i, cy0 + j, &kind, &x, &y) || !prop_glow(biome, kind, &ox, &oy, &c, &r)) continue;
            float sx = x - wx0 + ox, sy = y - wy0 + oy;
            if (!onscreen(sx, sy, r * 2)) continue;
            float fl = 0.75f + 0.25f * sinf(t * (biome == 2 ? 11.0f : 2.5f) + (float)(i * 7 + j * 3));
            gfx_glow(sx, sy, r * fl, col_a(c, 0.5f));
        }
}

static void draw_props(const Game *g)
{
    float wx0 = g->camx - VW * 0.5f - CELL, wy0 = g->camy - VH * 0.5f - CELL;
    int cx0 = (int)floorf(wx0 / CELL), cy0 = (int)floorf(wy0 / CELL);
    int ncx = (int)(VW / CELL) + 3, ncy = (int)(VH / CELL) + 3;
    for (int j = 0; j < ncy; j++)
        for (int i = 0; i < ncx; i++) {
            int kind; float x, y;
            if (!prop_at(g->biome, cx0 + i, cy0 + j, &kind, &x, &y)) continue;
            if (!onscreen(SX(x), SY(y), 60)) continue;
            art_prop(g->biome, kind, SX(x), SY(y), g->t, (uint32_t)((cx0 + i) * 31 + (cy0 + j) * 17));
        }
}

/* ------------------------------------------------------------------ */
/* ground stuff                                                          */
/* ------------------------------------------------------------------ */
static void draw_decals(const Game *g)
{
    for (int i = 0; i < MAX_DECAL; i++) {
        const Decal *d = &g->dec[i];
        if (d->life <= 0) continue;
        float x = SX(d->x), y = SY(d->y);
        if (!onscreen(x, y, d->r * 2 + 10)) continue;
        float a = clampf(d->life / 6.0f, 0, 1);
        uint32_t c = col_a(d->c, a);
        switch (d->kind) {
        case DC_POOL: {          /* a pool that spreads out over the first second, glossy in the middle */
            float r = d->r * (0.35f + 0.65f * clampf(d->age / 0.9f, 0, 1));
            blob(x, y, r, (uint32_t)(d->rot * 1000.0f), c, 8);
            blob(x - r * 0.15f, y - r * 0.1f, r * 0.55f, (uint32_t)(d->rot * 777.0f), col_a(col_mix(d->c, 0x000000FFu, 0.3f), a), 6);
            gfx_poly(x - r * 0.3f, y - r * 0.2f, r * 0.12f, 5, 0, col_a(0xFFD0D0FFu, a * 0.18f));
            for (int k = 0; k < 3; k++) {
                float an = d->rot + (float)k * 2.1f;
                gfx_poly(x + cosf(an) * r * 1.35f, y + sinf(an) * r * 0.95f, r * 0.2f, 5, 0, c);
            }
            break;
        }
        case DC_SPOT: gfx_poly(x, y, d->r, 5, d->rot, c); break;
        case DC_SMEAR: {         /* dragged along the way the body flew */
            float cs = cosf(d->rot), sn = sinf(d->rot), l = d->r * 1.6f, w = d->r * 0.35f;
            gfx_quad4(x - cs * l * 0.2f - sn * w, y - sn * l * 0.2f + cs * w, c, x + cs * l - sn * w * 0.3f, y + sn * l + cs * w * 0.3f, c & 0xFFFFFF00u,
                      x + cs * l + sn * w * 0.3f, y + sn * l - cs * w * 0.3f, c & 0xFFFFFF00u, x - cs * l * 0.2f + sn * w, y - sn * l * 0.2f - cs * w, c);
            break;
        }
        default:
            gfx_poly2(x, y, d->r, 9, d->rot, c, c & 0xFFFFFF00u);
            if (d->kind == DC_CRATER) gfx_ring(x, y, d->r * 0.8f, 3, 14, 0, col_a(0x000000FFu, a * 0.4f));
            break;
        }
    }
}

static void draw_fires(const Game *g, int glow)
{
    for (int i = 0; i < MAX_FIRE; i++) {
        const Fire *f = &g->fire[i];
        if (f->life <= 0) continue;
        float x = SX(f->x), y = SY(f->y);
        if (!onscreen(x, y, f->r + 20)) continue;
        float k = clampf(f->life, 0, 1);
        int green = f->enemy && g->biome == 3;
        if (!glow) {
            gfx_poly2(x, y, f->r, 12, 0, col_a(green ? 0x103008FFu : 0x200804FFu, 0.6f * k), 0x00000000u);
        } else {
            gfx_glow(x, y, f->r * 1.3f, col_a(green ? 0x60FF40FFu : 0xFF6018FFu, 0.45f * k));
            for (int t = 0; t < 4; t++) {
                float fl = sinf(g->t * 13.0f + (float)(i * 3 + t)) * 0.5f + 0.5f;
                float fx = x + cosf((float)t * 1.7f + (float)i) * f->r * 0.5f, fy = y + sinf((float)t * 2.3f + (float)i) * f->r * 0.3f;
                float h = (10.0f + fl * 12.0f) * k;
                gfx_tri(fx - 5, fy, col_a(green ? 0x50FF30FFu : 0xFF5010FFu, 0.8f * k), fx + 5, fy, col_a(green ? 0x50FF30FFu : 0xFF5010FFu, 0.8f * k),
                        fx + sinf(g->t * 9 + (float)t) * 3, fy - h, col_a(green ? 0xD0FF80FFu : 0xFFE080FFu, 0.0f));
            }
        }
    }
}

static void draw_corpses(const Game *g)
{
    for (int i = 0; i < MAX_CORPSE; i++) {
        const Corpse *c = &g->corpse[i];
        if (!c->alive) continue;
        float x = SX(c->x), y = SY(c->y);
        if (!onscreen(x, y, 40 * c->s)) continue;
        art_corpse(c, x, y, clampf(c->life / 4.0f, 0, 1));
    }
}

/* ------------------------------------------------------------------ */
/* the horde and the gunslinger, sorted by depth                         */
/* ------------------------------------------------------------------ */
static const float EYE_OFF[A_COUNT][3] = {
    { 0, -10, 4.2f }, { 2, -8, 0 }, { 0, -9, 6 }, { 0, -10, 4 },
    { 0, -6, 10 }, { 2, -8, 0 }, { 0, -6, 9 }, { 0, -10, 4 },
    { 1, -9, 4 }, { 0, -8, 3.4f }, { 0, -11, 6 }, { 0, -7, 6 },
    { 1, -9, 4 }, { 1, -1, 5 }, { 1, -16, 6 }, { 1, -11, 4 },
    { 0, -2, 8 }, { 3, -2, 0 }, { -6, -9, 4 }, { 0, -12, 3.6f },
    { 0, -20, 7.6f }, { 0, -20, 28 }, { 0, -24, 6 }, { 2.5f, -30, 9 }, { 5, 0, 0 },
};

typedef struct { float y; int16_t id; } Order;
static Order ord[MAX_EN + 2];

static void backup_screen_pos(const Game *g, int s, float *x, float *y, float *depth)
{
    float a = g->t * 1.6f + (float)s * TAU_F / 4.0f;
    *x = SX(g->p.x + cosf(a) * 30.0f); *y = SY(g->p.y - 6.0f + sinf(a) * 22.0f);
    *depth = sinf(a);
}

static void draw_player(const Game *g)
{
    const Player *p = &g->p;
    float x = SX(p->x), y = SY(p->y);
    art_shadow(x, y + 11, 11, 4, 1);
    Anim an;
    memset(&an, 0, sizeof an);
    an.t = g->t; an.run = p->walk;
    an.mv = clampf(sqrtf(p->vx * p->vx + p->vy * p->vy) / 150.0f, 0, 1);
    an.hurt = clampf(p->hurt_t / 0.3f, 0, 1);
    an.lookx = fabsf(p->ax); an.looky = p->ay;
    an.blink = fmodf(g->t, 3.7f) < 0.1f ? 1.0f : 0.0f;
    an.firing = p->firing; an.seed = 77;
    an.wounds = p->hp < p->maxhp * 0.75f ? (p->hp < p->maxhp * 0.5f ? (p->hp < p->maxhp * 0.25f ? 3 : 2) : 1) : 0;
    if (p->hp <= 0) {      /* down, in a spreading pool */
        an.dead = 1; an.mv = 0;
        art_player_ex(x, y + 4, p->character, p->face, &an, 1.0f, -1.45f * p->face);
        return;
    }
    int blink = p->inv > 0 && p->stand_t <= 0 && p->hurt_t <= 0 && ((int)(g->t * 20.0f) & 1);
    /* backups behind */
    for (int pass = 0; pass < 2; pass++) {
        for (int s = 0; s < NSLOTS; s++) {
            int w = p->slot[s];
            if (w < 0 || s == p->sel) continue;
            float bx, by, d;
            backup_screen_pos(g, s, &bx, &by, &d);
            if ((d < 0) != (pass == 0)) continue;
            float ang = atan2f(p->ay, p->ax);
            if (p->target >= 0 && g->en[p->target].alive) ang = atan2f(SY(g->en[p->target].y) - by, SX(g->en[p->target].x) - bx);
            art_shadow(bx, by + 18, 5, 2, 0.5f);
            art_weapon_held(w, bx, by, ang, 0.9f, 0.4f, g->t);
        }
        if (pass == 0 && !blink) {
            float rec = p->recoil * 3.0f;
            float px = x - p->ax * rec * 0.4f, py = y - p->ay * rec * 0.4f;
            int w = p->slot[p->sel];
            float ang = atan2f(p->ay, p->ax);
            float hx = px + p->ax * (9.0f - rec), hy = py - 3.0f + p->ay * (7.0f - rec);
            int gun_behind = p->ay < -0.3f;
            if (w >= 0 && gun_behind) art_weapon_held(w, hx, hy, ang, 1.1f, p->spin, g->t);
            art_player_ex(px, py, p->character, p->face, &an, 1.0f, 0);
            if (w >= 0 && !gun_behind) art_weapon_held(w, hx, hy, ang, 1.1f, p->spin, g->t);
        }
    }
}

static void draw_entities(const Game *g)
{
    int n = 0;
    for (int i = 0; i < g->nen; i++) {
        const Enemy *e = &g->en[i];
        if (!e->alive) continue;
        if (!onscreen(SX(e->x), SY(e->y), e->r * 2.5f + 20)) continue;
        ord[n].y = e->y; ord[n].id = (int16_t)i; n++;
    }
    art_set_detail(n < 45);
    ord[n].y = g->p.y; ord[n].id = -1; n++;
    /* insertion sort: nearly sorted from frame to frame anyway */
    for (int i = 1; i < n; i++) {
        Order o = ord[i];
        int j = i - 1;
        while (j >= 0 && ord[j].y > o.y) { ord[j + 1] = ord[j]; j--; }
        ord[j + 1] = o;
    }
    /* shadows first, all together */
    for (int k = 0; k < n; k++) {
        if (ord[k].id < 0) continue;
        const Enemy *e = &g->en[ord[k].id];
        float lift = (e->flags & EF_FLYING) ? 1.0f : 0.0f;
        float sr = EDEF[e->type].ai == AI_BOSS ? e->r * 1.4f : e->r;
        art_shadow(SX(e->x), SY(e->y) + sr * (0.9f + lift * 0.9f), sr * (lift ? 0.7f : 1.0f), sr * 0.35f, lift ? 0.6f : 1.0f);
    }
    for (int k = 0; k < n; k++) {
        if (ord[k].id < 0) { draw_player(g); continue; }
        const Enemy *e = &g->en[ord[k].id];
        float lift = (e->flags & EF_FLYING) ? -8.0f : 0.0f;
        if (EDEF[e->type].ai == AI_BOSS && e->state == 1 + 5 && e->sub) lift = -sinf((float)e->sub / 255.0f * PI_F) * 90.0f;   /* slam leap */
        art_enemy(g, e, SX(e->x), SY(e->y) + lift);
    }
}

static void draw_pickups(const Game *g)
{
    for (int i = 0; i < g->npu; i++) {
        const Pickup *u = &g->pu[i];
        if (!u->alive) continue;
        float x = SX(u->x), y = SY(u->y);
        if (!onscreen(x, y, 20)) continue;
        if (u->kind > PU_GEM) art_shadow(x, y + 7, 6, 2, 0.8f);
        art_pickup(u, x, y, g->t);
    }
    for (int k = 0; k < MAX_BARREL; k++) {
        const Barrel *b = &g->bar[k];
        if (!b->alive) continue;
        float x = SX(b->x), y = SY(b->y);
        if (!onscreen(x, y, 30)) continue;
        art_shadow(x, y + 10, 10, 3.5f, 1);
        art_barrel(x, y, g->t);
    }
}

/* ------------------------------------------------------------------ */
/* projectiles                                                          */
/* ------------------------------------------------------------------ */
static void draw_bombs(const Game *g)
{
    for (int i = 0; i < MAX_BOMB; i++) {
        const Bomb *m = &g->bomb[i];
        if (!m->alive) continue;
        float x = SX(m->x), y = SY(m->y);
        if (m->enemy) {
            /* the target marker: a ring closing in as it falls */
            float k = clampf(1.0f - m->z / 420.0f, 0, 1);
            gfx_ring(x, y, m->rad * (1.3f - 0.3f * k), 2.0f + k * 2, 18, 0, col_a(0xFF3020FFu, 0.4f + 0.5f * k));
            gfx_poly(x, y, m->rad * k, 16, 0, col_a(0xFF2010FFu, 0.18f));
            continue;
        }
        art_shadow(x, y + 3, 6, 2, 0.8f);
        art_weapon_held(W_DYNAMITE, x, y - m->z, m->rot, 1.0f, 0, g->t);
    }
}

static void draw_ebullets_solid(const Game *g)
{
    for (int i = 0; i < g->neb; i++) {
        const EBullet *b = &g->eb[i];
        if (!b->alive) continue;
        float x = SX(b->x), y = SY(b->y);
        if (!onscreen(x, y, 16)) continue;
        if (b->kind == EB_BONE) {
            float c = cosf(b->rot) * 5.0f, s = sinf(b->rot) * 5.0f;
            gfx_line(x - c, y - s, x + c, y + s, 4.0f, 0x140E14FFu, 0x140E14FFu);
            gfx_line(x - c, y - s, x + c, y + s, 2.2f, 0xEDE4D0FFu, 0xEDE4D0FFu);
            gfx_poly(x - c, y - s, 2.0f, 6, 0, 0xEDE4D0FFu); gfx_poly(x + c, y + s, 2.0f, 6, 0, 0xEDE4D0FFu);
        }
    }
}

static void draw_ebullets_glow(const Game *g)
{
    for (int i = 0; i < g->neb; i++) {
        const EBullet *b = &g->eb[i];
        if (!b->alive) continue;
        float x = SX(b->x), y = SY(b->y);
        if (!onscreen(x, y, 30)) continue;
        uint32_t c, core = 0xFFFFFFFFu;
        switch (b->kind) {
        case EB_BONE: gfx_glow_cheap(x, y, 10, 0xFFE0A040u); continue;
        case EB_GOO: c = 0x70FF40FFu; core = 0xE0FFB0FFu; break;
        case EB_POISON: c = 0x60FF30FFu; core = 0x203010FFu; break;
        case EB_HOMING: case EB_VOID: c = 0xC040FFFFu; core = 0xFFD0FFFFu; break;
        case EB_SKULL: c = 0x40E0FFFFu; core = 0xE0FFFFFFu; break;
        case EB_FIRE: c = 0xFF6020FFu; core = 0xFFF0A0FFu; break;
        default: c = 0x30C8FFFFu; core = 0xE8FFFFFFu; break;
        }
        float pul = 1.0f + 0.12f * sinf(g->t * 18.0f + (float)i);
        gfx_glow_cheap(x, y, b->r * 3.2f * pul, col_a(c, 0.5f));
        gfx_poly(x, y, b->r * 1.05f, 8, 0, col_a(c, 0.9f));
        gfx_poly(x, y, b->r * 0.55f, 6, 0, core);
        if (b->kind == EB_SKULL) {   /* a little skull face */
            gfx_poly(x - 2.5f, y - 1, 1.6f, 5, 0, 0x003040FFu); gfx_poly(x + 2.5f, y - 1, 1.6f, 5, 0, 0x003040FFu);
        }
        if (b->kind == EB_HOMING || b->kind == EB_VOID) gfx_capsule(x, y, b->vx, b->vy, 22, b->r * 1.4f, col_a(c, 0.5f));
    }
}

static void draw_bullets(const Game *g)
{
    for (int i = 0; i < g->nbul; i++) {
        const Bullet *b = &g->bul[i];
        float x = SX(b->x), y = SY(b->y);
        if (!onscreen(x, y, 40)) continue;
        switch (b->kind) {
        case PB_PELLET:
            gfx_capsule(x, y, b->vx, b->vy, 16, 7, 0xFF6010C0u);
            gfx_capsule(x, y, b->vx, b->vy, 9, 3.5f, 0xFFF0A0FFu);
            break;
        case PB_GATLING:
            gfx_capsule(x, y, b->vx, b->vy, 26, 3.2f, 0xFFD050D0u);
            gfx_capsule(x, y, b->vx, b->vy, 12, 1.6f, 0xFFFFE0FFu);
            break;
        default:
            gfx_capsule(x, y, b->vx, b->vy, 22, b->crit ? 6.0f : 4.5f, b->crit ? 0xFFF060E0u : 0xFFA030D0u);
            gfx_capsule(x, y, b->vx, b->vy, 10, 2.2f, 0xFFFFF0FFu);
            break;
        }
    }
}

/* ------------------------------------------------------------------ */
/* particles                                                            */
/* ------------------------------------------------------------------ */
static void draw_particles(const Game *g, int add)
{
    for (int i = 0; i < g->npt; i++) {
        const Particle *q = &g->pt[i];
        if (q->add != add) continue;
        float x = SX(q->x), y = SY(q->y) - q->z;
        if (!onscreen(x, y, q->size * 3 + 10)) continue;
        float k = q->life / q->max;      /* 1 -> 0 */
        uint32_t c = col_mix(q->c1, q->c0, k);
        switch (q->kind) {
        case PT_SPARK: gfx_capsule(x, y, q->vx, q->vy, 4.0f + sqrtf(q->vx * q->vx + q->vy * q->vy) * 0.025f, q->size, col_a(c, k)); break;
        case PT_FIRE: {
            float r = q->size * (0.5f + 0.8f * k);
            gfx_poly2(x, y, r * 1.3f, 7, q->rot, col_a(col_mix(c, 0xFFFFE0FFu, k * 0.4f), k * 0.55f), col_a(c, 0.0f));
            break;
        }
        case PT_SMOKE: gfx_poly2(x, y, q->size * (1.4f - 0.4f * k), 6, q->rot, col_a(c, 0.75f * k), col_a(c, 0.0f)); break;
        case PT_DUST: gfx_poly2(x, y, q->size, 6, q->rot, col_a(q->c0, k), col_a(q->c0, 0)); break;
        case PT_GOO: gfx_poly(x, y, q->size * (0.6f + 0.4f * k), 5, q->rot, c); break;
        case PT_CHUNK: {
            float s = q->size, cs = cosf(q->rot) * s, sn = sinf(q->rot) * s;
            gfx_quad4(x + cs, y + sn, q->c0, x - sn, y + cs, q->c1, x - cs, y - sn, q->c0, x + sn, y - cs, q->c1);
            break;
        }
        case PT_CASING: {
            float cs = cosf(q->rot) * 2.6f, sn = sinf(q->rot) * 2.6f;
            gfx_line(x - cs, y - sn, x + cs, y + sn, 1.8f, q->c0, q->c1);
            break;
        }
        case PT_BONE: {
            float cs = cosf(q->rot) * q->size, sn = sinf(q->rot) * q->size;
            gfx_line(x - cs, y - sn, x + cs, y + sn, 2.0f, 0xEDE4D0FFu, 0xEDE4D0FFu);
            gfx_poly(x - cs, y - sn, 1.6f, 5, 0, 0xEDE4D0FFu); gfx_poly(x + cs, y + sn, 1.6f, 5, 0, 0xEDE4D0FFu);
            break;
        }
        case PT_EMBER: gfx_poly2(x, y, q->size * 2.2f, 5, q->rot, col_a(0xFFF0C0FFu, k), col_a(c, 0.0f)); break;
        case PT_SOUL: gfx_glow(x, y, q->size * (1.0f + (1 - k)), col_a(q->c0, 0.6f * k)); break;
        case PT_STAR: {
            float s = q->size * (0.6f + 0.6f * k);
            gfx_glow(x, y, s * 1.6f, col_a(q->c1, 0.8f * k));
            gfx_tri(x - s, y, 0xFFFFE0FFu, x, y - s * 0.25f, 0xFFFFE0FFu, x + s, y, 0xFFFFE000u);
            gfx_tri(x - s, y, 0xFFFFE000u, x, y + s * 0.25f, 0xFFFFE0FFu, x + s, y, 0xFFFFE0FFu);
            gfx_tri(x, y - s * 0.7f, 0xFFFFE0FFu, x + s * 0.2f, y, 0xFFFFE0FFu, x, y + s * 0.7f, 0xFFFFE000u);
            break;
        }
        case PT_BLOOD: {
            float ang = atan2f(q->vy - q->vz * 0.3f, q->vx), l = q->size * 1.6f;
            float cs = cosf(ang) * l, sn = sinf(ang) * l;
            gfx_line(x - cs, y - sn, x + cs * 0.4f, y + sn * 0.4f, q->size, q->c1, q->c0);
            break;
        }
        case PT_MIST: gfx_poly2(x, y, q->size, 7, q->rot, col_a(q->c0, k * 0.6f), q->c1 & 0xFFFFFF00u); break;
        case PT_GIB: {
            float s = q->size, cs = cosf(q->rot) * s, sn = sinf(q->rot) * s;
            gfx_quad4(x + cs, y + sn, 0x120808FFu, x - sn * 0.8f, y + cs * 0.8f, 0x120808FFu, x - cs, y - sn, 0x120808FFu, x + sn * 0.8f, y - cs * 0.8f, 0x120808FFu);
            gfx_quad4(x + cs * 0.7f, y + sn * 0.7f, q->c0, x - sn * 0.55f, y + cs * 0.55f, q->c1, x - cs * 0.7f, y - sn * 0.7f, q->c0, x + sn * 0.55f, y - cs * 0.55f, q->c1);
            break;
        }
        case PT_HEAD: art_head((int)q->c1, x, y - 2, q->size, q->rot, 1); break;
        case PT_RING: {   /* a dash after-image */
            gfx_poly2(x, y, 11, 10, 0, col_a(q->c0, 0.5f * k), col_a(q->c0, 0));
            break;
        }
        }
    }
}

/* ------------------------------------------------------------------ */
/* explosions, beams, telegraphs                                        */
/* ------------------------------------------------------------------ */
static void draw_booms(const Game *g)
{
    for (int i = 0; i < MAX_BOOM; i++) {
        const Boom *b = &g->boom[i];
        if (b->t >= b->dur) continue;
        float x = SX(b->x), y = SY(b->y);
        float k = b->t / b->dur;
        if (!onscreen(x, y, b->r * 2)) continue;
        if (b->kind == 3) {     /* a shockwave ring */
            float r = b->r * (b->hurt ? k : (0.2f + 0.8f * sqrtf(k)));
            uint32_t c = b->hurt ? 0xFF4020FFu : (g->biome >= 3 ? 0xC060FFFFu : 0xFFE0B0FFu);
            gfx_ring2(x, y, r, 14.0f * (1.0f - k) + 4.0f, 36, 0, col_a(c, 0.9f * (1 - k)), col_a(c, 0.0f));
            gfx_ring(x, y, r, 3.0f, 36, 0, col_a(0xFFFFFFFFu, 0.7f * (1 - k)));
            continue;
        }
        uint32_t hot = 0xFFF8C0FFu, mid = 0xFFA030FFu, out = 0xFF3008FFu;
        if (b->kind == 1) { hot = 0xE0FFB0FFu; mid = 0x80FF40FFu; out = 0x208010FFu; }
        if (b->kind == 2) { hot = 0xFFD0FFFFu; mid = 0xC050FFFFu; out = 0x5010A0FFu; }
        float r = b->r * (0.45f + 0.75f * sqrtf(k));
        gfx_glow(x, y, r * 2.0f, col_a(out, 0.32f * (1 - k)));
        /* a billowing fireball: lumps rolling outwards round a white-hot core */
        float fa = (1.0f - k) * (1.0f - k);
        uint32_t seed = (uint32_t)(b->x * 7.0f + b->y * 13.0f);
        for (int j = 0; j < 6; j++) {
            float an = (float)j * 1.047f + (float)(seed & 7) + k * 1.5f;
            float rr = r * (0.38f + 0.12f * (float)((hash32(seed + (uint32_t)j) >> 8) & 3));
            gfx_poly2(x + cosf(an) * r * 0.45f, y + sinf(an) * r * 0.4f - k * r * 0.3f, rr, 9, an, col_a(col_mix(hot, mid, k), 0.55f * fa), col_a(out, 0.0f));
        }
        gfx_poly2(x, y, r * 0.42f, 10, -k * 2.0f, col_a(0xFFFFF0FFu, 0.75f * fa), col_a(hot, 0.0f));
        gfx_ring2(x, y, b->r * (0.3f + 1.4f * k), 10.0f * (1 - k) + 2, b->big ? 24 : 16, 0, col_a(0xFFFFFFFFu, 0.0f), col_a(hot, 0.7f * (1 - k)));
        if (k < 0.12f) gfx_glow_cheap(x, y, b->r * 2.4f, col_a(0xFFFFFFFFu, 0.35f * (1 - k / 0.12f)));
    }
}

static void lightning_path(float x0, float y0, float x1, float y1, uint32_t seed, float w, uint32_t c, int depth)
{
    float dx = x1 - x0, dy = y1 - y0, l = sqrtf(dx * dx + dy * dy);
    int n = (int)(l / 16.0f) + 2;
    if (n > 10) n = 10;
    float nx = -dy / (l + 1e-3f), ny = dx / (l + 1e-3f);
    float px = x0, py = y0;
    for (int i = 1; i <= n; i++) {
        float u = (float)i / (float)n;
        float j = i == n ? 0.0f : ((float)(hash32(seed + (uint32_t)i * 2654435761u) & 255) / 255.0f - 0.5f) * l * 0.28f;
        float x = x0 + dx * u + nx * j, y = y0 + dy * u + ny * j;
        gfx_glowline(px, py, x, y, w * 4.0f, col_a(c, 0.6f));
        gfx_line(px, py, x, y, w, 0xFFFFFFFFu, 0xFFFFFFFFu);
        if (depth == 0 && (hash32(seed ^ (uint32_t)i) & 3) == 0) {
            float ba = ((float)(hash32(seed + (uint32_t)i) & 255) / 255.0f - 0.5f) * 2.0f;
            lightning_path(x, y, x + (dx / l * cosf(ba) - dy / l * sinf(ba)) * l * 0.3f, y + (dy / l * cosf(ba) + dx / l * sinf(ba)) * l * 0.3f, seed * 7u + (uint32_t)i, w * 0.6f, c, 1);
        }
        px = x; py = y;
    }
}

static void draw_beams(const Game *g)
{
    uint32_t flick = (uint32_t)(g->t * 30.0f);
    for (int i = 0; i < MAX_BEAM; i++) {
        const Beam *b = &g->beam[i];
        if (b->life <= 0) continue;
        float k = b->life / b->max;
        float x0 = SX(b->x0), y0 = SY(b->y0), x1 = SX(b->x1), y1 = SY(b->y1);
        if (b->kind == BM_ARC) {
            lightning_path(x0, y0, x1, y1, b->seed + flick * 977u, b->w * (0.5f + 0.5f * k), 0x50B8FFFFu, 0);
            gfx_glow_cheap(x1, y1, 16, col_a(0x80D0FFFFu, 0.8f * k));
        } else if (b->kind == BM_RAIL) {
            float w = b->w * (0.4f + 0.9f * k);
            gfx_glowline(x0, y0, x1, y1, w * 3.5f, col_a(0xFF30D0FFu, 0.7f * k));
            gfx_glowline(x0, y0, x1, y1, w * 1.4f, col_a(0xFFA0F0FFu, 0.9f * k));
            gfx_line(x0, y0, x1, y1, w * 0.45f, col_a(0xFFFFFFFFu, k), col_a(0xFFFFFFFFu, k * 0.5f));
            gfx_glow(x0, y0, w * 3.0f, col_a(0xFFC0FFFFu, k));
        }
    }
    /* the Maw's laser */
    if (g->boss >= 0 && g->laser_on > 0 && g->en[g->boss].alive) {
        const Enemy *e = &g->en[g->boss];
        float x0 = SX(e->x) + 5, y0 = SY(e->y), lx = cosf(g->laser_a), ly = sinf(g->laser_a);
        float x1 = x0 + lx * 1000.0f, y1 = y0 + ly * 1000.0f;
        if (g->laser_on < 1.0f) {
            float fl = 0.5f + 0.5f * sinf(g->t * 40.0f);
            gfx_line(x0, y0, x1, y1, 2.0f, col_a(0xFF2040FFu, 0.4f + 0.4f * fl), col_a(0xFF2040FFu, 0.0f));
        } else {
            float w = 34.0f + sinf(g->t * 50.0f) * 4.0f;
            gfx_glowline(x0, y0, x1, y1, w * 2.6f, 0xFF1040B0u);
            gfx_glowline(x0, y0, x1, y1, w, 0xFF80A0FFu);
            gfx_line(x0, y0, x1, y1, w * 0.35f, 0xFFFFFFFFu, 0xFFFFFFFFu);
            gfx_glow(x0, y0, 70, 0xFF6080FFu);
        }
    }
}

static void draw_telegraphs(const Game *g)
{
    if (g->boss < 0 || !g->en[g->boss].alive) return;
    const Enemy *e = &g->en[g->boss];
    int pat = e->state - 1;
    if (pat == 3 && e->st < 0.9f) {     /* charge: an arrow along the path */
        float x0 = SX(e->x), y0 = SY(e->y), x1 = SX(g->btx), y1 = SY(g->bty);
        float dx = x1 - x0, dy = y1 - y0, l = sqrtf(dx * dx + dy * dy) + 1e-3f;
        x1 = x0 + dx / l * 460.0f; y1 = y0 + dy / l * 460.0f;
        float a = 0.25f + 0.25f * sinf(g->t * 30.0f);
        gfx_line(x0, y0, x1, y1, e->r * 1.6f, col_a(0xFF2010FFu, a), col_a(0xFF2010FFu, 0.0f));
    }
    if (pat == 5 && e->sub) {          /* slam: where it lands */
        float k = (float)e->sub / 255.0f;
        float x = SX(g->btx), y = SY(g->bty);
        gfx_ring(x, y, e->r * (2.2f - k), 3, 24, 0, col_a(0xFF3020FFu, 0.8f));
        gfx_poly(x, y, e->r * 1.2f * k, 20, 0, col_a(0xFF2010FFu, 0.25f));
    }
}

/* ------------------------------------------------------------------ */
/* glows: eyes in the dark, the player's eyes, coins twinkling          */
/* ------------------------------------------------------------------ */
static void draw_glows(const Game *g)
{
    float dark = g->biome >= 2 ? 1.0f : 0.6f;
    for (int i = 0; i < g->nen; i++) {
        const Enemy *e = &g->en[i];
        if (!e->alive) continue;
        float x = SX(e->x), y = SY(e->y);
        if (!onscreen(x, y, 40)) continue;
        const EnemyDef *d = &EDEF[e->type];
        float s = art_scale(e);
        float lift = (e->flags & EF_FLYING) ? -8.0f : 0.0f;
        float ex = x + EYE_OFF[e->type][0] * s * (e->face < 0 ? -1.0f : 1.0f), ey = y + lift + EYE_OFF[e->type][1] * s, sep = EYE_OFF[e->type][2] * s;
        uint32_t c = col_a(d->eye, 0.55f * dark);
        float r = (d->ai == AI_BOSS ? 14.0f : 5.0f) * (e->elite ? 1.5f : 1.0f);
        if (sep > 0) { gfx_glow_cheap(ex - sep * 0.5f, ey, r, c); gfx_glow_cheap(ex + sep * 0.5f, ey, r, c); }
        else gfx_glow_cheap(ex, ey, r * 1.3f, c);
        if (e->type == A_LANTERN) gfx_glow(x + 11 * s * e->face, y + 7 * s, 20, 0x40FF8070u);
        if (e->type == A_DOCTOR) gfx_glow(x + 12 * s * e->face, y + 3 * s, 14, 0x70FF4060u);
        if (e->type == A_IMP) gfx_glow(x, y - 10 * s, 18, 0xFF902060u);
        if (e->type == A_HULK || e->type == A_MOSS || e->type == A_PRIEST) gfx_glow_cheap(x, y, 26 * s, col_a(d->c1, 0.22f));
        if (e->elite) gfx_glow(x, y, e->r * 2.6f, col_a(0xFFC030FFu, 0.25f + 0.1f * sinf(g->t * 6)));
        if (e->burn_t > 0) gfx_glow_cheap(x, y, e->r * 1.6f, 0xFF602050u);
    }
    const Player *p = &g->p;
    if (p->hp > 0) {
        float x = SX(p->x), y = SY(p->y);
        float b = -fabsf(sinf(p->walk)) * 1.6f;
        gfx_glow_cheap(x + 1.0f * p->face - 2.2f, y - 12 + b, 4, 0xFFC03080u);
        gfx_glow_cheap(x + 1.0f * p->face + 2.2f, y - 12 + b, 4, 0xFFC03080u);
        if (p->frenzy > 0) { gfx_ring2(x, y, 22 + sinf(g->t * 12) * 3, 10, 20, g->t, 0xFF40E000u, 0xFF40E090u); }
        if (p->stand_t > 0) { gfx_glow(x, y, 70 + sinf(g->t * 20) * 8, 0xFF302080u); gfx_ring(x, y, 30 + fmodf(g->t * 80, 40), 3, 20, 0, 0xFF6040A0u); }
        if (p->magnet_t > 0) gfx_ring(x, y, 40 + fmodf(g->t * 60, 30), 2, 20, 0, 0x60C0FF80u);
        /* the held weapon's ambient glow */
        int w = p->slot[p->sel];
        static const uint32_t WG[NWEAP] = { 0xFFA03030u, 0xFF602040u, 0xFFD04028u, 0xFF804030u, 0x60C0FF50u, 0xFF40E040u };
        if (w >= 0) gfx_glow_cheap(x + p->ax * 14, y - 4 + p->ay * 10, 22, WG[w]);
        for (int s = 0; s < NSLOTS; s++) {
            int bw = p->slot[s];
            if (bw < 0 || s == p->sel) continue;
            float bx, by, d;
            backup_screen_pos(g, s, &bx, &by, &d);
            gfx_glow_cheap(bx, by, 14, WG[bw]);
        }
    }
    for (int i = 0; i < g->npu; i++) {
        const Pickup *u = &g->pu[i];
        if (!u->alive) continue;
        float x = SX(u->x), y = SY(u->y);
        if (!onscreen(x, y, 20)) continue;
        if (u->kind == PU_GEM) gfx_glow_cheap(x, y, 12, 0x30D8F070u);
        else if (u->kind <= PU_GOLD) { if (((i + (int)(g->t * 8)) % 7) == 0) gfx_glow_cheap(x, y, 7, 0xFFF0A0A0u); }
        else gfx_glow(x, y, 22 + sinf(g->t * 6) * 3, u->kind == PU_CHEST ? 0xFFC03070u : (u->kind == PU_FRENZY ? 0xE040FF70u : (u->kind == PU_HEALTH ? 0x60FF6060u : 0xFF603060u)));
    }
    for (int i = 0; i < MAX_BOMB; i++) {
        const Bomb *m = &g->bomb[i];
        if (!m->alive || m->enemy) continue;
        gfx_glow_cheap(SX(m->x), SY(m->y) - m->z, 12, 0xFFC04090u);
    }
}

/* ------------------------------------------------------------------ */
/* lighting: a grid of light values drawn as a multiply over the scene  */
/* ------------------------------------------------------------------ */
#define LGW 40
#define LGH 24
static float lr[LGH + 1][LGW + 1], lg_[LGH + 1][LGW + 1], lb[LGH + 1][LGW + 1];

static void draw_lighting(const Game *g)
{
    const float cw = VW / (float)LGW, ch = VH / (float)LGH;
    float ar = (float)BIO[g->biome].amb[0], ag = (float)BIO[g->biome].amb[1], ab = (float)BIO[g->biome].amb[2];
    if (g->lightning > 0) { float k = g->lightning * g->lightning; ar += 110 * k; ag += 115 * k; ab += 140 * k; }
    if (g->flash > 0) { ar += 120 * g->flash * g->flash_r; ag += 120 * g->flash * g->flash_g; ab += 120 * g->flash * g->flash_b; }
    for (int j = 0; j <= LGH; j++) for (int i = 0; i <= LGW; i++) { lr[j][i] = ar; lg_[j][i] = ag; lb[j][i] = ab; }
    for (int k = 0; k < g->nlight; k++) {
        const Light *L = &g->light[k];
        float x = (SX(L->x) - VW * 0.5f) * ZM + VW * 0.5f, y = (SY(L->y) - VH * 0.5f) * ZM + VH * 0.5f, r = L->r * ZM, inv = 1.0f / (r * r);
        int i0 = (int)((x - r) / cw), i1 = (int)((x + r) / cw) + 1, j0 = (int)((y - r) / ch), j1 = (int)((y + r) / ch) + 1;
        if (i0 < 0) i0 = 0; if (j0 < 0) j0 = 0; if (i1 > LGW) i1 = LGW; if (j1 > LGH) j1 = LGH;
        for (int j = j0; j <= j1; j++) {
            float dy = (float)j * ch - y, dy2 = dy * dy;
            for (int i = i0; i <= i1; i++) {
                float dx = (float)i * cw - x, d2 = dx * dx + dy2;
                if (d2 >= r * r) continue;
                float f = 1.0f - d2 * inv;
                f *= f;
                lr[j][i] += (float)L->cr * f; lg_[j][i] += (float)L->cg * f; lb[j][i] += (float)L->cb * f;
            }
        }
    }
    gfx_view_reset();
    gfx_blend(BL_MUL);
    /* write the grid straight into the vertex list: two triangles per cell */
    for (int j = 0; j < LGH; j++)
        for (int i = 0; i < LGW; i++) {
            uint32_t c00 = RGBA((int)lr[j][i], (int)lg_[j][i], (int)lb[j][i], 255);
            uint32_t c10 = RGBA((int)lr[j][i + 1], (int)lg_[j][i + 1], (int)lb[j][i + 1], 255);
            uint32_t c01 = RGBA((int)lr[j + 1][i], (int)lg_[j + 1][i], (int)lb[j + 1][i], 255);
            uint32_t c11 = RGBA((int)lr[j + 1][i + 1], (int)lg_[j + 1][i + 1], (int)lb[j + 1][i + 1], 255);
            float x0 = (float)i * cw, y0 = (float)j * ch;
            gfx_quad4(x0, y0, c00, x0 + cw, y0, c10, x0 + cw, y0 + ch, c11, x0, y0 + ch, c01);
        }
    gfx_view(0, 0, ZM);
}

/* ------------------------------------------------------------------ */
/* weather and the frame                                                */
/* ------------------------------------------------------------------ */
static void draw_weather(const Game *g)
{
    if (g->biome == 2 || g->biome == 1) {     /* rain: the graveyard pours, the bayou drizzles */
        int n = g->biome == 2 ? 140 : 50;
        for (int i = 0; i < n; i++) {
            uint32_t h = hash32((uint32_t)i * 7919u + 13u);
            float sp = 700.0f + (float)(h & 255);
            float x = fmodf((float)((h >> 8) & 1023) - g->camx * 0.3f + g->t * 120.0f, VW + 40.0f);
            if (x < 0) x += VW + 40.0f;
            float y = fmodf((float)((h >> 18) & 511) + g->t * sp - g->camy * 0.3f, VH + 60.0f);
            if (y < 0) y += VH + 60.0f;
            gfx_line(x - 20, y - 30, x - 26, y - 14, 1.2f, 0xA0B4D000u, 0xA0B4D060u);
        }
    }
    if (g->biome == 2 || g->biome == 4) {     /* fog banks drifting */
        for (int i = 0; i < 7; i++) {
            float x = fmodf((float)i * 197.0f - g->camx * 0.6f + g->t * 14.0f, VW + 400.0f);
            if (x < 0) x += VW + 400.0f;
            x -= 200.0f;
            float y = fmodf((float)i * 131.0f - g->camy * 0.6f, VH + 300.0f);
            if (y < 0) y += VH + 300.0f;
            y -= 150.0f;
            gfx_glow(x, y, 190, g->biome == 2 ? 0x8890A030u : 0x50204028u);
        }
    }
}

static void draw_vignette(const Game *g)
{
    int a = g->biome >= 3 ? 215 : (g->biome == 2 ? 190 : 150);
    if (g->p.hp > 0 && g->p.hp < g->p.maxhp * 0.3f) a += (int)(30.0f * (0.5f + 0.5f * sinf(g->t * 6.0f)));
    uint32_t edge = RGBA(0, 0, 0, a > 255 ? 255 : a), mid = 0x00000000u;
    float cx = VW * 0.5f, cy = VH * 0.5f;
    const int N = 24;
    for (int i = 0; i < N; i++) {
        float a0 = (float)i / (float)N * TAU_F, a1 = (float)(i + 1) / (float)N * TAU_F;
        float c0 = cosf(a0), s0 = sinf(a0), c1 = cosf(a1), s1 = sinf(a1);
        gfx_quad4(cx + c0 * VW * 0.36f, cy + s0 * VH * 0.36f, mid, cx + c0 * VW * 0.80f, cy + s0 * VH * 0.86f, edge,
                  cx + c1 * VW * 0.80f, cy + s1 * VH * 0.86f, edge, cx + c1 * VW * 0.36f, cy + s1 * VH * 0.36f, mid);
    }
    if (g->red > 0) {    /* hurt: red at the edges */
        uint32_t r = RGBA(200, 0, 0, (int)(170 * clampf(g->red, 0, 1)));
        for (int i = 0; i < N; i++) {
            float a0 = (float)i / (float)N * TAU_F, a1 = (float)(i + 1) / (float)N * TAU_F;
            gfx_quad4(cx + cosf(a0) * VW * 0.32f, cy + sinf(a0) * VH * 0.32f, 0xC0000000u, cx + cosf(a0) * VW * 0.8f, cy + sinf(a0) * VH * 0.86f, r,
                      cx + cosf(a1) * VW * 0.8f, cy + sinf(a1) * VH * 0.86f, r, cx + cosf(a1) * VW * 0.32f, cy + sinf(a1) * VH * 0.32f, 0xC0000000u);
        }
    }
    if (g->p.stand_t > 0) gfx_rect(0, 0, VW, VH, RGBA(120, 0, 0, (int)(40 + 20 * sinf(g->t * 10))));
    if (g->p.hp <= 0) gfx_rect(0, 0, VW, VH, RGBA(40, 0, 0, (int)(clampf(g->over_t, 0, 2) * 80.0f)));
}

static void draw_numbers(const Game *g)
{
    for (int i = 0; i < MAX_NUM; i++) {
        const Number *n = &g->num[i];
        float life = n->kind == 2 ? 1.6f : (n->crit ? 0.9f : 0.6f);
        if (n->t >= life) continue;
        float k = n->t / life;
        float x = SX(n->x), y = SY(n->y) - n->t * 40.0f - (n->kind == 2 ? 10 : 0);
        if (!onscreen(x, y, 40)) continue;
        float a = k > 0.7f ? (1 - k) / 0.3f : 1.0f;
        float pop = n->t < 0.08f ? 1.0f + (0.08f - n->t) * 8.0f : 1.0f;
        char buf[24];
        uint32_t c;
        float px;
        if (n->kind == 2) { snprintf(buf, sizeof buf, "%s", n->txt ? n->txt : ""); c = 0xFFE070FFu; px = 1.6f; }
        else if (n->crit) { snprintf(buf, sizeof buf, "%d!", n->val); c = 0xFFD030FFu; px = 2.2f; }
        else { snprintf(buf, sizeof buf, "%d", n->val); c = n->val >= 100 ? 0xFF4030FFu : 0xF0ECE0FFu; px = 1.5f; }
        px *= pop / ZM;
        float w = gfx_text_width(buf, px);
        gfx_text(buf, x - w * 0.5f + 1.2f, y + 1.2f, px, col_a(0x000000FFu, a * 0.9f));
        gfx_text(buf, x - w * 0.5f, y, px, col_a(c, a));
    }
}

/* ------------------------------------------------------------------ */
int dbg_nv[32]; const char *dbg_name[32]; int dbg_n;
#define DBG(n) do { if (dbg_n < 32) { dbg_nv[dbg_n] = gfx_nv; dbg_name[dbg_n++] = n; } } while (0)
void draw_world(const Game *g)
{
    dbg_n = 0;
    ZM = WORLD_ZOOM + g->zoom_punch;
    gfx_view(0, 0, ZM);
    OXs = VW * 0.5f - g->camx + g->shx;
    OYs = VH * 0.5f - g->camy + g->shy;

    gfx_blend(BL_ALPHA);
    draw_floor(g);
    DBG("draw_floor");
    draw_decals(g);
    DBG("draw_decals");
    draw_corpses(g);
    DBG("draw_corpses");
    draw_props(g);
    DBG("draw_props");
    draw_fires(g, 0);
    DBG("draw_fires");
    draw_pickups(g);
    DBG("draw_pickups");
    draw_telegraphs(g);
    DBG("draw_telegraphs");
    draw_entities(g);
    DBG("draw_entities");
    draw_bombs(g);
    DBG("draw_bombs");
    draw_ebullets_solid(g);
    DBG("draw_ebullets_solid");
    draw_particles(g, 0);
    DBG("draw_particles");
    draw_lighting(g);
    DBG("draw_lighting");
    gfx_blend(BL_ADD);
    draw_fires(g, 1);
    DBG("draw_fires");
    draw_glows(g);
    DBG("draw_glows");
    draw_bullets(g);
    DBG("draw_bullets");
    draw_booms(g);
    DBG("draw_booms");
    draw_beams(g);
    DBG("draw_beams");
    draw_particles(g, 1);
    DBG("draw_particles");
    draw_ebullets_glow(g);
    DBG("draw_ebullets_glow");
    prop_glows(g->biome, g->camx - VW * 0.5f - g->shx, g->camy - VH * 0.5f - g->shy, g->t);
    DBG("prop_glows");
    gfx_blend(BL_ALPHA);
    gfx_view_reset();
    draw_weather(g);
    DBG("draw_weather");
    draw_vignette(g);
    DBG("draw_vignette");
    gfx_view(0, 0, ZM);
    draw_numbers(g);
}

/* blood on the lens: splats that run down the screen, drawn after the post-processing */
void draw_lens(const Game *g)
{
    gfx_view_reset();
    gfx_blend(BL_ALPHA);
    for (int i = 0; i < MAX_LENS; i++) {
        const Lens *l = &g->lens[i];
        if (l->r <= 0) continue;
        float a = l->t < 2.2f ? 1.0f : clampf((3.5f - l->t) / 1.3f, 0, 1);
        float r = l->r * (l->t < 0.06f ? l->t / 0.06f : 1.0f);
        uint32_t c = col_a(0x6A0406E0u, a), c2 = col_a(0x9A0A0CD0u, a);
        blob(l->x, l->y, r, l->seed, c, 10);
        blob(l->x - r * 0.1f, l->y - r * 0.12f, r * 0.7f, l->seed * 3u, c2, 8);
        for (int k = 0; k < 6; k++) {           /* spatter around it */
            uint32_t h = hash32(l->seed + (uint32_t)k * 77u);
            float an = (float)(h & 1023) / 1023.0f * TAU_F, dd = r * (1.2f + (float)((h >> 10) & 255) / 255.0f * 1.2f);
            gfx_poly(l->x + cosf(an) * dd, l->y + sinf(an) * dd * 0.8f, r * 0.08f + (float)((h >> 18) & 7) * 0.6f, 5, 0, c);
        }
        for (int k = 0; k < 3; k++) {           /* runs */
            uint32_t h = hash32(l->seed * 13u + (uint32_t)k);
            float dx = ((float)(h & 255) / 255.0f - 0.5f) * r * 1.2f, len = l->drip * (0.5f + (float)((h >> 8) & 255) / 255.0f);
            float w = 1.5f + (float)((h >> 16) & 3);
            gfx_line(l->x + dx, l->y, l->x + dx, l->y + r * 0.5f + len, w, c, c2);
            gfx_poly(l->x + dx, l->y + r * 0.5f + len, w * 0.8f, 6, 0, c2);
        }
        gfx_poly(l->x - r * 0.35f, l->y - r * 0.35f, r * 0.12f, 6, 0, col_a(0xFFC0C0FFu, a * 0.25f));    /* a glint */
    }
}
