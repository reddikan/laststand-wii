/*
 * LAST STAND - procedurally composed and synthesized music.
 *
 *   MUS_MENU   the title: a lonely whistle and a twangy guitar over a funeral drum, wind
 *   MUS_RUN    the fight, layer 1: driving tom groove, galloping bass, a minor-key guitar riff
 *   MUS_CHAOS  the fight, layer 2: double-kick, a distorted power-chord riff, choir and brass - fades in
 *              as the screen fills up and the explosions pile on
 *   MUS_BOSS   the boss: faster, heavier, organ and pounding drums
 *   MUS_SHOP   between waves: a slow saloon piano
 *
 * Every track has the same length and tempo grid and loops in sync, so the game just fades them in and out.
 * The platform plays them a little lower (slower, darker) in each deeper biome.
 */
#ifndef MUSIC_H
#define MUSIC_H

#include <stdint.h>

#define MUS_RATE   22050
enum { MUS_MENU, MUS_RUN, MUS_CHAOS, MUS_BOSS, MUS_SHOP, MUS_TRACKS };

void           music_init(void);            /* compose + render every track (call once) */
const int16_t *music_data(int track);
int            music_len(int track);        /* samples, a multiple of 16 */
int            music_volume(int track);     /* playback volume at full level, 0..255 */

#endif
