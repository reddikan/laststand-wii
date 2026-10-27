/*
 * LAST STAND - the cast, drawn from flat shapes with chunky dark outlines and three-tone shading: twenty
 * monsters, five bosses and four gunslingers, all animated (run cycles with knees and swinging arms, heads
 * that bob a beat behind the body, blinking eyes that track you, mouths that snarl, gape and scream, faces that
 * screw up when hit, wounds that open as they take damage), plus their corpses, the guns, the loot and icons.
 *
 * Every sprite is drawn about its centre point (feet about 11 units below), facing right (mirrored by `face`),
 * optionally rotated (corpses). A white hit flash, a gold elite outline and a corpse darkening are applied
 * through the colour helpers.
 */
#include "game_int.h"
#include "gfx.h"

#include <string.h>

#define OUT  0x120C10FFu

static float FX = 1.0f;          /* facing for the sprite being drawn: 1 right, -1 left */
static float OX, OY;             /* its origin */
static float S = 1.0f;           /* its scale */
static float RC = 1.0f, RS = 0.0f;   /* its rotation */
static float FLASH;              /* white hit flash 0..1 */
static float DARK;               /* corpse: pulls every colour towards dried-blood black */
static uint32_t OUTC = OUT;      /* outline colour (gold for elites) */
static float ALPHA = 1.0f;
static int DETAIL = 1;           /* 0 when the screen is packed: no shading or limb outlines */

void art_set_detail(int d) { DETAIL = d; }

static uint32_t C(uint32_t c)
{
    if (DARK > 0) c = col_mix(c, 0x1E0E0EFFu, DARK);
    if (FLASH > 0) c = col_mix(c, 0xFFFFFFFFu, FLASH);
    if (ALPHA < 1.0f) c = col_a(c, ALPHA);
    return c;
}
static uint32_t CO(void) { return ALPHA < 1.0f ? col_a(OUTC, ALPHA) : OUTC; }
static uint32_t SHD(uint32_t c) { return col_mix(c, 0x0A0608FFu, 0.38f); }
static uint32_t HIL(uint32_t c) { return col_mix(c, 0xFFF4E0FFu, 0.22f); }

/* unit circles */
static float UC[25][2];
static int uc_n;
static void uc(int n)
{
    if (uc_n == n) return;
    uc_n = n;
    for (int i = 0; i <= n; i++) { UC[i][0] = cosf((float)i / (float)n * TAU_F); UC[i][1] = sinf((float)i / (float)n * TAU_F); }
}

/* local -> screen (mirror, rotate, scale) */
static inline float TX(float lx, float ly) { return OX + ((lx * FX) * RC - ly * RS) * S; }
static inline float TY(float lx, float ly) { return OY + ((lx * FX) * RS + ly * RC) * S; }
/* the unrotated forms, for the simple sprites further down */
#define PX(lx) (OX + (lx) * S * FX)
#define PY(ly) (OY + (ly) * S)

static void ell_raw(float cx, float cy, float rx, float ry, int n, uint32_t c)
{
    /* small on screen: fewer segments (nobody can tell, and there may be hundreds) */
    float rm = rx > ry ? rx : ry;
    int lim = 4 + (int)(rm * 0.7f);
    if (n > lim) n = lim < 4 ? 4 : lim;
    uc(n);
    float ax = RC * rx, ay = RS * rx, bx = -RS * ry, by = RC * ry;
    float px = cx + ax, py = cy + ay;
    for (int i = 1; i <= n; i++) {
        float x = cx + UC[i][0] * ax + UC[i][1] * bx, y = cy + UC[i][0] * ay + UC[i][1] * by;
        gfx_tri(cx, cy, c, px, py, c, x, y, c);
        px = x; py = y;
    }
}

static void E(float lx, float ly, float rx, float ry, int n, uint32_t c)
{
    float o = 1.7f;
    ell_raw(TX(lx, ly), TY(lx, ly), rx * S + o, ry * S + o, n, CO());
    ell_raw(TX(lx, ly), TY(lx, ly), rx * S, ry * S, n, C(c));
}
static void Ef(float lx, float ly, float rx, float ry, int n, uint32_t c) { ell_raw(TX(lx, ly), TY(lx, ly), rx * S, ry * S, n, C(c)); }
/* shaded: outline, a shadowed base, the lit part up and to the left, a glint */
static void E3(float lx, float ly, float rx, float ry, int n, uint32_t c)
{
    if (!DETAIL) { E(lx, ly, rx, ry, n, c); return; }
    float o = 1.7f;
    ell_raw(TX(lx, ly), TY(lx, ly), rx * S + o, ry * S + o, n, CO());
    ell_raw(TX(lx, ly), TY(lx, ly), rx * S, ry * S, n, C(SHD(c)));
    ell_raw(TX(lx - rx * 0.14f, ly - ry * 0.16f), TY(lx - rx * 0.14f, ly - ry * 0.16f), rx * S * 0.82f, ry * S * 0.8f, n, C(c));
    ell_raw(TX(lx - rx * 0.38f, ly - ry * 0.42f), TY(lx - rx * 0.38f, ly - ry * 0.42f), rx * S * 0.22f, ry * S * 0.18f, 5, C(HIL(c)));
}

static void P_raw(const float *pts, int n, uint32_t c, float grow)
{
    float cx = 0, cy = 0;
    for (int i = 0; i < n; i++) { cx += pts[i * 2]; cy += pts[i * 2 + 1]; }
    cx /= (float)n; cy /= (float)n;
    float sx = TX(cx, cy), sy = TY(cx, cy);
    for (int i = 0; i < n; i++) {
        int j = (i + 1) % n;
        float ax = pts[i * 2], ay = pts[i * 2 + 1], bx = pts[j * 2], by = pts[j * 2 + 1];
        if (grow > 0) {
            float dax = ax - cx, day = ay - cy, la = sqrtf(dax * dax + day * day) + 1e-4f;
            float dbx = bx - cx, dby = by - cy, lb = sqrtf(dbx * dbx + dby * dby) + 1e-4f;
            ax += dax / la * grow / S; ay += day / la * grow / S; bx += dbx / lb * grow / S; by += dby / lb * grow / S;
        }
        gfx_tri(sx, sy, c, TX(ax, ay), TY(ax, ay), c, TX(bx, by), TY(bx, by), c);
    }
}
static void P(const float *pts, int n, uint32_t c) { P_raw(pts, n, CO(), 1.9f); P_raw(pts, n, C(c), 0); }
static void Pf(const float *pts, int n, uint32_t c) { P_raw(pts, n, C(c), 0); }
/* a shaded polygon: the body, then a darker band along its lower edge */
static void P3(const float *pts, int n, uint32_t c)
{
    P(pts, n, c);
    if (!DETAIL) return;
    float lo = -1e9f, hi = 1e9f, cx = 0;
    for (int i = 0; i < n; i++) { if (pts[i * 2 + 1] > lo) lo = pts[i * 2 + 1]; if (pts[i * 2 + 1] < hi) hi = pts[i * 2 + 1]; cx += pts[i * 2]; }
    cx /= (float)n;
    /* shadow wedge on the far side */
    float sh[64];
    int m = n < 32 ? n : 32;
    for (int i = 0; i < m; i++) { float x = pts[i * 2], y = pts[i * 2 + 1]; sh[i * 2] = x > cx ? x : cx + (x - cx) * 0.15f; sh[i * 2 + 1] = y; }
    P_raw(sh, m, C(col_a(SHD(c), 0.55f)), 0);
    (void)lo; (void)hi;
}

static void R(float x0, float y0, float x1, float y1, uint32_t c) { float p[8] = { x0, y0, x1, y0, x1, y1, x0, y1 }; P(p, 4, c); }
static void Rf(float x0, float y0, float x1, float y1, uint32_t c) { float p[8] = { x0, y0, x1, y0, x1, y1, x0, y1 }; Pf(p, 4, c); }

static void L(float x0, float y0, float x1, float y1, float w, uint32_t c)
{
    if (DETAIL) gfx_line(TX(x0, y0), TY(x0, y0), TX(x1, y1), TY(x1, y1), w * S + 3.0f, CO(), CO());
    gfx_line(TX(x0, y0), TY(x0, y0), TX(x1, y1), TY(x1, y1), w * S, C(c), C(c));
}
static void Lf(float x0, float y0, float x1, float y1, float w, uint32_t c) { gfx_line(TX(x0, y0), TY(x0, y0), TX(x1, y1), TY(x1, y1), w * S, C(c), C(c)); }

/* legacy two-dot glowing eyes (props, loot) */
static void eyes(float lx, float ly, float sep, float r, uint32_t c)
{
    Ef(lx - sep * 0.5f, ly, r, r, 6, c);
    Ef(lx + sep * 0.5f, ly, r, r, 6, c);
}

static void begin(float x, float y, float s, float face, float flash, int elite)
{
    OX = x; OY = y; S = s; FX = face >= 0 ? 1.0f : -1.0f; FLASH = flash; OUTC = elite ? 0xFFC830FFu : OUT; ALPHA = 1.0f;
    RC = 1.0f; RS = 0.0f; DARK = 0;
}

void art_shadow(float x, float y, float rx, float ry, float a)
{
    uc(10);
    uint32_t c = RGBA(0, 0, 0, (int)(120 * a)), e = RGBA(0, 0, 0, 0);
    for (int i = 0; i < 10; i++)
        gfx_tri(x, y, c, x + UC[i][0] * rx, y + UC[i][1] * ry, e, x + UC[i + 1][0] * rx, y + UC[i + 1][1] * ry, e);
}

/* ================================================================== */
/* anatomy                                                              */
/* ================================================================== */
/* a leg from the hip: the foot swings on the stride, lifts on the way forward, the knee bends ahead */
static void leg(float hx, float hy, float ph, float stride, float lift, float len, float w, uint32_t c, uint32_t foot)
{
    float s = sinf(ph), cph = cosf(ph);
    float up = cph > 0 ? cph * lift : 0.0f;
    float fx = hx + s * stride, fy = hy + len - up;
    float kx = (hx + fx) * 0.5f + len * 0.18f + up * 0.4f, ky = (hy + fy) * 0.5f - up * 0.25f;
    L(hx, hy, kx, ky, w, c);
    L(kx, ky, fx, fy, w * 0.85f, c);
    if (foot) Ef(fx + w * 0.45f, fy + w * 0.2f, w * 0.85f, w * 0.5f, 5, foot);
}

/* an arm from the shoulder: a = swing angle from hanging (positive = forward), bend at the elbow */
static void arm(float sx, float sy, float a, float bend, float len, float w, uint32_t c, uint32_t hand)
{
    float ex = sx + sinf(a) * len * 0.5f, ey = sy + cosf(a) * len * 0.5f;
    float hx = ex + sinf(a + bend) * len * 0.5f, hy = ey + cosf(a + bend) * len * 0.5f;
    L(sx, sy, ex, ey, w, c);
    L(ex, ey, hx, hy, w * 0.9f, c);
    if (hand) Ef(hx, hy, w * 0.75f, w * 0.75f, 5, hand);
}

/* two eyes: whites (or sockets), pupils that track the target, blinking, brows, squeezed shut when hurt */
static void face_eyes(float x, float y, float sep, float r, uint32_t white, uint32_t pupil, const Anim *a, float angry, float pr)
{
    for (int k = -1; k <= 1; k += 2) {
        float ex = x + (float)k * sep * 0.5f;
        if (a->dead) {               /* crosses */
            Lf(ex - r * 0.7f, y - r * 0.7f, ex + r * 0.7f, y + r * 0.7f, r * 0.45f, 0x1A0A0AFFu);
            Lf(ex - r * 0.7f, y + r * 0.7f, ex + r * 0.7f, y - r * 0.7f, r * 0.45f, 0x1A0A0AFFu);
            continue;
        }
        if (a->hurt > 0.35f) {       /* screwed shut: > < */
            float d = (float)k;
            Lf(ex - r * d, y - r * 0.6f, ex + r * 0.3f * d, y, r * 0.5f, 0x140808FFu);
            Lf(ex + r * 0.3f * d, y, ex - r * d, y + r * 0.6f, r * 0.5f, 0x140808FFu);
        } else if (a->blink > 0.5f) {
            Lf(ex - r, y, ex + r, y + r * 0.15f, r * 0.45f, 0x140808FFu);
        } else {
            Ef(ex, y, r, r * (0.95f - 0.25f * angry), 6, white);
            float px = ex + a->lookx * r * 0.38f, py = y + a->looky * r * 0.3f;
            Ef(px, py, r * pr, r * pr * 1.1f, 5, pupil);
        }
        if (angry > 0.01f) {
            float ox = ex + (float)k * r * 1.2f, ix = ex - (float)k * r * 0.9f;
            Lf(ox, y - r * 1.45f, ix, y - r * 1.45f + angry * r * 0.95f, r * 0.55f, 0x140A0AFFu);
        }
    }
}

/* a mouth: dark hole, teeth top and bottom, a tongue when it's wide */
static void mouth(float x, float y, float w, float open, int teeth, uint32_t lip)
{
    float h = w * (0.1f + open * 0.5f);
    if (lip) Ef(x, y, w * 0.56f, h + w * 0.08f, 8, lip);
    Ef(x, y, w * 0.5f, h, 8, 0x1A0406FFu);
    if (open > 0.3f) Ef(x, y + h * 0.5f, w * 0.26f, h * 0.38f, 6, 0xA0303AFFu);
    if (teeth > 0) {
        float tw = w * 0.8f / (float)teeth, th = w * 0.18f + h * 0.15f;
        for (int i = 0; i < teeth; i++) {
            float tx = x - w * 0.4f + ((float)i + 0.5f) * tw;
            float t1[6] = { tx - tw * 0.45f, y - h * 0.8f, tx + tw * 0.45f, y - h * 0.8f, tx, y - h * 0.8f + th };
            Pf(t1, 3, 0xE6DCC0FFu);
            if (open > 0.15f) { float t2[6] = { tx - tw * 0.45f, y + h * 0.8f, tx + tw * 0.45f, y + h * 0.8f, tx, y + h * 0.8f - th }; Pf(t2, 3, 0xD8CCB0FFu); }
        }
    }
}

/* gashes that open as it takes damage, each dribbling */
static void wounds(const Anim *a, float x0, float y0, float x1, float y1, uint32_t blood)
{
    for (int k = 0; k < a->wounds && k < 4; k++) {
        uint32_t h = hash32(a->seed * 2654435761u + (uint32_t)k * 40503u);
        float px = x0 + (x1 - x0) * (float)(h & 255) / 255.0f, py = y0 + (y1 - y0) * (float)((h >> 8) & 255) / 255.0f;
        float ang = (float)((h >> 16) & 255) / 255.0f * 2.4f - 1.2f;
        float l = 2.2f + (float)((h >> 24) & 3) * 0.6f, c = cosf(ang) * l, s = sinf(ang) * l;
        float g[8] = { px - c, py - s, px - s * 0.25f, py + c * 0.25f, px + c, py + s, px + s * 0.25f, py - c * 0.25f };
        Pf(g, 4, 0x2A0405FFu);
        float g2[8] = { px - c * 0.7f, py - s * 0.7f, px - s * 0.1f, py + c * 0.1f, px + c * 0.7f, py + s * 0.7f, px + s * 0.1f, py - c * 0.1f };
        Pf(g2, 4, col_mix(blood, 0xFF3030FFu, 0.25f));
        float dl = 1.5f + fmodf(a->t * 2.0f + (float)k * 1.3f, 3.5f);
        Lf(px, py, px + 0.2f, py + dl, 0.7f, blood);
    }
}

static void skull(float lx, float ly, float r, uint32_t bone, uint32_t eye)
{
    E(lx, ly, r, r * 0.95f, 10, bone);
    Rf(lx - r * 0.55f, ly + r * 0.45f, lx + r * 0.55f, ly + r * 1.05f, bone);
    Ef(lx - r * 0.38f, ly + 0.05f * r, r * 0.3f, r * 0.34f, 6, 0x1A1018FFu);
    Ef(lx + r * 0.38f, ly + 0.05f * r, r * 0.3f, r * 0.34f, 6, 0x1A1018FFu);
    eyes(lx, ly + 0.08f * r, r * 0.76f, r * 0.12f, eye);
    for (int k = -1; k <= 1; k++) Lf(lx + (float)k * r * 0.25f, ly + r * 0.6f, lx + (float)k * r * 0.25f, ly + r * 1.0f, 0.8f, 0x2A2020FFu);
}

/* an animated skull: sockets with burning pupils that track you, a jaw that chatters open */
static void skull_a(float lx, float ly, float r, uint32_t bone, uint32_t eye, const Anim *a, float jaw)
{
    E3(lx, ly, r, r * 0.92f, 10, bone);
    /* the jaw drops */
    float jy = ly + r * 0.55f + jaw * r * 0.55f;
    if (jaw > 0.1f) Ef(lx + r * 0.1f, ly + r * 0.62f + jaw * r * 0.25f, r * 0.42f, jaw * r * 0.3f, 6, 0x120808FFu);
    R(lx - r * 0.5f + r * 0.1f, jy, lx + r * 0.6f, jy + r * 0.38f, SHD(bone));
    for (int k = 0; k < 3; k++) Lf(lx - r * 0.25f + (float)k * r * 0.28f, jy, lx - r * 0.25f + (float)k * r * 0.28f, jy + r * 0.3f, 0.6f, 0x2A1E1EFFu);
    Rf(lx - r * 0.5f + r * 0.1f, ly + r * 0.45f, lx + r * 0.6f, ly + r * 0.62f, bone);
    /* sockets */
    Anim b = *a;
    face_eyes(lx + r * 0.1f, ly - r * 0.02f, r * 0.82f, r * 0.33f, 0x0E0608FFu, eye, &b, a->rage * 0.8f, 0.5f);
    Pf((float[6]){ lx + r * 0.05f, ly + r * 0.3f, lx + r * 0.22f, ly + r * 0.3f, lx + r * 0.14f, ly + r * 0.12f }, 3, 0x1A1010FFu);   /* nose hole */
}

/* the stride, from the animation state */
#define STR(a, k) ((a)->mv * (k))
#define BOB(a, k) (-fabsf(sinf((a)->run)) * (a)->mv * (k))
#define SW(a) (sinf((a)->run))

/* ================================================================== */
/* monsters                                                             */
/* ================================================================== */
/* DUST GULCH ------------------------------------------------------- */
static void a_bandit(const Anim *a, uint32_t eye, uint32_t blood)
{
    float b = BOB(a, 2.0f), sw = SW(a), lean = a->mv * 2.0f;
    uint32_t bone = 0xD8CCB0FFu;
    leg(-2.5f, 4, a->run + PI_F, STR(a, 4.5f), 3.0f, 7, 1.8f, bone, 0x3A2A1EFFu);
    arm(-4 + lean, -2 + b, -sw * 0.8f * a->mv - 0.2f, 0.5f, 9, 1.6f, bone, 0);
    leg(2.5f, 4, a->run, STR(a, 4.5f), 3.0f, 7, 1.8f, bone, 0x3A2A1EFFu);
    /* pelvis, spine, ribs */
    E(0 + lean * 0.3f, 4 + b, 4, 1.8f, 6, bone);
    Lf(lean * 0.5f, 3 + b, lean, -3 + b, 1.4f, bone);
    float rib[10] = { -5 + lean, -4 + b, 5 + lean, -4 + b, 4 + lean, 2 + b, 0 + lean, 3 + b, -4 + lean, 2 + b };
    P(rib, 5, bone);
    for (int k = 0; k < 3; k++) Lf(-4 + lean, -2.5f + (float)k * 1.6f + b, 4 + lean, -2.5f + (float)k * 1.6f + b, 0.7f, 0x3A2A2AFFu);
    /* tattered bandana flapping */
    float fl = sinf(a->t * 13.0f) * 1.5f;
    Rf(-5 + lean, -5 + b, 5 + lean, -2.5f + b, 0x8A2420FFu);
    Pf((float[6]){ -4 + lean, -4 + b, -9 + lean - a->mv * 2, -2 + b + fl, -4 + lean, -2 + b }, 3, 0x8A2420FFu);
    wounds(a, -4 + lean, -3 + b, 4 + lean, 2 + b, blood);
    arm(4 + lean, -2 + b, sw * 0.8f * a->mv + 0.4f + a->atk * 0.8f, 0.7f, 9, 1.6f, bone, bone);
    /* skull a beat behind */
    float hb = -fabsf(sinf(a->run - 0.5f)) * a->mv * 2.2f;
    skull_a(1 + lean * 1.4f, -10 + hb, 5.4f, 0xEDE2C8FFu, eye, a, 0.25f + 0.5f * fabsf(sinf(a->t * 9.0f)) * a->rage + a->hurt * 0.6f);
    E(1 + lean * 1.4f, -15 + hb, 9.5f, 2.4f, 10, 0x4E3420FFu);
    R(-3 + lean * 1.4f, -21 + hb, 5 + lean * 1.4f, -15 + hb, 0x5E4026FFu);
    Rf(-3 + lean * 1.4f, -16.8f + hb, 5 + lean * 1.4f, -15.4f + hb, 0x2A1A10FFu);
}

static void a_buzzard(const Anim *a, uint32_t eye, uint32_t blood)
{
    float f = sinf(a->t * 13.0f + (float)(a->seed & 7));
    float dive = a->atk;
    /* ragged wings: three feather fingers each */
    for (int side = -1; side <= 1; side += 2) {
        float wy = -4 - f * 7, sx = (float)side;
        float w[8] = { 2 * sx, -1, 15 * sx, wy, 13 * sx, wy + 4, 3 * sx, 4 };
        P(w, 4, 0x3E2A1EFFu);
        for (int k = 0; k < 3; k++) Lf(12 * sx - (float)k * 2 * sx, wy + 1 + (float)k, 17 * sx - (float)k * 2.4f * sx, wy + 3 + (float)k * 2, 1.2f, 0x2A1A12FFu);
    }
    E3(0, 1, 6, 7.5f, 8, 0x4A3222FFu);
    wounds(a, -4, -2, 4, 5, blood);
    /* the bald neck stretches out on a dive */
    float nx = 2 + dive * 3, ny = -7 + dive * 2;
    L(1, -3, nx, ny, 2.6f, 0xC08080FFu);
    E3(nx, ny, 3.4f, 3.0f, 8, 0xC88A84FFu);
    float open = 0.2f + 0.6f * fmaxf(dive, a->hurt);
    Pf((float[6]){ nx + 2, ny - 1.4f, nx + 8, ny + 0.5f, nx + 2, ny + 0.4f }, 3, 0xD8B040FFu);
    Pf((float[6]){ nx + 2, ny + 0.6f + open, nx + 7, ny + 1.4f + open * 2, nx + 2, ny + 1.6f + open }, 3, 0xB89030FFu);
    Ef(nx + 0.8f, ny - 1, 1.0f, 1.0f, 5, a->hurt > 0.3f ? 0x200808FFu : eye);
}

static void a_cactus(const Anim *a, uint32_t eye, uint32_t blood)
{
    float b = BOB(a, 1.5f), sw = SW(a);
    leg(-4, 8, a->run + PI_F, STR(a, 3.0f), 2.5f, 4, 3.2f, 0x3E6A2EFFu, 0);
    leg(4, 8, a->run, STR(a, 3.0f), 2.5f, 4, 3.2f, 0x3E6A2EFFu, 0);
    float body[12] = { -7, 9 + b, 7, 9 + b, 8, -8 + b, 5, -16 + b, -5, -16 + b, -8, -8 + b };
    P3(body, 6, 0x4E7A36FFu);
    for (int k = 0; k < 3; k++) Lf(-3 + (float)k * 3, -14 + b, -3 + (float)k * 3.3f, 8 + b, 0.9f, 0x2E5A24FFu);
    /* arms raised and swinging */
    L(-7, -4 + b, -13, -4 + b + sw * 2, 3.2f, 0x447034FFu); L(-13, -4 + b + sw * 2, -13, -11 + b + sw * 2, 3.0f, 0x447034FFu);
    L(7, -2 + b, 13, -2 + b - sw * 2, 3.2f, 0x447034FFu); L(13, -2 + b - sw * 2, 13 + a->atk * 2, -9 + b - sw * 2, 3.0f, 0x447034FFu);
    for (int k = 0; k < 8; k++) { uint32_t h = hash32((uint32_t)k * 77u); Ef(-6 + (float)(h & 15) * 0.8f, -13 + (float)((h >> 4) & 15) * 1.3f + b, 0.6f, 0.6f, 4, 0xE8E0B0FFu); }
    wounds(a, -5, -10 + b, 5, 6 + b, blood);
    /* a carved, furious face */
    face_eyes(0, -9 + b, 6.0f, 1.7f, 0x140C0AFFu, eye, a, 0.9f, 0.55f);
    mouth(0.5f, -3 + b, 6, 0.3f + 0.5f * a->rage * fabsf(sinf(a->t * 4)) + a->hurt * 0.6f, 4, 0);
    E(0, -18 + b + sinf(a->t * 3) * 0.6f, 3, 2.2f, 7, 0xC8508AFFu);
    Ef(0, -18 + b, 1.0f, 0.8f, 5, 0xF0D040FFu);
}

static void a_slinger(const Anim *a, uint32_t eye, uint32_t blood)
{
    float b = BOB(a, 1.6f), sw = SW(a);
    uint32_t bone = 0xD8CCB0FFu;
    leg(-2.5f, 5, a->run + PI_F, STR(a, 4), 2.5f, 6, 1.7f, bone, 0x3A2A1EFFu);
    leg(2.5f, 5, a->run, STR(a, 4), 2.5f, 6, 1.7f, bone, 0x3A2A1EFFu);
    float pon[8] = { -9, 6 + b + sw * 0.5f, 9, 6 + b - sw * 0.5f, 5, -5 + b, -5, -5 + b };
    P3(pon, 4, 0xB0582AFFu);
    Lf(-8, 3 + b, 8, 3 + b, 1.2f, 0xD8B050FFu);
    Lf(-6, 0 + b, 6, 0 + b, 1.2f, 0x6A2416FFu);
    for (int k = 0; k < 5; k++) Lf(-8 + (float)k * 4, 6 + b, -8.5f + (float)k * 4, 8 + b, 0.8f, 0xB0582AFFu);   /* fringe */
    wounds(a, -5, -3 + b, 5, 4 + b, blood);
    /* the throw: wind up behind the head, then whip */
    float wind = a->atk;
    float aa = wind > 0 ? 2.8f * wind : 0.3f + sw * 0.4f * a->mv;
    arm(5, -2 + b, aa, 0.4f, 9, 1.6f, bone, 0);
    float hx = 5 + sinf(aa) * 4.5f + sinf(aa + 0.4f) * 4.5f, hy = -2 + b + cosf(aa) * 4.5f + cosf(aa + 0.4f) * 4.5f;
    L(hx - 2, hy - 1, hx + 2, hy + 1, 1.6f, 0xEDE4D0FFu);
    float hb = -fabsf(sinf(a->run - 0.5f)) * a->mv * 2.0f;
    skull_a(0.5f, -10 + hb, 5.1f, 0xEDE2C8FFu, eye, a, 0.2f + 0.6f * wind + a->hurt * 0.5f);
    E(0.5f, -16 + hb, 10.5f, 2.3f, 10, 0x6A4A22FFu);
    E3(0.5f, -18 + hb, 4.6f, 3.3f, 8, 0x7A5A2AFFu);
}

/* ROTWOOD BAYOU ---------------------------------------------------- */
static void a_toad(const Anim *a, uint32_t eye, uint32_t blood, float hop)
{
    float sq = 1.0f - hop * 0.18f, st = 1.0f + hop * 0.12f;      /* stretch in the air, squash on landing */
    float land = hop < 0.05f ? 0.15f : 0.0f;
    OY -= hop * 11.0f * S;
    /* legs folded, kicking out in the air */
    L(-9, 5, -12 - hop * 4, 9 + hop * 3, 3.0f, 0x3E6A30FFu); L(9, 5, 12 + hop * 4, 9 + hop * 3, 3.0f, 0x3E6A30FFu);
    float body[12] = { -12 * st, 8, 12 * st, 8, 13 * st, -3 * sq, 6, -10 * sq, -6, -10 * sq, -13 * st, -3 * sq };
    P3(body, 6, 0x4A7236FFu);
    Ef(0, 4, 9 * st, 3.2f, 8, 0x8AA060FFu);                                     /* pale belly */
    for (int k = 0; k < 5; k++) { uint32_t h = hash32((uint32_t)k * 31u + a->seed); Ef(-9 + (float)(h & 15) * 1.2f, -6 + (float)((h >> 4) & 7), 1.3f, 1.0f, 5, 0x2E4A22FFu); }   /* warts */
    wounds(a, -8, -6, 8, 4, blood);
    /* the huge mouth gapes on landing */
    float open = fmaxf(0.2f, fmaxf(land, a->hurt * 0.9f) + a->rage * 0.3f * fabsf(sinf(a->t * 2.5f)));
    mouth(0.5f, 2.2f, 19, open, 7, 0);
    /* bulging eyes on top, blinking out of step */
    Anim e1 = *a; e1.blink = (fmodf(a->t + (float)(a->seed & 15) * 0.2f, 2.6f) < 0.12f) ? 1.0f : 0.0f;
    E3(-5.5f, -8 * sq, 3.6f, 3.4f, 8, 0xD8C040FFu); E3(5.5f, -8 * sq, 3.6f, 3.4f, 8, 0xD8C040FFu);
    face_eyes(0, -8 * sq, 11, 2.6f, 0xE8D050FFu, 0x140C08FFu, &e1, 0.5f, 0.45f);
    (void)eye;
    OY += hop * 11.0f * S;
}

static void a_crow(const Anim *a, uint32_t eye, uint32_t blood)
{
    float f = sinf(a->t * 16.0f + (float)(a->seed & 7));
    for (int side = -1; side <= 1; side += 2) {
        float wy = -4 - f * 7, sx = (float)side;
        float w[8] = { 2 * sx, -1, 15 * sx, wy, 12 * sx, wy + 5, 3 * sx, 4 };
        P(w, 4, 0x182420FFu);
        for (int k = 0; k < 3; k++) Lf(12 * sx - (float)k * 2 * sx, wy + 1 + (float)k, 17 * sx - (float)k * 2.4f * sx, wy + 3 + (float)k * 2, 1.0f, 0x10181AFFu);
    }
    E3(0, 0, 5.5f, 7, 8, 0x1C2A24FFu);
    wounds(a, -3, -2, 3, 4, blood);
    E3(1.5f, -7, 3.6f, 3.3f, 8, 0x1C2A24FFu);
    float open = 0.2f + 0.7f * fmaxf(a->atk, a->hurt) + 0.3f * fabsf(sinf(a->t * 7));
    Pf((float[6]){ 4, -8.3f, 10, -7.0f, 4, -6.8f }, 3, 0x7A7A40FFu);
    Pf((float[6]){ 4, -6.6f + open, 9, -5.8f + open * 1.6f, 4, -5.6f + open }, 3, 0x5A5A30FFu);
    Ef(2.6f, -8, 1.1f, 1.1f, 5, a->hurt > 0.3f ? 0x200808FFu : eye);
}

static void a_moss(const Anim *a, uint32_t eye, uint32_t blood)
{
    float b = BOB(a, 2.5f), sw = SW(a);
    leg(-6, 6, a->run + PI_F, STR(a, 3.5f), 3, 6, 4.0f, 0x3A4630FFu, 0x2A3222FFu);
    leg(6, 6, a->run, STR(a, 3.5f), 3, 6, 4.0f, 0x3A4630FFu, 0x2A3222FFu);
    arm(-14, -6 + b, -sw * 0.5f * a->mv, 0.3f, 16, 4.5f, 0x4A5A3EFFu, 0x3A4630FFu);   /* knuckles dragging */
    float body[12] = { -15, 8 + b, 15, 8 + b, 17, -5 + b, 9, -16 + b, -9, -16 + b, -17, -5 + b };
    P3(body, 6, 0x4E5C40FFu);
    Ef(-6, -10 + b, 6, 3.5f, 8, 0x4A7A30FFu); Ef(8, -11 + b, 5, 3, 8, 0x5A8A38FFu); Ef(0, 5 + b, 9, 2.5f, 8, 0x3E6A2AFFu);
    float pul = 0.6f + 0.4f * sinf(a->t * 3.0f);
    Lf(-8, -2 + b, -2, 4 + b, 1.2f, col_mix(0x3A5A20FFu, 0x9CFF60FFu, pul)); Lf(4, -6 + b, 9, 2 + b, 1.2f, col_mix(0x3A5A20FFu, 0x9CFF60FFu, pul));
    wounds(a, -10, -8 + b, 10, 5 + b, blood);
    /* a cave of a face: deep sockets, a slit that opens into a roar */
    Ef(0, -6 + b, 8, 4, 8, 0x1A2014FFu);
    face_eyes(0, -7 + b, 8, 1.6f, 0x0A0C08FFu, eye, a, 0.6f, 0.6f);
    mouth(0, -2.5f + b, 7, 0.1f + a->rage * 0.4f * fabsf(sinf(a->t * 1.5f)) + a->hurt * 0.7f, 5, 0);
    arm(14, -6 + b, sw * 0.5f * a->mv + a->atk, 0.3f, 16, 4.5f, 0x4A5A3EFFu, 0x3A4630FFu);
}

static void a_lantern(const Anim *a, uint32_t eye, uint32_t blood)
{
    float b = BOB(a, 1.2f), sw = SW(a);
    /* shuffling feet under the hem */
    Ef(-3 + sw * 2 * a->mv, 11, 2.2f, 1.2f, 5, 0x141018FFu); Ef(3 - sw * 2 * a->mv, 11, 2.2f, 1.2f, 5, 0x141018FFu);
    float robe[10] = { -10 + sw, 11, 10 - sw, 11, 6, -6 + b, 0, -8 + b, -6, -6 + b };
    P3(robe, 5, 0x34264AFFu);
    for (int k = 0; k < 4; k++) Pf((float[6]){ -9 + (float)k * 5, 11, -7 + (float)k * 5, 13 + (float)(k & 1), -5 + (float)k * 5, 11 }, 3, 0x2A1E3AFFu);   /* ragged hem */
    Lf(0, -4 + b, 0, 10, 1.0f, 0x221830FFu);
    wounds(a, -5, -4 + b, 5, 7, blood);
    /* hood and beak mask */
    E3(0, -9 + b, 5.2f, 5.2f, 8, 0x241A32FFu);
    Ef(1, -9 + b, 3.6f, 3.8f, 8, 0x0E0A12FFu);
    face_eyes(1.5f, -10 + b, 3.6f, 1.2f, 0x2A3A20FFu, eye, a, 0.0f, 0.6f);
    float beak[6] = { 3, -10 + b, 12, -6 + b + a->hurt * 2, 3, -6.5f + b };
    P(beak, 3, 0xD8CCB4FFu);
    Lf(4, -8.5f + b, 10, -6.5f + b, 0.6f, 0x8A7A60FFu);
    E(0, -15 + b, 8, 2.0f, 10, 0x100C14FFu);
    R(-4, -22 + b, 4, -15 + b, 0x100C14FFu);
    /* the lantern swings on its chain */
    float pend = sinf(a->t * 3.0f) * 0.5f + a->mv * 0.3f * sw;
    float lx = 7 + sinf(pend) * 6, ly = -2 + b + cosf(pend) * 6;
    L(7, -2 + b, lx, ly, 0.8f, 0x3A3030FFu);
    E(lx, ly + 2, 2.8f, 3.4f, 8, 0x40FF80FFu);
    Ef(lx, ly + 2, 1.4f, 2.0f, 6, 0xE0FFE0FFu);
}

/* GRAVEWATCH ------------------------------------------------------- */
static void a_zombie(const Anim *a, uint32_t eye, uint32_t blood)
{
    /* a limp: the bad leg drags */
    float b = BOB(a, 2.0f) + sinf(a->run * 0.5f) * 0.6f, sw = SW(a);
    uint32_t skin = 0x7E9468FFu, shirt = 0x3A4458FFu;
    leg(-2.5f, 4, a->run + PI_F, STR(a, 2.0f), 1.0f, 7, 2.4f, 0x2E3040FFu, 0x1E1A18FFu);
    leg(2.5f, 4, a->run, STR(a, 4.0f), 3.0f, 7, 2.4f, 0x2E3040FFu, 0x1E1A18FFu);
    float body[10] = { -6, 6 + b, 6, 6 + b, 7, -4 + b, 0, -6 + b, -6, -4 + b };
    P3(body, 5, shirt);
    Pf((float[8]){ -5, 1 + b, -1, -1 + b, -1, 4 + b, -5, 5 + b }, 4, skin);                 /* torn shirt */
    for (int k = 0; k < 3; k++) Lf(-4.5f, 1.5f + (float)k * 1.1f + b, -1.5f, 0.8f + (float)k * 1.1f + b, 0.5f, 0x6A1A1AFFu);   /* ribs through it */
    wounds(a, -5, -3 + b, 5, 5 + b, blood);
    /* arms out, reaching, grabbing */
    float g = sinf(a->t * 5.0f) * 0.15f;
    arm(-4, -3 + b, 1.35f + g - sw * 0.1f, -0.2f, 11, 2.2f, skin, skin);
    arm(4, -3 + b, 1.5f - g + sw * 0.1f, -0.15f, 11, 2.2f, skin, skin);
    /* the head lolls */
    float tilt = sinf(a->t * 2.0f) * 1.2f, hb = -fabsf(sinf(a->run - 0.7f)) * a->mv * 2.2f;
    E3(1 + tilt * 0.3f, -10 + hb, 5.6f, 5.4f, 10, skin);
    Ef(-2 + tilt * 0.3f, -13 + hb, 3.2f, 1.5f, 6, 0x5E7450FFu);
    Ef(-3, -12.5f + hb, 1.2f, 1.6f, 5, 0x6A1010FFu);                                      /* a scalp wound */
    Anim e = *a; e.lookx *= 0.5f;
    float hx = 1.6f + tilt * 0.3f, hy = -10.5f + hb;
    Ef(hx - 1.7f, hy, 1.7f, 1.7f, 6, 0xD8D0A8FFu); Ef(hx + 2.2f, hy - 0.2f, 1.3f, 1.3f, 6, 0xD8D0A8FFu);   /* mismatched eyes */
    if (a->hurt < 0.35f) { Ef(hx - 1.7f + a->lookx * 0.5f, hy, 0.7f, 0.7f, 5, eye); Ef(hx + 2.2f + a->lookx * 0.4f, hy - 0.2f, 0.6f, 0.6f, 5, eye); }
    else face_eyes(hx, hy, 3.8f, 1.5f, 0, 0, &e, 0, 0.5f);
    mouth(hx + 0.5f, hy + 3.4f, 4.5f, 0.45f + 0.35f * fabsf(sinf(a->t * 3.0f)) + a->hurt * 0.4f, 4, 0);
}

static void a_bones(const Anim *a, uint32_t eye, uint32_t blood)
{
    float b = BOB(a, 2.0f), sw = SW(a), lean = a->mv * 2.5f;
    uint32_t bone = 0xE0D8C8FFu;
    leg(-2, 3, a->run + PI_F, STR(a, 4.0f), 2.5f, 6, 1.3f, bone, 0);
    arm(-3 + lean, -2 + b, -sw * 1.1f * a->mv, 0.6f, 7, 1.2f, bone, 0);
    leg(2, 3, a->run, STR(a, 4.0f), 2.5f, 6, 1.3f, bone, 0);
    E(lean * 0.3f, 3 + b, 3, 1.4f, 6, bone);
    float rib[8] = { -4 + lean, -3 + b, 4 + lean, -3 + b, 3 + lean, 2 + b, -3 + lean, 2 + b };
    P(rib, 4, bone);
    Lf(-3 + lean, -1.3f + b, 3 + lean, -1.3f + b, 0.6f, 0x3A3030FFu); Lf(-3 + lean, 0.4f + b, 3 + lean, 0.4f + b, 0.6f, 0x3A3030FFu);
    wounds(a, -3 + lean, -2 + b, 3 + lean, 1 + b, blood);
    arm(3 + lean, -2 + b, sw * 1.1f * a->mv + 0.5f, 0.6f, 7, 1.2f, bone, 0);
    skull_a(1 + lean * 1.2f, -8 + b, 4.4f, 0xEDE6D8FFu, eye, a, 0.2f + 0.6f * fabsf(sinf(a->t * 14.0f)));
}

static void a_coffin(const Anim *a, uint32_t eye, uint32_t blood)
{
    float b = BOB(a, 1.3f);
    leg(-4, 6, a->run + PI_F, STR(a, 3.0f), 2, 6, 2.6f, 0x3A3A44FFu, 0x1A1A20FFu);
    leg(4, 6, a->run, STR(a, 3.0f), 2, 6, 2.6f, 0x3A3A44FFu, 0x1A1A20FFu);
    float lid[12] = { -8, -18 + b, 8, -18 + b, 12, -8 + b, 9, 10 + b, -9, 10 + b, -12, -8 + b };
    P3(lid, 6, 0x3A3640FFu);
    float in[12] = { -6, -15 + b, 6, -15 + b, 9, -8 + b, 7, 7 + b, -7, 7 + b, -9, -8 + b };
    Pf(in, 6, 0x4A4452FFu);
    for (int k = 0; k < 4; k++) Lf(-6, -12 + (float)k * 5 + b, 6, -11 + (float)k * 5 + b, 0.5f, 0x3A3440FFu);   /* planks */
    Lf(0, -12 + b, 0, 3 + b, 2.0f, 0x8A8494FFu); Lf(-5, -7 + b, 5, -7 + b, 2.0f, 0x8A8494FFu);
    wounds(a, -7, -14 + b, 7, 6 + b, blood);
    /* the slit: eyes glaring through */
    Rf(-6, -12 + b, 6, -9 + b, 0x050405FFu);
    face_eyes(0, -10.5f + b, 6, 1.2f, 0x050405FFu, eye, a, 0.8f, 0.7f);
    /* grey fingers curled round the edges */
    for (int k = 0; k < 3; k++) { Ef(11, 1 + (float)k * 1.6f + b, 1.6f, 0.8f, 5, 0x7A9068FFu); Ef(-11, 1 + (float)k * 1.6f + b, 1.6f, 0.8f, 5, 0x7A9068FFu); }
}

static void a_wraith(const Anim *a, uint32_t eye, uint32_t blood, float phase)
{
    ALPHA = phase;
    float b = sinf(a->t * 4.0f) * 2.5f;
    for (int k = 0; k < 3; k++) {        /* tattered trailing wisps */
        float wx = -5 + (float)k * 5, ph = a->t * 7 + (float)k;
        float tail[8] = { wx - 3, -2 + b, wx + 3, -2 + b, wx + 1 + sinf(ph) * 3 - a->mv * 3, 14, wx - 2 + sinf(ph + 1) * 3 - a->mv * 3, 13 };
        P(tail, 4, 0xC8D8E0FFu);
    }
    E3(0, -6 + b, 7, 7.5f, 10, 0xE4ECF0FFu);
    /* arms reaching out, fingers spread */
    float r = sinf(a->t * 3) * 0.2f;
    arm(-5, -3 + b, 1.2f + r, -0.3f, 9, 1.4f, 0xD8E4EAFFu, 0);
    arm(5, -3 + b, 1.4f - r + a->atk * 0.5f, -0.3f, 9, 1.4f, 0xD8E4EAFFu, 0);
    wounds(a, -4, -9 + b, 4, -2 + b, blood);
    /* black hollow eyes and a mouth that stretches into a scream */
    Ef(-2.8f, -8 + b, 2.0f, 2.8f, 6, 0x0C1418FFu); Ef(2.8f, -8 + b, 2.0f, 2.8f, 6, 0x0C1418FFu);
    if (a->hurt < 0.35f) { Ef(-2.8f + a->lookx * 0.6f, -8 + b, 0.7f, 0.7f, 5, eye); Ef(2.8f + a->lookx * 0.6f, -8 + b, 0.7f, 0.7f, 5, eye); }
    float scream = 0.4f + 0.4f * fabsf(sinf(a->t * 2.0f)) + a->atk * 0.5f + a->hurt;
    Ef(0, -2.5f + b + scream, 1.8f, 1.5f + scream * 2.2f, 7, 0x0C1418FFu);
    ALPHA = 1.0f;
}

/* ASHEN HOLLOW ----------------------------------------------------- */
static void a_husk(const Anim *a, uint32_t eye, uint32_t blood)
{
    float b = BOB(a, 1.8f), sw = SW(a);
    float pul = 0.55f + 0.45f * sinf(a->t * 4.0f + (float)(a->seed & 7));
    uint32_t ember = col_mix(0x8A2008FFu, 0xFFA030FFu, pul), char_ = 0x221A18FFu;
    leg(-2.5f, 4, a->run + PI_F, STR(a, 3.5f), 2.5f, 7, 2.5f, char_, 0);
    arm(-4, -3 + b, -sw * 0.7f * a->mv - 0.3f, 0.5f, 10, 2.2f, char_, char_);
    leg(2.5f, 4, a->run, STR(a, 3.5f), 2.5f, 7, 2.5f, char_, 0);
    float body[10] = { -6, 6 + b, 6, 6 + b, 7, -4 + b, 0, -6 + b, -6, -4 + b };
    P3(body, 5, 0x2A2220FFu);
    Lf(-4, -2 + b, 2, 4 + b, 1.1f, ember); Lf(3, -3 + b, 1, 3 + b, 0.9f, ember); Lf(-2, 2 + b, -5, 5 + b, 0.8f, ember);
    wounds(a, -5, -3 + b, 5, 4 + b, blood);
    arm(4, -3 + b, sw * 0.7f * a->mv + 0.6f + a->atk, 0.5f, 10, 2.2f, char_, char_);
    float hb = -fabsf(sinf(a->run - 0.5f)) * a->mv * 2.0f;
    E3(1, -10 + hb, 5.5f, 5.2f, 10, 0x2A2220FFu);
    Lf(-3, -13 + hb, 1, -9 + hb, 0.9f, ember);
    face_eyes(1.5f, -11 + hb, 4.2f, 1.4f, 0x0A0606FFu, eye, a, 0.7f, 0.65f);
    mouth(2, -7 + hb, 4.2f, 0.4f + a->rage * 0.4f * fabsf(sinf(a->t * 2)) + a->hurt * 0.5f, 0, 0);
    Ef(2, -7 + hb, 1.6f, 0.8f + a->rage * 0.6f, 6, ember);                       /* fire inside */
}

static void a_imp(const Anim *a, uint32_t eye, uint32_t blood)
{
    float b = BOB(a, 2.5f), sw = SW(a), lean = a->mv * 2.0f;
    float fl = sinf(a->t * 22.0f) * 1.5f;
    leg(-2, 4, a->run + PI_F, STR(a, 3.5f), 2.5f, 5, 1.6f, 0xB02818FFu, 0x2A0A0AFFu);
    leg(2, 4, a->run, STR(a, 3.5f), 2.5f, 5, 1.6f, 0xB02818FFu, 0x2A0A0AFFu);
    /* whipping tail */
    float tx = -6 - sinf(a->t * 9) * 2, ty = 3 + cosf(a->t * 9) * 2;
    L(-3, 2 + b, tx, ty + b, 1.0f, 0xB02818FFu);
    Pf((float[6]){ tx - 1.5f, ty + b - 1, tx - 3.5f, ty + b + 0.5f, tx - 1, ty + b + 1.5f }, 3, 0x2A0A0AFFu);
    Pf((float[6]){ -4 + lean, -6 + b, fl + lean, -17 + b - a->mv * 2, 4 + lean, -6 + b }, 3, 0xFFB030FFu);
    Pf((float[6]){ -2 + lean, -6 + b, fl * 0.6f + lean, -13 + b, 2 + lean, -6 + b }, 3, 0xFFF080FFu);
    E3(lean, 0 + b, 6, 6.5f, 10, 0xD02E1EFFu);
    wounds(a, -4 + lean, -2 + b, 4 + lean, 4 + b, blood);
    P((float[6]){ -5 + lean, -4 + b, -7 + lean, -10 + b, -2 + lean, -5 + b }, 3, 0x2A0A0AFFu);
    P((float[6]){ 5 + lean, -4 + b, 7 + lean, -10 + b, 2 + lean, -5 + b }, 3, 0x2A0A0AFFu);
    face_eyes(1.2f + lean, -1.5f + b, 4.6f, 1.5f, 0xFFF080FFu, 0x200606FFu, a, 1.0f, 0.5f);
    /* the grin: ear to ear */
    mouth(1.2f + lean, 2.6f + b, 7.0f, 0.3f + a->hurt * 0.6f + 0.2f * fabsf(sinf(a->t * 10)), 6, 0);
    arm(4 + lean, 0 + b, sw * 0.9f * a->mv + 0.8f, 0.5f, 6, 1.4f, 0xB02818FFu, 0x2A0A0AFFu);
}

static void a_hulk(const Anim *a, uint32_t eye, uint32_t blood)
{
    float b = BOB(a, 2.2f), sw = SW(a);
    leg(-6, 5, a->run + PI_F, STR(a, 3.5f), 3, 7, 4.5f, 0x3A3432FFu, 0x1A1614FFu);
    leg(6, 5, a->run, STR(a, 3.5f), 3, 7, 4.5f, 0x3A3432FFu, 0x1A1614FFu);
    arm(-15, -6 + b, -sw * 0.6f * a->mv - 0.2f, 0.6f, 15, 5.0f, 0x46403EFFu, 0);
    E(-15 + sinf(-sw * 0.6f * a->mv - 0.2f) * 7 + sinf(-sw * 0.6f * a->mv + 0.4f) * 7, -6 + b + 13, 4.8f, 4.8f, 8, 0xFF5A1AFFu);
    float body[12] = { -15, 6 + b, 15, 6 + b, 17, -6 + b, 10, -15 + b, -10, -15 + b, -17, -6 + b };
    P3(body, 6, 0x4A4442FFu);
    float pul = 0.6f + 0.4f * sinf(a->t * 2.5f);
    uint32_t ember = col_mix(0x7A2008FFu, 0xFF6018FFu, pul);
    Lf(-10, -6 + b, -2, 2 + b, 1.5f, ember); Lf(6, -10 + b, 11, 0 + b, 1.5f, ember); Lf(-4, -12 + b, 2, -8 + b, 1.2f, ember);
    wounds(a, -11, -11 + b, 11, 3 + b, blood);
    /* the small head sunk into the shoulders, roaring */
    E3(1, -15 + b, 6.5f, 5.5f, 10, 0x3A3432FFu);
    face_eyes(1.5f, -16.5f + b, 5.4f, 1.6f, 0x100806FFu, eye, a, 1.0f, 0.65f);
    float roar = a->rage * 0.6f * fabsf(sinf(a->t * 1.3f)) + a->atk + a->hurt * 0.5f;
    mouth(2, -12 + b, 6, roar, 5, 0);
    if (roar > 0.3f) Ef(2, -12 + b, 1.8f, roar * 1.6f, 6, ember);
    float as = sw * 0.6f * a->mv + 0.3f + a->atk * 1.2f;
    arm(15, -6 + b, as, 0.6f, 15, 5.0f, 0x46403EFFu, 0);
    E(15 + sinf(as) * 7 + sinf(as + 0.6f) * 7, -6 + b + cosf(as) * 7 + cosf(as + 0.6f) * 7, 4.8f, 4.8f, 8, 0xFF5A1AFFu);
}

static void a_doctor(const Anim *a, uint32_t eye, uint32_t blood)
{
    float b = BOB(a, 1.4f), sw = SW(a);
    leg(-2.5f, 4, a->run + PI_F, STR(a, 4.0f), 2.5f, 7, 1.8f, 0x141014FFu, 0x0A080AFFu);
    leg(2.5f, 4, a->run, STR(a, 4.0f), 2.5f, 7, 1.8f, 0x141014FFu, 0x0A080AFFu);
    /* the coat billows behind */
    float bil = a->mv * 3.0f + sinf(a->t * 6.0f) * 1.2f;
    float coat[10] = { -10 - bil, 10, 9, 9, 6, -6 + b, 0, -8 + b, -6, -6 + b };
    P3(coat, 5, 0x1C181EFFu);
    Lf(0, -4 + b, 0, 8, 0.8f, 0x34303CFFu);
    for (int k = 0; k < 3; k++) Ef(1.5f, -3 + (float)k * 3 + b, 0.6f, 0.6f, 4, 0x8A7A50FFu);
    wounds(a, -5, -4 + b, 5, 6, blood);
    E3(0, -9 + b, 5, 5, 8, 0x262028FFu);
    float beak[6] = { 2, -11 + b, 15, -4 + b + a->hurt * 2, 2, -6 + b };
    P3(beak, 3, 0xE0D6C4FFu);
    Lf(3, -8.5f + b, 13, -4.5f + b, 0.5f, 0x8A7A60FFu);
    E(-0.5f, -11 + b, 2.2f, 2.2f, 8, 0x3A5A2AFFu); E(3.2f, -11 + b, 2.2f, 2.2f, 8, 0x3A5A2AFFu);   /* goggle lenses */
    face_eyes(1.35f, -11 + b, 3.7f, 1.1f, 0x5A8A3AFFu, eye, a, 0, 0.5f);
    E(0, -15 + b, 8.5f, 2.0f, 10, 0x100C10FFu);
    R(-4.5f, -24 + b, 4.5f, -15 + b, 0x100C10FFu);
    Rf(-4.5f, -17 + b, 4.5f, -15.8f + b, 0x5A1818FFu);
    /* the vial: raised to throw */
    float ra = 0.5f + sw * 0.3f * a->mv + a->atk * 2.2f;
    arm(5, -3 + b, ra, 0.5f, 9, 1.5f, 0x1C181EFFu, 0);
    float vx = 5 + sinf(ra) * 4.5f + sinf(ra + 0.5f) * 4.5f, vy = -3 + b + cosf(ra) * 4.5f + cosf(ra + 0.5f) * 4.5f;
    E(vx, vy, 1.8f, 2.6f, 6, 0x70FF40FFu);
}

/* HELLMOUTH -------------------------------------------------------- */
static void a_crawler(const Anim *a, uint32_t eye, uint32_t blood)
{
    float b = sinf(a->run * 2.0f) * 1.0f * a->mv;
    for (int k = 0; k < 4; k++) {          /* eight legs, each on its own beat */
        float ph = a->run * 2.0f + (float)k * 1.6f, lift = fmaxf(0, cosf(ph)) * 3.0f * a->mv;
        float off = sinf(ph) * 3.0f * a->mv, yy = (float)k * 2.6f - 3.0f;
        float kx = 9 + (float)k * 0.5f, ky = yy - 5 - lift;
        L(-3, yy * 0.4f, -kx, ky, 1.5f, 0x100A12FFu); L(-kx, ky, -kx - 3 + off, yy + 5, 1.2f, 0x100A12FFu);
        L(3, yy * 0.4f, kx, ky, 1.5f, 0x100A12FFu); L(kx, ky, kx + 3 - off, yy + 5, 1.2f, 0x100A12FFu);
    }
    E3(0, -2 + b, 10, 7, 10, 0x1A1020FFu);
    Ef(-2, -5 + b, 6, 2.5f, 8, 0x2E1A34FFu);
    wounds(a, -6, -5 + b, 6, 2 + b, blood);
    /* a cluster of eyes, blinking out of turn */
    static const float EY[7][3] = { {-5, -3, 1.2f}, {-1, -4.5f, 1.5f}, {3, -4, 1.3f}, {6, -1.5f, 1.0f}, {-3, 0, 1.0f}, {2, -0.5f, 1.4f}, {0, 2, 0.9f} };
    for (int k = 0; k < 7; k++) {
        int shut = a->hurt > 0.35f || fmodf(a->t * 1.3f + (float)k * 0.77f + (float)(a->seed & 7), 3.0f) < 0.15f;
        if (shut) Lf(EY[k][0] - EY[k][2], EY[k][1] + b, EY[k][0] + EY[k][2], EY[k][1] + b, 0.6f, 0x0A0408FFu);
        else { Ef(EY[k][0], EY[k][1] + b, EY[k][2], EY[k][2], 5, eye); Ef(EY[k][0] + a->lookx * 0.4f, EY[k][1] + b, EY[k][2] * 0.4f, EY[k][2] * 0.4f, 4, 0x0A0004FFu); }
    }
    mouth(1, 3.5f + b, 6, 0.2f + 0.4f * fabsf(sinf(a->t * 6)), 6, 0);
}

static void a_eye(const Anim *a, uint32_t eye, uint32_t blood)
{
    float b = sinf(a->t * 6.0f) * 3.0f;
    float fl[8] = { -5, 0 + b, 5, 0 + b, 2 + sinf(a->t * 9) * 2 - a->mv * 3, 12 + b, -3 + sinf(a->t * 11) * 2 - a->mv * 3, 13 + b };
    Pf(fl, 4, 0x8A28E0FFu);
    Pf((float[8]){ -3, 1 + b, 3, 1 + b, 1 + sinf(a->t * 9) * 1.5f, 8 + b, -2, 8 + b }, 4, 0xD080FFFFu);
    E3(0, -2 + b, 7.5f, 7.5f, 12, 0xF0E4E0FFu);
    for (int k = 0; k < 4; k++) { float an = (float)k * 1.6f + 0.4f; Lf(cosf(an) * 7, -2 + b + sinf(an) * 7, cosf(an) * 3.5f, -2 + b + sinf(an) * 3.5f, 0.5f, 0xC02838FFu); }
    wounds(a, -5, -6 + b, 5, 2 + b, blood);
    /* the iris tracks you; the pupil goes to a pinprick when hit */
    float px = a->lookx * 2.8f, py = a->looky * 2.0f;
    if (a->hurt > 0.35f) { Lf(-6, -2 + b, 6, -2 + b, 2.2f, 0xC02838FFu); return; }
    Ef(px, -2 + b + py, 3.6f, 3.6f, 10, 0xC01828FFu);
    float dil = 1.8f - a->atk * 0.9f;
    Ef(px + 0.3f, -2 + b + py, dil, dil * 1.1f, 8, 0x05000AFFu);
    Ef(px + 1.2f, -3.2f + b + py, 0.7f, 0.7f, 5, 0xFFFFFFFFu);
    /* the lid blinks */
    if (a->blink > 0.5f) Ef(0, -2 + b, 7.5f, 7.5f, 12, 0xB04A5AFFu);
    (void)eye;
}

static void a_flesh(const Anim *a, uint32_t eye, uint32_t blood)
{
    float p = sinf(a->t * 3.5f) * 1.5f, sw = SW(a);
    leg(-7, 7, a->run + PI_F, STR(a, 3.0f), 2.5f, 5, 3.5f, 0x8A3A48FFu, 0);
    leg(7, 7, a->run, STR(a, 3.0f), 2.5f, 5, 3.5f, 0x8A3A48FFu, 0);
    float body[14] = { -16 - p, 6, -10, 12, 10, 12, 16 + p, 6, 15, -8, 0, -16 - p, -15, -8 };
    P3(body, 7, 0xA84452FFu);
    Ef(-6, -8, 4, 3, 8, 0xC4687AFFu); Ef(8, -3, 3, 2.5f, 8, 0xC4687AFFu);
    for (int k = 0; k < 4; k++) Lf(-12 + (float)k * 7, -10 + (float)(k & 1) * 4, -9 + (float)k * 7, 8, 0.7f, 0x6A1A2AFFu);   /* veins */
    wounds(a, -12, -12, 12, 8, blood);
    /* the mouth chomps */
    float chomp = 0.3f + 0.7f * fabsf(sinf(a->t * 4.0f + sw)) * (0.4f + a->rage * 0.6f) + a->hurt * 0.4f;
    mouth(0, 2.5f, 18, chomp, 7, 0x6A1A2AFFu);
    if (chomp > 0.5f) Ef(4, 4 + chomp * 3, 3, 1.5f, 6, 0xD06070FFu);      /* tongue lolling out */
    face_eyes(-6, -9, 4.6f, 1.6f, 0xF0E0C0FFu, eye, a, 0.5f, 0.5f);
    Ef(9, -10, 1.7f, 1.7f, 6, 0xF0E0C0FFu); Ef(9 + a->lookx * 0.6f, -10, 0.8f, 0.8f, 5, eye);
}

static void a_priest(const Anim *a, uint32_t eye, uint32_t blood)
{
    float b = sinf(a->t * 3.0f) * 2.0f, cast = a->atk;
    float robe[8] = { -10, 12, 10, 12, 6, -10 + b, -6, -10 + b };
    P3(robe, 4, 0x2A1240FFu);
    for (int k = 0; k < 4; k++) Pf((float[6]){ -10 + (float)k * 5, 12, -7.5f + (float)k * 5, 15 + sinf(a->t * 5 + (float)k) * 1.5f, -5 + (float)k * 5, 12 }, 3, 0x200C32FFu);
    Lf(-6, 8, 6, 8, 1.0f, 0x9A38E0FFu);
    wounds(a, -6, -6 + b, 6, 8, blood);
    /* hands rise to cast */
    arm(-6, -6 + b, 0.4f + cast * 2.4f, 0.3f, 10, 1.8f, 0x2A1240FFu, 0x8A7090FFu);
    arm(6, -6 + b, 0.4f + cast * 2.4f, 0.3f, 10, 1.8f, 0x2A1240FFu, 0x8A7090FFu);
    float hood[8] = { -7, -6 + b, 7, -6 + b, 4, -20 + b, -4, -20 + b };
    P3(hood, 4, 0x2A1240FFu);
    Ef(0, -11 + b, 4, 5, 8, 0x040208FFu);
    face_eyes(0, -12 + b, 3.6f, 1.0f, 0x040208FFu, eye, a, 0.4f, 0.8f);
    if (cast > 0.2f) mouth(0, -8.5f + b, 2.6f, cast, 0, 0);
    for (int k = 0; k < 2; k++) { float an = a->t * 2.0f + (float)k * PI_F; E(cosf(an) * 11, -2 + b + sinf(an) * 4, 2.6f, 2.6f, 8, 0xB040FFFFu); }
}

/* ================================================================== */
/* bosses                                                               */
/* ================================================================== */
static void b_sheriff(const Anim *a, uint32_t eye, int state)
{
    float b = BOB(a, 2.5f), sw = SW(a);
    uint32_t bone = 0xDCD0B4FFu;
    leg(-6, 8, a->run + PI_F, STR(a, 6), 4, 14, 3.6f, bone, 0x3A2416FFu);
    leg(6, 8, a->run, STR(a, 6), 4, 14, 3.6f, bone, 0x3A2416FFu);
    float coat[10] = { -17 - a->mv * 3, 14 + b, 16, 13 + b, 12, -12 + b, 0, -14 + b, -12, -12 + b };
    P3(coat, 5, 0x6A4026FFu);
    Pf((float[8]){ -17 - a->mv * 3, 14 + b, -13, 14 + b, -15, 19 + b + sinf(a->t * 7), -19 - a->mv * 4, 18 + b }, 4, 0x5A341EFFu);
    Rf(-5, -12 + b, 5, 12 + b, bone);
    for (int k = 0; k < 4; k++) Lf(-4, -8 + (float)k * 4 + b, 4, -8 + (float)k * 4 + b, 1.0f, 0x3A2A2AFFu);
    Anim w = *a; w.wounds = a->wounds;
    wounds(&w, -10, -10 + b, 10, 10 + b, 0x6A0A0AFFu);
    float star[10];
    for (int k = 0; k < 5; k++) { float an = (float)k / 5.0f * TAU_F - PI_F / 2; star[k * 2] = -9 + cosf(an) * 3.8f; star[k * 2 + 1] = -5 + b + sinf(an) * 3.8f; }
    P(star, 5, 0xE8B830FFu);
    Rf(-16, 0 + b, 16, 3 + b, 0x3A2416FFu);                              /* gunbelt */
    float aim = (state == 3 || state == 1) ? -1.2f : 0.3f + sw * 0.3f * a->mv;
    arm(14, -9 + b, 1.0f - aim, 0.2f, 20, 3.4f, bone, bone);
    arm(-14, -9 + b, -1.0f + aim * 0.5f, -0.2f, 20, 3.4f, bone, bone);
    float g1x = 14 + sinf(1.0f - aim) * 10 + sinf(1.2f - aim) * 10, g1y = -9 + b + cosf(1.0f - aim) * 10 + cosf(1.2f - aim) * 10;
    R(g1x - 1, g1y - 3, g1x + 12, g1y + 1, 0x3A3A44FFu);
    float hb = -fabsf(sinf(a->run - 0.5f)) * a->mv * 3.0f;
    skull_a(1, -22 + hb, 10, 0xEDE2C8FFu, eye, a, 0.25f + 0.6f * a->atk + 0.3f * fabsf(sinf(a->t * 2)));
    E(1, -31 + hb, 22, 5, 14, 0x4A301EFFu);
    R(-9, -48 + hb, 11, -31 + hb, 0x5A3C24FFu);
    Rf(-9, -35 + hb, 11, -32 + hb, 0x8A2020FFu);
}

static void b_bogmother(const Anim *a, uint32_t eye, int state)
{
    float br = sinf(a->t * 2.5f) * 2.0f;
    float h = state == 6 ? 8.0f : 0.0f;
    OY -= h * S;
    L(-26, 10, -36, 18 + br, 6, 0x3E6A2EFFu); L(26, 10, 36, 18 - br, 6, 0x3E6A2EFFu);
    float body[12] = { -30, 16, 30, 16, 34, -4, 18, -24 - br, -18, -24 - br, -34, -4 };
    P3(body, 6, 0x4A7438FFu);
    E3(0, 6, 22, 10 + br * 0.5f, 14, 0xC8B478FFu);
    for (int k = 0; k < 9; k++) { uint32_t hh = hash32((uint32_t)k * 97u); E(-24 + (float)(hh & 31) * 1.5f, -18 + (float)((hh >> 5) & 15), 2.2f, 1.6f, 6, 0x34562AFFu); }
    Anim w = *a; wounds(&w, -24, -18, 24, 10, 0x3A5A10FFu);
    /* the mouth: a gulf */
    float open = 0.15f + 0.6f * a->atk + 0.25f * fabsf(sinf(a->t * 1.5f)) + a->hurt * 0.3f;
    mouth(0, 0, 48, open, 11, 0x3A5A2AFFu);
    if (open > 0.4f) L(6, 2, 14 + sinf(a->t * 10) * 4, 8 + open * 10, 3, 0xC85A6AFFu);    /* a tongue flicking */
    E3(-14, -20, 8, 7, 10, 0xD8C040FFu); E3(14, -20, 8, 7, 10, 0xD8C040FFu);
    face_eyes(0, -20, 28, 6, 0xE0C850FFu, 0x140C08FFu, a, 0.7f, 0.35f);
    for (int k = 0; k < 5; k++) L(-12 + (float)k * 6, -28 - br, -14 + (float)k * 7, -42 - br - (float)(k % 2) * 4 + sinf(a->t * 2 + (float)k), 1.8f, 0x7A8A3AFFu);
    (void)eye;
    OY += h * S;
}

static void b_undertaker(const Anim *a, uint32_t eye, int state)
{
    float b = BOB(a, 1.8f), sw = SW(a);
    leg(-4, 10, a->run + PI_F, STR(a, 6), 4, 15, 3.2f, 0x18161CFFu, 0x0A0A0CFFu);
    leg(4, 10, a->run, STR(a, 6), 4, 15, 3.2f, 0x18161CFFu, 0x0A0A0CFFu);
    float coat[10] = { -14 - a->mv * 4, 16 + b, 13, 15 + b, 10, -16 + b, 0, -18 + b, -10, -16 + b };
    P3(coat, 5, 0x201E26FFu);
    Rf(-3, -16 + b, 3, 6 + b, 0xD4D0C8FFu);
    Lf(0, -14 + b, 0, -6 + b, 2.0f, 0x6A1414FFu);
    Anim w = *a; wounds(&w, -9, -12 + b, 9, 12 + b, 0x5A0808FFu);
    /* the coffin, swung for the slam */
    float rot = state == 6 ? -0.9f : -0.2f + sw * 0.1f * a->mv;
    float cx = 18, cy = -12 + b;
    float cof[10] = { 0, -24, 8, -16, 6, 16, -6, 16, -8, -16 };
    float pts[10];
    for (int k = 0; k < 5; k++) { float x = cof[k * 2], y = cof[k * 2 + 1]; pts[k * 2] = cx + x * cosf(rot) - y * sinf(rot); pts[k * 2 + 1] = cy + x * sinf(rot) + y * cosf(rot); }
    P3(pts, 5, 0x4E3224FFu);
    Lf(cx, cy - 12, cx, cy + 6, 1.5f, 0xA89878FFu); Lf(cx - 4, cy - 6, cx + 4, cy - 6, 1.5f, 0xA89878FFu);
    arm(9, -12 + b, 2.6f + rot, 0.3f, 14, 2.6f, 0x201E26FFu, 0xC8C4BCFFu);
    /* the long pale face: a thin smile that splits */
    E3(0, -26 + b, 8, 10, 12, 0xD4D0C8FFu);
    face_eyes(0.5f, -28 + b, 6.4f, 1.8f, 0x0A0A0AFFu, eye, a, 0.5f, 0.55f);
    mouth(1, -21 + b, 9, 0.1f + a->atk * 0.8f + a->hurt * 0.4f, 7, 0);
    E(0, -35 + b, 14, 3, 12, 0x0E0E12FFu);
    R(-8, -56 + b, 8, -35 + b, 0x0E0E12FFu);
    Rf(-8, -39 + b, 8, -36 + b, 0x5A1414FFu);
}

static void b_prophet(const Anim *a, uint32_t eye, int state)
{
    float b = sinf(a->t * 2.0f) * 3.0f, cast = a->atk;
    for (int k = 0; k < 6; k++) {
        float an = a->t * 0.8f + (float)k / 6.0f * TAU_F;
        L(cosf(an) * 30, -30 + b + sinf(an) * 10 - 6, cosf(an) * 34, -28 + b + sinf(an) * 12, 0.6f, 0x3A3030FFu);
        E(cosf(an) * 34, -28 + b + sinf(an) * 12, 3, 4, 8, 0x70FF40FFu);
    }
    float robe[8] = { -26, 20, 26, 20, 14, -18 + b, -14, -18 + b };
    P3(robe, 4, 0x261C2CFFu);
    for (int k = 0; k < 7; k++) Pf((float[6]){ -26 + (float)k * 7.4f, 20, -22 + (float)k * 7.4f, 25 + sinf(a->t * 4 + (float)k) * 2, -18.6f + (float)k * 7.4f, 20 }, 3, 0x1C1420FFu);
    Lf(-20, 12, 20, 12, 2.0f, 0x6A40A0FFu);
    Anim w = *a; wounds(&w, -14, -12 + b, 14, 14, 0x3A6A10FFu);
    arm(-14, -12 + b, 0.5f + cast * 2.0f, 0.4f, 22, 3.6f, 0x261C2CFFu, 0xC8C0B0FFu);
    E(-24 - cast * 8, 10 + b - cast * 24, 5, 6, 10, 0x90FF60FFu);              /* the censer */
    arm(14, -12 + b, 0.5f + cast * 2.0f, 0.4f, 22, 3.6f, 0x261C2CFFu, 0xC8C0B0FFu);
    E3(0, -26 + b, 11, 11, 12, 0x221C28FFu);
    float beak[6] = { 4, -30 + b, 34, -14 + b + a->hurt * 4, 4, -18 + b };
    P3(beak, 3, 0xE6DCC8FFu);
    E(-2, -30 + b, 4, 4, 10, 0x4A7A2AFFu); E(7, -30 + b, 4, 4, 10, 0x4A7A2AFFu);
    face_eyes(2.5f, -30 + b, 9, 2.2f, 0x6A9A3AFFu, eye, a, 0, 0.5f);
    E(0, -38 + b, 18, 4, 14, 0x120E14FFu);
    R(-9, -58 + b, 9, -38 + b, 0x120E14FFu);
    (void)state;
}

static void b_maw(const Anim *a, uint32_t eye, int state, float laser)
{
    for (int k = 0; k < 8; k++) {
        float an = (float)k / 8.0f * TAU_F + 0.2f;
        float px = 0, py = 0;
        for (int s = 1; s <= 4; s++) {
            float w = sinf(a->t * 2.5f + (float)k + (float)s * 0.8f) * 0.35f * (float)s;
            float nx = cosf(an + w) * (22.0f + (float)s * 10.0f), ny = sinf(an + w) * (16.0f + (float)s * 8.0f);
            L(px, py, nx, ny, 7.0f - (float)s * 1.4f, 0x4E1220FFu);
            if (s == 4) Ef(nx, ny, 2.2f, 2.2f, 5, 0x8A2A3AFFu);
            px = nx; py = ny;
        }
    }
    E3(0, 0, 32, 28, 16, 0x6E1626FFu);
    Anim w = *a; wounds(&w, -24, -20, 24, 20, 0x8A0A14FFu);
    /* the ring of teeth flexes */
    float flex = 1.0f + 0.08f * sinf(a->t * 3.0f) + a->hurt * 0.1f;
    for (int k = 0; k < 12; k++) {
        float an = (float)k / 12.0f * TAU_F + a->t * 0.3f;
        float tt[6] = { cosf(an) * 28 * flex, sinf(an) * 24 * flex, cosf(an + 0.25f) * 18, sinf(an + 0.25f) * 15, cosf(an + 0.5f) * 28 * flex, sinf(an + 0.5f) * 24 * flex };
        Pf(tt, 3, 0xE8DCD0FFu);
    }
    E3(0, 0, 18, 18, 14, 0xEEE4DCFFu);
    for (int k = 0; k < 6; k++) { float an = (float)k * 1.05f; Lf(cosf(an) * 17, sinf(an) * 17, cosf(an) * 10, sinf(an) * 10, 0.8f, 0xC02838FFu); }
    if (a->hurt > 0.35f) { Lf(-17, 0, 17, 0, 4, 0x8A1828FFu); return; }
    float px = a->lookx * 6, py = a->looky * 5;
    Ef(px, py, 10, 10, 14, laser > 0 ? 0xFF2050FFu : 0xC01828FFu);
    float dil = 4.5f - a->atk * 2.5f + laser * 2;
    Ef(px + 1, py, dil, 8, 10, 0x05000AFFu);
    Ef(px + 3, py - 4, 2, 2, 6, 0xFFFFFFFFu);
    if (a->blink > 0.5f) Ef(0, 0, 18, 18, 14, 0x8A2A3AFFu);
    (void)eye; (void)state;
}

/* ================================================================== */
static void enemy_body(int type, const Anim *a, float laser, int state)
{
    uint32_t eye = EDEF[type].eye, bl = EDEF[type].blood;
    switch (type) {
    case A_BANDIT:  a_bandit(a, eye, bl); break;
    case A_BUZZARD: a_buzzard(a, eye, bl); break;
    case A_CACTUS:  a_cactus(a, eye, bl); break;
    case A_SLINGER: a_slinger(a, eye, bl); break;
    case A_TOAD: {
        float cyc = fmodf(a->run, 1.1f);
        float h = a->dead ? 0.0f : (cyc > 0.45f && cyc < 0.85f ? sinf((cyc - 0.45f) / 0.4f * PI_F) : 0.0f);
        a_toad(a, eye, bl, h);
        break;
    }
    case A_CROW:    a_crow(a, eye, bl); break;
    case A_MOSS:    a_moss(a, eye, bl); break;
    case A_LANTERN: a_lantern(a, eye, bl); break;
    case A_ZOMBIE:  a_zombie(a, eye, bl); break;
    case A_BONES:   a_bones(a, eye, bl); break;
    case A_COFFIN:  a_coffin(a, eye, bl); break;
    case A_WRAITH:  a_wraith(a, eye, bl, a->dead ? 0.6f : (fmodf(a->t, 4.0f) > 3.0f ? 0.35f : 0.92f)); break;
    case A_HUSK:    a_husk(a, eye, bl); break;
    case A_IMP:     a_imp(a, eye, bl); break;
    case A_HULK:    a_hulk(a, eye, bl); break;
    case A_DOCTOR:  a_doctor(a, eye, bl); break;
    case A_CRAWLER: a_crawler(a, eye, bl); break;
    case A_EYE:     a_eye(a, eye, bl); break;
    case A_FLESH:   a_flesh(a, eye, bl); break;
    case A_PRIEST:  a_priest(a, eye, bl); break;
    case A_BOSS1:   b_sheriff(a, eye, state); break;
    case A_BOSS2:   b_bogmother(a, eye, state); break;
    case A_BOSS3:   b_undertaker(a, eye, state); break;
    case A_BOSS4:   b_prophet(a, eye, state); break;
    case A_BOSS5:   b_maw(a, eye, state, laser); break;
    }
}

void art_enemy(const Game *g, const Enemy *e, float x, float y)
{
    const EnemyDef *d = &EDEF[e->type];
    float s = art_scale(e);
    float flash = e->flash > 0 ? (d->ai == AI_BOSS ? 0.35f : 0.75f) : 0.0f;
    if (e->state == 250) flash = 0.4f + 0.4f * sinf(g->t * 40.0f);
    begin(x, y, s, e->face, flash, e->elite);
    Anim a;
    memset(&a, 0, sizeof a);
    int idx = (int)(e - g->en);
    a.t = e->t + (float)idx * 0.37f;
    a.seed = (uint32_t)idx * 7919u + (uint32_t)e->type;
    a.run = e->anim;
    if (e->type == A_TOAD) a.run = e->t + e->st;
    a.mv = e->mvis;
    a.hurt = clampf(e->hurt / 0.3f, 0, 1);
    a.wounds = e->wounds;
    float dx = g->p.x - e->x, dy = g->p.y - e->y, l = sqrtf(dx * dx + dy * dy) + 1e-3f;
    a.lookx = fabsf(dx) / l; a.looky = dy / l;
    a.blink = fmodf(a.t * 0.9f + (float)(idx & 7) * 0.3f, 3.3f) < 0.12f ? 1.0f : 0.0f;
    a.rage = clampf(1.0f - l / 400.0f, 0.2f, 1.0f);
    if (d->ai == AI_SHOOT) a.atk = e->cd < 0.45f ? 1.0f - e->cd / 0.45f : 0.0f;
    if (d->ai == AI_FLY) { float cyc = fmodf(e->t + e->st * 3.0f, 3.0f); a.atk = cyc > 1.6f ? 1.0f : 0.0f; }
    if (d->ai == AI_BOSS) { a.atk = e->state ? 1.0f : 0.0f; a.t = e->t; }
    enemy_body(e->type, &a, g->laser_on, e->state);
    FLASH = 0; OUTC = OUT;
}

void art_corpse(const Corpse *c, float x, float y, float fade)
{
    begin(x, y, c->s, c->face, 0, 0);
    int keep = DETAIL;
    DETAIL = 0;
    RC = cosf(c->rot); RS = sinf(c->rot);
    DARK = 0.35f + 0.25f * (1.0f - fade);
    ALPHA = fade;
    Anim a;
    memset(&a, 0, sizeof a);
    a.dead = 1; a.seed = c->seed; a.wounds = 4; a.t = 0; a.lookx = 1; a.headless = c->headless;
    enemy_body(c->type, &a, 0, 0);
    DARK = 0; ALPHA = 1; RC = 1; RS = 0;
    DETAIL = keep;
}

/* a severed head, tumbling */
void art_head(int type, float x, float y, float s, float rot, float face)
{
    begin(x, y, s, face, 0, 0);
    RC = cosf(rot); RS = sinf(rot);
    Anim a;
    memset(&a, 0, sizeof a);
    a.dead = 1; a.lookx = 1;
    const EnemyDef *d = &EDEF[type];
    int bony = type == A_BANDIT || type == A_SLINGER || type == A_BONES || type == A_BOSS1;
    if (bony) skull_a(0, 0, 5.0f, 0xEDE2C8FFu, d->eye, &a, 0.6f);
    else {
        E3(0, 0, 5.0f, 4.8f, 8, d->c0);
        face_eyes(0.5f, -0.5f, 4.0f, 1.3f, 0xD8D0B0FFu, 0x200808FFu, &a, 0, 0.5f);
        mouth(0.8f, 2.5f, 4, 0.7f, 3, 0);
    }
    Ef(-4.5f, 1.5f, 2.0f, 2.6f, 6, d->blood);              /* the stump */
    RC = 1; RS = 0;
}

void art_enemy_preview(int type, float x, float y, float s, float t)
{
    begin(x, y, s, 1, 0, 0);
    Anim a;
    memset(&a, 0, sizeof a);
    a.t = t; a.run = t * 9.0f; a.mv = EDEF[type].ai == AI_BOSS ? 0.4f : 0.8f; a.lookx = 1; a.rage = 0.6f;
    a.seed = (uint32_t)type * 31u;
    enemy_body(type, &a, 0, 0);
}

void art_boss_portrait(int biome, float x, float y, float s, float t) { art_enemy_preview(A_BOSS1 + biome, x, y, s, t); }

/* ================================================================== */
/* the gunslingers                                                      */
/* ================================================================== */
void art_player_ex(float x, float y, int ch, float face, const Anim *a, float scale, float rot)
{
    begin(x, y, scale, face, a->hurt > 0.5f ? 0.55f : 0.0f, 0);
    RC = cosf(rot); RS = sinf(rot);
    if (a->dead) DARK = 0.25f;
    static const uint32_t COAT[NCHARS] = { 0x2C2630FFu, 0x18161CFFu, 0x523462FFu, 0x34502EFFu };
    static const uint32_t TROU[NCHARS] = { 0x2A2420FFu, 0x141216FFu, 0x3A2440FFu, 0x2A3422FFu };
    uint32_t coat = COAT[ch], trou = TROU[ch];
    float b = BOB(a, 2.0f), sw = SW(a), lean = a->mv * 1.6f;
    float rec = a->firing ? 0.3f : 0.0f;
    /* legs */
    leg(-2.5f, 4, a->run + PI_F, STR(a, 4.5f), 3.5f, 7, 2.3f, trou, 0x1E140EFFu);
    arm(-4 + lean, -3 + b, -sw * 0.9f * a->mv - 0.1f, 0.6f, 9, 2.0f, coat, 0xC89A78FFu);   /* the off hand swings */
    leg(2.5f, 4, a->run, STR(a, 4.5f), 3.5f, 7, 2.3f, trou, 0x1E140EFFu);
    /* the duster flares behind when running */
    float flare = a->mv * 4.0f + sinf(a->t * 7.0f) * 1.0f * a->mv;
    float duster[10] = { -10 - flare, 9 + b * 0.5f, 9 - flare * 0.2f, 8 + b * 0.5f, 7 + lean, -6 + b, 0 + lean, -8 + b, -7 + lean, -6 + b };
    P3(duster, 5, coat);
    Pf((float[8]){ -10 - flare, 9 + b * 0.5f, -6 - flare, 9 + b * 0.5f, -8 - flare * 1.3f, 12 + b * 0.5f + sinf(a->t * 9) * a->mv, -12 - flare * 1.3f, 11 + b * 0.5f }, 4, SHD(coat));
    Lf(0 + lean, -6 + b, 0, 7 + b * 0.5f, 1.0f, SHD(coat));
    Rf(-6 + lean * 0.5f, 1 + b, 7 + lean * 0.5f, 3 + b, 0x4A3018FFu);
    Ef(1 + lean * 0.5f, 2 + b, 1.5f, 1.3f, 5, 0xE0B850FFu);
    Ef(-5 + lean * 0.5f, 3.5f + b, 1.8f, 2.2f, 6, 0xC89030FFu);                      /* the lantern on the belt */
    wounds(a, -5, -4 + b, 5, 4 + b, 0x8A0A0AFFu);
    /* head: a beat behind the body */
    float hb = -fabsf(sinf(a->run - 0.6f)) * a->mv * 2.2f + rec * 0.5f, hx = lean * 1.3f;
    float squint = a->firing ? 0.8f : 0.2f;
    switch (ch) {
    case CH_DRIFTER:
        E3(hx, -11 + hb, 5.2f, 5.0f, 10, 0x30282CFFu);
        face_eyes(hx + 1.2f, -12 + hb, 4.2f, 1.2f, 0xFFD040FFu, 0x5A3000FFu, a, squint, 0.55f);
        Rf(hx - 5, -10 + hb, hx + 5.2f, -6.2f + hb, 0xA82424FFu);                        /* bandana, knot flapping */
        Pf((float[6]){ hx - 4.5f, -9.5f + hb, hx - 9 - a->mv * 3, -8 + hb + sinf(a->t * 14) * 1.5f, hx - 4.5f, -7 + hb }, 3, 0x8A1818FFu);
        E(hx, -15.5f + hb - a->mv * 0.4f, 12, 3.2f, 12, 0x521E1AFFu);
        { float cr[8] = { hx - 6, -15.5f + hb, hx + 6, -15.5f + hb, hx + 4.5f, -22 + hb, hx - 4.5f, -22 + hb }; P3(cr, 4, 0x622620FFu); }
        Rf(hx - 5.5f, -18 + hb, hx + 5.5f, -16.5f + hb, 0x2A1A14FFu);
        break;
    case CH_PREACHER:
        E3(hx, -11 + hb, 5.2f, 5.0f, 10, 0xC8A88AFFu);
        face_eyes(hx + 1.2f, -12 + hb, 4.2f, 1.2f, 0xF0F0E8FFu, 0x305080FFu, a, 0.4f + squint * 0.5f, 0.5f);
        Ef(hx, -7.5f + hb, 4.4f, 3.2f, 8, 0x8A8A90FFu);
        mouth(hx + 1.2f, -8 + hb, 3.2f, a->firing ? 0.8f : (a->hurt > 0.3f ? 0.6f : 0.1f), 3, 0);   /* hollering scripture */
        Rf(hx - 2.5f, -4 + hb, hx + 2.5f, -2.5f + hb, 0xF0F0F0FFu);
        E(hx, -15.5f + hb, 11, 2.4f, 12, 0x121216FFu);
        R(hx - 5.5f, -20 + hb, hx + 5.5f, -15.5f + hb, 0x16161CFFu);
        break;
    case CH_GRANNY:
        E3(hx, -11 + hb, 5.0f, 5.0f, 10, 0xE0C0A8FFu);
        E3(hx - 1, -17 + hb, 4, 3.2f, 8, 0xC0C0C8FFu);
        E(hx + 1.2f, -11.5f + hb, 1.9f, 1.9f, 8, 0xC0D8F0FFu); E(hx - 2.2f, -11.5f + hb, 1.9f, 1.9f, 8, 0xC0D8F0FFu);
        face_eyes(hx - 0.5f, -11.5f + hb, 3.4f, 0.9f, 0xC0D8F0FFu, 0x201820FFu, a, squint, 0.6f);
        mouth(hx + 0.6f, -8 + hb, 3.0f, a->firing ? 0.5f : 0.05f, 4, 0);                    /* gritted dentures */
        { float bon[8] = { hx - 7, -9 + hb, hx - 5, -17 + hb, hx + 5, -17 + hb, hx + 7, -9 + hb }; P(bon, 4, 0xA888C0FFu); }
        Lf(hx - 6, -9 + hb, hx + 6, -9 + hb, 1.0f, 0xD06090FFu);
        break;
    default:
        E3(hx, -11 + hb, 5.2f, 5.0f, 10, 0xD8B090FFu);
        for (int k = 0; k < 5; k++) L(hx - 5 + (float)k * 2.5f, -14 + hb, hx - 7 + (float)k * 3.5f - a->mv * 2, -19 + hb + (float)(k % 2) * 2 + sinf(a->t * 10 + (float)k) * 0.8f, 1.4f, 0xE06820FFu);
        E(hx - 1.5f, -12 + hb, 2.2f, 2.0f, 8, 0xE89828FFu); E(hx + 2.8f, -12 + hb, 2.2f, 2.0f, 8, 0xE89828FFu);
        face_eyes(hx + 0.65f, -12 + hb, 4.3f, 1.0f, 0xFFE070FFu, 0x201000FFu, a, 0, 0.5f);
        mouth(hx + 1.0f, -8 + hb, 4.2f, a->firing ? 0.35f : 0.2f, 5, 0);                        /* the mad grin */
        Lf(hx - 6, -12 + hb, hx + 6, -12 + hb, 1.0f, 0x3A2A1AFFu);
        E(hx, -16 + hb, 7, 2.0f, 10, 0x262626FFu);
        E3(hx, -18.5f + hb, 4.5f, 3.0f, 10, 0x262626FFu);
        break;
    }
    FLASH = 0; DARK = 0; RC = 1; RS = 0;
}

void art_player(float x, float y, int ch, float face, float walk, float ax, float ay, float t, float hurt, float scale)
{
    Anim a;
    memset(&a, 0, sizeof a);
    a.t = t; a.run = walk; a.mv = walk != 0.0f ? 1.0f : 0.0f; a.hurt = hurt > 0 ? 1.0f : 0.0f; a.lookx = 1; a.looky = 0;
    a.blink = fmodf(t, 3.7f) < 0.1f ? 1.0f : 0.0f;
    (void)ax; (void)ay;
    art_player_ex(x, y, ch, face, &a, scale, 0);
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
