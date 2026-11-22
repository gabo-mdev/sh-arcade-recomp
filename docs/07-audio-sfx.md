# Audio progress and remaining effects

## Chapel Hymn / Poltergeist incomplete YM — fixed 2026-11-20

**Symptom:** OST tracks play with missing instruments / incomplete polyphony.
First seen on **Hymn** (cue `$18`) after Evil Cross. Same family as Poltergeist
(area `$04`, cues `$0F`→`$12`→`$31`→`$2C` when knives / painting phases hit):
MAME Testers [MT04095](https://mametesters.org/view.php?id=4095). This is the
arcade YM2151 score path.

**Cause:** Native asserted main, sub, audiocpu, and MCU IRQs together at end of
frame. MAME `namcos1.cpp` `screen_vblank` asserts main/sub on the **rising**
edge and audiocpu/MCU on the **falling** edge; hap’s note is that
**Splatterhouse 2nd-boss music fails if audiocpu IRQ is simultaneous with
main/sub**. Cue changes mid-boss then program only some YM channels.

**Fix:** Delay the sound-CPU IRQ to the end of CRT blanking.

| When | Who |
|------|-----|
| End of frame (`service_vblank`) | main + sub + MCU (rising edge) |
| ~3840 cycles into the next frame (`service_audio_irq`) | audiocpu only (falling edge) |

3840 ≈ 40 blanking lines × 384 dots at 6.144 MHz, in main-CPU cycles
(49.152/32). An earlier 1536-cycle delay fixed Hymn by ear but was still early
for Poltergeist’s layered cue switches. MCU stays on the rising edge; moving
it with audio broke coin/start probes. Score tempo is the vblank IRQ at
`$D0F2` (sound ROM FIRQ vector is `$FFFF`, unused).

**Files:**

- `src/machine/machine.cpp` — `service_audio_irq`, `kAudioIrqAfter` in `run_frame`
- `src/machine/machine.hpp` — `service_audio_irq()` declaration
- `docs/07-audio-sfx.md` — this section

Earlier note (2026-11-13): the sound CPU was also overclocked (slice excess
discarded). That scheduler carry-over is a separate fix already on the tree;
it did not complete Hymn by itself.

Status: 2026-11-21. Gameplay is playable end-to-end with working board audio.
Bring-up notes below (from 2026-11-13) still describe the signal path and
open listening checks: sound CPU handshake, YM2151, CUS30, 48 kHz frames,
bounded SDL queue, and subframe synthesis are in. Coin, OST, damage voice,
and a confirmed punch sample are known good; kick, death, and some
monster/ambient cues still deserve event-aligned listening passes.

## Current signal path

- The sound 6809 is wired to its banked ROM, YM2151 at `$4000–$4001`,
  CUS30 at `$5000–$53FF` (mirrored through `$5FFF`), shared triram, and
  work RAM in `src/machine/machine.cpp`. The main/sub C117 virtual map also
  routes shared CUS30 RAM through `$2FE000–$2FE3FF`.
- `AudioSystem` renders YM2151 using the vendored BSD-licensed ymfm OPM core.
  It renders packed CUS30 waveform voices and basic noise mode with left/right
  volumes. Each write now flushes audio up to its approximate position within
  the video frame. The scheduler uses 16-main-CPU-cycle slices; exact CPU-cycle
  timestamps and MAME-calibrated CUS30 gain remain pending. MCU DAC writes
  now render at approximate subframe time. The voice ROM is mapped for MCU reads.
- SDL opens a float stereo stream and queues the resulting mix. Backlog during
  accelerated boot drops the newest frame instead of `SDL_ClearAudioStream`
  (a hard clear clicked/crackled under vsync hitch). An empty queue gets one
  silence frame of preroll to absorb brief present stalls. Device buffering
  and frame-end synthesis still add latency. The current `ym_mute_` path zeros
  the entire mix, so it must be moved before any future soundtrack replacement
  that should preserve effects.
- Frame pacing: sleep is the 60.606 Hz master clock. When vsync is also on,
  present already waited a refresh; late frames no longer snap `next_frame` to
  `now` every time (that jitter starved the audio queue on 60 Hz displays).

## First blocker: sound CPU progression

Before this change, native status snapshots in the mask scene and first stage (frames 2400,
2700, 3300, 3460) all reported sound PC `$D00B`. In a MAME 0.256 run with
coin at frame 360 and Start at 400, the sound PC was `$D02C–$D02F` at
frames 300, 500, 1100, 1600, and 1800. The native CPU appears to stay in
its early CUS30 handshake; this is an inference from the PC and ROM bytes,
not yet a write-by-write trace. At MAME frame 300, CUS30 `$5000/$5001`
read `$7C/$DC`; by then its sound CPU had passed that initialization.

At native frame 1855, the boot HLE now writes `$A6` to CUS30 `$000`.
The sound ROM consumes it, initializes the waveform RAM to `$7C/$DC`, and
reaches its normal `$D02D` command loop by frame 1857. The sound PC is not
forced. The HLE timing differs from MAME's natural startup, so compare events
relative to Start rather than absolute boot frame numbers.

At native Start, the sound CPU receives command `$01` and writes YM2151
registers. `sh_capture_audio.exe` produces an 8.25-second Start-sequence WAV
with peak about 0.20 and RMS about 0.03. MAME's same segment has audible
energy in a similar range, but sample-accurate alignment has not been done.
The CUS30 channel registers remain zero in this path. A synthetic one-channel
test verifies that its new renderer outputs a left-only waveform.

Before the MCU timer fix, native gameplay captures from frame 3350–3650 with
and without Right/Attack were byte-identical. The MAME Lua write taps used
in that comparison missed real DAC activity, so that result cannot establish
which chip carries a particular effect. Choose a confirmed contact event and
use debugger watchpoints for the reference trace.

## Coin comparison and current latency

Before the coin fix, the native coin-only capture (`sh_capture_audio.exe
build/audio/coin_native.wav 2050 2200`, scheduled coin at frame 2100) contained
118,800 samples with peak and RMS both zero. `sh_trace_audio.exe` showed no
nonzero CUS30 channel registers around that event. A MAME 0.256 reference with a coin pulse at frame 360
has audible output around the event. Its shared coin byte `$702A` becomes
`$10` at frame 362 and returns to zero by frame 366. At frame 368, CUS30
shared RAM `$5240` changes to `$01`; by frame 370, 17 bytes in the
`$5100–$513F` channel register window are nonzero. The native coin input
produces the same four-frame `$10` pulse but had no `$5240` command. A trial
with the native credit increment disabled still produced no register activity;
that trial was reverted. The coin-credit HLE now writes `$01` to CUS30 `$240`
when it accepts a coin. Native channel registers immediately follow the MAME
sequence (`0F 40 14 ED` in the first channel), and the coin-only WAV now has
peak 0.454 and RMS 0.107 instead of silence. This restores the effect via
the original sound CPU and CUS30, not a canned sample. The native command
currently occurs at the input edge; MAME writes it roughly eight frames after
the edge, so timing and debounce still need comparison.

The SDL stream previously accepted every generated frame without checking
the queued byte count, then later cleared the whole stream when more than
three frames were pending. Clearing made vsync/display hitches audible as
bed crackle. Backlog now skips queueing the newest frame (cap six frames) and
pads silence when the queue is nearly empty. Offline WAV capture still
bypasses SDL.

The Rick/Jennifer attract cue was silent for a different reason. The updater
at main `$BA80` posts the shared triram song only when `$1479` is nonzero or
`$2A == 3`. `$1479` is the C116 header byte 9 copied by the skipped boot at
`$E0B5`. With it left at 0, attract (including modes `$2B = 07` and `09`)
never wrote a command and the DAC stayed idle after the title sting. The HLE
now sets `$1479 = 1`. Native attract then posts song `$23` and keeps streaming
DAC 0 through that intro. Gameplay still posts `$2A` for the mask scene and
`$1F` once play starts.

Playing with that store also brings the stage-select menu back. `$1479` is
not a music-only flag in practice: the same byte is what `$E0B5` copies from
C116 header byte 9, and forcing it on reopened a menu that the `$147E` bit 5
latch had been holding off. Leave the value as it is until the menu and the
attract cue can be separated. See [06-start-possession.md](06-start-possession.md).

Next: identify the main/sub or MCU code path that normally writes `$5240`
and restore it through the boot HLE, replacing the targeted coin command.
For hits and ambient effects, capture one reliably triggered event and compare
the first nonzero frame in native WAV, native registers/DAC writes, and MAME.

## First playable level: November 12 follow-up

The user supplied [this arcade playthrough](https://www.youtube.com/watch?v=F9B0RXnphUI&t=85s)
as a listening and visual reference. The first real level begins at 1:25; the
visible section around 1:27 shows Rick fighting monsters in the dungeon. Do
not align its video timestamp directly with emulator frame numbers, because
the capture includes menus and player timing.

The previous mixer applied all YM2151/CUS30/DAC writes from a frame and then
generated its 792 stereo samples from the final chip state. A normal gameplay
frame contains many YM2151 register writes and key-on changes (for example,
native frame 4432 writes multiple values to YM register `$08`). Short notes
and instrument transitions could therefore disappear. `AudioSystem` now
starts a frame buffer, renders up to the current slice before each chip write,
and finishes the remaining samples after emulation. A synthetic CUS30 test
turns a voice on at sample 120 and off at sample 240; only that interval has
sound. A new native Start capture, `build/audio/start_subframe.wav`, has peak
0.252 and RMS 0.0385. This establishes a timing correction, not yet an
auditory match to MAME or the supplied video.

For a reproducible native damage event, schedule coin frame 2100, Start 2200,
and hold Right from 3350. The first hit lands near frame 3450. MAME 0.256
with coin at 360, Start at 400, and Right from 1550 takes the first hit near
frame 1652. An Attack pulse at native 3440 / MAME 1640 delays the hit by
roughly 59 frames in both. The MAME frame-1650 screenshot confirms the attack
hits the monster: the score changes from 0 to 100 while Rick still has four
hearts.

## Confirmed punch sample correction, 2026-11-13

MAME debugger watchpoints and the confirmed hit show the MCU writes a punch
sample to DAC 0: about 99 bytes per frame, with values from `$01` to `$FE`
around frame 1650. The CUS30 channel registers and YM2151 effect channels
did not start a separate voice for this hit. In the native port before this
correction, DAC 0 stayed at `$80` and shared triram `$2C` ended at `$03`.
MAME set `$2C` to `$83`, the MCU's accepted-command marker.

The MCU ROM routine at `$F4A5` uses BPL to inspect a voice-table entry whose
high bit requests a ROM-bank change. `HD63701::exec_one` incorrectly made
opcodes `$28/$29` test N and `$2A/$2B` test V. These are BVC/BVS and BPL/BMI,
respectively. The wrong BPL branch skipped the bank change, read `$FF` as
voice metadata, and left DAC 0 centred. Correcting all four conditions makes
the native MCU set `$2C=$83` and emit varying DAC 0 bytes from frame 3447.
The new attack WAV differs from the previous build by left-channel RMS 0.124
at frame 3448 and 0.172 at frame 3449, where the old build was silent on
that sample channel. The audible punch still needs a user listening check.

An earlier gameplay voice also aligns with MAME: at MAME frame 1582 and
native frame 3380, shared triram changes to `$2D=0D, $2E=86`. MAME's DAC 1
changes from a constant `$80` to values from `$01` to `$90` in that frame;
the native DAC 1 emits 65 non-centre values in its corresponding frame and
continues streaming afterward. This establishes an active second sample
channel. The event has not yet been identified visually or checked by ear
as a specific monster/environment sound.

`smoke_cpu` now checks both outcomes of all four branch opcodes. `audio`
replays the ambient voice and first enemy hit, checking accepted commands
and non-centre DAC samples on channels 1 and 0 respectively. All four CTest
suites pass.

## MCU sample timer correction, 2026-11-13

The previous MAME Lua memory taps missed DAC writes, so the earlier conclusion
that MAME only wrote DAC during startup was wrong. Debugger watchpoints on MCU
`$D000` and `$D400` show 198 writes total per video frame in gameplay,
including at frame 1650. The ROM routine at `$F37D–$F387` writes both DACs.
The native MCU previously made **zero** DAC writes in 4,500 frames.

The HD63701 core lacked its `$08` timer control, `$09–$0A` counter,
`$0B–$0C` output compare, and compare/overflow interrupt vectors. The
internal ROM's compare vector `$FFF4` points to `$F18C`, which dispatches
the sample routine. These registers and interrupts now run. The MCU also now
advances at its effective 1.536 MHz E clock rather than four times that rate;
the 16-cycle scheduler carries instruction overshoot into the next slice.
At native frame 3490 the DACs each write 98 times, compared with MAME's
about 99. The DAC outputs contain varying sample values, and the DAC gain
bits now follow MAME's separate left/right 2-bit mapping.

This confirms a missing sample path, not that each reported effect now sounds
correct. The legacy MCU port map currently puts P1 data at `$01` and P2 DDR
at `$02`; MAME's HD63701 core uses P2 DDR at `$01` and P1 data at `$02`.
Correcting just those addresses in this branch caused extra credits before
the scripted coin and failed `start_sequence`; the change was reverted for
this slice. It needs a dedicated boot/coin trace before it can be applied.

Next work, in order:

1. Listen in the game to the confirmed punch at the first enemy and compare
   the new native capture with MAME. Verify the kick separately, because its
   trigger and voice-table entry may differ.
2. Identify the `$0D` DAC 1 voice visually and check it by ear. Trace a
   confirmed Rick death through the MCU command bytes, voice bank, and DAC
   values. The branch fix may help other voices, but those events have not
   been established.
3. Correct the MCU P1/P2 internal register map with a credit/coin trace, then
   compare sample command data and DAC waveform around confirmed effects.
4. Compare music channel activity and short-window RMS/spectrum against a
   MAME WAV aligned to the same game event. Then refine YM timer/busy behavior,
   gain, and the scheduler's within-slice timing as evidence requires.

## Thin implementation slices

1. **Find a gameplay SFX command.** The native sound CPU now advances past
   the boot handshake and receives Start's command. Trace register
   writes around a chosen, clearly audible gameplay action. A single
   snapshot with zero CUS30 volumes does not prove silence throughout the
   action; transient writes need an event trace.
2. **Finish CUS30 fidelity.** The eight waveform channels, packed wave RAM,
   noise mode, left/right volume and 20-bit frequency now have a basic
   renderer. Refine volume/timing against MAME, and replay an actual game
   register trace when one is found.
3. **Refine emulated write time.** The fixed buffer is 792 stereo frames at
   48 kHz and 60.606 Hz. Writes now affect the approximate remaining portion
   of the frame, but all writes within one 16-cycle CPU slice share a sample
   timestamp. The SDL queue is bounded.
4. **Compare an effect with MAME.** Record the same input event in MAME and
   native to WAV. Align by the game event, then inspect onset, duration,
   pitch, channel balance, and waveform/RMS. `tools/compare_audio.py` only
   checks whole-file sample RMS; add event alignment or a small waveform
   plot before treating that result as an acceptance check.
5. **Expand afterward.** The two MCU DACs now receive timed writes and their
   separate gain bits for voice/sample effects. The YM2151 core is in place,
   but timer/busy and subframe timing still need comparison. Keep YM muting before the final
   mixer so a future replacement soundtrack leaves CUS30 and DAC audible.

MAME references: [Namco System 1 driver](https://github.com/mamedev/mame/blob/master/src/mame/namco/namcos1.cpp),
[HD63701 timer/ports](https://github.com/mamedev/mame/blob/master/src/devices/cpu/m6800/m6801.cpp),
and [CUS30 device](https://github.com/mamedev/mame/blob/master/src/devices/sound/namco.cpp).
The local comparison used MAME 0.256; check exact behavior against that
revision when copying timing details from current MAME source.

## Current verification commands

```powershell
./build.ps1
ctest --test-dir build --output-on-failure
./build/sh_trace_audio.exe 3520 3350 3440 |
  Select-String '^audio-stats|^dac f=34|^register f=34'
./build/sh_capture_audio.exe build/audio/start_subframe.wav 2200 2700
./build/sh_capture_audio.exe build/audio/gameplay_timer.wav 3350 3550 3350
./build/sh_capture_audio.exe build/audio/gameplay_attack_branch_fixed.wav 3350 3550 3350 3440
```

The `audio` test checks a CUS30 waveform, noise polarity, a subframe voice
window, sound CPU boot, YM2151 output after Start, the coin effect, about
98 DAC writes per channel in a gameplay frame, and 792 stereo samples per
48 kHz video frame. `start_sequence` still reaches gameplay with four hearts,
normal damage, death, and respawn. The new `gameplay_timer.wav` has peak
0.378 and RMS 0.079; this is a technical capture, not an auditory sign-off.
The confirmed hit additionally starts DAC 0 at frame 3447; MAME starts its
corresponding sample around frame 1649.
The `start_final.wav` capture is generated from the ROM and is an audible
checkpoint, not a claim of MAME-matched SFX. The YM2151 core's timers/busy
timing and exact subframe register scheduling remain unfinished.
