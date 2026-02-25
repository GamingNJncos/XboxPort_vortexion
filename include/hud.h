#ifndef HUD_H
#define HUD_H

#include <stdint.h>

/* Phase 2: draw solid black HUD bands at top and bottom of game area.
 * Top band:    fb_y = 48..79   (OFFSET_Y=48, PLAYABLE_Y_MIN=16, SCALE=2 → 32px)
 * Bottom band: fb_y = 400..431 (OFFSET_Y + PLAYABLE_Y_MAX*SCALE = 48+352=400, height=32px)
 * Phase 3 will add score/lives text rendering on top of these bands. */
void hud_draw_bars(uint32_t *fb);

#endif /* HUD_H */
