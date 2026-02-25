#ifndef PLAYER_SHOT_H
#define PLAYER_SHOT_H

#include <stdint.h>
#include "game_types.h"
#include "sprite.h"

/* Attempt to create player shot(s) based on weapon type and level.
 * Returns 1 if shot(s) created, 0 if at MAX_SHOTS cap. */
int player_shot_create(GameCtx *ctx, float px, float py,
                       int weapon_type, int weapon_level);

/* Advance all active player bullets one frame; remove out-of-bounds */
void player_shot_update(GameCtx *ctx);

/* Draw all active player bullets */
void player_shot_draw(const GameCtx *ctx, uint32_t *fb, const SpriteSheet *sheet);

#endif // PLAYER_SHOT_H
