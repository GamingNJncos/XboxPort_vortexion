#ifndef VORTEXION_EXPLOSION_H
#define VORTEXION_EXPLOSION_H

#include "../include/game_types.h"
#include "../include/sprite.h"
#include <stdint.h>

#define EXPLOSION_FRAMES     3
#define EXPLOSION_FRAME_TICKS 5   /* ticks per frame */

void explosion_spawn(Explosion *pool, int max, float x, float y, int delay);
void explosion_update_all(Explosion *pool, int max);
void explosion_draw_all(uint32_t *fb, const SpriteSheet *sheet,
                        Explosion *pool, int max);
int  explosion_any_active(const Explosion *pool, int max);

#endif
