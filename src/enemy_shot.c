/*
 * enemy_shot.c — Enemy projectile system for Vortexion (nxdk/Xbox)
 *
 * Manages up to MAX_ENEMY_SHOTS (256) simultaneous enemy-fired projectiles.
 * All positions are in game-pixel space (256×192); scaled to framebuffer on draw.
 *
 * nxdk notes:
 *   - No printf/fprintf
 *   - Declarations at top of blocks (C89-compatible)
 */

#include "enemy_shot.h"
#include "const.h"
#include "game_types.h"
#include "sprite.h"
#include "audio.h"

/* ── enemy_shot_update ──────────────────────────────────────────────────────
 * Advance every active shot one frame:
 *   - Decrement delay counter if still pending
 *   - Move by velocity once delay reaches zero
 *   - Deactivate shots that leave the playable game-pixel area
 * Recomputes ctx->enemy_shot_count from scratch at the end.
 * ──────────────────────────────────────────────────────────────────────────*/
void enemy_shot_update(GameCtx *ctx)
{
    int i;
    int count;
    EnemyShot *s;

    count = 0;

    for (i = 0; i < MAX_ENEMY_SHOTS; i++) {
        s = &ctx->enemy_shots[i];

        if (!s->active) {
            continue;
        }

        /* Pending: count down the delay before the shot starts moving */
        if (s->delay > 0) {
            s->delay--;
            /* Still counts as active while delayed */
            count++;
            continue;
        }

        /* Move */
        s->x += s->vx;
        s->y += s->vy;

        /* Out-of-bounds check in game-pixel space.
         * Horizontal: off either edge of the 256-wide game area.
         * Vertical: outside the playable strip [PLAYABLE_Y_MIN, PLAYABLE_Y_MAX). */
        if (s->x > GAME_W                       ||
            s->x + ENEMY_SHOT_SIZE < 0.0f        ||
            s->y < PLAYABLE_Y_MIN                ||
            s->y + ENEMY_SHOT_SIZE >= PLAYABLE_Y_MAX) {
            s->active = 0;
            continue;
        }

        count++;
    }

    ctx->enemy_shot_count = count;
}

/* ── enemy_shot_draw ────────────────────────────────────────────────────────
 * Render all active, non-delayed shots using the sprite at (ENEMY_SHOT_U,
 * ENEMY_SHOT_V) 4×4 game pixels — matching Python EnemyShot.draw():
 *   px.pal(15, self.colour); super().draw(); px.pal()
 * Palette 15 (white 0xFFFFFFFF) is remapped to current colour:
 *   colour=11 (green 0xFF00E436) or colour=6 (lightgray 0xFFC2C3C7),
 *   alternating every 10 frames.
 * Black pixels (Pyxel colkey-0) are skipped as transparent.
 * ──────────────────────────────────────────────────────────────────────────*/
void enemy_shot_draw(const GameCtx *ctx, uint32_t *fb, const SpriteSheet *sheet, int frame_count)
{
    uint32_t colour;
    int i, sy, sx, dy, dx;
    const EnemyShot *s;

    if (!sheet || !sheet->pixels) return;

    /* Alternate palette 11 (green) and palette 6 (lightgray) every 10 frames */
    colour = ((frame_count / 10) % 2) ? 0xFFC2C3C7u : 0xFF00E436u;

    for (i = 0; i < MAX_ENEMY_SHOTS; i++) {
        s = &ctx->enemy_shots[i];

        if (!s->active || s->delay > 0) continue;

        {
            int dst_x = OFFSET_X + (int)s->x * SCALE;
            int dst_y = OFFSET_Y + (int)s->y * SCALE;

            for (sy = 0; sy < ENEMY_SHOT_SIZE; sy++) {
                for (sx = 0; sx < ENEMY_SHOT_SIZE; sx++) {
                    uint32_t pix = sheet->pixels[
                        (ENEMY_SHOT_V + sy) * sheet->w + (ENEMY_SHOT_U + sx)];
                    if (pix == 0xFFFF00FFu) continue;          /* magenta colkey */
                    if (((pix >> 24) & 0xFF) == 0) continue;   /* alpha=0 */
                    if (pix == 0xFF000000u) continue;           /* black = Pyxel colkey-0 */
                    if (pix == 0xFFFFFFFFu) pix = colour;       /* remap pal15 → colour */
                    for (dy = 0; dy < SCALE; dy++) {
                        for (dx = 0; dx < SCALE; dx++) {
                            int fx = dst_x + sx * SCALE + dx;
                            int fy = dst_y + sy * SCALE + dy;
                            if (fx >= 0 && fx < 640 && fy >= 0 && fy < 480)
                                fb[fy * 640 + fx] = pix;
                        }
                    }
                }
            }
        }
    }
}

/* ── enemy_shot_check_player_collision ──────────────────────────────────────
 * AABB test every active, non-delayed shot against the player hitbox (16×8).
 * On hit: deactivate the shot, kill the player (if not invincible).
 * Full kill logic (lives, respawn) is Phase 5; for now, active=0 flags it.
 * ──────────────────────────────────────────────────────────────────────────*/
void enemy_shot_check_player_collision(GameCtx *ctx)
{
    int i;
    EnemyShot *s;

    /* No collision processing if the player is already dead */
    if (!ctx->player.active) {
        return;
    }

    for (i = 0; i < MAX_ENEMY_SHOTS; i++) {
        s = &ctx->enemy_shots[i];

        /* Skip inactive shots and shots still in their delay period */
        if (!s->active || s->delay > 0) {
            continue;
        }

        /* AABB: shot ENEMY_SHOT_SIZE×ENEMY_SHOT_SIZE vs player 16×8.
         * Python hitbox: player rect is (pos.x, pos.y, 16, 8). */
        if (s->x + ENEMY_SHOT_SIZE > ctx->player.pos.x         &&
            s->x                   < ctx->player.pos.x + 16.0f &&
            s->y + ENEMY_SHOT_SIZE > ctx->player.pos.y         &&
            s->y                   < ctx->player.pos.y + 8.0f) {

            /* Only damage player when not in the invincibility window */
            if (ctx->player.inv_timer == 0 && !ctx->invincible) {
                /* Consume the shot */
                s->active = 0;

                /* Signal death — substage machine in main.c detects active==0
                 * and transitions to SUBSTAGE_DEAD.  Lives are decremented there
                 * so the DEAD→SPAWNED or DEAD→GAME_OVER path is centralised. */
                ctx->player.active = 0;

                /* No further hit processing this frame */
                return;
            }
        }
    }
}
