#ifndef PLAYER_H
#define PLAYER_H

#include <stdint.h>
#include "game_types.h"
#include "sprite.h"

/* Initialise player at default spawn position (resets lives/score/weapons) */
void player_init(Player *p);

/* Respawn player in-stage: reset position + inv_timer, preserve lives/score/weapons */
void player_respawn(Player *p);

/* Process one game frame: input → move, shoot, decrement timers */
void player_update(Player *p, GameCtx *ctx);

/* Render player sprite to fb (skips alternate frames during invincibility) */
void player_draw(const Player *p, uint32_t *fb, const SpriteSheet *sheet, int frame_count);

#endif // PLAYER_H
