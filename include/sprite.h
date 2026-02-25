#ifndef SPRITE_H
#define SPRITE_H

#include <stdint.h>

typedef struct {
    uint32_t *pixels;
    int w, h;
} SpriteSheet;

/**
 * Loads a raw RGBA8 image from the specified path into a SpriteSheet.
 * Path should be an absolute path (e.g., starting with D:\ on Xbox).
 * Converts RGBA8 to ARGB32 for Xbox framebuffer compatibility.
 * Returns 1 on success, 0 on failure.
 */
int sheet_load(SpriteSheet *s, const char *path);

/**
 * Frees the memory allocated for the SpriteSheet's pixels.
 */
void sheet_free(SpriteSheet *s);

/**
 * Renders a w*h source pixel area at scale, centered at dst_x, dst_y in fb (640x480).
 * dst_x/dst_y are the top-left corner of the scaled output in framebuffer space.
 * Skips pixels with alpha=0 (no alpha blending).
 */
void blit_sprite(uint32_t *fb, const SpriteSheet *s, int dst_x, int dst_y, int src_x, int src_y, int w, int h, int scale);

/* Like blit_sprite but remaps pixels matching pyxel_palette[src_pal_idx] 
 * to pyxel_palette[dst_pal_idx]. Uses Pyxel 2.0 standard palette. */
void blit_sprite_palette(uint32_t *fb, const SpriteSheet *sheet,
                         int fb_x, int fb_y,
                         int src_x, int src_y, int w, int h, int scale,
                         uint8_t src_pal_idx, uint8_t dst_pal_idx);

/* Like blit_sprite but mirrors the source when flip_x or flip_y is non-zero.
 * Used for boss composite draw (4 mirrored 16×16 quadrants). */
void blit_sprite_flip(uint32_t *fb, const SpriteSheet *s, int dst_x, int dst_y,
                      int src_x, int src_y, int w, int h, int scale,
                      int flip_x, int flip_y);

/* Like blit_sprite_flip but also remaps pixels matching src_col to dst_col.
 * Used for boss per-quadrant palette remapping (Python px.pal equivalent). */
void blit_sprite_flip_tinted(uint32_t *fb, const SpriteSheet *s,
                              int dst_x, int dst_y,
                              int src_x, int src_y, int w, int h, int scale,
                              int flip_x, int flip_y,
                              uint32_t src_col, uint32_t dst_col);

void draw_text(uint32_t *fb, const SpriteSheet *sheet, int game_x, int game_y, const char *text);

/* Draw text at raw framebuffer coordinates with explicit scale (1 or 2).
 * Use for overlays that need a size other than the game's default SCALE=2. */
void draw_text_fb(uint32_t *fb, const SpriteSheet *sheet,
                  int fb_x, int fb_y, const char *text, int scale);

/* Draw text in animated rainbow colors. Each character gets a different color
 * from the Pyxel-bright palette, shifted by anim_frame for animation.
 * Uses game coordinates (SCALE=2). Lowercase is auto-uppercased. */
void draw_text_rainbow(uint32_t *fb, const SpriteSheet *sheet,
                       int game_x, int game_y, const char *text, int anim_frame);

/* Draw text at raw fb coords with a single solid color. Lowercase auto-uppercased.
 * Use with scale=1 for half-size text relative to the game's default SCALE=2. */
void draw_text_fb_colored(uint32_t *fb, const SpriteSheet *sheet,
                          int fb_x, int fb_y, const char *text, int scale, uint32_t color);

/* Draw text in animated rainbow colors at raw fb coords with explicit scale.
 * Lowercase auto-uppercased. anim_frame shifts the color cycle each call. */
void draw_text_fb_rainbow(uint32_t *fb, const SpriteSheet *sheet,
                          int fb_x, int fb_y, const char *text, int scale, int anim_frame);

void draw_text_fb_colored_sized(uint32_t *fb, const SpriteSheet *sheet,
                                 int fb_x, int fb_y, const char *text,
                                 int char_w, int char_h, uint32_t color);
void draw_text_fb_rainbow_sized(uint32_t *fb, const SpriteSheet *sheet,
                                 int fb_x, int fb_y, const char *text,
                                 int char_w, int char_h, int anim_frame);

#endif // SPRITE_H
