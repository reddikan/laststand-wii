/*
 * LAST STAND - scenes, menus, the shop, the armory, saving, sound and music routing.
 */
#include "game_int.h"
#include "gfx.h"
#include "music.h"

#include <string.h>
#include <stddef.h>
#include <stdio.h>

const int GAME_SIZE = (int)sizeof(Game);

const char *const WEAP_NAME[NWEAP] = { "HELLFIRE REVOLVERS", "DRAGON'S BREATH", "GATLING STORM", "DYNAMITE BUNDLE", "TESLA COIL", "SUN LANCE" };
const char *const WEAP_DESC[NWEAP] = {
    "FAST TWIN SIX-SHOOTERS. PIERCE, THEN FAN THE HAMMER.",
    "A SHOTGUN FULL OF FIRE. SETS THE HORDE ALIGHT.",
    "SPIN IT UP AND HOSE THEM DOWN. HEAVY TO CARRY.",
    "LOBBED BUNDLES. BIG BOOMS, THEN CLUSTER BOMBS.",
    "LIGHTNING THAT JUMPS FROM MONSTER TO MONSTER.",
    "A PIERCING BEAM OF LIGHT. HITS EVERYTHING IN A LINE.",
};
const char *const UPG_NAME[NUPG] = { "VITALITY", "ARMOR", "BOOTS", "MAGNET", "POWDER", "TRIGGER", "LUCK", "BACKUP RIG", "WHISKEY" };
const char *const UPG_DESC[NUPG] = {
    "+20 MAX HEALTH", "TAKE 8% LESS DAMAGE", "MOVE 7% FASTER", "PULL LOOT FROM FURTHER",
    "+12% DAMAGE", "+9% FIRE RATE", "MORE CRITS AND COIN", "BACKUP GUNS FIRE FASTER", "HEAL 50% OF YOUR HEALTH",
};
const char *const CHAR_NAME[NCHARS] = { "THE DRIFTER", "THE PREACHER", "GRANNY GATLING", "DYNAMITE DOC" };
const char *const CHAR_DESC[NCHARS] = {
    "REVOLVERS. +8% CRIT CHANCE.",
    "DRAGON'S BREATH. +20 HEALTH.",
    "GATLING STORM. 15% LESS DAMAGE, SLOWER.",
    "DYNAMITE. A PROPER MESS.",
};
const char *const PERK_NAME[NPERKS] = { "SAVINGS", "SECOND WIND", "GREED", "ARMS DEALER" };
const char *const PERK_DESC[NPERKS] = { "+$40 TO START A RUN", "+1 LAST STAND PER RUN", "+10% COIN DROPS", "BACKUP GUNS +5% RATE" };
const char *const BIOME_NAME[NBIOMES] = { "DUST GULCH", "ROTWOOD BAYOU", "GRAVEWATCH", "ASHEN HOLLOW", "HELLMOUTH" };
const char *const BIOME_SUB[NBIOMES] = {
    "THE DEAD RIDE AT SUNDOWN",
    "SOMETHING MOVES IN THE MURK",
    "THE GRAVES ARE EMPTY",
    "THE FOREST BURNED. IT IS STILL BURNING.",
    "THE END OF THE ROAD",
};
const char *const BOSS_NAME[NBIOMES] = { "THE BONE SHERIFF", "THE BOG MOTHER", "THE UNDERTAKER", "THE PLAGUE PROPHET", "THE MAW" };

#define SAVE_MAGIC 0x4C53544Eu      /* "LSTN" */

/* ------------------------------------------------------------------ */
/* prices                                                               */
/* ------------------------------------------------------------------ */
static float price_scale(const Game *g) { return 1.0f + 0.3f * (float)g->biome; }

static int equipped(const Game *g, int w)
{
    for (int s = 0; s < NSLOTS; s++) if (g->p.slot[s] == w) return 1;
    return 0;
}

int weapon_cost(const Game *g, int w)
{
    int L = g->p.wlvl[w];
    if (L == 0) {
        int owned = 0;
        for (int k = 0; k < NWEAP; k++) owned += g->p.wlvl[k] > 0;
        return (int)((float)(55 + 40 * owned) * price_scale(g));
    }
    if (!equipped(g, w)) return 0;                     /* re-equip for free */
    if (L >= MAX_WLVL) return -1;
    static const int UP[MAX_WLVL] = { 0, 45, 75, 120, 190, 300, 470, 740 };
    return (int)((float)UP[L] * price_scale(g));
}

int upgrade_cost(const Game *g, int u)
{
    if (u == UP_HEAL) return (int)(25.0f * (1.0f + 0.5f * (float)g->biome));
    int n = g->p.up[u];
    if (n >= MAX_ULVL) return -1;
    static const int UC[MAX_ULVL] = { 40, 65, 105, 170, 270, 430 };
    return (int)((float)UC[n] * price_scale(g));
}

int char_cost(const Game *g, int c) { static const int C[NCHARS] = { 0, 150, 320, 520 }; (void)g; return C[c]; }
int char_lvl_cost(const Game *g, int c) { int l = g->sv.chr_lvl[c]; return l >= 5 ? -1 : 90 + 70 * l; }
int perk_cost(const Game *g, int k) { int l = g->sv.perk[k]; return l >= 3 ? -1 : 120 + 110 * l; }

/* ------------------------------------------------------------------ */
/* sound                                                                */
/* ------------------------------------------------------------------ */
static void snd_push(Game *g, int id, int pitch, float pan, float vol)
{
    if (!g->sv.sfx || g->nsnd >= MAX_SND) return;
    SndEv *e = &g->snd[g->nsnd++];
    e->id = (int16_t)id; e->pitch = (int16_t)pitch; e->pan = pan; e->vol = vol;
}

void sfx(Game *g, int id, float x, float y, float vol, float jit)
{
    float dx = x - g->camx, dy = y - g->camy;
    float d = sqrtf(dx * dx + dy * dy);
    float att = d < 300.0f ? 1.0f : clampf(1.0f - (d - 300.0f) / 600.0f, 0.0f, 1.0f);
    if (att <= 0.02f) return;
    float pitch = 128.0f * (1.0f + frands(g) * jit);
    if (g->slowmo > 0) pitch *= 0.75f;
    snd_push(g, id, (int)pitch, clampf(dx / (VW * 0.5f), -1, 1) * 0.8f, vol * att);
}

void sfx_ui(Game *g, int id) { snd_push(g, id, 128, 0, 1.0f); }

int game_pop_sound(Game *g, int *pitch, float *pan, float *vol)
{
    static int head;
    if (head >= g->nsnd) { head = 0; g->nsnd = 0; return -1; }
    SndEv *e = &g->snd[head++];
    *pitch = e->pitch; *pan = e->pan; *vol = e->vol;
    return e->id;
}

/* ------------------------------------------------------------------ */
/* saving                                                               */
/* ------------------------------------------------------------------ */
static uint32_t save_sum(const Save *s)
{
    const uint8_t *b = (const uint8_t *)s;
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < offsetof(Save, sum); i++) { h ^= b[i]; h *= 16777619u; }
    return h;
}

static void save_defaults(Save *s)
{
    memset(s, 0, sizeof *s);
    s->magic = SAVE_MAGIC; s->ver = 1; s->biomes = 1;
    s->chr_open[0] = 1;
    s->music = 1; s->sfx = 1; s->autofire = 0;
}

void game_save_now(Game *g) { g->save_pending = 1; }
void game_storage(Game *g, int ok) { g->storage_ok = ok; }

void game_load(Game *g, const void *buf, int len)
{
    if (len < (int)sizeof(Save)) return;
    Save s;
    memcpy(&s, buf, sizeof s);
    if (s.magic != SAVE_MAGIC || s.sum != save_sum(&s)) return;
    if (s.biomes < 1 || s.biomes > NBIOMES) s.biomes = 1;
    s.chr_open[0] = 1;
    g->sv = s;
}

int game_save_request(Game *g, void *buf, int max)
{
    if (!g->save_pending || max < (int)sizeof(Save)) return 0;
    g->save_pending = 0;
    g->sv.sum = save_sum(&g->sv);
    memcpy(buf, &g->sv, sizeof g->sv);
    return (int)sizeof g->sv;
}

void game_save_result(Game *g, int ok) { g->save_state = ok ? 1 : 2; g->save_msg_t = 0; }

/* ------------------------------------------------------------------ */
/* music                                                                */
/* ------------------------------------------------------------------ */
void game_music(const Game *g, float vol[], int *pitch, int *epoch)
{
    for (int t = 0; t < MUS_TRACKS; t++) vol[t] = g->sv.music ? g->mus[t] : 0.0f;
    *pitch = g->music_pitch;
    *epoch = g->music_epoch;
}

static void music_mix(Game *g, float dt)
{
    float want[MUS_TRACKS] = { 0 };
    switch (g->scene) {
    case SC_TITLE: case SC_ARMORY: case SC_BIOMES: case SC_HELP: want[MUS_MENU] = 1.0f; break;
    case SC_INTRO: want[MUS_RUN] = 0.35f; break;
    case SC_SHOP: want[MUS_SHOP] = 1.0f; break;
    case SC_PAUSE: case SC_PLAY: {
        float k = g->scene == SC_PAUSE ? 0.3f : 1.0f;
        if (g->phase == PH_BOSS) { want[MUS_BOSS] = k; want[MUS_CHAOS] = 0.25f * k * g->chaos; }
        else if (g->phase == PH_BOSSDEAD) { want[MUS_RUN] = 0.2f * k; }
        else { want[MUS_RUN] = k; want[MUS_CHAOS] = k * clampf(g->chaos * 1.6f - 0.15f, 0, 1); }
        if (g->p.hp <= 0) { want[MUS_RUN] = 0; want[MUS_CHAOS] = 0; want[MUS_BOSS] = 0; }
        break;
    }
    case SC_CLEAR: case SC_WIN: want[MUS_SHOP] = 0.7f; break;
    case SC_OVER: want[MUS_MENU] = 0.6f; break;
    }
    for (int t = 0; t < MUS_TRACKS; t++) {
        float rate = want[t] > g->mus[t] ? 0.8f : 0.6f;
        if (t == MUS_CHAOS) rate = want[t] > g->mus[t] ? 1.6f : 0.35f;
        g->mus[t] = approach(g->mus[t], want[t], dt * rate);
    }
}

static void set_menu_music(Game *g)
{
    if (g->music_pitch != 128) { g->music_pitch = 128; g->music_epoch++; }
}

/* ------------------------------------------------------------------ */
/* scenes                                                               */
/* ------------------------------------------------------------------ */
static void go(Game *g, int scene)
{
    g->prev_scene = g->scene;
    g->scene = scene;
    g->scene_t = 0;
}

void game_init(Game *g)
{
    memset(g, 0, sizeof *g);
    g->rng = 0x9E3779B9u;
    save_defaults(&g->sv);
    audio_init();
    music_init();
    g->scene = SC_TITLE;
    g->music_pitch = 128;
    g->music_epoch = 1;
    g->boss = -1;
    g->pick_weapon = -1;
    for (int i = 0; i < MAX_NUM; i++) g->num[i].t = 99;
    g->p.slot[0] = W_REVOLVER;
    g->p.slot[1] = g->p.slot[2] = g->p.slot[3] = -1;
}

static int souls_for_run(const Game *g, int cleared)
{
    return g->total_kills / 6 + cleared * 80 + g->biome * 10 * NWAVES / 4 + g->wave * 3;
}

static void end_run(Game *g, int cleared_biomes)
{
    g->souls_gained = souls_for_run(g, cleared_biomes);
    g->sv.souls += g->souls_gained;
    g->sv.runs++;
    if (g->total_kills > g->sv.best_kills) g->sv.best_kills = g->total_kills;
    if ((int)g->total_time > g->sv.best_time) g->sv.best_time = (int)g->total_time;
    game_save_now(g);
}

static int pressed_dir(const Input *in, int *dx, int *dy, Game *g)
{
    /* d-pad or a flick of the stick, with repeat */
    static float rep;
    static int last;
    int x = 0, y = 0;
    if (in->held & BTN_LEFT) x = -1; else if (in->held & BTN_RIGHT) x = 1;
    if (in->held & BTN_UP) y = -1;   else if (in->held & BTN_DOWN) y = 1;
    if (in->sx < -0.6f) x = -1; else if (in->sx > 0.6f) x = 1;
    if (in->sy < -0.6f) y = -1; else if (in->sy > 0.6f) y = 1;
    int code = (x + 1) * 3 + (y + 1);
    *dx = 0; *dy = 0;
    if (x == 0 && y == 0) { last = code; rep = 0; return 0; }
    if (code != last) { last = code; rep = 0.32f; *dx = x; *dy = y; sfx_ui(g, SFX_MOVE); return 1; }
    rep -= 1.0f / 60.0f;
    if (rep <= 0) { rep = 0.1f; *dx = x; *dy = y; sfx_ui(g, SFX_MOVE); return 1; }
    return 0;
}

/* ---- title ---- */
enum { TM_PLAY, TM_HELP, TM_AUTO, TM_MUSIC, TM_SFX, TM_N };

static void title_update(Game *g, const Input *in)
{
    int dx, dy;
    set_menu_music(g);
    if (pressed_dir(in, &dx, &dy, g) && dy) g->menu = (g->menu + dy + TM_N) % TM_N;
    if (in->pressed & (BTN_A | BTN_PLUS)) {
        switch (g->menu) {
        case TM_PLAY: sfx_ui(g, SFX_SELECT); g->armory_sel = g->char_sel; go(g, SC_ARMORY); break;
        case TM_HELP: sfx_ui(g, SFX_SELECT); g->help_page = 0; go(g, SC_HELP); break;
        case TM_AUTO: g->sv.autofire ^= 1; sfx_ui(g, SFX_SELECT); game_save_now(g); break;
        case TM_MUSIC: g->sv.music ^= 1; sfx_ui(g, SFX_SELECT); game_save_now(g); break;
        case TM_SFX: g->sv.sfx ^= 1; sfx_ui(g, SFX_SELECT); game_save_now(g); break;
        }
    }
}

static void help_update(Game *g, const Input *in)
{
    int dx, dy;
    if (pressed_dir(in, &dx, &dy, g) && dx) g->help_page = (g->help_page + dx + 3) % 3;
    if (in->pressed & BTN_A) { if (g->help_page < 2) { g->help_page++; sfx_ui(g, SFX_SELECT); } else go(g, SC_TITLE); }
    if (in->pressed & (BTN_B | BTN_PLUS)) go(g, SC_TITLE);
}

/* ---- armory: pick a gunslinger, spend souls ---- */
static void armory_update(Game *g, const Input *in)
{
    int dx, dy;
    if (pressed_dir(in, &dx, &dy, g)) {
        int row = g->armory_sel / NCHARS, col = g->armory_sel % NCHARS;
        col = (col + dx + NCHARS) % NCHARS;
        if (dy) row = !row;
        g->armory_sel = row * NCHARS + col;
    }
    if (in->pressed & BTN_B) { go(g, SC_TITLE); return; }
    int sel = g->armory_sel;
    if (sel < NCHARS) {
        int c = sel;
        if (in->pressed & BTN_A) {
            if (g->sv.chr_open[c]) { g->char_sel = c; sfx_ui(g, SFX_SELECT); g->biome_sel = g->sv.biomes - 1; go(g, SC_BIOMES); }
            else if (g->sv.souls >= char_cost(g, c)) { g->sv.souls -= char_cost(g, c); g->sv.chr_open[c] = 1; sfx_ui(g, SFX_BUY); g->shop_flash = 0.5f; game_save_now(g); }
            else { sfx_ui(g, SFX_ERROR); g->shop_err = 0.5f; }
        }
        if (in->pressed & BTN_Y) {
            int cost = char_lvl_cost(g, c);
            if (g->sv.chr_open[c] && cost > 0 && g->sv.souls >= cost) { g->sv.souls -= cost; g->sv.chr_lvl[c]++; sfx_ui(g, SFX_BUY); g->shop_flash = 0.5f; game_save_now(g); }
            else { sfx_ui(g, SFX_ERROR); g->shop_err = 0.5f; }
        }
    } else if (in->pressed & BTN_A) {
        int k = sel - NCHARS, cost = perk_cost(g, k);
        if (cost > 0 && g->sv.souls >= cost) { g->sv.souls -= cost; g->sv.perk[k]++; sfx_ui(g, SFX_BUY); g->shop_flash = 0.5f; game_save_now(g); }
        else { sfx_ui(g, SFX_ERROR); g->shop_err = 0.5f; }
    }
}

static void biomes_update(Game *g, const Input *in)
{
    int dx, dy;
    if (pressed_dir(in, &dx, &dy, g) && dx) g->biome_sel = (g->biome_sel + dx + NBIOMES) % NBIOMES;
    if (in->pressed & BTN_B) { go(g, SC_ARMORY); return; }
    if (in->pressed & BTN_A) {
        if (g->biome_sel < g->sv.biomes) {
            sfx_ui(g, SFX_SELECT);
            world_start_run(g, g->biome_sel, g->char_sel);
            go(g, SC_INTRO);
        } else { sfx_ui(g, SFX_ERROR); g->shop_err = 0.5f; }
    }
}

/* ---- the shop between waves ---- */
/* item ids: 0..5 weapons, 6..14 upgrades, 15 continue */
#define SHOP_N (NWEAP + NUPG + 1)

void shop_item_pos(int i, float *x, float *y)
{
    if (i < NWEAP) { *x = 248.0f + (float)i * 100.0f; *y = 132.0f; return; }
    if (i < NWEAP + NUPG) {
        int k = i - NWEAP;
        int row = k / 5, col = k % 5;
        *x = 298.0f + (float)col * 100.0f - (row ? 0.0f : 0.0f); *y = 262.0f + (float)row * 92.0f;
        if (row == 1) *x += 50.0f;
        return;
    }
    *x = 698.0f; *y = 446.0f;
}

static int shop_nav(int cur, int dx, int dy)
{
    float cx, cy;
    shop_item_pos(cur, &cx, &cy);
    int best = cur;
    float bd = 1e9f;
    for (int i = 0; i < SHOP_N; i++) {
        if (i == cur) continue;
        float x, y;
        shop_item_pos(i, &x, &y);
        float ddx = x - cx, ddy = y - cy;
        float along = ddx * (float)dx + ddy * (float)dy;
        if (along <= 1.0f) continue;
        float across = fabsf(ddx * (float)dy) + fabsf(ddy * (float)dx);
        float score = along + across * 2.5f;
        if (score < bd) { bd = score; best = i; }
    }
    return best;
}

static void continue_from_shop(Game *g)
{
    sfx_ui(g, SFX_SELECT);
    if (g->shop_after == 0) {
        g->wave++;
        world_begin_wave(g);
        go(g, SC_PLAY);
    } else {
        world_start_biome(g, g->biome + 1);
        go(g, SC_INTRO);
    }
}

static void place_weapon(Game *g, int w, int slot)
{
    Player *p = &g->p;
    int cost = weapon_cost(g, w);
    if (p->wlvl[w] == 0) {
        if (p->cash < cost) { sfx_ui(g, SFX_ERROR); g->shop_err = 0.5f; g->pick_weapon = -1; return; }
        p->cash -= cost;
        p->wlvl[w] = 1;
    }
    for (int s = 0; s < NSLOTS; s++) if (p->slot[s] == w) p->slot[s] = -1;
    p->slot[slot] = (int8_t)w;
    if (p->slot[p->sel] < 0) p->sel = slot;
    g->pick_weapon = -1;
    g->shop_flash = 0.5f;
    sfx_ui(g, SFX_BUY);
}

static void shop_update(Game *g, const Input *in)
{
    Player *p = &g->p;
    if (g->pick_weapon >= 0) {
        static const uint32_t SB[NSLOTS] = { BTN_A, BTN_B, BTN_X, BTN_Y };
        for (int s = 0; s < NSLOTS; s++) if (in->pressed & SB[s]) { place_weapon(g, g->pick_weapon, s); return; }
        if (in->pressed & (BTN_PLUS | BTN_L | BTN_ZL)) { g->pick_weapon = -1; sfx_ui(g, SFX_PAUSE); }
        return;
    }
    int dx, dy;
    if (pressed_dir(in, &dx, &dy, g)) {
        if (dx) g->shop_sel = shop_nav(g->shop_sel, dx, 0);
        if (dy) g->shop_sel = shop_nav(g->shop_sel, 0, dy);
    }
    if (in->pressed & BTN_PLUS) { continue_from_shop(g); return; }
    if (!(in->pressed & BTN_A)) return;
    int i = g->shop_sel;
    if (i == SHOP_N - 1) { continue_from_shop(g); return; }
    if (i < NWEAP) {
        int w = i, cost = weapon_cost(g, w);
        if (p->wlvl[w] == 0 || !equipped(g, w)) {
            if (p->cash < cost) { sfx_ui(g, SFX_ERROR); g->shop_err = 0.5f; return; }
            g->pick_weapon = w;                      /* which button? */
            sfx_ui(g, SFX_SELECT);
            return;
        }
        if (cost < 0 || p->cash < cost) { sfx_ui(g, SFX_ERROR); g->shop_err = 0.5f; return; }
        p->cash -= cost;
        p->wlvl[w]++;
        g->shop_flash = 0.5f;
        sfx_ui(g, SFX_BUY);
        return;
    }
    int u = i - NWEAP, cost = upgrade_cost(g, u);
    if (cost < 0 || p->cash < cost || (u == UP_HEAL && p->hp >= p->maxhp)) { sfx_ui(g, SFX_ERROR); g->shop_err = 0.5f; return; }
    p->cash -= cost;
    if (u == UP_HEAL) p->hp = fminf(p->maxhp, p->hp + p->maxhp * 0.5f);
    else {
        p->up[u]++;
        if (u == UP_VITAL) { p->maxhp += 20.0f; p->hp += 20.0f; }
    }
    g->shop_flash = 0.5f;
    sfx_ui(g, SFX_BUY);
}

/* ---- pause ---- */
enum { PM_RESUME, PM_AUTO, PM_MUSIC, PM_SFX, PM_QUIT, PM_N };

static void pause_update(Game *g, const Input *in)
{
    int dx, dy;
    if (pressed_dir(in, &dx, &dy, g) && dy) g->pause_sel = (g->pause_sel + dy + PM_N) % PM_N;
    if (in->pressed & (BTN_PLUS | BTN_B)) { go(g, SC_PLAY); sfx_ui(g, SFX_PAUSE); return; }
    if (in->pressed & BTN_A) {
        switch (g->pause_sel) {
        case PM_RESUME: go(g, SC_PLAY); sfx_ui(g, SFX_PAUSE); break;
        case PM_AUTO: g->sv.autofire ^= 1; sfx_ui(g, SFX_SELECT); game_save_now(g); break;
        case PM_MUSIC: g->sv.music ^= 1; sfx_ui(g, SFX_SELECT); game_save_now(g); break;
        case PM_SFX: g->sv.sfx ^= 1; sfx_ui(g, SFX_SELECT); game_save_now(g); break;
        case PM_QUIT: end_run(g, g->biome); g->over_t = 0; go(g, SC_OVER); sfx_ui(g, SFX_GAMEOVER); break;
        }
    }
}

/* ---- the fight ---- */
static void play_update(Game *g, const Input *in, float dt)
{
    if (in->pressed & BTN_PLUS && g->p.hp > 0 && g->phase != PH_BOSSDEAD) { g->pause_sel = 0; go(g, SC_PAUSE); sfx_ui(g, SFX_PAUSE); return; }

    float sdt = dt;
    if (g->slowmo > 0) { g->slowmo -= dt; sdt *= 0.35f; }
    Input pin = *in;
    if (g->p.hp <= 0) { pin.held = 0; pin.pressed = 0; pin.sx = pin.sy = pin.rx = pin.ry = 0; }
    world_update(g, &pin, sdt);

    if (g->p.hp <= 0) {
        g->over_t += dt;
        if (g->over_t < dt * 1.5f) { g->slowmo = 1.5f; sfx_ui(g, SFX_GAMEOVER); announce(g, "YOU DIED", 0xFF2020FFu, SFX_V_GAMEOVER); }
        if (g->over_t > 2.6f) { end_run(g, g->biome); g->over_t = 0; go(g, SC_OVER); }
        return;
    }
    if (g->phase == PH_WAVEEND && g->phase_t > 2.8f) {
        world_collect_all(g);
        g->shop_after = 0;
        g->pick_weapon = -1;
        if (g->shop_sel >= SHOP_N) g->shop_sel = 0;
        go(g, SC_SHOP);
    }
    if (g->phase == PH_BOSSDEAD && g->phase_t > 4.5f) {
        world_collect_all(g);
        if (g->biome + 2 > g->sv.biomes && g->biome + 1 < NBIOMES) g->sv.biomes = (uint8_t)(g->biome + 2);
        game_save_now(g);
        sfx_ui(g, SFX_VICTORY);
        announce(g, NULL, 0, SFX_V_CLEARED);
        go(g, SC_CLEAR);
    }
}

void game_update(Game *g, const Input *in, float dt)
{
    g->t += dt;
    g->scene_t += dt;
    if (g->shop_flash > 0) g->shop_flash -= dt;
    if (g->shop_err > 0) g->shop_err -= dt;
    g->save_msg_t += dt;
    for (int i = 0; i < SFX_COUNT; i++) if (g->snd_cool[i] > 0) g->snd_cool[i] -= dt;

    switch (g->scene) {
    case SC_TITLE: title_update(g, in); break;
    case SC_HELP: help_update(g, in); break;
    case SC_ARMORY: armory_update(g, in); break;
    case SC_BIOMES: biomes_update(g, in); break;
    case SC_INTRO:
        if (g->scene_t > 3.4f || (g->scene_t > 0.6f && (in->pressed & (BTN_A | BTN_PLUS)))) { world_begin_wave(g); go(g, SC_PLAY); }
        break;
    case SC_PLAY: play_update(g, in, dt); break;
    case SC_PAUSE: pause_update(g, in); break;
    case SC_SHOP: shop_update(g, in); break;
    case SC_CLEAR:
        if (g->scene_t > 1.2f && (in->pressed & (BTN_A | BTN_PLUS))) {
            sfx_ui(g, SFX_SELECT);
            if (g->biome + 1 >= NBIOMES) {
                g->sv.wins++;
                end_run(g, NBIOMES);
                go(g, SC_WIN);
            } else { g->shop_after = 1; g->pick_weapon = -1; g->shop_sel = SHOP_N - 1; go(g, SC_SHOP); }
        }
        break;
    case SC_OVER: case SC_WIN:
        if (g->scene_t > 1.5f && (in->pressed & (BTN_A | BTN_PLUS))) { sfx_ui(g, SFX_SELECT); g->menu = 0; go(g, SC_TITLE); }
        break;
    }
    music_mix(g, dt);
}

void game_render(const Game *g)
{
    gfx_begin();
    switch (g->scene) {
    case SC_PLAY: case SC_PAUSE: case SC_INTRO:
        draw_world(g);
        hud_draw(g);
        break;
    case SC_SHOP: case SC_CLEAR:
        draw_world(g);
        break;
    default: break;
    }
    hud_menus(g);
}
