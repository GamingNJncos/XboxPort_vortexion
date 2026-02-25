#include "hud.h"
#include <string.h>

/* Framebuffer is 640×480, stride=640.
 * HUD top band: rows 48..79 (32 rows × 640 pixels)
 * HUD bottom band: rows 400..431 (32 rows × 640 pixels) */
#define FB_W         640
#define HUD_TOP_Y    48
#define HUD_BOT_Y    400
#define HUD_HEIGHT   32
#define HUD_COLOR    0xFF000000u   /* solid black */

void hud_draw_bars(uint32_t *fb) {
    /* Top HUD band */
    for (int y = HUD_TOP_Y; y < HUD_TOP_Y + HUD_HEIGHT; y++) {
        uint32_t *row = fb + y * FB_W;
        for (int x = 0; x < FB_W; x++) {
            row[x] = HUD_COLOR;
        }
    }
    /* Bottom HUD band */
    for (int y = HUD_BOT_Y; y < HUD_BOT_Y + HUD_HEIGHT; y++) {
        uint32_t *row = fb + y * FB_W;
        for (int x = 0; x < FB_W; x++) {
            row[x] = HUD_COLOR;
        }
    }
}
