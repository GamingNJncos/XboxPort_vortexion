#include "tilemap.h"
#include "const.h"
#include "game_types.h"

int tilemap_is_solid(const uint16_t *tiles, int map_cols, int game_px, int game_py) {
    int tx, ty;
    uint16_t tile_id;

    if (!tiles) return 0;

    /* Match Python exactly: collision ignores scroll_x and uses screen coords */
    tx = game_px / TILE_SIZE;
    ty = (game_py - PLAYABLE_Y_MIN) / TILE_SIZE;

    if (ty < 0 || ty >= MAP_H_TILES) return 0;

    /* Wrap tx only if it somehow exceeds map_cols (usually 32 for collision window) */
    tx = tx % map_cols;
    if (tx < 0) tx += map_cols;

    tile_id = tiles[ty * map_cols + tx];
    return (tile_id >= SOLID_TILE_START_ID);
}

void tilemap_draw_bg(
    uint32_t *fb,
    const SpriteSheet *sheet,
    const uint16_t *tiles_flat,   /* pointer to tiles[0][0] — flat row-major array */
    int map_cols,                  /* columns in map (32, 128, or 256) */
    int map_rows,                  /* rows in map (20 or 24) */
    int scroll_x,                  /* scroll position in game pixels (integer, already advanced by accum) */
    int fb_origin_x,               /* framebuffer x where column 0 of viewport starts (= OFFSET_X = 64) */
    int fb_origin_y,               /* framebuffer y where row 0 of tilemap starts (= OFFSET_Y + 32 = 80) */
    int scale                      /* pixel scale factor (= 2) */
) {
    int first_col, x_offset, cols_to_draw, r, i;

    /* 1. Compute first visible tile column */
    first_col = scroll_x / TILE_SIZE;
    /* 2. Compute x pixel offset into that tile (in game pixels) */
    x_offset = scroll_x % TILE_SIZE;
    /* 3. Number of columns to draw (extra 1 for partial left tile) */
    cols_to_draw = (256 / TILE_SIZE) + 1;

    /* 4. Draw rows */
    for (r = 0; r < map_rows; r++) {
        int fb_y = fb_origin_y + r * TILE_SIZE * scale;
        if (fb_y >= 480) break; /* Out of screen */

        for (i = 0; i < cols_to_draw; i++) {
            int col = first_col + i;
            uint16_t tile_id;
            int src_x, src_y, fb_x;

            col = col % map_cols;   /* wrap: seamless horizontal loop */

            tile_id = tiles_flat[r * map_cols + col];
            if (tile_id == 0) continue; /* Empty tile */

            src_x = (tile_id % 32) * TILE_SIZE;
            src_y = (tile_id / 32) * TILE_SIZE;
            fb_x = fb_origin_x + (i * TILE_SIZE - x_offset) * scale;

            blit_sprite(fb, sheet, fb_x, fb_y, src_x, src_y, TILE_SIZE, TILE_SIZE, scale);
        }
    }
}

/* Vortex-stage BG: like tilemap_draw_bg but wraps at visual_cols (32) while
 * using the full map_cols row stride for array indexing.
 * Matches Python: bltm(vortex_scroll_x, 16, tm, 0, 0, VIEW_W, VIEW_H) × 2.
 * Renders only 33 visible columns instead of all map columns — ~15× faster. */
void tilemap_draw_vortex_bg(
    uint32_t *fb, const SpriteSheet *sheet,
    const uint16_t *tiles_flat, int map_cols, int map_rows,
    int visual_cols, int scroll_x,
    int fb_origin_x, int fb_origin_y, int scale)
{
    int first_col    = scroll_x / TILE_SIZE;
    int x_offset     = scroll_x % TILE_SIZE;
    int cols_to_draw = (256 / TILE_SIZE) + 1;  /* 33 */
    int r, i;

    for (r = 0; r < map_rows; r++) {
        int fb_y = fb_origin_y + r * TILE_SIZE * scale;
        if (fb_y >= 480) break;
        for (i = 0; i < cols_to_draw; i++) {
            int col      = (first_col + i) % visual_cols;   /* wrap at 32 */
            uint16_t tid = tiles_flat[r * map_cols + col];  /* stride = full width */
            if (tid == 0) continue;
            int src_x = (tid % 32) * TILE_SIZE;
            int src_y = (tid / 32) * TILE_SIZE;
            int fb_x  = fb_origin_x + (i * TILE_SIZE - x_offset) * scale;
            blit_sprite(fb, sheet, fb_x, fb_y, src_x, src_y, TILE_SIZE, TILE_SIZE, scale);
        }
    }
}

/* Draw an entire tilemap at absolute fb position (no scroll offset).
 * Used for title/complete BG and FG layers. blit_sprite handles clipping.
 * tile_id == 0 is skipped (empty tile sentinel). */
void tilemap_draw_full(uint32_t *fb, const SpriteSheet *sheet,
                       const uint16_t *tiles_flat, int map_cols, int map_rows,
                       int fb_origin_x, int fb_origin_y, int scale) {
    int r, c;
    for (r = 0; r < map_rows; r++) {
        int fb_y = fb_origin_y + r * TILE_SIZE * scale;
        if (fb_y >= 480) break;
        for (c = 0; c < map_cols; c++) {
            uint16_t tile_id = tiles_flat[r * map_cols + c];
            if (tile_id == 0) continue;
            {
                int src_x = (tile_id % 32) * TILE_SIZE;
                int src_y = (tile_id / 32) * TILE_SIZE;
                int fb_x = fb_origin_x + c * TILE_SIZE * scale;
                blit_sprite(fb, sheet, fb_x, fb_y, src_x, src_y, TILE_SIZE, TILE_SIZE, scale);
            }
        }
    }
}
