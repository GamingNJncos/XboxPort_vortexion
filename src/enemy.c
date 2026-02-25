/*
 * enemy.c — Enemy system for Vortexion (nxdk/Xbox)
 *
 * Implements the type-init table, vtable, and all public enemy API functions.
 * All 16 enemy types share enemy_base_draw; per-type AI is in enemy_types.c.
 *
 * nxdk notes:
 *   - No printf/fprintf
 *   - Declarations at top of blocks (C89-compatible)
 *   - GetTickCount() for timing (SDL_GetTicks() always 0 on nxdk)
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "enemy.h"
#include "enemy_types.h"
#include "enemy_shot.h"
#include "powerup.h"
#include "explosion.h"
#include "const.h"
#include "sprite.h"
#include "audio.h"

/* ── Type-init table ────────────────────────────────────────────────────────
 * Row 10 of the 256×256 sprite sheet = v=80 in game pixels.
 * Normal enemies: w=16, h=16.  Bosses K/L/M: w=32, h=32.
 * u values follow the spec exactly (not simply type*16 for bosses).
 * ──────────────────────────────────────────────────────────────────────────*/
const EnemyTypeDef enemy_type_defs[ENEMY_TYPE_COUNT] = {
    /*  u    v    w    h    hp   score  is_boss  boss_col    q_col[TL, TR, BL, BR] */
    {   0,  80,  16,  16,   2,   100,    0,   0u, {0u,0u,0u,0u} },  /* ENEMY_A */
    {  16,  80,  16,  16,   2,   100,    0,   0u, {0u,0u,0u,0u} },  /* ENEMY_B */
    {  32,  80,  16,  16,   2,   100,    0,   0u, {0u,0u,0u,0u} },  /* ENEMY_C */
    {  48,  80,  16,  16,   2,   100,    0,   0u, {0u,0u,0u,0u} },  /* ENEMY_D */
    {  64,  80,  16,  16,   2,   100,    0,   0u, {0u,0u,0u,0u} },  /* ENEMY_E */
    {  80,  80,  16,  16,   2,   100,    0,   0u, {0u,0u,0u,0u} },  /* ENEMY_F */
    {  96,  80,  16,  16,   2,   100,    0,   0u, {0u,0u,0u,0u} },  /* ENEMY_G */
    { 112,  80,  16,  16,   4,   100,    0,   0u, {0u,0u,0u,0u} },  /* ENEMY_H */
    { 128,  80,  16,  16,   3,   100,    0,   0u, {0u,0u,0u,0u} },  /* ENEMY_I */
    { 144,  80,  16,  16,  40,   100,    0,   0u, {0u,0u,0u,0u} },  /* ENEMY_J */
    /* Boss K: colour=11 (yellow 0xFFE6CE80). TL=as-is, TR→pal6=red,
     * BL→pal9=light-red, BR→pal13=purple. Hit flash→white (in draw). */
    { 160,  80,  32,  32, 200,  5000,    1,
      0xFFE6CE80u, {0xFFE6CE80u, 0xFFD4524Du, 0xFFFF7978u, 0xFFC95BBAu} },  /* ENEMY_K */
    /* Boss L: colour=3 (light-green 0xFF5EDC78). TL=as-is, TR→pal2=green,
     * BL→pal12=dark-green, BR→pal5=light-blue. Hit flash→white (in draw). */
    { 176,  80,  32,  32, 100,  5000,    1,
      0xFF5EDC78u, {0xFF5EDC78u, 0xFF21C842u, 0xFF21B03Bu, 0xFF7D76FCu} },  /* ENEMY_L */
    /* Boss M: colour=9 (light-red 0xFFFF7978). TL=as-is, TR→pal6=red,
     * BL→pal8=bright-red, BR→pal13=purple. Hit flash→white (in draw). */
    { 192,  80,  32,  32, 300,  5000,    1,
      0xFFFF7978u, {0xFFFF7978u, 0xFFD4524Du, 0xFFFC5554u, 0xFFC95BBAu} },  /* ENEMY_M */
    { 208,  80,  16,  16,   1,   100,    0,   0u, {0u,0u,0u,0u} },  /* ENEMY_N */
    { 224,  80,  16,  16,   1,   100,    0,   0u, {0u,0u,0u,0u} },  /* ENEMY_O */
    { 240,  80,  16,  16,   2,   100,    0,   0u, {0u,0u,0u,0u} },  /* ENEMY_P */
};

/* ── Vtable ─────────────────────────────────────────────────────────────────
 * All 16 types share enemy_base_draw; per-type AI is dispatched via update.
 * ──────────────────────────────────────────────────────────────────────────*/
const EnemyVTable enemy_vtable[ENEMY_TYPE_COUNT] = {
    { enemy_a_update, enemy_base_draw },  /* ENEMY_A */
    { enemy_b_update, enemy_base_draw },  /* ENEMY_B */
    { enemy_c_update, enemy_base_draw },  /* ENEMY_C */
    { enemy_d_update, enemy_base_draw },  /* ENEMY_D */
    { enemy_e_update, enemy_base_draw },  /* ENEMY_E */
    { enemy_f_update, enemy_base_draw },  /* ENEMY_F */
    { enemy_g_update, enemy_base_draw },  /* ENEMY_G */
    { enemy_h_update, enemy_base_draw },  /* ENEMY_H */
    { enemy_i_update, enemy_base_draw },  /* ENEMY_I */
    { enemy_j_update, enemy_base_draw },  /* ENEMY_J */
    { enemy_k_update, enemy_base_draw },  /* ENEMY_K */
    { enemy_l_update, enemy_base_draw },  /* ENEMY_L */
    { enemy_m_update, enemy_base_draw },  /* ENEMY_M */
    { enemy_n_update, enemy_base_draw },  /* ENEMY_N */
    { enemy_o_update, enemy_base_draw },  /* ENEMY_O */
    { enemy_p_update, enemy_base_draw },  /* ENEMY_P */
};

/* ── enemy_init ─────────────────────────────────────────────────────────────
 * Zero the slot, copy fields from the type-init table, set position + active.
 * ──────────────────────────────────────────────────────────────────────────*/
void enemy_init(EnemyBase *e, EnemyType type, float x, float y)
{
    const EnemyTypeDef *def;

    memset(e, 0, sizeof(*e));

    def      = &enemy_type_defs[type];
    e->type  = type;
    e->x     = x;
    e->y     = y;
    e->u     = def->u;
    e->v     = def->v;
    e->w     = def->w;
    e->h     = def->h;
    e->hp    = def->hp;
    e->score = def->score;
    e->is_boss = def->is_boss;
    e->boss_col = def->boss_col;
    e->q_col[0] = def->q_col[0];
    e->q_col[1] = def->q_col[1];
    e->q_col[2] = def->q_col[2];
    e->q_col[3] = def->q_col[3];
    e->active = 1;
}

/* ── enemy_base_update ──────────────────────────────────────────────────────
 * Per-frame bookkeeping shared by all types: advance lifetime, decay hit timer.
 * ──────────────────────────────────────────────────────────────────────────*/
void enemy_base_update(EnemyBase *e)
{
    e->lifetime++;
    if (e->hit_frames > 0) {
        e->hit_frames--;
    }
}

/* ── enemy_update ───────────────────────────────────────────────────────────
 * Call base bookkeeping first, then dispatch to the per-type AI function.
 * ──────────────────────────────────────────────────────────────────────────*/
void enemy_update(EnemyBase *e, GameCtx *ctx)
{
    enemy_base_update(e);
    enemy_vtable[e->type].update(e, ctx);
}

/* ── enemy_base_draw ────────────────────────────────────────────────────────
 * Blit the enemy sprite, then overlay a solid white rectangle during the
 * hit-flash window.  Bounds-checked to stay inside the 640×480 framebuffer.
 * ──────────────────────────────────────────────────────────────────────────*/
void enemy_base_draw(const EnemyBase *e, uint32_t *fb, const SpriteSheet *sheet)
{
    int dst_x, dst_y;
    int fy, fx, py, px2;

    dst_x = OFFSET_X + (int)e->x * SCALE;
    dst_y = OFFSET_Y + (int)e->y * SCALE;

    if (e->is_boss) {
        /* Bosses: 4 mirrored 16×16 quadrants with per-quadrant palette remapping.
         * Matches Python draw_composite(): px.pal(colour, X) per quadrant.
         * Hit flash: all quadrants remap boss_col → peach (palette 15). */
        int qw = 16, qh = 16;
        uint32_t bc = e->boss_col;
        if (e->hit_frames > 0) {
            uint32_t hc = 0xFFFFCCAAu; /* palette 15 = peach */
            blit_sprite_flip_tinted(fb, sheet, dst_x,              dst_y,              e->u, e->v, qw, qh, SCALE, 0, 0, bc, hc);
            blit_sprite_flip_tinted(fb, sheet, dst_x + qw * SCALE, dst_y,              e->u, e->v, qw, qh, SCALE, 1, 0, bc, hc);
            blit_sprite_flip_tinted(fb, sheet, dst_x,              dst_y + qh * SCALE, e->u, e->v, qw, qh, SCALE, 0, 1, bc, hc);
            blit_sprite_flip_tinted(fb, sheet, dst_x + qw * SCALE, dst_y + qh * SCALE, e->u, e->v, qw, qh, SCALE, 1, 1, bc, hc);
        } else {
            /* TL: q_col[0]==0 means no remap (draw as natural color) */
            if (e->q_col[0] != 0u)
                blit_sprite_flip_tinted(fb, sheet, dst_x, dst_y, e->u, e->v, qw, qh, SCALE, 0, 0, bc, e->q_col[0]);
            else
                blit_sprite_flip(fb, sheet, dst_x, dst_y, e->u, e->v, qw, qh, SCALE, 0, 0);
            /* TR */
            if (e->q_col[1] != 0u)
                blit_sprite_flip_tinted(fb, sheet, dst_x + qw * SCALE, dst_y, e->u, e->v, qw, qh, SCALE, 1, 0, bc, e->q_col[1]);
            else
                blit_sprite_flip(fb, sheet, dst_x + qw * SCALE, dst_y, e->u, e->v, qw, qh, SCALE, 1, 0);
            /* BL */
            if (e->q_col[2] != 0u)
                blit_sprite_flip_tinted(fb, sheet, dst_x, dst_y + qh * SCALE, e->u, e->v, qw, qh, SCALE, 0, 1, bc, e->q_col[2]);
            else
                blit_sprite_flip(fb, sheet, dst_x, dst_y + qh * SCALE, e->u, e->v, qw, qh, SCALE, 0, 1);
            /* BR */
            if (e->q_col[3] != 0u)
                blit_sprite_flip_tinted(fb, sheet, dst_x + qw * SCALE, dst_y + qh * SCALE, e->u, e->v, qw, qh, SCALE, 1, 1, bc, e->q_col[3]);
            else
                blit_sprite_flip(fb, sheet, dst_x + qw * SCALE, dst_y + qh * SCALE, e->u, e->v, qw, qh, SCALE, 1, 1);
        }
    } else {
        blit_sprite(fb, sheet, dst_x, dst_y, e->u, e->v, e->w, e->h, SCALE);
        /* Non-boss hit flash: overwrite visible pixels with white */
        if (e->hit_frames > 0) {
            for (fy = 0; fy < e->h * SCALE; fy++) {
                int src_y = e->v + fy / SCALE;
                py = dst_y + fy;
                if (py < 0 || py >= 480) continue;
                for (fx = 0; fx < e->w * SCALE; fx++) {
                    int src_x = e->u + fx / SCALE;
                    uint32_t src_px = sheet->pixels[src_y * sheet->w + src_x];
                    if (src_px == 0xFFFF00FFu) continue;
                    if (((src_px >> 24) & 0xFF) == 0) continue;
                    px2 = dst_x + fx;
                    if (px2 < 0 || px2 >= 640) continue;
                    fb[py * 640 + px2] = 0xFFFFFFFFu;
                }
            }
        }
    }
}

/* ── enemy_draw ─────────────────────────────────────────────────────────────
 * Dispatch to the vtable draw function (always enemy_base_draw for all types).
 * ──────────────────────────────────────────────────────────────────────────*/
void enemy_draw(const EnemyBase *e, uint32_t *fb, const SpriteSheet *sheet)
{
    enemy_vtable[e->type].draw(e, fb, sheet);
}

/* ── enemy_hit ──────────────────────────────────────────────────────────────
 * Apply damage, clamp hp to 0, destroy if dead, else start hit-flash timer.
 * ──────────────────────────────────────────────────────────────────────────*/
void enemy_hit(EnemyBase *e, int dmg, GameCtx *ctx)
{
    e->hp -= dmg;
    if (e->hp < 0) {
        e->hp = 0;
    }
    if (e->hp == 0) {
        enemy_destroy(e, ctx);
    } else {
        e->hit_frames = ENEMY_HIT_FRAMES;
        sfx_play(SFX_BLIP); /* Python on_hit(): play_sound(BLIP) when hp > 0 */
    }
}

/* ── enemy_destroy ──────────────────────────────────────────────────────────
 * Mark enemy dead, credit score, update game-state flags.
 * Guard against double-destroy via active check.
 * Explosions and powerup spawns are Phase 7 — omitted here.
 * ──────────────────────────────────────────────────────────────────────────*/
void enemy_destroy(EnemyBase *e, GameCtx *ctx)
{
    int new_score;

    if (!e->active) {
        return;
    }

    e->active = 0;

    /* Credit score, clamp to MAX_SCORE */
    new_score = ctx->player.score + e->score;
    if (new_score > MAX_SCORE) {
        new_score = MAX_SCORE;
    }
    ctx->player.score = new_score;

    /* Boss death → stage clear only when ALL bosses dead (matching Python
     * game_state_stage.py: if len(self.bosses)==0: stage_clear_init()) */
    if (e->is_boss) {
        if (ctx->bosses_alive > 0) ctx->bosses_alive--;
        if (ctx->bosses_alive == 0) ctx->stage_clear = 1;
    } else {
        ctx->enemy_count--;
        /* Caller is responsible for compacting the enemy array if needed */
    }

    /* Spawn a powerup on the cycle (every POWERUP_CYCLE_GAP kills) */
    powerup_spawn_on_enemy_kill(ctx, e->x, e->y);

    /* Spawn explosions — replicate Python death sequence */
    if (e->type == ENEMY_K) {
        /* Boss K: 12 explosions, delay=i*5, scattered */
        int i;
        for (i = 0; i < 12; i++)
            explosion_spawn(ctx->explosions, MAX_EXPLOSIONS,
                            e->x + 8 + (rand() % 25) - 12,
                            e->y + 8 + (rand() % 13) - 6,
                            i * 5);
    } else if (e->type == ENEMY_L) {
        /* Boss L: 6 explosions, delay=i*5, scattered */
        int i;
        for (i = 0; i < 6; i++)
            explosion_spawn(ctx->explosions, MAX_EXPLOSIONS,
                            e->x + 8 + (rand() % 25) - 12,
                            e->y + 8 + (rand() % 13) - 6,
                            i * 5);
    } else if (e->type == ENEMY_M) {
        /* Boss M: 6 explosions, delay=i*5, scattered (same as L) */
        int i;
        for (i = 0; i < 6; i++)
            explosion_spawn(ctx->explosions, MAX_EXPLOSIONS,
                            e->x + 8 + (rand() % 25) - 12,
                            e->y + 8 + (rand() % 13) - 6,
                            i * 5);
    } else {
        /* Normal enemy: 1 explosion, no delay, centered */
        explosion_spawn(ctx->explosions, MAX_EXPLOSIONS, e->x, e->y, 0);
    }
}

/* ── enemy_shoot_at_angle ───────────────────────────────────────────────────
 * Fire one enemy shot from the enemy centre at a fixed angle (radians).
 * Finds a free slot in ctx->enemy_shots[].  No-op if all slots are occupied.
 * ──────────────────────────────────────────────────────────────────────────*/
void enemy_shoot_at_angle(EnemyBase *e, GameCtx *ctx,
                          float speed, float angle_rad, int delay)
{
    float cx, cy;
    int i;
    EnemyShot *s;

    cx = e->x + e->w / 2.0f;
    cy = e->y + e->h / 2.0f;

    for (i = 0; i < MAX_ENEMY_SHOTS; i++) {
        if (!ctx->enemy_shots[i].active) {
            s = &ctx->enemy_shots[i];
            s->x      = cx;
            s->y      = cy;
            s->vx     = cosf(angle_rad) * speed;
            s->vy     = sinf(angle_rad) * speed;
            s->delay  = delay;
            s->active = 1;
            ctx->enemy_shot_count++;
            break;
        }
    }
}

/* ── enemy_shoot_at_player ──────────────────────────────────────────────────
 * Compute the angle from the enemy centre to the player centre, then fire.
 * Player centre = (player.pos.x + 8, player.pos.y + 4) matching Python source.
 * ──────────────────────────────────────────────────────────────────────────*/
void enemy_shoot_at_player(EnemyBase *e, GameCtx *ctx, float speed, int delay)
{
    float target_x, target_y;
    float cx, cy;
    float angle;

    target_x = ctx->player.pos.x + 8.0f;
    target_y = ctx->player.pos.y + 4.0f;

    cx = e->x + e->w / 2.0f;
    cy = e->y + e->h / 2.0f;

    angle = atan2f(target_y - cy, target_x - cx);
    enemy_shoot_at_angle(e, ctx, speed, angle, delay);
}

/* ── enemy_check_player_shot_collisions ─────────────────────────────────────
 * Skip if enemy is still in its spawn-invincibility window.
 * AABB test this enemy against every active player bullet (owner == 0).
 * On hit: consume the bullet, call enemy_hit, return early if enemy died.
 * Bullet hitbox assumed 14×14 game pixels (matches Python b.pos + 14 check).
 * ──────────────────────────────────────────────────────────────────────────*/
void enemy_check_player_shot_collisions(EnemyBase *e, GameCtx *ctx)
{
    int i;
    Bullet *b;

    /* Spawn-invincibility: ignore collisions for the first N lifetime frames */
    if (e->lifetime < ENEMY_INVINCIBLE_START) {
        return;
    }

    for (i = 0; i < MAX_BULLETS; i++) {
        b = &ctx->bullets[i];

        /* Skip inactive bullets and enemy-owned bullets */
        if (!b->active || b->owner != 0) {
            continue;
        }

        /* AABB overlap: bullet [bx, bx+14) × [by, by+14) vs enemy rect */
        if (b->pos.x + 14 > e->x     && b->pos.x < e->x + e->w &&
            b->pos.y + 14 > e->y     && b->pos.y < e->y + e->h) {

            /* Consume the bullet */
            b->active = 0;
            ctx->bullet_count--;
            if (ctx->bullet_count < 0) {
                ctx->bullet_count = 0;
            }

            /* Apply damage; return immediately if the enemy was destroyed */
            enemy_hit(e, b->damage, ctx);
            if (!e->active) {
                return;
            }
        }
    }
}
