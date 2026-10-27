/*
 * LAST STAND - native test harness (no Wii toolchain needed).
 *
 *  - walks the menus, the shop and the save like a player would
 *  - software-rasterizes the exact triangle list the Wii's GPU gets and saves screenshots
 *  - plays whole runs with a bot (dodges, holds the trigger, shops greedily) to check the balance
 *  - measures CPU time and vertex counts per frame
 *  - renders a preview video with the music and sound (reel)
 *
 *  usage: test <outdir> [all|ui|shots|bots|reel] [biome]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "../source/game.h"
#include "../source/game_int.h"
#include "../source/gfx.h"
#include "../source/music.h"
#include "../source/audio.h"

#define OW 1280
#define OH 720
static float fb[OH][OW][3];
static const char *outdir = ".";
static int fails;

#define CHECK(c, ...) do { if (!(c)) { printf("  FAIL: " __VA_ARGS__); printf("\n"); fails++; } } while (0)

/* ------------------------------------------------------------------ */
/* software rasterizer                                                  */
/* ------------------------------------------------------------------ */
/* the Wii's post-processing (see gfx.h and main.c), done on the float framebuffer */
static float pc[OH][OW][3];
#define BW (OW / 4)
#define BH (OH / 4)
static float pb[BH][BW][3], py[BH][BW];
static int post_ready;

static void samp_c(float u, float v, float *o)       /* bilinear, clamped */
{
    float x = u * OW - 0.5f, y = v * OH - 0.5f;
    if (x < 0) x = 0; if (y < 0) y = 0; if (x > OW - 1.001f) x = OW - 1.001f; if (y > OH - 1.001f) y = OH - 1.001f;
    int ix = (int)x, iy = (int)y; float fx = x - ix, fy = y - iy;
    for (int c = 0; c < 3; c++) {
        float a = pc[iy][ix][c] + (pc[iy][ix + 1][c] - pc[iy][ix][c]) * fx;
        float b = pc[iy + 1][ix][c] + (pc[iy + 1][ix + 1][c] - pc[iy + 1][ix][c]) * fx;
        o[c] = a + (b - a) * fy;
    }
}
static void samp_b(float u, float v, float *o, float *lum)
{
    float x = u * BW - 0.5f, y = v * BH - 0.5f;
    if (x < 0) x = 0; if (y < 0) y = 0; if (x > BW - 1.001f) x = BW - 1.001f; if (y > BH - 1.001f) y = BH - 1.001f;
    int ix = (int)x, iy = (int)y; float fx = x - ix, fy = y - iy;
    if (o) for (int c = 0; c < 3; c++) {
        float a = pb[iy][ix][c] + (pb[iy][ix + 1][c] - pb[iy][ix][c]) * fx;
        float b = pb[iy + 1][ix][c] + (pb[iy + 1][ix + 1][c] - pb[iy + 1][ix][c]) * fx;
        o[c] = a + (b - a) * fy;
    }
    if (lum) { float a = py[iy][ix] + (py[iy][ix + 1] - py[iy][ix]) * fx, b = py[iy + 1][ix] + (py[iy + 1][ix + 1] - py[iy + 1][ix]) * fx; *lum = a + (b - a) * fy; }
}
static float edge_k(float x, float y)
{
    float dx = (x / VW - 0.5f) * 2.0f, dy = (y / VH - 0.5f) * 2.0f;
    float d = sqrtf(dx * dx * 0.8f + dy * dy * 1.1f);
    float k = (d - 0.35f) / 0.75f;
    return k < 0 ? 0 : (k > 1 ? 1 : k * k);
}

static void post_apply(void)
{
    if (!post_ready) { gfx_post_init(); post_ready = 1; }
    const GfxPost *P = &gfx_post_p;
    memcpy(pc, fb, sizeof pc);
    for (int y = 0; y < BH; y++) for (int x = 0; x < BW; x++) {
        float acc[3] = { 0, 0, 0 };
        for (int j = 0; j < 4; j++) for (int i = 0; i < 4; i++) for (int c = 0; c < 3; c++) acc[c] += fb[y * 4 + j][x * 4 + i][c];
        for (int c = 0; c < 3; c++) pb[y][x][c] = acc[c] / 16.0f;
        py[y][x] = 0.257f * pb[y][x][0] + 0.504f * pb[y][x][1] + 0.098f * pb[y][x][2] + 16.0f / 255.0f;
    }
    float su = P->split / VW;
    float tr = (float)(P->tint >> 24) / 255.0f, tg = (float)((P->tint >> 16) & 255) / 255.0f, tb = (float)((P->tint >> 8) & 255) / 255.0f;
    for (int y = 0; y < OH; y++) for (int x = 0; x < OW; x++) {
        float u = ((float)x + 0.5f) / OW, v = ((float)y + 0.5f) / OH, o[3], t[3];
        samp_c(u - su, v, t); o[0] = t[0];                 /* red sampled at u - su, like the Wii's texcoords */
        samp_c(u, v, t); o[1] = t[1];
        samp_c(u + su, v, t); o[2] = t[2];
        float xv = u * VW, yv = v * VH;
        float a = P->blur + P->blur_edge * edge_k(xv, yv);
        for (int pass = 0; pass < 2; pass++) {
            float off = pass ? -1.5f : 1.5f, ka = pass ? 0.5f : 0.75f, aa = a * ka;
            if (aa > 1) aa = 1;
            samp_b(u + off / VW, v + off / VH, t, NULL);
            for (int c = 0; c < 3; c++) o[c] = o[c] * (1 - aa) + t[c] * aa;
        }
        float lum;
        samp_b(u, v, NULL, &lum);
        o[0] = o[0] * (1 - P->desat) + lum * tr * P->desat;
        o[1] = o[1] * (1 - P->desat) + lum * tg * P->desat;
        o[2] = o[2] * (1 - P->desat) + lum * tb * P->desat;
        int nx = ((int)floorf((P->gu + xv * POST_NU) * POST_TEX)) & (POST_TEX - 1), ny = ((int)floorf((P->gv + yv * POST_NV) * POST_TEX)) & (POST_TEX - 1);
        float n = (float)gfx_noise_tex[ny * POST_TEX + nx] / 255.0f;
        float dx = u * POST_TEX - 0.5f, dy = v * POST_TEX - 0.5f;
        int ix = ((int)floorf(dx)) & (POST_TEX - 1), iy = ((int)floorf(dy)) & (POST_TEX - 1), ix1 = (ix + 1) & (POST_TEX - 1), iy1 = (iy + 1) & (POST_TEX - 1);
        float fx = dx - floorf(dx), fy = dy - floorf(dy);
        float d0 = gfx_dirt_tex[iy * POST_TEX + ix] + (gfx_dirt_tex[iy * POST_TEX + ix1] - gfx_dirt_tex[iy * POST_TEX + ix]) * fx;
        float d1 = gfx_dirt_tex[iy1 * POST_TEX + ix] + (gfx_dirt_tex[iy1 * POST_TEX + ix1] - gfx_dirt_tex[iy1 * POST_TEX + ix]) * fx;
        float d = (d0 + (d1 - d0) * fy) / 255.0f;
        float gm = 1.0f + P->grain * (n - 0.5f), dm = 1.0f + P->dirt * (d - 0.5f);
        for (int c = 0; c < 3; c++) { float val = o[c] * gm * dm; fb[y][x][c] = val > 1 ? 1 : (val < 0 ? 0 : val); }
    }
}

static void raster(void)
{
    for (int y = 0; y < OH; y++) for (int x = 0; x < OW; x++) fb[y][x][0] = fb[y][x][1] = fb[y][x][2] = 0;
    const float S = (float)OW / VW;
    for (int s = 0; s < gfx_nseg; s++) {
        int mode = gfx_seg[s].mode;
        if (mode == BL_POST) { post_apply(); continue; }
        for (int i = gfx_seg[s].start; i + 2 < gfx_seg[s].start + gfx_seg[s].count; i += 3) {
            const GVtx *v = &gfx_v[i];
            float x0 = v[0].x * S, y0 = v[0].y * S, x1 = v[1].x * S, y1 = v[1].y * S, x2 = v[2].x * S, y2 = v[2].y * S;
            float area = (x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0);
            if (fabsf(area) < 1e-6f) continue;
            int minx = (int)floorf(fminf(x0, fminf(x1, x2))), maxx = (int)ceilf(fmaxf(x0, fmaxf(x1, x2)));
            int miny = (int)floorf(fminf(y0, fminf(y1, y2))), maxy = (int)ceilf(fmaxf(y0, fmaxf(y1, y2)));
            if (minx < 0) minx = 0; if (miny < 0) miny = 0;
            if (maxx > OW - 1) maxx = OW - 1; if (maxy > OH - 1) maxy = OH - 1;
            float c[3][4];
            for (int k = 0; k < 3; k++) {
                uint32_t col = v[k].c;
                c[k][0] = (float)(col >> 24) / 255.0f; c[k][1] = (float)((col >> 16) & 255) / 255.0f;
                c[k][2] = (float)((col >> 8) & 255) / 255.0f; c[k][3] = (float)(col & 255) / 255.0f;
            }
            float inv = 1.0f / area;
            for (int py = miny; py <= maxy; py++) {
                float fy = (float)py + 0.5f;
                for (int px = minx; px <= maxx; px++) {
                    float fx = (float)px + 0.5f;
                    float w0 = ((x1 - fx) * (y2 - fy) - (x2 - fx) * (y1 - fy)) * inv;
                    float w1 = ((x2 - fx) * (y0 - fy) - (x0 - fx) * (y2 - fy)) * inv;
                    float w2 = 1.0f - w0 - w1;
                    if (w0 < 0 || w1 < 0 || w2 < 0) continue;
                    float a = c[0][3] * w0 + c[1][3] * w1 + c[2][3] * w2;
                    for (int ch = 0; ch < 3; ch++) {
                        float src = c[0][ch] * w0 + c[1][ch] * w1 + c[2][ch] * w2;
                        float d;
                        if (mode == BL_ADD) d = fb[py][px][ch] + src * a;
                        else if (mode == BL_MUL) d = 2.0f * src * fb[py][px][ch];
                        else d = fb[py][px][ch] * (1 - a) + src * a;
                        fb[py][px][ch] = d > 1 ? 1 : (d < 0 ? 0 : d);
                    }
                }
            }
        }
    }
}

static void save_ppm(const char *path)
{
    FILE *f = fopen(path, "wb");
    if (!f) return;
    fprintf(f, "P6\n%d %d\n255\n", OW, OH);
    static unsigned char row[OW * 3];
    for (int y = 0; y < OH; y++) {
        for (int x = 0; x < OW; x++)
            for (int ch = 0; ch < 3; ch++) row[x * 3 + ch] = (unsigned char)(fb[y][x][ch] * 255.0f + 0.5f);
        fwrite(row, 1, sizeof row, f);
    }
    fclose(f);
}

static void shot(const Game *g, const char *name)
{
    game_render(g);
    raster();
    char p[512];
    snprintf(p, sizeof p, "%s/%s.ppm", outdir, name);
    save_ppm(p);
    printf("  shot %s (%d vertices)\n", name, gfx_nv);
    extern int dbg_nv[32], dbg_n; extern const char *dbg_name[32];
    if (gfx_nv > 50000) { int prev = 0; for (int i = 0; i < dbg_n; i++) { printf("     %-20s %6d\n", dbg_name[i], dbg_nv[i] - prev); prev = dbg_nv[i]; } }
}

/* ------------------------------------------------------------------ */
/* the bot                                                              */
/* ------------------------------------------------------------------ */
static Game *G;
static uint32_t bot_rng = 1;

static uint32_t prev_held;
static void step_in(Game *g, uint32_t held, float sx, float sy)
{
    Input in;
    memset(&in, 0, sizeof in);
    in.held = held; in.pressed = held & ~prev_held; in.sx = sx; in.sy = sy;
    prev_held = held;
    game_update(g, &in, 1.0f / 60.0f);
    int p; float pan, vol;
    while (game_pop_sound(g, &p, &pan, &vol) >= 0) {}
}
static void step(Game *g, uint32_t held) { step_in(g, held, 0, 0); }
static void press(Game *g, uint32_t b) { step(g, b); step(g, 0); }
static void run(Game *g, float secs) { for (int i = 0; i < (int)(secs * 60); i++) step(g, 0); }

/* dodge, kite in a circle, grab coins when it's quiet, dash out of trouble */
static float bot_ang;
static void bot_input(Game *g, Input *in, int skill)
{
    memset(in, 0, sizeof *in);
    Player *p = &g->p;
    float fx = 0, fy = 0, danger = 0;
    for (int i = 0; i < g->nen; i++) {
        const Enemy *e = &g->en[i];
        if (!e->alive) continue;
        float dx = p->x - e->x, dy = p->y - e->y, d2 = dx * dx + dy * dy;
        float R = EDEF[e->type].ai == AI_BOSS ? 250.0f : 200.0f;
        if (d2 > R * R) continue;
        float d = sqrtf(d2) + 1;
        float w = (R - d) / R;
        w = w * w * (EDEF[e->type].ai == AI_BOSS ? 4.0f : 1.0f + e->r / 12.0f);
        fx += dx / d * w; fy += dy / d * w;
        if (d < e->r + 30) danger += 1;
    }
    for (int i = 0; i < g->neb && skill != 2; i++) {
        const EBullet *b = &g->eb[i];
        if (!b->alive) continue;
        float dx = p->x - b->x, dy = p->y - b->y, d2 = dx * dx + dy * dy;
        if (d2 > 110 * 110) continue;
        float d = sqrtf(d2) + 1;
        /* step out of its path */
        float vl = sqrtf(b->vx * b->vx + b->vy * b->vy) + 1;
        float px = -b->vy / vl, py = b->vx / vl;
        float side = (dx * px + dy * py) >= 0 ? 1.0f : -1.0f;
        float w = (110 - d) / 110 * 3.0f;
        fx += px * side * w; fy += py * side * w;
        if (d < 26) danger += 2;
    }
    /* a boss: keep it in range, orbiting it */
    if (g->boss >= 0 && g->en[g->boss].alive) {
        const Enemy *b = &g->en[g->boss];
        float dx = b->x - p->x, dy = b->y - p->y, d = sqrtf(dx * dx + dy * dy) + 1;
        if (d > 300) { fx += dx / d * 1.5f; fy += dy / d * 1.5f; }
        fx += -dy / d * 0.4f; fy += dx / d * 0.4f;
    }
    /* boss rings and rain markers */
    for (int i = 0; i < MAX_BOMB; i++) if (g->bomb[i].alive && g->bomb[i].enemy) {
        float dx = p->x - g->bomb[i].x, dy = p->y - g->bomb[i].y, d = sqrtf(dx * dx + dy * dy) + 1;
        if (d < 80) { fx += dx / d * 3; fy += dy / d * 3; }
    }
    /* circle-strafe so it doesn't get pinned */
    bot_ang += 0.004f;
    float cx = cosf(bot_ang), cy = sinf(bot_ang);
    float fl = sqrtf(fx * fx + fy * fy);
    if (fl > 0.05f) { fx += -fy / fl * 0.5f * fl; fy += fx / fl * 0.5f * fl; }
    else {
        /* quiet: go get the loot */
        int best = -1; float bd = 260 * 260;
        for (int i = 0; i < g->npu; i++) {
            float dx = g->pu[i].x - p->x, dy = g->pu[i].y - p->y, d2 = dx * dx + dy * dy;
            if (g->pu[i].alive && d2 < bd) { bd = d2; best = i; }
        }
        if (best >= 0) { fx = g->pu[best].x - p->x; fy = g->pu[best].y - p->y; }
        else { fx = cx; fy = cy; }
    }
    fl = sqrtf(fx * fx + fy * fy);
    if (fl > 1e-3f) { in->sx = fx / fl; in->sy = fy / fl; }
    if (skill == 0) { in->sx = cx * 0.6f; in->sy = cy * 0.6f; }      /* the careless bot just ambles about */
    in->held = BTN_R;
    if (skill >= 1 && danger > 0 && p->dash_cd <= 0) in->held |= BTN_L;
    if (skill == 2) { in->sx *= 0.8f; in->sy *= 0.8f; }     /* the average bot: slower to react, ignores bullets */
    /* hold the best gun */
    int best = p->sel, bl = -1;
    for (int s = 0; s < NSLOTS; s++) if (p->slot[s] >= 0 && p->wlvl[p->slot[s]] * 10 + p->slot[s] > bl) { bl = p->wlvl[p->slot[s]] * 10 + p->slot[s]; best = s; }
    static const uint32_t SB[NSLOTS] = { BTN_A, BTN_B, BTN_X, BTN_Y };
    if (best != p->sel) in->held |= SB[best];
}

/* greedy shopping through the real shop UI */
static void bot_shop(Game *g)
{
    static const int WANT[NWEAP] = { W_GATLING, W_TESLA, W_DYNAMITE, W_RAIL, W_SCATTER, W_REVOLVER };
    Player *p = &g->p;
    for (int guard = 0; guard < 40 && g->scene == SC_SHOP; guard++) {
        int item = -1;
        int owned = 0, empty = -1;
        for (int s = 0; s < NSLOTS; s++) { if (p->slot[s] >= 0) owned++; else if (empty < 0) empty = s; }
        if (p->hp < p->maxhp * 0.5f && p->cash >= upgrade_cost(g, UP_HEAL)) item = NWEAP + UP_HEAL;
        if (item < 0 && owned < 4)
            for (int k = 0; k < NWEAP; k++) { int w = WANT[k]; if (p->wlvl[w] == 0 && p->cash >= weapon_cost(g, w)) { item = w; break; } }
        if (item < 0) {
            int bc = 1 << 30;
            for (int s = 0; s < NSLOTS; s++) {
                int w = p->slot[s];
                if (w < 0) continue;
                int c = weapon_cost(g, w);
                if (c > 0 && c <= p->cash && c < bc) { bc = c; item = w; }
            }
            static const int UP[] = { UP_POWER, UP_VITAL, UP_RATE, UP_BACKUP, UP_ARMOR, UP_SPEED, UP_MAGNET };
            for (unsigned k = 0; k < sizeof UP / sizeof UP[0]; k++) {
                int c = upgrade_cost(g, UP[k]);
                if (c > 0 && c <= p->cash && (float)c < (float)bc * 0.9f) { bc = c; item = NWEAP + UP[k]; }
            }
        }
        if (item < 0) break;
        g->shop_sel = item;
        int before = p->cash;
        press(g, BTN_A);
        if (g->pick_weapon >= 0) press(g, empty >= 0 ? (empty == 0 ? BTN_A : empty == 1 ? BTN_B : empty == 2 ? BTN_X : BTN_Y) : BTN_Y);
        if (p->cash == before) break;
    }
    press(g, BTN_PLUS);
}

/* ------------------------------------------------------------------ */
/* menus, shop, save                                                    */
/* ------------------------------------------------------------------ */
static void ui_test(void)
{
    printf("ui\n");
    Game *g = G;
    game_init(g);
    game_storage(g, 1);
    run(g, 0.5f);
    shot(g, "title");
    press(g, BTN_A);
    CHECK(g->scene == SC_ARMORY, "title RIDE OUT goes to the armory (scene %d)", g->scene);
    run(g, 0.3f);
    shot(g, "armory");
    press(g, BTN_A);
    CHECK(g->scene == SC_BIOMES, "choosing the drifter goes to the biome map");
    shot(g, "biomes");
    press(g, BTN_A);
    CHECK(g->scene == SC_INTRO, "A starts the run");
    run(g, 1.0f);
    shot(g, "intro");
    run(g, 3.0f);
    CHECK(g->scene == SC_PLAY, "the intro hands over to the fight");
    CHECK(g->p.slot[0] == W_REVOLVER && g->p.wlvl[W_REVOLVER] == 1, "the drifter starts with revolvers");
    for (int i = 0; i < 120; i++) step_in(g, BTN_R, 0.3f, 0);
    CHECK(g->nbul > 0 || g->p.kills > 0, "R fires");
    press(g, BTN_PLUS);
    CHECK(g->scene == SC_PAUSE, "+ pauses");
    shot(g, "pause");
    press(g, BTN_PLUS);
    CHECK(g->scene == SC_PLAY, "+ resumes");
    /* jump to the end of the wave */
    g->run_t = WAVE_TIME - 0.05f;
    run(g, 0.2f);
    CHECK(g->phase == PH_WAVEEND, "the wave ends at 1:00");
    run(g, 3.0f);
    CHECK(g->scene == SC_SHOP, "then the trading post opens (scene %d)", g->scene);
    g->p.cash = 500;
    shot(g, "shop");
    g->shop_sel = W_TESLA;
    press(g, BTN_A);
    CHECK(g->pick_weapon == W_TESLA, "buying a new gun asks for a button");
    shot(g, "shop_pick");
    press(g, BTN_X);
    CHECK(g->p.slot[2] == W_TESLA && g->p.wlvl[W_TESLA] == 1, "X puts the tesla coil on X");
    int cash = g->p.cash;
    g->shop_sel = W_TESLA;
    press(g, BTN_A);
    CHECK(g->p.wlvl[W_TESLA] == 2 && g->p.cash < cash, "A upgrades an equipped gun");
    g->shop_sel = NWEAP + UP_VITAL;
    float mh = g->p.maxhp;
    press(g, BTN_A);
    CHECK(g->p.maxhp > mh, "vitality raises max health");
    press(g, BTN_PLUS);
    CHECK(g->scene == SC_PLAY && g->wave == 1, "+ starts wave 2 (wave %d)", g->wave);
    press(g, BTN_X);
    CHECK(g->p.sel == 2, "X switches to the tesla coil");
    for (int i = 0; i < 60; i++) step(g, BTN_ZR);
    char buf[SAVE_MAX];
    game_save_now(g);
    int n = game_save_request(g, buf, sizeof buf);
    CHECK(n > 0, "the game saves");
    if (n > 0) {
        Game *h = (Game *)malloc((size_t)GAME_SIZE);
        game_init(h);
        g->sv.souls = 1234;
        g->save_pending = 1;
        n = game_save_request(g, buf, sizeof buf);
        game_load(h, buf, n);
        CHECK(h->sv.souls == 1234, "the save loads back");
        buf[5] ^= 0x55;
        Game *k = (Game *)malloc((size_t)GAME_SIZE);
        game_init(k);
        game_load(k, buf, n);
        CHECK(k->sv.souls == 0, "a corrupt save is ignored");
        free(h); free(k);
    }
    /* help pages */
    game_init(g);
    g->menu = 1;
    press(g, BTN_A);
    CHECK(g->scene == SC_HELP, "how to play");
    shot(g, "help");
    press(g, BTN_A); press(g, BTN_A); press(g, BTN_A);
    CHECK(g->scene == SC_TITLE, "help pages lead back to the title");
}

/* ------------------------------------------------------------------ */
/* screenshots of each biome in full swing                              */
/* ------------------------------------------------------------------ */
static void setup_fight(Game *g, int biome, int wave, int lvl)
{
    game_init(g);
    g->sv.biomes = 5;
    g->bot = 2;
    world_start_run(g, biome, CH_DRIFTER);
    Player *p = &g->p;
    p->slot[0] = W_GATLING; p->slot[1] = W_DYNAMITE; p->slot[2] = W_TESLA; p->slot[3] = W_RAIL;
    for (int w = 0; w < NWEAP; w++) p->wlvl[w] = (uint8_t)lvl;
    p->sel = 0;
    p->up[UP_BACKUP] = 3;
    g->wave = wave;
    g->run_t = (float)wave * WAVE_TIME;
    g->scene = SC_PLAY;
    world_begin_wave(g);
}

static void shots_biomes(int only)
{
    char name[64];
    for (int b = 0; b < NBIOMES; b++) {
        if (only >= 0 && b != only) continue;
        Game *g = G;
        setup_fight(g, b, 6, 3);
        bot_rng = 7;
        for (int f = 0; f < 60 * 24; f++) { Input in; bot_input(g, &in, 1); in.pressed = in.held & ~prev_held; prev_held = in.held; game_update(g, &in, 1.0f / 60.0f); int q; float pa, vo; while (game_pop_sound(g, &q, &pa, &vo) >= 0) {} }
        snprintf(name, sizeof name, "biome%d_fight", b + 1);
        shot(g, name);
        for (int f = 0; f < 37; f++) { Input in; bot_input(g, &in, 1); in.held |= BTN_R; prev_held = in.held; game_update(g, &in, 1.0f / 60.0f); int q; float pa, vo; while (game_pop_sound(g, &q, &pa, &vo) >= 0) {} }
        snprintf(name, sizeof name, "biome%d_fight2", b + 1);
        shot(g, name);
        /* the boss */
        setup_fight(g, b, NWAVES, 4);
        for (int f = 0; f < 60 * 9; f++) { Input in; bot_input(g, &in, 1); in.pressed = in.held & ~prev_held; prev_held = in.held; game_update(g, &in, 1.0f / 60.0f); int q; float pa, vo; while (game_pop_sound(g, &q, &pa, &vo) >= 0) {} }
        snprintf(name, sizeof name, "biome%d_boss", b + 1);
        shot(g, name);
    }
}

static void shots_rest(void)
{
    Game *g = G;
    /* the bestiary on one screen */
    gfx_begin();
    gfx_rect(0, 0, VW, VH, 0x2A2420FFu);
    for (int i = 0; i < 20; i++) {
        float x = 70.0f + (float)(i % 10) * 80.0f, y = 120.0f + (float)(i / 10) * 140.0f;
        art_shadow(x, y + 16, 14, 4, 1);
        art_enemy_preview(i, x, y, 2.0f, 0.3f);
    }
    for (int c = 0; c < NCHARS; c++) art_player(560.0f + (float)c * 70.0f, 420, c, 1, 0, 1, 0, 0, 0, 2.2f);
    for (int w = 0; w < NWEAP; w++) art_weapon_icon(w, 60.0f + (float)w * 70.0f, 400, 2.0f, -0.3f, 0xFFFFFFFFu);
    raster();
    char p[512];
    snprintf(p, sizeof p, "%s/bestiary.ppm", outdir);
    save_ppm(p);
    gfx_begin();
    gfx_rect(0, 0, VW, VH, 0x2A2420FFu);
    for (int b = 0; b < 5; b++) art_boss_portrait(b, 90.0f + (float)b * 170.0f, 260, 1.6f, 0.4f);
    raster();
    snprintf(p, sizeof p, "%s/bosses.ppm", outdir);
    save_ppm(p);
    /* clear and death screens */
    setup_fight(g, 0, NWAVES, 5);
    g->bot = 0;
    g->total_kills = 1234; g->p.earned = 4321; g->best_combo = 345; g->total_time = 655;
    g->scene = SC_CLEAR; g->scene_t = 2;
    shot(g, "clear");
    g->scene = SC_OVER; g->souls_gained = 123; g->scene_t = 2;
    shot(g, "over");
    g->scene = SC_WIN; g->scene_t = 2;
    shot(g, "win");
}

/* ------------------------------------------------------------------ */
/* whole runs                                                           */
/* ------------------------------------------------------------------ */
static double worst_ms, total_ms;
static long frames;
static int max_vtx, max_en, max_pt;

/* plays from `biome` until death or victory; returns biomes cleared */
static int bot_run(int biome, int skill, uint32_t seed, int verbose)
{
    Game *g = G;
    game_init(g);
    g->rng ^= seed * 2654435761u;
    bot_rng = seed;
    g->sv.biomes = 5;
    world_start_run(g, biome, CH_DRIFTER);
    g->scene = SC_INTRO;
    run(g, 3.6f);
    int cleared = 0;
    int last_wave = -1;
    while (frames < 60L * 60 * 60 * 30) {
        if (g->scene == SC_PLAY) {
            Input in;
            bot_input(g, &in, skill);
            in.pressed = in.held & ~prev_held;
            prev_held = in.held;
            clock_t t0 = clock();
            game_update(g, &in, 1.0f / 60.0f);
            int q; float pa, vo; while (game_pop_sound(g, &q, &pa, &vo) >= 0) {}
            if ((frames & 7) == 0) {
                game_render(g);
                double ms = (double)(clock() - t0) * 1000.0 / CLOCKS_PER_SEC;
                if (ms > worst_ms) { worst_ms = ms; printf("      (worst so far %.2f ms: phase %d, %d enemies, %d particles, %d bullets, %d ebullets, %d vertices)\n", ms, g->phase, g->nen, g->npt, g->nbul, g->neb, gfx_nv); }
                total_ms += ms;
                if (gfx_nv > max_vtx) max_vtx = gfx_nv;
            }
            int alive = 0;
            for (int i = 0; i < g->nen; i++) alive += g->en[i].alive;
            if (alive > max_en) max_en = alive;
            if (g->npt > max_pt) max_pt = g->npt;
            frames++;
            static int last_phase = -1;
            if (g->phase != last_phase && verbose && (g->phase == PH_BOSS || g->phase == PH_BOSSDEAD)) printf("    %s at %.0f s (hp %.0f)\n", g->phase == PH_BOSS ? "boss up" : "boss down", g->total_time, g->p.hp);
            last_phase = g->phase;
            if (g->wave != last_wave && verbose) {
                printf("    b%d w%2d  hp %3.0f/%3.0f  $%5d  kills %5d  guns", g->biome + 1, g->wave + 1, g->p.hp, g->p.maxhp, g->p.cash, g->p.kills);
                for (int s = 0; s < NSLOTS; s++) if (g->p.slot[s] >= 0) printf(" %d:L%d", g->p.slot[s], g->p.wlvl[g->p.slot[s]]);
                printf("  ups");
                for (int u = 0; u < UP_HEAL; u++) printf(" %d", g->p.up[u]);
                printf("\n");
                last_wave = g->wave;
            }
        } else if (g->scene == SC_SHOP) {
            bot_shop(g);
        } else if (g->scene == SC_CLEAR) {
            cleared++;
            if (verbose) printf("    biome %d cleared at %.0f s\n", g->biome + 1, g->total_time);
            run(g, 1.5f); press(g, BTN_A);
        } else if (g->scene == SC_INTRO) {
            run(g, 0.5f); press(g, BTN_A);
        } else if (g->scene == SC_OVER || g->scene == SC_WIN) {
            { extern float dbg_hurt[4]; if (verbose) printf("    damage taken: contact %.0f, shots %.0f, other %.0f\n", dbg_hurt[0], dbg_hurt[1], dbg_hurt[2]); dbg_hurt[0] = dbg_hurt[1] = dbg_hurt[2] = 0; }
            if (verbose) printf("    %s in biome %d wave %d (%.0f s, %d kills, +%d souls)\n", g->scene == SC_WIN ? "WON" : "died", g->biome + 1, g->wave + 1, g->total_time, g->total_kills, g->souls_gained);
            break;
        } else step(g, 0);
    }
    return cleared;
}

static void bots(int only)
{
    printf("bots\n");
    worst_ms = 0; total_ms = 0; frames = 0;
    int b0 = only >= 0 ? only : 0;
    printf("  the dodging bot from biome %d:\n", b0 + 1);
    int c = bot_run(b0, 1, 3, 1);
    printf("  cleared %d biome(s)\n", c);
    printf("  the average bot (dashes, but ignores bullets) from biome %d:\n", b0 + 1);
    c = bot_run(b0, 2, 4, 1);
    printf("  cleared %d biome(s)\n", c);
    printf("  the careless bot from biome %d:\n", b0 + 1);
    bot_run(b0, 0, 5, 1);
    double avg = total_ms / (double)(frames / 8 > 0 ? frames / 8 : 1);
    printf("  frame (update+render, native): avg %.3f ms, worst %.2f ms; max vertices %d of %d; max enemies %d; max particles %d\n",
           avg, worst_ms, max_vtx, GFX_MAX_VTX, max_en, max_pt);
    CHECK(max_vtx < GFX_MAX_VTX, "vertex buffer overflow");
}

/* ------------------------------------------------------------------ */
/* reel: frames + audio                                                 */
/* ------------------------------------------------------------------ */
#define REEL_RATE 32000
static float *amix;
static int    alen, amax;

static void mix_sfx(int id, int pitch, float pan, float vol, int at)
{
    const int16_t *d = audio_data(id);
    int n = audio_len(id);
    float rate = (float)pitch / 128.0f, v = (float)audio_volume(id) / 255.0f * vol * 0.5f;
    float vl = v * (pan > 0 ? 1.0f - pan * 0.7f : 1.0f), vr = v * (pan < 0 ? 1.0f + pan * 0.7f : 1.0f);
    for (int i = 0; ; i++) {
        float p = (float)i * rate;
        int k = (int)p;
        if (k >= n - 1 || at + i >= amax) break;
        float s = ((float)d[k] + ((float)d[k + 1] - (float)d[k]) * (p - (float)k)) / 32768.0f;
        amix[(at + i) * 2] += s * vl; amix[(at + i) * 2 + 1] += s * vr;
    }
}

typedef struct { int scene, biome, wave, lvl, ch; float secs; int s0, s1, s2, s3; } Shot;
static const Shot SCRIPT[] = {
    { SC_TITLE, 0, 0, 0, 0, 3.0f, 0, 0, 0, 0 },
    { SC_PLAY, 0, 2, 2, CH_DRIFTER, 15.0f, W_REVOLVER, W_SCATTER, W_DYNAMITE, -1 },
    { SC_PLAY, 1, 5, 3, CH_PREACHER, 15.0f, W_SCATTER, W_TESLA, W_GATLING, W_DYNAMITE },
    { SC_PLAY, 2, 7, 4, CH_GRANNY, 17.0f, W_GATLING, W_DYNAMITE, W_TESLA, W_REVOLVER },
    { SC_PLAY, 3, NWAVES, 6, CH_DOC, 17.0f, W_DYNAMITE, W_RAIL, W_SCATTER, W_TESLA },
    { SC_PLAY, 4, 8, 6, CH_DRIFTER, 16.0f, W_RAIL, W_GATLING, W_TESLA, W_DYNAMITE },
    { SC_PLAY, 4, NWAVES, 8, CH_DRIFTER, 16.0f, W_RAIL, W_GATLING, W_TESLA, W_DYNAMITE },
};

static void reel_setup(Game *g, const Shot *sh)
{
    if (sh->scene == SC_TITLE) { game_init(g); g->scene = SC_TITLE; return; }
    game_init(g);
    g->sv.biomes = 5;
    g->bot = 2;
    world_start_run(g, sh->biome, sh->ch);
    Player *p = &g->p;
    int sl[4] = { sh->s0, sh->s1, sh->s2, sh->s3 };
    for (int k = 0; k < NSLOTS; k++) { p->slot[k] = (int8_t)sl[k]; if (sl[k] >= 0) p->wlvl[sl[k]] = (uint8_t)sh->lvl; }
    p->sel = 0;
    for (int u = 0; u < UP_HEAL; u++) p->up[u] = (uint8_t)(sh->lvl * 5 / 8);
    g->wave = sh->wave;
    g->run_t = (float)sh->wave * WAVE_TIME + 8.0f;
    g->scene = SC_PLAY;
    world_begin_wave(g);
    /* get the field busy before the camera rolls */
    for (int f = 0; f < 60 * 6; f++) { Input in; bot_input(g, &in, 1); in.pressed = in.held & ~prev_held; prev_held = in.held; game_update(g, &in, 1.0f / 60.0f); int q; float pa, vo; while (game_pop_sound(g, &q, &pa, &vo) >= 0) {} }
}

static void reel(int only, int boss)
{
    Game *g = G;
    (void)boss;
    char dir[512];
    snprintf(dir, sizeof dir, "%s/reel", outdir);
    amax = REEL_RATE * 130; amix = calloc((size_t)amax * 2, sizeof(float)); alen = 0;
    int frame = 0, sample = 0, mpos = 0;
    float mrate = 1.0f;
    int ep = -1;
    int nshots = (int)(sizeof SCRIPT / sizeof SCRIPT[0]);
    for (int si = 0; si < nshots; si++) {
        if (only >= 0 && si != only) continue;
        const Shot *sh = &SCRIPT[si];
        reel_setup(g, sh);
        int frames = (int)(sh->secs * 30.0f);
        float swap_t = 0;
        for (int fr = 0; fr < frames; fr++) {
            for (int s = 0; s < 2; s++) {
                Input in;
                memset(&in, 0, sizeof in);
                if (g->scene == SC_PLAY) {
                    bot_input(g, &in, 1);
                    in.held &= ~(BTN_A | BTN_B | BTN_X | BTN_Y);
                    swap_t += 1.0f / 60.0f;
                    if (swap_t > 4.5f) {            /* show the guns off: swap every few seconds */
                        swap_t = 0;
                        static const uint32_t SB[NSLOTS] = { BTN_A, BTN_B, BTN_X, BTN_Y };
                        for (int k = 1; k <= NSLOTS; k++) { int sl = (g->p.sel + k) % NSLOTS; if (g->p.slot[sl] >= 0) { in.held |= SB[sl]; break; } }
                    }
                    if (g->p.hp < g->p.maxhp * 0.45f) g->p.hp = g->p.maxhp * 0.45f;
                }
                in.pressed = in.held & ~prev_held; prev_held = in.held;
                game_update(g, &in, 1.0f / 60.0f);
                int id, p; float pan, vol;
                while ((id = game_pop_sound(g, &p, &pan, &vol)) >= 0) mix_sfx(id, p, pan, vol, sample);
                float mv[MUS_TRACKS];
                int pitch, epoch;
                game_music(g, mv, &pitch, &epoch);
                if (epoch != ep || sh->scene == SC_TITLE) { if (epoch != ep) { ep = epoch; mpos = 0; } mrate = (float)pitch / 128.0f; }
                if (fr == 0 && s == 0) for (int t = 0; t < MUS_TRACKS; t++) G->mus[t] = mv[t];
                int n = REEL_RATE / 60;
                for (int i = 0; i < n && sample + i < amax; i++) {
                    float acc = 0;
                    for (int t = 0; t < MUS_TRACKS; t++) {
                        if (mv[t] < 0.001f) continue;
                        int L = music_len(t);
                        float pos = (float)(mpos + i) * (float)MUS_RATE * mrate / (float)REEL_RATE;
                        int k = (int)pos % L;
                        acc += (float)music_data(t)[k] / 32768.0f * mv[t] * (float)music_volume(t) / 255.0f;
                    }
                    amix[(sample + i) * 2] += acc * 0.42f; amix[(sample + i) * 2 + 1] += acc * 0.42f;
                }
                mpos += n; sample += n;
            }
            game_render(g);
            raster();
            char p[600];
            snprintf(p, sizeof p, "%s/f%04d.ppm", dir, frame);
            save_ppm(p);
            frame++;
        }
        printf("  reel: shot %d done (%d frames so far)\n", si, frame);
        fflush(stdout);
    }
    alen = sample;
    char wp[600];
    snprintf(wp, sizeof wp, "%s/music.wav", dir);
    FILE *f = fopen(wp, "wb");
    int bytes = alen * 4;
    unsigned char h[44] = { 'R','I','F','F',0,0,0,0,'W','A','V','E','f','m','t',' ',16,0,0,0,1,0,2,0,0,0,0,0,0,0,0,0,4,0,16,0,'d','a','t','a',0,0,0,0 };
    int v = 36 + bytes; memcpy(h + 4, &v, 4); v = REEL_RATE; memcpy(h + 24, &v, 4); v = REEL_RATE * 4; memcpy(h + 28, &v, 4); memcpy(h + 40, &bytes, 4);
    fwrite(h, 1, 44, f);
    float peak = 0;
    for (int i = 0; i < alen * 2; i++) if (fabsf(amix[i]) > peak) peak = fabsf(amix[i]);
    float k = peak > 0.95f ? 0.95f / peak : 1.0f;
    for (int i = 0; i < alen * 2; i++) { short s = (short)(amix[i] * k * 32767.0f); fwrite(&s, 2, 1, f); }
    fclose(f);
    printf("  reel: %d frames, audio peak %.2f\n", frame, peak);
}

int main(int argc, char **argv)
{
    if (argc > 1) outdir = argv[1];
    const char *what = argc > 2 ? argv[2] : "all";
    int only = argc > 3 ? atoi(argv[3]) - 1 : -1;
    G = (Game *)malloc((size_t)GAME_SIZE);
    clock_t t0 = clock();
    game_init(G);
    printf("init (synthesis) %.0f ms, sfx pool %d samples, Game %d bytes\n", (double)(clock() - t0) * 1000.0 / CLOCKS_PER_SEC, audio_pool_len(), GAME_SIZE);

    if (!strcmp(what, "icon")) {
        /* the Homebrew Channel icon (128 x 48): drawn at 854 x 320 into the top of the frame */
        gfx_begin();
        gfx_rect_v(0, 0, VW, 320, 0x1A0806FFu, 0x8A3010FFu);
        gfx_blend(BL_ADD); gfx_glow(640, 120, 220, 0xFF802070u); gfx_blend(BL_ALPHA);
        for (int i = 0; i < 6; i++) art_enemy_preview(i % 2 ? A_ZOMBIE : A_BANDIT, 470.0f + (float)i * 70.0f, 300, 2.2f, (float)i);
        gfx_rect_v(0, 250, VW, 70, 0x00000000u, 0x000000E0u);
        art_player(110, 230, 0, 1, 0, 1, 0, 0, 0, 7.0f);
        const char *t1 = "LAST", *t2 = "STAND";
        float px = 13.0f;
        gfx_text(t1, 250 + 9, 30 + 9, px, 0x000000C0u); gfx_text(t1, 250, 30, px, 0xC8281AFFu); gfx_text_top(t1, 250, 30, px, 0xFF8A20FFu, 3); gfx_text_top(t1, 250, 30, px, 0xFFE070FFu, 1);
        px = 11.0f;
        gfx_text(t2, 250 + 8, 160 + 8, px, 0x000000C0u); gfx_text(t2, 250, 160, px, 0xC8281AFFu); gfx_text_top(t2, 250, 160, px, 0xFF8A20FFu, 3); gfx_text_top(t2, 250, 160, px, 0xFFE070FFu, 1);
        raster();
        char p[512];
        snprintf(p, sizeof p, "%s/icon_full.ppm", outdir);
        save_ppm(p);
        return 0;
    }
    if (!strcmp(what, "boss")) {
        static const int EXP[NBIOMES] = { 3, 5, 7, 8, 8 };
        for (int b = 0; b < NBIOMES; b++) for (int lv = EXP[b] - 1; lv <= EXP[b]; lv++) {
            Game *g = G;
            setup_fight(g, b, NWAVES, lv);
            g->bot = 2;
            for (int u = 0; u < UP_HEAL; u++) g->p.up[u] = (uint8_t)(lv * 6 / 8);
            float t = 0;
            while (t < 600 && g->phase == PH_BOSS) {
                Input in; bot_input(g, &in, 1); in.pressed = in.held & ~prev_held; prev_held = in.held;
                game_update(g, &in, 1.0f / 60.0f); t += 1.0f / 60.0f;
                int q; float pa, vo; while (game_pop_sound(g, &q, &pa, &vo) >= 0) {}
            }
            printf("  biome %d, gear L%d: boss %s after %.0f s (boss hp %.0f, left %.0f%%)\n", b + 1, lv, g->phase == PH_BOSS ? "ALIVE" : "dead", t,
                   EDEF[A_BOSS1 + b].hp, g->boss >= 0 ? 100.0f * g->en[g->boss].hp / g->en[g->boss].maxhp : 0.0f);
        }
        return 0;
    }
    if (!strcmp(what, "reel")) { reel(only, argc > 4); return 0; }
    if (!strcmp(what, "all") || !strcmp(what, "ui")) ui_test();
    if (!strcmp(what, "all") || !strcmp(what, "shots")) { printf("screenshots\n"); shots_biomes(only); shots_rest(); }
    if (!strcmp(what, "all") || !strcmp(what, "bots")) bots(only);
    printf(fails ? "%d FAILURES\n" : "all checks passed\n", fails);
    return fails ? 1 : 0;
}
