/*
 * LAST STAND - Nintendo Wii platform layer (devkitPPC + libogc + libfat).
 *
 *   video : the game's triangle list is drawn by the Wii's GPU (GX): flat / gouraud shaded triangles
 *           with alpha, additive and 2x-multiply (lighting) blending, 60 fps, 16:9 anamorphic
 *           (or letterboxed inside 4:3 - press MINUS)
 *   input : Wii Classic Controller (in Wii Remote 1): buttons and both sticks
 *   audio : synthesized effects through ASND, pitch-shifted and panned per event (the busiest voice is
 *           stolen for loud effects when all are playing); the music tracks loop forever on voices
 *           0-4, in sync, restarted (at a biome's pitch) when the game asks, and faded in and out
 *   save  : a small file on the SD card (sd:/apps/laststand/save.dat)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <math.h>
#include <sys/stat.h>

#include <gccore.h>
#include <ogc/lwp_watchdog.h>
#include <wiiuse/wpad.h>
#include <asndlib.h>
#include <fat.h>

#include "game.h"
#include "music.h"
#include "gfx.h"

#define FIFO_SIZE (512 * 1024)
#define X_HOME  (1u << 16)
#define X_MINUS (1u << 17)
#define SAVE_DIR  "sd:/apps/laststand"
#define SAVE_PATH "sd:/apps/laststand/save.dat"

/* CPU-only working buffers go to MEM2 (see gfx.h) */
void *big_alloc(unsigned long bytes)
{
    u32 lo = ((u32)SYS_GetArena2Lo() + 31) & ~31u;
    SYS_SetArena2Lo((void *)(lo + ((bytes + 31) & ~31ul)));
    return (void *)lo;
}

static GXRModeObj *rmode;
static void       *xfb[2];
static int         fbi;
static int         wide = 1;
static volatile int quit;
static Game       *game;

/* ------------------------------------------------------------------ */
/* video                                                               */
/* ------------------------------------------------------------------ */
static void set_view(void)
{
    float w = (float)rmode->fbWidth, h = (float)rmode->efbHeight;
    if (wide) {
        GX_SetViewport(0, 0, w, h, 0, 1);
        GX_SetScissor(0, 0, rmode->fbWidth, rmode->efbHeight);
    } else {
        float bh = h * 0.75f, top = (h - bh) * 0.5f;
        GX_SetViewport(0, top, w, bh, 0, 1);
        GX_SetScissor(0, (u32)top, rmode->fbWidth, (u32)bh);
    }
}

static void video_init(void)
{
    VIDEO_Init();
    rmode = VIDEO_GetPreferredMode(NULL);
    if ((rmode->viTVMode >> 2) == VI_PAL) rmode = &TVEurgb60Hz480IntDf;     /* always 60 Hz */
    rmode->viWidth   = 678;
    rmode->viXOrigin = (VI_MAX_WIDTH_NTSC - rmode->viWidth) / 2;

    xfb[0] = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
    xfb[1] = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
    VIDEO_Configure(rmode);
    VIDEO_ClearFrameBuffer(rmode, xfb[0], COLOR_BLACK);
    VIDEO_ClearFrameBuffer(rmode, xfb[1], COLOR_BLACK);
    VIDEO_SetNextFramebuffer(xfb[0]);
    VIDEO_SetBlack(FALSE);
    VIDEO_Flush();
    VIDEO_WaitVSync();
    if (rmode->viTVMode & VI_NON_INTERLACE) VIDEO_WaitVSync();

    void *fifo = memalign(32, FIFO_SIZE);
    memset(fifo, 0, FIFO_SIZE);
    GX_Init(fifo, FIFO_SIZE);

    GXColor black = {0, 0, 0, 255};
    GX_SetCopyClear(black, 0x00FFFFFF);
    f32 yscale = GX_GetYScaleFactor(rmode->efbHeight, rmode->xfbHeight);
    u32 xfbh = GX_SetDispCopyYScale(yscale);
    GX_SetDispCopySrc(0, 0, rmode->fbWidth, rmode->efbHeight);
    GX_SetDispCopyDst(rmode->fbWidth, xfbh);
    GX_SetCopyFilter(rmode->aa, rmode->sample_pattern, GX_TRUE, rmode->vfilter);
    GX_SetFieldMode(rmode->field_rendering, ((rmode->viHeight == 2 * rmode->xfbHeight) ? GX_ENABLE : GX_DISABLE));
    GX_SetPixelFmt(rmode->aa ? GX_PF_RGB565_Z16 : GX_PF_RGB8_Z24, GX_ZC_LINEAR);
    GX_SetDispCopyGamma(GX_GM_1_0);
    GX_SetCullMode(GX_CULL_NONE);
    GX_SetDither(GX_ENABLE);
    set_view();

    GX_ClearVtxDesc();
    GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
    GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XY, GX_F32, 0);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);

    GX_SetNumChans(1);
    GX_SetChanCtrl(GX_COLOR0A0, GX_DISABLE, GX_SRC_REG, GX_SRC_VTX, GX_LIGHTNULL, GX_DF_NONE, GX_AF_NONE);
    GX_SetNumTexGens(0);
    GX_SetNumTevStages(1);
    GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORDNULL, GX_TEXMAP_NULL, GX_COLOR0A0);
    GX_SetTevOp(GX_TEVSTAGE0, GX_PASSCLR);
    GX_SetZMode(GX_FALSE, GX_ALWAYS, GX_FALSE);
    GX_SetAlphaUpdate(GX_FALSE);
    GX_SetColorUpdate(GX_TRUE);
    GX_InvVtxCache();
    GX_InvalidateTexAll();

    Mtx44 proj;
    guOrtho(proj, 0, VH, 0, VW, 0, 300);
    GX_LoadProjectionMtx(proj, GX_ORTHOGRAPHIC);
    Mtx view;
    guMtxIdentity(view);
    guMtxTransApply(view, view, 0.0f, 0.0f, -50.0f);
    GX_LoadPosMtxImm(view, GX_PNMTX0);
}

/* ------------------------------------------------------------------ */
/* post-processing (see gfx.h): the frame is copied into textures and   */
/* drawn back split, blurred, graded, grained and grimed                */
/* ------------------------------------------------------------------ */
static void *tex_c, *tex_b, *tex_y, *tex_n, *tex_d;
static GXTexObj to_c, to_b, to_y, to_n, to_d;

/* a linear 8-bit image into the GPU's tiled I8 layout (8 x 4 texel blocks) */
static void *make_i8(const uint8_t *src, int w, int h)
{
    uint8_t *dst = memalign(32, (size_t)(w * h));
    int k = 0;
    for (int by = 0; by < h; by += 4)
        for (int bx = 0; bx < w; bx += 8)
            for (int y = 0; y < 4; y++)
                for (int x = 0; x < 8; x++) dst[k++] = src[(by + y) * w + bx + x];
    DCFlushRange(dst, (u32)(w * h));
    return dst;
}

static void post_init(void)
{
    gfx_post_init();
    tex_c = memalign(32, GX_GetTexBufferSize(640, 480, GX_TF_RGB565, GX_FALSE, 0));
    tex_b = memalign(32, GX_GetTexBufferSize(320, 240, GX_TF_RGB565, GX_FALSE, 0));
    tex_y = memalign(32, GX_GetTexBufferSize(320, 240, GX_TF_I8, GX_FALSE, 0));
    tex_n = make_i8(gfx_noise_tex, POST_TEX, POST_TEX);
    tex_d = make_i8(gfx_dirt_tex, POST_TEX, POST_TEX);
    GX_InitTexObj(&to_n, tex_n, POST_TEX, POST_TEX, GX_TF_I8, GX_REPEAT, GX_REPEAT, GX_FALSE);
    GX_InitTexObjLOD(&to_n, GX_NEAR, GX_NEAR, 0, 0, 0, GX_FALSE, GX_FALSE, GX_ANISO_1);
    GX_InitTexObj(&to_d, tex_d, POST_TEX, POST_TEX, GX_TF_I8, GX_REPEAT, GX_REPEAT, GX_FALSE);
    GX_InitTexObjLOD(&to_d, GX_LINEAR, GX_LINEAR, 0, 0, 0, GX_FALSE, GX_FALSE, GX_ANISO_1);
    Mtx m;
    guMtxIdentity(m);
    GX_LoadTexMtxImm(m, GX_TEXMTX0, GX_MTX2x4);
    GX_SetVtxAttrFmt(GX_VTXFMT1, GX_VA_POS, GX_POS_XY, GX_F32, 0);
    GX_SetVtxAttrFmt(GX_VTXFMT1, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
    GX_SetVtxAttrFmt(GX_VTXFMT1, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
}

static void tq(float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1, u32 c)
{
    GX_Begin(GX_QUADS, GX_VTXFMT1, 4);
    GX_Position2f32(x0, y0); GX_Color1u32(c); GX_TexCoord2f32(u0, v0);
    GX_Position2f32(x1, y0); GX_Color1u32(c); GX_TexCoord2f32(u1, v0);
    GX_Position2f32(x1, y1); GX_Color1u32(c); GX_TexCoord2f32(u1, v1);
    GX_Position2f32(x0, y1); GX_Color1u32(c); GX_TexCoord2f32(u0, v1);
    GX_End();
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
    const GfxPost *P = &gfx_post_p;
    u32 w = rmode->fbWidth, h = rmode->efbHeight, top = 0;
    if (!wide) { u32 bh = (h * 3 / 4) & ~3u; top = ((h - bh) / 2) & ~1u; h = bh; }
    /* copies: the frame, a half-size (box filtered) copy, and its luminance */
    GX_SetTexCopySrc(0, top, w, h);
    GX_SetTexCopyDst(w, h, GX_TF_RGB565, GX_FALSE);
    GX_CopyTex(tex_c, GX_FALSE);
    GX_SetTexCopyDst(w / 2, h / 2, GX_TF_RGB565, GX_TRUE);
    GX_CopyTex(tex_b, GX_FALSE);
    GX_SetTexCopyDst(w / 2, h / 2, GX_TF_I8, GX_TRUE);
    GX_CopyTex(tex_y, GX_FALSE);
    GX_PixModeSync();
    GX_InvalidateTexAll();
    GX_InitTexObj(&to_c, tex_c, (u16)w, (u16)h, GX_TF_RGB565, GX_CLAMP, GX_CLAMP, GX_FALSE);
    GX_InitTexObjLOD(&to_c, GX_LINEAR, GX_LINEAR, 0, 0, 0, GX_FALSE, GX_FALSE, GX_ANISO_1);
    GX_InitTexObj(&to_b, tex_b, (u16)(w / 2), (u16)(h / 2), GX_TF_RGB565, GX_CLAMP, GX_CLAMP, GX_FALSE);
    GX_InitTexObjLOD(&to_b, GX_LINEAR, GX_LINEAR, 0, 0, 0, GX_FALSE, GX_FALSE, GX_ANISO_1);
    GX_InitTexObj(&to_y, tex_y, (u16)(w / 2), (u16)(h / 2), GX_TF_I8, GX_CLAMP, GX_CLAMP, GX_FALSE);
    GX_InitTexObjLOD(&to_y, GX_LINEAR, GX_LINEAR, 0, 0, 0, GX_FALSE, GX_FALSE, GX_ANISO_1);

    /* textured drawing: colour = texture * vertex colour, alpha = vertex alpha */
    GX_ClearVtxDesc();
    GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
    GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
    GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);
    GX_SetNumTexGens(1);
    GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_TEXMTX0);
    GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
    GX_SetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_TEXC, GX_CC_RASC, GX_CC_ZERO);
    GX_SetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_RASA);
    GX_SetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
    GX_SetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
    GX_InvVtxCache();

    /* 1. black, then the three channels added back, red and blue pulled apart */
    GX_LoadTexObj(&to_c, GX_TEXMAP0);
    GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
    tq(0, 0, VW, VH, 0, 0, 1, 1, 0x000000FFu);
    GX_SetBlendMode(GX_BM_BLEND, GX_BL_ONE, GX_BL_ONE, GX_LO_CLEAR);
    float su = P->split / VW;
    tq(0, 0, VW, VH, -su, 0, 1 - su, 1, 0xFF0000FFu);
    tq(0, 0, VW, VH, 0, 0, 1, 1, 0x00FF00FFu);
    tq(0, 0, VW, VH, su, 0, 1 + su, 1, 0x0000FFFFu);

    /* 2. blur: the half-size copy over the frame, more towards the edges (a mesh with per-vertex alpha) */
    GX_LoadTexObj(&to_b, GX_TEXMAP0);
    GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
    const int MX = 16, MY = 9;
    for (int pass = 0; pass < 2; pass++) {
        float off = pass ? -1.5f : 1.5f, ou = off / VW, ov = off / VH, ka = pass ? 0.5f : 0.75f;
        for (int j = 0; j < MY; j++) {
            GX_Begin(GX_TRIANGLESTRIP, GX_VTXFMT1, (u16)((MX + 1) * 2));
            for (int i = 0; i <= MX; i++) {
                for (int r = 0; r < 2; r++) {
                    float x = VW * (float)i / (float)MX, y = VH * (float)(j + r) / (float)MY;
                    float a = (P->blur + P->blur_edge * edge_k(x, y)) * ka;
                    if (a > 1) a = 1;
                    GX_Position2f32(x, y); GX_Color4u8(255, 255, 255, (u8)(a * 255.0f)); GX_TexCoord2f32(x / VW + ou, y / VH + ov);
                }
            }
            GX_End();
        }
    }

    /* 3. grade: tinted luminance over the top */
    GX_LoadTexObj(&to_y, GX_TEXMAP0);
    u32 tint = (P->tint & 0xFFFFFF00u) | (u32)(P->desat * 255.0f);
    tq(0, 0, VW, VH, 0, 0, 1, 1, tint);

    /* 4 + 5. grain and grime: dst * (src + srcalpha), src = tex * k, alpha = 1 - k/2 */
    GX_SetBlendMode(GX_BM_BLEND, GX_BL_DSTCLR, GX_BL_SRCALPHA, GX_LO_CLEAR);
    GX_LoadTexObj(&to_n, GX_TEXMAP0);
    u8 k = (u8)(P->grain * 255.0f);
    tq(0, 0, VW, VH, P->gu, P->gv, P->gu + VW * POST_NU, P->gv + VH * POST_NV, ((u32)k << 24) | ((u32)k << 16) | ((u32)k << 8) | (u32)(255 - k / 2));
    GX_LoadTexObj(&to_d, GX_TEXMAP0);
    k = (u8)(P->dirt * 255.0f);
    tq(0, 0, VW, VH, 0, 0, 1, 1, ((u32)k << 24) | ((u32)k << 16) | ((u32)k << 8) | (u32)(255 - k / 2));

    /* back to plain coloured triangles */
    GX_ClearVtxDesc();
    GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
    GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
    GX_SetNumTexGens(0);
    GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORDNULL, GX_TEXMAP_NULL, GX_COLOR0A0);
    GX_SetTevOp(GX_TEVSTAGE0, GX_PASSCLR);
    GX_InvVtxCache();
}

static void draw_list(void)
{
    for (int s = 0; s < gfx_nseg; s++) {
        const GSeg *seg = &gfx_seg[s];
        if (seg->mode == BL_POST) { post_apply(); continue; }
        if (seg->count < 3) continue;
        switch (seg->mode) {
        case BL_ADD: GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_ONE, GX_LO_CLEAR); break;
        case BL_MUL: GX_SetBlendMode(GX_BM_BLEND, GX_BL_DSTCLR, GX_BL_SRCCLR, GX_LO_CLEAR); break;   /* 2 * src * dst */
        default:     GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR); break;
        }
        int i = seg->start, end = seg->start + seg->count;
        while (i + 2 < end) {
            int n = end - i;
            if (n > 65532) n = 65532;
            n -= n % 3;
            GX_Begin(GX_TRIANGLES, GX_VTXFMT0, (u16)n);
            for (int k = 0; k < n; k++) {
                const GVtx *v = &gfx_v[i + k];
                GX_Position2f32(v->x, v->y);
                GX_Color1u32(v->c);
            }
            GX_End();
            i += n;
        }
    }
}

static int frame_pending;

static void show_previous(void)
{
    if (!frame_pending) { VIDEO_WaitVSync(); return; }
    GX_WaitDrawDone();
    VIDEO_SetNextFramebuffer(xfb[fbi ^ 1]);
    VIDEO_Flush();
    VIDEO_WaitVSync();
    frame_pending = 0;
}

static void present(void)
{
    GX_SetColorUpdate(GX_TRUE);
    GX_CopyDisp(xfb[fbi], GX_TRUE);
    GX_SetDrawDone();
    GX_Flush();
    frame_pending = 1;
    fbi ^= 1;
}

/* ------------------------------------------------------------------ */
/* input                                                               */
/* ------------------------------------------------------------------ */
static float stick_axis(int pos, int center, int lo, int hi)
{
    float d = (float)(pos - center);
    float range = d >= 0 ? (float)(hi - center) : (float)(center - lo);
    if (range < 4.0f) return 0.0f;
    float v = d / range;
    return v > 1.0f ? 1.0f : (v < -1.0f ? -1.0f : v);
}

static void read_stick(const joystick_t *js, float *ox, float *oy)
{
    float x = stick_axis(js->pos.x, js->center.x, js->min.x, js->max.x);
    float y = -stick_axis(js->pos.y, js->center.y, js->min.y, js->max.y);
    if (x == 0.0f && y == 0.0f && js->mag > 0.0f && js->mag <= 1.5f) {
        float a = js->ang * 3.14159265f / 180.0f;
        x = sinf(a) * js->mag; y = -cosf(a) * js->mag;
    }
    float m = sqrtf(x * x + y * y);
    if (m < 0.2f || !isfinite(m)) { x = y = 0.0f; }
    else { float k = (m > 1.0f ? 1.0f : m); k = (k - 0.2f) / 0.8f / m; x *= k; y *= k; }
    *ox = x; *oy = y;
}

static u32 read_input(Input *in)
{
    WPAD_ScanPads();
    u32 h = WPAD_ButtonsHeld(0), b = 0;
    if (h & WPAD_CLASSIC_BUTTON_UP)     b |= BTN_UP;
    if (h & WPAD_CLASSIC_BUTTON_DOWN)   b |= BTN_DOWN;
    if (h & WPAD_CLASSIC_BUTTON_LEFT)   b |= BTN_LEFT;
    if (h & WPAD_CLASSIC_BUTTON_RIGHT)  b |= BTN_RIGHT;
    if (h & WPAD_CLASSIC_BUTTON_A)      b |= BTN_A;
    if (h & WPAD_CLASSIC_BUTTON_B)      b |= BTN_B;
    if (h & WPAD_CLASSIC_BUTTON_X)      b |= BTN_X;
    if (h & WPAD_CLASSIC_BUTTON_Y)      b |= BTN_Y;
    if (h & WPAD_CLASSIC_BUTTON_ZL)     b |= BTN_ZL;
    if (h & WPAD_CLASSIC_BUTTON_ZR)     b |= BTN_ZR;
    if (h & WPAD_CLASSIC_BUTTON_FULL_L) b |= BTN_L;
    if (h & WPAD_CLASSIC_BUTTON_FULL_R) b |= BTN_R;
    if (h & WPAD_CLASSIC_BUTTON_PLUS)   b |= BTN_PLUS;
    if (h & WPAD_CLASSIC_BUTTON_MINUS)  b |= X_MINUS;
    if ((h & WPAD_CLASSIC_BUTTON_HOME) || (h & WPAD_BUTTON_HOME)) b |= X_HOME;

    in->sx = in->sy = in->rx = in->ry = 0.0f;
    WPADData *wd = WPAD_Data(0);
    if (wd && wd->exp.type == WPAD_EXP_CLASSIC) {
        read_stick(&wd->exp.classic.ljs, &in->sx, &in->sy);
        read_stick(&wd->exp.classic.rjs, &in->rx, &in->ry);
        /* the original Classic Controller's analog triggers: a half press already fires / dashes */
        if (wd->exp.classic.r_shoulder > 0.45f) b |= BTN_R;
        if (wd->exp.classic.l_shoulder > 0.45f) b |= BTN_L;
    }
    return b;
}

/* ------------------------------------------------------------------ */
/* audio                                                               */
/* ------------------------------------------------------------------ */
/* voice 0 carries the menu or the shop music (never both at once); voices 1-3 the three fight layers in sync */
#define FIRST_SFX_VOICE 4
static const int MUSV[4] = { MUS_MENU, MUS_RUN, MUS_CHAOS, MUS_BOSS };
static int v0_track = MUS_MENU;
static s32 mus_rate = MUS_RATE;

static u64 voice_start[16];
static int voice_vol[16];

static void play_sfx(int id, int pitch, float pan, float vol)
{
    s32 v = -1;
    u64 now = gettime();
    for (s32 k = FIRST_SFX_VOICE; k < 16; k++) if (ASND_StatusVoice(k) == SND_UNUSED) { v = k; break; }
    float base = (float)audio_volume(id) * (vol > 1.0f ? 1.0f : vol) * 0.7f;   /* master: headroom for the explosions */
    if (v < 0) {
        /* every voice busy: a loud effect (an explosion, the boss) takes over the quietest voice that has
         * been playing for a little while */
        if (base < 120.0f) return;
        int best = -1, bv = 1 << 30;
        for (s32 k = FIRST_SFX_VOICE; k < 16; k++) {
            if (diff_usec(voice_start[k], now) < 60000) continue;
            if (voice_vol[k] < bv) { bv = voice_vol[k]; best = k; }
        }
        if (best < 0 || (float)bv > base) return;
        ASND_StopVoice(best);
        v = best;
    }
    int vl = (int)(base * (pan > 0 ? 1.0f - pan * 0.7f : 1.0f));
    int vr = (int)(base * (pan < 0 ? 1.0f + pan * 0.7f : 1.0f));
    s32 rate = SFX_RATE * pitch / 128;
    if (rate > MAX_PITCH) rate = MAX_PITCH;
    if (rate < 1000) rate = 1000;
    ASND_SetVoice(v, VOICE_MONO_16BIT, rate, 0, (void *)audio_data(id), audio_len(id) * 2, vl, vr, NULL);
    voice_start[v] = now;
    voice_vol[v] = (int)base;
}

static void music_start(int pitch)
{
    ASND_Pause(1);                                   /* start every loop on the same sample */
    mus_rate = MUS_RATE * pitch / 128;
    for (int v = 0; v < 4; v++) {
        int t = v == 0 ? v0_track : MUSV[v];
        ASND_StopVoice(v);
        ASND_SetInfiniteVoice(v, VOICE_MONO_16BIT, mus_rate, 0, (void *)music_data(t), music_len(t) * 2, 0, 0);
    }
    ASND_Pause(0);
}

static void music_volumes(const float *mv)
{
    int other = v0_track == MUS_MENU ? MUS_SHOP : MUS_MENU;
    if (mv[v0_track] < 0.02f && mv[other] > mv[v0_track]) {     /* hand voice 0 over */
        v0_track = other;
        ASND_StopVoice(0);
        ASND_SetInfiniteVoice(0, VOICE_MONO_16BIT, mus_rate, 0, (void *)music_data(v0_track), music_len(v0_track) * 2, 0, 0);
    }
    for (int v = 0; v < 4; v++) {
        int t = v == 0 ? v0_track : MUSV[v];
        int vol = (int)(mv[t] * (float)music_volume(t) * 0.85f);
        ASND_ChangeVolumeVoice(v, vol, vol);
    }
}

/* ------------------------------------------------------------------ */
/* saving                                                              */
/* ------------------------------------------------------------------ */
static int sd_ok;

static void save_load(void)
{
    sd_ok = fatInitDefault();
    game_storage(game, sd_ok);
    if (!sd_ok) return;
    FILE *f = fopen(SAVE_PATH, "rb");
    if (!f) return;
    static char buf[SAVE_MAX];
    int n = (int)fread(buf, 1, sizeof buf, f);
    fclose(f);
    if (n > 0) game_load(game, buf, n);
}

static void save_write(void)
{
    static char buf[SAVE_MAX];
    int n = game_save_request(game, buf, sizeof buf);
    if (n <= 0 || !sd_ok) return;
    mkdir("sd:/apps", 0777);
    mkdir(SAVE_DIR, 0777);
    FILE *f = fopen(SAVE_PATH, "wb");
    int ok = 0;
    if (f) { ok = fwrite(buf, 1, (size_t)n, f) == (size_t)n; ok = (fclose(f) == 0) && ok; }
    game_save_result(game, ok);
}

/* one frame while the sound and music are being synthesized */
static void show_loading(void)
{
    gfx_begin();
    gfx_rect(0, 0, VW, VH, 0x0C0A0AFFu);
    gfx_text_c("LAST STAND", VW * 0.5f, VH * 0.5f - 40, 6.0f, 0xFF8A2AFFu);
    gfx_text_c("LOADING THE HORDE...", VW * 0.5f, VH * 0.5f + 20, 1.6f, 0x9AA4B4FFu);
    for (int k = 0; k < 2; k++) {          /* into both framebuffers */
        draw_list();
        present();
        show_previous();
    }
}

static void on_reset(u32 irq, void *ctx) { (void)irq; (void)ctx; quit = 1; }
static void on_power(void) { quit = 1; }

/* ------------------------------------------------------------------ */
int main(int argc, char **argv)
{
    (void)argc; (void)argv;

    video_init();
    WPAD_Init();
    WPAD_SetDataFormat(WPAD_CHAN_ALL, WPAD_FMT_BTNS_ACC);
    ASND_Init();
    ASND_Pause(0);
    SYS_SetResetCallback(on_reset);
    SYS_SetPowerCallback(on_power);

    post_init();
    show_loading();
    game = (Game *)memalign(32, GAME_SIZE);
    game_init(game);                                 /* also synthesizes the sound and the music */
    DCFlushRange((void *)audio_pool(), (u32)audio_pool_len() * 2);
    for (int t = 0; t < MUS_TRACKS; t++) DCFlushRange((void *)music_data(t), (u32)music_len(t) * 2);
    save_load();
    int mus_epoch = -1;

    const float dt = 1.0f / 60.0f;
    u32 prev = 0;
    u64 last = gettime();

    while (!quit) {
        Input in;
        u32 held = read_input(&in);
        u32 pressed = held & ~prev;
        prev = held;

        if (held & X_HOME) break;
        if (pressed & X_MINUS) { wide = !wide; set_view(); }

        u64 now = gettime();
        float elapsed = (float)diff_usec(last, now) * 1e-6f;
        last = now;
        int steps = (int)(elapsed / dt + 0.5f);
        if (steps < 1) steps = 1;
        if (steps > 3) steps = 3;
        for (int s = 0; s < steps; s++) {
            in.held = held & 0xFFFFu;
            in.pressed = s == 0 ? (pressed & 0xFFFFu) : 0;
            game_update(game, &in, dt);
        }

        int id, pitch;
        float pan, vol;
        while ((id = game_pop_sound(game, &pitch, &pan, &vol)) >= 0) play_sfx(id, pitch, pan, vol);

        float mv[MUS_TRACKS];
        int mpitch = 128, mep = 0;
        game_music(game, mv, &mpitch, &mep);
        if (mep != mus_epoch) { mus_epoch = mep; music_start(mpitch); }
        music_volumes(mv);

        save_write();

        game_render(game);
        show_previous();
        draw_list();
        present();
    }

    ASND_End();
    return 0;
}
