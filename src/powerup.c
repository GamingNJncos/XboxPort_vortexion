/*
 * powerup.c — Powerup spawn, update, draw, and collision for Vortexion (nxdk/Xbox)
 *
 * Powerup cycle matches Python source (powerup.py):
 *   TYPE_CYCLE = [WEAPON, WEAPON, BOMB, WEAPON, LIFE, WEAPON, BOMB]
 *   Spawns one every POWERUP_CYCLE_GAP enemy kills.
 *
 * Sprite UVs (game pixels, v=0 for all):
 *   POWERUP_LIFE   → u=16
 *   POWERUP_WEAPON → u=32 (A), 48 (B), 64 (C)  — cycles every 60 frames
 *   POWERUP_BOMB   → u=80
 */

#include <math.h>
#include <string.h>

#include "powerup.h"
#include "enemy.h"
#include "const.h"
#include "audio.h"

/* Powerup type cycle from Python powerup.py */
static const PowerupType type_cycle[POWERUP_CYCLE_LEN] = {
    POWERUP_WEAPON, POWERUP_WEAPON, POWERUP_BOMB,
    POWERUP_WEAPON, POWERUP_LIFE,  POWERUP_WEAPON, POWERUP_BOMB
};

/* Sprite sheet U coordinate for weapon powerup (indexed by weapon_type 0-2) */
static const int weapon_u[3] = { 32, 48, 64 };

/* Pyxel default palette colors 2–15 as ARGB32, for px.pal(15, colour) cycling.
 * Powerup sprites are all palette index 15 (white in exported PNG).
 * cycle index 0 = Pyxel color 2, index 13 = Pyxel color 15. */
static const uint32_t pyxel_pal_cycle[14] = {
    0xFF7E2553u,  /* 2  dark purple */
    0xFF008751u,  /* 3  dark green  */
    0xFFAB5236u,  /* 4  brown       */
    0xFF5F574Fu,  /* 5  dark grey   */
    0xFFC2C3C7u,  /* 6  light grey  */
    0xFFFFF1E8u,  /* 7  cream       */
    0xFFFF004Du,  /* 8  red         */
    0xFFFFA300u,  /* 9  orange      */
    0xFFFFEC27u,  /* 10 yellow      */
    0xFF00E436u,  /* 11 green       */
    0xFF29ADFFu,  /* 12 light blue  */
    0xFF83769Cu,  /* 13 purple      */
    0xFFFF77A8u,  /* 14 pink        */
    0xFFFFCCAAu,  /* 15 peach       */
};

static int powerup_get_u(const Powerup *p)
{
    if (p->type == POWERUP_LIFE)   return 16;
    if (p->type == POWERUP_BOMB)   return 80;
    return weapon_u[p->weapon_type]; /* POWERUP_WEAPON */
}

/* ── powerup_spawn_on_enemy_kill ─────────────────────────────────────────────
 * Called from enemy_destroy() every time any enemy (normal or boss) dies.
 * Increments the kill counter; spawns a powerup every POWERUP_CYCLE_GAP kills.
 * ─────────────────────────────────────────────────────────────────────────── */
void powerup_spawn_on_enemy_kill(GameCtx *ctx, float x, float y)
{
    int i;
    Powerup *p;

    ctx->powerup_kill_cnt++;
    if (ctx->powerup_kill_cnt < POWERUP_CYCLE_GAP) {
        return;
    }
    ctx->powerup_kill_cnt = 0;

    /* Find a free slot */
    for (i = 0; i < MAX_POWERUPS; i++) {
        if (!ctx->powerups[i].active) break;
    }
    if (i == MAX_POWERUPS) return; /* no free slots */

    p = &ctx->powerups[i];
    memset(p, 0, sizeof(Powerup));
    p->pos.x      = x;
    p->pos.y      = y;
    p->type       = type_cycle[ctx->powerup_cycle_idx];
    p->weapon_type = 0;
    p->active     = 1;

    ctx->powerup_cycle_idx++;
    if (ctx->powerup_cycle_idx >= POWERUP_CYCLE_LEN) {
        ctx->powerup_cycle_idx = 0;
    }
}

/* ── powerup_update_all ──────────────────────────────────────────────────────
 * Per-frame: move left 0.5 game px/frame, gentle vertical bob, weapon cycling.
 * ─────────────────────────────────────────────────────────────────────────── */
void powerup_update_all(GameCtx *ctx)
{
    int i;
    for (i = 0; i < MAX_POWERUPS; i++) {
        Powerup *p = &ctx->powerups[i];
        if (!p->active) continue;

        /* Move left 0.5 game px/frame via sub-pixel accumulator */
        p->move_accum += POWERUP_SPEED_ACCUM;
        if (p->move_accum >= POWERUP_SPEED_THRESH) {
            p->pos.x -= 1.0f;
            p->move_accum -= POWERUP_SPEED_THRESH;
        }

        /* Gentle vertical bob: ±1 game pixel, ~40-frame period */
        p->bob_timer++;
        p->pos.y += sinf(p->bob_timer * 0.15f) * 0.4f;

        /* Remove when fully off the left edge */
        if (p->pos.x + POWERUP_W < 0) {
            p->active = 0;
            continue;
        }

        /* Color tint cycles every 5 frames (Pyxel colors 2→15) */
        p->tint_timer++;
        if (p->tint_timer >= 5) {
            p->tint_timer = 0;
            p->tint_idx++;
            if (p->tint_idx >= 14) p->tint_idx = 0;
        }

        /* Weapon type cycles every 60 frames */
        if (p->type == POWERUP_WEAPON) {
            p->weapon_timer++;
            if (p->weapon_timer >= 60) {
                p->weapon_timer = 0;
                p->weapon_type++;
                if (p->weapon_type >= MAX_WEAPONS) {
                    p->weapon_type = 0;
                }
            }
        }
    }
}

/* ── powerup_draw_all ────────────────────────────────────────────────────────
 * Blit each active powerup sprite into the framebuffer.
 * All powerup sprites are 16×16 game pixels at v=0 in the sprite sheet.
 * Palette index 15 (white = 0xFFFFFFFF in gfx.rgba) is replaced with the
 * current tint color from pyxel_pal_cycle[], matching Python px.pal(15,colour).
 * ─────────────────────────────────────────────────────────────────────────── */
void powerup_draw_all(GameCtx *ctx, uint32_t *fb, const SpriteSheet *sheet)
{
    int i, sy, sx, dy, dx;
    for (i = 0; i < MAX_POWERUPS; i++) {
        const Powerup *p = &ctx->powerups[i];
        if (!p->active) continue;

        uint32_t tint = pyxel_pal_cycle[p->tint_idx];
        int src_u    = powerup_get_u(p);
        int dst_x    = OFFSET_X + (int)p->pos.x * SCALE;
        int dst_y    = OFFSET_Y + (int)p->pos.y * SCALE;

        for (sy = 0; sy < POWERUP_H; sy++) {
            for (sx = 0; sx < POWERUP_W; sx++) {
                uint32_t pix = sheet->pixels[sy * sheet->w + (src_u + sx)];
                if (pix == 0xFFFF00FFu) continue; /* colkey — transparent */
                if (((pix >> 24) & 0xFF) == 0)    continue; /* alpha=0 */
                /* Replace palette index 15 (white) with cycling tint */
                if (pix == 0xFFFFFFFFu) pix = tint;
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

/* ── powerup_check_player_collision ─────────────────────────────────────────
 * AABB test each powerup vs the player ship (16×8 game pixels).
 * On collect: apply effect and deactivate the powerup.
 * ─────────────────────────────────────────────────────────────────────────── */
void powerup_check_player_collision(GameCtx *ctx)
{
    int i, j;
    Player *pl = &ctx->player;

    if (!pl->active) return;

    for (i = 0; i < MAX_POWERUPS; i++) {
        Powerup *p = &ctx->powerups[i];
        if (!p->active) continue;

        /* Player ship: pos.x, pos.y, 16×8.  Powerup: pos.x, pos.y, 16×16. */
        if (pl->pos.x + 16 > p->pos.x     && pl->pos.x < p->pos.x + POWERUP_W &&
            pl->pos.y + 8  > p->pos.y     && pl->pos.y < p->pos.y + POWERUP_H) {

            p->active = 0;

            switch (p->type) {
            case POWERUP_LIFE:
                if (pl->lives < MAX_LIVES) pl->lives++;
                sfx_play(SFX_LIFE_POWERUP);
                break;

            case POWERUP_WEAPON:
                /* Switch to this weapon and level it up */
                pl->weapon = (WeaponType)p->weapon_type;
                if (pl->weapon_levels[p->weapon_type] < MAX_WEAPON_LEVEL) {
                    pl->weapon_levels[p->weapon_type]++;
                }
                sfx_play(SFX_WEAPON_POWERUP);
                break;

            case POWERUP_BOMB:
                /* Clear all enemy shots; deal bomb damage to all enemies */
                memset(ctx->enemy_shots, 0, sizeof(ctx->enemy_shots));
                ctx->enemy_shot_count = 0;
                for (j = 0; j < MAX_ENEMIES; j++) {
                    if (ctx->enemies[j].active) {
                        enemy_hit(&ctx->enemies[j], BOMB_DAMAGE, ctx);
                    }
                }
                for (j = 0; j < MAX_BOSSES; j++) {
                    if (ctx->bosses[j].active) {
                        enemy_hit(&ctx->bosses[j], BOMB_DAMAGE, ctx);
                    }
                }
                sfx_play(SFX_BOMB_POWERUP);
                break;
            }
        }
    }
}
