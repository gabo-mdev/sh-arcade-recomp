#!/usr/bin/env python3
"""Disassemble bank7 page 0x1FF (E000 window) using dis_boot decoder."""
from pathlib import Path
import importlib.util

spec = importlib.util.spec_from_file_location("dis_boot", "tools/dis_boot.py")
db = importlib.util.module_from_spec(spec)
spec.loader.exec_module(db)

rom = Path("rom/mainrom.bin").read_bytes()
BASE_E = 0x1FF * 0x2000
# After STD $EC00 #$03B4, C000 is page 0x3B4 — may be chip-relative.
# Try both 0x1B4 (seen in probe) and low 9 bits of 0x3B4.
BASE_C_CANDIDATES = {
    "1B4": 0x1B4 * 0x2000,
    "3B4": None,  # may OOB
    "0B4": 0x0B4 * 0x2000,
}


def make_r(base_c: int):
    def r(addr: int) -> int:
        if 0xE000 <= addr <= 0xFFFF:
            return rom[BASE_E + (addr - 0xE000)]
        if 0xC000 <= addr <= 0xDFFF:
            return rom[base_c + (addr - 0xC000)]
        raise ValueError(hex(addr))

    return r


import sys

start = int(sys.argv[1], 16) if len(sys.argv) > 1 else 0xE00A
end = int(sys.argv[2], 16) if len(sys.argv) > 2 else 0xE0C0
base_c = int(sys.argv[3], 16) * 0x2000 if len(sys.argv) > 3 else 0x1B4 * 0x2000
db.r = make_r(base_c)
print(f"=== {start:04X}-{end:04X} bank7=1FF bank6={base_c // 0x2000:03X} ===")
for line in db.dis(start, end):
    print(line)
