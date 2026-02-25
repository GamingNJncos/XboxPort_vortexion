/*
 * enemy_types.c — Per-type enemy update functions for vortexion
 *
 * Called from enemy.c vtable AFTER enemy_base_update() runs
 * (lifetime++ and hit_frames-- are already applied before each call).
 *
 * Coordinate system: x increases right, y increases down. Game pixel space.
 * Angles: 0 = right (+x), M_PI_F/2 = down (+y), M_PI_F = left (-x).
 */

#include <math.h>
#include "enemy_types.h"
#include "enemy.h"
#include "const.h"
#include "game_types.h"

/* File-scope constant used by enemy_i_update (serpentine stage velocities) */
static const float serp_vel[6] = { -0.5f, 1.0f, -1.25f, 1.25f, -1.0f, 0.5f };

/* ─────────────────────────────────────────────────────────────────────────────
 * ENEMY A — simple leftward sweeper
 *   Movement : x -= 1 px/frame; no Y movement.
 *   Shooting : at lifetime==120, two leftward shots offset ±8 px in Y.
 *   Removal  : when sprite fully exits left edge.
 * ───────────────────────────────────────────────────────────────────────────*/
void enemy_a_update(EnemyBase *e, GameCtx *ctx)
{
    float saved_y;

    if (!e->active) return;

    e->x -= 1.0f;

    if (e->lifetime == 120) {
        saved_y = e->y;

        e->y = saved_y - 8.0f;
        enemy_shoot_at_angle(e, ctx, 1.5f, M_PI_F, 0);

        e->y = saved_y + 8.0f;
        enemy_shoot_at_angle(e, ctx, 1.5f, M_PI_F, 20);

        e->y = saved_y;
    }

    if (e->x + e->w < 0) e->active = 0;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * ENEMY B — sinusoidal leftward sweeper
 *   Movement : x -= 1.5 px/frame; y oscillates via cosf of lifetime.
 *   Shooting : at lifetime==20, one shot aimed at player, speed 1.5.
 *   Removal  : when sprite fully exits left edge.
 * ───────────────────────────────────────────────────────────────────────────*/
void enemy_b_update(EnemyBase *e, GameCtx *ctx)
{
    if (!e->active) return;

    e->x -= 1.5f;
    e->y += cosf((float)e->lifetime * 0.05f) * 1.5f;

    /* Clamp to playable area */
    if (e->y < PLAYABLE_Y_MIN) e->y = (float)PLAYABLE_Y_MIN;
    if (e->y > PLAYABLE_Y_MAX - e->h) e->y = (float)(PLAYABLE_Y_MAX - e->h);

    if (e->lifetime == 20) {
        enemy_shoot_at_player(e, ctx, 1.5f, 0);
    }

    if (e->x + e->w < 0) e->active = 0;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * ENEMY C — scroll-matched, shoots at player twice
 *   Movement : x -= SCROLL_X_SPEED (stays fixed relative to background).
 *   flip_y   : set on first frame — 0 if spawn y < 96, 1 otherwise.
 *   Shooting : aimed at player at lifetime==25 and lifetime==50, speed 1.5.
 *   Removal  : when sprite fully exits left edge.
 * ───────────────────────────────────────────────────────────────────────────*/
void enemy_c_update(EnemyBase *e, GameCtx *ctx)
{
    if (!e->active) return;

    if (e->lifetime == 1) {
        e->flip_y = (e->y < 96) ? 0 : 1;
    }

    e->x -= SCROLL_X_SPEED;

    if (e->lifetime == 25 || e->lifetime == 50) {
        enemy_shoot_at_player(e, ctx, 1.5f, 0);
    }

    if (e->x + e->w < 0) e->active = 0;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * ENEMY D — scroll-matched, dives toward player on Y proximity
 *   Movement : x -= SCROLL_X_SPEED always.
 *              When player is within 24 px on Y axis, begins a Y dive:
 *              vel_y accelerates toward player at 0.4 px/frame^2.
 *   State    : ms.f0 = vel_y, ms.i0 = diving flag (0/1).
 *   flip_y   : set on first frame.
 *   Removal  : exits playable Y bounds OR exits left edge.
 * ───────────────────────────────────────────────────────────────────────────*/
void enemy_d_update(EnemyBase *e, GameCtx *ctx)
{
    float dy;
    float accel;

    if (!e->active) return;

    if (e->lifetime == 1) {
        e->flip_y = (e->y < 96) ? 0 : 1;
    }

    e->x -= SCROLL_X_SPEED;

    dy = ctx->player.pos.y - e->y;
    if (!e->ms.i0 && fabsf(dy) < 24.0f) {
        e->ms.i0 = 1;
    }

    if (e->ms.i0) {
        accel = (dy > 0.0f) ? 0.4f : -0.4f;
        e->ms.f0 += accel;
        e->y += e->ms.f0;
    }

    if (e->y < PLAYABLE_Y_MIN || e->y > PLAYABLE_Y_MAX || e->x + e->w < 0) {
        e->active = 0;
    }
}

/* ─────────────────────────────────────────────────────────────────────────────
 * ENEMY E — enters from off left edge, moves rightward
 *   Movement : spawns at x = -GAME_W on first frame; x += 1 px/frame.
 *   Shooting : at lifetime==200, one shot fired leftward (M_PI_F), speed 1.5.
 *   Removal  : when sprite fully exits right edge.
 * ───────────────────────────────────────────────────────────────────────────*/
void enemy_e_update(EnemyBase *e, GameCtx *ctx)
{
    if (!e->active) return;

    if (e->lifetime == 1) {
        e->x = -(float)GAME_W;
    }

    e->x += 1.0f;

    if (e->lifetime == 200) {
        enemy_shoot_at_angle(e, ctx, 1.5f, M_PI_F, 0);
    }

    if (e->x > GAME_W + 16) e->active = 0;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * ENEMY F — scroll-matched, fires directional fan shots
 *   Movement : x -= SCROLL_X_SPEED.
 *   flip_y   : set on first frame.
 *   Shooting : at lifetime==100, 200, 300 — 3-bullet fan.
 *              Top half (y<96): fan at 90°, 110°, 130° (downward spread).
 *              Bottom half    : fan at 230°, 250°, 270° (upward spread).
 *   State    : ms.i0 = top_half flag (1 = top, 0 = bottom).
 *   Removal  : when sprite fully exits left edge.
 * ───────────────────────────────────────────────────────────────────────────*/
void enemy_f_update(EnemyBase *e, GameCtx *ctx)
{
    float base_deg;

    if (!e->active) return;

    if (e->lifetime == 1) {
        e->ms.i0 = (e->y < 96) ? 1 : 0;
        e->flip_y = (e->y < 96) ? 0 : 1;
    }

    e->x -= SCROLL_X_SPEED;

    if (e->lifetime == 100 || e->lifetime == 200 || e->lifetime == 300) {
        base_deg = e->ms.i0 ? 90.0f : 230.0f;
        enemy_shoot_at_angle(e, ctx, 1.5f, base_deg         * (M_PI_F / 180.0f), 0);
        enemy_shoot_at_angle(e, ctx, 1.5f, (base_deg + 20.0f) * (M_PI_F / 180.0f), 0);
        enemy_shoot_at_angle(e, ctx, 1.5f, (base_deg + 40.0f) * (M_PI_F / 180.0f), 0);
    }

    if (e->x + e->w < 0) e->active = 0;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * ENEMY G — scroll-matched, performs a U-turn
 *   Phase 1 (lifetime < 250): x -= SCROLL_X_SPEED + 0.5f (slightly faster).
 *   Phase 2 (lifetime >= 250): flip_x toggled once, then x += 1.5f (rightward)
 *                               plus Y drift away from centre.
 *   State    : ms.i0 = turned flag (0/1).
 *   Removal  : at lifetime >= 300.
 * ───────────────────────────────────────────────────────────────────────────*/
void enemy_g_update(EnemyBase *e, GameCtx *ctx)
{
    if (!e->active) return;

    if (!e->ms.i0) {
        e->x -= SCROLL_X_SPEED + 0.5f;

        if (e->lifetime == 250) {
            e->ms.i0 = 1;
            e->flip_x = !e->flip_x;
        }
    }

    if (e->ms.i0) {
        e->x += 1.5f;
        e->y += e->flip_y ? -1.5f : 1.5f;
    }

    if (e->lifetime >= 300) e->active = 0;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * ENEMY H — gravity bouncer (red, hp=4)
 *   Movement : x -= 1.5 px/frame.
 *              vel_y initialised to -5.0 (first frame); gravity += 0.2/frame.
 *              Floor at y==136: vel_y reset to -5.0 (hard bounce).
 *              Ceiling clamp at y <= PLAYABLE_Y_MIN: vel_y zeroed.
 *   State    : ms.f0 = vel_y.
 *   Removal  : when sprite fully exits left edge.
 * ───────────────────────────────────────────────────────────────────────────*/
void enemy_h_update(EnemyBase *e, GameCtx *ctx)
{
    if (!e->active) return;

    if (e->lifetime == 1) {
        e->ms.f0 = -5.0f;
    }

    e->x -= 1.5f;
    e->ms.f0 += 0.2f;
    e->y += e->ms.f0;

    if (e->y >= 136.0f) {
        e->y = 136.0f;
        e->ms.f0 = -5.0f;
    }
    if (e->y < (float)PLAYABLE_Y_MIN) {
        e->y = (float)PLAYABLE_Y_MIN;
        e->ms.f0 = 0.0f;
    }

    if (e->x + e->w < 0) e->active = 0;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * ENEMY I — serpentine (red, hp=3)
 *   Movement : x -= 1.5 px/frame.
 *              Y velocity cycles through 6 stages (45 frames each) taken from
 *              serp_vel[]: { -0.5, +1.0, -1.25, +1.25, -1.0, +0.5 }.
 *   State    : ms.i0 = current stage index (derived from lifetime / 45).
 *   Shooting : at lifetime==30, one shot aimed at player, speed 1.5.
 *   Removal  : when sprite fully exits left edge.
 * ───────────────────────────────────────────────────────────────────────────*/
void enemy_i_update(EnemyBase *e, GameCtx *ctx)
{
    int stage;

    if (!e->active) return;

    e->x -= 1.5f;

    stage = (e->lifetime / 45) % 6;
    e->ms.i0 = stage;
    e->y += serp_vel[stage];

    /* Clamp to playable area */
    if (e->y < PLAYABLE_Y_MIN) e->y = (float)PLAYABLE_Y_MIN;
    if (e->y > PLAYABLE_Y_MAX - e->h) e->y = (float)(PLAYABLE_Y_MAX - e->h);

    if (e->lifetime == 30) {
        enemy_shoot_at_player(e, ctx, 1.5f, 0);
    }

    if (e->x + e->w < 0) e->active = 0;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * ENEMY J — defense turret (purple, hp=40)
 *   Movement : x -= SCROLL_X_SPEED (scroll-matched).
 *   Shooting : every 120 frames, 5-bullet fan leftward:
 *              angles 210°, 195°, 180°, 165°, 150° with delays 0,10,20,30,40.
 *              Speed 1.5.
 *   Removal  : when sprite fully exits left edge.
 * ───────────────────────────────────────────────────────────────────────────*/
void enemy_j_update(EnemyBase *e, GameCtx *ctx)
{
    static const float fan_angles[5] = {
        210.0f, 195.0f, 180.0f, 165.0f, 150.0f
    };
    int j;

    if (!e->active) return;

    e->x -= SCROLL_X_SPEED;

    if (e->lifetime > 0 && e->lifetime % 120 == 0) {
        for (j = 0; j < 5; j++) {
            enemy_shoot_at_angle(e, ctx, 1.5f,
                fan_angles[j] * (M_PI_F / 180.0f),
                j * 10);
        }
    }

    if (e->x + e->w < 0) e->active = 0;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * ENEMY K — BOSS 1 (32×32, hp=200, score=5000)
 *   Movement : x -= SCROLL_X_SPEED; y bounces between 40 and 120 at vel_y=0.5.
 *   State    : ms.f0 = vel_y (init 0.5f on first frame).
 *   Shooting : every 60f (no regular enemies) or 200f (regular enemies alive).
 *              3 shots aimed at player with delays 0, 5, 10. Speed 2.5.
 *              Matches Python: shoot_at_player(2.5), shoot_at_player(2.5, 5),
 *              shoot_at_player(2.5, 10) — delay not angle spread.
 * ───────────────────────────────────────────────────────────────────────────*/
void enemy_k_update(EnemyBase *e, GameCtx *ctx)
{
    int shoot_interval;

    if (!e->active) return;

    /* Init vel_y on first frame */
    if (e->lifetime == 1) {
        e->ms.f0 = 0.5f;
    }

    /* Drift left with scroll; Y bounces 40–120 at 0.5 px/frame. */
    if (!ctx->scroll_locked) {
        e->x -= SCROLL_X_SPEED;
    }
    /* When scroll locked: boss stops at its natural position, matching Python behavior */
    e->y += e->ms.f0;
    if (e->y > 120.0f) { e->y = 120.0f; e->ms.f0 = -0.5f; }
    if (e->y <  40.0f) { e->y =  40.0f; e->ms.f0 =  0.5f; }

    if (e->x + e->w < 0.0f) { e->active = 0; return; }

    /* Triple player-tracking shots with delays 0, 5, 10 */
    shoot_interval = (ctx->enemy_count > 0) ? 200 : 60;
    if (e->lifetime > 0 && e->lifetime % shoot_interval == 0 && e->x < (float)GAME_W) {
        enemy_shoot_at_player(e, ctx, 2.5f,  0);
        enemy_shoot_at_player(e, ctx, 2.5f,  5);
        enemy_shoot_at_player(e, ctx, 2.5f, 10);
    }
}

/* ─────────────────────────────────────────────────────────────────────────────
 * ENEMY L — BOSS 2 (32×32, hp=100, score=5000) — "plant boss"
 *   Movement : x -= SCROLL_X_SPEED; no Y movement.
 *   Shooting : every 60f (no regular enemies) or 200f (regular enemies alive).
 *              2 shots aimed at player with delays 0, 5. Speed 1.5.
 *              Matches Python: shoot_at_player(1.5), shoot_at_player(1.5, 5).
 *   Stage 2 has multiple EnemyL bosses; stage clear only when ALL die.
 * ───────────────────────────────────────────────────────────────────────────*/
void enemy_l_update(EnemyBase *e, GameCtx *ctx)
{
    int shoot_interval;

    if (!e->active) return;

    /* Drift left with scroll; no Y movement. */
    if (!ctx->scroll_locked) {
        e->x -= SCROLL_X_SPEED;
    }
    /* When scroll locked: boss stops at its natural position, matching Python behavior */

    if (e->x + e->w < 0.0f) { e->active = 0; return; }

    /* Dual player-tracking shots with delays 0, 5 */
    shoot_interval = (ctx->enemy_count > 0) ? 200 : 60;
    if (e->lifetime > 0 && e->lifetime % shoot_interval == 0 && e->x < (float)GAME_W) {
        enemy_shoot_at_player(e, ctx, 1.5f, 0);
        enemy_shoot_at_player(e, ctx, 1.5f, 5);
    }
}

/* ─────────────────────────────────────────────────────────────────────────────
 * ENEMY M — BOSS 3 (32×32, hp=300, score=5000)
 *   Movement : x -= SCROLL_X_SPEED; NO Y movement (matches Python enemy_m.py).
 *   Shooting : every 60f (no regular enemies) or 200f (regular enemies alive).
 *              4 shots aimed at player with delays 0, 5, 25, 30. Speed 1.5.
 *              Matches Python: shoot_at_player(1.5), (1.5,5), (1.5,25), (1.5,30).
 * ───────────────────────────────────────────────────────────────────────────*/
void enemy_m_update(EnemyBase *e, GameCtx *ctx)
{
    int shoot_interval;

    if (!e->active) return;

    /* Drift left with scroll; no Y movement (Python enemy_m.py has none). */
    if (!ctx->scroll_locked) {
        e->x -= SCROLL_X_SPEED;
    }
    /* When scroll locked: boss stops at its natural position, matching Python behavior */

    if (e->x + e->w < 0.0f) { e->active = 0; return; }

    /* Four player-tracking shots with delays 0, 5, 25, 30 */
    shoot_interval = (ctx->enemy_count > 0) ? 200 : 60;
    if (e->lifetime > 0 && e->lifetime % shoot_interval == 0 && e->x < (float)GAME_W) {
        enemy_shoot_at_player(e, ctx, 1.5f,  0);
        enemy_shoot_at_player(e, ctx, 1.5f,  5);
        enemy_shoot_at_player(e, ctx, 1.5f, 25);
        enemy_shoot_at_player(e, ctx, 1.5f, 30);
    }
}

/* ─────────────────────────────────────────────────────────────────────────────
 * ENEMY N — fast fragile bouncer (grey, hp=1)
 *   Movement : x -= 4 px/frame (fast).
 *              Y drift set on first frame: +0.5 if spawn y>=96, -0.5 if y<96.
 *              Y clamped to playable area.
 *   State    : ms.f0 = drift_y.
 *   No shooting.
 *   Removal  : when sprite fully exits left edge.
 * ───────────────────────────────────────────────────────────────────────────*/
void enemy_n_update(EnemyBase *e, GameCtx *ctx)
{
    if (!e->active) return;

    if (e->lifetime == 1) {
        e->ms.f0 = (e->y >= 96.0f) ? 0.5f : -0.5f;
    }

    e->x -= 4.0f;
    e->y += e->ms.f0;

    if (e->y < (float)PLAYABLE_Y_MIN)
        e->y = (float)PLAYABLE_Y_MIN;
    if (e->y > (float)(PLAYABLE_Y_MAX - e->h))
        e->y = (float)(PLAYABLE_Y_MAX - e->h);

    if (e->x + e->w < 0) e->active = 0;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * ENEMY O — fast weak tracker (cyan, hp=1)
 *   Movement : x -= 2.5 px/frame.
 *   Shooting : every 120 frames, one shot aimed at player, delay=40, speed 1.5.
 *   Removal  : when sprite fully exits left edge.
 * ───────────────────────────────────────────────────────────────────────────*/
void enemy_o_update(EnemyBase *e, GameCtx *ctx)
{
    if (!e->active) return;

    e->x -= 2.5f;

    if (e->lifetime > 0 && e->lifetime % 120 == 0) {
        enemy_shoot_at_player(e, ctx, 1.5f, 40);
    }

    if (e->x + e->w < 0) e->active = 0;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * ENEMY P — medium-speed dual-shot (pink, hp=2)
 *   Movement : x -= 2.5 px/frame.
 *   Shooting : every 120 frames, two shots at 190° and 170° (slight spread),
 *              both with delay=25. Speed 1.5.
 *   Removal  : when sprite fully exits left edge.
 * ───────────────────────────────────────────────────────────────────────────*/
void enemy_p_update(EnemyBase *e, GameCtx *ctx)
{
    if (!e->active) return;

    e->x -= 2.5f;

    if (e->lifetime > 0 && e->lifetime % 120 == 0) {
        enemy_shoot_at_angle(e, ctx, 4.0f, 190.0f * (M_PI_F / 180.0f), 25);
        enemy_shoot_at_angle(e, ctx, 4.0f, 170.0f * (M_PI_F / 180.0f), 25);
    }

    if (e->x + e->w < 0) e->active = 0;
}
