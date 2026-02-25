# Vortexion — Xbox Port Plan

**Source:** https://github.com/helpcomputer/vortexion
**Source dir:** `srcZIP/vortexion/`
**Language:** Python 3 / Pyxel 2.0
**Genre:** Retro horizontal shoot-em-up (MSX/SG-1000 aesthetic)
**Lines of source:** ~2255 across 37 files

---

## Architecture Overview

### Display
- Logical resolution: **256×192** (Pyxel native)
- Rendered to Xbox 640×480 framebuffer — must scale 2.5× or letterbox
- Tile size: **8×8 px**
- HUD: top 16px + bottom 16px reserved; playable area = 256×160
- Sprite sheet: single `gfx.png` (256×256, 16-color indexed palette)

### Game States
```
TITLES → STAGE (substates: SPAWNED→PLAY→DEAD→PAUSED→CLEAR→GAME_OVER) → GAME_COMPLETE → TITLES
```

### Stage Structure (5 stages)
- Stages 1, 3, 5 = normal horizontal scroll + boss at end
- Stages 2, 4 = vortex (special dual-scroll layer, no boss)
- Map files: `.tmx` (Tiled XML format, CSV layer encoding)
  - Layer 0 = background tiles
  - Layer 1 = enemy spawn markers (tile index encodes enemy type)
- Map width: 2048px (normal), 1024px (vortex); height: 160px (20 tiles)
- Scroll speed: 0.5 px/frame

### Enemy System
- 16 enemy types (A–P) + 3 bosses (K, L, M)
- Spawned by scanning enemy layer of tilemap as screen scrolls
- Each enemy is its own Python class → translate to C struct + vtable pattern
- Bosses tracked in separate list; stage clear when all bosses dead

### Player
- Speed: 2.0 px/frame (diagonal: 2×0.707)
- 3 weapon types (A, B, C), each with 6 levels
- Shot delay: 10 frames
- Invincibility: 120 frames after hit
- Starting lives: 3, max 9
- On death: -2 weapon levels, reset to weapon A

### Audio
- Music: 8 JSON files (stage music, boss, stage clear, game over, title, vortex, complete)
- SFX: `sounds.pyxres` (Pyxel binary sound bank)
- 3 music channels (0–2) + 1 SFX channel (3)
- Fade-out: linear decay at scroll_x=1664px; boss music at scroll_x=1784px
- **Xbox plan:** Convert Pyxel music JSON → WAV files via Pyxel export; use direct XAudio (same as purelaxJS)

### Input
| Action | Xbox Gamepad |
|--------|-------------|
| Move up/down/left/right | D-pad |
| Shoot | A button |
| Pause | B button |

---

## C Port File Plan

```
projects/vortexion/
├── Makefile                    # nxdk (already scaffolded)
├── PORTING_PLAN.md             # This file
├── src/
│   ├── main.c                  # nxdk entry, init, game loop
│   ├── const.h                 # All constants (#defines)
│   ├── game_types.h            # Shared structs/enums
│   ├── game.c / game.h         # Top-level state machine
│   ├── input.c / input.h       # SDL gamepad → pressing/tapped bitmask
│   ├── sprite.c / sprite.h     # gfx.png blit, AABB collision
│   ├── tilemap.c / tilemap.h   # TMX pre-parsed → tile arrays; scroll; solid detection
│   ├── player.c / player.h     # Player state, movement, weapons, invincibility
│   ├── player_shot.c           # Player shot types by weapon/level
│   ├── enemy.c / enemy.h       # Base enemy struct + dispatch table
│   ├── enemy_a.c ... enemy_p.c # 16 enemy types (AI patterns)
│   ├── boss_k.c ... boss_m.c   # 3 boss types
│   ├── enemy_shot.c            # Enemy projectiles
│   ├── enemy_spawn.c           # Tilemap-driven spawner
│   ├── powerup.c / powerup.h   # Item drops (life, weapon, bomb)
│   ├── explosion.c             # Particle/sprite explosions
│   ├── hud.c / hud.h           # Score, lives, weapon level display
│   ├── audio.c / audio.h       # XAudio direct (reuse purelaxJS pattern)
│   ├── game_state_titles.c     # Title screen state
│   ├── game_state_stage.c      # Stage play state
│   └── game_state_complete.c   # Game complete state
└── assets/
    ├── gfx.rgba                # gfx.png → raw RGBA (use SDL_LoadBMP or stb_image)
    ├── stage_1.bin ... stage_5.bin  # Pre-parsed TMX tile arrays
    ├── title.bin / complete.bin
    ├── music_title.wav         # Pyxel JSON → WAV export
    ├── music_stage_1.wav ... music_stage_5.wav
    ├── music_boss.wav
    ├── music_stage_clear.wav
    ├── music_game_over.wav
    ├── music_game_complete.wav
    ├── music_vortex.wav
    └── sounds.wav              # SFX bank (8 effects)
```

---

## Key Constants

```c
#define SCREEN_W          256
#define SCREEN_H          192
#define TILE_SIZE         8
#define PLAYABLE_Y_MIN    16
#define PLAYABLE_Y_MAX    176
#define FPS               60

#define PLAYER_MOVE_SPEED     2.0f
#define SHOT_DELAY_FRAMES     10
#define INVINCIBILITY_FRAMES  120
#define STARTING_LIVES        3
#define MAX_LIVES             9
#define MAX_WEAPONS           3
#define MAX_WEAPON_LEVEL      5
#define SCROLL_SPEED_FIXED    1   // 0.5 px/frame → use sub-pixel accumulator

#define MAP_W_NORMAL     2048
#define MAP_W_VORTEX     1024
#define MAP_H            160

#define ENEMY_SCORE      100
#define BOSS_SCORE       5000
#define MAX_SCORE        999999
```

### Color Palette (Pyxel 16-color → ARGB32)
```c
static const uint32_t PALETTE[16] = {
    0x00000000, // 0 transparent (magenta key → alpha 0)
    0xFF000000, // 1 black
    0xFF21C842, // 2 dark green
    0xFF5EDC78, // 3 light green
    0xFF5455ED, // 4 dark blue
    0xFF7D76FC, // 5 light blue
    0xFFD4524D, // 6 dark red
    0xFF42EBF5, // 7 cyan
    0xFFFC5554, // 8 bright red
    0xFFFF7978, // 9 light red
    0xFFD4C154, // 10 dark yellow
    0xFFE6CE80, // 11 light yellow
    0xFF21B03B, // 12 mid green
    0xFFC95BBA, // 13 pink
    0xFFCCCCCC, // 14 gray
    0xFFFFFFFF, // 15 white
};
```

---

## Xbox-Specific Porting Notes

1. **Framebuffer scaling:** Game is 256×192, Xbox is 640×480.
   - Option A: Letterbox with black bars, center 256×192 at 2x (512×384) with black bars
   - Option B: Scale to full 640×480 (lossy, non-integer scale)
   - **Recommended: 2× integer scale centered** → 512×384 at offset (64, 48)

2. **Sprite sheet:** Load `gfx.png` via `stbi_load_from_memory` + `SDL_RWFromFile` (same as purelaxJS/dickfight). Apply palette LUT. Use color index 0 = transparent (skip pixel).

3. **TMX parsing:** Pre-convert TMX → C arrays offline. Each stage = `uint8_t tiles[ROWS][COLS]` and `uint8_t enemies[ROWS][COLS]`. Embed as header files or load from D:\ assets.

4. **Audio:** Same XAudio direct pattern as purelaxJS (purelaxJS xlax_sound.c is the reference). Pre-convert Pyxel JSON music to WAV using Pyxel's `--export_sound` tool.

5. **Sub-pixel scroll:** Scroll speed is 0.5 px/frame — use fixed-point accumulator:
   ```c
   scroll_frac += 1;  // +0.5 per frame × 2 = 1 per 2 frames
   if (scroll_frac >= 2) { scroll_x++; scroll_frac = 0; }
   ```

6. **No delta-time:** All movement is frame-based at 60 FPS. Use `GetTickCount()` only for frame cap.

7. **Double buffer:** `static uint32_t back_buf[640*480]` pattern (purelaxJS/dickfight reference).

8. **Enemy dispatch:** Use array of function pointers or switch statement per enemy type in update/draw loop.

---

## Porting Phases

### Phase 0 — Asset Prep (pre-code)
- [ ] Convert `gfx.png` → `gfx.rgba` (RGBA8 raw dump for stb_image bypass)
- [ ] Convert TMX files → C tile arrays (Python script)
- [ ] Export Pyxel music JSON → WAV files
- [ ] Extract SFX from `sounds.pyxres`

### Phase 1 — Core Scaffold
- [ ] `const.h` — all constants
- [ ] `game_types.h` — shared structs (Vec2, Rect, GameState enum)
- [ ] `main.c` — nxdk init, 60fps loop
- [ ] `input.c` — SDL gamepad polling
- [ ] `sprite.c` — gfx.png load, blit with color 0 = transparent, AABB

### Phase 2 — Rendering
- [ ] `tilemap.c` — load pre-parsed tile arrays, render tile layer at scroll_x
- [ ] `stage_background.c` — normal scroll + vortex dual-scroll
- [ ] `hud.c` — score/lives/weapon display (bitmap font from gfx.png)

### Phase 3 — Player
- [ ] `player.c` — movement, weapons, invincibility, death/respawn
- [ ] `player_shot.c` — 3 weapons × 6 levels shot objects

### Phase 4 — Enemies
- [ ] `enemy.c` — base struct, list management
- [ ] `enemy_spawn.c` — tilemap-driven spawner
- [ ] `enemy_shot.c` — enemy projectile objects
- [ ] `enemy_a.c` through `enemy_p.c` — 16 AI types
- [ ] `boss_k.c` / `boss_l.c` / `boss_m.c` — 3 boss types

### Phase 5 — Game States
- [ ] `game_state_titles.c` — title/menu screen
- [ ] `game_state_stage.c` — full stage substates
- [ ] `game_state_complete.c` — credits/victory

### Phase 6 — Audio
- [ ] `audio.c` — XAudio direct, music channels 0–2, SFX channel 3, fade

### Phase 7 — Polish
- [ ] Explosions, powerups, screen transitions, score display

---

## Scale / Effort Estimate
| Phase | Complexity | Delegation |
|-------|-----------|------------|
| Phase 0 (assets) | LOW | Haiku (scripts) |
| Phase 1 (scaffold) | LOW-MEDIUM | Gemini (parallel) |
| Phase 2 (rendering) | MEDIUM | Gemini |
| Phase 3 (player) | MEDIUM | Gemini |
| Phase 4 (enemies) | MEDIUM-HIGH | Gemini (parallel batches) |
| Phase 5 (states) | MEDIUM | Gemini |
| Phase 6 (audio) | MEDIUM | Claude direct (reuse purelaxJS) |
| Phase 7 (polish) | LOW | Haiku/Gemini |

Similar scale to purelaxJS port (~3–4 weeks with delegation).
