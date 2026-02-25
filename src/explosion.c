#include "explosion.h"
#include "const.h"
#include "audio.h"

void explosion_spawn(Explosion *pool, int max, float x, float y, int delay) {
    /* Find first inactive slot */
    for (int i = 0; i < max; i++) {
        if (!pool[i].active) {
            pool[i].active      = 1;
            pool[i].pos.x       = x;
            pool[i].pos.y       = y;
            pool[i].delay       = delay;
            pool[i].frame       = 0;
            pool[i].frame_timer = EXPLOSION_FRAME_TICKS;
            if (pool[i].delay == 0) {
                sfx_play(SFX_EXPLODE_SMALL); /* Python Explosion.__init__: sound() when delay==0 */
            }
            return;
        }
    }
}

void explosion_update_all(Explosion *pool, int max) {
    for (int i = 0; i < max; i++) {
        if (!pool[i].active) continue;
        if (pool[i].delay > 0) {
            pool[i].delay--;
            if (pool[i].delay == 0) {
                sfx_play(SFX_EXPLODE_SMALL); /* Python Explosion.update(): sound() when delay reaches 0 */
            }
            continue;
        }
        pool[i].frame_timer--;
        if (pool[i].frame_timer <= 0) {
            pool[i].frame++;
            pool[i].frame_timer = EXPLOSION_FRAME_TICKS;
            if (pool[i].frame >= EXPLOSION_FRAMES) {
                pool[i].active = 0;
            }
        }
    }
}

void explosion_draw_all(uint32_t *fb, const SpriteSheet *sheet,
                        Explosion *pool, int max) {
    /* Sprite sheet UVs per frame */
    static const int frame_u[3] = { 0, 16, 32 };
    static const int frame_v[3] = { 64, 64, 64 };

    for (int i = 0; i < max; i++) {
        if (!pool[i].active) continue;
        if (pool[i].delay > 0) continue;   /* still waiting */
        int f = pool[i].frame;
        if (f < 0 || f >= EXPLOSION_FRAMES) continue;
        int dst_x = OFFSET_X + (int)pool[i].pos.x * SCALE;
        int dst_y = OFFSET_Y + (int)pool[i].pos.y * SCALE;
        blit_sprite(fb, sheet, dst_x, dst_y, frame_u[f], frame_v[f], 16, 16, SCALE);
    }
}

int explosion_any_active(const Explosion *pool, int max) {
    for (int i = 0; i < max; i++)
        if (pool[i].active) return 1;
    return 0;
}
