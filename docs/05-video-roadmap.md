# Intro/demo video: next debugging steps

**Next focused milestone:** the mask possessing Rick after Start Game. Follow
[06-start-possession.md](06-start-possession.md) for the input sequence,
capture requirements, and acceptance checks. The renderer and attract issues
below remain open; prioritize them when they block that sequence.
Coin/Start injection and captures are now implemented. The sequence reaches
the first stage, but its sprites and a transient stage layer differ from MAME.

Status: 2026-11-12. Matched MAME states for the title, intro, cave, and demo
are now pixel-identical except 3 cave pixels. The mask scene and first stage
still differ. Coin and Start are playable: four hearts, one hit removes one
heart. Start again opens the SH3 stage select after the attract-music
store `$1479 = 1`; that regression is documented in
[06-start-possession.md](06-start-possession.md) and is not to be patched
until the music gate and the menu can be separated. Each sprite byte holds two pixels, and the left pixel is the high nibble.
The old odd-column step read the low nibble first and then skipped 8 bits,
so column 13 copied column 15. That was the line through Rick and the gap
at the end of a punch or kick. Leave the flipped origin `sx - 150` alone.
Do not treat
a passing unit test or a similar-looking screenshot as proof of video fidelity.
The stray stage number in the mask scene was traced to `$147E` bit 5 being
re-enabled before gameplay; it now stays clear through the full possession
sequence. A reported first-start 3-heart display has a guarded HUD sync when
player health is 4, though the original interactive timing is still untraced.

### Exact-state comparison now available

`tools/render_video_state.cpp` loads one MAME video dump and renders it through
the native `Video` implementation without running native CPUs.
`tools/compare_mame_video.py` writes the rendered PNG and difference PNG, then
counts different pixels. Build first, capture MAME using `03-verification.md`,
then run, for example:

```powershell
python tools/compare_mame_video.py 300 build/mame_ref/state/splatter/0000.png
python tools/compare_mame_video.py 700 build/mame_ref/state/splatter/0001.png
python tools/compare_mame_video.py 1300 build/mame_ref/state/splatter/0002.png
python tools/compare_mame_video.py 2200 build/mame_ref/state/splatter/0003.png
```

With the old odd-column step and these MAME 0.256 captures, the differences
were **0 / 0 / 3 / 0 pixels**, respectively, out of 64,512. That step also
repeated column 15 of every sprite. The decoder now takes the high nibble
as the left pixel of each byte. Re-compare frames 700, 1300, and 2200 before
trusting the old counts. Frame 1300 differed by 3 pixels in one column at
about (180, 113) under the old step. `sx - 150` stays the flipped X origin:
`sx - 151` returns
frame 1300 to 260 pixels, and `sx - 152` does not beat `sx - 150` on the
mask scene. The native attract can still disagree with MAME because the boot
HLE does not produce the same sprite list; that is separate from this
renderer result.

## Reproducible baseline

- Native captures: `python tools/capture_frames.py 2200 3000 4000 6000`.
  Each `build/captures/frame_NNNNN/` folder has `frame.png`, raw VRAM,
  buffered sprite RAM, C123 controls, C116 RGB RAM, and `status.json`.
- MAME 0.256 capture command and Lua script: see [03-verification.md](03-verification.md#local-mame-reference).
  Use this installed binary for visual diagnosis; keep MAME 0.287+ as the
  pinned final reference. The older local `Mame32.exe` does not start.
- Compare equivalent **scene state**, not equal frame numbers. Native frame
  2200 is close to MAME frame 300 at the title: C123 controls match exactly
  and VRAM differs in 42 bytes. Native frame 3000 and MAME frame 1300 show
  the cave, with only 32 VRAM bytes different, but scrolling-layer palette
  banks differ at C123 offsets `0x18..0x1a` (native `2`, MAME `1`). Sprite
  list contents also differ, so those two cave screenshots are not a clean
  renderer-only comparison.
- Preserve the current output before each experiment. Capture the same
  scene and record the native frame, MAME frame, control bytes, active sprite
  bytes, ROM identity, MAME version, and a before/after image. The working
  tree already contains unrelated edits; inspect `git status --short` before
  changing or committing overlapping files.

## Ordered work

### 1. Build a same-state renderer comparison (highest priority)

Use the new offline fixture for every renderer change. Its next extension
should render diagnostic images for each of the six tile layers, sprites
alone, and the final priority composite. For the first mismatched pixel, log layer,
screen coordinate, tile/sprite code, source coordinate, mask bit, pen, palette
index, priority, and expected/actual RGB. This distinguishes a decode error
from a placement error without changing game logic.

The current Lua dump reads the C116 window through the C117 virtual space.
Before using all palette pages as an oracle, validate its readback for pens
above `0x0fff` against the actual MAME palette/device. In the cave comparison,
the first `0x1000` native RGB entries matched the readback; the upper pages
did not. That mismatch is not yet explained.

Acceptance: with one identical captured video state, the layer and sprite
images align pixel-for-pixel, or each remaining mismatch has a logged source
coordinate and a specific test case. Do not tune offsets using screenshots
from different game states.

### 2. Verify C123 geometry and orientation

Check the scroll registers and `set_scrolldx` / `set_scrolldy` offsets for all
four scrolling layers, both cabinet flip values, 64x64 and 64x32 maps, wrap
at map edges, and the 288x224 visible window. Check the two fixed 36x28
layers separately. The implementation currently uses a derived flip+ROT180
origin and sets scrolling origin to zero when flip is clear; that latter path
is an unverified assumption. Also check whether the C116 clipping registers
change the visible area during intro transitions. Measure boundaries with
tile-only diagnostic images before editing constants.

Relevant code: `src/video/video.cpp` (`draw_layer_scroll`,
`draw_layer_fixed`, `draw_tilemaps`). Reference:
[MAME 0.256 C123 tilemap](https://github.com/mamedev/mame/blob/mame0256/src/mame/namco/namco_c123tmap.cpp).

Acceptance: corners, layer boundaries, and scroll wrap align on matched
states for title, intro, and demo; a test covers each corrected transform.

### 3. Verify sprite geometry, decode, priority, and buffering

For a matched active sprite list, inspect one visible sprite in every size
(4, 8, 16, 32), flip combination, and clipped edge. Compare its decoded
32x32 ROM tile, `tx/ty` subwindow, X/Y position, global offsets, one-scanline
delay, transparency pen `0x0f`, shadow color `0x7f`, and tile priority mask.
Check overlap order with two real sprites. The existing synthetic tests cover
only a few 16x16 cases and do not prove every format.

Then check **when** bytes 4..9 are copied into 10..15. MAME requests the
copy on a write to sprite offset `0x0ff2` and performs it on the rising
vblank edge. Native `service_vblank()` calls `latch_sprites()` before render;
verify that this samples the same edge relative to CPU writes. Dump both
staging and active lists around a mismatch. Do not substitute staging bytes
for the visible list to improve one screenshot.

Relevant code: `src/video/video.cpp` (`sprite_pen`, `draw_sprites`,
`copy_sprites`) and `src/machine/machine.cpp` (`service_vblank`, `run_frame`).
Reference: [MAME 0.256 System 1 video](https://github.com/mamedev/mame/blob/mame0256/src/mame/namco/namcos1_v.cpp).

Acceptance: matched-state sprite-only image is pixel-identical, including
overlaps and clipping; a one-frame motion test catches a wrong latch edge.

### 4. Trace the cave palette-bank divergence

Log every native write to C123 offsets `0x18..0x1a` with frame, CPU, PC,
old/new value, and nearby game-mode/scene variables. Add equivalent MAME
write tracing for the same scene. Find the **first** different write or
different state before it. The cave comparison has equal RGB data in the
used lower palette pages; forcing bank 1 in the renderer would conceal a
game-state or timing error.

Acceptance: the first divergence is identified and explained by a CPU,
mapping, or timing difference; rerunning the scene selects the same banks
without a renderer override.

### 5. Align progression and reduce boot HLE drift

The native boot uses HLE shortcuts in `src/machine/machine.cpp`, so its frame
number and attract sequence diverge from MAME. Capture C123 control/VRAM
fingerprints every 10–30 frames, then pair equivalent states before comparing
CPU or pixels. Once the visual renderer is validated in isolation, trace the
earliest divergence in main/sub CPU execution, bank mapping, IRQ/vblank, and
HLE state changes. Avoid adding scene-specific sprite or palette patches.

Acceptance: title, intro, cave, Game Over, and demo occur in the same order
with comparable durations; selected matched-state frames are pixel-identical
before making a cycle-exact claim.

## Practical next session

1. Use the deterministic Coin/Start commands in `06-start-possession.md` to
   capture the mask scene and first stage more densely.
2. Isolate Rick and mask sprites in MAME frame 500/600/800 dumps; find the
   source-pixel or palette-index cause of the repeatable 563-pixel difference.
3. Trace the first native C123 offset `0x13` write that enables layer 3 in
   the fragmented first-stage frame; compare nearby MAME frames before fixing.
4. Recheck the full title → mask → stage sequence after each correction.

The current renderer changes improved visible output but are still hypotheses
where no exact-state pixel comparison exists. Keep each correction small and
record its effect on title, intro, and demo captures.
