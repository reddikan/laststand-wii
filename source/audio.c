/*
 * LAST STAND - sound effect synthesis. See audio.h.
 *
 * Every effect is built at start-up from a small synth (band-limited and FM oscillators, resonant filtered
 * noise and a short echo). Guns are a crack, a body thump and a tail; explosions are layered low booms with
 * debris; the announcer is formant speech run through a big gravelly distortion and a hall echo.
 */
#include "audio.h"
#include "speech.h"

#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define MAXLEN 96000                       /* longest single effect: 3 s at 32 kHz */
#define POOLN  (2600000)                   /* all effects together (~5 MB) */

static int16_t POOL[POOLN] __attribute__((aligned(32)));
static int     pool_used;
static int     sfx_off[SFX_COUNT], sfx_len[SFX_COUNT];
static float   mixbuf[MAXLEN];
static float   outbuf[MAXLEN];
static int     mixend;
static uint32_t rng = 0x2545F491u;
static int     ready;

/* playback volume per effect (every effect is normalised first, then scaled by this) */
static const uint8_t VOL[SFX_COUNT] = {
    /* REVOLVER */ 150, /* SCATTER */ 205, /* GATLING */ 120, /* SPINUP */ 120, /* THROW */ 110, /* BOOM */ 215, /* BOOM_BIG */ 255,
    /* TESLA */ 140, /* RAIL */ 220, /* CASING */ 60,
    /* HIT */ 85, /* SPLAT */ 120, /* BONES */ 120, /* SHIELD */ 110, /* IMP */ 140, /* BURN */ 100,
    /* HURT */ 200, /* DASH */ 130, /* COIN */ 95, /* GEM */ 110, /* HEAL */ 170, /* POWERUP */ 190, /* CHEST */ 200, /* NUKE */ 255, /* LASTSTAND */ 255, /* SWAP */ 120,
    /* EORB */ 90, /* EBONE */ 90, /* HOP */ 70, /* ROAR */ 240, /* SIREN */ 200, /* SLAM */ 250, /* LASER */ 230, /* CHARGE */ 170, /* SUMMON */ 190, /* SCREECH */ 170,
    /* THUNDER */ 220, /* BARREL */ 200,
    /* MOVE */ 55, /* SELECT */ 120, /* BUY */ 170, /* ERROR */ 130, /* PAUSE */ 110, /* HORN */ 210, /* WAVECLEAR */ 210, /* GAMEOVER */ 220, /* VICTORY */ 220, /* TICK */ 100,
    /* voices */ 245, 240, 245, 240, 245, 245, 240, 240,
};

enum { W_SIN, W_TRI, W_SOFT, W_FM };           /* oscillator shapes */
enum { F_LP, F_BP, F_HP };                      /* noise filter types */

#define SR ((float)SFX_RATE)
#define TABN 4096
static float sine_tab[TABN + 1];

static inline float sine01(float ph)            /* ph in [0,1) */
{
    float x = ph * (float)TABN;
    int i = (int)x;
    float fr = x - (float)i;
    return sine_tab[i] + (sine_tab[i + 1] - sine_tab[i]) * fr;
}

static float frand(void)                        /* -1 .. 1 */
{
    rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
    return (float)(rng >> 8) / 8388608.0f - 1.0f;
}

/*
 * One pitched voice mixed into the effect being built.
 *   start, dur : seconds
 *   f0 -> f1   : exponential pitch glide in Hz
 *   atk        : attack time (s);  tau : decay time constant (s), 0 = sustained
 *   W_SIN      : pure sine
 *   W_TRI      : soft triangle (sine + a few odd harmonics)
 *   W_SOFT     : warm band-limited saw; a = extra high-harmonic roll-off (0 bright .. 2 dark)
 *   W_FM       : two-operator FM; a = modulator/carrier ratio, b = peak modulation index (fades with the note)
 */
static void tone(float start, float dur, float f0, float f1, float vol, float atk, float tau, int wave, float a, float b)
{
    int s0 = (int)(start * SR), n = (int)(dur * SR);
    float ph = 0.0f, pm = 0.0f;
    float glide = (f1 == f0) ? 1.0f : powf(f1 / f0, 1.0f / (float)(n > 1 ? n : 1));
    float f = f0, hw[10];
    for (int h = 1; h <= 9; h++) hw[h] = 1.0f / powf((float)h, 1.0f + a);
    float env = 1.0f, k = (tau > 0.0f) ? expf(-1.0f / (tau * SR)) : 1.0f;
    float atk_n = atk * SR > 1.0f ? atk * SR : 1.0f;
    float rel_n = 0.006f * SR;
    for (int i = 0; i < n && s0 + i < MAXLEN; i++) {
        float amp = env;
        if ((float)i < atk_n) amp *= (float)i / atk_n;
        if ((float)(n - i) < rel_n) amp *= (float)(n - i) / rel_n;
        float v;
        switch (wave) {
        case W_TRI:
            v = sine01(ph) + 0.111f * sine01(ph * 3.0f - floorf(ph * 3.0f)) * -1.0f + 0.04f * sine01(ph * 5.0f - floorf(ph * 5.0f));
            break;
        case W_SOFT: {
            v = 0.0f;
            int nh = (int)(0.42f * SR / (f > 1.0f ? f : 1.0f));
            if (nh > 9) nh = 9;
            for (int h = 1; h <= nh; h++) {
                float x = ph * (float)h;
                v += sine01(x - floorf(x)) * hw[h];
            }
            break;
        }
        case W_FM: {
            float idx = b * env;
            float m = sine01(pm);
            float x = ph + idx * m / (2.0f * (float)M_PI) * 2.0f;
            x -= floorf(x);
            v = sine01(x);
            break;
        }
        default:
            v = sine01(ph);
            break;
        }
        mixbuf[s0 + i] += v * amp * vol;
        if (s0 + i + 1 > mixend) mixend = s0 + i + 1;
        ph += f / SR; if (ph >= 1.0f) ph -= 1.0f;
        pm += f * a / SR; pm -= floorf(pm);
        f *= glide;
        env *= k;
    }
}

/* resonant filtered noise: fc0 -> fc1 is the filter cutoff sweep in Hz, q the resonance */
static void noise(float start, float dur, float fc0, float fc1, float q, float vol, float atk, float tau, int mode)
{
    int s0 = (int)(start * SR), n = (int)(dur * SR);
    float z1 = 0.0f, z2 = 0.0f;                  /* transposed direct form II state */
    float b0 = 0, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    float env = 1.0f, k = (tau > 0.0f) ? expf(-1.0f / (tau * SR)) : 1.0f;
    float atk_n = atk * SR > 1.0f ? atk * SR : 1.0f;
    float rel_n = 0.006f * SR;
    float ratio = (fc1 == fc0) ? 1.0f : powf(fc1 / fc0, 1.0f / (float)(n > 1 ? n : 1));
    float fc = fc0;
    for (int i = 0; i < n && s0 + i < MAXLEN; i++) {
        if ((i & 7) == 0) {                       /* retune the filter every 8 samples */
            float w = 2.0f * (float)M_PI * (fc < 0.45f * SR ? fc : 0.45f * SR) / SR;
            float cw = cosf(w), alpha = sinf(w) / (2.0f * q), a0 = 1.0f + alpha;
            switch (mode) {
            case F_LP: b0 = (1 - cw) / 2; b1 = 1 - cw; b2 = (1 - cw) / 2; break;
            case F_HP: b0 = (1 + cw) / 2; b1 = -(1 + cw); b2 = (1 + cw) / 2; break;
            default:   b0 = alpha; b1 = 0; b2 = -alpha; break;
            }
            a1 = -2.0f * cw / a0; a2 = (1.0f - alpha) / a0;
            b0 /= a0; b1 /= a0; b2 /= a0;
        }
        float x = frand();
        float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        float amp = env;
        if ((float)i < atk_n) amp *= (float)i / atk_n;
        if ((float)(n - i) < rel_n) amp *= (float)(n - i) / rel_n;
        mixbuf[s0 + i] += y * amp * vol;
        if (s0 + i + 1 > mixend) mixend = s0 + i + 1;
        env *= k;
        fc *= ratio;
    }
}

static void begin(void)
{
    memset(mixbuf, 0, sizeof mixbuf);
    mixend = 0;
}

/* mix down with a two-tap room echo, normalise, store in the pool as 16-bit */
static void finish_fx(int id, float wet, float room)
{
    int d1 = (int)((0.045f + 0.07f * room) * SR), d2 = d1 * 2 + (int)(0.013f * SR);
    int tail = (wet > 0.0f) ? d2 + (int)(0.05f * SR) : (int)(0.03f * SR);
    int len = mixend + tail;
    if (len > MAXLEN) len = MAXLEN;
    float peak = 0.0f;
    for (int i = 0; i < len; i++) {
        float x = (i < mixend) ? mixbuf[i] : 0.0f;
        if (i >= d1 && i - d1 < mixend) x += wet * 0.6f * mixbuf[i - d1];
        if (i >= d2 && i - d2 < mixend) x += wet * 0.3f * mixbuf[i - d2];
        outbuf[i] = x;
        if (fabsf(x) > peak) peak = fabsf(x);
    }
    int last = len;
    while (last > 1 && fabsf(outbuf[last - 1]) < peak * 0.003f) last--;
    last += (int)(0.02f * SR);
    if (last < len) len = last;
    int fade = (int)(0.02f * SR);
    float scale = peak > 0.0f ? 0.85f * 32000.0f / peak : 0.0f;
    int outlen = (len + 15) & ~15;
    if (pool_used + outlen > POOLN) outlen = (POOLN - pool_used) & ~15;
    if (outlen < 16) { sfx_off[id] = 0; sfx_len[id] = 16; return; }
    int16_t *dst = POOL + pool_used;
    memset(dst, 0, (size_t)outlen * 2);
    for (int i = 0; i < len && i < outlen; i++) {
        float x = outbuf[i];
        if (len - i < fade) x *= (float)(len - i) / (float)fade;
        dst[i] = (int16_t)(x * scale);
    }
    sfx_off[id] = pool_used;
    sfx_len[id] = outlen;
    pool_used += outlen;
}

/* soft saturation for a bit of grit */
static float sat(float x) { return x / (1.0f + fabsf(x)); }

static void bell(float t, float f, float vol, float tau)
{
    tone(t, tau * 5.0f, f, f, vol, 0.002f, tau, W_FM, 3.0f, 1.1f);
    tone(t, tau * 3.0f, f * 2.0f, f * 2.0f, vol * 0.25f, 0.002f, tau * 0.5f, W_SIN, 0, 0);
}

/* a gunshot: crack + body + room tail. bright: high crack level, body: low thump Hz, size: tail length */
static void gunshot(float bright, float body, float size, float dark)
{
    noise(0.0f, 0.06f, 9000 - dark * 3000, 2500, 0.7f, 1.0f * bright, 0.0005f, 0.012f, F_HP);
    noise(0.0f, 0.12f + size * 0.1f, 2600 - dark * 900, 500, 0.8f, 0.9f, 0.0008f, 0.04f + size * 0.05f, F_LP);
    tone(0.0f, 0.16f + size * 0.1f, body * 2.2f, body * 0.7f, 0.8f, 0.0008f, 0.035f + size * 0.03f, W_SIN, 0, 0);
    noise(0.012f, 0.4f + size * 0.6f, 1400 - dark * 400, 300, 0.7f, 0.18f + 0.1f * size, 0.01f, 0.12f + size * 0.2f, F_LP);   /* the room */
}


/* the announcer: a deep growling voice in a big hall */
static void announcer(int id, const char *phrase, float pitch, float rate)
{
    begin();
    int n = speech_render(phrase, pitch, rate, 0.55f, mixbuf + 400, MAXLEN - 12000, SR);
    float peak = 1e-6f;
    for (int i = 0; i < n; i++) if (fabsf(mixbuf[400 + i]) > peak) peak = fabsf(mixbuf[400 + i]);
    float lp = 0;
    for (int i = 0; i < n; i++) {
        float x = mixbuf[400 + i] / peak;
        x = sat(x * 2.2f);
        lp += (x - lp) * 0.6f;
        mixbuf[400 + i] = lp;
    }
    mixend = 400 + n;
    tone(0.0f, (float)n / SR + 0.3f, 55, 50, 0.25f, 0.02f, 0.6f, W_SIN, 0, 0);       /* a sub under it */
    finish_fx(id, 0.55f, 1.0f);
}

static void explosion(float size, float dark)
{
    tone(0, 0.9f + size, 90 - dark * 20, 26, 1.0f, 0.001f, 0.22f + size * 0.25f, W_SIN, 0, 0);
    noise(0, 1.0f + size, 4200 - dark * 1500, 80, 0.6f, 1.0f, 0.0008f, 0.2f + size * 0.25f, F_LP);
    noise(0, 0.1f, 9000, 3000, 0.8f, 0.6f, 0.0005f, 0.025f, F_HP);
    for (int i = 0; i < 6 + (int)(size * 8); i++)
        noise(0.04f + 0.05f * (float)i + 0.02f * frand(), 0.06f, 2400 - 120 * (float)i, 700, 2.0f, 0.18f, 0.001f, 0.02f, F_BP);   /* debris */
    noise(0.2f, 1.0f + size * 1.2f, 400, 90, 0.8f, 0.35f + size * 0.2f, 0.15f, 0.45f + size * 0.4f, F_LP);                   /* rumble */
}

void audio_init(void)
{
    if (ready) return;
    ready = 1;
    pool_used = 0;
    for (int i = 0; i <= TABN; i++) sine_tab[i] = sinf((float)i * 2.0f * (float)M_PI / (float)TABN);

    /* --- weapons --- */
    begin(); gunshot(1.1f, 130, 0.45f, 0.0f); bell(0.0f, 2900, 0.12f, 0.03f); finish_fx(SFX_REVOLVER, 0.3f, 0.6f);
    begin();                                                         /* dragon's breath: a boom with a fiery roar */
    gunshot(0.9f, 70, 1.0f, 0.3f);
    tone(0.0f, 0.3f, 120, 38, 0.9f, 0.001f, 0.09f, W_SIN, 0, 0);
    noise(0.02f, 0.5f, 1200, 400, 0.7f, 0.6f, 0.02f, 0.15f, F_BP);
    noise(0.38f, 0.03f, 2200, 2200, 2.0f, 0.3f, 0.001f, 0.01f, F_BP);
    noise(0.48f, 0.04f, 1600, 1600, 2.0f, 0.35f, 0.001f, 0.012f, F_BP);
    finish_fx(SFX_SCATTER, 0.35f, 0.7f);
    begin();                                                         /* gatling: a short hard crack */
    noise(0.0f, 0.04f, 7000, 2500, 0.7f, 0.9f, 0.0005f, 0.01f, F_HP);
    noise(0.0f, 0.07f, 2200, 500, 0.8f, 0.8f, 0.0008f, 0.02f, F_LP);
    tone(0.0f, 0.07f, 180, 70, 0.7f, 0.0008f, 0.02f, W_SIN, 0, 0);
    finish_fx(SFX_GATLING, 0.1f, 0.3f);
    begin();                                                         /* spin up: a motor whine rising */
    for (int i = 0; i < 3; i++) tone(0, 0.6f, 120 + 40 * (float)i, 520 + 120 * (float)i, 0.3f, 0.05f, 0, W_SOFT, 0.4f, 0);
    noise(0, 0.6f, 800, 3200, 3.0f, 0.3f, 0.1f, 0, F_BP);
    finish_fx(SFX_SPINUP, 0.0f, 0.0f);
    begin();                                                         /* throw: a fuse hiss and a whoosh */
    noise(0, 0.25f, 6000, 6000, 1.0f, 0.3f, 0.005f, 0.1f, F_HP);
    noise(0.03f, 0.25f, 600, 1800, 1.0f, 0.5f, 0.08f, 0.08f, F_BP);
    finish_fx(SFX_THROW, 0.1f, 0.2f);
    begin(); explosion(0.3f, 0.0f); finish_fx(SFX_BOOM, 0.4f, 0.8f);
    begin(); explosion(1.0f, 0.4f); tone(0, 2.0f, 45, 28, 0.8f, 0.01f, 0.8f, W_SIN, 0, 0); finish_fx(SFX_BOOM_BIG, 0.5f, 1.0f);
    begin();                                                         /* tesla: an electric snap and buzz */
    noise(0, 0.03f, 8000, 8000, 1.0f, 1.0f, 0.0005f, 0.008f, F_HP);
    tone(0, 0.22f, 120, 110, 0.7f, 0.001f, 0.08f, W_FM, 7.3f, 6.0f);
    tone(0, 0.18f, 1900, 600, 0.3f, 0.001f, 0.05f, W_FM, 1.41f, 3.0f);
    for (int i = 0; i < 4; i++) noise(0.03f + 0.035f * (float)i, 0.02f, 5000, 5000, 2.0f, 0.4f, 0.001f, 0.005f, F_BP);
    finish_fx(SFX_TESLA, 0.15f, 0.3f);
    begin();                                                         /* sun lance: a charge-whine and a searing beam */
    tone(0, 0.12f, 400, 2600, 0.5f, 0.005f, 0, W_FM, 2.0f, 2.0f);
    tone(0.1f, 0.6f, 220, 110, 0.8f, 0.001f, 0.2f, W_FM, 1.5f, 4.0f);
    noise(0.1f, 0.5f, 7000, 1500, 0.7f, 0.8f, 0.001f, 0.12f, F_HP);
    tone(0.1f, 0.5f, 70, 40, 0.9f, 0.001f, 0.15f, W_SIN, 0, 0);
    finish_fx(SFX_RAIL, 0.35f, 0.8f);
    begin(); bell(0, 4100, 0.5f, 0.025f); bell(0.05f, 5200, 0.3f, 0.02f); finish_fx(SFX_CASING, 0.0f, 0.0f);

    /* --- hits and deaths --- */
    begin(); noise(0, 0.06f, 900, 300, 1.0f, 1.0f, 0.001f, 0.015f, F_LP); tone(0, 0.06f, 200, 80, 0.5f, 0.001f, 0.02f, W_SIN, 0, 0); finish_fx(SFX_HIT, 0.0f, 0.0f);
    begin();                                                         /* splat: wet and nasty */
    noise(0, 0.2f, 1400, 200, 1.4f, 1.0f, 0.001f, 0.05f, F_LP);
    tone(0, 0.15f, 160, 50, 0.6f, 0.001f, 0.04f, W_SIN, 0, 0);
    for (int i = 0; i < 4; i++) noise(0.04f + 0.03f * (float)i, 0.04f, 900 + 300 * frand(), 400, 3.0f, 0.25f, 0.001f, 0.012f, F_BP);
    finish_fx(SFX_SPLAT, 0.1f, 0.2f);
    begin();                                                         /* bones clattering */
    for (int i = 0; i < 7; i++) { float t = 0.025f * (float)i + 0.01f * frand(); noise(t, 0.03f, 2600 + 900 * frand(), 1800, 5.0f, 0.5f, 0.0005f, 0.008f, F_BP); tone(t, 0.04f, 900 + 500 * frand(), 700, 0.15f, 0.001f, 0.01f, W_TRI, 0, 0); }
    finish_fx(SFX_BONES, 0.15f, 0.2f);
    begin(); bell(0, 1700, 0.5f, 0.06f); bell(0, 2300, 0.3f, 0.04f); noise(0, 0.04f, 4000, 4000, 2.0f, 0.4f, 0.001f, 0.01f, F_BP); finish_fx(SFX_SHIELD, 0.2f, 0.3f);
    begin(); explosion(0.0f, 0.0f); tone(0, 0.15f, 900, 200, 0.4f, 0.001f, 0.05f, W_FM, 2.0f, 2.0f); finish_fx(SFX_IMP, 0.2f, 0.4f);
    begin(); noise(0, 0.5f, 1500, 600, 0.6f, 0.6f, 0.05f, 0.2f, F_BP); finish_fx(SFX_BURN, 0.1f, 0.2f);

    /* --- the player --- */
    begin();                                                         /* hurt: a thump and a grunt */
    noise(0, 0.1f, 900, 300, 1.0f, 1.0f, 0.001f, 0.03f, F_LP);
    tone(0, 0.18f, 180, 110, 0.7f, 0.005f, 0.07f, W_SOFT, 0.3f, 0);
    tone(0.02f, 0.16f, 240, 140, 0.4f, 0.01f, 0.06f, W_FM, 1.0f, 2.0f);
    finish_fx(SFX_HURT, 0.15f, 0.2f);
    begin(); noise(0, 0.22f, 500, 4000, 1.5f, 0.8f, 0.03f, 0.07f, F_BP); tone(0, 0.2f, 300, 900, 0.15f, 0.02f, 0.07f, W_SIN, 0, 0); finish_fx(SFX_DASH, 0.1f, 0.2f);
    begin(); bell(0, 2093, 0.45f, 0.05f); bell(0.04f, 3136, 0.35f, 0.06f); finish_fx(SFX_COIN, 0.0f, 0.0f);
    begin(); bell(0, 1568, 0.4f, 0.08f); bell(0.05f, 2349, 0.35f, 0.08f); bell(0.1f, 3136, 0.3f, 0.1f); finish_fx(SFX_GEM, 0.2f, 0.2f);
    begin(); { static const float N[4] = { 523, 659, 784, 1047 }; for (int i = 0; i < 4; i++) tone(0.06f * (float)i, 0.4f, N[i], N[i], 0.35f, 0.005f, 0.15f, W_TRI, 0, 0); } finish_fx(SFX_HEAL, 0.3f, 0.3f);
    begin(); tone(0, 0.5f, 300, 1200, 0.5f, 0.01f, 0, W_FM, 2.0f, 1.5f); tone(0.25f, 0.5f, 600, 2400, 0.3f, 0.01f, 0.2f, W_FM, 3.0f, 1.0f); noise(0, 0.6f, 1000, 6000, 2.0f, 0.3f, 0.2f, 0.2f, F_BP); finish_fx(SFX_POWERUP, 0.3f, 0.4f);
    begin();                                                         /* chest: creak, then a glittering fanfare */
    tone(0, 0.3f, 300, 420, 0.3f, 0.05f, 0.15f, W_FM, 2.6f, 2.5f);
    { static const float N[6] = { 784, 988, 1175, 1568, 1976, 2349 }; for (int i = 0; i < 6; i++) bell(0.25f + 0.05f * (float)i, N[i], 0.35f, 0.15f); }
    finish_fx(SFX_CHEST, 0.4f, 0.5f);
    begin(); explosion(1.6f, 0.6f); tone(0, 3.0f, 38, 22, 1.0f, 0.01f, 1.2f, W_SIN, 0, 0); finish_fx(SFX_NUKE, 0.6f, 1.0f);
    begin();                                                         /* last stand: a heartbeat slam and a choir-ish chord */
    tone(0, 0.3f, 90, 40, 1.0f, 0.001f, 0.1f, W_SIN, 0, 0);
    tone(0.35f, 0.3f, 90, 40, 1.0f, 0.001f, 0.1f, W_SIN, 0, 0);
    for (int i = 0; i < 4; i++) { static const float C4[4] = { 110, 165, 220, 262 }; tone(0.3f, 2.2f, C4[i], C4[i] * 1.01f, 0.25f, 0.2f, 0.9f, W_SOFT, 0.5f, 0); }
    noise(0.3f, 1.8f, 300, 3000, 1.0f, 0.4f, 0.4f, 0.6f, F_BP);
    finish_fx(SFX_LASTSTAND, 0.5f, 1.0f);
    begin(); noise(0, 0.03f, 3000, 3000, 3.0f, 0.7f, 0.0005f, 0.006f, F_BP); tone(0, 0.04f, 900, 700, 0.3f, 0.001f, 0.01f, W_SIN, 0, 0);
             noise(0.07f, 0.03f, 2200, 2200, 3.0f, 0.8f, 0.0005f, 0.008f, F_BP); tone(0.07f, 0.04f, 600, 500, 0.3f, 0.001f, 0.012f, W_SIN, 0, 0); finish_fx(SFX_SWAP, 0.1f, 0.1f);

    /* --- enemies and bosses --- */
    begin(); tone(0, 0.18f, 900, 300, 0.5f, 0.002f, 0.06f, W_FM, 1.5f, 2.0f); noise(0, 0.1f, 2000, 800, 2.0f, 0.3f, 0.002f, 0.03f, F_BP); finish_fx(SFX_EORB, 0.2f, 0.3f);
    begin(); noise(0, 0.15f, 800, 2400, 1.5f, 0.6f, 0.01f, 0.05f, F_BP); tone(0, 0.06f, 700, 500, 0.2f, 0.001f, 0.02f, W_TRI, 0, 0); finish_fx(SFX_EBONE, 0.1f, 0.1f);
    begin(); tone(0, 0.12f, 140, 260, 0.6f, 0.002f, 0.04f, W_FM, 1.0f, 3.0f); finish_fx(SFX_HOP, 0.05f, 0.1f);
    begin();                                                         /* the boss roars */
    tone(0, 1.6f, 70, 52, 0.8f, 0.05f, 0.6f, W_FM, 0.51f, 6.0f);
    tone(0, 1.5f, 105, 80, 0.6f, 0.05f, 0.6f, W_FM, 1.49f, 4.0f);
    noise(0, 1.6f, 900, 300, 1.5f, 0.8f, 0.05f, 0.6f, F_BP);
    noise(0, 1.4f, 2600, 900, 2.0f, 0.3f, 0.05f, 0.5f, F_BP);
    finish_fx(SFX_ROAR, 0.45f, 1.0f);
    begin(); for (int i = 0; i < 3; i++) { tone(0.6f * (float)i, 0.3f, 600, 900, 0.5f, 0.02f, 0, W_SOFT, 0.2f, 0); tone(0.6f * (float)i + 0.3f, 0.3f, 900, 600, 0.5f, 0.0f, 0, W_SOFT, 0.2f, 0); } finish_fx(SFX_SIREN, 0.4f, 0.8f);
    begin(); explosion(0.8f, 0.9f); tone(0, 1.2f, 50, 25, 1.0f, 0.001f, 0.35f, W_SIN, 0, 0); finish_fx(SFX_SLAM, 0.5f, 1.0f);
    begin(); tone(0, 2.2f, 90, 85, 0.6f, 0.05f, 0, W_FM, 2.01f, 5.0f); tone(0, 2.2f, 2400, 2300, 0.15f, 0.05f, 0, W_FM, 1.5f, 1.0f); noise(0, 2.2f, 5000, 3000, 1.0f, 0.4f, 0.05f, 0, F_BP); finish_fx(SFX_LASER, 0.3f, 0.6f);
    begin(); tone(0, 0.9f, 200, 1100, 0.5f, 0.6f, 0, W_FM, 2.0f, 2.0f); noise(0, 0.9f, 400, 4000, 2.0f, 0.4f, 0.7f, 0, F_BP); finish_fx(SFX_CHARGE, 0.2f, 0.4f);
    begin(); tone(0, 1.0f, 80, 160, 0.6f, 0.1f, 0.4f, W_FM, 0.5f, 5.0f); noise(0, 1.0f, 300, 1500, 1.0f, 0.5f, 0.1f, 0.4f, F_BP); bell(0.1f, 220, 0.3f, 0.3f); finish_fx(SFX_SUMMON, 0.5f, 0.8f);
    begin(); for (int i = 0; i < 5; i++) tone(0.04f * (float)i, 0.6f, 900 + 140 * (float)i, 500 + 60 * (float)i, 0.2f, 0.02f, 0.25f, W_FM, 1.3f, 3.0f); noise(0, 0.6f, 2500, 1200, 2.0f, 0.4f, 0.02f, 0.25f, F_BP); finish_fx(SFX_SCREECH, 0.4f, 0.6f);

    /* --- world --- */
    begin();                                                         /* thunder */
    noise(0.0f, 2.8f, 260, 60, 0.7f, 1.0f, 0.03f, 0.8f, F_LP);
    noise(0.15f, 1.6f, 500, 90, 0.9f, 0.5f, 0.05f, 0.4f, F_LP);
    noise(0.55f, 1.8f, 300, 70, 0.9f, 0.55f, 0.08f, 0.5f, F_LP);
    noise(0.0f, 0.15f, 5000, 2000, 0.8f, 0.5f, 0.001f, 0.05f, F_HP);
    tone(0.0f, 2.4f, 42, 30, 0.5f, 0.05f, 0.8f, W_SIN, 0, 0);
    finish_fx(SFX_THUNDER, 0.0f, 0.0f);
    begin(); explosion(0.9f, 0.2f); for (int i = 0; i < 10; i++) noise(0.05f + 0.04f * (float)i, 0.05f, 1800, 900, 3.0f, 0.2f, 0.001f, 0.015f, F_BP); finish_fx(SFX_BARREL, 0.5f, 0.9f);

    /* --- interface --- */
    begin(); tone(0, 0.04f, 1500, 1300, 0.5f, 0.001f, 0.01f, W_SIN, 0, 0); finish_fx(SFX_MOVE, 0.0f, 0.0f);
    begin(); tone(0, 0.06f, 900, 1100, 0.5f, 0.002f, 0.025f, W_SIN, 0, 0); tone(0.04f, 0.1f, 1500, 1500, 0.35f, 0.002f, 0.04f, W_FM, 2.0f, 0.5f); finish_fx(SFX_SELECT, 0.2f, 0.2f);
    begin(); bell(0, 1318, 0.4f, 0.08f); bell(0.06f, 1760, 0.4f, 0.1f); noise(0, 0.15f, 3000, 3000, 2.0f, 0.2f, 0.001f, 0.05f, F_BP); bell(0.12f, 2637, 0.35f, 0.12f); finish_fx(SFX_BUY, 0.3f, 0.3f);
    begin(); tone(0, 0.12f, 160, 150, 0.7f, 0.003f, 0.05f, W_SOFT, 0.0f, 0); tone(0.1f, 0.14f, 125, 115, 0.7f, 0.003f, 0.05f, W_SOFT, 0.0f, 0); finish_fx(SFX_ERROR, 0.0f, 0.0f);
    begin(); tone(0, 0.18f, 1200, 500, 0.5f, 0.004f, 0.07f, W_SIN, 0, 0); finish_fx(SFX_PAUSE, 0.3f, 0.3f);
    begin();                                                         /* the wave horn */
    tone(0, 1.4f, 98, 98, 0.6f, 0.08f, 0.6f, W_SOFT, 0.3f, 0);
    tone(0, 1.4f, 147, 147, 0.5f, 0.08f, 0.6f, W_SOFT, 0.4f, 0);
    tone(0, 1.4f, 196, 196, 0.3f, 0.1f, 0.6f, W_SOFT, 0.6f, 0);
    tone(0, 0.5f, 90, 40, 0.8f, 0.001f, 0.15f, W_SIN, 0, 0);
    finish_fx(SFX_HORN, 0.5f, 1.0f);
    begin(); { static const float N[5] = { 392, 523, 659, 784, 1047 }; for (int i = 0; i < 5; i++) tone(0.08f * (float)i, 0.5f, N[i], N[i], 0.35f, 0.005f, 0.2f, W_FM, 2.0f, 1.0f); }
             tone(0.4f, 1.2f, 130.8f, 130.8f, 0.4f, 0.03f, 0.5f, W_SOFT, 0.6f, 0); finish_fx(SFX_WAVECLEAR, 0.45f, 0.6f);
    begin(); tone(0, 2.4f, 98, 92, 0.6f, 0.02f, 0.9f, W_SOFT, 0.4f, 0); tone(0, 2.4f, 104, 97, 0.5f, 0.02f, 0.9f, W_SOFT, 0.5f, 0);
             tone(0.2f, 2.2f, 147, 138, 0.3f, 0.05f, 0.8f, W_FM, 1.0f, 1.0f); tone(0, 0.5f, 80, 30, 0.9f, 0.001f, 0.2f, W_SIN, 0, 0); noise(0, 1.5f, 400, 120, 0.8f, 0.4f, 0.05f, 0.6f, F_LP); finish_fx(SFX_GAMEOVER, 0.5f, 0.9f);
    begin(); { static const float NF[7] = { 523.25f, 659.25f, 783.99f, 1046.5f, 783.99f, 1046.5f, 1318.5f }; static const float TT[7] = { 0, 0.12f, 0.24f, 0.36f, 0.6f, 0.72f, 0.84f };
               for (int i = 0; i < 7; i++) tone(TT[i], 0.7f, NF[i], NF[i], 0.32f, 0.005f, 0.3f, W_FM, 2.0f, 1.0f); }
             tone(0.84f, 1.8f, 130.8f, 130.8f, 0.4f, 0.03f, 0.8f, W_SOFT, 0.6f, 0); tone(0.84f, 1.8f, 196.0f, 196.0f, 0.3f, 0.03f, 0.8f, W_SOFT, 0.7f, 0); finish_fx(SFX_VICTORY, 0.5f, 0.7f);
    begin(); noise(0, 0.02f, 4000, 4000, 4.0f, 0.6f, 0.0005f, 0.006f, F_BP); tone(0, 0.03f, 2000, 2000, 0.3f, 0.001f, 0.008f, W_SIN, 0, 0); finish_fx(SFX_TICK, 0.1f, 0.1f);

    /* --- the announcer --- */
    const float A = 0.82f;
    announcer(SFX_V_LASTSTAND, "L AE^ S T _ S T AE^ N D", A, 1.15f);
    announcer(SFX_V_WAVE,      "W EY^ V _ K L IH^ R", A, 1.0f);
    announcer(SFX_V_BOSS,      "B AO^ S _ IH N K AH^ M IH NG", A, 1.0f);
    announcer(SFX_V_FRENZY,    "F R EH^ N Z IY", A, 1.0f);
    announcer(SFX_V_KABOOM,    "K AX B UW^ M", A, 1.1f);
    announcer(SFX_V_GAMEOVER,  "G EY^ M _ OW^ V ER", A * 0.95f, 1.2f);
    announcer(SFX_V_CLEARED,   "K L IH^ R D", A, 1.1f);
    announcer(SFX_V_RAMPAGE,   "R AE^ M P EY D ZH", A, 1.0f);
}

const int16_t *audio_data(int id) { return POOL + sfx_off[id]; }
int            audio_len(int id)  { return sfx_len[id]; }
int            audio_volume(int id) { return VOL[id]; }
const int16_t *audio_pool(void) { return POOL; }
int            audio_pool_len(void) { return pool_used; }
