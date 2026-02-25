#include <stdint.h>
#include <stdlib.h>
#include <SDL.h>
#include "sprite.h"

/* Pyxel 2.0 standard 16-color palette (index -> 0xAARRGGBB) */
static const uint32_t pyxel_palette[16] = {
    0xFF000000UL, /* 0:  black      */
    0xFF1D2B53UL, /* 1:  navy       */
    0xFF7E2553UL, /* 2:  dark purple */
    0xFF008751UL, /* 3:  dark green  */
    0xFFAB5236UL, /* 4:  brown       */
    0xFF5F574FUL, /* 5:  dark gray   */
    0xFFC2C3C7UL, /* 6:  light gray  */
    0xFFFFFFFFUL, /* 7:  white       */
    0xFFFF004DUL, /* 8:  red         */
    0xFFFFA300UL, /* 9:  orange      */
    0xFFFFEC27UL, /* 10: yellow      */
    0xFF00E436UL, /* 11: green       */
    0xFF29ADFFUL, /* 12: blue        */
    0xFF83769CUL, /* 13: lavender    */
    0xFFFF77A8UL, /* 14: pink        */
    0xFFFFCCAAUL  /* 15: peach       */
};

/* Python palette indices documented from source (srcZIP/vortexion/src/):
 * - Player: Uses index 15 (white in Python custom palette, peach in Pyxel 2.0).
 * - Powerups: Replace white (15) with weapon (12: blue), bomb (8: red), or life (14: pink).
 * - Enemy hit flash: Replaces enemy color with 15 (white/peach).
 * - Enemy shots: Replace white (15) with shot color.
 * - Bosses (K, L, M): Per-quadrant palette remapping using indices like 6, 8, 9, 13 etc.
 */

int sheet_load(SpriteSheet *s, const char *path) {
    SDL_RWops *rw = SDL_RWFromFile(path, "rb");
    if (!rw) {
        return 0;
    }

    Sint64 size = SDL_RWsize(rw);
    if (size != 256 * 256 * 4) {
        SDL_RWclose(rw);
        return 0;
    }

    s->pixels = (uint32_t *)malloc(256 * 256 * sizeof(uint32_t));
    if (!s->pixels) {
        SDL_RWclose(rw);
        return 0;
    }

    uint8_t *temp_buf = (uint8_t *)malloc(256 * 256 * 4);
    if (!temp_buf) {
        free(s->pixels);
        s->pixels = NULL;
        SDL_RWclose(rw);
        return 0;
    }

    if (SDL_RWread(rw, temp_buf, 1, 256 * 256 * 4) != 256 * 256 * 4) {
        free(temp_buf);
        free(s->pixels);
        s->pixels = NULL;
        SDL_RWclose(rw);
        return 0;
    }

    for (int i = 0; i < 256 * 256; i++) {
        uint8_t r = temp_buf[i * 4 + 0];
        uint8_t g = temp_buf[i * 4 + 1];
        uint8_t b = temp_buf[i * 4 + 2];
        uint8_t a = temp_buf[i * 4 + 3];
        // Convert RGBA8 to ARGB32 for Xbox (Little Endian: B, G, R, A in memory)
        s->pixels[i] = (a << 24) | (r << 16) | (g << 8) | b;
    }

    s->w = 256;
    s->h = 256;

    SDL_RWclose(rw);
    free(temp_buf);
    return 1;
}

void sheet_free(SpriteSheet *s) {
    if (s && s->pixels) {
        free(s->pixels);
        s->pixels = NULL;
    }
}

void blit_sprite(uint32_t *fb, const SpriteSheet *s, int dst_x, int dst_y, int src_x, int src_y, int w, int h, int scale) {
    if (!fb || !s || !s->pixels) return;

    for (int sy = 0; sy < h; sy++) {
        for (int sx = 0; sx < w; sx++) {
            // Source pixel lookup
            uint32_t src_pixel = s->pixels[(src_y + sy) * s->w + (src_x + sx)];

            /* Skip transparent pixels (alpha=0) and Pyxel colkey (magenta 0xFFFF00FF).
             * To render colkey as visible pink for automated screen-capture analysis,
             * define COLKEY_VISIBLE in const.h before building. */
#ifndef COLKEY_VISIBLE
            if (src_pixel == 0xFFFF00FFUL) {
                continue;
            }
#endif
            if (((src_pixel >> 24) & 0xFF) == 0) {
                continue;
            }

            // Draw scaled pixel
            for (int dy = 0; dy < scale; dy++) {
                for (int dx = 0; dx < scale; dx++) {
                    int fb_x = dst_x + sx * scale + dx;
                    int fb_y = dst_y + sy * scale + dy;

                    // Bounds check for 640x480 framebuffer
                    if (fb_x >= 0 && fb_x < 640 && fb_y >= 0 && fb_y < 480) {
                        fb[fb_y * 640 + fb_x] = src_pixel;
                    }
                }
            }
        }
    }
}

void blit_sprite_palette(uint32_t *fb, const SpriteSheet *sheet,
                         int fb_x, int fb_y,
                         int src_x, int src_y, int w, int h, int scale,
                         uint8_t src_pal_idx, uint8_t dst_pal_idx)
{
    if (!fb || !sheet || !sheet->pixels || src_pal_idx >= 16 || dst_pal_idx >= 16) return;

    uint32_t src_col = pyxel_palette[src_pal_idx];
    uint32_t dst_col = pyxel_palette[dst_pal_idx];

    for (int sy = 0; sy < h; sy++) {
        for (int sx = 0; sx < w; sx++) {
            uint32_t pix = sheet->pixels[(src_y + sy) * sheet->w + (src_x + sx)];
            if (pix == 0xFFFF00FFUL) continue;
            if (((pix >> 24) & 0xFF) == 0) continue;

            if (pix == src_col) pix = dst_col;

            for (int dy = 0; dy < scale; dy++) {
                for (int dx = 0; dx < scale; dx++) {
                    int out_x = fb_x + sx * scale + dx;
                    int out_y = fb_y + sy * scale + dy;
                    if (out_x >= 0 && out_x < 640 && out_y >= 0 && out_y < 480) {
                        fb[out_y * 640 + out_x] = pix;
                    }
                }
            }
        }
    }
}

void blit_sprite_flip(uint32_t *fb, const SpriteSheet *s,
                      int dst_x, int dst_y,
                      int src_x, int src_y, int w, int h, int scale,
                      int flip_x, int flip_y)
{
    int sy, sx, dy, dx;
    if (!fb || !s || !s->pixels) return;

    for (sy = 0; sy < h; sy++) {
        int actual_sy = flip_y ? (h - 1 - sy) : sy;
        for (sx = 0; sx < w; sx++) {
            int actual_sx = flip_x ? (w - 1 - sx) : sx;
            uint32_t src_pixel = s->pixels[(src_y + actual_sy) * s->w + (src_x + actual_sx)];
#ifndef COLKEY_VISIBLE
            if (src_pixel == 0xFFFF00FFu) continue;
#endif
            if (((src_pixel >> 24) & 0xFF) == 0) continue;

            for (dy = 0; dy < scale; dy++) {
                for (dx = 0; dx < scale; dx++) {
                    int fb_x = dst_x + sx * scale + dx;
                    int fb_y = dst_y + sy * scale + dy;
                    if (fb_x >= 0 && fb_x < 640 && fb_y >= 0 && fb_y < 480) {
                        fb[fb_y * 640 + fb_x] = src_pixel;
                    }
                }
            }
        }
    }
}

void blit_sprite_flip_tinted(uint32_t *fb, const SpriteSheet *s,
                              int dst_x, int dst_y,
                              int src_x, int src_y, int w, int h, int scale,
                              int flip_x, int flip_y,
                              uint32_t src_col, uint32_t dst_col)
{
    int sy, sx, dy, dx;
    if (!fb || !s || !s->pixels) return;

    for (sy = 0; sy < h; sy++) {
        int actual_sy = flip_y ? (h - 1 - sy) : sy;
        for (sx = 0; sx < w; sx++) {
            int actual_sx = flip_x ? (w - 1 - sx) : sx;
            uint32_t pix = s->pixels[(src_y + actual_sy) * s->w + (src_x + actual_sx)];
            if (pix == 0xFFFF00FFu) continue;
            if (((pix >> 24) & 0xFF) == 0) continue;
            if (pix == src_col) pix = dst_col;
            for (dy = 0; dy < scale; dy++) {
                for (dx = 0; dx < scale; dx++) {
                    int fb_x = dst_x + sx * scale + dx;
                    int fb_y = dst_y + sy * scale + dy;
                    if (fb_x >= 0 && fb_x < 640 && fb_y >= 0 && fb_y < 480)
                        fb[fb_y * 640 + fb_x] = pix;
                }
            }
        }
    }
}

/* Rainbow bright colors — 7 distinct Pyxel palette hues (indices 8-14) */
static const uint32_t rainbow_colors[7] = {
    0xFFFF004DUL, /* red      (8)  */
    0xFFFFA300UL, /* orange   (9)  */
    0xFFFFEC27UL, /* yellow   (10) */
    0xFF00E436UL, /* green    (11) */
    0xFF29ADFFUL, /* blue     (12) */
    0xFF83769CUL, /* lavender (13) */
    0xFFFF77A8UL  /* pink     (14) */
};

/* Blit a sprite tile, replacing non-background opaque pixels with tint_color.
 * Skips magenta colkey, alpha=0, and black (palette 0 background).
 * Used for draw_text_rainbow so only font foreground pixels get the tint. */
static void blit_sprite_colored(uint32_t *fb, const SpriteSheet *s,
                                 int dst_x, int dst_y,
                                 int src_x, int src_y, int w, int h, int scale,
                                 uint32_t tint_color)
{
    int sy, sx, dy, dx;
    if (!fb || !s || !s->pixels) return;
    for (sy = 0; sy < h; sy++) {
        for (sx = 0; sx < w; sx++) {
            uint32_t pix = s->pixels[(src_y + sy) * s->w + (src_x + sx)];
            if (pix == 0xFFFF00FFUL) continue;           /* magenta colkey */
            if (((pix >> 24) & 0xFF) == 0) continue;     /* alpha=0 */
            if (pix == pyxel_palette[0]) continue;       /* black = Pyxel colkey-0 background */
            for (dy = 0; dy < scale; dy++) {
                for (dx = 0; dx < scale; dx++) {
                    int fx = dst_x + sx * scale + dx;
                    int fy = dst_y + sy * scale + dy;
                    if (fx >= 0 && fx < 640 && fy >= 0 && fy < 480)
                        fb[fy * 640 + fx] = tint_color;
                }
            }
        }
    }
}

void draw_text_rainbow(uint32_t *fb, const SpriteSheet *sheet,
                       int game_x, int game_y, const char *text, int anim_frame)
{
    int fb_x = 64 + game_x * 2;
    int fb_y = 48 + game_y * 2;
    int idx  = 0;
    while (*text) {
        int code = (int)(unsigned char)(*text++);
        /* Auto-uppercase lowercase a-z */
        if (code >= 'a' && code <= 'z') code -= 32;
        if (code < 32 || code > 95) {
            fb_x += 8 * 2;
            idx++;
            continue;
        }
        code -= 32;
        {
            int src_x = (code % 32) * 8;
            int src_y = 240 + (code / 32) * 8;
            uint32_t col = rainbow_colors[(idx + anim_frame) % 7];
            blit_sprite_colored(fb, sheet, fb_x, fb_y, src_x, src_y, 8, 8, 2, col);
        }
        fb_x += 8 * 2;
        idx++;
    }
}

/* Draw text at raw framebuffer coordinates with a solid color (scale-aware).
 * Lowercase auto-uppercased. Uses blit_sprite_colored for foreground-only tint. */
void draw_text_fb_colored(uint32_t *fb, const SpriteSheet *sheet,
                          int fb_x, int fb_y, const char *text, int scale, uint32_t color)
{
    while (*text) {
        int code = (int)(unsigned char)(*text++);
        if (code >= 'a' && code <= 'z') code -= 32;
        if (code < 32 || code > 95) {
            fb_x += 8 * scale;
            continue;
        }
        code -= 32;
        {
            int src_x = (code % 32) * 8;
            int src_y = 240 + (code / 32) * 8;
            blit_sprite_colored(fb, sheet, fb_x, fb_y, src_x, src_y, 8, 8, scale, color);
        }
        fb_x += 8 * scale;
    }
}

/* Draw text in animated rainbow colors at raw framebuffer coordinates (scale-aware).
 * Lowercase auto-uppercased. Each character gets a different rainbow palette color
 * shifted by anim_frame. */
void draw_text_fb_rainbow(uint32_t *fb, const SpriteSheet *sheet,
                          int fb_x, int fb_y, const char *text, int scale, int anim_frame)
{
    int idx = 0;
    while (*text) {
        int code = (int)(unsigned char)(*text++);
        if (code >= 'a' && code <= 'z') code -= 32;
        if (code < 32 || code > 95) {
            fb_x += 8 * scale;
            idx++;
            continue;
        }
        code -= 32;
        {
            int src_x = (code % 32) * 8;
            int src_y = 240 + (code / 32) * 8;
            uint32_t col = rainbow_colors[(idx + anim_frame) % 7];
            blit_sprite_colored(fb, sheet, fb_x, fb_y, src_x, src_y, 8, 8, scale, col);
        }
        fb_x += 8 * scale;
        idx++;
    }
}

/* Draw one 8×8 bitmap character at custom pixel output size (char_w × char_h).
 * Source pixels are nearest-neighbor mapped from 8×8 to char_w×char_h.
 * Skips magenta colkey, alpha=0, and black (palette 0 background). */
static void blit_char_sized_colored(uint32_t *fb, const SpriteSheet *s,
                                     int dst_x, int dst_y,
                                     int src_x, int src_y,
                                     int char_w, int char_h,
                                     uint32_t tint_color)
{
    int dy, dx;
    for (dy = 0; dy < char_h; dy++) {
        int sy = src_y + (dy * 8) / char_h;
        for (dx = 0; dx < char_w; dx++) {
            int sx = src_x + (dx * 8) / char_w;
            uint32_t pix = s->pixels[sy * s->w + sx];
            if (pix == 0xFFFF00FFu) continue;
            if (((pix >> 24) & 0xFF) == 0) continue;
            if (pix == 0xFF000000u) continue;
            {
                int fx = dst_x + dx, fy = dst_y + dy;
                if (fx >= 0 && fx < 640 && fy >= 0 && fy < 480)
                    fb[fy * 640 + fx] = tint_color;
            }
        }
    }
}

void draw_text_fb(uint32_t *fb, const SpriteSheet *sheet,
                  int fb_x, int fb_y, const char *text, int scale) {
    while (*text) {
        int code = (int)(unsigned char)(*text++);
        if (code < 32 || code > 95) {
            fb_x += 8 * scale;
            continue;
        }
        code -= 32;
        int src_x = (code % 32) * 8;
        int src_y = 240 + (code / 32) * 8;
        blit_sprite(fb, sheet, fb_x, fb_y, src_x, src_y, 8, 8, scale);
        fb_x += 8 * scale;
    }
}

void draw_text(uint32_t *fb, const SpriteSheet *sheet, int game_x, int game_y, const char *text) {
    int fb_x = 64 + game_x * 2;
    int fb_y = 48 + game_y * 2;
    while (*text) {
        int code = (int)(unsigned char)(*text++);
        if (code < 32 || code > 95) {
            fb_x += 8 * 2;
            continue;
        }
        code -= 32;
        int src_x = (code % 32) * 8;
        int src_y = 240 + (code / 32) * 8;
        blit_sprite(fb, sheet, fb_x, fb_y, src_x, src_y, 8, 8, 2);
        fb_x += 8 * 2;
    }
}

/* Draw text at raw framebuffer coordinates using custom per-character pixel size.
 * char_w × char_h determines output size. Source font is always 8×8 bitmap. */
void draw_text_fb_colored_sized(uint32_t *fb, const SpriteSheet *sheet,
                                 int fb_x, int fb_y, const char *text,
                                 int char_w, int char_h, uint32_t color)
{
    while (*text) {
        int code = (int)(unsigned char)(*text++);
        if (code >= 'a' && code <= 'z') code -= 32;
        if (code < 32 || code > 95) {
            fb_x += char_w;
            continue;
        }
        code -= 32;
        {
            int src_x = (code % 32) * 8;
            int src_y = 240 + (code / 32) * 8;
            blit_char_sized_colored(fb, sheet, fb_x, fb_y, src_x, src_y, char_w, char_h, color);
        }
        fb_x += char_w;
    }
}

/* Draw text in animated rainbow colors using custom per-character pixel size. */
void draw_text_fb_rainbow_sized(uint32_t *fb, const SpriteSheet *sheet,
                                 int fb_x, int fb_y, const char *text,
                                 int char_w, int char_h, int anim_frame)
{
    int idx = 0;
    while (*text) {
        int code = (int)(unsigned char)(*text++);
        if (code >= 'a' && code <= 'z') code -= 32;
        if (code < 32 || code > 95) {
            fb_x += char_w;
            idx++;
            continue;
        }
        code -= 32;
        {
            int src_x = (code % 32) * 8;
            int src_y = 240 + (code / 32) * 8;
            uint32_t col = rainbow_colors[(idx + anim_frame) % 7];
            blit_char_sized_colored(fb, sheet, fb_x, fb_y, src_x, src_y, char_w, char_h, col);
        }
        fb_x += char_w;
        idx++;
    }
}
