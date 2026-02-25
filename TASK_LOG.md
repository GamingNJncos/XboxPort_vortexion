# Vortexion Port — Task Completion Log
> Manager/documentation record. NOT loaded by Claude on session start.
> Source: Python/Pyxel horizontal shmup → nxdk Xbox C port.
> Src ZIP: `srcZIP/vortexion/` | Project: `projects/vortexion/`

---

## Phase 0 — Asset Pipeline (DONE ✅)
- `assets/gfx.rgba` — 256×256 RGBA8 sprite sheet extracted from Pyxel .pyxres
- `assets/maps/map_*.h` — 7 TMX maps as C tile arrays (bg + enemy layers), via `assets/maps/tmx_convert.py`
- `assets/music/*.h` — 9 music tracks as C note arrays (chiptune), via `assets/convert_music.py`
- `assets/sounds/*.wav` — 5 SFX WAV files (blip, explode_small, weapon_powerup, life_powerup, bomb_powerup), via `assets/convert_sfx.py`
- **Note**: Pyxel `save()` requires init() + display — NOT usable headlessly. All asset extraction uses standalone Python scripts.

---

## Phase 1 — Scaffold + Core Systems (DONE ✅ user-verified 2026-02-24)
- `include/const.h` — all game/FB/tile/scroll/score constants
- `include/game_types.h` — Vec2, Rect, all enums, Player/Bullet/Powerup/Explosion, GameCtx
- `include/input.h` + `src/input.c` — single-player, BTN_UP/DOWN/LEFT/RIGHT/SHOOT/PAUSE, deadzone 8000
- `include/sprite.h` + `src/sprite.c` — raw RGBA8 loader (SDL_RWFromFile), RGBA→ARGB32, scale blit
- `src/main.c` — nxdk entry, back_buf[640*480], GetTickCount() frame cap, game_update/draw stubs
- `Makefile` — NXDK_SDL=y, ISO_ASSETS_DIR=assets, LDFLAGS=-lm
- **Build**: XBE 952KB, ISO 2.7MB ✅; boots to black screen (game running, no crash)
- **menu-walker.py**: vortexion config added

---

## Phase 2 — Player + Tilemap (DONE ✅ user-verified 2026-02-24)
- `include/player.h` + `src/player.c` — movement (2px axial/1.414 diagonal), inv blink (skip even frames), shoot delegation, clamp x:[0,240] y:[16,168]
- `include/player_shot.h` + `src/player_shot.c` — 3 weapon patterns, 6 speed levels, UV(lvl*16+1, 16+type*16+1), SIZE=14, bounds removal, MAX_SHOTS=4
- Player spawns at (0,92); sprite src_x=0,src_y=4,16×8; inv_timer=120
- `src/tilemap.c` — scrolling BG, tile drawing

---

## Phase 3 — Rendering Fixes (DONE ✅ user-verified 2026-02-24)
- **Full-screen stretch**: `stretch_to_fill()` — 5/4 nearest-neighbour upscale of 512×384 game area → 640×480 fb. Precomputed `sx_lut[640]`. Fixes black borders AND tilemap left-edge flicker.
- **Double-buffer**: `static uint32_t back_buf[640*480]` → `memcpy(XVideoGetFB(), back_buf, ...)` before `XVideoFlushFB()` every frame.

---

## Phase 4 — Enemy System (DONE ✅ user-verified 2026-02-24)
- `include/enemy.h` + `src/enemy.c` — type-init table (16 types), vtable, init/hit/destroy/shoot/update/draw/collision
- `include/enemy_shot.h` + `src/enemy_shot.c` — 4×4 solid rect shots, delay, OOB removal, player collision
- `include/enemy_types.h` + `src/enemy_types.c` — all 16 per-type AI update functions (A–P)
- `include/enemy_spawn.h` + `src/enemy_spawn.c` — column-scan spawn system, sheet_col→EnemyType[32] table
- **Bug fixed**: `#define ENEMY_H` include guard clashed with `ENEMY_H` enum value → renamed to `VORTEXION_ENEMY_H`
- **Init**: `spawn_last_col = GAME_W / TILE_SIZE - 1 = 31`

---

## Phase 5 — Game States + Title Screen (DONE ✅ user-verified 2026-02-24)
- `src/main.c` — full state machine: STATE_TITLES / STATE_STAGE / STATE_GAME_COMPLETE
- Title screen: VORTEXION logo (FG tilemap), scrolling BG, "GAME START"/"CONTINUE" menu, ship cursor, HI-SCORE HUD
- Stage substates: SPAWNED (30f) / PLAY / PAUSED / DEAD (135f→respawn/game over) / CLEAR (180f→next) / GAME_OVER
- Death model: lives-- + player_respawn or GAME_OVER
- Multi-stage: game_select_map() sets cur_map_* pointers; stages 1–5 → game_complete
- `src/sprite.c` — added `draw_text()` (bitmap font at UV(0,240), 8×8 chars, ASCII 32–95)
- `src/tilemap.c` — added `tilemap_draw_full()` (non-scrolling, for title/complete BG)
- **Title screen input lock**: 60-frame grace period prevents spurious startup press
- **Pipeline**: PASS — 8 captures, demo GIF 84KB

---

## Phase 5 Post-Verification Fixes (DONE ✅ user-verified 2026-02-24)
- **Colkey fix**: `blit_sprite` skips `0xFFFF00FF` (Pyxel magenta transparency key)
- **ARM/LvL HUD**: full top+bottom weapon HUD in `hud_draw_stage()` — top ARM+weapon sprite (u=weapon*16,v=224), bottom ARM+LVL labels + 3 weapon columns (x=56/120/184) each with weapon name+sprite+6 pips
- **Hit flash fix**: `enemy_base_draw` hit-flash samples source sheet pixels — only overwrites visible pixels with white
- **Invincibility cheat**: X+Y+Start toggles `ctx->invincible`. Survives stage transitions. Auto-fires. "I" indicator top-right.
- **Vortex BG loop fix**: `tilemap_draw_bg` wraps col modulo map_cols. Vortex stages wrap scroll_x.
- **Enemy shots vs player**: fixed — `lives--` + `inv_timer=INVINCIBILITY_FRAMES`, keep `active=1`. Player blinks on hit.
- **Boss flies off screen**: entry/fight state machine (`ms.i1`). Boss slides in, locks at fight position, bounces Y, fires at player.
- **BTN_PAUSE** remapped to SDL_CONTROLLER_BUTTON_START. BTN_X/BTN_Y added.

---

## Phase 6 — Audio (DONE ✅ user-verified 2026-02-24)
**Architecture**: Direct XAudio (`hal/audio.h`), flat staging buffer (`s_mix_buf[PUSH_FRAMES*2]` normal RAM → memcpy → DMA)
- `PUSH_FRAMES=800`, `DMA_BUF_COUNT=6`, `DMA_BUF_BYTES=3200`, `AUDIO_FREQ=48000`
- Phase reset only on frequency change — prevents 30Hz phase-chop buzz
- SFX WAVs: all 5 at 48000Hz stereo 16-bit PCM
- Music: chiptune C arrays, 9 tracks, mixed in fill_buffer() each frame
- Music volume: `s_music_vol` (0.0–1.0), set via `music_set_volume()`
- SFX gain table: per-SFX float gain applied per voice
- **nv×222**: note volume multiplied by 222 for ~40% of Pyxel reference level
- **SDL audio entirely bypassed**: removed SDL_INIT_AUDIO; XAudio direct only (SDL push model broken on nxdk)

### P6-FIX (DONE ✅ user-verified 2026-02-25)
- SFX paths corrected to `D:\\sounds\\`
- Blip SFX wiring fixed
- `g_last_stage` continue support
- PAUSED subtitle: "A VIBEBOX PORT / BY GNJ" rainbow animation
- Pause menu J color → cyan (0xFF00FFFFu)

---

## Phase 7 — Accuracy Review + Polish

### P7-ACC — Gemini accuracy review (DONE ✅ user-verified 2026-02-25)
4 Gemini agents reviewed Python vs C port → `accuracy_review/TRIAGE.md` (35 items).
Fixed this phase: Palette ✅, audio_fade ✅, boss_pos ✅, complete_HUD ✅, player_tile_collision (SOLID_TILE_START_ID 705) ✅

### User-verified fixes 2026-02-25 (built in session, all verified)
1. FPS timer 500ms→250ms ✅ (verify #11)
2. FPS toggle A+B+Start, PAUSED only ✅ (verify #8)
3. Invincible X+Y+Start, PAUSED only, reset on titles ✅ (verify #9)
4. Audio normalization: SFX gain table, music nv×222, music_play init bug fixed ✅ (verify #10)
5. Enemy shots sprite-based draw at (u=6,v=102) 4×4 ✅
6. Vortex BG loop all stages — tilemap_draw_bg wraps via col%map_cols; vortex stages use tilemap_draw_vortex_bg with visual_cols=32 wrap ✅
7. GnJ text: G/J at 10×10 matching VIBEBOX, n at 8×8 centered in 10px slot, J at x=387 (main.c line ~687) ✅
8. Explosion WAV: noise synthesis fixed (freq-dependent LFSR), 48kHz stereo — user confirmed WAV correct via afplay ✅

### Tasks that were queued but resolved differently (2026-02-25)
- Task #5 (explosion buzz: limit to 2 concurrent voices) — DELETED, was never accurate, user cancelled
- Task #6 (GnJ font revert to scale=1) — DELETED, overlapping task, already done
- Task #7 (boss fight position): ORIGINALLY proposed slide to BOSS_FIGHT_X=168. See BOSS-POS section below — slide approach was wrong.

### BOSS FIGHT BUG ROOT CAUSE (2026-02-25 — original analysis, kept for reference)
- Boss K at map col=251 (of 256). spawn_x=256 game coords. boss decrements x at SCROLL_X_SPEED=0.5f/frame = same as scroll. Net: boss stays at x=256 permanently.
- At scroll_lock (map_end=1792), boss SHOULD be at world_x(2008)-map_end(1792)=216. But C port was keeping it at 256.
- ORIGINAL proposed fix: slide e->x toward BOSS_FIGHT_X=168 at 1px/frame. Add `#define BOSS_FIGHT_X 168` to const.h. Files: src/enemy_types.c, include/const.h
- **THIS WAS WRONG** — 168 is 48px too far left. Python boss has no slide target; it stops naturally at ~216 when scroll stops.
- ACTUAL fix applied: removed slide block entirely. Boss stops at natural position. See BOSS-POS section.

### D1/D2 — Music fade + boss music trigger (DONE — AWAITING USER VERIFY)
**Problem**: Old code set `music_set_volume(0.0f)` every frame for scroll_x 1784–1792, even after boss music started via `music_play()`. `music_play()` resets s_music_vol=1.0f on first frame but subsequent frames muted it back to 0. Boss music played silently.
**Fix** (`src/main.c`): Wrapped entire fade+trigger block in `if (!boss_music_started)`. Once boss music starts, volume is never touched again.
**Boss music trigger**: position-based (`scroll_x >= BOSS_MUSIC_X = 1784`), matching Python original.

### D3 — Stage clear music sync (DONE — AWAITING USER VERIFY)
**Problem**: Stage clear advanced after exactly 180 frames regardless of whether jingle finished.
**Fix** (`src/audio.c`, `include/audio.h`, `src/main.c`):
- Added `int music_is_playing(void) { return s_music_active; }` to audio.c/audio.h
- Changed transition condition: `substage_timer >= 180 && !music_is_playing()`
- Stage clear jingle (loop=0) plays once; transition waits for natural end.

### BOSS-POS — Boss fight position (DONE ✅ user-verified 2026-02-25)
**Problem**: Previous Haiku agent added `else if (e->x > BOSS_FIGHT_X) { slide to 168 }` to all 3 boss update functions. Python boss has NO slide-to-target — it just stops drifting when `get_scroll_x_speed()` returns 0 at scroll lock. Slide moved boss 48px too far left (~168 vs correct ~216).
**Fix** (`src/enemy_types.c`): Removed `else if` slide block from enemy_k_update, enemy_l_update, enemy_m_update. Bosses now stop at natural position (~x=216) matching Python.
**Rule**: NEVER add slide-to-target for bosses. `BOSS_FIGHT_X` constant exists in const.h but is NOT used in boss update functions.

---

## TRIAGE Items — Remaining (from `accuracy_review/TRIAGE.md`)

### CRITICAL (auto-dispatchable)
- Enemy P: bullet speed 1.5 → 4.0 (`enemy_types.c`)
- Enemy A: spawn marker `sheet_col_to_type[0]` → ENEMY_A (`enemy_spawn.c`)

### MODERATE (21 items)
- [A] Player Death: Add `subtract_life()`, `decrease_all_weapon_levels(2)`, `change_weapon(0)` on death in `main.c` line 440
- [A] Player Spawn: Animate x from 0 to 60 over 30 frames in `main.c` line 416 (add `update_spawned` movement)
- [B] Enemy A AI: Fire two shots simultaneously (Y offsets -10, 6) instead of temporal delay in `enemy_a.c`
- [B] Enemy B AI: Y oscillation from `cosf(lifetime * 0.05) * 1.5` to `sinf(lifetime * PI)` in `enemy_b.c`
- [B] Enemy B Bullet Speed: 1.5 to 2.0 in `enemy_b.c`
- [B] Enemy D AI: Dive trigger from Y-proximity to X-proximity (`x - player.x < 24`) in `enemy_d.c`
- [B] Enemy D AI: Dive movement from acceleration 0.4 to constant speed 2.0 in `enemy_d.c`
- [B] Enemy G AI: Phase 2 X Speed from 1.5 to `scroll + 0.5` in `enemy_g.c`
- [B] Enemy G AI: Y drift direction (initialize `flip_y` correctly) and duration (30 frames only, lifetime 250-280) in `enemy_g.c`
- [B] Enemy H AI: Initial `vel_y` from -5 to 5 (moving down) in `enemy_h.c`
- [B] Enemy N AI: Y drift direction inverted (if y < 96, move down not up) in `enemy_n.c`
- [B] Enemy P AI: Shooting timing from 120 (with 25 delay) to 26, then every 120 frames in `enemy_p.c`
- [B] Powerup Bobbing: Amplitude/speed from `sinf(timer * 0.15f) * 0.4f` to `sin(frame_count * PI) * 1.0` in `powerup.c`
- [C] Enemy A: Bullet Speed from 1.5 to 2.0 in `enemy_a.c`
- [C] Enemy A: First shot at lifetime 21 (not 120), then every 120 frames in `enemy_a.c`
- [C] Enemy A: Y offsets from -8, 8 to -10, 6; remove 20-frame delay on second shot in `enemy_a.c`
- [C] Enemy C: Bullet Speed from 1.5 to 2.0 in `enemy_c.c`
- [C] Enemy D: X Speed adjustment from scroll-only to `scroll + 0.25` in `enemy_d.c`
- [C] Enemy E: Bullet Speed from 1.5 to 2.0 in `enemy_e.c`
- [C] Enemy E: Start position from x=-256 to x=-272 in `enemy_e.c`
- [C] Enemy F: Bullet Speed from 1.5 to 2.0 in `enemy_f.c`
- [C] Enemy O: Bullet Speed from 1.5 to 2.0 in `enemy_o.c`
- [C] Enemy O: Shooting timing from 120 (with 40 delay) to 41, then every 120 frames in `enemy_o.c`
- [D] Game States: Add scroll-based music fade-out (1664-1784) in `game_state_stage.c` — DONE via D1/D2

### MINOR (4 items)
- [A] Input: Pause button already approved as BTN_PAUSE (START) — no change needed
- [B] Boss Hit Flash: White (0xffffff) to Peach (0xFFFFCCAAu) in `enemy_types.c`
- [D] Title Screen: Input lock on entry (add 60-frame lock on STATE_TITLE entry)
- [D] Title Screen: Version text from "v1.0" to "V1.0" (capitalize V)

### MISSING (3 items)
- [B] Enemy Shot: Background tile collision check (`collide_background` center-pixel check in `enemy_shot.py`) — implement in `enemy_shot.c`
- [D] UI: Game Complete screen HUD drawing (top/bottom bars, score/stats) — verified correct as-is, no action needed
- [D] UI: Stage clear music synchronization — DONE via D3

### NEEDS USER DECISION (resolved)
- Music fade 1664–1784: DONE (D1/D2)
- Boss music trigger: DONE (position-based)
- Stage clear sync: DONE (D3)
- Game Complete HUD: verified correct as-is

---

## Key Architecture Facts
- Game coords: 256×192 at scale=2 → 512×384, OFFSET_X=64 OFFSET_Y=48, stretch_to_fill → 640×480
- Framebuffer: ARGB32, stride=640. Render to back_buf, memcpy→XVideoGetFB() before XVideoFlushFB()
- Timing: GetTickCount() frame cap. FRAME_MS=16. SDL_GetTicks()=0 always on nxdk.
- File I/O: SDL_RWFromFile("D:\\assets\\gfx.rgba") — pdclib fopen fails for D:\ paths
- Scroll: SCROLL_SPEED_ACCUM=1, SCROLL_SPEED_THRESH=2 → scroll_x++ every 2 frames (0.5px/frame)
- Boss spawn: col*TILE_SIZE - scroll_x at spawn frame. Stage 1 boss K spawns at x=256, natural stop ~216.
- Stage map columns: Stage 1=256, map_end=1792, BOSS_MUSIC_X=1784

## SPRITE_COLOR_MAP (verified 2026-02-25 by sampling gfx.rgba)
Sprite sheet uses CUSTOM RGB values, NOT standard Pyxel 16-color palette.
| Sprite | UV | Actual ARGB32 |
|--------|----|---------------|
| Boss K colour | 160,80 | `0xFFE6CE80` |
| Boss L colour | 176,80 | `0xFF5EDC78` |
| Boss M colour | 192,80 | `0xFFFF7978` |
| Enemy Shot pal15 | 6,102 | `0xFFFFFFFF` |
| Enemy A colour | 0,80 | `0xFF42EBF5` |
