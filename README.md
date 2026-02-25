# XboxPort: Vortexion

> Xbox homebrew port built with [nxdk](https://github.com/XboxDev/nxdk).

## Source Game

| Field | Value |
|-------|-------|
| Original title | Vortexion |
| Original platform | Unknown |
| Original source | _(add link)_ |
| Port status | BOOTABLE |
| Build date | 2026-02-25 |
| XBE size | 1.1M |

## Download

Latest XBE: [`XBE/vortexion.xbe`](./XBE/vortexion.xbe)

Additional XBE variants available in the [`XBE/`](./XBE/) folder.

Copy to your Xbox: `E:\Games\vortexion\default.xbe`

---

## Proof

| | |
|---|---|
| ![Gameplay](./proof1.gif) | ![Gameplay](./proof2.gif) |

_Replace proof1.gif and proof2.gif with actual gameplay captures._

---

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

---

## Open Tasks

_No open tasks._

Full task history: [TASK_LOG.md](./TASK_LOG.md)

## Building from Source

This port targets the original Xbox and is built with [nxdk](https://github.com/XboxDev/nxdk), the open-source New Xbox Development Kit.

### Prerequisites

1. **nxdk** — [https://github.com/XboxDev/nxdk](https://github.com/XboxDev/nxdk)
   Follow the [Getting Started guide](https://github.com/XboxDev/nxdk/wiki/Getting-Started) for your platform.
2. **LLVM/Clang** cross-compiler (required by nxdk — see nxdk docs)
3. Set the `NXDK_DIR` environment variable to your nxdk install path

### Build

```bash
export NXDK_DIR=/path/to/nxdk
make
```

The compiled XBE will be at `bin/default.xbe`.
Copy to Xbox: `E:\Games\vortexion\default.xbe`

---

## Credits

| Role | Credit |
|------|--------|
| Original game | _(add author / link)_ |
| Xbox port | [GamingNJncos](https://github.com/GamingNJncos) |
