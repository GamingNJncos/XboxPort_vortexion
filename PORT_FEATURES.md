# Xbox Port — Added Features

This document describes features added during the Xbox port that are not present
in the original Python/Pyxel source game.

## Controls

| Action | Button | Notes |
|--------|--------|-------|
| Move | Left stick / D-pad | 8-directional, deadzone 8000 |
| Shoot | A | — |
| Pause / Unpause | Start | — |

## Debug / Development Features

| Feature | Button Combo | Notes |
|---------|-------------|-------|
| FPS Counter | A+B+Start (PAUSED only) | Toggle. Green≥50fps Yellow≥40 Blue≥30 Red<30 |
| Invincibility | X+Y+Start (PAUSED only) | Toggle. Auto-fires. "I" indicator top-right. Resets on title screen. |

## Visual / Branding Additions

| Feature | Notes |
|---------|-------|
| Pause screen subtitle | "A VIBEBOX PORT / BY GNJ" with rainbow color animation |
| Full-screen stretch | 512×384 game area upscaled to 640×480 with nearest-neighbour LUT |

## Notes

- Original game: Python/Pyxel horizontal shmup
- Port target: Original Xbox (nxdk, XAudio direct, SDL bypassed for audio)
- Audio: Direct XAudio push model (SDL audio bypassed — SDL push model broken on nxdk)
