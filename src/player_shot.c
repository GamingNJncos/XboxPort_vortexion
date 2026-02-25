#include "player_shot.h"
#include "const.h"
#include "sprite.h"

/* Shot sprite dimensions in game pixels (matches Python SIZE = 14) */
#define SHOT_SIZE 14

/* Maximum simultaneous player bullets (matches Python MAX_SHOTS = 4) */
#define MAX_SHOTS 4

/* UV layout constants (match Python UV_FRAME_OFFSET=1, UV_OFFSET_Y=16) */
#define UV_FRAME_OFFSET 1
#define UV_OFFSET_Y     16

/* Speed table indexed by weapon level 0..5 (game pixels per frame) */
static const float speed_table[6] = {
    10.0f, 10.0f, 11.0f, 11.0f, 12.0f, 12.0f
};

/* Damage table [weapon_type 0..2][weapon_level 0..5] */
static const int damage_table[3][6] = {
    {1, 1, 1, 1, 1, 2},  /* weapon A — forward */
    {1, 1, 1, 2, 2, 3},  /* weapon B — spread  */
    {1, 1, 2, 2, 3, 3},  /* weapon C — back+fwd */
};

/* ------------------------------------------------------------------ */
/* Internal helper: fill one free Bullet slot.                        */
/* Returns 1 if a slot was found and filled, 0 if array is full.      */
/* ------------------------------------------------------------------ */
static int spawn_bullet(GameCtx *ctx,
                        float x, float y,
                        float velx, float vely,
                        int weapon_type, int weapon_level)
{
    int i;
    for (i = 0; i < MAX_BULLETS; i++) {
        if (!ctx->bullets[i].active) {
            Bullet *b = &ctx->bullets[i];
            b->pos.x      = x;
            b->pos.y      = y;
            b->vel.x      = velx;
            b->vel.y      = vely;
            b->owner      = 0;           /* 0 = player */
            b->active     = 1;
            b->damage     = damage_table[weapon_type][weapon_level];
            b->weapon_type  = weapon_type;
            b->weapon_level = weapon_level;
            return 1;
        }
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* player_shot_create                                                  */
/*                                                                     */
/* Count active player bullets. If already at MAX_SHOTS (4), return 0.*/
/* Otherwise spawn 2 bullets according to weapon_type.                 */
/* Returns 1 on success, 0 if cap was reached.                         */
/* ------------------------------------------------------------------ */
int player_shot_create(GameCtx *ctx, float px, float py,
                       int weapon_type, int weapon_level)
{
    int i;
    int active_count = 0;

    /* Count currently active player bullets */
    for (i = 0; i < MAX_BULLETS; i++) {
        if (ctx->bullets[i].active && ctx->bullets[i].owner == 0) {
            active_count++;
        }
    }

    if (active_count >= MAX_SHOTS) {
        return 0;
    }

    float speed = speed_table[weapon_level];

    if (weapon_type == WEAPON_A) {
        /* Forward: 2 parallel shots moving right */
        spawn_bullet(ctx, px + 12.0f, py - 10.0f,
                     speed, 0.0f, weapon_type, weapon_level);
        spawn_bullet(ctx, px + 12.0f, py +  4.0f,
                     speed, 0.0f, weapon_type, weapon_level);
    } else if (weapon_type == WEAPON_B) {
        /* Spread: 2 shots at ~26.5 degrees above and below horizontal */
        float spdx =  speed * 0.894f;
        float spdy =  speed * 0.447f;
        spawn_bullet(ctx, px + 12.0f, py - 10.0f,
                     spdx, -spdy, weapon_type, weapon_level);
        spawn_bullet(ctx, px + 12.0f, py +  4.0f,
                     spdx, +spdy, weapon_type, weapon_level);
    } else {
        /* WEAPON_C — back+fwd: one shot forward, one backward */
        spawn_bullet(ctx, px + 12.0f, py - 3.0f,
                      speed, 0.0f, weapon_type, weapon_level);
        spawn_bullet(ctx, px - 10.0f, py - 3.0f,
                     -speed, 0.0f, weapon_type, weapon_level);
    }

    return 1;
}

/* ------------------------------------------------------------------ */
/* player_shot_update                                                  */
/*                                                                     */
/* Advance every active player bullet by its velocity and deactivate  */
/* any that leave the playable area.                                   */
/* ------------------------------------------------------------------ */
void player_shot_update(GameCtx *ctx)
{
    int i;
    int count = 0;

    for (i = 0; i < MAX_BULLETS; i++) {
        Bullet *b = &ctx->bullets[i];
        if (!b->active || b->owner != 0) {
            continue;
        }

        b->pos.x += b->vel.x;
        b->pos.y += b->vel.y;

        /* Out-of-bounds check (game pixel space, matches Python bounds) */
        if (b->pos.x > GAME_W ||
            b->pos.x + SHOT_SIZE < 0.0f ||
            b->pos.y < PLAYABLE_Y_MIN ||
            b->pos.y + SHOT_SIZE >= PLAYABLE_Y_MAX) {
            b->active = 0;
            continue;
        }

        count++;
    }

    ctx->bullet_count = count;
}

/* ------------------------------------------------------------------ */
/* player_shot_draw                                                    */
/*                                                                     */
/* Blit every active player bullet into the framebuffer using its     */
/* weapon_type and weapon_level to determine the source UV rect.       */
/* ------------------------------------------------------------------ */
void player_shot_draw(const GameCtx *ctx, uint32_t *fb, const SpriteSheet *sheet)
{
    int i;

    for (i = 0; i < MAX_BULLETS; i++) {
        const Bullet *b = &ctx->bullets[i];
        if (!b->active || b->owner != 0) {
            continue;
        }

        /* UV in sprite sheet (game pixels) */
        int src_x = b->weapon_level * 16 + UV_FRAME_OFFSET;
        int src_y = UV_OFFSET_Y + b->weapon_type * 16 + UV_FRAME_OFFSET;

        /* Map game-pixel position to framebuffer position */
        int dst_x = OFFSET_X + (int)b->pos.x * SCALE;
        int dst_y = OFFSET_Y + (int)b->pos.y * SCALE;

        blit_sprite(fb, sheet, dst_x, dst_y,
                    src_x, src_y, SHOT_SIZE, SHOT_SIZE, SCALE);
    }
}
