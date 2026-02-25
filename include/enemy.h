#ifndef VORTEXION_ENEMY_H
#define VORTEXION_ENEMY_H

#include <stdint.h>
#include "game_types.h"
#include "sprite.h"

/* ── Type-init table (UV, size, HP, score, is_boss) ─────────────────────────
 * Indexed by EnemyType enum (ENEMY_A=0 … ENEMY_P=15).
 * All normal enemies: u = type*16, v = 80, w=16, h=16, score=100
 * Bosses K/L/M:       w=32, h=32, score=5000
 * ─────────────────────────────────────────────────────────────────────────── */
typedef struct {
    int u, v, w, h;
    int hp;
    int score;
    int is_boss;
    uint32_t boss_col;   /* ARGB32 of boss's palette 'colour'; 0 for non-bosses */
    uint32_t q_col[4];   /* per-quadrant target colors [TL,TR,BL,BR]; 0=no remap */
} EnemyTypeDef;
extern const EnemyTypeDef enemy_type_defs[ENEMY_TYPE_COUNT];

/* ── Vtable (one update + draw fn per type) ─────────────────────────────────*/
typedef struct {
    void (*update)(EnemyBase *e, GameCtx *ctx);
    void (*draw)(const EnemyBase *e, uint32_t *fb, const SpriteSheet *sheet);
} EnemyVTable;
extern const EnemyVTable enemy_vtable[ENEMY_TYPE_COUNT];

/* ── Public API ─────────────────────────────────────────────────────────────*/

/* Initialise an enemy slot from the type-init table */
void enemy_init(EnemyBase *e, EnemyType type, float x, float y);

/* Apply damage; calls enemy_destroy if hp reaches 0 */
void enemy_hit(EnemyBase *e, int dmg, GameCtx *ctx);

/* Mark dead: add score, add explosion, maybe spawn powerup */
void enemy_destroy(EnemyBase *e, GameCtx *ctx);

/* Shoot a bullet at a fixed angle (radians, 0=right, π/2=down) */
void enemy_shoot_at_angle(EnemyBase *e, GameCtx *ctx,
                          float speed, float angle_rad, int delay);

/* Compute angle to player centre and call enemy_shoot_at_angle */
void enemy_shoot_at_player(EnemyBase *e, GameCtx *ctx, float speed, int delay);

/* Base per-frame bookkeeping: lifetime++, hit_frames-- */
void enemy_base_update(EnemyBase *e);

/* Dispatch to vtable update (calls enemy_base_update first) */
void enemy_update(EnemyBase *e, GameCtx *ctx);

/* Dispatch to vtable draw */
void enemy_draw(const EnemyBase *e, uint32_t *fb, const SpriteSheet *sheet);

/* Common draw: blit sprite + white-fill hit flash */
void enemy_base_draw(const EnemyBase *e, uint32_t *fb, const SpriteSheet *sheet);

/* Check one enemy against all active player bullets; remove bullets on hit */
void enemy_check_player_shot_collisions(EnemyBase *e, GameCtx *ctx);

#endif // VORTEXION_ENEMY_H
