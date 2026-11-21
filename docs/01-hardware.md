# Namco System 1 hardware

Reference for the Splatterhouse arcade port. Derived from public MAME
`namcos1` / `c117` documentation (BSD-3-Clause), rewritten for this project.

## CPUs

| Role | CPU | Clock (MAME) |
|------|-----|--------------|
| Main | MC6809E | XTAL 49.152 MHz / 32 ≈ 1.536 MHz |
| Sub | MC6809E | same |
| Sound | MC6809E | same |
| MCU | HD63701V0 (CUS64) | XTAL / 8 |

Main and sub share a 23-bit virtual address space through **CUS117**.
Inter-CPU communication uses a 2K **triram** region.

## CUS117 MMU

- CPU space split into eight 8 KB banks (`0000–1FFF` … `E000–FFFF`).
- Each bank points at a 8 KB window in the 23-bit virtual map.
- Writes to `E000–FFFF` program CUS117 (bank select, SUBRES, watchdog, IRQ/FIRQ ack).
- Default reset banks (MAME):
  - main bank0 / bank1 → `0x180` (work RAM window)
  - main bank7 → `0x3FF` (PRG7)
  - sub bank0 → `0x180`, sub bank7 → `0x3FF`
- Main can halt/reset everything except itself via **SUBRES**.
- Main can set sub's bank7 (reset vectors) before releasing SUBRES.

Bank programming: write even address sets A22–A21; odd sets A20–A13.

## Program ROM packing

`mainrom` is 4 MiB. Splatterhouse SH3 loads eight 64 KiB PRG chips at
`0x00000`, `0x80000`, … `0x380000`, each mirrored eight times into its
512 KiB slot (`ROM_LOAD_512`).

PRG7 has bit 16 of the physical address inverted during driver init
(MAME `driver_init`).

## Sound

- YM2151 (+ YM3012 DAC) for music — primary OST replacement hook later
- Namco CUS30 8-channel wavetable WSG for SFX
- Two 8-bit DACs on the MCU for voice samples (`sh_voi-*`)

Sound CPU banks 16 KiB windows of `sh1_snd0b` / `sh1_snd1`.

## Video

- Resolution 288×224, ~60.61 Hz, ROT180 in cabinet (we present upright)
- CUS133 / CUS123 tilemaps, CUS39/48 sprites, CUS116/120 mixer + palette
- Character ROMs `sh_chr-0..5`, mask `sh_chr-8`, sprites `sh_obj-0..7`

## Splatterhouse keycus

`init_splatter` → key type 3, id **181**. Protection traffic must match
for a faithful boot past attract; see `docs/02-splatterhouse.md`.
