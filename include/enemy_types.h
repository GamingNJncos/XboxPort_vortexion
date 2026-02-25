#ifndef ENEMY_TYPES_H
#define ENEMY_TYPES_H

#include "game_types.h"
#include "sprite.h"
#include <stdint.h>

/* Per-type update function declarations (called from vtable) */
void enemy_a_update(EnemyBase *e, GameCtx *ctx);
void enemy_b_update(EnemyBase *e, GameCtx *ctx);
void enemy_c_update(EnemyBase *e, GameCtx *ctx);
void enemy_d_update(EnemyBase *e, GameCtx *ctx);
void enemy_e_update(EnemyBase *e, GameCtx *ctx);
void enemy_f_update(EnemyBase *e, GameCtx *ctx);
void enemy_g_update(EnemyBase *e, GameCtx *ctx);
void enemy_h_update(EnemyBase *e, GameCtx *ctx);
void enemy_i_update(EnemyBase *e, GameCtx *ctx);
void enemy_j_update(EnemyBase *e, GameCtx *ctx);
void enemy_k_update(EnemyBase *e, GameCtx *ctx);
void enemy_l_update(EnemyBase *e, GameCtx *ctx);
void enemy_m_update(EnemyBase *e, GameCtx *ctx);
void enemy_n_update(EnemyBase *e, GameCtx *ctx);
void enemy_o_update(EnemyBase *e, GameCtx *ctx);
void enemy_p_update(EnemyBase *e, GameCtx *ctx);

/* All types share the base draw (blit + hit-flash); no per-type draw needed */

#endif // ENEMY_TYPES_H
