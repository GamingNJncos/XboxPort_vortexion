// vortexion — Xbox port (nxdk/C)
// Phase 5: Title screen, stage substates, game complete, multi-stage progression

#include <hal/video.h>
#include <hal/xbox.h>
#include <windows.h>   // GetTickCount, Sleep
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <SDL.h>

#include "const.h"
#include "game_types.h"
#include "input.h"
#include "sprite.h"
#include "tilemap.h"
#include "hud.h"
#include "player.h"
#include "player_shot.h"
#include "enemy.h"
#include "enemy_shot.h"
#include "enemy_spawn.h"
#include "powerup.h"
#include "explosion.h"
#include "audio.h"

// All stage + UI maps (static const arrays — included once, no linker conflict)
#include "../assets/maps/map_stage_1.h"
#include "../assets/maps/map_stage_2.h"
#include "../assets/maps/map_stage_3.h"
#include "../assets/maps/map_stage_4.h"
#include "../assets/maps/map_stage_5.h"
#include "../assets/maps/map_title.h"
#include "../assets/maps/map_complete.h"

/* Software back buffer — render here, memcpy to hardware fb each frame */
static uint32_t back_buf[FB_W * FB_H];

/* FPS counter overlay — toggle with Start + Y + B */
static int   g_fps_enabled  = 0;
static int   g_fps_cooldown = 0;
static int   g_fps_fcount   = 0;
static DWORD g_fps_last_ms  = 0;
static int   g_fps_value    = 0;

/* Global sprite sheet */
static SpriteSheet gfx_sheet;

/* Top-level game context */
static GameCtx g_ctx;

/* Boss music flag — set once when boss fight starts */
static int boss_music_started = 0;
static int g_last_stage = 1; /* stage saved on game over for continue */

/* Precomputed stretch LUT: sx_lut[ox] = source X in back_buf for output column ox */
static int sx_lut[FB_W];

/* ── stretch_to_fill ──────────────────────────────────────────────────────────
 * Nearest-neighbour 5/4 upscale: reads 512×384 game area from src,
 * writes full 640×480 framebuffer in dst. */
static void stretch_to_fill(const uint32_t *src, uint32_t *dst)
{
    int ox, oy;
    for (oy = 0; oy < FB_H; oy++) {
        int sy = OFFSET_Y + (oy * (GAME_H * SCALE)) / FB_H;
        const uint32_t *src_row = src + sy * FB_W;
        uint32_t       *dst_row = dst + oy * FB_W;
        for (ox = 0; ox < FB_W; ox++) {
            dst_row[ox] = src_row[sx_lut[ox]];
        }
    }
}

/* ── Map selection — sets ctx->cur_map_* from ctx->stage_num ─────────────── */
static void game_select_map(GameCtx *ctx)
{
    switch (ctx->stage_num) {
    case 2:
        ctx->cur_map_tiles   = &map_stage_2_tiles[0][0];
        ctx->cur_map_enemies = &map_stage_2_enemies[0][0];
        ctx->cur_map_cols    = MAP_STAGE_2_COLS;
        ctx->cur_map_rows    = MAP_STAGE_2_ROWS;
        break;
    case 3:
        ctx->cur_map_tiles   = &map_stage_3_tiles[0][0];
        ctx->cur_map_enemies = &map_stage_3_enemies[0][0];
        ctx->cur_map_cols    = MAP_STAGE_3_COLS;
        ctx->cur_map_rows    = MAP_STAGE_3_ROWS;
        break;
    case 4:
        ctx->cur_map_tiles   = &map_stage_4_tiles[0][0];
        ctx->cur_map_enemies = &map_stage_4_enemies[0][0];
        ctx->cur_map_cols    = MAP_STAGE_4_COLS;
        ctx->cur_map_rows    = MAP_STAGE_4_ROWS;
        break;
    case 5:
        ctx->cur_map_tiles   = &map_stage_5_tiles[0][0];
        ctx->cur_map_enemies = &map_stage_5_enemies[0][0];
        ctx->cur_map_cols    = MAP_STAGE_5_COLS;
        ctx->cur_map_rows    = MAP_STAGE_5_ROWS;
        break;
    default: /* stage 1 (also catch-all) */
        ctx->cur_map_tiles   = &map_stage_1_tiles[0][0];
        ctx->cur_map_enemies = &map_stage_1_enemies[0][0];
        ctx->cur_map_cols    = MAP_STAGE_1_COLS;
        ctx->cur_map_rows    = MAP_STAGE_1_ROWS;
        break;
    }
}

/* ── game_init_stage ──────────────────────────────────────────────────────────
 * Reset all stage-local state.  Preserves score, lives, weapon, weapon_levels,
 * hi_score, and stage_num across stage transitions. */
static void game_init_stage(GameCtx *ctx)
{
    music_stop();
    int stage = ctx->stage_num;
    int hi    = ctx->hi_score;
    int score = ctx->player.score;
    int lives = ctx->player.lives;
    WeaponType weapon = ctx->player.weapon;
    int wl[MAX_WEAPONS];
    int inv = ctx->invincible;
    int i;
    for (i = 0; i < MAX_WEAPONS; i++) wl[i] = ctx->player.weapon_levels[i];

    memset(ctx, 0, sizeof(GameCtx));

    ctx->stage_num  = stage;
    ctx->hi_score   = hi;
    ctx->player.score = score;
    ctx->player.lives = lives;
    ctx->player.weapon = weapon;
    ctx->invincible = inv;
    for (i = 0; i < MAX_WEAPONS; i++) ctx->player.weapon_levels[i] = wl[i];

    game_select_map(ctx);
    ctx->spawn_last_col = GAME_W / TILE_SIZE - 1;
    ctx->state    = STATE_STAGE;
    ctx->substage = SUBSTAGE_SPAWNED;
    boss_music_started = 0;

    /* Spawn player at (0,92), set inv_timer for spawn-blink */
    player_respawn(&ctx->player);
    /* player_respawn does not touch lives/score/weapon — preserved above */
}

/* ── game_init_new_game ────────────────────────────────────────────────────── */
static void game_init_new_game(GameCtx *ctx)
{
    int hi = ctx->hi_score;
    memset(ctx, 0, sizeof(GameCtx));
    ctx->hi_score        = hi;
    ctx->stage_num       = 1;
    ctx->player.lives    = STARTING_LIVES;
    /* score and weapon_levels start at 0 (already zeroed) */
    game_init_stage(ctx);
}

static void game_init_titles(GameCtx *ctx); /* forward declaration */

/* ── game_init_continue ────────────────────────────────────────────────────── */
static void game_init_continue(GameCtx *ctx)
{
    int hi = ctx->hi_score;
    memset(ctx, 0, sizeof(GameCtx));
    ctx->hi_score     = hi;
    ctx->stage_num    = g_last_stage;
    ctx->player.lives = STARTING_LIVES;
    game_init_stage(ctx);
}

/* ── game_init_titles ──────────────────────────────────────────────────────── */
static void game_init_titles(GameCtx *ctx)
{
    ctx->state          = STATE_TITLES;
    ctx->title_scroll_x = 0;
    ctx->menu_selection = 0;
    ctx->substage_timer = 0;
    ctx->invincible     = 0;   /* reset invincible on title screen */
    music_stop();
    music_play(MUSIC_TITLE, 1);
}

/* ── game_init_complete ────────────────────────────────────────────────────── */
static void game_init_complete(GameCtx *ctx)
{
    ctx->state              = STATE_GAME_COMPLETE;
    ctx->complete_scroll_x  = 0;
    music_play(MUSIC_GAME_COMPLETE, 1);
}

/* ── titles_update ─────────────────────────────────────────────────────────── */
static void titles_update(GameCtx *ctx)
{
    /* Input lock: ignore all menu input for first 60 frames to prevent
     * spurious button presses on startup/re-entry from triggering transition */
    if (ctx->substage_timer < 60) {
        ctx->substage_timer++;
        /* Still scroll the background during the lock period */
        ctx->title_scroll_x -= 8;
        if (ctx->title_scroll_x <= -GAME_W) ctx->title_scroll_x += GAME_W;
        return;
    }

    /* BG scrolls left: -8 game px/frame, wraps at -GAME_W */
    ctx->title_scroll_x -= 8;
    if (ctx->title_scroll_x <= -GAME_W) ctx->title_scroll_x += GAME_W;

    /* Up/Down: toggle selection */
    if (input_just_pressed(BTN_UP) || input_just_pressed(BTN_DOWN)) {
        ctx->menu_selection = (ctx->menu_selection == 0) ? 1 : 0;
    }

    /* A (SHOOT) or B (PAUSE): confirm */
    if (input_just_pressed(BTN_SHOOT) || input_just_pressed(BTN_PAUSE)) {
        if (ctx->menu_selection == 0) {
            game_init_new_game(ctx);
        } else {
            game_init_continue(ctx);
        }
    }
}

/* ── titles_draw ───────────────────────────────────────────────────────────── */
static void titles_draw(GameCtx *ctx, uint32_t *fb, SpriteSheet *sheet)
{
    char buf[16];
    /* fb_y for the playable-area origin (below top HUD strip) */
    int title_y = OFFSET_Y + PLAYABLE_Y_MIN * SCALE;  /* 80 */

    /* BG: drawn twice for seamless horizontal scroll wrap */
    tilemap_draw_full(fb, sheet,
                      &map_title_tiles[0][0], MAP_TITLE_COLS, MAP_TITLE_ROWS,
                      OFFSET_X + ctx->title_scroll_x * SCALE, title_y, SCALE);
    tilemap_draw_full(fb, sheet,
                      &map_title_tiles[0][0], MAP_TITLE_COLS, MAP_TITLE_ROWS,
                      OFFSET_X + (ctx->title_scroll_x + GAME_W) * SCALE, title_y, SCALE);

    /* FG: VORTEXION logo (fixed position, drawn on top of BG) */
    tilemap_draw_full(fb, sheet,
                      &map_title_fg_tiles[0][0], MAP_TITLE_COLS, MAP_TITLE_ROWS,
                      OFFSET_X, title_y, SCALE);

    /* HUD bands (black strips) */
    hud_draw_bars(fb);

    /* Top HUD text */
    snprintf(buf, sizeof(buf), "%06d", ctx->player.score);
    draw_text(fb, sheet, 16, 8, buf);
    draw_text(fb, sheet, 96, 0, "HI-SCORE");
    snprintf(buf, sizeof(buf), "%06d", ctx->hi_score);
    draw_text(fb, sheet, 104, 8, buf);

    /* Menu items */
    draw_text(fb, sheet, 96, 112, "GAME START");
    draw_text(fb, sheet, 96, 128, "CONTINUE");

    /* Cursor: 16×16 sprite from sheet (0,0), positioned to left of selected item
     * Python: px.blt(loc[0]-16, loc[1]-4, 0, 0, 0, 16, 16, 0) */
    {
        int cursor_game_y = (ctx->menu_selection == 0) ? 112 : 128;
        blit_sprite(fb, sheet,
                    OFFSET_X + (96 - 16) * SCALE,
                    OFFSET_Y + (cursor_game_y - 4) * SCALE,
                    0, 0, 16, 16, SCALE);
    }

    /* Version string at bottom of playable area */
    draw_text(fb, sheet, 8, 152, "V1.0");

    /* Rainbow port credit — centered, animated, near bottom of playable area.
     * "A .:VIBEBOX:. PORT" = 18 chars × 8 game-px = 144 px; center x=(256-144)/2=56 */

    /* Invincibility indicator — top-right corner */
    if (ctx->invincible) {
        draw_text(fb, sheet, 240, 0, "I");
    }
}

/* ── hud_draw_stage ────────────────────────────────────────────────────────── */
static void hud_draw_stage(GameCtx *ctx, uint32_t *fb, SpriteSheet *sheet)
{
    char buf[16];
    int i, j;
    int wtype = (int)ctx->player.weapon;  /* 0=A, 1=B, 2=C */
    static const char *weapon_names[3] = {"A", "B", "C"};

    hud_draw_bars(fb);

    /* ── Top HUD ── */

    /* 1UP + score */
    draw_text(fb, sheet, 24, 0, "1UP");
    snprintf(buf, sizeof(buf), "%06d", ctx->player.score);
    draw_text(fb, sheet, 16, 8, buf);

    /* Hi-score */
    draw_text(fb, sheet, 96, 0, "HI-SCORE");
    snprintf(buf, sizeof(buf), "%06d", ctx->hi_score);
    draw_text(fb, sheet, 104, 8, buf);

    /* ARM label + current weapon name + weapon sprite
     * Python: draw_text(176,0,"ARM"), draw_text(176,8,name), blt(184,8, 0, weapon*16,224, 16,8) */
    draw_text(fb, sheet, 176, 0, "ARM");
    draw_text(fb, sheet, 176, 8, weapon_names[wtype]);
    blit_sprite(fb, sheet,
                OFFSET_X + 184 * SCALE,
                OFFSET_Y + 8 * SCALE,
                wtype * 16, 224, 16, 8, SCALE);

    /* Lives ship icon + count
     * Python: blt(216,0, 0, 0,4, 16,8, 0), draw_text(224,8, lives) */
    blit_sprite(fb, sheet, OFFSET_X + 216 * SCALE, OFFSET_Y, 0, 4, 16, 8, SCALE);
    snprintf(buf, sizeof(buf), "%d", ctx->player.lives);
    draw_text(fb, sheet, 224, 8, buf);

    /* ── Bottom HUD ── */

    /* ARM + LVL labels
     * Python: draw_text(16,176,"ARM"), draw_text(16,184,"LVL") */
    draw_text(fb, sheet, 16, 176, "ARM");
    draw_text(fb, sheet, 16, 184, "LVL");

    /* Three weapon columns at x=56, 120, 184
     * Python draw_weapon_level(i, 56+(64*i), 176):
     *   draw_text(x+16, y,   weapon_name)
     *   blt(x+24, y,   0, i*16,224, 16,8)          — weapon sprite
     *   for j=0..weapon_levels[i]:  blt(x+j*8, y+8, 0, 32,232, 8,8)  — filled pip
     *   for j..MAX_WEAPON_LEVEL:    blt(x+j*8, y+8, 0, 40,232, 8,8)  — empty pip
     * MAX_WEAPON_LEVEL = 5 (6 pips total: 0..5) */
    for (i = 0; i < 3; i++) {
        int bx = 56 + 64 * i;   /* base x for this weapon column */
        int by = 176;            /* base y (bottom HUD) */
        int lvl = ctx->player.weapon_levels[i];

        /* weapon letter label */
        draw_text(fb, sheet, bx + 16, by, weapon_names[i]);

        /* weapon sprite (16×8 at sheet u=i*16, v=224) */
        blit_sprite(fb, sheet,
                    OFFSET_X + (bx + 24) * SCALE,
                    OFFSET_Y + by * SCALE,
                    i * 16, 224, 16, 8, SCALE);

        /* level pips row (each 8×8) */
        for (j = 0; j <= 5; j++) {
            int pip_u = (j <= lvl) ? 32 : 40;   /* 32=filled, 40=empty */
            blit_sprite(fb, sheet,
                        OFFSET_X + (bx + j * 8) * SCALE,
                        OFFSET_Y + (by + 8) * SCALE,
                        pip_u, 232, 8, 8, SCALE);
        }
    }

    /* Stage number in bottom HUD */
    snprintf(buf, sizeof(buf), "STAGE %d", ctx->stage_num);
    draw_text(fb, sheet, 8, 184, buf);
}

/* ── stage_update ──────────────────────────────────────────────────────────── */
static void stage_update(GameCtx *ctx)
{
    /* Always track hi-score */
    if (ctx->player.score > ctx->hi_score) ctx->hi_score = ctx->player.score;

    switch (ctx->substage) {

    case SUBSTAGE_SPAWNED:
        /* Player visible-but-blinking for 30 frames, then start PLAY */
        ctx->substage_timer++;
        if (ctx->substage_timer >= 30) {
            ctx->substage       = SUBSTAGE_PLAY;
            ctx->substage_timer = 0;
            /* Wire stage music based on stage number */
            if (ctx->stage_num % 2 == 0) {
                music_play(MUSIC_VORTEX, 1);  /* stages 2,4 = vortex */
            } else if (ctx->stage_num == 1) {
                music_play(MUSIC_STAGE_1, 1);
            } else if (ctx->stage_num == 3) {
                music_play(MUSIC_STAGE_3, 1);
            } else {
                music_play(MUSIC_STAGE_5, 1);  /* stage 5 */
            }
        }
        return;  /* freeze all game logic during spawn window */

    case SUBSTAGE_PAUSED:
        /* B/Pause resumes */
        if (input_just_pressed(BTN_PAUSE)) {
            ctx->substage       = SUBSTAGE_PLAY;
            ctx->substage_timer = 0;
            music_resume(); /* continue music from where it was paused */
        }
        return;  /* freeze everything */

    case SUBSTAGE_DEAD:
        /* Spawn player death explosions on first frame, then wait for them to finish */
        if (ctx->substage_timer == 0) {
            int i;
            for (i = 0; i < 12; i++)
                explosion_spawn(ctx->explosions, MAX_EXPLOSIONS,
                                ctx->player.pos.x + (rand() % 25) - 12,
                                ctx->player.pos.y - 4 + (rand() % 13) - 6,
                                i * 8);
        }
        ctx->substage_timer++;
        /* Wait for all 12 explosions to finish + 0.5s buffer.
         * Last explosion: delay=88 frames + 15 play frames = complete at frame 103.
         * Add 30 frames (0.5s at 60fps) → wait 135 frames total. */
        if (ctx->substage_timer >= 135) {
            if (ctx->player.lives > 0) {
                ctx->player.lives--;
                player_respawn(&ctx->player);
                /* Clear shots so respawn isn't immediately hit */
                memset(ctx->enemy_shots, 0, sizeof(ctx->enemy_shots));
                ctx->enemy_shot_count = 0;
                ctx->substage       = SUBSTAGE_SPAWNED;
                ctx->substage_timer = 0;
            } else {
                ctx->substage       = SUBSTAGE_GAME_OVER;
                ctx->substage_timer = 0;
                g_last_stage        = ctx->stage_num;
                music_stop();
                music_play(MUSIC_GAME_OVER, 0);
            }
        }
        return;

    case SUBSTAGE_GAME_OVER:
        ctx->substage_timer++;
        if (input_just_pressed(BTN_SHOOT) || input_just_pressed(BTN_PAUSE)) {
            game_init_titles(ctx);
        }
        return;

    case SUBSTAGE_CLEAR:
        if (ctx->substage_timer == 0) {
            music_play(MUSIC_STAGE_CLEAR, 0);
        }
        ctx->substage_timer++;
        if (ctx->substage_timer >= 180 && !music_is_playing()) {
            music_stop(); /* clean audio state before transition — prevents level-switch blip */
            if (ctx->stage_num < 5) {
                ctx->stage_num++;
                game_init_stage(ctx);
            } else {
                game_init_complete(ctx);
            }
        }
        /* Scroll and BG still advance during clear, but no entity logic */
        break;

    case SUBSTAGE_PLAY:
        /* B/Pause suspends */
        if (input_just_pressed(BTN_PAUSE)) {
            ctx->substage       = SUBSTAGE_PAUSED;
            ctx->substage_timer = 0;
            music_pause(); /* silence music while paused */
            return;
        }
        break;
    }

    /* ── Scroll (runs in PLAY and CLEAR) ──────────────────────────────────── */
    /* Python: background.scroll_x advances by 0.5/frame until map_end, then stops.
     * Vortex stages (2+4): trigger stage_clear at map_end, then let scroll wrap.
     * Boss stages (1,3,5): lock scroll at map_end; bosses stop (scroll_locked=1).
     *   Stage clear comes from boss death in enemy_destroy(). */
    {
        int map_width = ctx->cur_map_cols * TILE_SIZE;
        int map_end   = map_width - GAME_W;   /* = Python's map_width - VIEW_WIDTH */
        int is_vortex = (ctx->stage_num == 2 || ctx->stage_num == 4);

        if (!ctx->scroll_locked) {
            ctx->scroll_accum += SCROLL_SPEED_ACCUM;
            if (ctx->scroll_accum >= SCROLL_SPEED_THRESH) {
                ctx->scroll_x++;
                ctx->scroll_accum -= SCROLL_SPEED_THRESH;
            }

            /* Music fade-out before boss (linear 100%→~0% over 120px scroll).
             * Guard with !boss_music_started: once boss music is running, never touch
             * s_music_vol — music_play() already reset it to 1.0f internally. */
            if (!boss_music_started) {
                if (ctx->scroll_x >= 1664 && ctx->scroll_x < BOSS_MUSIC_X) {
                    music_set_volume((float)(BOSS_MUSIC_X - ctx->scroll_x) / 120.0f);
                }
                if (ctx->scroll_x >= BOSS_MUSIC_X) {
                    boss_music_started = 1;
                    music_play(MUSIC_BOSS, 1); /* resets s_music_vol to 1.0f internally */
                }
            }

            if (ctx->scroll_x >= map_end) {
                /* Python: scroll_x_speed = 0 on reaching map_end for ALL stages.
                 * Vortex: additionally call end_of_vortex_stage() → stage_clear.
                 * Boss:   stage_clear comes from boss death in enemy_destroy(). */
                ctx->scroll_x    = map_end;
                ctx->scroll_locked = 1;
                if (is_vortex && !ctx->stage_clear) {
                    ctx->stage_clear = 1;
                }
            }
        }
    }

    /* ── Vortex BG fast scroll (stages 2+4 only) ──────────────────────────── */
    if (ctx->stage_num == 2 || ctx->stage_num == 4) {
        ctx->vortex_bg_x -= 8;
        if (ctx->vortex_bg_x <= -GAME_W) {
            ctx->vortex_bg_x += GAME_W;
        }
    }

    /* ── Entity logic (PLAY only) ─────────────────────────────────────────── */
    if (ctx->substage == SUBSTAGE_PLAY) {
        int i;

        player_update(&ctx->player, ctx);
        player_shot_update(ctx);
        enemy_spawn_update(ctx);

        for (i = 0; i < MAX_ENEMIES; i++) {
            if (ctx->enemies[i].active) {
                enemy_check_player_shot_collisions(&ctx->enemies[i], ctx);
                if (ctx->enemies[i].active)
                    enemy_update(&ctx->enemies[i], ctx);
            }
        }
        for (i = 0; i < MAX_BOSSES; i++) {
            if (ctx->bosses[i].active) {
                enemy_check_player_shot_collisions(&ctx->bosses[i], ctx);
                if (ctx->bosses[i].active)
                    enemy_update(&ctx->bosses[i], ctx);
            }
        }

        powerup_update_all(ctx);
        powerup_check_player_collision(ctx);

        explosion_update_all(ctx->explosions, MAX_EXPLOSIONS);

        enemy_shot_update(ctx);
        enemy_shot_check_player_collision(ctx);

        /* Player-enemy body collision: kills player (if not invincible).
         * Non-boss enemies also die (explode). Bosses survive player contact.
         * AABB: player is 16×8, enemy is e->w × e->h (game-pixel coords). */
        if (ctx->player.active && ctx->player.inv_timer == 0 && !ctx->invincible) {
            /* Normal enemies — both player and enemy die */
            for (i = 0; i < MAX_ENEMIES && ctx->player.active; i++) {
                EnemyBase *e = &ctx->enemies[i];
                if (!e->active) continue;
                if (ctx->player.pos.x + 16.0f > e->x &&
                    ctx->player.pos.x         < e->x + (float)e->w &&
                    ctx->player.pos.y + 8.0f  > e->y &&
                    ctx->player.pos.y         < e->y + (float)e->h) {
                    enemy_destroy(e, ctx);   /* enemy explodes */
                    ctx->player.active = 0; /* player dies */
                }
            }
            /* Bosses — player dies but boss survives */
            if (ctx->player.active) {
                for (i = 0; i < MAX_BOSSES && ctx->player.active; i++) {
                    EnemyBase *b = &ctx->bosses[i];
                    if (!b->active) continue;
                    if (ctx->player.pos.x + 16.0f > b->x &&
                        ctx->player.pos.x         < b->x + (float)b->w &&
                        ctx->player.pos.y + 8.0f  > b->y &&
                        ctx->player.pos.y         < b->y + (float)b->h) {
                        ctx->player.active = 0;
                    }
                }
            }
        }

        /* Death detection: enemy_shot.c sets player.active=0 on lethal hit */
        if (!ctx->player.active) {
            ctx->substage       = SUBSTAGE_DEAD;
            ctx->substage_timer = 0;
        }
        /* Stage clear: enemy.c sets stage_clear=1 on boss death;
         * vortex end handled in scroll clamp above. */
        else if (ctx->stage_clear) {
            /* Clear all projectiles — no shots visible during stage-clear screen */
            memset(ctx->enemy_shots, 0, sizeof(ctx->enemy_shots));
            ctx->enemy_shot_count = 0;
            memset(ctx->bullets, 0, sizeof(ctx->bullets));
            ctx->bullet_count     = 0;
            ctx->substage         = SUBSTAGE_CLEAR;
            ctx->substage_timer   = 0;
        }
    }

    /* Update explosions during SUBSTAGE_CLEAR — boss death multi-explosion chains
     * must keep animating after the substage transitions. Python runs
     * sprites_update(explosions) regardless of state. */
    if (ctx->substage == SUBSTAGE_CLEAR) {
        explosion_update_all(ctx->explosions, MAX_EXPLOSIONS);
    }
}

/* ── stage_draw ─────────────────────────────────────────────────────────────── */
static void stage_draw(GameCtx *ctx, uint32_t *fb, SpriteSheet *sheet)
{
    int i;

    /* Background tilemap */
    if (ctx->stage_num == 2 || ctx->stage_num == 4) {
        /* Vortex stage: the visual BG is the first 32 columns (256 game px) of
         * the tilemap looping at 8 game px/frame — matching Python's:
         *   bltm(vortex_scroll_x, 16, 0, 0,0, 256,160) × 2 copies.
         * tilemap_draw_vortex_bg renders only 33 visible columns (vs 256+× 2 with
         * draw_full), wrapping at visual_cols=32 using the full row stride. */
        int vbg_cols = GAME_W / TILE_SIZE;    /* 32 = visual BG loop columns */
        int vs = -ctx->vortex_bg_x;          /* 0..255 (vortex_bg_x is 0 .. -255) */
        vs = vs % (vbg_cols * TILE_SIZE);
        if (vs < 0) vs += vbg_cols * TILE_SIZE;
        tilemap_draw_vortex_bg(fb, sheet,
                               ctx->cur_map_tiles,
                               ctx->cur_map_cols, ctx->cur_map_rows,
                               vbg_cols, vs,
                               OFFSET_X, OFFSET_Y + PLAYABLE_Y_MIN * SCALE, SCALE);
    } else {
        tilemap_draw_bg(fb, sheet,
                        ctx->cur_map_tiles,
                        ctx->cur_map_cols, ctx->cur_map_rows,
                        ctx->scroll_x,
                        OFFSET_X,
                        OFFSET_Y + PLAYABLE_Y_MIN * SCALE,
                        SCALE);
    }

    /* Player shots */
    player_shot_draw(ctx, fb, sheet);

    /* Normal enemies */
    for (i = 0; i < MAX_ENEMIES; i++) {
        if (ctx->enemies[i].active) enemy_draw(&ctx->enemies[i], fb, sheet);
    }
    /* Bosses */
    for (i = 0; i < MAX_BOSSES; i++) {
        if (ctx->bosses[i].active) enemy_draw(&ctx->bosses[i], fb, sheet);
    }

    /* Explosions */
    explosion_draw_all(fb, sheet, ctx->explosions, MAX_EXPLOSIONS);

    /* Powerups */
    powerup_draw_all(ctx, fb, sheet);

    /* Enemy shots */
    enemy_shot_draw(ctx, fb, sheet, ctx->frame_count);

    /* Player — hidden when dead or game over (no sprite during death sequence) */
    if (ctx->substage != SUBSTAGE_DEAD &&
        ctx->substage != SUBSTAGE_GAME_OVER) {
        player_draw(&ctx->player, fb, sheet, ctx->frame_count);
    }

    /* HUD overlay */
    hud_draw_stage(ctx, fb, sheet);

    /* Substate text overlays */
    if (ctx->substage == SUBSTAGE_PAUSED) {
        /* PAUSED: game_x=104, 6 chars x 8 x scale2 = 96 fb px, centered at fb_x=320 */
        draw_text(fb, sheet, 104, 88, "PAUSED");
        /* UI-2 subtitle: scale=1 (half PAUSED size), centered below PAUSED.
         * PAUSED bottom=fb_y 240. Gap to L1 = 16px (50% of 32). L1 fb_y=256.
         * Gap L1 top to L2 top = 5px (50% of 10). L2 fb_y=261.
         * Line 1: "A VIBEBOX PORT" = 14 chars x 8 = 112 fb px, centered at 320, fb_x=264..376
         * Line 2: "BY GNJ"         =  6 chars x 8 =  48 fb px, centered at 320, fb_x=296..344
         * "VIBEBOX" and "GNJ" animated rainbow; "A ", " PORT", "BY " plain white. */
        {
            int anim_f = ctx->frame_count / 4;
            /* Layout: VIBEBOX 7×10=70px, PORT BY 9×8=72px, G/J 10×10 + N 8×8 in 10px slot = 172px total.
             * Centered at fb_x=320: start = 320 - 172/2 = 234.
             * G/J advance=10px; N rendered at slot+1 (center 8px in 10px slot), advance=10px. */
            draw_text_fb_rainbow_sized(fb, sheet, 234, 256, "VIBEBOX", 10, 10, anim_f);
            draw_text_fb_colored(fb, sheet, 304, 256, " PORT BY ", 1, 0xFFFFFFFFu);
            draw_text_fb_colored_sized(fb, sheet, 376, 256, "G", 10, 10, 0xFF00FFFFu);
            draw_text_fb_colored(fb, sheet, 387, 256, "N", 1, 0xFF0060A0u);
            draw_text_fb_colored_sized(fb, sheet, 387, 256, "J", 10, 10, 0xFF00FFFFu);
        }
    } else if (ctx->substage == SUBSTAGE_GAME_OVER) {
        draw_text(fb, sheet, 96, 88, "GAME OVER");
    } else if (ctx->substage == SUBSTAGE_CLEAR && ctx->substage_timer > 60) {
        /* Python: stage 1→2: "ENTERING VORTEX"; stage 2→3: "LEAVING VORTEX" etc. */
        if (ctx->stage_num == 2 || ctx->stage_num == 4) {
            draw_text(fb, sheet, 80, 88, "LEAVING VORTEX");
        } else {
            draw_text(fb, sheet, 80, 88, "ENTERING VORTEX");
        }
    }

    /* Invincibility indicator — top-right corner */
    if (ctx->invincible) {
        draw_text(fb, sheet, 240, 0, "I");
    }
}

/* ── complete_update ──────────────────────────────────────────────────────── */
static void complete_update(GameCtx *ctx)
{
    ctx->complete_scroll_x -= 8;
    if (ctx->complete_scroll_x <= -GAME_W) ctx->complete_scroll_x += GAME_W;

    if (input_just_pressed(BTN_SHOOT) || input_just_pressed(BTN_PAUSE)) {
        game_init_titles(ctx);
    }
}

/* ── complete_draw ────────────────────────────────────────────────────────── */
static void complete_draw(GameCtx *ctx, uint32_t *fb, SpriteSheet *sheet)
{
    char buf[16];
    /* Complete map covers full 192px game height (24 rows × 8px) */
    int comp_y = OFFSET_Y;

    /* BG (scrolling) — two copies for seamless wrap */
    tilemap_draw_full(fb, sheet,
                      &map_complete_tiles[0][0], MAP_COMPLETE_COLS, MAP_COMPLETE_ROWS,
                      OFFSET_X + ctx->complete_scroll_x * SCALE, comp_y, SCALE);
    tilemap_draw_full(fb, sheet,
                      &map_complete_tiles[0][0], MAP_COMPLETE_COLS, MAP_COMPLETE_ROWS,
                      OFFSET_X + (ctx->complete_scroll_x + GAME_W) * SCALE, comp_y, SCALE);

    /* Fix 5: Game complete HUD (top score band) */
    hud_draw_bars(fb);

    /* 1UP + score */
    draw_text(fb, sheet, 24, 0, "1UP");
    snprintf(buf, sizeof(buf), "%06d", ctx->player.score);
    draw_text(fb, sheet, 16, 8, buf);

    /* Hi-score */
    draw_text(fb, sheet, 96, 0, "HI-SCORE");
    snprintf(buf, sizeof(buf), "%06d", ctx->hi_score);
    draw_text(fb, sheet, 104, 8, buf);

    draw_text(fb, sheet, 56, 72, "THANKS FOR PLAYING");
    draw_text(fb, sheet, 88, 96, "FINAL SCORE");
    snprintf(buf, sizeof(buf), "%d", ctx->player.score);
    draw_text(fb, sheet, 104, 112, buf);
}

/* ── Top-level dispatch ─────────────────────────────────────────────────────── */
static void game_update(GameCtx *ctx)
{
    ctx->frame_count++;

    /* FPS toggle cooldown tick (runs every frame) */
    if (g_fps_cooldown > 0) g_fps_cooldown--;

    /* FPS toggle: A+B+Start. Only toggleable from in-game pause menu.
     * Returns to swallow BTN_PAUSE so the pause menu doesn't resume. */
    if (g_fps_cooldown == 0 &&
        ctx->state == STATE_STAGE && ctx->substage == SUBSTAGE_PAUSED &&
        input_just_pressed(BTN_PAUSE) &&
        input_pressed(BTN_SHOOT) && input_pressed(BTN_B)) {
        g_fps_enabled = !g_fps_enabled;
        g_fps_cooldown = 30;
        return;
    }

    /* Invincible toggle: X+Y+Start. Only toggleable from in-game pause menu.
     * Stays enabled across levels and deaths. Resets on reaching title screen. */
    if (ctx->state == STATE_STAGE && ctx->substage == SUBSTAGE_PAUSED &&
        input_just_pressed(BTN_PAUSE) &&
        input_pressed(BTN_X) && input_pressed(BTN_Y)) {
        ctx->invincible = !ctx->invincible;
        return;
    }

    switch (ctx->state) {
    case STATE_TITLES:        titles_update(ctx);   break;
    case STATE_STAGE:         stage_update(ctx);    break;
    case STATE_GAME_COMPLETE: complete_update(ctx); break;
    }
}

static void game_draw(GameCtx *ctx, uint32_t *fb, SpriteSheet *sheet)
{
    memset(fb, 0, FB_W * FB_H * sizeof(uint32_t));
    switch (ctx->state) {
    case STATE_TITLES:        titles_draw(ctx, fb, sheet);    break;
    case STATE_STAGE:         stage_draw(ctx, fb, sheet);     break;
    case STATE_GAME_COMPLETE: complete_draw(ctx, fb, sheet);  break;
    }

    /* FPS overlay — bottom-right corner at scale=1 (half game text size).
     * "FPS:NNN" = 7 chars × 8 fb-px = 56px. Right-aligned to game area (fb_x=576).
     * 10 screen pixels above where the full-size text would sit. */
    if (g_fps_enabled) {
        char fps_buf[8];
        int fps_fb_x = OFFSET_X + GAME_W * SCALE - 7 * 8;   /* 576 - 56 = 520 */
        int fps_fb_y = OFFSET_Y + GAME_H * SCALE - 12;  /* bottom of game area, inside */
        snprintf(fps_buf, sizeof(fps_buf), "FPS:%3d", g_fps_value);
        draw_text_fb(fb, sheet, fps_fb_x, fps_fb_y, fps_buf, 1);
    }
}

/* ── main ─────────────────────────────────────────────────────────────────── */
void __cdecl main(void)
{
    int ox;

    // 1. Video
    XVideoSetMode(640, 480, 32, REFRESH_DEFAULT);

    // 2. SDL (gamepad only — XAudio direct for audio in Phase 6)
    SDL_Init(SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK);
    SDL_JoystickEventState(SDL_ENABLE);

    // 3. Load sprite sheet
    sheet_load(&gfx_sheet, "D:\\gfx.rgba");

    // 4. Stretch LUT
    for (ox = 0; ox < FB_W; ox++) {
        sx_lut[ox] = OFFSET_X + (ox * (GAME_W * SCALE)) / FB_W;
    }

    // 5. Input
    input_init();

    // 6. RNG
    srand(GetTickCount());

    // 7. Audio system
    audio_init();

    // 8. Start at title screen
    memset(&g_ctx, 0, sizeof(g_ctx));
    g_ctx.player.lives = STARTING_LIVES;
    game_init_titles(&g_ctx);

    // 9. Main loop
    {
        const DWORD FRAME_TARGET_MS = FRAME_MS;
        while (1) {
            DWORD now = GetTickCount();
            SDL_Event ev;
            while (SDL_PollEvent(&ev)) { /* discard */ }

            /* FPS measurement: sample every ~250 ms using GetTickCount() */
            g_fps_fcount++;
            if (g_fps_last_ms == 0) g_fps_last_ms = now;
            {
                DWORD fps_el = now - g_fps_last_ms;
                if (fps_el >= 250) {
                    g_fps_value   = (int)((g_fps_fcount * 1000u) / fps_el);
                    g_fps_fcount  = 0;
                    g_fps_last_ms = now;
                }
            }

            input_update();
            game_update(&g_ctx);
            audio_update();
            game_draw(&g_ctx, back_buf, &gfx_sheet);

            stretch_to_fill(back_buf, (uint32_t *)XVideoGetFB());
            XVideoFlushFB();

            {
                DWORD elapsed = GetTickCount() - now;
                if (elapsed < FRAME_TARGET_MS) {
                    Sleep(FRAME_TARGET_MS - elapsed);
                }
            }
        }
    }

    // Unreachable
    sheet_free(&gfx_sheet);
    input_shutdown();
    XReboot();
}
