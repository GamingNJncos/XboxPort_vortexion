#ifndef CONST_H
#define CONST_H

/* ── Sprite colkey debug ─────────────────────────────────────────────────────
 * Uncomment COLKEY_VISIBLE to render Pyxel's magenta transparency key
 * (0xFFFF00FF) as solid pink instead of transparent.  Useful for automated
 * screen-capture analysis: player/enemy sprite bounding boxes become clearly
 * visible as pink rectangles against any background.
 * Leave commented out for normal gameplay rendering.
 * #define COLKEY_VISIBLE
 * ─────────────────────────────────────────────────────────────────────────── */

// Framebuffer (Xbox hardware)
#define FB_W 640
#define FB_H 480

// Game logical resolution (Pyxel native)
#define GAME_W 256
#define GAME_H 192

// Display scaling: 2x integer scale, centered in 640x480
#define SCALE 2
#define OFFSET_X 64    // (640 - 256*2) / 2 = 64
#define OFFSET_Y 48    // (480 - 192*2) / 2 = 48

// Tile system
#define TILE_SIZE 8
#define PLAYABLE_Y_MIN 16   // HUD top strip
#define PLAYABLE_Y_MAX 176  // HUD bottom strip

// Timing (no delta-time — frame-based at 60fps)
#define FPS 60
#define FRAME_MS 15   /* 15ms target; ~1ms xemu sleep overhead → ~62fps effective */

// Player
#define PLAYER_MOVE_SPEED 2.0f
#define PLAYER_DIAG_SPEED 1.414f   // 2.0 * 0.707
#define SHOT_DELAY_FRAMES 10
#define INVINCIBILITY_FRAMES 120
#define STARTING_LIVES 3
#define MAX_LIVES 9
#define MAX_WEAPONS 3
#define MAX_WEAPON_LEVEL 5

// Scroll (sub-pixel: 0.5 px/frame — accumulate 1 per frame, advance scroll_x every 2 frames)
#define SCROLL_SPEED_ACCUM 1     // add this each frame
#define SCROLL_SPEED_THRESH 2    // advance map_x when accum reaches this
#define MAP_W_NORMAL 2048
#define MAP_W_VORTEX 1024
#define MAP_H_TILES 20           // 160px / 8px per tile

// Scores
#define ENEMY_SCORE 100
#define BOSS_SCORE 5000
#define MAX_SCORE 999999

// Entity list limits
#define MAX_ENEMIES 64
#define MAX_BULLETS 128
#define MAX_POWERUPS 16
#define MAX_EXPLOSIONS 32
#define MAX_ENEMY_SHOTS 256
#define MAX_BOSSES 4

// Scroll speed (stage 1: 0.5 game-px/frame via accumulator)
#define SCROLL_X_SPEED 0.5f

// Boss fight X position (when scroll locks, bosses slide to this position)
#define BOSS_FIGHT_X 168

// Boss music trigger position (223 * 8 = 1784)
#define BOSS_MUSIC_X 1784

// Math
#define M_PI_F 3.14159265f

// Enemy
#define ENEMY_HIT_FRAMES 5
#define ENEMY_INVINCIBLE_START 15   // first 15 lifetime frames = no collision

// Powerup
#define POWERUP_CYCLE_GAP    8     // every N enemy kills, spawn one powerup
#define POWERUP_CYCLE_LEN    7     // length of TYPE_CYCLE array
#define POWERUP_W           16     // game pixels
#define POWERUP_H           16
#define POWERUP_SPEED_ACCUM  1     // sub-pixel accumulator per frame
#define POWERUP_SPEED_THRESH 2     // advance x by 1 when accum reaches this
#define BOMB_DAMAGE         30     // from Python const.py

#endif // CONST_H
