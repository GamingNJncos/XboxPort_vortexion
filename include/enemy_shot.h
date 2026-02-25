#ifndef ENEMY_SHOT_H
#define ENEMY_SHOT_H

#include <stdint.h>
#include "game_types.h"
#include "sprite.h"

/* Shot sprite info (from Python: SIZE=4, u=6, v=102) */
#define ENEMY_SHOT_SIZE  4
#define ENEMY_SHOT_U     6
#define ENEMY_SHOT_V     102

/* Advance all active enemy shots one frame; remove out-of-bounds */
void enemy_shot_update(GameCtx *ctx);

/* Draw all active enemy shots using sprite at (ENEMY_SHOT_U, ENEMY_SHOT_V).
 * Remaps palette-15 (peach) to palette-11 (green) or palette-6 (lightgray),
 * alternating every 10 frames — matching Python EnemyShot.draw(). */
void enemy_shot_draw(const GameCtx *ctx, uint32_t *fb, const SpriteSheet *sheet, int frame_count);

/* Check all active enemy shots against the player; kill player on hit */
void enemy_shot_check_player_collision(GameCtx *ctx);

#endif // ENEMY_SHOT_H
