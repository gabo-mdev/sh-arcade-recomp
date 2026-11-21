# Splatterhouse (Arcade) — native PC port

Dedicated Namco System 1 runtime for **Splatterhouse (1988)**.
Bring your own MAME romset (`splatter.zip`). No game code or assets are shipped.

This is **not** the Xbox 360 (2010) recomp. It targets the arcade board.

## Player quick start (Windows x64)

1. Grab a release zip (or build from source below).
2. Put your MAME romset next to `splatterhouse.exe` as `splatter.zip`
   (World / SH3 preferred; `splatter2.zip` and `splatterj.zip` also work).
3. Run `splatterhouse.exe`.

On first launch the exe extracts and maps the romset into a local `rom/`
folder. Later launches reuse that folder.

You can also pass a path:

```text
splatterhouse.exe path\to\splatter.zip
splatterhouse.exe path\to\extracted-mame-folder
splatterhouse.exe path\to\prepared\rom
splatterhouse.exe --prepare path\to\splatter.zip
```

Escape opens the options menu (language: English default, Spanish available).
F1 opens the debug panel when enabled in options.

## Status

Hybrid plan in progress:

1. Dedicated NS1 runtime (interpreters) — playable path
2. MAME oracle verification (traces / frames / audio)
3. Static recompilation of 6809 code → C
4. Later: presentation options + OST replacement hooks

Coin and Start reach a controllable first stage. Audio bring-up continues;
see [audio SFX progress](docs/07-audio-sfx.md) and
[video roadmap](docs/05-video-roadmap.md).

## Requirements (players)

- Windows 10/11 x64
- Your own MAME Splatterhouse romset (`splatter` / `splatter2` / `splatterj`)

## Requirements (developers)

- Windows 10/11 x64
- CMake 3.25+, Ninja, Clang or MSVC (C++20)
- Python 3.10+ (optional; only for the legacy `tools/extract_rom.py` helper)
- SDL3 (fetched via CMake `FetchContent`)
- Optional: MAME ~0.287+ for oracle traces

## Build from source

```powershell
# Optional: pre-build rom/ with the Python helper (the exe can also do this)
python tools/extract_rom.py splatter.zip

# Build (sets up MSVC INCLUDE/LIB + Ninja; SDL window ON by default)
.\build.ps1

.\build\splatterhouse.exe
.\build\splatterhouse.exe --headless --frames 5
```

If you call `cmake`/`ninja` by hand, use an **x64 Native Tools / Developer PowerShell for VS**
prompt. A normal PowerShell lacks MSVC include paths (`cstdint` not found).

### Recomp / analysis scaffolding

```powershell
python tools/analyze.py      # -> analysis/static.json
python tools/transpile.py    # -> src/gen/main_recomp.cpp (gitignored)
```

## Launcher notes

- **Quiver Launcher:** publish a GitHub Release named like
  `Splatterhouse-v0.1.0-Windows-x64.zip` with `splatterhouse.exe` and
  `recomplauncher.json` (no ROMs). Submit the repo at
  [quiverlauncher.com/submit](https://quiverlauncher.com/submit).
- **RecompLauncher:** Level 1 setup ships as [`recomplauncher.json`](recomplauncher.json).
  Players drop a MAME folder (with `sh1_prg0.bin`); the launcher runs
  `splatterhouse.exe --prepare` once, then starts from `rom/`.

## Layout

```
rom/          # gitignored — prepared romset (auto-built next to the exe)
docs/         # hardware notes and verification
tools/        # extract, analyze, transpile, MAME Lua, diffs
src/
  cpu/        # MC6809E + HD63701
  bus/        # CUS117 + virtual address decode
  video/      # tilemaps / sprites / palette
  audio/      # YM2151 + Namco WSG + DAC
  host/       # present + audio backends (SDL / null)
  machine/    # runtime + ROM prepare/load
  gen/        # generated recomp output (gitignored)
tests/
recomplauncher.json
```

## Legal

Splatterhouse © Namco. This repository contains **no** ROM data and does not
redistribute generated game code. Use only a romset you are entitled to use.

Runtime, tools, and docs: MIT — see [LICENSE](LICENSE).
