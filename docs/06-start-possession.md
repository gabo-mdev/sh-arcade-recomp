# Next milestone: Start Game and mask possession

Status: 2026-11-12. Coin and Start reach a controllable first stage. The
mask scene and that stage are **not yet pixel-correct**. See
[05-video-roadmap.md](05-video-roadmap.md) for the measured video progress.
The play rules below are host behavior in `src/machine/machine.cpp`, not a
claim that the frames match MAME.

## Current verified baseline

- The native renderer matches MAME 0.256 video states for the title, intro,
  and demo (0 pixels). The cave state differs by 3 pixels. These measurements
  test the renderer with **MAME state loaded into it**, not the native game's
  execution or timing.
- The native cave and MAME cave have similar VRAM, but different C123 palette
  banks and sprite lists. A visual mismatch there cannot be assigned wholly
  to the renderer.
- Scheduled input is now available in the native executable and MAME Lua
  capture script. In the tested native run, Coin 1 at zero-based frame 2100
  gives one credit, Start 1 at frame 2200 enters the purple mask scene, and
  the game transitions into the first stage around frame 3200.
- With Coin 1 after MAME frame 360 and Start 1 after frame 400, MAME shows
  mask movement around frames 500–1000, contact/possession by 1100–1200, and
  the first stage by 1400. Align by scene/event rather than subtracting a
  fixed boot-frame offset.
- Feeding the MAME frame 500/600/800 video states to the native renderer
  now leaves 69 pixels different on frame 500 (the same order of error on
  600 and 800), down from 563. Frame 1200 leaves 47. The mask and Rick
  silhouettes match; the remaining pixels are interior shade differences.
  MAME stage frames 1400/1600 differ by 106/122 pixels when rendered from
  identical MAME state, down from 824/998. Idle frames 700/1300/2200 are
  0/3/0 under the old odd-column step. That step repeated column 15; the
  decoder now reads the high nibble as the left pixel. Re-compare before
  treating those counts as current. Do not replace the flipped origin.
- Native frame 3400 has visible vertical fragments. Its C123 layer-3 control
  byte is `0x05` (enabled); MAME stage frames 1400 and 1600 use `0x0f`
  (disabled). Native frame 3200 matches MAME controls, and by native frame
  3600 layer 3 is disabled again. Frame 3400 also differs from MAME frame 1600
  in 504 VRAM bytes and 1879 sprite-RAM bytes. These are not equivalent
  complete video states, so trace the first differing control write before
  attributing the fragments to the renderer.

## Reproduce the scene

1. Build with SDL (`.\build.ps1`) and run `.\build\splatterhouse.exe`.
2. At the title, press `5` to insert one coin, then `1` to start Player 1.
   Keyboard mapping is in `src/host/sdl_backends.cpp`; both inputs are
   active-low. Gamepad View inserts a coin and Menu starts.
3. Run the same ROM set in the working MAME 0.256 installation and use its
   mapped Coin 1 / Start 1 controls. Record the exact MAME version and ROM
   set. Do not assume the two programs reach the scene at equal frame counts.
4. For both runs, note whether credit is accepted, Start is accepted, the
   mask appears, moves toward Rick, contacts him, possession finishes, and
   the game enters the first playable state. Mark the **first** incorrect
   moment, not only the final image.

The automated native reproduction is:

```powershell
python tools/capture_frames.py 2200 2250 2600 2900 3000 3100 3200 3400 3600 `
  --coin-frame 2100 --start-frame 2200 --input-hold 4
```

The tool places these captures under
`build/captures/coin_2100_start_2200/`, preserving the idle baseline. The
injection frames and hold length are also recorded in each `status.json`.
`--coin-frame` and `--start-frame` work directly with `splatterhouse.exe
--probe`; frames are zero-based and each input pulse clears an active-low bit.

For MAME, set `SH_COIN_FRAME=360`, `SH_START_FRAME=400`,
`SH_INPUT_HOLD=4`, and `SH_SNAP_FRAMES=400,500,600,800,900,1000,1100,1200,1400,1600`
before running the command in `03-verification.md`. The Lua callback sets
the input after the named frame has completed, so the next frame first reads
the pressed value. Save to a separate `SH_DUMP_DIR` to keep the idle captures.

## Capture and compare in the next implementation session

The input schedules are implemented. Next define `t=0` as the accepted Start
edge (or the first game-state transition after it), then collect dense frames
around each milestone: before Start, mask first visible, movement, contact,
possession, and playable Rick.
Record the actual event frame in each emulator; align by event and video
state, not by the boot frame number.

For the first mismatch, save screenshot plus C123 VRAM/control, C116 palette,
sprite staging bytes 4..9 and active bytes 10..15, global sprite offsets,
and main/sub/MCU PC and input-port values. Run
`tools/compare_mame_video.py` with the **same captured MAME state** to decide
whether the remaining error is in sprite rendering or in native execution.
If MAME state renders correctly offline but native state differs, trace the
earliest input, object, palette-bank, sprite-latch, IRQ, or HLE divergence.
The current boot HLE and coin handling in `src/machine/machine.cpp` deserve
special attention before changing mask artwork or hard-coding coordinates.
For the stage fragments, trace native C123 offset `0x13` and find why it
temporarily selects priority `5` rather than disabled `0x0f`. Capture MAME
more densely around its stage entry before treating that as a permanent
state divergence. For the remaining mask-scene pixels, render Rick and mask as
separate sprite layers and compare source pixels and palette indices.

## Play rules now in the port

These are native-execution fixes. They do not change the sprite nibble
decode, the flipped sprite origin `sx - 150`, or the unflipped scroll origin.

- **Score.** 8-bit `ADD`/`ADC` set the 6809 half-carry the way MAME does,
  including both `cf |= 0x60` steps of `DAA`. The HUD digit path (sub
  `F2AB` on page `0x177`) shows 1500 instead of a corrupted BCD value.
- **Hearts.** LIFE is CUS39 sprites, tile `0x4A`, not fixed-layer text. The
  HUD actor is object `0x0F9D` (stride `0x20`), character `0x11`; its byte 5
  is how many hearts are drawn. The current health is in the `0x40`-stride
  player struct at `$011D + $39 * 0x40`, field `+0x1C`. Field `+0x32` is
  remaining lives: it stays at 3 while hearts fall and becomes 2 after a
  death and respawn. Do not write the HUD actor at `+0x32`: that byte
  belongs to the next object.
- **One hit, one heart.** MAME starts with `$147A = 4`, player `+0x1C = 4`,
  and four HUD hearts. Repeated enemy contact reduces both current-health
  fields 4→3→2→1→0; death follows zero, and respawn restores four hearts.
  The `E00A` HLE skipped the initialization that sets the maximum, so native
  `$147A` had been 0. An earlier host patch watched `+0x32` and repeatedly
  copied its value of 3 into the HUD; it hid damage until a death transition.
  The HLE now seeds the maximum once, restores four hearts at the first
  playable entry, and leaves subsequent damage/respawn to the game code.
  `tests/start_sequence.cpp` checks the first two separate contacts and the
  full death/respawn cycle.
- **Stage select.** SH3 DIP SW3 (bit `0x04`, Off when the bit is set) is the
  arcade switch. See [02-splatterhouse.md](02-splatterhouse.md). Forcing
  `$147E` bit 5 for the heart HUD also creates a movable stage number in the
  mask scene. Clearing it only while the sub PC passed `FB81–FC3F` was too
  brief: the number reappeared on subsequent frames. Bit 5 now stays clear
  throughout the pre-play sequence, then latches on in play state `$2C ==
  0x0A`. At native frame 2700, the two number tiles at VRAM `0x74B9` and
  `0x74BB` are spaces; `tests/start_sequence.cpp` checks this. MAME's observed `$147E` value is
  0 in the compared gameplay frames, while native currently uses `0x22`, so
  this HLE bit remains a separate fidelity issue. On 2026-11-13, setting
  `$1479 = 1` so the attract music updater at `$BA80` will post a song also
  brought the stage-select menu back in play. Do not treat that store as
  music-only, and do not clear it just to hide the menu until the header
  byte and the bit 5 latch are traced together. The headless check that
  VRAM `0x74B9` and `0x74BB` are spaces was written for the earlier latch;
  it does not by itself prove the interactive menu stays closed. The one-hit death was
  reproduced with `$147A = 0` and the host writing the lives value into HUD
  hearts; the corrected natural damage path works without using this bit as
  an invulnerability switch.
- **First-start HUD report.** A player saw three visible hearts on one first
  start, with the first hit leaving three visible, then four on a restart.
  The exact startup timing was not reproduced in headless runs: starts at
  frames 2000, 2200, 2300, 2400, and 2500 (with a prior coin) reached
  gameplay with player health 4 and HUD hearts 4. The port now aligns the
  visible HUD to 4 only while the player health field is still 4; once health
  drops, game damage owns the HUD. The test forces a 4-health/3-HUD mismatch
  and checks its repair, then verifies that two hits reduce both fields.
  This guards the observed symptom; the timing that first creates the
  mismatch remains to be traced in an interactive startup.
- **Checksum pacing.** The 16.5 ms frame cap and SDL vsync turn off only
  while the main PC is in the ROM checksum `C481–C510`. The RAM bit-walk
  stays paced. Outside that window the game runs at 60.606 Hz.

### Reproducing the gameplay health check

Build with `./build.ps1`, then run `ctest --test-dir build
--output-on-failure`. The `start_sequence` test inserts a coin at frame
2100, presses Start at 2200, and holds Right from 3350 for 500 frames. It
checks four hearts before contact, three and two after successive contacts,
zero after later contacts, and four hearts with two lives after respawn.
For a frame-by-frame RAM trace, run `./build/sh_trace_damage.exe 3350 4250`.
The trace prints player health, HUD hearts, remaining lives, `$147A`, and
`$147E`; capture status JSON reports the same fields as `player_health`,
`hud_hearts`, `player_lives`, `max_hearts`, and `control_147e`.

Observed native frames in this deterministic path: 3340 has 4/4 hearts and
3 lives; around 3450, 3530, and 3610 have 3/3, 2/2, and 1/1 hearts; around
3690 has 0/0; by 4230 play resumes with 4/4 hearts and 2 lives. MAME
likewise identifies `+0x1C` as health and `+0x32` as lives. The exact frame
numbers are diagnostic checkpoints for the current timing, not gameplay
rules.

## Acceptance for this milestone

- One coin and Start reliably enter the same sequence in native and MAME.
- The mask and Rick appear in the right order and positions, with no stray
  sprite fragments or incorrect layer occlusion; contact and possessed pose
  occur at comparable event-relative times.
- A chosen state from each stage renders identically when fed to the native
  offline renderer, or each remaining pixel mismatch has a specific recorded
  cause and test.
- The sequence proceeds into a controllable first scene without a freeze or
  forced scene-specific sprite/palette override.

Keep the intro/demo issues visible in `05-video-roadmap.md`; passing this
milestone does not imply that the full attract loop is complete.
