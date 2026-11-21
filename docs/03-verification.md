# Verification against MAME

For the remaining intro/demo video investigation, follow
[05-video-roadmap.md](05-video-roadmap.md).

Pinned reference: **MAME 0.287+** (record the exact version in `trace/mame_version.txt`).

## Workflow

```powershell
# Coverage (which PCs run)
mame splatter -rompath rom -debug -debugger none -autoboot_delay 0 `
  -autoboot_script tools/mame/trace_cov.lua -seconds_to_run 30 `
  -video none -sound none -nothrottle -skip_gameinfo
python tools/cov_extract.py

# Instruction traces (main CPU)
mame splatter -rompath rom -debug -debugger none -autoboot_delay 0 `
  -autoboot_script tools/mame/trace_main.lua -seconds_to_run 5 `
  -video none -sound none -nothrottle -skip_gameinfo
.\build\splatterhouse.exe rom --trace-main 200000 > trace/ours_main.log
python tools/difftrace.py trace/mame_main.log trace/ours_main.log

# Frame snapshot
python tools/compare_frame.py trace/frame.raw ref/frames/0000.png

# Reproducible local captures (one folder per frame count)
python tools/capture_frames.py 2200 3000 4000 6000

# Audio RMS
python tools/compare_audio.py trace/ours.wav trace/mame.wav
```

## Pass criteria (Phase 2)

- Attract-loop window: PC / A / B / X / Y / U / S / CC / cycles match MAME
- Selected frames pixel-identical at the same frame index
- Audio RMS error &lt; 1% over a fixed window

Each `build/captures/frame_NNNNN/` folder contains the original-size PNG,
scaled preview, VRAM, sprite RAM, C123 control bytes, C116 RGB palette RAM,
and `status.json` with CPU state and sprite latch count. Compare the same game
state in MAME before interpreting pixel differences; the current boot uses HLE
and the same numeric frame can represent a different scene.

## Local MAME reference

`D:\LaunchBox\Emulators\MAME 0.256\mame.exe` runs this ROM set and passed
`-verifyroms splatter` during the November 2026 comparison. The other local
`D:\LaunchBox\Emulators\mame\Mame32.exe` failed at startup with a Windows DLL
reallocation error. MAME 0.256 is useful for video state and screenshots, but
keep the pinned 0.287+ version for eventual cycle and pixel acceptance.

```powershell
$env:SH_SNAP_FRAMES = '300,700,1300,2200'
$env:SH_DUMP_DIR = 'F:\projects\splatterrecomp\build\mame_ref\state'
New-Item -ItemType Directory -Force $env:SH_DUMP_DIR | Out-Null
& 'D:\LaunchBox\Emulators\MAME 0.256\mame.exe' splatter `
  -noreadconfig -rompath 'F:\projects\splatterrecomp' `
  -autoboot_delay 0 -autoboot_script 'F:\projects\splatterrecomp\tools\mame\snap_frame.lua' `
  -seconds_to_run 40 -snapshot_directory $env:SH_DUMP_DIR `
  -cfg_directory 'F:\projects\splatterrecomp\build\mame_ref\cfg' `
  -nvram_directory 'F:\projects\splatterrecomp\build\mame_ref\nvram' `
  -video none -sound none -nothrottle -skip_gameinfo
```

The Lua script saves a screenshot for each requested frame and the corresponding
C123 VRAM/control, C116 palette window, and buffered sprite RAM. MAME also
saves one extra screenshot when it exits; use the printed `snapshot frame` lines
to match the requested screenshots in order. In the current boot, MAME frame
300 and native frame 2200 have the same C123 control bytes and only 42 differing
VRAM bytes. MAME frame 1300 and native frame 3000 show the same cave scene,
but the native scene uses palette bank 2 for scrolling layers 0–2 while MAME
uses bank 1. That remaining color difference is in emulated state, so changing
RGB conversion would hide the cause.

## Gotchas

- Quantum between CPUs (MAME uses ~38400 Hz) — keep slice small
- PRG7 bit-16 swap must match before comparing vectors
- Keycus 181 must return id on op==3
- Video RAM contention deadlocks if main/sub run too far apart
