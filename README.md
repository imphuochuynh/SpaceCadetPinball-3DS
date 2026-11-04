# Space Cadet Pinball for 3DS

A fork of [MaikelChan's 3DS port](https://github.com/MaikelChan/SpaceCadetPinball/tree/3ds) of *3D Pinball – Space Cadet*, the table that shipped with Windows 95 through XP. It turns the port into a complete handheld game: a camera that follows the ball, the score panel and high scores on the bottom screen, arcade-style initials, a touch menu, remappable controls, and saved settings, all styled with the game's own dot-matrix font.

It's built on [k4zmu2a's decompilation](https://github.com/k4zmu2a/SpaceCadetPinball) of the original game. Tested on a New 3DS with the Space Cadet table from *Full Tilt! Pinball*.

**Status:** complete. This is a personal project, released as is. It does what I wanted, and I'm not planning further updates or support.

## Features

**Top screen: the table**
- **Follow view** (default): zoomed in about 1.75× and tracking the ball. The camera eases toward a falling ball ahead of time so the flippers are on screen before it arrives, and it always keeps the flippers fully in view near the bottom.
- **Classic view:** the whole table at once.
- **Select** switches between the two views. Your choice is saved.

**Bottom screen: score panel and high scores**
- The game's own score panel (ball count, score and mission messages) at 1:1 pixel size, on a dark background.
- The top 3 high scores beside it, plus the current view and a ☰ menu icon.

**High scores**
- Scores are recorded and saved to the SD card the moment you enter your initials. The original port didn't record high scores at all.
- **Arcade-style initials:** three large letters on the bottom screen, while the table dims on the top screen with "NEW HIGH SCORE". Your last initials are remembered.
- The full top 5 is on the menu's **High Scores** page, along with Reset (which asks for confirmation).

**Menu** (Start, or tap the ☰ corner; the game pauses)
- Resume, New Game, Players 1–4, View, Sound, Music, Controls, Demo, High Scores, Quit.
- Works with touch, or with the D-pad and A/B.
- Changing the player count starts a new game when you resume, like the original.

**Remappable controls**
- Flippers, plunger and the three nudges can go on any button except Start and Select, including ZL/ZR, the C-Stick and the Circle Pad.
- Picking a button that's already in use swaps the two actions.

**Themed with the game's font**
- The menu, high scores, initials entry and "PAUSED" use the table's own dot-matrix message font (the one that shows "Awaiting Deployment"). It's read straight from your game data at startup.

**Everything is saved**
- High scores, view, sound, music, players, controls and last initials are stored in `sdmc:/3ds/SpaceCadetPinball/settings.ini`. The file is written to a temp copy first, then swapped in, so a crash or power-off can't wipe your scores.

**Performance**
- A steady 60 fps on New 3DS:
  - Only the 8×8 tiles of the game screen that changed are copied to the GPU each frame. The original re-sent the whole screen as a 2 MB texture every frame.
  - The Follow camera moves on a critically damped spring, so it stays smooth on long drops.
- A hidden frame meter: open the menu and press **Y**. The footer then shows the average CPU time per frame and a count of dropped frames.

**Other changes**
- Faster plunger. The pull-back now steps once per frame; the original step landed on every second frame on 3DS, which made it sluggish.
- Music for all three Full Tilt tracks.
- Installs as its own HOME Menu title (*Space Cadet Pinball*, title ID `0x5CADE`), so it sits alongside the original port instead of replacing it.

## Controls

| Action | Default button |
|---|---|
| Left / right flipper | L / R |
| Plunger | A (hold, release) |
| Nudge left / right / up | D-Pad Left / Right / Up |
| Menu (pauses) | Start, or tap the ☰ corner |
| Switch view (Follow / Classic) | Select |
| New game (on the game-over screen) | X |

Flippers, plunger and nudges can be remapped in **Menu → Controls**.

**Initials entry:** D-Pad Up/Down changes the letter (hold to scroll). D-Pad Left/Right, or A and B, move between letters. A on the last letter, or Start, confirms.

## Installing

You need your own copy of the game data; it isn't included. The *Full Tilt! Pinball* CD (`CADET` folder) is recommended. The Windows XP version (`Program Files\Windows NT\Pinball`) also works, but the layout was tuned and tested on Full Tilt, and with XP data the themed text falls back to the 3DS system font.

1. Get `SpaceCadetPinball.cia` from the latest build (Actions → most recent run → Artifacts), or build it yourself (see below).
2. Copy it to the SD card and install it with FBI.
3. Make sure `/3ds/dspfirm.cdc` is on the SD card. Homebrew has no sound without it. Run [DSP1](https://github.com/zoogie/DSP1/releases) once if it's missing.
4. Set up the game data:

```
/3ds/SpaceCadetPinball/
  CADET.DAT
  SOUND/
    SOUND*.WAV          (from the CD's CADET/SOUND folder)
    TABA1.ogg TABA2.ogg TABA3.ogg   (music, see below)
```

For the XP version, use `PINBALL.DAT`, its `.WAV` files and `PINBALL.ogg` in `/3ds/SpaceCadetPinball/` instead.

### Music

The 3DS build can't play MIDI, so the music has to be converted to OGG. Full Tilt stores its three tracks as `.MDS` (MIDI streams). `tools/mds2mid.py` converts them to standard MIDI, using a Python port of the decompilation's own converter. Then render and encode them at **22,050 Hz**, the game's audio rate; any other rate costs CPU for resampling:

```bash
python3 tools/mds2mid.py TABA1.MDS TABA1.mid
fluidsynth -ni -F TABA1.wav -r 22050 -g 0.35 FluidR3_GM.sf2 TABA1.mid
ffmpeg -i TABA1.wav -af "silenceremove=stop_periods=1:stop_duration=1:stop_threshold=-60dB,alimiter=limit=0.85:level=false" \
       -ar 22050 -c:a libvorbis -q:a 4 TABA1.ogg
```

Repeat for TABA2 and TABA3. Any General MIDI soundfont works. The silence trim lets each track loop without a gap.

## Building

**GitHub Actions** builds `SpaceCadetPinball.cia` and `.3dsx` on every push (`.github/workflows/build.yml`):
- The code is compiled in the `devkitpro/devkitarm` container.
- The CIA is packaged on the host runner, because current `makerom` releases need a newer glibc than the container has. `.github/record-makerom.sh` records makerom's arguments inside the container so the host can replay them.

**Locally:**
- Install [devkitPro](https://devkitpro.org/wiki/Getting_Started) with `3ds-dev`, `3ds-sdl` and `3ds-sdl_mixer`, then run `make`.
- For a CIA, put [makerom](https://github.com/3DSGuy/Project_CTR/releases) and [bannertool](https://github.com/Epicpkmn11/bannertool/releases) in `$DEVKITPRO/tools/bin` and run `make BUILD_CIA=1`.

## How it's put together

New 3DS-specific code lives in `SpaceCadetPinball/n3ds_*`:

| File | What it does |
|---|---|
| `n3ds_camera` | Follow / Classic views; spring-based ball tracking |
| `n3ds_ui` | Bottom-screen top-3 column and footer |
| `n3ds_menu` | Pause menu, Controls, High Scores and confirmation pages |
| `n3ds_initials` | Arcade initials entry |
| `n3ds_controls` | Remappable buttons |
| `n3ds_font` | Packs the game's message font into a GPU texture; draws text with citro2d |
| `n3ds_dirty`, `n3ds_tiles` | Tracks changed 8×8 tiles; copies them into GPU tile order |
| `n3ds_graphics` | citro3d setup; restores GPU state around citro2d drawing |

**Off-device tools** in `tools/` (not needed to play):

| Tool | Purpose |
|---|---|
| `mds2mid.py` | Full Tilt music → MIDI |
| `datdump.py` | Inspect `.DAT` files: background art, score-box and text-box positions |
| `ui_preview/preview.py` | Render the themed bottom-screen pages with the real game font |
| `camera_preview/` | Run the camera code natively: projected corners, table renders, and a jerk test on a simulated ball drop. Needs citro3d's math sources compiled for your PC. |
| `tile_test/` | Checks that the tile upload is byte-identical to the original port's swizzle |

## Credits

- **Original game:** Cinematronics / Maxis, published by Microsoft.
- **Decompilation:** [k4zmu2a](https://github.com/k4zmu2a/SpaceCadetPinball).
- **3DS port:** [MaikelChan](https://github.com/MaikelChan/SpaceCadetPinball/tree/3ds), with contributions credited in its history.
- **This fork:** [@imphuochuynh](https://github.com/imphuochuynh).

MIT licensed, like the projects it's based on (see `LICENSE`). Game data and music are not included and remain the property of their owners.

## AI disclosure

The changes in this fork were written with **Claude**, Anthropic's AI assistant. That covers the code, build setup, tools and this README. I directed the project: I chose the features, made the design calls, and tested every build on my own New 3DS, reporting what looked or felt wrong until it was right. The upstream port and decompilation it builds on are the work of their authors and were not AI-generated.
