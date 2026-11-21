#!/usr/bin/env python3
"""Disassemble code in bank7 page 0x1FF ($E000 window)."""
from pathlib import Path
import importlib.util

spec = importlib.util.spec_from_file_location("dis_boot", "tools/dis_boot.py")
# Inline minimal: reuse fetch from page 1FF for E000-FFFF and need C000 from bank6.
rom = Path("rom/mainrom.bin").read_bytes()
BASE_E = 0x1FF * 0x2000
# After E019 banks bank0=0x180; bank6 for C000 may still be 0x3B4 chip or boot page.
# From reset vector path: STD $EC00 with #$03B4 → page 0xB4 with high chip.
# For reading C000 during early E00A, bank6 may still be whatever C0AB left.
# C0AB did NOT set bank6/7. Boot likely set bank6 to page 0x184 (code we had).

BASE_C = 0x184 * 0x2000  # prior working assumption


def r(addr: int) -> int:
    if 0xE000 <= addr <= 0xFFFF:
        return rom[BASE_E + (addr - 0xE000)]
    if 0xC000 <= addr <= 0xDFFF:
        return rom[BASE_C + (addr - 0xC000)]
    raise ValueError(hex(addr))


# Monkeypatch dis_boot.r
import tools.dis_boot as db

db.r = r
for label, s, e in [("E000", 0xE000, 0xE0C0), ("E0A8", 0xE0A8, 0xE100)]:
    print("====", label, "====")
    for line in db.dis(s, e):
        print(line)
    print()
