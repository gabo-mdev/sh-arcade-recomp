# Host backends (presentation + OST hooks)

The core never talks to SDL directly. It uses:

- `IPresentBackend` — framebuffer present, window events, fullscreen
- `IAudioBackend` — float32 stereo queue, YM mute, OST gain/track load
- `IInputBackend` — active-low control ports + coin

## Null backends

Always available. Used for `--headless`, CI, and oracle runs.

## SDL3 backends

Built when CMake `-DSH_HOST_SDL=ON`. Integer scale present. Keyboard defaults
match MAME (arrows / Z / X / 1 / 5). Escape opens the play options menu
(video/audio/controls/language). F1, or right-stick click, opens the debug
panel when `debug_menu` is enabled. The first gamepad uses an Xbox layout:
stick and d-pad move, X/B/RB/RT attack, A/Y/LB/LT jump, Menu starts, View inserts a coin.

## ROM setup

`splatterhouse.exe` accepts a prepared `rom/` folder, a loose MAME ROM folder,
or `splatter.zip` / `splatter2.zip` / `splatterj.zip`. Zip extract on Windows
uses PowerShell `Expand-Archive`. Mapped images go under `rom/` next to the
exe, or under `--data` / `RECOMPLAUNCHER_DATA` when a launcher provides one.

Launcher-oriented flags (opt-in; unchanged without them):

- `--game PATH` — romset zip, MAME folder, or prepared `rom/`
- `--data PATH` — options.cfg + prepared `rom/` live here
- `--prepare` — windowless setup; with `RECOMPLAUNCHER=1` prints
  `progress 0.4 Text` lines for the setup screen

Python `tools/extract_rom.py` remains as a dev helper.

## Phase 4 extension points

1. **Presentation:** swap `SdlPresent` for a backend that letterboxes to widescreen,
   applies shaders, or renders at higher internal resolution once video is exact.
2. **OST:** call `IAudioBackend::set_ym_mute(true)` when a replacement track is
   active; `load_ost_track(path)` streams ogg/flac mixed at `set_ost_gain`.
   Cue detection hooks live next to `AudioSystem::ym_write`.
