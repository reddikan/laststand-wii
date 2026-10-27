/*
 * LAST STAND - a bullet-heaven roguelite survival shooter for the Wii.
 *
 * Survive ten waves of a cursed biome, spend the bounty between waves on six outrageous weapons and your
 * gunslinger, then put down the boss that crawls out at 10:00. Five biomes, each darker than the last.
 *
 * The game code is plain C99 and knows nothing about the console: it takes a BTN_* bitmask plus two analog
 * sticks, draws into the gfx triangle list, and emits sound events and music volumes that the platform
 * layer (main.c) plays. Progress is a small blob the platform layer saves to and loads from the SD card.
 */
#ifndef GAME_H
#define GAME_H

#include <stdint.h>
#include "audio.h"

/* ---- input ---- */
enum {
    BTN_UP    = 1 << 0,
    BTN_DOWN  = 1 << 1,
    BTN_LEFT  = 1 << 2,
    BTN_RIGHT = 1 << 3,
    BTN_A     = 1 << 4,     /* choose the weapon in slot A */
    BTN_B     = 1 << 5,     /* choose the weapon in slot B */
    BTN_X     = 1 << 6,     /* choose the weapon in slot X */
    BTN_Y     = 1 << 7,     /* choose the weapon in slot Y */
    BTN_ZL    = 1 << 8,     /* dash                        */
    BTN_ZR    = 1 << 9,     /* fire (R2)                   */
    BTN_PLUS  = 1 << 10,    /* pause                       */
    BTN_L     = 1 << 11,    /* dash                        */
    BTN_R     = 1 << 12     /* fire (R1)                   */
};

typedef struct {
    uint32_t held, pressed;
    float    sx, sy;        /* left stick, -1..1 (y down is +) */
    float    rx, ry;        /* right stick */
} Input;

typedef struct Game Game;

extern const int GAME_SIZE;

void game_init(Game *g);                         /* once: also synthesizes the sound and music */
void game_update(Game *g, const Input *in, float dt);
void game_render(const Game *g);                 /* fills the gfx triangle list */
/* pops the next sound event, or -1. pitch: 128 = normal. pan: -1 left .. 1 right. vol: 0..1 */
int  game_pop_sound(Game *g, int *pitch, float *pan, float *vol);
/* volume (0..1) wanted for each music track right now (music.h). The tracks loop in sync; whenever *epoch
 * changes the platform restarts them all from the top at playback pitch *pitch (128 = normal). */
void game_music(const Game *g, float vol[], int *pitch, int *epoch);

/* ---- saving ---- */
#define SAVE_MAX 256
void game_storage(Game *g, int ok);              /* could the platform open storage (shown on the title) */
void game_load(Game *g, const void *buf, int len);
int  game_save_request(Game *g, void *buf, int max);   /* blob to save now, or 0 */
void game_save_result(Game *g, int ok);

#endif
