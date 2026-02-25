# XboxPort: Vortexion
Vortexion is a MSX/SG-1000 inspired shoot-em-up written in Python and uses the Pyxel game engine.
I wanted to try porting a python game, here we are. Enjoy.


> Xbox homebrew port built with [nxdk](https://github.com/XboxDev/nxdk).

## Proof

![](/1st.gif?raw=true "")

## Source Game

| Field | Value |
|-------|-------|
| Original title | Vortexion |
| Original platform | [Pyxel](https://github.com/kitao/pyxel) |
| Original source |  [helpcomputer](https://github.com/helpcomputer/vortexion)|
| Port status | BOOTABLE/Playable |
| Build date | 2026-02-25 |
| XBE size | 1.1M |

---
## Known Issues
Some in game colors did not port well.
Edge collision (top and bottom of screen) may behave slightly differntly
Audio required quite a bit of tinkering to play without major audio clipping

It's close enough for me but I'm pubishing the modified source if anyone else would like to go for a PURE ACCURACY port. 

## Download

Latest XBE: [`XBE/vortexion.xbe`](./XBE/vortexion.xbe)

Additional XBE variants *may be* available in the [`XBE/`](./XBE/) folder.

Copy to your Xbox: `E:\Games\vortexion\default.xbe`
Bootable ISO: If you want to wrap to make this bootable in an iso make sure to rename to default.xbe then 
 `extract-xiso -c /tmp/<tmpdir> lastFiles/<project>.iso`
 
---

# Xbox Port — Added Features

Features and outright sins committed during the Xbox port that are not present in the original Python/Pyxel source game.

## Controls

| Action | Button | Notes |
|--------|--------|-------|
| Move | Left stick / D-pad | 8-directional, deadzone 8000 |
| Shoot | A | — |
| Pause / Unpause | Start | — |

## Debug / Development Features
Note these features are enabled and disabled exclusively from the pause menu. 
You *MUST* Start/PAUSE, KEYCOMBO (Confirm visual Indicator), Start/PAUSE.

| Feature | Button Combo | Notes |
|---------|-------------|-------|
| FPS Counter | Press and hold A+B *while held* Press Start (PAUSED only) | **Displays BOTTOM RIGHT** Green≥50fps Yellow≥40 Blue≥30 Red<30 |
| Invincibility + Auto Fire | Press and hold X+Y *while held* Press Start (PAUSED only) | Toggle. Auto-fires. **"I" indicator top-right**. Resets on title screen. |

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
- Game design and art by [badcomputer](https://twitter.com/badcomputer0)
- Music generator [frenchbread1222](https://github.com/shiromofufactory/8bit-bgm-generator)
- Font by [Damien Guard](https://damieng.com/)


Xbox port  [GamingNJncos](https://github.com/GamingNJncos) |

## License
[MIT license](http://en.wikipedia.org/wiki/MIT_License)
