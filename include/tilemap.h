#ifndef TILEMAP_H
#define TILEMAP_H

#include <stdint.h>
#include "sprite.h"

#define SOLID_TILE_START_ID 9999

/* Returns 1 if the tile at game-pixel position (game_px, game_py) is a solid 
 * tile (index >= 705). Ignores scroll_x to match Python reference behavior.
 * game_px: 0..255, game_py: 0..191. */
int tilemap_is_solid(const uint16_t *tiles, int map_cols, int game_px, int game_py);

void tilemap_draw_bg(
    uint32_t *fb,
    const SpriteSheet *sheet,
    const uint16_t *tiles_flat,   // pointer to tiles[0][0] — flat row-major array
    int map_cols,                  // columns in map (32, 128, or 256)
    int map_rows,                  // rows in map (20 or 24)
    int scroll_x,                  // scroll position in game pixels (integer, already advanced by accum)
    int fb_origin_x,               // framebuffer x where column 0 of viewport starts (= OFFSET_X = 64)
    int fb_origin_y,               // framebuffer y where row 0 of tilemap starts (= OFFSET_Y + 32 = 80)
    int scale                      // pixel scale factor (= 2)
);

void tilemap_draw_full(uint32_t *fb, const SpriteSheet *sheet,
                       const uint16_t *tiles_flat, int map_cols, int map_rows,
                       int fb_origin_x, int fb_origin_y, int scale);

/* Vortex-stage BG: renders 33 visible columns wrapping at visual_cols (32),
 * indexed by the full map_cols row stride. Matches Python bltm from (0,0). */
void tilemap_draw_vortex_bg(uint32_t *fb, const SpriteSheet *sheet,
                             const uint16_t *tiles_flat, int map_cols, int map_rows,
                             int visual_cols, int scroll_x,
                             int fb_origin_x, int fb_origin_y, int scale);

#endif // TILEMAP_H
