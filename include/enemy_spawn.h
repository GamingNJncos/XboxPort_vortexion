#ifndef ENEMY_SPAWN_H
#define ENEMY_SPAWN_H

#include "game_types.h"

/* Call once per frame to check if newly-visible tile columns contain spawn
 * markers and create enemy instances accordingly.
 *
 * Uses map_stage_1_enemies[][]; advances ctx->spawn_last_col as scroll moves.
 * Only call during STATE_STAGE / active play — not in titles/complete. */
void enemy_spawn_update(GameCtx *ctx);

/* Add a new enemy to ctx->enemies[] or ctx->bosses[].
 * type = enemy type enum; x,y = game-pixel spawn position. */
void enemy_spawn_add(GameCtx *ctx, EnemyType type, float x, float y);

#endif // ENEMY_SPAWN_H
