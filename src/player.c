#include "player.h"
#include "player_shot.h"
#include "input.h"
#include "sprite.h"
#include "const.h"
#include "tilemap.h"

/* -------------------------------------------------------------------------
 * player_init
 *
 * Spawns the player at the Python-reference default position (x=0, y=92).
 * Invincibility timer is pre-loaded so the player blinks on first spawn.
 * hi_score lives in GameCtx, not Player — not initialised here.
 * ------------------------------------------------------------------------- */
void player_init(Player *p) {
    int i;
    p->pos.x = 0.0f;
    p->pos.y = 92.0f;
    p->lives = STARTING_LIVES;
    p->score = 0;
    p->weapon = WEAPON_A;
    for (i = 0; i < MAX_WEAPONS; i++) {
        p->weapon_levels[i] = 0;
    }
    p->shot_timer = 0;
    p->inv_timer  = INVINCIBILITY_FRAMES;
    p->active     = 1;
}

/* -------------------------------------------------------------------------
 * player_respawn
 *
 * Re-spawns the player mid-stage.  Resets position and grants an
 * invincibility window, but preserves lives, score, weapon, weapon_levels.
 * ------------------------------------------------------------------------- */
void player_respawn(Player *p) {
    p->pos.x = 0.0f;
    p->pos.y = 92.0f;
    p->shot_timer = 0;
    p->inv_timer  = INVINCIBILITY_FRAMES;
    p->active     = 1;
}

/* -------------------------------------------------------------------------
 * player_update
 *
 * Order matches the Python update():
 *   1. move()  — read directional input, apply speed, clamp bounds
 *   2. decrement inv_timer
 *   3. decrement shot_timer (or fire if 0 and shoot held)
 *
 * Diagonal speed: PLAYER_DIAG_SPEED (≈1.414) replaces PLAYER_MOVE_SPEED.
 * Axis clamping:
 *   x ∈ [0, GAME_W - 16]          = [0, 240]
 *   y ∈ [16, 168]   (Python: max(16, min(APP_HEIGHT-16-h, y))
 *                             = max(16, min(192-16-8, y))
 *                             = max(16, min(168, y)))
 * ------------------------------------------------------------------------- */
void player_update(Player *p, GameCtx *ctx) {
    if (!p->active) return;

    /* --- move --- */
    int move_x = 0;
    int move_y = 0;

    if (input_pressed(BTN_LEFT))       move_x = -1;
    else if (input_pressed(BTN_RIGHT)) move_x =  1;

    if (input_pressed(BTN_UP))         move_y = -1;
    else if (input_pressed(BTN_DOWN))  move_y =  1;

    float dx = 0.0f;
    float dy = 0.0f;

    if (move_x != 0 && move_y != 0) {
        /* Diagonal — apply reduced speed on both axes */
        dx = (float)move_x * PLAYER_DIAG_SPEED;
        dy = (float)move_y * PLAYER_DIAG_SPEED;
    } else if (move_x != 0) {
        dx = (float)move_x * PLAYER_MOVE_SPEED;
    } else if (move_y != 0) {
        dy = (float)move_y * PLAYER_MOVE_SPEED;
    }

    if (dx != 0.0f) {
        p->pos.x += dx;
        /* Clamp position to playable area */
        if (p->pos.x < 0.0f)                 p->pos.x = 0.0f;
        if (p->pos.x > (float)(GAME_W - 16)) p->pos.x = (float)(GAME_W - 16);
    }

    if (dy != 0.0f) {
        p->pos.y += dy;
        /* Clamp position to playable area */
        if (p->pos.y < 16.0f)                p->pos.y = 16.0f;
        if (p->pos.y > 168.0f)               p->pos.y = 168.0f;
    }

    /* --- invincibility countdown --- */
    if (p->inv_timer > 0) {
        p->inv_timer--;
    } else {
        /* Lethal tile collision (matches Python exactly):
         * Only check when NOT invincible. Check center point (x+8, y+4). */
        if (!ctx->invincible &&
            tilemap_is_solid(ctx->cur_map_tiles, ctx->cur_map_cols,
                             (int)p->pos.x + 8, (int)p->pos.y + 4)) {
            p->active = 0;
            return;
        }
    }

    /* --- shooting --- */
    if (p->shot_timer > 0) {
        p->shot_timer--;
    } else if (input_pressed(BTN_SHOOT) || ctx->invincible) {
        /* Fire: player_shot_create returns 1 if slot was available */
        if (player_shot_create(ctx,
                               p->pos.x,
                               p->pos.y,
                               (int)p->weapon,
                               p->weapon_levels[(int)p->weapon])) {
            p->shot_timer = SHOT_DELAY_FRAMES;
        }
    }
}

/* -------------------------------------------------------------------------
 * player_draw
 *
 * Blinks during invincibility by skipping draw on even frames.
 * Sprite source: src_x=0, src_y=4, size 16x8 game pixels.
 * Framebuffer coords = OFFSET_X/Y + game pos * SCALE.
 * ------------------------------------------------------------------------- */
void player_draw(const Player *p, uint32_t *fb, const SpriteSheet *sheet, int frame_count) {
    if (!p->active) return;

    /* Invincibility blink: skip draw on even inv_timer ticks.
     * Uses inv_timer (not frame_count) so blink freezes during SUBSTAGE_PAUSED
     * when inv_timer stops decrementing. */
    if (p->inv_timer > 0 && (p->inv_timer % 2) == 0) {
        return;
    }

    int dst_x = OFFSET_X + (int)p->pos.x * SCALE;
    int dst_y = OFFSET_Y + (int)p->pos.y * SCALE;

    blit_sprite(fb, sheet, dst_x, dst_y,
                /* src_x */ 0,
                /* src_y */ 4,
                /* w     */ 16,
                /* h     */ 8,
                /* scale */ SCALE);
}
