// audio.c - Vortexion unified audio system
// Single XAudio instance: SFX voices + chiptune music channels mixed per buffer.
//
// ARCHITECTURE — flat software buffer, push once per video frame:
//   - Mix SFX + chiptune into s_mix_buf (normal cacheable RAM) each frame
//   - memcpy to DMA buffer, then XAudioProvideSamples — no timer, no conditions
//   - PUSH_FRAMES=800 @ 60fps = 48000 samples/sec = exact hardware rate
//   - DMA_BUF_COUNT=6 silence buffers pre-filled on init (6×13.3ms=80ms headroom)
//   - DPC callback is a no-op; nxdk XAudio DPC does not fire per-buffer
//
// WHY THE OLD CODE WAS CHOPPY:
//   - FRAME_MS=15 → ~62fps game loop, one XAudioProvideSamples call per frame
//   - Each buffer = 1600 samples @ 48000Hz = 33.3ms
//   - Submitted 62 buffers/sec; hardware consumed 30/sec → descriptor ring overflow
//   - Old DPC callback was a no-op → no flow control → queue corruption → glitch
//
// nxdk constraints (read before ANY changes — Xport_Lessons.md §1):
//   - #include <windows.h> required for XAudio types
//   - #include "hal/audio.h" for XAudioInit/XAudioPlay/XAudioProvideSamples
//   - CreateFile/ReadFile for WAV parsing (D:\ paths, not fopen/SDL_LoadWAV)
//   - MmAllocateContiguousMemoryEx for physically-contiguous DMA buffers
//   - SDL audio push model (callback=NULL + SDL_QueueAudio) is broken on nxdk
//   - SDL_GetTicks() always returns 0; GetTickCount() works
//   - DPCs run between CPU instructions on single-core Xbox — volatile int is safe

#include "audio.h"
#include <windows.h>
#include <hal/audio.h>
#include <xboxkrnl/xboxkrnl.h>
#include <SDL.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <stdint.h>

// ---------------------------------------------------------------------------
// Music header data (pre-converted chiptune arrays)
// ---------------------------------------------------------------------------
#include "../assets/music/title.h"
#include "../assets/music/stage_1.h"
#include "../assets/music/stage_3.h"
#include "../assets/music/stage_5.h"
#include "../assets/music/vortex.h"
#include "../assets/music/boss.h"
#include "../assets/music/stage_clear.h"
#include "../assets/music/game_over.h"
#include "../assets/music/game_complete.h"

// ---------------------------------------------------------------------------
// XAudio / DMA constants  (mirrors purelaxJS xlax_sound.c — confirmed working)
// ---------------------------------------------------------------------------
#define AUDIO_FREQ      48000
// PUSH_FRAMES = 48000 / 60fps = 800.  One buffer per video frame at 60fps
// produces exactly 48000 samples/sec — matches hardware consumption rate.
// (Previous value of 1600 at 62fps = 2× oversubmission → queue corruption.)
#define PUSH_FRAMES     800
#define DMA_BUF_COUNT   6                                           // 6 × 13.3ms = 80ms headroom
#define DMA_BUF_BYTES   (PUSH_FRAMES * 2 * (int)sizeof(int16_t))  // 3200 bytes
#define MAXRAM          0x03FFAFFF

// ---------------------------------------------------------------------------
// SFX constants
// ---------------------------------------------------------------------------
#define MAX_SFX_VOICES  16

// ---------------------------------------------------------------------------
// Music constants
// ---------------------------------------------------------------------------
#define MAX_MUSIC_CHANNELS  4
#define SAMPLES_PER_TICK    1600   // 48000Hz / 30Hz tick rate

// ---------------------------------------------------------------------------
// SFX buffer (one per SfxId)
// ---------------------------------------------------------------------------
typedef struct {
    int16_t  *samples;       // stereo interleaved, 48000Hz S16LE
    uint32_t  num_samples;   // stereo pairs
    int       loaded;
} SfxBuffer;

static SfxBuffer s_sfx_buf[SFX_COUNT];

// ---------------------------------------------------------------------------
// SFX voice pool
// ---------------------------------------------------------------------------
typedef struct {
    const int16_t *samples;
    uint32_t       total_samples;   // stereo pairs
    uint32_t       pos;             // current stereo-pair index
    int            active;
    float          gain;
    int            sfx_id;          // SfxId of the currently playing sound
} SfxVoice;

static SfxVoice s_voices[MAX_SFX_VOICES];

// ---------------------------------------------------------------------------
// Music track definitions
// ---------------------------------------------------------------------------
typedef struct {
    const uint8_t *notes[MAX_MUSIC_CHANNELS];
    const uint8_t *vols[MAX_MUSIC_CHANNELS];
    int            n_notes[MAX_MUSIC_CHANNELS];
    int            tone[MAX_MUSIC_CHANNELS];
    int            num_ch;
} MusicTrackDef;

static const MusicTrackDef s_tracks[MUSIC_COUNT] = {
    /* MUSIC_TITLE */ {
        { MUSIC_TITLE_CH0_notes, MUSIC_TITLE_CH1_notes,
          MUSIC_TITLE_CH2_notes, MUSIC_TITLE_CH3_notes },
        { MUSIC_TITLE_CH0_volumes, MUSIC_TITLE_CH1_volumes,
          MUSIC_TITLE_CH2_volumes, MUSIC_TITLE_CH3_volumes },
        { MUSIC_TITLE_CH0_N_NOTES, MUSIC_TITLE_CH1_N_NOTES,
          MUSIC_TITLE_CH2_N_NOTES, MUSIC_TITLE_CH3_N_NOTES },
        { MUSIC_TITLE_CH0_TONE, MUSIC_TITLE_CH1_TONE,
          MUSIC_TITLE_CH2_TONE, MUSIC_TITLE_CH3_TONE },
        NUM_CHANNELS__MUSIC_TITLE
    },
    /* MUSIC_STAGE_1 */ {
        { MUSIC_STAGE_1_CH0_notes, MUSIC_STAGE_1_CH1_notes,
          MUSIC_STAGE_1_CH2_notes, MUSIC_STAGE_1_CH3_notes },
        { MUSIC_STAGE_1_CH0_volumes, MUSIC_STAGE_1_CH1_volumes,
          MUSIC_STAGE_1_CH2_volumes, MUSIC_STAGE_1_CH3_volumes },
        { MUSIC_STAGE_1_CH0_N_NOTES, MUSIC_STAGE_1_CH1_N_NOTES,
          MUSIC_STAGE_1_CH2_N_NOTES, MUSIC_STAGE_1_CH3_N_NOTES },
        { MUSIC_STAGE_1_CH0_TONE, MUSIC_STAGE_1_CH1_TONE,
          MUSIC_STAGE_1_CH2_TONE, MUSIC_STAGE_1_CH3_TONE },
        NUM_CHANNELS__MUSIC_STAGE_1
    },
    /* MUSIC_STAGE_3 */ {
        { MUSIC_STAGE_3_CH0_notes, MUSIC_STAGE_3_CH1_notes,
          MUSIC_STAGE_3_CH2_notes, MUSIC_STAGE_3_CH3_notes },
        { MUSIC_STAGE_3_CH0_volumes, MUSIC_STAGE_3_CH1_volumes,
          MUSIC_STAGE_3_CH2_volumes, MUSIC_STAGE_3_CH3_volumes },
        { MUSIC_STAGE_3_CH0_N_NOTES, MUSIC_STAGE_3_CH1_N_NOTES,
          MUSIC_STAGE_3_CH2_N_NOTES, MUSIC_STAGE_3_CH3_N_NOTES },
        { MUSIC_STAGE_3_CH0_TONE, MUSIC_STAGE_3_CH1_TONE,
          MUSIC_STAGE_3_CH2_TONE, MUSIC_STAGE_3_CH3_TONE },
        NUM_CHANNELS__MUSIC_STAGE_3
    },
    /* MUSIC_STAGE_5 */ {
        { MUSIC_STAGE_5_CH0_notes, MUSIC_STAGE_5_CH1_notes,
          MUSIC_STAGE_5_CH2_notes, MUSIC_STAGE_5_CH3_notes },
        { MUSIC_STAGE_5_CH0_volumes, MUSIC_STAGE_5_CH1_volumes,
          MUSIC_STAGE_5_CH2_volumes, MUSIC_STAGE_5_CH3_volumes },
        { MUSIC_STAGE_5_CH0_N_NOTES, MUSIC_STAGE_5_CH1_N_NOTES,
          MUSIC_STAGE_5_CH2_N_NOTES, MUSIC_STAGE_5_CH3_N_NOTES },
        { MUSIC_STAGE_5_CH0_TONE, MUSIC_STAGE_5_CH1_TONE,
          MUSIC_STAGE_5_CH2_TONE, MUSIC_STAGE_5_CH3_TONE },
        NUM_CHANNELS__MUSIC_STAGE_5
    },
    /* MUSIC_VORTEX */ {
        { MUSIC_VORTEX_CH0_notes, MUSIC_VORTEX_CH1_notes,
          MUSIC_VORTEX_CH2_notes, MUSIC_VORTEX_CH3_notes },
        { MUSIC_VORTEX_CH0_volumes, MUSIC_VORTEX_CH1_volumes,
          MUSIC_VORTEX_CH2_volumes, MUSIC_VORTEX_CH3_volumes },
        { MUSIC_VORTEX_CH0_N_NOTES, MUSIC_VORTEX_CH1_N_NOTES,
          MUSIC_VORTEX_CH2_N_NOTES, MUSIC_VORTEX_CH3_N_NOTES },
        { MUSIC_VORTEX_CH0_TONE, MUSIC_VORTEX_CH1_TONE,
          MUSIC_VORTEX_CH2_TONE, MUSIC_VORTEX_CH3_TONE },
        NUM_CHANNELS__MUSIC_VORTEX
    },
    /* MUSIC_BOSS */ {
        { MUSIC_BOSS_CH0_notes, MUSIC_BOSS_CH1_notes,
          MUSIC_BOSS_CH2_notes, MUSIC_BOSS_CH3_notes },
        { MUSIC_BOSS_CH0_volumes, MUSIC_BOSS_CH1_volumes,
          MUSIC_BOSS_CH2_volumes, MUSIC_BOSS_CH3_volumes },
        { MUSIC_BOSS_CH0_N_NOTES, MUSIC_BOSS_CH1_N_NOTES,
          MUSIC_BOSS_CH2_N_NOTES, MUSIC_BOSS_CH3_N_NOTES },
        { MUSIC_BOSS_CH0_TONE, MUSIC_BOSS_CH1_TONE,
          MUSIC_BOSS_CH2_TONE, MUSIC_BOSS_CH3_TONE },
        NUM_CHANNELS__MUSIC_BOSS
    },
    /* MUSIC_STAGE_CLEAR */ {
        { MUSIC_STAGE_CLEAR_CH0_notes, MUSIC_STAGE_CLEAR_CH1_notes,
          MUSIC_STAGE_CLEAR_CH2_notes, MUSIC_STAGE_CLEAR_CH3_notes },
        { MUSIC_STAGE_CLEAR_CH0_volumes, MUSIC_STAGE_CLEAR_CH1_volumes,
          MUSIC_STAGE_CLEAR_CH2_volumes, MUSIC_STAGE_CLEAR_CH3_volumes },
        { MUSIC_STAGE_CLEAR_CH0_N_NOTES, MUSIC_STAGE_CLEAR_CH1_N_NOTES,
          MUSIC_STAGE_CLEAR_CH2_N_NOTES, MUSIC_STAGE_CLEAR_CH3_N_NOTES },
        { MUSIC_STAGE_CLEAR_CH0_TONE, MUSIC_STAGE_CLEAR_CH1_TONE,
          MUSIC_STAGE_CLEAR_CH2_TONE, MUSIC_STAGE_CLEAR_CH3_TONE },
        NUM_CHANNELS__MUSIC_STAGE_CLEAR
    },
    /* MUSIC_GAME_OVER */ {
        { MUSIC_GAME_OVER_CH0_notes, MUSIC_GAME_OVER_CH1_notes,
          MUSIC_GAME_OVER_CH2_notes, MUSIC_GAME_OVER_CH3_notes },
        { MUSIC_GAME_OVER_CH0_volumes, MUSIC_GAME_OVER_CH1_volumes,
          MUSIC_GAME_OVER_CH2_volumes, MUSIC_GAME_OVER_CH3_volumes },
        { MUSIC_GAME_OVER_CH0_N_NOTES, MUSIC_GAME_OVER_CH1_N_NOTES,
          MUSIC_GAME_OVER_CH2_N_NOTES, MUSIC_GAME_OVER_CH3_N_NOTES },
        { MUSIC_GAME_OVER_CH0_TONE, MUSIC_GAME_OVER_CH1_TONE,
          MUSIC_GAME_OVER_CH2_TONE, MUSIC_GAME_OVER_CH3_TONE },
        NUM_CHANNELS__MUSIC_GAME_OVER
    },
    /* MUSIC_GAME_COMPLETE */ {
        { MUSIC_GAME_COMPLETE_CH0_notes, MUSIC_GAME_COMPLETE_CH1_notes,
          MUSIC_GAME_COMPLETE_CH2_notes, MUSIC_GAME_COMPLETE_CH3_notes },
        { MUSIC_GAME_COMPLETE_CH0_volumes, MUSIC_GAME_COMPLETE_CH1_volumes,
          MUSIC_GAME_COMPLETE_CH2_volumes, MUSIC_GAME_COMPLETE_CH3_volumes },
        { MUSIC_GAME_COMPLETE_CH0_N_NOTES, MUSIC_GAME_COMPLETE_CH1_N_NOTES,
          MUSIC_GAME_COMPLETE_CH2_N_NOTES, MUSIC_GAME_COMPLETE_CH3_N_NOTES },
        { MUSIC_GAME_COMPLETE_CH0_TONE, MUSIC_GAME_COMPLETE_CH1_TONE,
          MUSIC_GAME_COMPLETE_CH2_TONE, MUSIC_GAME_COMPLETE_CH3_TONE },
        NUM_CHANNELS__MUSIC_GAME_COMPLETE
    },
};

// ---------------------------------------------------------------------------
// Music playback state
// ---------------------------------------------------------------------------
static MusicId s_cur_track    = MUSIC_NONE;
static int     s_music_loop   = 0;
static int     s_music_active = 0;
static float   s_music_vol    = 1.0f;

// Per-channel sequencer state
static int     s_note_pos[MAX_MUSIC_CHANNELS];
static int     s_samples_rem[MAX_MUSIC_CHANNELS];
static float   s_phase_acc[MAX_MUSIC_CHANNELS];
static float   s_ch_freq[MAX_MUSIC_CHANNELS];
static int32_t s_ch_amp[MAX_MUSIC_CHANNELS];
static int     s_noise_hold[MAX_MUSIC_CHANNELS];
static int     s_noise_ctr[MAX_MUSIC_CHANNELS];

// ---------------------------------------------------------------------------
// XAudio DMA state
// ---------------------------------------------------------------------------
static unsigned char *s_dma_buf[DMA_BUF_COUNT];
static int            s_dma_next        = 0;
static int            s_audio_ok        = 0;
static DWORD          s_submit_start_ms = 0;
static uint32_t       s_submit_count    = 0;

// Software staging buffer — mix is built here (normal cacheable RAM) then
// memcpy'd to the DMA buffer in one bulk write.  Mixing directly into a
// PAGE_WRITECOMBINE DMA buffer while also reading from it (accumulation) is
// unreliable: WC write buffers can shadow stale reads on x86.
static int16_t s_mix_buf[PUSH_FRAMES * 2];

// ---------------------------------------------------------------------------
// DPC callback — no-op. nxdk's DPC does not fire per-buffer-completion;
// submission is managed entirely by the GetTickCount() timer in audio_update.
// ---------------------------------------------------------------------------
static void xaudio_dpc_callback(void *pac97device, void *data) {
    (void)pac97device;
    (void)data;
}

// ---------------------------------------------------------------------------
// Internal: load one WAV from disc using Win32 RIFF parser (Xport §1.8).
// WAV files are pre-generated at 48000Hz/S16LSB/stereo — no conversion needed.
// Uses CreateFile/ReadFile (windows.h). fopen fails on D:\ paths in nxdk.
// ---------------------------------------------------------------------------
static int load_sfx_wav(SfxBuffer *buf, const char *path) {
    HANDLE hf = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hf == INVALID_HANDLE_VALUE) return 0;

    char tag[4];
    DWORD n;
    ReadFile(hf, tag, 4, &n, NULL);        // "RIFF"
    DWORD riff_size;
    ReadFile(hf, &riff_size, 4, &n, NULL);
    ReadFile(hf, tag, 4, &n, NULL);        // "WAVE"

    while (1) {
        char  chunk_id[4];
        DWORD chunk_size;
        if (!ReadFile(hf, chunk_id, 4, &n, NULL) || n < 4) break;
        if (!ReadFile(hf, &chunk_size, 4, &n, NULL) || n < 4) break;

        if (chunk_id[0]=='d' && chunk_id[1]=='a' &&
            chunk_id[2]=='t' && chunk_id[3]=='a') {
            int16_t *pcm = (int16_t *)malloc(chunk_size);
            if (!pcm) { CloseHandle(hf); return 0; }
            ReadFile(hf, pcm, chunk_size, &n, NULL);
            CloseHandle(hf);
            buf->samples     = pcm;
            buf->num_samples = chunk_size / (2 * sizeof(int16_t));
            buf->loaded      = 1;
            return 1;
        } else {
            DWORD skip = (chunk_size + 1) & ~1U;
            SetFilePointer(hf, (LONG)skip, NULL, FILE_CURRENT);
        }
    }
    CloseHandle(hf);
    return 0;
}

// ---------------------------------------------------------------------------
// Internal: synthesize PUSH_FRAMES stereo samples into mix[] (normal RAM).
// mix[] must be PUSH_FRAMES*2 int16_t elements.  Caller memcpy's to DMA.
// ---------------------------------------------------------------------------
static void fill_buffer(int16_t *mix) {
    memset(mix, 0, DMA_BUF_BYTES);

    // --- SFX voices ---
    for (int vi = 0; vi < MAX_SFX_VOICES; vi++) {
        SfxVoice *v = &s_voices[vi];
        if (!v->active) continue;

        for (int f = 0; f < PUSH_FRAMES; f++) {
            if (v->pos >= v->total_samples) {
                v->active = 0;
                break;
            }
            int16_t sl = v->samples[v->pos * 2    ];
            int16_t sr = v->samples[v->pos * 2 + 1];
            v->pos++;

            int32_t gsl = (int32_t)((float)sl * v->gain);
            int32_t gsr = (int32_t)((float)sr * v->gain);
            int32_t ml = (int32_t)mix[f * 2    ] + gsl;
            int32_t mr = (int32_t)mix[f * 2 + 1] + gsr;
            if (ml >  32767) ml =  32767;
            if (ml < -32768) ml = -32768;
            if (mr >  32767) mr =  32767;
            if (mr < -32768) mr = -32768;
            mix[f * 2    ] = (int16_t)ml;
            mix[f * 2 + 1] = (int16_t)mr;
        }
    }

    // --- Chiptune music channels ---
    if (s_music_active && s_cur_track != MUSIC_NONE) {
        const MusicTrackDef *tr = &s_tracks[s_cur_track];

        for (int f = 0; f < PUSH_FRAMES; f++) {
            int32_t music_l = 0;
            int32_t music_r = 0;

            for (int ch = 0; ch < tr->num_ch; ch++) {
                if (s_samples_rem[ch] <= 0) {
                    s_note_pos[ch]++;
                    if (s_note_pos[ch] >= tr->n_notes[ch]) {
                        if (s_music_loop) {
                            s_note_pos[ch] = 0;
                        } else {
                            s_note_pos[ch] = tr->n_notes[ch] - 1;
                            s_music_active = 0;
                        }
                    }
                    s_samples_rem[ch] = SAMPLES_PER_TICK;

                    // Cache freq+amp once per note — powf() is ~50-100 cycles on P3.
                    // Notes change at 30Hz (every 1600 samples); compute once, reuse.
                    uint8_t nn = tr->notes[ch][s_note_pos[ch]];
                    uint8_t nv = tr->vols[ch][s_note_pos[ch]];
                    float new_freq;
                    if (nn != 255 && nv != 0)
                        new_freq = 440.0f * powf(2.0f, (float)(nn - 33) / 12.0f);
                    else
                        new_freq = 0.0f;
                    // Only reset oscillator phase when pitch changes.
                    // Resetting on every tick — even same note — causes 30Hz phase
                    // chopping: square wave interrupted every 33ms = audible buzz.
                    if (new_freq != s_ch_freq[ch])
                        s_phase_acc[ch] = 0.0f;
                    s_ch_freq[ch] = new_freq;
                    // Music gain: 40% of Pyxel reference (vol * 585 full, vol * 222 = 38%).
                    // Reduces music to ~25% full scale so SFX remains audible over music.
                    s_ch_amp[ch] = (int32_t)nv * 222;
                }
                s_samples_rem[ch]--;

                if (s_ch_freq[ch] == 0.0f || s_ch_amp[ch] == 0) continue;

                int16_t raw;
                if (tr->tone[ch] == 3) {
                    // Noise: sample-and-hold every 8 samples
                    if (s_noise_ctr[ch] <= 0) {
                        s_noise_hold[ch] = (rand() % 65536) - 32768;
                        s_noise_ctr[ch]  = 8;
                    }
                    s_noise_ctr[ch]--;
                    raw = (int16_t)s_noise_hold[ch];
                } else {
                    // Triangle or square oscillator
                    float phase = s_phase_acc[ch];
                    phase += s_ch_freq[ch] / (float)AUDIO_FREQ;
                    if (phase >= 1.0f) phase -= 1.0f;
                    s_phase_acc[ch] = phase;

                    if (tr->tone[ch] == 0) {
                        // Triangle
                        float tri = phase < 0.5f
                            ? (4.0f * phase - 1.0f)
                            : (3.0f - 4.0f * phase);
                        raw = (int16_t)(tri * 32767.0f);
                    } else {
                        // Square
                        raw = (phase < 0.5f) ? 32767 : -32767;
                    }
                }

                int32_t scaled = ((int32_t)raw * s_ch_amp[ch]) / 32767;
                music_l += scaled;
                music_r += scaled;
            }

            // Apply music volume (Fix 3: linear fade-out)
            if (s_music_vol != 1.0f) {
                music_l = (int32_t)((float)music_l * s_music_vol);
                music_r = (int32_t)((float)music_r * s_music_vol);
            }

            // No /4 normalization — Pyxel gain (×0.125) keeps max sum at 16380,
            // well within 32767. Dividing would make it 2× too quiet.
            int32_t final_l = (int32_t)mix[f * 2    ] + music_l;
            int32_t final_r = (int32_t)mix[f * 2 + 1] + music_r;
            if (final_l >  32767) final_l =  32767;
            if (final_l < -32768) final_l = -32768;
            if (final_r >  32767) final_r =  32767;
            if (final_r < -32768) final_r = -32768;
            mix[f * 2    ] = (int16_t)final_l;
            mix[f * 2 + 1] = (int16_t)final_r;
        }
    }
}

// ---------------------------------------------------------------------------
// audio_init
// ---------------------------------------------------------------------------
void audio_init(void) {
    memset(s_sfx_buf, 0, sizeof(s_sfx_buf));
    memset(s_voices,  0, sizeof(s_voices));
    memset(s_dma_buf, 0, sizeof(s_dma_buf));
    s_audio_ok  = 0;
    s_dma_next  = 0;
    s_cur_track = MUSIC_NONE;
    s_music_loop   = 0;
    s_music_active = 0;

    // Allocate physically-contiguous DMA buffers (Xport §1.1)
    for (int i = 0; i < DMA_BUF_COUNT; i++) {
        s_dma_buf[i] = MmAllocateContiguousMemoryEx(
            DMA_BUF_BYTES, 0, MAXRAM, 0,
            PAGE_READWRITE | PAGE_WRITECOMBINE);
        if (!s_dma_buf[i]) {
            for (int j = 0; j < i; j++) {
                MmFreeContiguousMemory(s_dma_buf[j]);
                s_dma_buf[j] = NULL;
            }
            return;
        }
        memset(s_dma_buf[i], 0, DMA_BUF_BYTES);
    }

    // Init XAudio with DPC callback — callback fires once per consumed buffer
    XAudioInit(16, 2, xaudio_dpc_callback, NULL);

    // Pre-fill ALL DMA slots with silence and submit — same as purelaxJS.
    // Gives DMA_BUF_COUNT × 13.3ms = 80ms of startup headroom.
    for (int i = 0; i < DMA_BUF_COUNT; i++) {
        XAudioProvideSamples(s_dma_buf[i], DMA_BUF_BYTES, FALSE);
    }
    s_dma_next        = 0;
    s_submit_start_ms = GetTickCount();
    s_submit_count    = DMA_BUF_COUNT;  // account for pre-filled silence buffers

    XAudioPlay();
    s_audio_ok = 1;

    // Load SFX WAV files from disc (Win32 RIFF parser — SDL_LoadWAV broken with
    // SDL_INIT_AUDIO unset; CreateFile works on D:\ paths per Xport §1.8)
    static const char *SFX_PATHS[SFX_COUNT] = {
        "D:\\sounds\\blip.wav",
        "D:\\sounds\\explode_small.wav",
        "D:\\sounds\\weapon_powerup.wav",
        "D:\\sounds\\life_powerup.wav",
        "D:\\sounds\\bomb_powerup.wav",
    };

    for (int i = 0; i < SFX_COUNT; i++) {
        load_sfx_wav(&s_sfx_buf[i], SFX_PATHS[i]);
    }
}

// ---------------------------------------------------------------------------
// SFX gain levels (file-scope for visibility in sfx_play)
// ---------------------------------------------------------------------------
static const float s_sfx_gain[SFX_COUNT] = {
    1.0f,   /* SFX_BLIP          — reference */
    0.40f,  /* SFX_EXPLODE_SMALL — reduced: prevents buzz from 12 simultaneous death explosions */
    0.97f,  /* SFX_WEAPON_POWERUP */
    0.96f,  /* SFX_LIFE_POWERUP  */
    0.63f,  /* SFX_BOMB_POWERUP  — was substantially louder, normalize down */
};

// ---------------------------------------------------------------------------
// sfx_play
// ---------------------------------------------------------------------------
void sfx_play(SfxId id) {
    if (!s_audio_ok) return;
    if (id < 0 || id >= SFX_COUNT) return;
    if (!s_sfx_buf[id].loaded) return;

    /* Explosion limiter: max 2 concurrent explosion voices to prevent buzz */
    if (id == SFX_EXPLODE_SMALL) {
        int count = 0, oldest_idx = -1;
        uint32_t oldest_pos = 0;
        for (int i = 0; i < MAX_SFX_VOICES; i++) {
            if (s_voices[i].active && s_voices[i].sfx_id == (int)SFX_EXPLODE_SMALL) {
                count++;
                if (s_voices[i].pos > oldest_pos) {
                    oldest_pos = s_voices[i].pos;
                    oldest_idx = i;
                }
            }
        }
        if (count >= 2 && oldest_idx >= 0) {
            s_voices[oldest_idx].pos = 0;  /* restart furthest-along voice */
            return;
        }
    }

    /* Standard voice allocation */
    for (int i = 0; i < MAX_SFX_VOICES; i++) {
        if (!s_voices[i].active) {
            s_voices[i].samples       = s_sfx_buf[id].samples;
            s_voices[i].total_samples = s_sfx_buf[id].num_samples;
            s_voices[i].pos           = 0;
            s_voices[i].active        = 1;
            s_voices[i].sfx_id        = (int)id;
            s_voices[i].gain          = s_sfx_gain[id];
            break;
        }
    }
}

// ---------------------------------------------------------------------------
// music_play
// ---------------------------------------------------------------------------
void music_play(MusicId id, int loop) {
    if (id < 0 || id >= MUSIC_COUNT) return;
    s_cur_track    = id;
    s_music_loop   = loop;
    s_music_active = 1;
    s_music_vol    = 1.0f;

    const MusicTrackDef *tr = &s_tracks[id];
    for (int ch = 0; ch < tr->num_ch; ch++) {
        s_note_pos[ch]    = 0;
        s_samples_rem[ch] = SAMPLES_PER_TICK;
        s_phase_acc[ch]   = 0.0f;
        s_noise_hold[ch]  = 0;
        s_noise_ctr[ch]   = 0;
        uint8_t n0 = tr->notes[ch][0];
        uint8_t v0 = tr->vols[ch][0];
        if (n0 != 255 && v0 != 0)
            s_ch_freq[ch] = 440.0f * powf(2.0f, (float)(n0 - 33) / 12.0f);
        else
            s_ch_freq[ch] = 0.0f;
        s_ch_amp[ch] = (int32_t)v0 * 222;   // match fill_buffer value (-5% from 234)
    }
}

// ---------------------------------------------------------------------------
// music_stop
// ---------------------------------------------------------------------------
void music_stop(void) {
    s_cur_track    = MUSIC_NONE;
    s_music_active = 0;
}

/* Returns 1 if a music track is currently active (playing or looping),
 * 0 if stopped, paused, or finished. */
int music_is_playing(void) {
    return s_music_active;
}

/* Temporarily silence music without clearing track/position state.
 * Call music_resume() to continue from the same position. */
void music_pause(void) {
    s_music_active = 0;
}

/* Resume a paused music track from the same position.
 * No-op if no track is loaded. */
void music_resume(void) {
    if (s_cur_track != MUSIC_NONE) {
        s_music_active = 1;
        s_music_vol    = 1.0f;
    }
}

/* Set music volume (0.0 to 1.0). */
void music_set_volume(float vol) {
    if (vol < 0.0f) vol = 0.0f;
    if (vol > 1.0f) vol = 1.0f;
    s_music_vol = vol;
}

// ---------------------------------------------------------------------------
// audio_update — call once per video frame from the main game loop.
//
// Time-based submission with flat staging buffer:
//   1. Mix SFX + chiptune into s_mix_buf (normal cacheable RAM)
//   2. memcpy to DMA buffer — bulk write to PAGE_WRITECOMBINE memory
//   3. Submit ONLY when hardware has consumed a buffer (time-tracked)
//
// WHY NOT PER-FRAME: game at ~66fps (FRAME_MS=15) pushes 66 bufs/sec,
// hardware consumes 60/sec → dma_buf[0] gets overwritten while hardware
// is still reading it after ~3 sec → corruption. Time-based avoids this.
//
// Stays DMA_BUF_COUNT/2 buffers ahead of hardware consumption position.
// Pre-filled silence (DMA_BUF_COUNT slots at init) gives startup headroom.
// ---------------------------------------------------------------------------
void audio_update(void) {
    if (!s_audio_ok) return;

    DWORD elapsed = GetTickCount() - s_submit_start_ms;
    // How many PUSH_FRAMES-sized buffers hardware has consumed since init
    uint32_t hw_consumed = (uint32_t)((uint64_t)elapsed * AUDIO_FREQ
                                      / (1000u * PUSH_FRAMES));
    // Target: stay DMA_BUF_COUNT/2 buffers ahead of hardware
    uint32_t target = hw_consumed + (DMA_BUF_COUNT / 2);

    int submitted = 0;
    while (s_submit_count < target && submitted < DMA_BUF_COUNT) {
        fill_buffer(s_mix_buf);
        memcpy(s_dma_buf[s_dma_next], s_mix_buf, DMA_BUF_BYTES);
        XAudioProvideSamples(s_dma_buf[s_dma_next], DMA_BUF_BYTES, FALSE);
        s_dma_next = (s_dma_next + 1) % DMA_BUF_COUNT;
        s_submit_count++;
        submitted++;
    }
}

// ---------------------------------------------------------------------------
// audio_shutdown
// ---------------------------------------------------------------------------
void audio_shutdown(void) {
    if (!s_audio_ok) return;

    XAudioInit(16, 2, NULL, NULL);

    for (int i = 0; i < DMA_BUF_COUNT; i++) {
        if (s_dma_buf[i]) {
            MmFreeContiguousMemory(s_dma_buf[i]);
            s_dma_buf[i] = NULL;
        }
    }
    s_audio_ok = 0;

    for (int i = 0; i < SFX_COUNT; i++) {
        if (s_sfx_buf[i].samples) {
            free(s_sfx_buf[i].samples);
            s_sfx_buf[i].samples = NULL;
        }
        s_sfx_buf[i].loaded = 0;
    }

    s_cur_track    = MUSIC_NONE;
    s_music_active = 0;
}
