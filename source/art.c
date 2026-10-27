/*
 * LAST STAND - the cast, drawn from flat shapes with chunky dark outlines (in the spirit of a hand-made
 * vector look): twenty monsters, five bosses, four gunslingers, six guns, the loot and the shop icons.
 *
 * Every sprite is drawn about its ground point, facing right (mirrored by `face`); a white hit flash and a
 * gold elite outline are applied through the colour helpers.
 */
#include "game_int.h"
#include "gfx.h"

#include <string.h>

#define OUT  0x140E14FFu

static float FX = 1.0f;          /* facing for the sprite being drawn: 1 right, -1 left */
static float OX, OY;             /* its origin */
static float S = 1.0f;           /* its scale */
static float FLASH;              /* white hit flash 0..1 */
static uint32_t OUTC = OUT;      /* outline colour (gold for elites) */
static float ALPHA = 1.0f;

static uint32_t C(uint32_t c)
{
    if (FLASH > 0) c = col_mix(c, 0xFFFFFFFFu, FLASH);
    if (ALPHA < 1.0f) c = col_a(c, ALPHA);
    return c;
}
static uint32_t CO(void) { return ALPHA < 1.0f ? col_a(OUTC, ALPHA) : OUTC; }

/* unit circles */
static float UC[25][2];
static int uc_n;
static void uc(int n)
{
    if (uc_n == n) return;
    uc_n = n;
    for (int i = 0; i <= n; i++) { UC[i][0] = cosf((float)i / (float)n * TAU_F); UC[i][1] = sinf((float)i / (float)n * TAU_F); }
}

/* world position of a local point (local x mirrored by facing) */
#define PX(lx) (OX + (lx) * S * FX)
#define PY(ly) (OY + (ly) * S)

static void ell_raw(float cx, float cy, float rx, float ry, int n, uint32_t c)
{
    uc(n);
    for (int i = 0; i < n; i++)
        gfx_tri(cx, cy, c, cx + UC[i][0] * rx, cy + UC[i][1] * ry, c, cx + UC[i + 1][0] * rx, cy + UC[i + 1][1] * ry, c);
}

/* local-space ellipse, outlined */
static void E(float lx, float ly, float rx, float ry, int n, uint32_t c)
{
    float o = 1.7f;
    ell_raw(PX(lx), PY(ly), rx * S + o, ry * S + o, n, CO());
    ell_raw(PX(lx), PY(ly), rx * S, ry * S, n, C(c));
}
/* local-space ellipse, no outline */
static void Ef(float lx, float ly, float rx, float ry, int n, uint32_t c) { ell_raw(PX(lx), PY(ly), rx * S, ry * S, n, C(c)); }

/* local-space polygon (convex, fan from the first point), with or without outline */
static void P_raw(const float *pts, int n, uint32_t c, float grow)
{
    float cx = 0, cy = 0;
    for (int i = 0; i < n; i++) { cx += pts[i * 2]; cy += pts[i * 2 + 1]; }
    cx /= (float)n; cy /= (float)n;
    for (int i = 0; i < n; i++) {
        int j = (i + 1) % n;
        float ax = pts[i * 2], ay = pts[i * 2 + 1], bx = pts[j * 2], by = pts[j * 2 + 1];
        if (grow > 0) {
            float dax = ax - cx, day = ay - cy, la = sqrtf(dax * dax + day * day) + 1e-4f;
            float dbx = bx - cx, dby = by - cy, lb = sqrtf(dbx * dbx + dby * dby) + 1e-4f;
            ax += dax / la * grow / S; ay += day / la * grow / S; bx += dbx / lb * grow / S; by += dby / lb * grow / S;
        }
        gfx_tri(PX(cx), PY(cy), c, PX(ax), PY(ay), c, PX(bx), PY(by), c);
    }
}
static void P(const float *pts, int n, uint32_t c) { P_raw(pts, n, CO(), 1.9f); P_raw(pts, n, C(c), 0); }
static void Pf(const float *pts, int n, uint32_t c) { P_raw(pts, n, C(c), 0); }

/* local-space rectangle */
static void R(float x0, float y0, float x1, float y1, uint32_t c)
{
    float p[8] = { x0, y0, x1, y0, x1, y1, x0, y1 };
    P(p, 4, c);
}
static void Rf(float x0, float y0, float x1, float y1, uint32_t c)
{
    float p[8] = { x0, y0, x1, y0, x1, y1, x0, y1 };
    Pf(p, 4, c);
}
/* local-space thick line */
static void L(float x0, float y0, float x1, float y1, float w, uint32_t c)
{
    gfx_line(PX(x0), PY(y0), PX(x1), PY(y1), w * S + 3.0f, CO(), CO());
    gfx_line(PX(x0), PY(y0), PX(x1), PY(y1), w * S, C(c), C(c));
}
static void Lf(float x0, float y0, float x1, float y1, float w, uint32_t c) { gfx_line(PX(x0), PY(y0), PX(x1), PY(y1), w * S, C(c), C(c)); }

/* glowing eyes: solid here; the additive pass adds the bloom */
static void eyes(float lx, float ly, float sep, float r, uint32_t c)
{
    Ef(lx - sep * 0.5f, ly, r, r, 6, c);
    Ef(lx + sep * 0.5f, ly, r, r, 6, c);
}

static void begin(float x, float y, float s, float face, float flash, int elite)
{
    OX = x; OY = y; S = s; FX = face >= 0 ? 1.0f : -1.0f; FLASH = flash; OUTC = elite ? 0xFFC830FFu : OUT; ALPHA = 1.0f;
}

void art_shadow(float x, float y, float rx, float ry, float a)
{
    uc(10);
    uint32_t c = RGBA(0, 0, 0, (int)(110 * a)), e = RGBA(0, 0, 0, 0);
    for (int i = 0; i < 10; i++)
        gfx_tri(x, y, c, x + UC[i][0] * rx, y + UC[i][1] * ry, e, x + UC[i + 1][0] * rx, y + UC[i + 1][1] * ry, e);
}

/* ================================================================== */
/* monsters                                                             */
/* ================================================================== */
static void skull(float lx, float ly, float r, uint32_t bone, uint32_t eye)
{
    E(lx, ly, r, r * 0.95f, 10, bone);
    Rf(lx - r * 0.55f, ly + r * 0.45f, lx + r * 0.55f, ly + r * 1.05f, bone);
    Ef(lx - r * 0.38f, ly + 0.05f * r, r * 0.3f, r * 0.34f, 6, 0x1A1018FFu);
    Ef(lx + r * 0.38f, ly + 0.05f * r, r * 0.3f, r * 0.34f, 6, 0x1A1018FFu);
    eyes(lx, ly + 0.08f * r, r * 0.76f, r * 0.12f, eye);
    for (int k = -1; k <= 1; k++) Lf(lx + (float)k * r * 0.25f, ly + r * 0.6f, lx + (float)k * r * 0.25f, ly + r * 1.0f, 0.8f, 0x2A2020FFu);
}

static void a_bandit(float t, uint32_t eye)
{
    float b = -fabsf(sinf(t * 9.0f)) * 1.5f;
    L(-4, 5 + b, -5, 11, 2.2f, 0xD8CCB0FFu); L(4, 5 + b, 5, 11, 2.2f, 0xD8CCB0FFu);
    R(-6, -3 + b, 6, 6 + b, 0xD8CCB0FFu);                              /* ribs */
    for (int k = 0; k < 3; k++) Lf(-5, -1 + (float)k * 2.5f + b, 5, -1 + (float)k * 2.5f + b, 0.8f, 0x3A2A2AFFu);
    Rf(-6, -4 + b, 6, -1 + b, 0xB03028FFu);                            /* bandana */
    L(6, -1 + b, 11, 3 + b + sinf(t * 9) * 2, 2.0f, 0xD8CCB0FFu);      /* arm */
    skull(0, -10 + b, 5.5f, 0xEDE4D0FFu, eye);
    E(0, -15 + b, 9.5f, 2.6f, 10, 0x5A3A22FFu);                       /* hat brim */
    R(-4.5f, -21 + b, 4.5f, -15 + b, 0x6A4628FFu);
}

static void a_buzzard(float t, uint32_t eye)
{
    float f = sinf(t * 14.0f);
    float w = 10.0f + f * 4.0f;
    float lw[6] = { -2, -2, -14, -6 - f * 6, -10, 2 };
    float rw[6] = { 2, -2, 14, -6 - f * 6, 10, 2 };
    P(lw, 3, 0x4A3020FFu); P(rw, 3, 0x4A3020FFu);
    (void)w;
    E(0, 0, 6, 7, 8, 0x5A3A28FFu);
    E(1, -7, 3.2f, 3.2f, 8, 0xD89A90FFu);
    float bk[6] = { 3, -8, 8, -6, 3, -5 };
    P(bk, 3, 0xF0C040FFu);
    Ef(2, -8, 0.9f, 0.9f, 6, eye);
}

static void a_cactus(float t, uint32_t eye)
{
    float sw = sinf(t * 4.0f) * 1.0f;
    R(-7, -14 + sw, 7, 10, 0x4E8A3AFFu);
    E(0, -14 + sw, 7, 5, 10, 0x4E8A3AFFu);
    Rf(-7, -14 + sw, 7, 10, 0x4E8A3AFFu);
    R(-15, -8, -9, 0, 0x447A34FFu); R(-15, -14, -11, -6, 0x447A34FFu);     /* arms */
    R(9, -6, 15, 2, 0x447A34FFu); R(11, -12, 15, -4, 0x447A34FFu);
    for (int k = 0; k < 3; k++) Lf(-3 + (float)k * 3, -16 + sw, -3 + (float)k * 3, 9, 0.8f, 0x2E5A26FFu);
    for (int k = 0; k < 6; k++) Ef(-5 + (float)(k % 3) * 5, -6 + (float)(k / 3) * 8, 0.7f, 0.7f, 4, 0xF0E8C0FFu);
    Ef(-3, -9 + sw, 2.2f, 1.6f, 6, 0x1A1418FFu); Ef(3, -9 + sw, 2.2f, 1.6f, 6, 0x1A1418FFu);
    eyes(0, -9 + sw, 6, 1.0f, eye);
    Lf(-3, -4 + sw, 3, -4 + sw, 1.2f, 0x1A1418FFu);
    E(0, -20 + sw, 3, 2.4f, 8, 0xE860A0FFu);                               /* flower */
}

static void a_slinger(float t, uint32_t eye, float cd)
{
    float b = -fabsf(sinf(t * 8.0f)) * 1.2f;
    L(-3, 5 + b, -4, 11, 2.0f, 0xD8CCB0FFu); L(3, 5 + b, 4, 11, 2.0f, 0xD8CCB0FFu);
    float pon[8] = { -9, 6 + b, 9, 6 + b, 5, -4 + b, -5, -4 + b };
    P(pon, 4, 0xD86A2AFFu);
    Lf(-8, 3 + b, 8, 3 + b, 1.2f, 0xF0D060FFu);
    Lf(-6, 0 + b, 6, 0 + b, 1.2f, 0x8A2A1AFFu);
    float raise = cd < 0.5f ? -6.0f : 0.0f;
    L(6, -1 + b, 10, -4 + b + raise, 2.0f, 0xD8CCB0FFu);
    E(11, -6 + b + raise, 2.5f, 1.4f, 6, 0xEDE4D0FFu);                    /* the bone to throw */
    skull(0, -10 + b, 5.2f, 0xEDE4D0FFu, eye);
    E(0, -16 + b, 10, 2.4f, 10, 0x7A5A2AFFu);                              /* sombrero */
    E(0, -18 + b, 4.5f, 3.4f, 8, 0x8A6A32FFu);
}

static void a_toad(float t, uint32_t eye, float hop)
{
    float h = hop;                                                    /* 0..1 off the ground */
    float sq = 1.0f - h * 0.15f;
    OY -= h * 10.0f * S;
    float body[10] = { -11, 8, 11, 8, 12, -4, 0, -10 * sq, -12, -4 };
    P(body, 5, 0x4A7A3AFFu);
    Rf(-10, -2, 10, 4, 0x5A8A46FFu);
    for (int k = 0; k < 5; k++) { float tx = -8 + (float)k * 4; float tp[6] = { tx, 8, tx + 2, 3, tx + 4, 8 }; Pf(tp, 3, 0xE8E0C0FFu); }   /* jagged teeth */
    Lf(-10, 3, 10, 3, 1.5f, 0x1A1418FFu);
    E(-5, -6, 3.2f, 3.0f, 8, 0xF0D040FFu); E(5, -6, 3.2f, 3.0f, 8, 0xF0D040FFu);
    Ef(-5, -6, 1.2f, 2.2f, 6, 0x1A1418FFu); Ef(5, -6, 1.2f, 2.2f, 6, 0x1A1418FFu);
    eyes(0, -6, 10, 0.6f, eye);
    OY += h * 10.0f * S;
}

static void a_crow(float t, uint32_t eye)
{
    float f = sinf(t * 16.0f);
    float lw[6] = { -2, -1, -15, -4 - f * 7, -9, 3 };
    float rw[6] = { 2, -1, 15, -4 - f * 7, 9, 3 };
    P(lw, 3, 0x1E2A24FFu); P(rw, 3, 0x1E2A24FFu);
    E(0, 0, 5.5f, 7, 8, 0x22302AFFu);
    E(1, -7, 3.5f, 3.2f, 8, 0x22302AFFu);
    float bk[6] = { 3, -8, 9, -6.5f, 3, -5.5f };
    P(bk, 3, 0x9AA040FFu);
    Ef(2, -8, 1.0f, 1.0f, 6, eye);
}

static void a_moss(float t, uint32_t eye)
{
    float b = sinf(t * 3.0f) * 1.0f;
    float body[12] = { -14, 10, 14, 10, 16, -4, 9, -15 + b, -9, -15 + b, -16, -4 };
    P(body, 6, 0x5A6A48FFu);
    Ef(-6, -9 + b, 6, 4, 8, 0x4A7A3AFFu); Ef(7, -10 + b, 5, 3, 8, 0x4A7A3AFFu); Ef(0, 4, 9, 3, 8, 0x3E6A30FFu);
    L(-14, -2, -19, 8, 4.5f, 0x5A6A48FFu); L(14, -2, 19, 8, 4.5f, 0x5A6A48FFu);
    Lf(-8, -2, -2, 4, 1.2f, 0x8CFF60FFu); Lf(4, -6, 9, 2, 1.2f, 0x8CFF60FFu);   /* glowing cracks */
    eyes(0, -6 + b, 9, 1.4f, eye);
}

static void a_lantern(float t, uint32_t eye)
{
    float b = sinf(t * 5.0f) * 1.2f;
    float robe[8] = { -9, 10, 9, 10, 5, -6 + b, -5, -6 + b };
    P(robe, 4, 0x3A2A5AFFu);
    Lf(0, -4 + b, 0, 9, 1.0f, 0x2A1E44FFu);
    E(0, -9 + b, 5, 5, 8, 0x2A1E3AFFu);
    float beak[6] = { 2, -10 + b, 11, -6 + b, 2, -6 + b };
    P(beak, 3, 0xEDE6D8FFu);
    eyes(0, -10 + b, 4, 1.1f, eye);
    E(0, -15 + b, 8, 2.0f, 10, 0x141018FFu);
    R(-4, -22 + b, 4, -15 + b, 0x141018FFu);
    L(7, -2 + b, 11, 4, 1.2f, 0x2A1E3AFFu);
    E(11, 7, 2.8f, 3.4f, 8, 0x40FF80FFu);                                   /* the lantern */
}

static void a_zombie(float t, uint32_t eye)
{
    float b = -fabsf(sinf(t * 6.0f)) * 1.5f, sw = sinf(t * 6.0f) * 2.0f;
    L(-3, 5 + b, -4 - sw * 0.5f, 11, 2.6f, 0x3A3A4AFFu); L(3, 5 + b, 4 + sw * 0.5f, 11, 2.6f, 0x3A3A4AFFu);
    R(-6, -4 + b, 6, 6 + b, 0x3A4A6AFFu);
    Lf(-6, 1 + b, -2, 5 + b, 1.0f, 0x7A9A6AFFu);
    L(5, -2 + b, 13, -3 + b + sw, 2.4f, 0x7A9A6AFFu); L(3, 0 + b, 12, 1 + b - sw, 2.4f, 0x7A9A6AFFu);
    E(0, -9 + b, 5.5f, 5.2f, 10, 0x7A9A6AFFu);
    Ef(-2, -12 + b, 3, 1.5f, 6, 0x5A7A4AFFu);
    eyes(1, -9 + b, 4, 1.1f, eye);
    Lf(-2, -6 + b, 3, -5 + b, 1.0f, 0x2A2020FFu);
}

static void a_bones(float t, uint32_t eye)
{
    float b = -fabsf(sinf(t * 12.0f)) * 1.5f, sw = sinf(t * 12.0f) * 2.5f;
    Lf(-2, 3 + b, -3 - sw, 9, 1.5f, 0xE8E4D8FFu); Lf(2, 3 + b, 3 + sw, 9, 1.5f, 0xE8E4D8FFu);
    R(-4, -3 + b, 4, 4 + b, 0xE8E4D8FFu);
    Lf(-3, -1 + b, 3, -1 + b, 0.7f, 0x3A3030FFu); Lf(-3, 1.5f + b, 3, 1.5f + b, 0.7f, 0x3A3030FFu);
    L(4, -2 + b, 9, 0 + b + sw, 1.4f, 0xE8E4D8FFu);
    skull(0, -8 + b, 4.5f, 0xEDE8DCFFu, eye);
}

static void a_coffin(float t, uint32_t eye)
{
    float b = -fabsf(sinf(t * 5.0f)) * 1.0f;
    float lid[12] = { -8, -18 + b, 8, -18 + b, 12, -8 + b, 9, 12, -9, 12, -12, -8 + b };
    P(lid, 6, 0x3A3A44FFu);
    float in[12] = { -6, -15 + b, 6, -15 + b, 9, -8 + b, 7, 9, -7, 9, -9, -8 + b };
    Pf(in, 6, 0x4A4A58FFu);
    Lf(0, -12 + b, 0, 4, 2.0f, 0x8A8A98FFu); Lf(-5, -6 + b, 5, -6 + b, 2.0f, 0x8A8A98FFu);   /* the cross */
    Ef(-3, -11 + b, 1.4f, 1.0f, 6, 0x0A0A0AFFu); Ef(3, -11 + b, 1.4f, 1.0f, 6, 0x0A0A0AFFu);
    eyes(0, -11 + b, 6, 1.0f, eye);
    E(10, 8, 2.5f, 2.0f, 6, 0x7A9A6AFFu); E(-10, 8, 2.5f, 2.0f, 6, 0x7A9A6AFFu);           /* hands */
}

static void a_wraith(float t, uint32_t eye, float phase)
{
    ALPHA = phase;
    float b = sinf(t * 4.0f) * 2.5f;
    float tail[8] = { -7, -2 + b, 7, -2 + b, 4 + sinf(t * 7) * 3, 13, -3 + sinf(t * 7 + 1) * 3, 13 };
    P(tail, 4, 0xE8F0F4FFu);
    E(0, -6 + b, 7, 7, 10, 0xE8F0F4FFu);
    Ef(-3, -7 + b, 2, 2.6f, 6, 0x10181CFFu); Ef(3, -7 + b, 2, 2.6f, 6, 0x10181CFFu);
    eyes(0, -7 + b, 6, 1.0f, eye);
    Ef(0, -2 + b, 1.6f, 2.2f, 6, 0x10181CFFu);
    L(6, -3 + b, 11, 1 + b, 1.6f, 0xE8F0F4FFu);
    ALPHA = 1.0f;
}

static void a_husk(float t, uint32_t eye)
{
    float b = -fabsf(sinf(t * 7.0f)) * 1.5f, sw = sinf(t * 7.0f) * 2.0f;
    L(-3, 5 + b, -4 - sw * 0.4f, 11, 2.8f, 0x2A2220FFu); L(3, 5 + b, 4 + sw * 0.4f, 11, 2.8f, 0x2A2220FFu);
    R(-6, -4 + b, 6, 6 + b, 0x2A2220FFu);
    Lf(-4, -2 + b, 2, 4 + b, 1.0f, 0xFF7020FFu); Lf(3, -3 + b, 1, 3 + b, 0.9f, 0xFF9020FFu);
    L(5, -2 + b, 12, 2 + b + sw, 2.6f, 0x2A2220FFu);
    E(0, -9 + b, 5.5f, 5.2f, 10, 0x2A2220FFu);
    Lf(-3, -12 + b, 1, -8 + b, 0.9f, 0xFF7020FFu);
    eyes(1, -9 + b, 4, 1.2f, eye);
}

static void a_imp(float t, uint32_t eye)
{
    float b = -fabsf(sinf(t * 14.0f)) * 2.0f;
    float fl = sinf(t * 20.0f) * 1.5f;
    float flame[6] = { -4, -8 + b, 0 + fl, -18 + b, 4, -8 + b };
    Pf(flame, 3, 0xFFB030FFu);
    E(0, 0 + b, 6, 6.5f, 10, 0xD83020FFu);
    float hl[6] = { -5, -4 + b, -7, -10 + b, -2, -5 + b }, hr[6] = { 5, -4 + b, 7, -10 + b, 2, -5 + b };
    P(hl, 3, 0x2A1010FFu); P(hr, 3, 0x2A1010FFu);
    eyes(1, -1 + b, 5, 1.2f, eye);
    Lf(-2, 3 + b, 4, 3 + b, 1.0f, 0x2A1010FFu);
    L(-3, 6 + b, -5, 10, 1.5f, 0xD83020FFu); L(3, 6 + b, 5, 10, 1.5f, 0xD83020FFu);
}

static void a_hulk(float t, uint32_t eye)
{
    float b = -fabsf(sinf(t * 4.0f)) * 1.5f, sw = sinf(t * 4.0f) * 2.0f;
    L(-6, 6 + b, -7, 13, 4.5f, 0x3A3432FFu); L(6, 6 + b, 7, 13, 4.5f, 0x3A3432FFu);
    float body[12] = { -15, 6 + b, 15, 6 + b, 17, -6 + b, 10, -14 + b, -10, -14 + b, -17, -6 + b };
    P(body, 6, 0x4A4442FFu);
    Lf(-10, -6 + b, -2, 2 + b, 1.4f, 0xFF5018FFu); Lf(6, -10 + b, 11, 0 + b, 1.4f, 0xFF5018FFu);
    L(-15, -4 + b, -19, 8 + b + sw, 5.0f, 0x4A4442FFu); L(15, -4 + b, 19, 8 + b - sw, 5.0f, 0x4A4442FFu);
    E(-19, 10 + b + sw, 4.5f, 4.5f, 8, 0xFF6020FFu); E(19, 10 + b - sw, 4.5f, 4.5f, 8, 0xFF6020FFu);   /* molten fists */
    E(0, -16 + b, 6, 5, 8, 0x3A3432FFu);
    eyes(1, -16 + b, 6, 1.4f, eye);
}

static void a_doctor(float t, uint32_t eye)
{
    float b = sinf(t * 5.0f) * 1.0f;
    float coat[8] = { -10, 11, 10, 11, 6, -6 + b, -6, -6 + b };
    P(coat, 4, 0x1E1A20FFu);
    Lf(0, -4 + b, 0, 10, 0.8f, 0x3A3440FFu);
    E(0, -9 + b, 5, 5, 8, 0x2A242AFFu);
    float beak[6] = { 2, -11 + b, 14, -4 + b, 2, -6 + b };
    P(beak, 3, 0xE8E0D0FFu);
    E(-1, -11 + b, 2.0f, 2.0f, 8, 0x60A040FFu); E(3, -11 + b, 2.0f, 2.0f, 8, 0x60A040FFu);   /* goggle lenses */
    eyes(1, -11 + b, 4, 0.9f, eye);
    E(0, -15 + b, 8.5f, 2.0f, 10, 0x141018FFu);
    R(-4.5f, -24 + b, 4.5f, -15 + b, 0x141018FFu);
    L(6, -1 + b, 11, 2 + b, 1.4f, 0x1E1A20FFu);
    E(12, 3 + b, 2.0f, 3.0f, 8, 0x70FF40FFu);                                   /* the vial */
}

static void a_crawler(float t, uint32_t eye)
{
    float b = sinf(t * 10.0f) * 1.5f;
    for (int k = 0; k < 4; k++) {
        float a = (float)k * 0.5f - 0.75f, sw = sinf(t * 12.0f + (float)k) * 2.0f;
        L(-4, 0, -12 - sw, 6 + a * 8, 1.6f, 0x140E18FFu);
        L(4, 0, 12 + sw, 6 + a * 8, 1.6f, 0x140E18FFu);
    }
    E(0, -2 + b, 10, 7, 10, 0x140E18FFu);
    Ef(-2, -5 + b, 6, 2.5f, 8, 0x2A1A30FFu);
    static const float EY[7][2] = { {-5, -3}, {-1, -4}, {3, -4}, {6, -2}, {-3, 0}, {2, -1}, {0, 1.5f} };
    for (int k = 0; k < 7; k++) Ef(EY[k][0], EY[k][1] + b, 1.1f, 1.1f, 6, eye);
}

static void a_eye(float t, uint32_t eye)
{
    float b = sinf(t * 6.0f) * 3.0f;
    float fl[8] = { -5, 0 + b, 5, 0 + b, 2 + sinf(t * 9) * 2, 12 + b, -3 + sinf(t * 11) * 2, 13 + b };
    Pf(fl, 4, 0x9A30FFFFu);
    E(0, -2 + b, 7, 7, 12, 0xF0E8E8FFu);
    Lf(-5, -5 + b, -2, -3 + b, 0.6f, 0xD03040FFu); Lf(4, 1 + b, 6, -1 + b, 0.6f, 0xD03040FFu);
    Ef(2, -2 + b, 3.4f, 3.4f, 10, 0xD02030FFu);
    Ef(2.5f, -2 + b, 1.6f, 1.8f, 8, 0x0A0408FFu);
    Ef(3.4f, -3 + b, 0.7f, 0.7f, 6, 0xFFFFFFFFu);
}

static void a_flesh(float t, uint32_t eye)
{
    float p = sinf(t * 3.5f) * 1.5f;
    float body[14] = { -16 - p, 6, -10, 12, 10, 12, 16 + p, 6, 15, -8, 0, -16 - p, -15, -8 };
    P(body, 7, 0xB04A5AFFu);
    Ef(-6, -8, 4, 3, 8, 0xC86A78FFu); Ef(8, -3, 3, 2.5f, 8, 0xC86A78FFu);
    E(0, 3, 9, 5.5f, 12, 0x3A0A14FFu);                                          /* the mouth */
    for (int k = 0; k < 6; k++) { float x = -7 + (float)k * 2.8f; float tt[6] = { x, -1.5f, x + 1.4f, 3, x + 2.8f, -1.5f }; Pf(tt, 3, 0xF0E8D0FFu); }
    for (int k = 0; k < 6; k++) { float x = -7 + (float)k * 2.8f; float tt[6] = { x, 7.5f, x + 1.4f, 3, x + 2.8f, 7.5f }; Pf(tt, 3, 0xF0E8D0FFu); }
    eyes(-6, -9, 4, 1.2f, eye);
    Ef(9, -10, 1.6f, 1.6f, 6, eye);
}

static void a_priest(float t, uint32_t eye)
{
    float b = sinf(t * 3.0f) * 2.0f;
    float robe[8] = { -10, 12, 10, 12, 6, -10 + b, -6, -10 + b };
    P(robe, 4, 0x2A1240FFu);
    Lf(-6, 8, 6, 8, 1.0f, 0xB040FFFFu);
    float hood[8] = { -7, -6 + b, 7, -6 + b, 4, -20 + b, -4, -20 + b };
    P(hood, 4, 0x2A1240FFu);
    Ef(0, -11 + b, 4, 5, 8, 0x05020AFFu);
    eyes(0, -12 + b, 3.6f, 1.0f, eye);
    E(-11, -2 + b, 2.6f, 2.6f, 8, 0xB040FFFFu); E(11, -2 + b, 2.6f, 2.6f, 8, 0xB040FFFFu);   /* floating orbs */
}

/* ================================================================== */
/* bosses                                                               */
/* ================================================================== */
static void b_sheriff(float t, uint32_t eye, int state)
{
    float b = -fabsf(sinf(t * 5.0f)) * 2.0f;
    L(-7, 8 + b, -10, 22, 4, 0xE0D4B8FFu); L(7, 8 + b, 10, 22, 4, 0xE0D4B8FFu);
    float coat[8] = { -16, 12 + b, 16, 12 + b, 12, -12 + b, -12, -12 + b };
    P(coat, 4, 0x7A4A2AFFu);
    Rf(-5, -12 + b, 5, 12 + b, 0xE0D4B8FFu);
    for (int k = 0; k < 4; k++) Lf(-4, -8 + (float)k * 4 + b, 4, -8 + (float)k * 4 + b, 1.0f, 0x3A2A2AFFu);
    float star[10];
    for (int k = 0; k < 5; k++) { float a = (float)k / 5.0f * TAU_F - PI_F / 2; star[k * 2] = -9 + cosf(a) * 3.5f; star[k * 2 + 1] = -5 + b + sinf(a) * 3.5f; }
    P(star, 5, 0xFFD040FFu);
    float aim = state ? -6.0f : 0.0f;
    L(14, -8 + b, 24, -6 + b + aim, 3, 0xE0D4B8FFu); L(-14, -8 + b, -24, -6 + b + aim, 3, 0xE0D4B8FFu);
    R(22, -9 + b + aim, 34, -5 + b + aim, 0x3A3A44FFu); R(-34, -9 + b + aim, -22, -5 + b + aim, 0x3A3A44FFu);
    skull(0, -20 + b, 10, 0xEDE2C8FFu, eye);
    E(0, -29 + b, 22, 5, 14, 0x5A3A22FFu);
    R(-10, -44 + b, 10, -29 + b, 0x6A4628FFu);
    Rf(-10, -33 + b, 10, -30 + b, 0xB03028FFu);
}

static void b_bogmother(float t, uint32_t eye, int state)
{
    float br = sinf(t * 2.5f) * 2.0f;
    float h = state == 6 ? 8.0f : 0.0f;
    OY -= h * S;
    float body[12] = { -30, 16, 30, 16, 34, -4, 18, -24 - br, -18, -24 - br, -34, -4 };
    P(body, 6, 0x4E7E3CFFu);
    E(0, 6, 22, 10 + br * 0.5f, 14, 0xD8C080FFu);                              /* belly */
    Lf(-24, 0, 24, 0, 2.5f, 0x1A1418FFu);
    for (int k = 0; k < 8; k++) { float x = -20 + (float)k * 5.5f; float tt[6] = { x, 0, x + 2.7f, 6, x + 5.5f, 0 }; Pf(tt, 3, 0xF0E8C8FFu); }
    E(-14, -20, 8, 7, 10, 0xF0D040FFu); E(14, -20, 8, 7, 10, 0xF0D040FFu);
    Ef(-14, -20, 2.5f, 5, 8, 0x1A1418FFu); Ef(14, -20, 2.5f, 5, 8, 0x1A1418FFu);
    eyes(0, -20, 28, 1.2f, eye);
    for (int k = 0; k < 5; k++) L(-12 + (float)k * 6, -28 - br, -14 + (float)k * 7, -40 - br - (float)(k % 2) * 4, 1.8f, 0x8A9A40FFu);   /* the crown of reeds */
    E(-34, 14, 7, 4, 8, 0x4E7E3CFFu); E(34, 14, 7, 4, 8, 0x4E7E3CFFu);
    OY += h * S;
}

static void b_undertaker(float t, uint32_t eye, int state)
{
    float b = -fabsf(sinf(t * 4.0f)) * 1.5f;
    L(-5, 10 + b, -6, 24, 4, 0x1A181EFFu); L(5, 10 + b, 6, 24, 4, 0x1A181EFFu);
    float coat[8] = { -14, 14 + b, 14, 14 + b, 10, -16 + b, -10, -16 + b };
    P(coat, 4, 0x24222AFFu);
    Rf(-3, -16 + b, 3, 6 + b, 0xD8D4CCFFu);
    Lf(0, -14 + b, 0, -6 + b, 2.0f, 0x6A1A1AFFu);
    /* the coffin over his shoulder */
    float rot = state == 6 ? -0.6f : 0.0f;
    float cx = 18, cy = -12 + b;
    float cof[12] = { 0, -24, 8, -16, 6, 16, -6, 16, -8, -16, 0, -24 };
    float pts[12];
    for (int k = 0; k < 6; k++) { float x = cof[k * 2], y = cof[k * 2 + 1]; pts[k * 2] = cx + x * cosf(rot) - y * sinf(rot); pts[k * 2 + 1] = cy + x * sinf(rot) + y * cosf(rot); }
    P(pts + 2, 5, 0x5A3A2AFFu);
    Lf(cx, cy - 12, cx, cy + 6, 1.5f, 0xB0A080FFu); Lf(cx - 4, cy - 6, cx + 4, cy - 6, 1.5f, 0xB0A080FFu);
    E(0, -24 + b, 8, 9, 12, 0xD8D4CCFFu);
    Ef(-3, -24 + b, 2.6f, 3, 8, 0x0A0A0AFFu); Ef(3, -24 + b, 2.6f, 3, 8, 0x0A0A0AFFu);
    eyes(0, -24 + b, 6, 1.2f, eye);
    Lf(-4, -18 + b, 4, -18 + b, 1.0f, 0x2A2020FFu);
    E(0, -32 + b, 14, 3, 12, 0x101014FFu);
    R(-8, -52 + b, 8, -32 + b, 0x101014FFu);
    Rf(-8, -36 + b, 8, -33 + b, 0x5A1A1AFFu);
}

static void b_prophet(float t, uint32_t eye, int state)
{
    float b = sinf(t * 2.0f) * 3.0f;
    /* the halo of lanterns */
    for (int k = 0; k < 6; k++) {
        float a = t * 0.8f + (float)k / 6.0f * TAU_F;
        E(cosf(a) * 34, -28 + b + sinf(a) * 12, 3, 4, 8, 0x70FF40FFu);
    }
    float robe[8] = { -26, 18, 26, 18, 14, -18 + b, -14, -18 + b };
    P(robe, 4, 0x2A2030FFu);
    Lf(-20, 12, 20, 12, 2.0f, 0x6A40A0FFu);
    Lf(0, -14 + b, 0, 16, 1.5f, 0x3A3044FFu);
    E(0, -26 + b, 11, 11, 12, 0x241E2AFFu);
    float beak[6] = { 4, -30 + b, 34, -14 + b, 4, -18 + b };
    P(beak, 3, 0xEDE4D2FFu);
    E(-2, -30 + b, 4, 4, 10, 0x60A040FFu); E(7, -30 + b, 4, 4, 10, 0x60A040FFu);
    eyes(2.5f, -30 + b, 9, 1.6f, eye);
    E(0, -38 + b, 18, 4, 14, 0x141018FFu);
    R(-9, -58 + b, 9, -38 + b, 0x141018FFu);
    L(-22, -6 + b, -30, 12 + b, 3, 0x2A2030FFu);
    E(-30, 16 + b, 5, 6, 10, 0x90FF60FFu);                                        /* the censer */
    (void)state;
}

static void b_maw(float t, uint32_t eye, int state, float laser)
{
    /* tentacles */
    for (int k = 0; k < 8; k++) {
        float a = (float)k / 8.0f * TAU_F + 0.2f;
        float px = 0, py = 0;
        for (int s = 1; s <= 4; s++) {
            float w = sinf(t * 2.5f + (float)k + (float)s * 0.8f) * 0.35f * (float)s;
            float nx = cosf(a + w) * (22.0f + (float)s * 10.0f), ny = sinf(a + w) * (16.0f + (float)s * 8.0f);
            L(px, py, nx, ny, 7.0f - (float)s * 1.4f, 0x5A1424FFu);
            px = nx; py = ny;
        }
    }
    E(0, 0, 32, 28, 16, 0x7A1A2AFFu);
    for (int k = 0; k < 12; k++) {                                                 /* the ring of teeth */
        float a = (float)k / 12.0f * TAU_F;
        float tt[6] = { cosf(a) * 28, sinf(a) * 24, cosf(a + 0.25f) * 19, sinf(a + 0.25f) * 16, cosf(a + 0.5f) * 28, sinf(a + 0.5f) * 24 };
        Pf(tt, 3, 0xF0E8E0FFu);
    }
    E(0, 0, 18, 18, 14, 0xF0E8E0FFu);
    float look = state ? 1.0f : 0.0f; (void)look;
    Ef(4, 0, 10, 10, 14, laser > 0 ? 0xFF2050FFu : 0xD02030FFu);
    Ef(5, 0, 4.5f, 8, 10, 0x05020AFFu);
    Ef(7, -4, 2, 2, 6, 0xFFFFFFFFu);
    (void)eye;
}

/* ================================================================== */
static void enemy_body(const Enemy *e, float t, float laser)
{
    uint32_t eye = EDEF[e->type].eye;
    switch (e->type) {
    case A_BANDIT:  a_bandit(t, eye); break;
    case A_BUZZARD: a_buzzard(t, eye); break;
    case A_CACTUS:  a_cactus(t, eye); break;
    case A_SLINGER: a_slinger(t, eye, e->cd); break;
    case A_TOAD: {
        float cyc = fmodf(e->t + e->st, 1.1f);
        float h = cyc > 0.45f && cyc < 0.85f ? sinf((cyc - 0.45f) / 0.4f * PI_F) : 0.0f;
        a_toad(t, eye, h);
        break;
    }
    case A_CROW:    a_crow(t, eye); break;
    case A_MOSS:    a_moss(t, eye); break;
    case A_LANTERN: a_lantern(t, eye); break;
    case A_ZOMBIE:  a_zombie(t, eye); break;
    case A_BONES:   a_bones(t, eye); break;
    case A_COFFIN:  a_coffin(t, eye); break;
    case A_WRAITH:  a_wraith(t, eye, fmodf(e->t, 4.0f) > 3.0f ? 0.35f : 0.9f); break;
    case A_HUSK:    a_husk(t, eye); break;
    case A_IMP:     a_imp(t, eye); break;
    case A_HULK:    a_hulk(t, eye); break;
    case A_DOCTOR:  a_doctor(t, eye); break;
    case A_CRAWLER: a_crawler(t, eye); break;
    case A_EYE:     a_eye(t, eye); break;
    case A_FLESH:   a_flesh(t, eye); break;
    case A_PRIEST:  a_priest(t, eye); break;
    case A_BOSS1:   b_sheriff(e->t, eye, e->state); break;
    case A_BOSS2:   b_bogmother(e->t, eye, e->state); break;
    case A_BOSS3:   b_undertaker(e->t, eye, e->state); break;
    case A_BOSS4:   b_prophet(e->t, eye, e->state); break;
    case A_BOSS5:   b_maw(e->t, eye, e->state, laser); break;
    }
}

void art_enemy(const Game *g, const Enemy *e, float x, float y)
{
    const EnemyDef *d = &EDEF[e->type];
    float s = art_scale(e);
    float flash = e->flash > 0 ? (d->ai == AI_BOSS ? 0.4f : 0.85f) : 0.0f;
    if (e->state == 250) flash = 0.4f + 0.4f * sinf(g->t * 40.0f);
    begin(x, y, s, e->face, flash, e->elite);
    enemy_body(e, e->t + (float)(e - g->en) * 0.37f, g->laser_on);
    FLASH = 0; OUTC = OUT;
}

void art_enemy_preview(int type, float x, float y, float s, float t)
{
    Enemy e;
    memset(&e, 0, sizeof e);
    e.type = (uint8_t)type; e.r = EDEF[type].radius * s; e.face = 1; e.t = t; e.cd = 2;
    begin(x, y, s, 1, 0, 0);
    enemy_body(&e, t, 0);
}

void art_boss_portrait(int biome, float x, float y, float s, float t) { art_enemy_preview(A_BOSS1 + biome, x, y, s, t); }

/* ================================================================== */
/* the gunslingers                                                      */
/* ================================================================== */
void art_player(float x, float y, int ch, float face, float walk, float ax, float ay, float t, float hurt, float scale)
{
    begin(x, y, scale, face, hurt > 0 ? 0.6f : 0.0f, 0);
    float b = -fabsf(sinf(walk)) * 1.6f;
    float lg = sinf(walk) * 3.0f;
    static const uint32_t COAT[NCHARS] = { 0x2C2630FFu, 0x18161CFFu, 0x5A3A6AFFu, 0x3A5A3AFFu };
    static const uint32_t COAT2[NCHARS] = { 0x3E3640FFu, 0x2A2830FFu, 0x7A5A8AFFu, 0x4E7A4EFFu };
    uint32_t coat = COAT[ch], coat2 = COAT2[ch];
    /* boots */
    L(-3, 6, -3 - lg * 0.5f, 12, 3.0f, 0x2A1A14FFu);
    L(3, 6, 3 + lg * 0.5f, 12, 3.0f, 0x2A1A14FFu);
    /* the coat: a wide duster flaring out */
    float sw = sinf(t * 3.0f) * 1.0f + lg * 0.3f;
    float duster[10] = { -11 - sw, 9 + b, 11 + sw, 9 + b, 8, -6 + b, 0, -8 + b, -8, -6 + b };
    P(duster, 5, coat);
    Lf(0, -6 + b, 0, 8 + b, 1.2f, coat2);
    Rf(-7, 1 + b, 7, 3 + b, 0x5A3A22FFu);                         /* belt */
    Ef(0, 2 + b, 1.6f, 1.4f, 6, 0xE8C060FFu);
    /* head */
    switch (ch) {
    case CH_DRIFTER:
        E(0, -11 + b, 5.2f, 5.0f, 10, 0x2A2228FFu);
        Rf(-5, -10 + b, 5, -6 + b, 0xC02828FFu);                   /* bandana */
        eyes(1.0f, -12 + b, 4.4f, 1.2f, 0xFFD030FFu);
        E(0, -15.5f + b, 12, 3.2f, 12, 0x5A221EFFu);              /* the wide hat */
        { float cr[8] = { -6, -15.5f + b, 6, -15.5f + b, 4.5f, -22 + b, -4.5f, -22 + b }; P(cr, 4, 0x6A2A22FFu); }
        Rf(-5.5f, -18 + b, 5.5f, -16.5f + b, 0x2A1A14FFu);
        break;
    case CH_PREACHER:
        E(0, -11 + b, 5.2f, 5.0f, 10, 0xD8B898FFu);
        Ef(0, -7.5f + b, 4.2f, 3.0f, 8, 0x9A9AA0FFu);              /* beard */
        Rf(-2.5f, -4 + b, 2.5f, -2.5f + b, 0xF0F0F0FFu);           /* collar */
        eyes(1.0f, -12 + b, 4.4f, 1.0f, 0x60D0FFFFu);
        E(0, -15.5f + b, 11, 2.4f, 12, 0x141418FFu);
        R(-5.5f, -20 + b, 5.5f, -15.5f + b, 0x18181EFFu);
        break;
    case CH_GRANNY:
        E(0, -11 + b, 5.0f, 5.0f, 10, 0xE8C8B0FFu);
        E(-1, -17 + b, 4, 3.2f, 8, 0xC8C8D0FFu);                  /* the bun */
        E(1.2f, -11.5f + b, 1.8f, 1.8f, 8, 0xD0E8FFFFu); E(-2.2f, -11.5f + b, 1.8f, 1.8f, 8, 0xD0E8FFFFu);   /* glasses */
        eyes(-0.5f, -11.5f + b, 3.4f, 0.8f, 0x302030FFu);
        { float bon[8] = { -7, -9 + b, -5, -17 + b, 5, -17 + b, 7, -9 + b }; P(bon, 4, 0xB898D0FFu); }
        Lf(-6, -9 + b, 6, -9 + b, 1.0f, 0xE070A0FFu);
        break;
    default:
        E(0, -11 + b, 5.2f, 5.0f, 10, 0xE0B898FFu);
        for (int k = 0; k < 5; k++) L(-5 + (float)k * 2.5f, -14 + b, -7 + (float)k * 3.5f, -19 + b + (float)(k % 2) * 2, 1.4f, 0xF07020FFu);
        E(-1.5f, -12 + b, 2.2f, 2.0f, 8, 0xF0A030FFu); E(2.8f, -12 + b, 2.2f, 2.0f, 8, 0xF0A030FFu);   /* goggles */
        Lf(-6, -12 + b, 6, -12 + b, 1.0f, 0x3A2A1AFFu);
        E(0, -16 + b, 7, 2.0f, 10, 0x2A2A2AFFu);
        E(0, -18.5f + b, 4.5f, 3.0f, 10, 0x2A2A2AFFu);
        eyes(0.6f, -12 + b, 4.4f, 0.8f, 0x302010FFu);
        break;
    }
    FLASH = 0;
    (void)ax; (void)ay;
}

/* ================================================================== */
/* guns: drawn in a rotated local frame                                 */
/* ================================================================== */
static float GX0, GY0, GC, GS, GSC;
static void gframe(float x, float y, float ang, float s)
{
    GX0 = x; GY0 = y; GC = cosf(ang); GS = sinf(ang); GSC = s;
    /* keep the gun upright when pointing left */
    if (GC < 0) { GS = -GS; GC = GC; }
}
static int GFLIP;
static void gpt(float lx, float ly, float *x, float *y)
{
    if (GFLIP) ly = -ly;
    *x = GX0 + (lx * GC - ly * GS) * GSC;
    *y = GY0 + (lx * GS + ly * GC) * GSC;
}
static void grect(float x0, float y0, float x1, float y1, uint32_t c, int outline)
{
    float ax, ay, bx, by, cx, cy, dx, dy;
    if (outline) {
        float o = 1.0f / GSC;
        gpt(x0 - o, y0 - o, &ax, &ay); gpt(x1 + o, y0 - o, &bx, &by); gpt(x1 + o, y1 + o, &cx, &cy); gpt(x0 - o, y1 + o, &dx, &dy);
        gfx_quad4(ax, ay, OUT, bx, by, OUT, cx, cy, OUT, dx, dy, OUT);
    }
    gpt(x0, y0, &ax, &ay); gpt(x1, y0, &bx, &by); gpt(x1, y1, &cx, &cy); gpt(x0, y1, &dx, &dy);
    gfx_quad4(ax, ay, c, bx, by, c, cx, cy, c, dx, dy, c);
}

static void gun_shape(int w, float spin, float t, uint32_t tint)
{
    uint32_t steel = col_mix(0x8A8C98FFu, tint, 0.25f), dark = 0x2A2A32FFu, wood = col_mix(0x8A4A22FFu, tint, 0.2f);
    switch (w) {
    case W_REVOLVER:
        grect(-3, 0, 1, 5, wood, 1);
        grect(-3, -3, 4, 1, steel, 1);
        grect(4, -2.5f, 11, -0.5f, steel, 1);
        grect(0, -2, 3, 1.5f, dark, 0);
        break;
    case W_SCATTER:
        grect(-9, -1, -1, 3, wood, 1);
        grect(-1, -2.5f, 4, 2, 0x4A3A2AFFu, 1);
        grect(4, -2.5f, 17, -0.5f, steel, 1);
        grect(4, 0, 17, 1.8f, steel, 1);
        grect(7, 2, 12, 3.5f, wood, 1);
        break;
    case W_GATLING:
        grect(-8, -4, 2, 4, 0x6A5A3AFFu, 1);
        grect(2, -5, 5, 5, steel, 1);
        for (int k = 0; k < 3; k++) {
            float o = sinf(spin * 40.0f * t + (float)k * 2.1f) * 3.0f;
            grect(5, o - 0.8f, 20, o + 0.8f, k == 1 ? 0xB0B2C0FFu : steel, 1);
        }
        grect(14, -4, 16, 4, dark, 1);
        break;
    case W_DYNAMITE:
        grect(-6, -2.5f, 6, 2.5f, 0xD02820FFu, 1);
        grect(-6, -1, 6, 1, 0xE84030FFu, 0);
        grect(-2, -2.5f, -1, 2.5f, 0xE8D8B0FFu, 0);
        grect(6, -0.4f, 9, 0.4f, 0xE8D8B0FFu, 0);
        break;
    case W_TESLA:
        grect(-6, -3, 4, 3, 0x5A4A3AFFu, 1);
        for (int k = 0; k < 4; k++) grect(-4 + (float)k * 2.4f, -4, -3 + (float)k * 2.4f, 4, 0xD08030FFu, 0);
        grect(4, -1.2f, 10, 1.2f, steel, 1);
        break;
    default:
        grect(-8, -2, 2, 3, 0xE8E8F0FFu, 1);
        grect(2, -3, 18, -0.5f, 0xF0F0F8FFu, 1);
        grect(2, 0.5f, 18, 2.5f, 0xD040E0FFu, 1);
        grect(-2, -4.5f, 5, -3, 0xD040E0FFu, 0);
        break;
    }
}

void art_weapon_held(int w, float x, float y, float ang, float s, float spin, float t)
{
    float c = cosf(ang);
    GFLIP = c < 0;
    GX0 = x; GY0 = y; GC = cosf(ang); GS = sinf(ang); GSC = s;
    gun_shape(w, spin, t, 0xFFFFFFFFu);
    if (w == W_TESLA) {
        float bx, by; gpt(11, 0, &bx, &by);
        gfx_poly(bx, by, 3.2f * s, 8, 0, 0x80E0FFFFu);
    }
    if (w == W_DYNAMITE) { float fx, fy; gpt(9.5f, 0, &fx, &fy); gfx_poly(fx, fy, 1.8f * s, 6, 0, 0xFFE070FFu); }
}

void art_weapon_icon(int w, float x, float y, float s, float ang, uint32_t tint)
{
    GFLIP = 0;
    GX0 = x - cosf(ang) * 5.0f * s; GY0 = y - sinf(ang) * 5.0f * s; GC = cosf(ang); GS = sinf(ang); GSC = s;
    gun_shape(w, 0, 0, tint);
    if (w == W_TESLA) { float bx, by; gpt(11, 0, &bx, &by); gfx_poly(bx, by, 3.2f * s, 8, 0, 0x80E0FFFFu); }
    if (w == W_DYNAMITE) {   /* a bundle of three */
        GY0 -= 5.5f * s * GC; GX0 += 5.5f * s * GS; gun_shape(w, 0, 0, tint);
        GY0 += 11.0f * s * GC; GX0 -= 11.0f * s * GS; gun_shape(w, 0, 0, tint);
    }
    (void)gframe;
}

/* ================================================================== */
/* loot                                                                 */
/* ================================================================== */
void art_pickup(const Pickup *u, float x, float y, float t)
{
    float bob = sinf(t * 5.0f + u->x * 0.1f) * 1.5f;
    switch (u->kind) {
    case PU_COIN:                 /* cheap: there can be hundreds */
        gfx_poly(x, y + bob, 4.4f, 6, 0, OUT);
        gfx_poly2(x, y + bob, 3.0f, 6, 0, 0xFFF0A0FFu, 0xE0A828FFu);
        break;
    case PU_GOLD:
        gfx_poly(x, y + bob, 5.8f, 6, 0, OUT);
        gfx_poly2(x, y + bob, 4.4f, 6, 0, 0xFFF4B0FFu, 0xD89820FFu);
        gfx_rect(x - 1.5f, y + bob - 3.0f, 3.0f, 1.2f, 0xFFFFE0FFu);
        break;
    case PU_GEM: {
        float yy = y + bob;
        gfx_quad4(x, yy - 7.5f, OUT, x + 6, yy, OUT, x, yy + 7.5f, OUT, x - 6, yy, OUT);
        gfx_quad4(x, yy - 6, 0xA0F8FFFFu, x + 4.5f, yy, 0x30D8F0FFu, x, yy + 6, 0x1890C0FFu, x - 4.5f, yy, 0x30D8F0FFu);
        break;
    }
    case PU_HEALTH:
        begin(x, y + bob, 1.2f, 1, 0, 0);
        R(-3, -2, 3, 7, 0x8A4A1AFFu); R(-1.2f, -7, 1.2f, -2, 0x8A4A1AFFu); Rf(-3, 1, 3, 4, 0xE8D8B0FFu);
        Lf(-1, 2.5f, 1, 2.5f, 1.2f, 0xC02020FFu);
        break;
    case PU_MAGNET:
        begin(x, y + bob, 1.2f, 1, 0, 0);
        L(-4, -4, -4, 3, 3, 0xD02828FFu); L(4, -4, 4, 3, 3, 0xD02828FFu); L(-4, 3, 4, 3, 3, 0xD02828FFu);
        Rf(-5.5f, -6, -2.5f, -3.5f, 0xE0E0E8FFu); Rf(2.5f, -6, 5.5f, -3.5f, 0xE0E0E8FFu);
        break;
    case PU_NUKE:
        begin(x, y + bob, 1.2f, 1, 0, 0);
        R(-6, -5, 6, 5, 0xB02818FFu); Lf(-6, -1, 6, -1, 1.0f, 0x6A1810FFu);
        skull(0, -1, 2.5f, 0xF0E8D8FFu, 0xFF2020FFu);
        break;
    case PU_FRENZY:
        begin(x, y + bob, 1.2f, 1, 0, 0);
        { float fl[6] = { -4, 0, 0, -10 + sinf(t * 18) * 1.5f, 4, 0 }; Pf(fl, 3, 0xE040FFFFu); }
        skull(0, -1, 3.5f, 0xF0E0FFFFu, 0xFF40FFFFu);
        break;
    case PU_CHEST:
        begin(x, y + bob * 0.5f, 1.4f, 1, 0, 0);
        R(-7, -2, 7, 6, 0x7A4A22FFu); E(0, -2, 7, 3.5f, 10, 0x8A5A2AFFu);
        Rf(-7, -2.5f, 7, -1, 0xE8B030FFu); Rf(-1.2f, -3, 1.2f, 2, 0xF0D060FFu);
        Lf(-7, 6, 7, 6, 1, 0xE8B030FFu);
        break;
    }
}

/* ================================================================== */
/* shop icons                                                           */
/* ================================================================== */
void art_upgrade_icon(int u, float x, float y, float s, float t)
{
    begin(x, y, s, 1, 0, 0);
    switch (u) {
    case UP_VITAL: {
        float pul = 1.0f + sinf(t * 5.0f) * 0.06f;
        S = s * pul;
        E(-3.5f, -2, 4.5f, 4.5f, 10, 0xD82838FFu); E(3.5f, -2, 4.5f, 4.5f, 10, 0xD82838FFu);
        float h[6] = { -8, -0.5f, 8, -0.5f, 0, 8 };
        Pf(h, 3, 0xD82838FFu);
        Ef(-4, -3.5f, 1.5f, 1.2f, 6, 0xFF8090FFu);
        break;
    }
    case UP_ARMOR: {
        float sh[10] = { -7, -7, 7, -7, 7, 0, 0, 8, -7, 0 };
        P(sh, 5, 0x8A8C98FFu);
        float in[10] = { -4.5f, -4.5f, 4.5f, -4.5f, 4.5f, -0.5f, 0, 5, -4.5f, -0.5f };
        Pf(in, 5, 0x5A6A8AFFu);
        break;
    }
    case UP_SPEED:
        R(-3, -8, 3, 3, 0x6A3A1AFFu); R(-3, 1, 8, 6, 0x6A3A1AFFu); Rf(-3, 5, 8, 7, 0x2A1A10FFu);
        Ef(-4, 4, 1.6f, 1.6f, 6, 0xE8C060FFu);
        L(-10, -4, -6, -4, 1.0f, 0xF0F0F0FFu); L(-11, 0, -6, 0, 1.0f, 0xF0F0F0FFu);
        break;
    case UP_MAGNET:
        L(-5, -6, -5, 2, 3.5f, 0xD02828FFu); L(5, -6, 5, 2, 3.5f, 0xD02828FFu); L(-5, 3, 5, 3, 3.5f, 0xD02828FFu);
        Rf(-7, -8, -3, -5, 0xE0E0E8FFu); Rf(3, -8, 7, -5, 0xE0E0E8FFu);
        break;
    case UP_POWER:
        R(-6, -6, 6, 8, 0x7A4A22FFu); Lf(-6, -3, 6, -3, 1.2f, 0x3A2A1AFFu); Lf(-6, 4, 6, 4, 1.2f, 0x3A2A1AFFu);
        Rf(-3, -1, 3, 2, 0x2A2A2AFFu);
        L(0, -6, 3, -11, 1.0f, 0xE8D8B0FFu);
        Ef(3, -11, 1.4f + sinf(t * 20) * 0.4f, 1.4f, 6, 0xFFE070FFu);
        break;
    case UP_RATE:
        for (int k = 0; k < 3; k++) { R(-6 + (float)k * 4.5f, -4, -3 + (float)k * 4.5f, 6, 0xE8B048FFu); E(-4.5f + (float)k * 4.5f, -5, 1.5f, 2.0f, 6, 0xC09038FFu); }
        break;
    case UP_LUCK:
        for (int k = 0; k < 4; k++) { float a = (float)k * PI_F / 2 + PI_F / 4; E(cosf(a) * 3.5f, sinf(a) * 3.5f - 1, 3.4f, 3.4f, 8, 0x40B040FFu); }
        L(0, 1, 2, 8, 1.2f, 0x2A7A2AFFu);
        break;
    case UP_BACKUP: {
        float a = t * 2.0f;
        for (int k = 0; k < 3; k++) { float b = a + (float)k * TAU_F / 3; E(cosf(b) * 6, sinf(b) * 5, 2.4f, 2.4f, 6, 0xB0B2C0FFu); }
        E(0, 0, 3, 3, 8, 0xFFB030FFu);
        break;
    }
    default:
        R(-4, -2, 4, 9, 0x8A4A1AFFu); R(-1.5f, -8, 1.5f, -2, 0x8A4A1AFFu); Rf(-4, 2, 4, 6, 0xE8D8B0FFu);
        Lf(-1.5f, 4, 1.5f, 4, 1.4f, 0xC02020FFu); Lf(0, 2.5f, 0, 5.5f, 1.4f, 0xC02020FFu);
        break;
    }
}

/* ================================================================== */
/* props: six kinds of scenery per biome                                */
/* ================================================================== */
static void rock(float lx, float ly, float r, uint32_t c, uint32_t hi)
{
    float p[12] = { lx - r, ly + r * 0.4f, lx - r * 0.7f, ly - r * 0.5f, lx, ly - r * 0.8f, lx + r * 0.8f, ly - r * 0.4f, lx + r, ly + r * 0.4f, lx, ly + r * 0.6f };
    P(p, 6, c);
    float h[8] = { lx - r * 0.6f, ly - r * 0.3f, lx, ly - r * 0.6f, lx + r * 0.5f, ly - r * 0.3f, lx, ly - r * 0.1f };
    Pf(h, 4, hi);
}

static void dead_tree(float s, uint32_t c)
{
    L(0, 4, 0, -24, 4.0f * s, c);
    L(0, -12, -12, -24, 2.4f * s, c); L(-12, -24, -16, -32, 1.6f * s, c);
    L(0, -18, 11, -30, 2.4f * s, c); L(11, -30, 17, -33, 1.4f * s, c);
    L(0, -24, 3, -36, 1.8f * s, c);
    L(0, 4, -7, 8, 2.0f, c); L(0, 4, 7, 8, 2.0f, c);
}

void art_prop(int biome, int kind, float x, float y, float t, uint32_t seed)
{
    begin(x, y, 1.0f, (seed & 1) ? 1 : -1, 0, 0);
    switch (biome * 6 + kind) {
    /* DUST GULCH */
    case 0: rock(0, 0, 11, 0x6A5040FFu, 0x8A6A52FFu); break;
    case 1: skull(0, 0, 4, 0xE8DCC0FFu, 0x1A1018FFu); L(-4, -3, -12, -8, 2.2f, 0xE8DCC0FFu); L(4, -3, 12, -8, 2.2f, 0xE8DCC0FFu); L(-12, -8, -13, -12, 1.8f, 0xE8DCC0FFu); L(12, -8, 13, -12, 1.8f, 0xE8DCC0FFu); break;
    case 2: R(-3, -16, 3, 4, 0x4E8A3AFFu); R(-9, -10, -3, -6, 0x447A34FFu); R(-9, -14, -6, -6, 0x447A34FFu); R(3, -6, 8, -3, 0x447A34FFu); R(5, -10, 8, -3, 0x447A34FFu); break;
    case 3: { uc(12); for (int i = 0; i < 6; i++) Lf(0, 0, UC[i * 2][0] * 9, UC[i * 2][1] * 6, 1.2f, 0x6A4A2AFFu);
              ell_raw(PX(0), PY(0), 11, 7.5f, 14, OUT); ell_raw(PX(0), PY(0), 9.5f, 6.2f, 14, 0x8A5A30FFu); ell_raw(PX(0), PY(0), 8, 5, 14, 0x76583AFFu);
              for (int i = 0; i < 6; i++) Lf(0, 0, UC[i * 2][0] * 8, UC[i * 2][1] * 5, 1.2f, 0x8A5A30FFu); E(0, 0, 2, 1.5f, 6, 0x5A3A20FFu); break; }
    case 4: for (int k = 0; k < 7; k++) { float a = (float)k * 0.9f; L(0, 2, cosf(a) * 10, sinf(a) * 6 - 4, 1.0f, 0x8A6A3AFFu); } break;
    case 5: rock(-6, 2, 7, 0x6A5040FFu, 0x8A6A52FFu); rock(6, 0, 9, 0x5E463AFFu, 0x7E6048FFu); break;
    /* ROTWOOD BAYOU */
    case 6: for (int k = 0; k < 6; k++) { float sx = -8 + (float)k * 3.2f, sw = sinf(t * 1.5f + (float)k) * 1.5f; L(sx, 4, sx + sw, -16 - (float)(k % 3) * 4, 1.4f, 0x5A7A3AFFu); if (k % 2) E(sx + sw, -18 - (float)(k % 3) * 4, 1.4f, 3.2f, 6, 0x5A3A22FFu); } break;
    case 7: ell_raw(PX(0), PY(0), 30, 14, 16, 0x1A261EFFu); ell_raw(PX(0), PY(0), 26, 11, 16, 0x223024FFu);
            E(-10, -2, 5, 2.5f, 8, 0x4A8A3AFFu); E(8, 3, 4, 2, 8, 0x4A8A3AFFu); E(14, -4, 3, 1.6f, 8, 0x5A9A44FFu); Ef(-9, -3, 1.2f, 1.0f, 6, 0xE880C0FFu); break;
    case 8: R(-9, -10, 9, 6, 0x4A3A2AFFu); E(0, -10, 9, 3.5f, 10, 0x6A5238FFu); Ef(0, -10, 6, 2.2f, 10, 0x8A6A48FFu); Ef(0, -10, 2.5f, 1.0f, 8, 0x6A5238FFu); L(-9, 4, -14, 8, 2.5f, 0x4A3A2AFFu); L(9, 4, 14, 8, 2.5f, 0x4A3A2AFFu); break;
    case 9: for (int k = 0; k < 3; k++) { float mx = -6 + (float)k * 6, mh = 6 + (float)((seed >> k) & 3) * 2; L(mx, 2, mx, -mh, 1.6f, 0xD8E0C0FFu); E(mx, -mh, 4, 2.4f, 8, 0x60D0B0FFu); } break;
    case 10: dead_tree(1.0f, 0x2A2A24FFu); break;
    case 11: rock(0, 0, 12, 0x3A4A34FFu, 0x4A6A3AFFu); Ef(-3, -5, 5, 2, 8, 0x5A8A3AFFu); break;
    /* GRAVEWATCH */
    case 12: { float gs[10] = { -7, 4, 7, 4, 7, -10, 0, -15, -7, -10 }; P(gs, 5, 0x6A6870FFu); Lf(-3, -6, 3, -6, 1.2f, 0x3A3840FFu); Lf(-3, -3, 3, -3, 1.2f, 0x3A3840FFu); Rf(-9, 3, 9, 6, 0x2A2420FFu); break; }
    case 13: R(-1.8f, -18, 1.8f, 4, 0x5A5860FFu); R(-7, -12, 7, -8.5f, 0x5A5860FFu); Rf(-6, 3, 6, 6, 0x2A2420FFu); break;
    case 14: dead_tree(1.1f, 0x3A2A22FFu); break;
    case 15: R(-6, -6, -3, 3, 0xE8E0C8FFu); R(3, -9, 6, 3, 0xE8E0C8FFu); Ef(-4.5f, -8, 1.0f, 1.8f, 6, 0xFFC040FFu); Ef(4.5f, -11, 1.0f, 1.8f, 6, 0xFFC040FFu); break;
    case 16: skull(-5, 0, 3.5f, 0xE0D8C4FFu, 0x1A1018FFu); skull(4, 2, 3.2f, 0xD8D0BCFFu, 0x1A1018FFu); skull(0, -4, 3.4f, 0xE8E0CCFFu, 0x1A1018FFu); break;
    case 17: R(-8, -14, 8, 10, 0x2A2420FFu); Rf(-6, -12, 6, 8, 0x0E0C0AFFu); E(0, -16, 9, 3, 10, 0x4A3A2AFFu); break;
    /* ASHEN HOLLOW */
    case 18: R(-7, -8, 7, 4, 0x1E1A18FFu); E(0, -8, 7, 2.6f, 10, 0x2E2824FFu); Ef(0, -8, 4, 1.4f, 8, 0xFF6020FFu); break;
    case 19: dead_tree(1.2f, 0x161210FFu); Lf(0, -4, 0, -16, 1.0f, 0xFF5018FFu); break;
    case 20: rock(0, 0, 10, 0x2A2220FFu, 0x3A302CFFu); Lf(-5, 0, 4, -4, 1.3f, 0xFF7020FFu); Lf(1, 2, 6, 0, 1.0f, 0xFF9030FFu); break;
    case 21: skull(-4, 0, 3.4f, 0xC8BCA8FFu, 0x1A1018FFu); Lf(0, 3, 9, 1, 2.0f, 0xC8BCA8FFu); Lf(2, 5, 10, 7, 2.0f, 0xC8BCA8FFu); break;
    case 22: ell_raw(PX(0), PY(0), 22, 9, 14, 0x140C08FFu); ell_raw(PX(0), PY(0), 18, 6.5f, 14, 0xD84010FFu); ell_raw(PX(0), PY(0), 11, 3.5f, 12, 0xFFA030FFu); break;
    case 23: rock(0, 0, 13, 0x3A3430FFu, 0x4A443EFFu); break;
    /* HELLMOUTH */
    case 24: { float sw = sinf(t * 1.3f + (float)(seed & 7)) * 3; L(0, 4, sw, -16, 3, 0x6A2A3AFFu); E(sw, -18, 5, 5, 10, 0xF0E8E8FFu); Ef(sw + 1.5f, -18, 2.4f, 2.4f, 8, 0xD02030FFu); Ef(sw + 1.8f, -18, 1.1f, 1.4f, 6, 0x0A0408FFu); break; }
    case 25: for (int k = 0; k < 4; k++) { float rx = -9 + (float)k * 6; L(rx, 4, rx + 2, -10 - (float)(k % 2) * 3, 2.0f, 0xD8CCB8FFu); } L(-10, 4, 10, 4, 3.0f, 0xD8CCB8FFu); break;
    case 26: for (int k = 0; k < 4; k++) { float sx = -9 + (float)k * 6; float sp[6] = { sx - 2.5f, 4, sx + 0.5f, -12 - (float)(k % 2) * 6, sx + 3, 4 }; P(sp, 3, 0x2A1A28FFu); } break;
    case 27: ell_raw(PX(0), PY(0), 24, 10, 14, 0x200810FFu); ell_raw(PX(0), PY(0), 20, 7.5f, 14, 0x8A1020FFu); ell_raw(PX(-4), PY(-1), 7, 2.5f, 10, 0xC02838FFu); break;
    case 28: { float p = 1.0f + sinf(t * 3.0f + (float)(seed & 7)) * 0.08f; E(0, -3, 8 * p, 6 * p, 12, 0x9A3A5AFFu); Ef(-2, -5, 3, 2, 8, 0xE0809AFFu); E(5, 1, 4, 3, 8, 0x8A2A4AFFu); break; }
    case 29: rock(0, 0, 12, 0x1E1420FFu, 0x2E2030FFu); break;
    }
}

void art_barrel(float x, float y, float t)
{
    begin(x, y, 1.0f, 1, 0, 0);
    R(-7, -12, 7, 6, 0x9A3A20FFu);
    E(0, -12, 7, 2.6f, 10, 0xB04A28FFu);
    Rf(-7, -8, 7, -6.5f, 0x3A2A22FFu); Rf(-7, 1, 7, 2.5f, 0x3A2A22FFu);
    Rf(-3, -5, 3, -1, 0xF0E0B0FFu);
    Lf(-1.5f, -4, 1.5f, -2, 0.8f, 0xC02020FFu); Lf(1.5f, -4, -1.5f, -2, 0.8f, 0xC02020FFu);
    L(0, -13, 2, -18, 1.0f, 0xE8D8B0FFu);
    Ef(2, -18, 1.2f + 0.4f * sinf(t * 25.0f), 1.2f, 6, 0xFFE070FFu);
}
