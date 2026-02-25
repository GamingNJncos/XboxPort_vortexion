// audio.h - Vortexion unified audio system
// Single XAudio instance: SFX voices + chiptune music channels mixed per frame.
// XAudio init pattern identical to purelaxJS xlax_sound.c.

#pragma once
#include <stdint.h>

// ---------------------------------------------------------------------------
// SFX IDs
// ---------------------------------------------------------------------------
typedef enum {
    SFX_BLIP = 0,
    SFX_EXPLODE_SMALL,
    SFX_WEAPON_POWERUP,
    SFX_LIFE_POWERUP,
    SFX_BOMB_POWERUP,
    SFX_COUNT
} SfxId;

// ---------------------------------------------------------------------------
// Music IDs
// ---------------------------------------------------------------------------
typedef enum {
    MUSIC_NONE          = -1,
    MUSIC_TITLE         =  0,
    MUSIC_STAGE_1       =  1,
    MUSIC_STAGE_3       =  2,
    MUSIC_STAGE_5       =  3,
    MUSIC_VORTEX        =  4,
    MUSIC_BOSS          =  5,
    MUSIC_STAGE_CLEAR   =  6,
    MUSIC_GAME_OVER     =  7,
    MUSIC_GAME_COMPLETE =  8,
    MUSIC_COUNT         =  9
} MusicId;

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

// Call once at startup — allocates DMA buffers, inits XAudio, loads WAVs.
void audio_init(void);

// Call once per video frame from the main loop — mixes and submits DMA buffer.
void audio_update(void);

// Call at shutdown.
void audio_shutdown(void);

// Trigger a one-shot SFX (finds a free voice; silently drops if all busy).
void sfx_play(SfxId id);

// Start a music track. loop=1 → loop forever, loop=0 → play once then stop.
void music_play(MusicId id, int loop);

// Stop music immediately.
void music_stop(void);

// Temporarily silence music (preserves track position for resume).
void music_pause(void);

// Resume a paused music track from where it was paused.
void music_resume(void);

// Set music volume (0.0 to 1.0).
void music_set_volume(float vol);

// Returns 1 if a music track is currently active (playing or looping),
// 0 if stopped, paused, or finished.
int  music_is_playing(void);
