/*
 * LAST STAND - music. See music.h.
 *
 * A tiny offline synth: each instrument renders whole notes into a float loop buffer (notes that run past the
 * end wrap round to the start, so every loop is seamless), then a small Schroeder reverb is run round the loop
 * twice and the result is normalised to 16-bit. The fight tracks share tempo, length and harmony
 * (E minor: Em - C - D - B), so any mix of them is in time and in tune.
 */
#include "music.h"
#include "gfx.h"

#include <math.h>
#include <string.h>
#include "fastmath.h"

/* a quick exp for the envelopes and filters (millions of calls while composing; ~1e-6 relative error) */
static inline float fast_exp(float x)
{
    if (x < -86.0f) return 0.0f;
    if (x > 87.0f) x = 87.0f;
    float t = x * 1.442695041f, fl = (float)(int)t;
    if (fl > t) fl -= 1.0f;
    float f = t - fl;
    float p = 1.0f + f * (0.6931472f + f * (0.2402265f + f * (0.0555041f + f * (0.0096181f + f * 0.0013334f))));
    union { float f; int32_t i; } u;
    u.i = ((int32_t)fl + 127) << 23;
    return p * u.f;
}
#define expf(x) fast_exp(x)

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#define SR    ((float)MUS_RATE)
#define TAU_F 6.28318530718f

#define MUS_MAX (MUS_RATE * 30 + 16)
static int16_t MUS[MUS_TRACKS][MUS_MAX] __attribute__((aligned(32)));
static int     mus_len[MUS_TRACKS];
static float  *mix;               /* MUS_MAX floats, from big_alloc */
static int     N;                         /* length of the loop being built */
static float   BEAT;                      /* seconds per beat */
static uint32_t rng = 0x1234567u;
static int     ready;

static const uint8_t VOL[MUS_TRACKS] = { 120, 118, 112, 128, 105 };

static float frnd(void)                  /* -1..1 */
{
    rng = rng * 1664525u + 1013904223u;
    return (float)(rng >> 8) / 8388608.0f - 1.0f;
}

static float mtof(float m) { return 440.0f * powf(2.0f, (m - 69.0f) / 12.0f); }

static inline void put(int i, float v)
{
    i %= N;
    if (i < 0) i += N;
    mix[i] += v;
}

/* attack / exponential decay / release envelope */
static inline float env_adr(int i, int n, float atk, float tau, float rel)
{
    float t = (float)i / SR, e = 1.0f;
    if (t < atk) e = t / atk;
    if (tau > 0) e *= expf(-t / tau);
    float left = (float)(n - i) / SR;
    if (left < rel) e *= left / rel;
    return e;
}

/* ------------------------------------------------------------------ */
/* instruments (times in beats)                                         */
/* ------------------------------------------------------------------ */

/* warm pad: two detuned band-limited saws through a 2-pole low-pass, slow attack */
static void pad(float b, float dur, float m, float vel, float cutoff)
{
    int s0 = (int)(b * BEAT * SR), n = (int)((dur * BEAT + 1.2f) * SR);
    float f = mtof(m), ph1 = 0, ph2 = 0.37f, lp1 = 0, lp2 = 0;
    float k = 1.0f - expf(-TAU_F * cutoff / SR);
    float d1 = f * 1.004f / SR, d2 = f * 0.996f / SR;
    for (int i = 0; i < n; i++) {
        float s = 0;
        /* polyBLEP saws */
        for (int o = 0; o < 2; o++) {
            float *ph = o ? &ph2 : &ph1, dt = o ? d2 : d1, x = 2.0f * *ph - 1.0f, t = *ph;
            if (t < dt) { t /= dt; x -= t + t - t * t - 1.0f; }
            else if (t > 1.0f - dt) { t = (t - 1.0f) / dt; x -= t * t + t + t + 1.0f; }
            s += x;
            *ph += dt; if (*ph >= 1.0f) *ph -= 1.0f;
        }
        lp1 += (s - lp1) * k; lp2 += (lp1 - lp2) * k;
        float e = env_adr(i, n, 0.9f, 0, 1.2f);
        put(s0 + i, lp2 * e * vel * 0.18f);
    }
}

/* electric piano: FM with a decaying index plus a soft sine body */
static void epiano(float b, float dur, float m, float vel)
{
    int s0 = (int)(b * BEAT * SR), n = (int)((dur * BEAT + 0.6f) * SR);
    float f = mtof(m), pc = 0, pm = 0;
    for (int i = 0; i < n; i++) {
        float t = (float)i / SR, idx = 1.6f * expf(-t * 3.0f);
        float e = env_adr(i, n, 0.004f, 1.4f, 0.3f);
        float v = sinf(TAU_F * pc + idx * sinf(TAU_F * pm)) + 0.35f * sinf(TAU_F * pc * 2.0f) * expf(-t * 2.0f);
        put(s0 + i, v * e * vel * 0.32f);
        pc += f / SR; pc -= floorf(pc);
        pm += f * 1.0f / SR; pm -= floorf(pm);
    }
}

/* celesta / glassy bell */
static void bell(float b, float m, float vel, float decay)
{
    int s0 = (int)(b * BEAT * SR), n = (int)(decay * 5.0f * SR);
    float f = mtof(m), pc = 0, pm = 0;
    for (int i = 0; i < n; i++) {
        float t = (float)i / SR;
        float e = env_adr(i, n, 0.002f, decay, 0.05f);
        float v = sinf(TAU_F * pc + 1.2f * expf(-t * 6.0f) * sinf(TAU_F * pm)) + 0.25f * sinf(TAU_F * pc * 4.0f) * expf(-t * 8.0f);
        put(s0 + i, v * e * vel * 0.28f);
        pc += f / SR; pc -= floorf(pc);
        pm += f * 3.5f / SR; pm -= floorf(pm);
    }
}

/* plucked string (Karplus-Strong): guitar-ish when bright, pizzicato bass when dark */
static void pluck(float b, float dur, float m, float vel, float bright)
{
    static float ks[2048];
    int s0 = (int)(b * BEAT * SR), n = (int)((dur * BEAT + 0.6f) * SR);
    float f = mtof(m);
    int L = (int)(SR / f);
    if (L < 2) L = 2;
    if (L > 2047) L = 2047;
    float lp = 0;
    for (int i = 0; i < L; i++) { lp += (frnd() - lp) * bright; ks[i] = lp; }
    int p = 0;
    float damp = 0.4975f + 0.0024f * bright;
    for (int i = 0; i < n; i++) {
        int q = p + 1 == L ? 0 : p + 1;
        float y = ks[p];
        ks[p] = (ks[p] + ks[q]) * damp;
        p = q;
        float e = env_adr(i, n, 0.001f, 0, 0.08f);
        put(s0 + i, y * e * vel * 0.9f);
    }
}




/* soft round bass */
static void bass(float b, float dur, float m, float vel)
{
    int s0 = (int)(b * BEAT * SR), n = (int)((dur * BEAT + 0.1f) * SR);
    float f = mtof(m), ph = 0;
    for (int i = 0; i < n; i++) {
        float e = env_adr(i, n, 0.01f, dur * BEAT * 1.5f, 0.08f);
        float v = sinf(TAU_F * ph) + 0.2f * sinf(TAU_F * ph * 2.0f);
        put(s0 + i, v * e * vel * 0.4f);
        ph += f / SR; ph -= floorf(ph);
    }
}

/* low drone with a slow swell */
__attribute__((unused)) static void drone(float m, float vel)
{
    float f = mtof(m), ph = 0;
    for (int i = 0; i < N; i++) {
        float t = (float)i / (float)N;
        float sw = 0.6f + 0.4f * sinf(TAU_F * t * 2.0f);   /* whole cycles, so it loops cleanly */
        put(i, (sinf(TAU_F * ph) + 0.3f * sinf(TAU_F * ph * 1.5f)) * sw * vel * 0.12f);
        ph += f / SR; ph -= floorf(ph);
    }
}

static void kick(float b, float vel)
{
    int s0 = (int)(b * BEAT * SR), n = (int)(0.28f * SR);
    float ph = 0;
    for (int i = 0; i < n; i++) {
        float t = (float)i / SR, f = 45.0f + 75.0f * expf(-t * 30.0f);
        put(s0 + i, sinf(TAU_F * ph) * expf(-t * 12.0f) * vel * 0.55f);
        ph += f / SR;
    }
}

/* noise percussion: shaker (bright, short) or finger snap / clap (band-passed) */
static void noise_hit(float b, float vel, float decay, float centre, float q)
{
    int s0 = (int)(b * BEAT * SR), n = (int)(decay * 6.0f * SR);
    float w = TAU_F * centre / SR, a = sinf(w) / (2.0f * q), cw = cosf(w), a0 = 1.0f + a;
    float b0 = a / a0, b2 = -a / a0, a1 = -2.0f * cw / a0, a2 = (1.0f - a) / a0, z1 = 0, z2 = 0;
    for (int i = 0; i < n; i++) {
        float x = frnd(), y = b0 * x + z1;
        z1 = -a1 * y + z2;
        z2 = b2 * x - a2 * y;
        float t = (float)i / SR;
        put(s0 + i, y * expf(-t / decay) * vel * 0.5f);
    }
}


/* ------------------------------------------------------------------ */
/* reverb (Schroeder: 4 combs + 2 allpasses), run round the loop twice  */
/* ------------------------------------------------------------------ */
static void reverb(float amount, float size)
{
    static float cb[4][3000], ap[2][800];
    static const int CL[4] = { 1116, 1188, 1277, 1356 }, AL[2] = { 556, 341 };
    int cl[4], al[2];
    for (int k = 0; k < 4; k++) cl[k] = (int)((float)CL[k] * 0.69f * size);   /* tuned for 44.1k, scaled to 22k */
    for (int k = 0; k < 2; k++) al[k] = (int)((float)AL[k] * 0.69f);
    memset(cb, 0, sizeof cb); memset(ap, 0, sizeof ap);
    int cp[4] = {0}, apos[2] = {0};
    float fs[4] = {0};
    for (int pass = 0; pass < 2; pass++)
        for (int i = 0; i < N; i++) {
            float x = mix[i] * 0.25f, out = 0;
            for (int k = 0; k < 4; k++) {
                float y = cb[k][cp[k]];
                fs[k] = y * 0.6f + fs[k] * 0.4f;              /* damping */
                cb[k][cp[k]] = x + fs[k] * 0.82f;
                if (++cp[k] >= cl[k]) cp[k] = 0;
                out += y;
            }
            for (int k = 0; k < 2; k++) {
                float bo = ap[k][apos[k]];
                float y = -out + bo;
                ap[k][apos[k]] = out + bo * 0.5f;
                if (++apos[k] >= al[k]) apos[k] = 0;
                out = y;
            }
            /* second time round: add the reverb in place (each sample is read as dry before it is written) */
            if (pass) mix[i] += out * amount;
        }
}

static void begin(float bpm, int beats)
{
    BEAT = 60.0f / bpm;
    N = ((int)((float)beats * BEAT * SR) + 15) & ~15;
    if (N > MUS_MAX) N = MUS_MAX;
    memset(mix, 0, sizeof(float) * MUS_MAX);
}

static void finish(int track)
{
    float peak = 0;
    for (int i = 0; i < N; i++) if (fabsf(mix[i]) > peak) peak = fabsf(mix[i]);
    float k = peak > 0 ? 0.85f * 32000.0f / peak : 0;
    for (int i = 0; i < N; i++) MUS[track][i] = (int16_t)(mix[i] * k);
    mus_len[track] = N;
}


/* ------------------------------------------------------------------ */
/* more instruments                                                     */
/* ------------------------------------------------------------------ */

/* snare: a tuned body and a burst of bright noise */
static void snare(float b, float vel)
{
    int s0 = (int)(b * BEAT * SR), n = (int)(0.3f * SR);
    float ph = 0, lp = 0, hp = 0, hpp = 0;
    for (int i = 0; i < n; i++) {
        float t = (float)i / SR;
        float body = sinf(TAU_F * ph) * expf(-t * 28.0f);
        ph += (190.0f - 40.0f * t) / SR;
        float nz = frnd();
        lp += (nz - lp) * 0.6f;
        hp = 0.9f * (hp + lp - hpp); hpp = lp;
        put(s0 + i, (body * 0.5f + hp * expf(-t * 16.0f) * 0.9f) * vel * 0.5f);
    }
}

/* toms: a pitched thump */
static void tom(float b, float m, float vel)
{
    int s0 = (int)(b * BEAT * SR), n = (int)(0.45f * SR);
    float f = mtof(m), ph = 0;
    for (int i = 0; i < n; i++) {
        float t = (float)i / SR;
        put(s0 + i, sinf(TAU_F * ph) * expf(-t * 9.0f) * vel * 0.5f + frnd() * expf(-t * 60.0f) * vel * 0.1f);
        ph += f * (1.0f + 0.6f * expf(-t * 25.0f)) / SR;
    }
}

/* crash cymbal: long bright noise */
static void crash(float b, float vel)
{
    int s0 = (int)(b * BEAT * SR), n = (int)(2.2f * SR);
    float hp = 0, hpp = 0;
    for (int i = 0; i < n; i++) {
        float t = (float)i / SR, nz = frnd();
        hp = 0.85f * (hp + nz - hpp); hpp = nz;
        put(s0 + i, hp * expf(-t * 2.2f) * vel * 0.22f);
    }
}

/* big low impact for the bed: a boom with a long tail */
static void boom(float b, float vel)
{
    int s0 = (int)(b * BEAT * SR), n = (int)(2.4f * SR);
    float ph = 0, lp = 0;
    for (int i = 0; i < n; i++) {
        float t = (float)i / SR, f = 38.0f + 40.0f * expf(-t * 14.0f);
        lp += (frnd() - lp) * 0.02f;
        put(s0 + i, (sinf(TAU_F * ph) * 0.8f + lp * 2.0f) * expf(-t * 1.6f) * vel * 0.5f);
        ph += f / SR;
    }
}

/* a gritty synth bass: saw through a resonant-ish low-pass with an envelope, soft-clipped */
static void sbass(float b, float dur, float m, float vel, float bright)
{
    int s0 = (int)(b * BEAT * SR), n = (int)((dur * BEAT + 0.04f) * SR);
    float f = mtof(m), ph = 0, lp1 = 0, lp2 = 0;
    float dt = f / SR;
    for (int i = 0; i < n; i++) {
        float t = (float)i / SR;
        float x = 2.0f * ph - 1.0f, u = ph;
        if (u < dt) { u /= dt; x -= u + u - u * u - 1.0f; }
        else if (u > 1.0f - dt) { u = (u - 1.0f) / dt; x -= u * u + u + u + 1.0f; }
        ph += dt; if (ph >= 1.0f) ph -= 1.0f;
        float cut = 180.0f + bright * 1400.0f * expf(-t * 18.0f);
        float k = 1.0f - expf(-TAU_F * cut / SR);
        lp1 += (x - lp1) * k; lp2 += (lp1 - lp2) * k;
        float y = lp2 * 1.8f; y = y / (1.0f + fabsf(y));
        float e = env_adr(i, n, 0.003f, 0, 0.03f);
        put(s0 + i, y * e * vel * 0.55f);
    }
}

/* brass / string stab: several detuned saws, bright attack closing down */
static void stab(float b, float dur, const float *notes, int nn, float vel)
{
    for (int k = 0; k < nn; k++) {
        int s0 = (int)(b * BEAT * SR), n = (int)((dur * BEAT + 0.25f) * SR);
        float f = mtof(notes[k]);
        float ph[3] = { 0, 0.33f, 0.71f }, det[3] = { 1.0f, 1.006f, 0.994f };
        float lp1 = 0, lp2 = 0;
        for (int i = 0; i < n; i++) {
            float t = (float)i / SR, s = 0;
            for (int o = 0; o < 3; o++) {
                float dt = f * det[o] / SR, x = 2.0f * ph[o] - 1.0f, u = ph[o];
                if (u < dt) { u /= dt; x -= u + u - u * u - 1.0f; }
                else if (u > 1.0f - dt) { u = (u - 1.0f) / dt; x -= u * u + u + u + 1.0f; }
                s += x;
                ph[o] += dt; if (ph[o] >= 1.0f) ph[o] -= 1.0f;
            }
            float cut = 500.0f + 3000.0f * expf(-t * 7.0f);
            float kk = 1.0f - expf(-TAU_F * cut / SR);
            lp1 += (s - lp1) * kk; lp2 += (lp1 - lp2) * kk;
            float e = env_adr(i, n, 0.01f, 0, 0.2f);
            put(s0 + i, lp2 * e * vel * 0.11f);
        }
    }
}

/* string tremolo: a pad chopped in 16ths */
__attribute__((unused)) static void tremolo(float b, float dur, float m, float vel)
{
    int s0 = (int)(b * BEAT * SR), n = (int)((dur * BEAT + 0.3f) * SR);
    float f = mtof(m), ph1 = 0, ph2 = 0.4f, lp = 0;
    for (int i = 0; i < n; i++) {
        float t = (float)i / SR;
        float s = sinf(TAU_F * ph1) + 0.5f * sinf(TAU_F * ph1 * 2.0f) + 0.3f * sinf(TAU_F * ph2 * 3.0f);
        ph1 += f * (1.0f + 0.003f * sinf(TAU_F * 5.5f * t)) / SR; ph1 -= floorf(ph1);
        ph2 += f * 1.002f / SR; ph2 -= floorf(ph2);
        lp += (s - lp) * 0.3f;
        float trem = 0.55f + 0.45f * sinf(TAU_F * t / (BEAT * 0.25f));
        float e = env_adr(i, n, 0.15f, 0, 0.3f);
        put(s0 + i, lp * trem * e * vel * 0.13f);
    }
}


/* ------------------------------------------------------------------ */
/* western instruments                                                  */
/* ------------------------------------------------------------------ */
static float clampm(float x) { return x < 0 ? 0 : (x > 1 ? 1 : x); }

/* a lonely whistle: sine with a slow vibrato and a breath of noise */
static void whistle(float b, float dur, float m, float vel)
{
    int s0 = (int)(b * BEAT * SR), n = (int)((dur * BEAT + 0.2f) * SR);
    float f = mtof(m), ph = 0, lp = 0;
    for (int i = 0; i < n; i++) {
        float t = (float)i / SR;
        float vib = 1.0f + 0.006f * sinf(TAU_F * 5.2f * t) * clampm(t * 2.0f);
        float bend = t < 0.06f ? 0.97f + 0.5f * t : 1.0f;     /* slide up into the note */
        lp += (frnd() - lp) * 0.3f;
        float e = env_adr(i, n, 0.05f, 0, 0.18f);
        put(s0 + i, (sinf(TAU_F * ph) + lp * 0.05f) * e * vel * 0.3f);
        ph += f * vib * bend / SR; ph -= floorf(ph);
    }
}

/* overdriven power chord (root + fifth + octave), palm-muted when short */
static void power(float b, float dur, float m, float vel, float mute)
{
    int s0 = (int)(b * BEAT * SR), n = (int)((dur * BEAT + 0.05f) * SR);
    float f0 = mtof(m), ph[3] = { 0, 0.21f, 0.53f }, mul[3] = { 1.0f, 1.4983f, 2.0f };
    float lp = 0, lp2 = 0;
    float k = 1.0f - expf(-TAU_F * (900.0f + (1.0f - mute) * 2400.0f) / SR);
    for (int i = 0; i < n; i++) {
        float s = 0;
        for (int k = 0; k < 3; k++) {
            float dt = f0 * mul[k] * (1.0f + 0.002f * (float)k) / SR, x = 2.0f * ph[k] - 1.0f, u = ph[k];
            if (u < dt) { u /= dt; x -= u + u - u * u - 1.0f; }
            else if (u > 1.0f - dt) { u = (u - 1.0f) / dt; x -= u * u + u + u + 1.0f; }
            s += x * (k == 2 ? 0.6f : 1.0f);
            ph[k] += dt; if (ph[k] >= 1.0f) ph[k] -= 1.0f;
        }
        s *= 3.5f; s = s / (1.0f + fabsf(s));                       /* the fuzz */
        lp += (s - lp) * k; lp2 += (lp - lp2) * k;
        float e = env_adr(i, n, 0.002f, mute > 0.5f ? 0.09f : 0.0f, 0.03f);
        put(s0 + i, lp2 * e * vel * 0.22f);
    }
}

/* a church organ: stacked sines with a little chorus */
static void organ(float b, float dur, float m, float vel)
{
    int s0 = (int)(b * BEAT * SR), n = (int)((dur * BEAT + 0.4f) * SR);
    float f = mtof(m), p1 = 0, p2 = 0, p3 = 0;
    for (int i = 0; i < n; i++) {
        float t = (float)i / SR;
        float v = sinf(TAU_F * p1) + 0.6f * sinf(TAU_F * p2) + 0.45f * sinf(TAU_F * p3);
        float e = env_adr(i, n, 0.03f, 0, 0.35f);
        put(s0 + i, v * e * vel * 0.12f);
        float ch = 1.0f + 0.002f * sinf(TAU_F * 6.0f * t);
        p1 += f * ch / SR; p1 -= floorf(p1);
        p2 += f * 2.0f / SR; p2 -= floorf(p2);
        p3 += f * 3.0f * ch / SR; p3 -= floorf(p3);
    }
}

/* the wind on the plains: slowly swelling filtered noise across the whole loop */
static void wind(float vel)
{
    float lp1 = 0, lp2 = 0;
    for (int i = 0; i < N; i++) {
        float t = (float)i / (float)N;
        float sw = 0.5f + 0.5f * sinf(TAU_F * t * 3.0f + 1.0f);
        float cut = 300.0f + 500.0f * sw;
        float k = 1.0f - expf(-TAU_F * cut / SR);
        lp1 += (frnd() - lp1) * k; lp2 += (lp1 - lp2) * k;
        put(i, lp2 * sw * vel * 0.9f);
    }
}


/* ------------------------------------------------------------------ */
/* the harmony: Em - C - D - B, two bars each                           */
/* ------------------------------------------------------------------ */
static const float ROOT[4] = { 40, 36, 38, 35 };                 /* E2 C2 D2 B1 */
static const float CHORD[4][3] = { {64, 67, 71}, {60, 64, 67}, {62, 66, 69}, {59, 63, 66} };
static int chord_at(int bar) { return (bar / 2) % 4; }
#define FIGHT_BPM 132.0f

/* ------------------------------------------------------------------ */
/* MENU: whistle and guitar at dusk                                     */
/* ------------------------------------------------------------------ */
static void track_menu(void)
{
    begin(76, 32);
    static const float R[4] = { 40, 36, 38, 35 };
    for (int bar = 0; bar < 8; bar++) {
        int c = (bar / 2) % 4;
        float b0 = (float)bar * 4;
        /* fingerpicked arpeggio */
        static const int ARP[8] = { 0, 1, 2, 1, 0, 2, 1, 2 };
        for (int q = 0; q < 8; q++) pluck(b0 + (float)q * 0.5f, 0.6f, CHORD[c][ARP[q]] - 12 + (q == 0 ? -12 : 0), 0.32f, 0.55f);
        bass(b0, 3.5f, R[c], 0.35f);
        boom(b0, 0.25f);
        if (bar % 2 == 1) noise_hit(b0 + 3.5f, 0.05f, 0.03f, 5200, 1.5f);
    }
    static const float MEL[][3] = {
        { 0, 1.5f, 76 }, { 1.5f, 0.5f, 79 }, { 2, 2, 83 }, { 4, 3, 81 }, { 7, 1, 79 },
        { 8, 1.5f, 79 }, { 9.5f, 0.5f, 81 }, { 10, 2, 79 }, { 12, 4, 76 },
        { 16, 1.5f, 76 }, { 17.5f, 0.5f, 74 }, { 18, 2, 74 }, { 20, 3, 78 }, { 23, 1, 81 },
        { 24, 2, 83 }, { 26, 1, 81 }, { 27, 1, 78 }, { 28, 4, 76 },
    };
    for (unsigned i = 0; i < sizeof MEL / sizeof MEL[0]; i++) whistle(MEL[i][0], MEL[i][1], MEL[i][2], 0.42f);
    wind(0.35f);
    reverb(0.7f, 1.5f);
    finish(MUS_MENU);
}

/* ------------------------------------------------------------------ */
/* RUN: the gallop                                                       */
/* ------------------------------------------------------------------ */
static void track_run(void)
{
    begin(FIGHT_BPM, 64);
    for (int bar = 0; bar < 16; bar++) {
        int c = chord_at(bar);
        float b0 = (float)bar * 4;
        /* galloping bass: da-da-dum */
        for (int bt = 0; bt < 4; bt++) {
            sbass(b0 + (float)bt, 0.22f, ROOT[c], 0.75f, 0.5f);
            sbass(b0 + (float)bt + 0.5f, 0.2f, ROOT[c], 0.5f, 0.3f);
            sbass(b0 + (float)bt + 0.75f, 0.2f, ROOT[c] + (bt == 3 ? 7 : 0), 0.55f, 0.3f);
        }
        /* tom groove and a kick */
        kick(b0, 0.85f); kick(b0 + 2, 0.75f);
        tom(b0 + 1, 45, 0.5f); tom(b0 + 3, 43, 0.5f); tom(b0 + 3.5f, 47, 0.35f);
        for (int q = 0; q < 8; q++) noise_hit(b0 + (float)q * 0.5f, (q & 1) ? 0.05f : 0.08f, 0.02f, 7500, 1.5f);   /* shaker */
        if (bar % 4 == 3) for (int q = 0; q < 4; q++) tom(b0 + 2 + (float)q * 0.5f, 50.0f - (float)q * 2.0f, 0.55f);
        /* twangy guitar: a riff on each chord */
        static const float RIFF[8][2] = { {0, 0}, {0.5f, 7}, {1, 12}, {1.5f, 10}, {2, 7}, {2.5f, 5}, {3, 7}, {3.5f, 3} };
        for (int k = 0; k < 8; k++) if ((bar & 1) == 0 || k < 4) pluck(b0 + RIFF[k][0], 0.45f, ROOT[c] + 24 + RIFF[k][1], 0.32f, 0.85f);
    }
    /* the whistle comes in for the second half */
    static const float MEL[][3] = {
        { 32, 2, 76 }, { 34, 1, 79 }, { 35, 1, 83 }, { 36, 3, 84 }, { 39, 1, 83 },
        { 40, 2, 81 }, { 42, 2, 78 }, { 44, 4, 81 },
        { 48, 2, 79 }, { 50, 1, 78 }, { 51, 1, 76 }, { 52, 4, 75 }, { 56, 4, 78 }, { 60, 4, 76 },
    };
    for (unsigned i = 0; i < sizeof MEL / sizeof MEL[0]; i++) whistle(MEL[i][0], MEL[i][1], MEL[i][2], 0.3f);
    reverb(0.3f, 1.1f);
    finish(MUS_RUN);
}

/* ------------------------------------------------------------------ */
/* CHAOS: the metal layer                                               */
/* ------------------------------------------------------------------ */
static void track_chaos(void)
{
    begin(FIGHT_BPM, 64);
    for (int bar = 0; bar < 16; bar++) {
        int c = chord_at(bar);
        float b0 = (float)bar * 4;
        for (int q = 0; q < 16; q++) kick(b0 + (float)q * 0.25f, (q & 1) ? 0.55f : 0.8f);   /* double kick */
        snare(b0 + 1, 1.0f); snare(b0 + 3, 1.0f);
        if (bar % 2 == 1) { snare(b0 + 3.5f, 0.5f); snare(b0 + 3.75f, 0.6f); }
        if (bar % 2 == 0) crash(b0, 0.7f);
        for (int q = 0; q < 8; q++) noise_hit(b0 + (float)q * 0.5f, 0.07f, 0.015f, 9500, 2.0f);
        /* chugging riff: muted 16ths, open chord stabs */
        static const int PAT[16] = { 1, 1, 0, 1, 2, 0, 1, 1, 0, 1, 2, 0, 1, 0, 2, 0 };
        for (int q = 0; q < 16; q++) {
            if (PAT[q] == 1) power(b0 + (float)q * 0.25f, 0.2f, ROOT[c] + 12, 0.8f, 1.0f);
            if (PAT[q] == 2) power(b0 + (float)q * 0.25f, 0.45f, ROOT[c] + 12 + ((bar & 1) && q > 8 ? 3 : 0), 0.9f, 0.0f);
        }
        if ((bar & 1) == 0) {
            float ch[3] = { CHORD[c][0], CHORD[c][1], CHORD[c][2] };
            stab(b0, 1.0f, ch, 3, 0.6f);
            for (int k = 0; k < 3; k++) pad(b0, 8.0f, CHORD[c][k], 0.4f, 1600.0f);     /* choir-ish */
        }
    }
    reverb(0.2f, 1.0f);
    finish(MUS_CHAOS);
}

/* ------------------------------------------------------------------ */
/* BOSS                                                                 */
/* ------------------------------------------------------------------ */
static void track_boss(void)
{
    begin(FIGHT_BPM, 64);
    static const float BR[4] = { 40, 41, 40, 39 };                    /* E  F  E  D#: Phrygian dread */
    static const float BC[4][3] = { {64, 67, 71}, {65, 69, 72}, {64, 67, 71}, {63, 66, 71} };
    for (int bar = 0; bar < 16; bar++) {
        int c = (bar / 2) % 4;
        float b0 = (float)bar * 4;
        static const float KICKS[] = { 0, 0.75f, 1.5f, 2, 2.5f, 2.75f, 3.5f };
        for (unsigned k = 0; k < sizeof KICKS / sizeof KICKS[0]; k++) kick(b0 + KICKS[k], 1.0f);
        snare(b0 + 1, 1.0f); snare(b0 + 3, 1.0f);
        if (bar % 4 == 3) for (int q = 0; q < 8; q++) tom(b0 + 2 + (float)q * 0.25f, 52.0f - (float)q * 1.5f, 0.6f);
        if (bar % 2 == 0) crash(b0, 0.8f);
        for (int q = 0; q < 16; q++) noise_hit(b0 + (float)q * 0.25f, (q & 1) ? 0.04f : 0.08f, 0.012f, 9000, 2.0f);
        for (int q = 0; q < 16; q++) sbass(b0 + (float)q * 0.25f, 0.2f, BR[c] + ((q % 4 == 3) ? 12 : 0), 0.8f, 0.8f);
        for (int q = 0; q < 4; q++) power(b0 + (float)q, q == 0 ? 0.9f : 0.2f, BR[c] + 12, 0.75f, q == 0 ? 0.0f : 1.0f);
        if ((bar & 1) == 0) for (int k = 0; k < 3; k++) organ(b0, 8.0f, BC[c][k], 0.55f);
        if (bar % 2 == 1) { float st[3] = { BC[c][0] + 12, BC[c][1] + 12, BC[c][2] + 12 }; stab(b0 + 2.5f, 0.6f, st, 3, 0.7f); }
        bell(b0, BC[c][0] + 24, 0.25f, 0.8f);
    }
    reverb(0.25f, 1.2f);
    finish(MUS_BOSS);
}

/* ------------------------------------------------------------------ */
/* SHOP: a saloon piano                                                  */
/* ------------------------------------------------------------------ */
static void track_shop(void)
{
    begin(FIGHT_BPM, 64);                    /* same grid, played at half feel */
    static const float SR_[4] = { 43, 48, 38, 43 };                  /* G  C  D  G */
    static const float SC[4][3] = { {67, 71, 74}, {67, 72, 76}, {66, 69, 74}, {67, 71, 74} };
    for (int bar = 0; bar < 16; bar++) {
        int c = (bar / 2) % 4;
        float b0 = (float)bar * 4;
        /* oom-pah */
        epiano(b0, 0.9f, SR_[c], 0.55f);
        epiano(b0 + 2, 0.9f, SR_[c] + 7, 0.45f);
        for (int k = 0; k < 3; k++) { epiano(b0 + 1, 0.5f, SC[c][k] - 12, 0.28f); epiano(b0 + 3, 0.5f, SC[c][k] - 12, 0.28f); }
        noise_hit(b0 + 1, 0.04f, 0.02f, 6000, 2.0f); noise_hit(b0 + 3, 0.04f, 0.02f, 6000, 2.0f);
    }
    static const float MEL[][3] = {
        { 0, 1, 79 }, { 1, 1, 83 }, { 2, 2, 86 }, { 4, 1, 84 }, { 5, 1, 83 }, { 6, 2, 81 },
        { 8, 1, 79 }, { 9, 1, 81 }, { 10, 1, 83 }, { 11, 1, 84 }, { 12, 4, 83 },
        { 16, 1, 81 }, { 17, 1, 78 }, { 18, 2, 81 }, { 20, 1, 86 }, { 21, 1, 84 }, { 22, 2, 81 },
        { 24, 2, 79 }, { 26, 2, 78 }, { 28, 4, 79 },
    };
    for (int rep = 0; rep < 2; rep++)
        for (unsigned i = 0; i < sizeof MEL / sizeof MEL[0]; i++) epiano(MEL[i][0] + 32.0f * (float)rep, MEL[i][1], MEL[i][2] - (rep ? 0 : 0), 0.4f);
    reverb(0.35f, 1.0f);
    finish(MUS_SHOP);
}

void music_init(void)
{
    if (ready) return;
    ready = 1;
    mix = (float *)big_alloc(sizeof(float) * MUS_MAX);
    track_menu();
    track_run();
    track_chaos();
    track_boss();
    track_shop();
}

const int16_t *music_data(int track) { return MUS[track]; }
int            music_len(int track)  { return mus_len[track]; }
int            music_volume(int track) { return VOL[track]; }
