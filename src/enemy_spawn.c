/*
 * enemy_spawn.c — Scroll-triggered enemy spawn system for Vortexion
 *
 * As the stage scrolls left, newly visible tile columns are checked against
 * the enemy spawn layer (map_stage_1_enemies[][]).  Each non-zero even
 * sheet_col value in that layer identifies the left tile of a 2-wide enemy
 * sprite; the value is mapped to an EnemyType via sheet_col_to_type[].
 *
 * Quirk: sheet_col 0 is ambiguous (also means "empty") so ENEMY_A (val=0)
 * is never spawned from the map layer — its entry in the table is -1.
 *
 * Porting note (nxdk): GetTickCount() / Sleep() for timing; no delta-time.
 * All array accesses are bounds-checked before use.
 */

#include "enemy_spawn.h"
#include "enemy.h"
#include "const.h"

/* ── sheet_col → EnemyType lookup ──────────────────────────────────────────
 *
 * Index = sheet_col value stored in map_stage_1_enemies[][] (0–31).
 * -1   = do not spawn (empty cell, right-tile of 2-wide sprite, or ENEMY_A
 *        ambiguity at index 0).
 * >=0  = cast directly to EnemyType.
 *
 * Only even indices carry left-tile markers; odd indices are right tiles.
 * val=0  → -1    (ambiguous: same encoding as "empty")
 * val=2  → ENEMY_B  (1)
 * val=4  → ENEMY_C  (2)
 * val=6  → ENEMY_D  (3)
 * val=8  → ENEMY_E  (4)
 * val=10 → ENEMY_F  (5)
 * val=12 → ENEMY_G  (6)
 * val=14 → ENEMY_H  (7)
 * val=16 → ENEMY_I  (8)
 * val=18 → ENEMY_J  (9)
 * val=20 → ENEMY_K  boss (10)
 * val=22 → ENEMY_L  boss (11)
 * val=24 → ENEMY_M  boss (12)
 * val=26 → ENEMY_N  (13)
 * val=28 → ENEMY_O  (14)
 * val=30 → ENEMY_P  (15)
 * All odd indices → -1 (right half of sprite, skip)
 */
static const int8_t sheet_col_to_type[32] = {
    ENEMY_A,      /* 0  — empty / ENEMY_A ambiguity: skip */
    -1,           /* 1  — right tile of ENEMY_A: skip */
    ENEMY_B,      /* 2  */
    -1,           /* 3  — right tile */
    ENEMY_C,      /* 4  */
    -1,           /* 5  — right tile */
    ENEMY_D,      /* 6  */
    -1,           /* 7  — right tile */
    ENEMY_E,      /* 8  */
    -1,           /* 9  — right tile */
    ENEMY_F,      /* 10 */
    -1,           /* 11 — right tile */
    ENEMY_G,      /* 12 */
    -1,           /* 13 — right tile */
    ENEMY_H,      /* 14 */
    -1,           /* 15 — right tile */
    ENEMY_I,      /* 16 */
    -1,           /* 17 — right tile */
    ENEMY_J,      /* 18 */
    -1,           /* 19 — right tile */
    ENEMY_K,      /* 20 — boss */
    -1,           /* 21 — right tile */
    ENEMY_L,      /* 22 — boss */
    -1,           /* 23 — right tile */
    ENEMY_M,      /* 24 — boss */
    -1,           /* 25 — right tile */
    ENEMY_N,      /* 26 */
    -1,           /* 27 — right tile */
    ENEMY_O,      /* 28 */
    -1,           /* 29 — right tile */
    ENEMY_P,      /* 30 */
    -1            /* 31 — right tile */
};

/* ── enemy_spawn_add ────────────────────────────────────────────────────────
 *
 * Allocate a free slot from ctx->bosses[] (if is_boss) or ctx->enemies[],
 * initialise it, and bump the relevant counter.
 * If all slots of the required pool are occupied the spawn is silently
 * dropped — this matches the Python behaviour where no exception is raised
 * when the entity list is full.
 */
void enemy_spawn_add(GameCtx *ctx, EnemyType type, float x, float y)
{
    int i;

    if (enemy_type_defs[type].is_boss) {
        for (i = 0; i < MAX_BOSSES; i++) {
            if (!ctx->bosses[i].active) {
                enemy_init(&ctx->bosses[i], type, x, y);
                ctx->boss_count++;
                ctx->bosses_alive++;
                return;
            }
        }
        /* Boss pool full — drop spawn */
    } else {
        for (i = 0; i < MAX_ENEMIES; i++) {
            if (!ctx->enemies[i].active) {
                enemy_init(&ctx->enemies[i], type, x, y);
                ctx->enemy_count++;
                return;
            }
        }
        /* Enemy pool full — drop spawn */
    }
}

/* ── enemy_spawn_update ─────────────────────────────────────────────────────
 *
 * Call once per frame during STATE_STAGE / active play.
 *
 * Computes the rightmost tile column currently visible:
 *   next_col = (scroll_x + GAME_W) / TILE_SIZE
 *
 * Iterates every column that has become visible since the last call
 * (spawn_last_col+1 .. next_col inclusive).  For each column, scans all
 * MAP_STAGE_1_ROWS rows looking for non-zero spawn markers.  When a valid
 * left-tile marker is found the enemy is created and the next row is skipped
 * (it holds the bottom half of the 2-tall sprite and must not trigger a
 * second spawn).
 *
 * Equivalent Python logic (check_next_enemy_spawn):
 *   col = (scroll_x + VIEW_WIDTH) // TILE_SIZE
 *   if col > last_col_checked: scan rows, create_enemy, row += 1
 */
void enemy_spawn_update(GameCtx *ctx)
{
    int col;
    int row;
    int next_col;
    uint8_t val;
    int8_t etype;
    float spawn_x;
    float spawn_y;

    next_col = (ctx->scroll_x + GAME_W) / TILE_SIZE;

    for (col = ctx->spawn_last_col + 1; col <= next_col; col++) {
        /* Clamp to valid map range */
        if (col < 0 || col >= ctx->cur_map_cols) {
            ctx->spawn_last_col = col;
            continue;
        }

        /* Advance the cursor unconditionally so we never re-scan this col */
        ctx->spawn_last_col = col;

        row = 0;
        while (row < ctx->cur_map_rows) {
            val = ctx->cur_map_enemies[row * ctx->cur_map_cols + col];

            if (val > 0) {
                /* Guard: val must be < 32 to index the lookup table safely */
                etype = sheet_col_to_type[(val < 32) ? val : 0];

                if (etype >= 0) {
                    /* spawn_x: game-pixel X at the right edge of the visible
                     * area — the column's left pixel minus how far we have
                     * scrolled, matching Python: col*8 - scroll_x */
                    spawn_x = (float)(col * TILE_SIZE - ctx->scroll_x);

                    /* spawn_y: game-pixel Y offset by HUD top strip,
                     * matching Python: PLAYABLE_Y_MIN + row * TILE_SIZE */
                    spawn_y = (float)(PLAYABLE_Y_MIN + row * TILE_SIZE);

                    enemy_spawn_add(ctx, (EnemyType)etype, spawn_x, spawn_y);

                    /* Skip the next row: it is the bottom half of the 2-tall
                     * sprite marker and must not be treated as a new spawn */
                    row++;
                }
            }

            row++;
        }
    }
}
