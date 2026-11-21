# Splatterhouse (arcade) specifics

## Supported sets

| MAME name | Label | Program chips |
|-----------|-------|---------------|
| `splatter` | World SH3 | `sh1_prg0..4` + `sh3_prg5..7` |

Only the complete parent set (`splatter.zip`) is accepted for now. MAME clones
`splatter2` (SH2) and `splatterj` (Japan) are not supported yet.

## Shared assets

- MCU: `cus64-64a1.mcu` (4 KiB)
- Voice: `sh_voi-0..3.bin`
- Tiles: `sh_chr-0..5.bin`, mask `sh_chr-8.bin`
- Sprites: `sh_obj-0..7.bin`
- Sound: `sh1_snd0b.bin` + `sh1_snd1.bin`

## Protection

`init_splatter()` installs **keycus type 3**, id **181**:

```text
key_type_3_init(181, 3, 4, -1, -1, -1, -1);
```

Keycus window sits at virtual `0x2F8000–0x2F9FFF`. A faithful boot needs these handlers.

## PRG7 shuffle

During `driver_init`, MAME swaps bytes in the PRG7 slot so that bit 16 of the
physical address is inverted. `tools/extract_rom.py` applies the same transform
when building `rom/mainrom.bin`.

## Inputs (active low unless noted)

Player 1/2: 8-way stick, button1 (attack), button2 (jump), start.
Coins / service via MCU port 1.

## SH3 DIP switches

`splatter` (World SH3) uses MAME's `splatter3` port set. Switches are
active-low: the bit set means Off. The factory image is `0xFF`.

| Bit | Switch | Meaning | Off (bit set) | On (bit clear) |
|-----|--------|---------|---------------|----------------|
| 0x01 | SW1 | Service mode | Off | On |
| 0x04 | SW3 | Stage select | Off (Start enters the game) | On (Start opens the stage menu) |

The other bits are the Namco System 1 watchdog options and are not gameplay
switches. In the SDL host, F1 or right-stick click opens the options menu.
The menu writes `options.cfg` next to the executable. Stage select
clears bit `0x04` from the next frame. "Arranque rapido" leaves the ROM
sum in place and only drops the 60 Hz cap until the title HLE, so the RAM
and VRAM tests are not a 30 second black screen. Stage select
is not a button combination on this ROM. The factory
DIP still reads Off in this port, but the host store `$1479 = 1` (added so
attract music plays) brings the menu back in play. That interaction is open;
see [06-start-possession.md](06-start-possession.md). The Select ×3,
down-left, both buttons sequence belongs to the TurboGrafx-16 /
PC Engine port. The arcade debug screen (STAGE / LEFT / LIFE) is reached
by patching the sub immediate at file page `0x177` offset `0x1C3C` from
`0x27` to `0x2C`, not by an input combo.
