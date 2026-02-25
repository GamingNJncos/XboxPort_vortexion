#ifndef VORTEXION_POWERUP_H
#define VORTEXION_POWERUP_H

#include <stdint.h>
#include "game_types.h"
#include "sprite.h"

/* Called from enemy_destroy() for every enemy (including bosses) that dies.
 * Spawns a powerup every POWERUP_CYCLE_GAP kills following the TYPE_CYCLE. */
void powerup_spawn_on_enemy_kill(GameCtx *ctx, float x, float y);

/* Per-frame update: move left, bob, cycle weapon type, remove OOB. */
void powerup_update_all(GameCtx *ctx);

/* Draw all active powerups into fb. */
void powerup_draw_all(GameCtx *ctx, uint32_t *fb, const SpriteSheet *sheet);

/* AABB test each powerup vs player; apply collected effect on overlap. */
void powerup_check_player_collision(GameCtx *ctx);

#endif /* VORTEXION_POWERUP_H */
