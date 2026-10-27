/*
 * LAST STAND - procedurally synthesized sound effects and the announcer.
 *
 * No audio files: every effect is generated at start-up from oscillators, filtered noise and a small formant
 * speech synthesizer (the gravel-voiced announcer). Samples are 16-bit signed mono at SFX_RATE Hz in the
 * host's byte order (big-endian on the Wii), packed into one 32-byte aligned pool.
 */
#ifndef AUDIO_H
#define AUDIO_H

#include <stdint.h>

#define SFX_RATE 32000

enum {
    /* weapons */
    SFX_REVOLVER, SFX_SCATTER, SFX_GATLING, SFX_SPINUP, SFX_THROW, SFX_BOOM, SFX_BOOM_BIG, SFX_TESLA, SFX_RAIL,
    SFX_CASING,
    /* hits and deaths */
    SFX_HIT, SFX_SPLAT, SFX_BONES, SFX_SHIELD, SFX_IMP, SFX_BURN,
    /* the player */
    SFX_HURT, SFX_DASH, SFX_COIN, SFX_GEM, SFX_HEAL, SFX_POWERUP, SFX_CHEST, SFX_NUKE, SFX_LASTSTAND, SFX_SWAP,
    /* enemies and bosses */
    SFX_EORB, SFX_EBONE, SFX_HOP, SFX_ROAR, SFX_SIREN, SFX_SLAM, SFX_LASER, SFX_CHARGE, SFX_SUMMON, SFX_SCREECH,
    /* world */
    SFX_THUNDER, SFX_BARREL,
    /* interface */
    SFX_MOVE, SFX_SELECT, SFX_BUY, SFX_ERROR, SFX_PAUSE, SFX_HORN, SFX_WAVECLEAR, SFX_GAMEOVER, SFX_VICTORY, SFX_TICK,
    /* announcer */
    SFX_V_LASTSTAND, SFX_V_WAVE, SFX_V_BOSS, SFX_V_FRENZY, SFX_V_KABOOM, SFX_V_GAMEOVER, SFX_V_CLEARED, SFX_V_RAMPAGE,
    SFX_COUNT
};

void           audio_init(void);               /* synthesize every effect (call once) */
const int16_t *audio_data(int id);             /* 32-byte aligned sample buffer */
int            audio_len(int id);              /* length in samples (multiple of 16) */
int            audio_volume(int id);           /* suggested playback volume 0..255 */
const int16_t *audio_pool(void);               /* the whole pool (for cache flushing) */
int            audio_pool_len(void);           /* samples */

#endif
