/*
 * LAST STAND - the HUD and every menu: title, how to play, the armory, the biome map, the intro cards,
 * pause, the trading post between waves, and the end screens.
 */
#include "game_int.h"
#include "gfx.h"

#include <string.h>
#include <stdio.h>

#define GOLD   0xFFC93AFFu
#define FIRE   0xFF7A22FFu
#define BONE   0xEDE4D0FFu
#define DIM    0x9A92A0FFu
#define BLOOD  0xD8282AFFu
#define PANEL  0x161218E8u
#define PANEL2 0x221C26F0u

void shop_item_pos(int i, float *x, float *y);

static const char *const SLOT_BTN[NSLOTS] = { "A", "B", "X", "Y" };
static const uint32_t SLOT_COL[NSLOTS] = { 0xE84A3AFFu, 0xF0C030FFu, 0x40A8F0FFu, 0x50C860FFu };

static void panel(float x, float y, float w, float h, uint32_t c, uint32_t edge)
{
    gfx_rect(x, y, w, h, c);
    gfx_rect(x, y, w, 2, edge); gfx_rect(x, y + h - 2, w, 2, edge);
    gfx_rect(x, y, 2, h, edge); gfx_rect(x + w - 2, y, 2, h, edge);
}

/* level pips centred on cx, spanning w */
static void pips(float cx, float y, float w, float h, int n, int filled)
{
    float step = w / (float)n;
    for (int l = 0; l < n; l++) gfx_rect(cx - w * 0.5f + (float)l * step, y, step - 2.0f, h, l < filled ? GOLD : 0x3A3440FFu);
}

static void bar(float x, float y, float w, float h, float k, uint32_t c, uint32_t bg)
{
    gfx_rect(x - 2, y - 2, w + 4, h + 4, 0x0A080AFFu);
    gfx_rect(x, y, w, h, bg);
    k = clampf(k, 0, 1);
    gfx_rect_v(x, y, w * k, h, col_mix(c, 0xFFFFFFFFu, 0.3f), c);
}

/* fire-coloured big lettering: dark shadow, orange body, yellow top rows */
static void fire_text(const char *s, float cx, float y, float px, float t)
{
    float w = gfx_text_width(s, px), x = cx - w * 0.5f;
    gfx_text(s, x + px * 0.8f, y + px * 0.8f, px, 0x000000C0u);
    gfx_text(s, x, y, px, 0xC8281AFFu);
    gfx_text_top(s, x, y, px, 0xFF8A20FFu, 3);
    gfx_text_top(s, x, y, px, 0xFFE070FFu, 1);
    (void)t;
}

static void fmt_time(char *buf, int n, float secs)
{
    int s = (int)secs;
    snprintf(buf, (size_t)n, "%02d:%02d", s / 60, s % 60);
}

/* ------------------------------------------------------------------ */
/* the in-game HUD                                                      */
/* ------------------------------------------------------------------ */
static void weapon_slots(const Game *g, float cx, float y, float sz)
{
    const Player *p = &g->p;
    float gap = sz + 10.0f, x0 = cx - gap * 1.5f;
    for (int s = 0; s < NSLOTS; s++) {
        float x = x0 + (float)s * gap;
        int w = p->slot[s], sel = s == p->sel;
        float pop = sel ? 1.0f + 0.15f * clampf(1.0f - p->sel_t * 5.0f, 0, 1) : 1.0f;
        float h = sz * pop;
        if (sel) { gfx_blend(BL_ADD); gfx_glow(x, y, sz * 1.1f, 0xFFB03060u); gfx_blend(BL_ALPHA); }
        panel(x - h * 0.5f, y - h * 0.5f, h, h, sel ? 0x3A2A1EF0u : PANEL, sel ? GOLD : 0x4A4050FFu);
        if (w >= 0) {
            art_weapon_icon(w, x, y, sz / 26.0f * pop, -0.35f, 0xFFFFFFFFu);
            pips(x, y + h * 0.5f - 7.0f, h - 10.0f, 3.0f, MAX_WLVL, p->wlvl[w]);
            if (sel && w == W_GATLING && p->spin > 0) gfx_rect(x - h * 0.5f + 4, y - h * 0.5f + 4, (h - 8) * p->spin, 2, FIRE);
        }
        /* the button letter */
        gfx_poly(x - h * 0.5f + 2, y - h * 0.5f + 2, 9, 12, 0, 0x0A080AFFu);
        gfx_poly(x - h * 0.5f + 2, y - h * 0.5f + 2, 7.5f, 12, 0, SLOT_COL[s]);
        gfx_text_c(SLOT_BTN[s], x - h * 0.5f + 2.5f, y - h * 0.5f - 2.5f, 1.2f, 0x101010FFu);
    }
    /* dash */
    float dx = x0 - gap * 0.5f - 34.0f;
    float k = p->dash_cd > 0 ? 1.0f - p->dash_cd / 1.1f : 1.0f;
    gfx_ring(dx, y, 16, 4, 20, 0, 0x2A2430FFu);
    gfx_arc(dx, y, 14, 18, -PI_F / 2, -PI_F / 2 + TAU_F * k, 20, k >= 1 ? 0x60D0FFFFu : 0x4080A0FFu, k >= 1 ? 0x60D0FFFFu : 0x4080A0FFu);
    gfx_text_c("L", dx, y - 6, 1.3f, k >= 1 ? BONE : DIM);
    gfx_text_c("DASH", dx, y + 22, 1.0f, DIM);
    /* fire hint */
    float fx = x0 + gap * 3.5f + 34.0f;
    int firing = g->sv.autofire;
    gfx_text_c(firing ? "AUTO" : "R/ZR", fx, y - 8, 1.2f, firing ? GOLD : BONE);
    gfx_text_c("FIRE", fx, y + 6, 1.0f, DIM);
}

void hud_draw(const Game *g)
{
    const Player *p = &g->p;
    char buf[64];
    gfx_view_reset();
    if (g->scene == SC_INTRO) return;

    /* health */
    float hk = p->hp / p->maxhp;
    gfx_text_s("HP", 16, 14, 1.6f, BONE);
    uint32_t hc = hk < 0.3f ? col_mix(BLOOD, 0xFF8080FFu, 0.5f + 0.5f * sinf(g->t * 10)) : BLOOD;
    bar(42, 13, 190, 13, hk, hc, 0x3A1014FFu);
    snprintf(buf, sizeof buf, "%d/%d", (int)ceilf(p->hp), (int)p->maxhp);
    gfx_text_s(buf, 46, 16, 1.1f, 0xFFFFFFFFu);
    /* last stands left */
    for (int k = 0; k < p->stands; k++) {
        float x = 248 + (float)k * 18;
        gfx_poly(x, 19, 7, 10, 0, 0x0A080AFFu); gfx_poly(x, 18, 5.5f, 10, 0, BONE);
        gfx_rect(x - 2.5f, 17, 2, 2, 0x200808FFu); gfx_rect(x + 0.5f, 17, 2, 2, 0x200808FFu);
    }
    /* cash */
    snprintf(buf, sizeof buf, "$%d", p->cash_shown);
    gfx_poly(24, 44, 7, 10, 0, 0x7A5A10FFu); gfx_poly(24, 44, 5.5f, 10, 0, GOLD);
    gfx_text_s(buf, 36, 38, 2.0f, GOLD);
    /* frenzy / magnet timers */
    float ty = 62;
    if (p->frenzy > 0) { bar(16, ty, 120, 6, p->frenzy / 9.0f, 0xE040FFFFu, 0x2A102AFFu); gfx_text_s("FRENZY", 142, ty - 2, 1.1f, 0xF0A0FFFFu); ty += 14; }
    if (p->stand_t > 0) { bar(16, ty, 120, 6, p->stand_t / 5.0f, 0xFF4020FFu, 0x2A0A0AFFu); gfx_text_s("LAST STAND", 142, ty - 2, 1.1f, 0xFF8060FFu); ty += 14; }

    /* the clock and the wave */
    if (g->phase == PH_BOSS || g->phase == PH_BOSSDEAD) {
        const char *bn = BOSS_NAME[g->biome];
        gfx_text_sc(bn, VW * 0.5f, 10, 1.8f, 0xFF5040FFu);
        bar(VW * 0.5f - 210, 30, 420, 12, g->boss_hp_shown, 0xD82020FFu, 0x2A0808FFu);
        if (g->boss >= 0 && g->en[g->boss].alive) {
            float real = g->en[g->boss].hp / g->en[g->boss].maxhp;
            gfx_rect(VW * 0.5f - 210, 30, 420 * clampf(real, 0, 1), 12, 0xFF3030FFu);
        }
        fmt_time(buf, sizeof buf, g->run_t);
        gfx_text_sc(buf, VW * 0.5f, 48, 1.4f, DIM);
    } else {
        fmt_time(buf, sizeof buf, g->run_t);
        gfx_text_sc(buf, VW * 0.5f, 10, 2.6f, BONE);
        snprintf(buf, sizeof buf, "WAVE %d/%d", g->wave + 1, NWAVES);
        gfx_text_sc(buf, VW * 0.5f, 34, 1.3f, GOLD);
        float wk = (g->run_t - (float)g->wave * WAVE_TIME) / WAVE_TIME;
        bar(VW * 0.5f - 60, 48, 120, 4, wk, FIRE, 0x2A1A14FFu);
    }

    /* kills and combo */
    snprintf(buf, sizeof buf, "%d", p->kills);
    gfx_text_sr(buf, VW - 34, 12, 2.0f, BONE);
    gfx_poly(VW - 20, 19, 8, 10, 0, 0x0A080AFFu); gfx_poly(VW - 20, 18, 6.5f, 10, 0, BONE);
    gfx_rect(VW - 23.5f, 16, 2.5f, 3, 0x200808FFu); gfx_rect(VW - 19, 16, 2.5f, 3, 0x200808FFu);
    if (g->combo >= 10) {
        float pop = 1.0f + 0.2f * clampf(g->combo_t - 2.2f, 0, 0.3f) / 0.3f;
        snprintf(buf, sizeof buf, "X%d COMBO", g->combo);
        uint32_t cc = g->combo >= 250 ? 0xFF4040FFu : (g->combo >= 100 ? FIRE : GOLD);
        gfx_text_sr(buf, VW - 16, 38, 1.6f * pop, cc);
        bar(VW - 116, 56, 100, 3, g->combo_t / 2.5f, cc, 0x201810FFu);
    }

    weapon_slots(g, VW * 0.5f, VH - 40, 46);

    /* first wave: how to fight */
    if (g->biome == 0 && g->wave == 0 && g->run_t < 12.0f && g->phase == PH_WAVE) {
        float a = clampf(12.0f - g->run_t, 0, 1);
        gfx_text_sc("HOLD R OR ZR TO FIRE   -   A B X Y SWITCH GUNS   -   L TO DASH", VW * 0.5f, VH - 86, 1.2f, col_a(BONE, a));
        gfx_text_sc("AIM WITH THE RIGHT STICK, OR LET IT AIM FOR YOU", VW * 0.5f, VH - 72, 1.0f, col_a(DIM, a));
    }

    /* wave banner */
    if ((g->phase == PH_WAVE || g->phase == PH_BOSS) && g->wave_banner < 2.4f && g->scene == SC_PLAY) {
        float k = g->wave_banner;
        float a = k < 0.3f ? k / 0.3f : (k > 1.8f ? (2.4f - k) / 0.6f : 1.0f);
        float sc = 1.0f + 0.4f * clampf(0.3f - k, 0, 0.3f) / 0.3f;
        if (g->phase == PH_BOSS) {
            gfx_rect(0, VH * 0.5f - 46, VW, 92, col_a(0x300000FFu, 0.6f * a));
            gfx_text_sc("BOSS", VW * 0.5f, VH * 0.5f - 38, 5.0f * sc, col_a(0xFF3020FFu, a));
            gfx_text_sc(BOSS_NAME[g->biome], VW * 0.5f, VH * 0.5f + 12, 2.0f, col_a(BONE, a));
        } else {
            snprintf(buf, sizeof buf, "WAVE %d", g->wave + 1);
            gfx_text_sc(buf, VW * 0.5f, VH * 0.5f - 70, 4.5f * sc, col_a(GOLD, a));
            if (g->wave == NWAVES - 1) gfx_text_sc("FINAL WAVE", VW * 0.5f, VH * 0.5f - 26, 1.8f, col_a(FIRE, a));
        }
    }
    /* announcements */
    if (g->announce && g->announce_t < 2.4f) {
        float k = g->announce_t;
        float a = k > 1.8f ? (2.4f - k) / 0.6f : 1.0f;
        float sc = k < 0.15f ? 1.6f - k * 4.0f : 1.0f;
        gfx_text_wave(g->announce, VW * 0.5f + 2, VH * 0.28f + 2, 3.2f * sc, col_a(0x000000FFu, a * 0.8f), g->t, 3.0f);
        gfx_text_wave(g->announce, VW * 0.5f, VH * 0.28f, 3.2f * sc, col_a(g->announce_col, a), g->t, 3.0f);
    }
    if (g->p.hp <= 0) {
        float a = clampf(g->over_t, 0, 1);
        gfx_text_sc("YOU DIED", VW * 0.5f, VH * 0.42f, 5.0f, col_a(BLOOD, a));
    }
}

/* ------------------------------------------------------------------ */
/* a backdrop for the menus: a blood-red dusk over a hill of graves     */
/* ------------------------------------------------------------------ */
static void menu_bg(const Game *g, int biome)
{
    static const uint32_t SKY[NBIOMES][2] = {
        { 0x3A1410FFu, 0xB04A1AFFu }, { 0x0E1A14FFu, 0x3A5A30FFu }, { 0x0C0C14FFu, 0x3A3A5AFFu }, { 0x100606FFu, 0x7A2008FFu }, { 0x06020AFFu, 0x5A0A2AFFu } };
    gfx_rect_v(0, 0, VW, VH * 0.62f, SKY[biome][0], SKY[biome][1]);
    gfx_rect(0, VH * 0.62f, VW, VH * 0.38f, 0x0E0A0CFFu);
    gfx_blend(BL_ADD);
    gfx_glow(VW * 0.72f, VH * 0.24f, 160, col_a(SKY[biome][1], 0.5f));
    gfx_blend(BL_ALPHA);
    gfx_poly(VW * 0.72f, VH * 0.24f, 46, 24, 0, col_mix(SKY[biome][1], 0xFFE8C0FFu, 0.6f));
    /* the horizon: hills, crosses, dead trees */
    for (int i = 0; i < 18; i++) {
        float x = (float)i * 52.0f - 20.0f, h = 30.0f + 18.0f * sinf((float)i * 1.7f);
        gfx_tri(x - 60, VH * 0.62f, 0x0E0A0CFFu, x, VH * 0.62f - h, 0x0E0A0CFFu, x + 60, VH * 0.62f, 0x0E0A0CFFu);
        if (i % 3 == 1) { gfx_rect(x - 2, VH * 0.62f - h - 26, 4, 28, 0x0E0A0CFFu); gfx_rect(x - 9, VH * 0.62f - h - 18, 18, 4, 0x0E0A0CFFu); }
        if (i % 5 == 3) { gfx_line(x, VH * 0.62f - h, x - 8, VH * 0.62f - h - 46, 4, 0x0E0A0CFFu, 0x0E0A0CFFu); gfx_line(x - 5, VH * 0.62f - h - 30, x - 20, VH * 0.62f - h - 44, 2.5f, 0x0E0A0CFFu, 0x0E0A0CFFu); }
    }
    /* embers rising */
    gfx_blend(BL_ADD);
    for (int i = 0; i < 60; i++) {
        uint32_t h = hash32((uint32_t)i * 2246822519u);
        float sp = 20.0f + (float)(h & 63);
        float x = fmodf((float)((h >> 6) & 1023) + sinf(g->t * 0.7f + (float)i) * 20.0f, VW);
        float y = VH - fmodf(g->t * sp + (float)((h >> 16) & 511), VH + 20.0f);
        gfx_glow_cheap(x, y, 4.0f + (float)(h & 3), biome == 4 ? 0xFF305080u : 0xFF802080u);
    }
    gfx_blend(BL_ALPHA);
}

static void vignette_menu(void)
{
    uint32_t edge = 0x000000E0u, mid = 0x00000000u;
    float cx = VW * 0.5f, cy = VH * 0.5f;
    for (int i = 0; i < 24; i++) {
        float a0 = (float)i / 24.0f * TAU_F, a1 = (float)(i + 1) / 24.0f * TAU_F;
        gfx_quad4(cx + cosf(a0) * VW * 0.4f, cy + sinf(a0) * VH * 0.4f, mid, cx + cosf(a0) * VW * 0.8f, cy + sinf(a0) * VH * 0.86f, edge,
                  cx + cosf(a1) * VW * 0.8f, cy + sinf(a1) * VH * 0.86f, edge, cx + cosf(a1) * VW * 0.4f, cy + sinf(a1) * VH * 0.4f, mid);
    }
}

/* ------------------------------------------------------------------ */
/* title                                                                */
/* ------------------------------------------------------------------ */
static void draw_title(const Game *g)
{
    char buf[64];
    menu_bg(g, 0);
    /* the horde on the ridge, the gunslinger in front */
    static const int H[] = { A_BANDIT, A_ZOMBIE, A_LANTERN, A_HULK, A_TOAD, A_DOCTOR, A_COFFIN, A_IMP, A_CRAWLER, A_BONES };
    for (int i = 0; i < 10; i++) {
        float x = 60.0f + (float)i * 82.0f + sinf((float)i * 3.1f) * 20.0f, y = VH * 0.66f + (float)(i % 3) * 8.0f;
        art_enemy_preview(H[i], x, y, 1.6f, g->t + (float)i);
    }
    gfx_rect(0, VH * 0.6f, VW, VH * 0.4f, 0x0A06088Cu);
    art_shadow(VW * 0.5f, VH * 0.82f + 18, 30, 8, 1);
    art_player(VW * 0.5f, VH * 0.82f, g->char_sel, 1, 0, 1, 0, g->t, 0, 3.0f);
    art_weapon_held(W_REVOLVER, VW * 0.5f + 26, VH * 0.82f - 8, -0.3f, 3.0f, 0, g->t);
    gfx_blend(BL_ADD);
    gfx_glow(VW * 0.5f + 3, VH * 0.82f - 36, 30, 0xFFC03060u);
    gfx_blend(BL_ALPHA);
    vignette_menu();

    float bob = sinf(g->t * 1.5f) * 3.0f;
    gfx_blend(BL_ADD);
    gfx_glow(VW * 0.5f, 92 + bob, 260, 0xFF401860u);
    gfx_blend(BL_ALPHA);
    fire_text("LAST STAND", VW * 0.5f, 56 + bob, 10.0f, g->t);
    gfx_text_sc("A BULLET HEAVEN ON THE WII", VW * 0.5f, 146, 1.4f, DIM);

    static const char *const ITEMS[] = { "RIDE OUT", "HOW TO PLAY", "AUTO FIRE", "MUSIC", "SOUND" };
    for (int i = 0; i < 5; i++) {
        float y = 186.0f + (float)i * 26.0f;
        int sel = g->menu == i;
        const char *val = i == 2 ? (g->sv.autofire ? ": ON" : ": OFF") : (i == 3 ? (g->sv.music ? ": ON" : ": OFF") : (i == 4 ? (g->sv.sfx ? ": ON" : ": OFF") : ""));
        snprintf(buf, sizeof buf, "%s%s", ITEMS[i], val);
        if (sel) {
            float w = gfx_text_width(buf, 2.0f);
            gfx_rect(VW * 0.5f - w * 0.5f - 18, y - 5, w + 36, 24, 0x3A140CC0u);
            gfx_text_sc(">", VW * 0.5f - w * 0.5f - 10, y, 2.0f, FIRE);
        }
        gfx_text_sc(buf, VW * 0.5f, y, 2.0f, sel ? GOLD : BONE);
    }
    snprintf(buf, sizeof buf, "SOULS %d", g->sv.souls);
    gfx_text_s(buf, 16, VH - 24, 1.4f, 0x80E0FFFFu);
    if (g->sv.best_kills > 0) { snprintf(buf, sizeof buf, "BEST %d KILLS", g->sv.best_kills); gfx_text_sr(buf, VW - 16, VH - 24, 1.4f, DIM); }
    const char *st = g->storage_ok ? (g->save_state == 2 ? "SAVE FAILED - CHECK THE SD CARD" : "PROGRESS SAVES TO THE SD CARD") : "NO SD CARD - PROGRESS WILL NOT BE SAVED";
    gfx_text_sc(st, VW * 0.5f, VH - 22, 1.0f, g->storage_ok && g->save_state != 2 ? 0x6A6470FFu : 0xD06040FFu);
}

static void draw_help(const Game *g)
{
    menu_bg(g, 2);
    gfx_rect(0, 0, VW, VH, 0x000000A0u);
    panel(60, 40, VW - 120, VH - 80, PANEL, 0x5A4A3AFFu);
    char buf[48];
    snprintf(buf, sizeof buf, "HOW TO PLAY  %d/3", g->help_page + 1);
    gfx_text_sc(buf, VW * 0.5f, 56, 2.0f, GOLD);
    static const char *const P0[] = {
        "LEFT STICK ........ MOVE",
        "RIGHT STICK ....... AIM (CENTRED: IT AIMS FOR YOU)",
        "R OR ZR ........... FIRE THE GUN IN YOUR HAND",
        "A  B  X  Y ........ SWITCH TO THE GUN ON THAT BUTTON",
        "L OR ZL ........... DASH (YOU CAN'T BE HIT MID-DASH)",
        "+ ................. PAUSE      - ...... 16:9 / 4:3",
        "",
        "YOUR OTHER GUNS FLOAT AROUND YOU AND FIRE ON THEIR OWN,",
        "SLOWER. UPGRADE THE BACKUP RIG TO MAKE THEM FASTER.",
    };
    static const char *const P1[] = {
        "EACH BIOME IS TEN WAVES OF ONE MINUTE.",
        "SURVIVE A WAVE AND EVERYTHING LEFT BLOWS UP.",
        "THEN THE TRADING POST: SPEND YOUR BOUNTY ON",
        "NEW GUNS (UP TO FOUR, ONE PER BUTTON), GUN LEVELS",
        "AND YOURSELF.",
        "",
        "AT 10:00 THE BOSS ARRIVES. PUT IT DOWN TO RIDE ON",
        "TO THE NEXT BIOME - EACH ONE DARKER THAN THE LAST.",
    };
    static const char *const P2[] = {
        "SHOOT THE POWDER KEGS.  ELITES (GOLD) DROP CHESTS.",
        "WHISKEY HEALS.  THE MAGNET PULLS IN ALL THE COIN.",
        "THE TNT CRATE BLOWS UP THE WHOLE SCREEN.",
        "THE PURPLE SKULL IS A FRENZY: EVERY GUN AT FULL TILT.",
        "",
        "FALL ONCE AND YOU GET A LAST STAND: FIVE SECONDS OF",
        "FURY, THEN YOU GET BACK UP.  ONLY ONCE PER RUN...",
        "",
        "WHEN YOU DIE YOU KEEP YOUR SOULS: SPEND THEM IN THE",
        "ARMORY ON NEW GUNSLINGERS AND PERKS.",
    };
    const char *const *P = g->help_page == 0 ? P0 : (g->help_page == 1 ? P1 : P2);
    int n = g->help_page == 0 ? 9 : (g->help_page == 1 ? 8 : 10);
    for (int i = 0; i < n; i++) gfx_text(P[i], 96, 100.0f + (float)i * 28.0f, 1.6f, BONE);
    gfx_text_sc("A: NEXT   B: BACK", VW * 0.5f, VH - 66, 1.3f, DIM);
}

/* ------------------------------------------------------------------ */
/* armory                                                               */
/* ------------------------------------------------------------------ */
static void draw_armory(const Game *g)
{
    char buf[64];
    menu_bg(g, 0);
    gfx_rect(0, 0, VW, VH, 0x000000A0u);
    fire_text("THE ARMORY", VW * 0.5f, 18, 4.0f, g->t);
    snprintf(buf, sizeof buf, "SOULS: %d", g->sv.souls);
    gfx_text_sc(buf, VW * 0.5f, 56, 1.8f, g->shop_err > 0 ? 0xFF4040FFu : 0x80E0FFFFu);
    for (int c = 0; c < NCHARS; c++) {
        float x = 120.0f + (float)c * 205.0f, y = 80;
        int sel = g->armory_sel == c, open = g->sv.chr_open[c];
        panel(x - 92, y, 184, 210, sel ? PANEL2 : PANEL, sel ? GOLD : 0x3A3240FFu);
        if (sel) { gfx_blend(BL_ADD); gfx_glow(x, y + 90, 90, 0xFFA03030u); gfx_blend(BL_ALPHA); }
        art_shadow(x, y + 128, 24, 6, 1);
        art_player(x, y + 112, c, 1, sel ? g->t * 6.0f : 0.0f, 1, 0, g->t, 0, 2.6f);
        if (!open) gfx_rect(x - 90, y + 2, 180, 150, 0x000000B0u);
        gfx_text_sc(CHAR_NAME[c], x, y + 150, 1.4f, open ? BONE : DIM);
        gfx_text_sc(CHAR_DESC[c], x, y + 168, 0.85f, DIM);
        if (open) {
            for (int l = 0; l < 5; l++) gfx_rect(x - 45 + (float)l * 19, y + 186, 15, 5, l < g->sv.chr_lvl[c] ? GOLD : 0x3A3440FFu);
            int lc = char_lvl_cost(g, c);
            if (sel) { if (lc > 0) snprintf(buf, sizeof buf, "Y: LEVEL UP (%d SOULS)", lc); else snprintf(buf, sizeof buf, "MAX LEVEL"); gfx_text_sc(buf, x, y + 196, 0.9f, 0x80E0FFFFu); }
            if (g->char_sel == c) gfx_text_sc("CHOSEN", x, y + 8, 1.2f, GOLD);
        } else {
            snprintf(buf, sizeof buf, "LOCKED: %d SOULS", char_cost(g, c));
            gfx_text_sc(buf, x, y + 190, 1.1f, g->sv.souls >= char_cost(g, c) ? 0x80E0FFFFu : 0xC05050FFu);
            gfx_text_sc("?", x, y + 60, 6.0f, 0x6A6070FFu);
        }
    }
    gfx_text_sc("PERKS (FOR EVERY RUN)", VW * 0.5f, 304, 1.4f, GOLD);
    for (int k = 0; k < NPERKS; k++) {
        float x = 120.0f + (float)k * 205.0f, y = 324;
        int sel = g->armory_sel == NCHARS + k;
        panel(x - 92, y, 184, 76, sel ? PANEL2 : PANEL, sel ? GOLD : 0x3A3240FFu);
        gfx_text_sc(PERK_NAME[k], x, y + 10, 1.5f, BONE);
        gfx_text_sc(PERK_DESC[k], x, y + 30, 0.9f, DIM);
        for (int l = 0; l < 3; l++) gfx_rect(x - 30 + (float)l * 21, y + 46, 17, 5, l < g->sv.perk[k] ? GOLD : 0x3A3440FFu);
        int pc = perk_cost(g, k);
        if (pc > 0) snprintf(buf, sizeof buf, "%d SOULS", pc); else snprintf(buf, sizeof buf, "MAXED");
        gfx_text_sc(buf, x, y + 58, 1.0f, pc > 0 && g->sv.souls >= pc ? 0x80E0FFFFu : 0x8A6A6AFFu);
    }
    if (g->shop_flash > 0) { gfx_blend(BL_ADD); gfx_rect(0, 0, VW, VH, col_a(0x80E0FF40u, g->shop_flash)); gfx_blend(BL_ALPHA); }
    gfx_text_sc(g->armory_sel < NCHARS ? (g->sv.chr_open[g->armory_sel] ? "A: RIDE WITH THEM   Y: LEVEL UP   B: BACK" : "A: UNLOCK   B: BACK") : "A: BUY PERK   B: BACK",
                VW * 0.5f, VH - 30, 1.3f, BONE);
}

/* ------------------------------------------------------------------ */
/* biome map                                                            */
/* ------------------------------------------------------------------ */
static void draw_biomes(const Game *g)
{
    char buf[64];
    int b = g->biome_sel;
    menu_bg(g, b < g->sv.biomes ? b : 4);
    gfx_rect(0, 0, VW, VH, 0x00000070u);
    fire_text("WHERE TO MAKE YOUR STAND", VW * 0.5f, 18, 3.0f, g->t);
    for (int i = 0; i < NBIOMES; i++) {
        float x = 107.0f + (float)i * 160.0f, y = 120;
        int sel = i == b, open = i < g->sv.biomes;
        float h = sel ? 200 : 180, w = sel ? 150 : 136;
        panel(x - w * 0.5f, y - (sel ? 10 : 0), w, h, sel ? PANEL2 : PANEL, sel ? GOLD : 0x3A3240FFu);
        if (open) {
            art_shadow(x, y + 110, 26, 7, 1);
            art_boss_portrait(i, x, y + 92, 0.9f, g->t);
            gfx_text_sc(BIOME_NAME[i], x, y + 128, 1.2f, BONE);
        } else {
            gfx_text_sc("?", x, y + 50, 6.0f, 0x5A5060FFu);
            gfx_text_sc("LOCKED", x, y + 128, 1.2f, DIM);
        }
        snprintf(buf, sizeof buf, "BIOME %d", i + 1);
        gfx_text_sc(buf, x, y + 10, 1.1f, sel ? GOLD : DIM);
        for (int d = 0; d <= i; d++) gfx_poly(x - (float)i * 6 + (float)d * 12, y + 152, 4, 3, -PI_F / 2, 0xC03020FFu);   /* danger */
    }
    if (b < g->sv.biomes) {
        gfx_text_sc(BIOME_SUB[b], VW * 0.5f, 350, 1.6f, 0xD0C0B0FFu);
        snprintf(buf, sizeof buf, "BOSS: %s", BOSS_NAME[b]);
        gfx_text_sc(buf, VW * 0.5f, 376, 1.4f, 0xFF6050FFu);
        gfx_text_sc("A: RIDE OUT   B: BACK", VW * 0.5f, VH - 34, 1.4f, BONE);
    } else {
        gfx_text_sc("DEFEAT THE BOSS OF THE BIOME BEFORE IT TO RIDE HERE", VW * 0.5f, 360, 1.3f, DIM);
        gfx_text_sc("B: BACK", VW * 0.5f, VH - 34, 1.4f, BONE);
    }
}

/* ------------------------------------------------------------------ */
/* intro card                                                           */
/* ------------------------------------------------------------------ */
static void draw_intro(const Game *g)
{
    char buf[64];
    float k = g->scene_t;
    float a = k < 0.4f ? k / 0.4f : (k > 2.9f ? clampf((3.4f - k) / 0.5f, 0, 1) : 1.0f);
    gfx_rect(0, 0, VW, VH, col_a(0x000000FFu, 0.7f * a + 0.3f));
    float bar_h = 70 * a;
    gfx_rect(0, 0, VW, bar_h, 0x000000FFu); gfx_rect(0, VH - bar_h, VW, bar_h, 0x000000FFu);
    snprintf(buf, sizeof buf, "BIOME %d OF 5", g->biome + 1);
    gfx_text_sc(buf, VW * 0.5f, 150, 1.6f, col_a(DIM, a));
    gfx_blend(BL_ADD);
    gfx_glow(VW * 0.5f, 200, 220, col_a(0xFF401880u, a));
    gfx_blend(BL_ALPHA);
    float sc = 1.0f + 0.1f * clampf(0.4f - k, 0, 0.4f);
    float w = gfx_text_width(BIOME_NAME[g->biome], 6.0f * sc);
    if (a > 0.05f) fire_text(BIOME_NAME[g->biome], VW * 0.5f, 176, 6.0f * sc, g->t);
    (void)w;
    gfx_text_sc(BIOME_SUB[g->biome], VW * 0.5f, 240, 1.6f, col_a(0xD0C0B0FFu, a));
    snprintf(buf, sizeof buf, "SURVIVE 10 WAVES.  %s COMES AT 10:00.", BOSS_NAME[g->biome]);
    gfx_text_sc(buf, VW * 0.5f, 290, 1.2f, col_a(0xFF6050FFu, a));
}

/* ------------------------------------------------------------------ */
/* pause                                                                */
/* ------------------------------------------------------------------ */
static void draw_pause(const Game *g)
{
    gfx_rect(0, 0, VW, VH, 0x000000A8u);
    fire_text("PAUSED", VW * 0.5f, 100, 5.0f, g->t);
    static const char *const ITEMS[] = { "RESUME", "AUTO FIRE", "MUSIC", "SOUND", "ABANDON THE RUN" };
    char buf[48];
    for (int i = 0; i < 5; i++) {
        float y = 190.0f + (float)i * 30.0f;
        const char *val = i == 1 ? (g->sv.autofire ? ": ON" : ": OFF") : (i == 2 ? (g->sv.music ? ": ON" : ": OFF") : (i == 3 ? (g->sv.sfx ? ": ON" : ": OFF") : ""));
        snprintf(buf, sizeof buf, "%s%s", ITEMS[i], val);
        int sel = g->pause_sel == i;
        if (sel) { float w = gfx_text_width(buf, 2.0f); gfx_rect(VW * 0.5f - w * 0.5f - 16, y - 5, w + 32, 24, 0x3A140CC0u); }
        gfx_text_sc(buf, VW * 0.5f, y, 2.0f, sel ? GOLD : BONE);
    }
    char st[64];
    snprintf(st, sizeof st, "%s - WAVE %d", BIOME_NAME[g->biome], g->wave + 1);
    gfx_text_sc(st, VW * 0.5f, 360, 1.3f, DIM);
}

/* ------------------------------------------------------------------ */
/* the trading post                                                     */
/* ------------------------------------------------------------------ */
static void draw_shop(const Game *g)
{
    const Player *p = &g->p;
    char buf[80];
    gfx_rect(0, 0, VW, VH, 0x0A0608D8u);
    fire_text("THE TRADING POST", VW * 0.5f, 12, 3.4f, g->t);
    if (g->shop_after) snprintf(buf, sizeof buf, "%s CLEARED - NEXT: %s", BIOME_NAME[g->biome], BIOME_NAME[g->biome + 1]);
    else if (g->wave + 1 >= NWAVES) snprintf(buf, sizeof buf, "WAVE %d SURVIVED - THE BOSS IS COMING", g->wave + 1);
    else snprintf(buf, sizeof buf, "WAVE %d SURVIVED - WAVE %d NEXT", g->wave + 1, g->wave + 2);
    gfx_text_sc(buf, VW * 0.5f, 46, 1.3f, g->wave + 1 >= NWAVES && !g->shop_after ? 0xFF6050FFu : DIM);

    /* cash and health */
    snprintf(buf, sizeof buf, "$%d", p->cash);
    gfx_poly(30, 30, 9, 10, 0, 0x7A5A10FFu); gfx_poly(30, 30, 7, 10, 0, GOLD);
    gfx_text_s(buf, 46, 21, 2.6f, g->shop_err > 0 ? 0xFF4040FFu : GOLD);
    bar(VW - 176, 24, 150, 10, p->hp / p->maxhp, BLOOD, 0x3A1014FFu);
    snprintf(buf, sizeof buf, "HP %d/%d", (int)ceilf(p->hp), (int)p->maxhp);
    gfx_text_sr(buf, VW - 24, 40, 1.1f, BONE);

    /* the four buttons */
    gfx_text_sc("YOUR GUNS", 70, 84, 1.2f, GOLD);
    for (int s = 0; s < NSLOTS; s++) {
        float x = 70, y = 128.0f + (float)s * 68.0f;
        int w = p->slot[s];
        int hl = g->pick_weapon >= 0;
        panel(x - 50, y - 28, 100, 58, PANEL, hl ? col_mix(0x4A4050FFu, SLOT_COL[s], 0.5f + 0.5f * sinf(g->t * 8)) : 0x4A4050FFu);
        gfx_poly(x - 50, y - 28, 10, 12, 0, 0x0A080AFFu); gfx_poly(x - 50, y - 28, 8.5f, 12, 0, SLOT_COL[s]);
        gfx_text_c(SLOT_BTN[s], x - 49.5f, y - 33, 1.3f, 0x101010FFu);
        if (w >= 0) {
            art_weapon_icon(w, x, y - 6, 1.6f, -0.3f, 0xFFFFFFFFu);
            pips(x, y + 18, 76, 4, MAX_WLVL, p->wlvl[w]);
        } else gfx_text_c("EMPTY", x, y - 4, 1.0f, 0x5A5060FFu);
    }

    /* items */
    int sel = g->shop_sel;
    for (int i = 0; i < NWEAP + NUPG + 1; i++) {
        float x, y;
        shop_item_pos(i, &x, &y);
        int is_sel = i == sel && g->pick_weapon < 0;
        if (i == NWEAP + NUPG) {
            const char *t = g->shop_after ? "RIDE ON >" : (g->wave + 1 >= NWAVES ? "FACE THE BOSS >" : "NEXT WAVE >");
            float w = gfx_text_width(t, 1.8f) + 36;
            panel(x - w * 0.5f, y - 18, w, 34, is_sel ? 0x5A1A10F0u : 0x2A1410F0u, is_sel ? GOLD : FIRE);
            gfx_text_sc(t, x, y - 7, 1.8f, is_sel ? GOLD : BONE);
            continue;
        }
        float cw = 90, ch = i < NWEAP ? 96 : 84;
        float pop = is_sel ? 1.06f : 1.0f;
        panel(x - cw * 0.5f * pop, y - ch * 0.5f * pop, cw * pop, ch * pop, is_sel ? PANEL2 : PANEL, is_sel ? GOLD : 0x3A3240FFu);
        if (is_sel) { gfx_blend(BL_ADD); gfx_glow(x, y, 70, 0xFFA03028u); gfx_blend(BL_ALPHA); }
        int cost, lvl, maxl;
        if (i < NWEAP) {
            int w = i;
            cost = weapon_cost(g, w); lvl = p->wlvl[w]; maxl = MAX_WLVL;
            art_weapon_icon(w, x, y - 14, 1.7f, -0.35f, lvl ? 0xFFFFFFFFu : 0x8A8A90FFu);
            static const char *const SHORT[NWEAP] = { "REVOLVERS", "DRAGON'S B.", "GATLING", "DYNAMITE", "TESLA", "SUN LANCE" };
            gfx_text_c(SHORT[w], x, y - 42, 0.9f, BONE);
            pips(x, y + 8, 76, 4, maxl, lvl);
            int eq = 0;
            for (int s = 0; s < NSLOTS; s++) if (p->slot[s] == w) { eq = 1; gfx_poly(x + 34, y - 24, 7, 10, 0, SLOT_COL[s]); gfx_text_c(SLOT_BTN[s], x + 34.5f, y - 28, 1.1f, 0x101010FFu); }
            if (lvl == 0) snprintf(buf, sizeof buf, "$%d", cost);
            else if (!eq) snprintf(buf, sizeof buf, "EQUIP");
            else if (cost < 0) snprintf(buf, sizeof buf, "MAX");
            else snprintf(buf, sizeof buf, "$%d", cost);
            gfx_text_c(lvl == 0 ? "NEW" : (eq ? "UPGRADE" : "OWNED"), x, y + 16, 0.9f, DIM);
        } else {
            int u = i - NWEAP;
            cost = upgrade_cost(g, u); lvl = p->up[u]; maxl = u == UP_HEAL ? 0 : MAX_ULVL;
            art_upgrade_icon(u, x, y - 10, 1.8f, g->t);
            gfx_text_c(UPG_NAME[u], x, y - 37, 0.9f, BONE);
            if (maxl) pips(x, y + 10, 76, 4, maxl, lvl);
            if (cost < 0) snprintf(buf, sizeof buf, "MAX"); else snprintf(buf, sizeof buf, "$%d", cost);
        }
        int afford = cost >= 0 && p->cash >= cost;
        gfx_text_c(buf, x, y + ch * 0.5f - 18, 1.5f, cost < 0 ? DIM : (afford ? GOLD : 0xB04040FFu));
    }
    /* what the selected thing is */
    if (sel < NWEAP + NUPG) {
        const char *name = sel < NWEAP ? WEAP_NAME[sel] : UPG_NAME[sel - NWEAP];
        const char *desc = sel < NWEAP ? WEAP_DESC[sel] : UPG_DESC[sel - NWEAP];
        gfx_text_s(name, 140, 412, 1.6f, BONE);
        gfx_text_s(desc, 140, 434, 1.1f, DIM);
        if (sel < NWEAP && p->wlvl[sel] > 0) {
            static const char *const NEXT[NWEAP][MAX_WLVL] = {
                { "", "LV2: MORE DAMAGE", "LV3: PIERCES 2", "LV4: FAN THE HAMMER", "LV5: PIERCES 3", "LV6: FASTER, HARDER", "LV7: PIERCES 4", "LV8: HELLFIRE" },
                { "", "LV2: MORE PELLETS", "LV3: LEAVES FIRE", "LV4: PELLETS PIERCE", "LV5: INFERNO", "LV6: HOTTER", "LV7: HOTTER STILL", "LV8: DRAGON" },
                { "", "LV2: FASTER", "LV3: FASTER", "LV4: ROUNDS PIERCE", "LV5: TWIN BARRELS", "LV6: FASTER", "LV7: HEAVIER ROUNDS", "LV8: STORM" },
                { "", "LV2: BIGGER BOOM", "LV3: CLUSTER BOMBS", "LV4: MEGA BLAST", "LV5: TWO BUNDLES", "LV6: BIGGER", "LV7: BIGGER STILL", "LV8: ARMAGEDDON" },
                { "", "LV2: ONE MORE JUMP", "LV3: ONE MORE JUMP", "LV4: ONE MORE JUMP", "LV5: STORM", "LV6: MORE JUMPS", "LV7: MORE JUMPS", "LV8: THUNDERGOD" },
                { "", "LV2: MORE DAMAGE", "LV3: TRIPLE BEAM", "LV4: WIDER", "LV5: FIVE BEAMS", "LV6: HOTTER", "LV7: WIDER", "LV8: SUPERNOVA" },
            };
            int l = p->wlvl[sel];
            if (l < MAX_WLVL) gfx_text_s(NEXT[sel][l], 140, 452, 1.0f, GOLD);
        }
    }
    gfx_text_s("A: BUY   +: CONTINUE", 140, VH - 14, 1.0f, 0x6A6470FFu);
    if (g->shop_flash > 0) { gfx_blend(BL_ADD); gfx_rect(0, 0, VW, VH, col_a(0xFFC03040u, g->shop_flash)); gfx_blend(BL_ALPHA); }

    /* choosing a button for a gun */
    if (g->pick_weapon >= 0) {
        gfx_rect(140, 150, 670, 150, 0x0A0608F0u);
        panel(140, 150, 670, 150, 0x1A1218F8u, GOLD);
        art_weapon_icon(g->pick_weapon, 220, 210, 3.0f, -0.3f, 0xFFFFFFFFu);
        snprintf(buf, sizeof buf, "%s", WEAP_NAME[g->pick_weapon]);
        gfx_text(buf, 300, 172, 2.0f, GOLD);
        gfx_text("WHICH BUTTON?  PRESS", 300, 204, 1.6f, BONE);
        for (int s = 0; s < NSLOTS; s++) {
            float bx = 330 + (float)s * 60, by = 254;
            gfx_poly(bx, by, 18, 14, 0, 0x0A080AFFu); gfx_poly(bx, by, 15, 14, 0, SLOT_COL[s]);
            gfx_text_c(SLOT_BTN[s], bx + 1, by - 8, 2.4f, 0x101010FFu);
        }
        int full = 1;
        for (int s = 0; s < NSLOTS; s++) if (p->slot[s] < 0) full = 0;
        gfx_text(full ? "(REPLACES THAT GUN - YOU KEEP IT)" : "+: CANCEL", 600, 250, 0.9f, DIM);
        if (full) gfx_text("+: CANCEL", 600, 266, 0.9f, DIM);
    }
}

/* ------------------------------------------------------------------ */
/* end screens                                                          */
/* ------------------------------------------------------------------ */
static void stats_block(const Game *g, float y)
{
    char buf[64], tb[16];
    fmt_time(tb, sizeof tb, g->total_time);
    snprintf(buf, sizeof buf, "KILLS ........... %d", g->total_kills); gfx_text_sc(buf, VW * 0.5f, y, 1.6f, BONE);
    snprintf(buf, sizeof buf, "TIME ............ %s", tb);            gfx_text_sc(buf, VW * 0.5f, y + 24, 1.6f, BONE);
    snprintf(buf, sizeof buf, "BEST COMBO ...... %d", g->best_combo);  gfx_text_sc(buf, VW * 0.5f, y + 48, 1.6f, BONE);
    snprintf(buf, sizeof buf, "BOUNTY EARNED ... $%d", g->p.earned);   gfx_text_sc(buf, VW * 0.5f, y + 72, 1.6f, GOLD);
}

static void draw_clear(const Game *g)
{
    char buf[64];
    gfx_rect(0, 0, VW, VH, 0x000000B0u);
    gfx_blend(BL_ADD);
    for (int i = 0; i < 8; i++) {
        float a = g->t * 0.3f + (float)i * TAU_F / 8.0f;
        gfx_tri(VW * 0.5f, 140, 0xFFC04030u, VW * 0.5f + cosf(a) * 600, 140 + sinf(a) * 600, 0xFFC04000u, VW * 0.5f + cosf(a + 0.2f) * 600, 140 + sinf(a + 0.2f) * 600, 0xFFC04000u);
    }
    gfx_blend(BL_ALPHA);
    snprintf(buf, sizeof buf, "%s CLEARED", BIOME_NAME[g->biome]);
    fire_text(buf, VW * 0.5f, 80, 4.0f, g->t);
    snprintf(buf, sizeof buf, "%s IS DEAD", BOSS_NAME[g->biome]);
    gfx_text_sc(buf, VW * 0.5f, 132, 1.6f, 0xFF7060FFu);
    stats_block(g, 190);
    if (g->biome + 1 < NBIOMES) {
        snprintf(buf, sizeof buf, "NEXT: %s", BIOME_NAME[g->biome + 1]);
        gfx_text_sc(buf, VW * 0.5f, 320, 2.0f, BONE);
        gfx_text_sc(BIOME_SUB[g->biome + 1], VW * 0.5f, 346, 1.2f, DIM);
    }
    if (g->scene_t > 1.2f) gfx_text_sc("A: CONTINUE", VW * 0.5f, VH - 50, 1.6f, col_a(GOLD, 0.6f + 0.4f * sinf(g->t * 5)));
}

static void draw_over(const Game *g)
{
    char buf[64];
    menu_bg(g, g->biome);
    gfx_rect(0, 0, VW, VH, 0x200000B0u);
    float sc = 1.0f + 0.2f * clampf(0.5f - g->scene_t, 0, 0.5f);
    gfx_text_sc("YOU DIED", VW * 0.5f + 3, 73, 7.0f * sc, 0x000000C0u);
    gfx_text_sc("YOU DIED", VW * 0.5f, 70, 7.0f * sc, BLOOD);
    snprintf(buf, sizeof buf, "%s - WAVE %d", BIOME_NAME[g->biome], g->wave + 1 > NWAVES ? NWAVES : g->wave + 1);
    gfx_text_sc(g->phase == PH_BOSS ? "THE BOSS TOOK YOU" : buf, VW * 0.5f, 140, 1.6f, DIM);
    stats_block(g, 180);
    snprintf(buf, sizeof buf, "+%d SOULS", g->souls_gained);
    gfx_text_sc(buf, VW * 0.5f, 300, 2.6f, 0x80E0FFFFu);
    snprintf(buf, sizeof buf, "TOTAL: %d - SPEND THEM IN THE ARMORY", g->sv.souls);
    gfx_text_sc(buf, VW * 0.5f, 334, 1.2f, DIM);
    if (g->scene_t > 1.5f) gfx_text_sc("A: BACK TO TOWN", VW * 0.5f, VH - 50, 1.6f, col_a(GOLD, 0.6f + 0.4f * sinf(g->t * 5)));
}

static void draw_win(const Game *g)
{
    char buf[64];
    menu_bg(g, 0);
    gfx_rect(0, 0, VW, VH, 0x00000080u);
    gfx_blend(BL_ADD);
    for (int i = 0; i < 12; i++) {
        uint32_t h = hash32((uint32_t)i * 977u + (uint32_t)(g->t * 0.7f) * 31u);
        float x = (float)(h & 1023) * VW / 1024.0f, y = 60 + (float)((h >> 10) & 255);
        float k = fmodf(g->t * 0.7f, 1.0f);
        gfx_ring(x, y, 10 + k * 80, 3, 20, 0, col_hsv((float)(h & 255) / 255.0f, 0.6f, 1.0f, (int)(200 * (1 - k))));
    }
    gfx_blend(BL_ALPHA);
    fire_text("THE STAND HOLDS", VW * 0.5f, 70, 5.0f, g->t);
    gfx_text_sc("THE MAW IS CLOSED. THE DEAD LIE STILL. FOR NOW.", VW * 0.5f, 130, 1.4f, BONE);
    stats_block(g, 180);
    snprintf(buf, sizeof buf, "+%d SOULS", g->souls_gained);
    gfx_text_sc(buf, VW * 0.5f, 300, 2.6f, 0x80E0FFFFu);
    if (g->scene_t > 1.5f) gfx_text_sc("A: BACK TO TOWN", VW * 0.5f, VH - 50, 1.6f, col_a(GOLD, 0.6f + 0.4f * sinf(g->t * 5)));
}

void hud_menus(const Game *g)
{
    gfx_view_reset();
    gfx_blend(BL_ALPHA);
    switch (g->scene) {
    case SC_TITLE: draw_title(g); break;
    case SC_HELP: draw_help(g); break;
    case SC_ARMORY: draw_armory(g); break;
    case SC_BIOMES: draw_biomes(g); break;
    case SC_INTRO: draw_intro(g); break;
    case SC_PAUSE: draw_pause(g); break;
    case SC_SHOP: draw_shop(g); break;
    case SC_CLEAR: draw_clear(g); break;
    case SC_OVER: draw_over(g); break;
    case SC_WIN: draw_win(g); break;
    default: break;
    }
}
