# FH6 ChaseCam

A drift chase camera for Forza Horizon 6 (PC). Low, close and wide, it looks into the corner while you drift and tips the horizon slightly, like the drift showcase clips.

by SPIRE · [drift.ph](https://drift.ph) · spireowll@gmail.com

## How FH6 camera mods work

FH6 has no scripting, but it reads two text files from `mediapc\camera` next to the game exe:

| File | What it holds |
|---|---|
| `CameraSettings.ini` | Height, distance, aim height, FOV and the springs that make each camera follow the car |
| `CameraPhysics.xml` | Effect layers per camera. Each layer reads an input (speed, G-forces, tyre slip, bumps), maps it through a curve and adds it to an output (offset, rotation, FOV) |

This repo generates both files from Playground's originals plus a preset. It edits the original text in place, so every line a preset doesn't change stays byte-for-byte stock.

FH6 ChaseCam replaces **Chase Far**, so stock Chase Near is still one button press away.

## Build

1. Copy `CameraSettings.ini` and `CameraPhysics.xml` from your FH6 install (the game's own, not another mod's) into `stock/`.
2. Run:

```sh
python3 build.py            # all presets
python3 build.py drift      # one preset
```

Output: `dist/<preset>/mediapc/camera/` and `dist/fh6-chasecam-<preset>-v<version>.zip`.

## Presets

Defined in `presets.py`.

| Preset | What it does |
|---|---|
| `drift` | The FH6 ChaseCam look: about 0.7 m lower, 0.5 m closer, +6° FOV, looser rotation follow, corner aim 6°, drift tilt 3°, extra speed FOV |
| `calibrate` | Test build: only big drift effects (corner aim 20°, tilt 15°) on Chase Far, to confirm which way each effect turns in game |

Values in a preset: a number replaces the stock value, `rel(x)` adds to it, `scale(x)` multiplies it.

### Calibrating directions

Install the `calibrate` build, pick Chase Far and hold a **left-hand** drift. The view should turn left (into the corner) and the horizon should tip toward the outside. If either goes the other way, flip `AIM_SIGN` or `TILT_SIGN` in `presets.py` and rebuild.

## Install

- **Steam:** copy the `mediapc` folder into the folder with `forzahorizon6.exe`.
- **Xbox app / Game Pass:** copy it into the game's `Content` folder, for example `D:\XboxGames\Forza Horizon 6\Content`, and let it merge.

Back up any files already in `mediapc\camera` first. To uninstall, delete the two files or restore the backups.

Recommended in game: Motion Blur on Long. Try the built-in Drift camera too (Settings > HUD & Gameplay).

## Test

```sh
python3 tests/test_build.py
```

Runs against a small fixture in the game's format, and against `stock/` when the game files are there.

## Note

Playground does not support mods, and changing game files is at your own risk.
