#ifndef GAME_TYPES_H
#define GAME_TYPES_H

#include <stdint.h>
#include "const.h"

typedef struct {
    float x, y;
} Vec2;

typedef struct {
    int x, y, w, h;
} Rect;

typedef enum GameStateID {
    STATE_TITLES = 0,
    STATE_STAGE = 1,
    STATE_GAME_COMPLETE = 2
} GameStateID;

typedef enum StageSubstate {
    SUBSTAGE_SPAWNED = 0,
    SUBSTAGE_PLAY,
    SUBSTAGE_DEAD,
    SUBSTAGE_PAUSED,
    SUBSTAGE_CLEAR,
    SUBSTAGE_GAME_OVER
} StageSubstate;

typedef enum WeaponType {
    WEAPON_A = 0,
    WEAPON_B = 1,
    WEAPON_C = 2
} WeaponType;

typedef enum PowerupType {
    POWERUP_LIFE = 0,
    POWERUP_WEAPON,
    POWERUP_BOMB
} PowerupType;

typedef enum EnemyType {
    ENEMY_A = 0,
    ENEMY_B,
    ENEMY_C,
    ENEMY_D,
    ENEMY_E,
    ENEMY_F,
    ENEMY_G,
    ENEMY_H,
    ENEMY_I,
    ENEMY_J,
    ENEMY_K,
    ENEMY_L,
    ENEMY_M,
    ENEMY_N,
    ENEMY_O,
    ENEMY_P,
    ENEMY_TYPE_COUNT
} EnemyType;

typedef struct Player {
    Vec2 pos;
    Vec2 vel;
    int lives;
    int score;
    WeaponType weapon;
    int weapon_levels[3];
    int shot_timer;
    int inv_timer;
    int active;
} Player;

typedef struct Bullet {
    Vec2 pos;
    Vec2 vel;
    int owner;
    int active;
    int damage;
    int weapon_type;
    int weapon_level;
} Bullet;

typedef struct Powerup {
    Vec2 pos;
    PowerupType type;
    int weapon_type;    /* 0=A,1=B,2=C — rotates every 60f for POWERUP_WEAPON */
    int weapon_timer;   /* frame counter for weapon type cycling */
    int move_accum;     /* sub-pixel movement accumulator */
    int bob_timer;      /* frame counter for vertical sine bob */
    int tint_idx;       /* 0-13: index into Pyxel pal cycle (colors 2-15) */
    int tint_timer;     /* increments each frame; tint_idx++ every 5 frames */
    int active;
} Powerup;

typedef struct Explosion {
    Vec2 pos;
    int active;
    int delay;        /* ticks to wait before starting (0=immediate) */
    int frame;        /* current animation frame index 0..2 */
    int frame_timer;  /* countdown within current frame (starts at 5) */
} Explosion;

/* Generic movement state for all 16 enemy types */
typedef struct {
    float f0, f1;   /* type-specific floats (vel_y, sine_t, accel, etc.) */
    int   i0, i1;   /* type-specific ints  (stage, flag, etc.) */
} EnemyMoveState;

/* Base enemy — covers both normal enemies and bosses */
typedef struct EnemyBase {
    float x, y;
    int w, h;           /* sprite size in game pixels (16 for normal, 32 for boss) */
    int u, v;           /* sprite sheet UV in game pixels */
    int flip_x, flip_y;
    int hp;
    int hit_frames;
    int lifetime;
    int score;
    int active;
    int is_boss;        /* 1 if boss (K/L/M), 0 otherwise */
    EnemyType type;
    EnemyMoveState ms;
    uint32_t boss_col;  /* ARGB32 of boss's palette 'colour'; 0 for non-bosses */
    uint32_t q_col[4];  /* per-quadrant target colors [TL,TR,BL,BR]; 0=no remap */
} EnemyBase;

/* Enemy-fired projectile */
typedef struct {
    float x, y;
    float vx, vy;
    int delay;
    int active;
} EnemyShot;

typedef struct GameCtx {
    GameStateID state;
    StageSubstate substage;
    int substage_timer;         /* frames spent in current substate */
    int stage_num;              /* 1..5 */
    int scroll_accum;
    int scroll_x;
    int frame_count;
    int bullet_count;
    int hi_score;

    /* Current stage map — set by game_init_stage() based on stage_num */
    const uint16_t *cur_map_tiles;   /* flat row-major tile array */
    const uint8_t  *cur_map_enemies; /* flat row-major enemy spawn array */
    int cur_map_cols;
    int cur_map_rows;

    /* Title / game-complete screen scroll (game pixels, decreases by 8/frame) */
    int title_scroll_x;
    int complete_scroll_x;
    int menu_selection;         /* 0=GAME START, 1=CONTINUE */

    Player player;
    Bullet bullets[128];
    Powerup powerups[16];
    Explosion explosions[32];
    EnemyBase enemies[64];
    int enemy_count;
    EnemyBase bosses[4];
    int boss_count;
    EnemyShot enemy_shots[256];
    int enemy_shot_count;
    int spawn_last_col;
    int bosses_alive;
    int stage_clear;
    int invincible;

    /* Powerup spawn cycle (reset each stage via game_init_stage memset) */
    int powerup_kill_cnt;   /* enemies killed since last powerup spawn */
    int powerup_cycle_idx;  /* next index into TYPE_CYCLE[POWERUP_CYCLE_LEN] */

    /* Vortex stage: fast BG scroll independent of scroll_x (8 game px/frame) */
    int vortex_bg_x;        /* screen-space offset in game pixels, decreases 8/frame */

    /* Boss stage scroll lock: set when scroll_x reaches map_end; bosses stop moving */
    int scroll_locked;
} GameCtx;

#endif // GAME_TYPES_H
