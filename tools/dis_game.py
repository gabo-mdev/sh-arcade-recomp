#!/usr/bin/env python3
from pathlib import Path
import sys

sys.path.insert(0, "tools")
import dis_boot

rom = Path("rom/mainrom.bin").read_bytes()
BASE_C = 0x1B4 * 0x2000
BASE_E = 0x1FF * 0x2000


def r(addr: int) -> int:
    if 0xC000 <= addr <= 0xDFFF:
        return rom[BASE_C + (addr - 0xC000)]
    if 0xE000 <= addr <= 0xFFFF:
        return rom[BASE_E + (addr - 0xE000)]
    raise ValueError(hex(addr))


dis_boot.r = r
start = int(sys.argv[1], 16) if len(sys.argv) > 1 else 0xD5C0
end = int(sys.argv[2], 16) if len(sys.argv) > 2 else start + 0x60
print(rom[BASE_C + (start - 0xC000) : BASE_C + (end - 0xC000)].hex(" "))
for line in dis_boot.dis(start, end):
    print(line)

# callers of start
chunk = rom[BASE_C : BASE_C + 0x2000]
target = start
for i in range(len(chunk) - 2):
    op = chunk[i]
    cpu = 0xC000 + i
    if op in (0x7E, 0xBD):
        t = (chunk[i + 1] << 8) | chunk[i + 2]
        if t == target:
            print(("JMP" if op == 0x7E else "JSR"), f"from {cpu:04X}")
    if op in (0x16, 0x17):
        rel = (chunk[i + 1] << 8) | chunk[i + 2]
        if rel >= 0x8000:
            rel -= 0x10000
        if (cpu + 3 + rel) & 0xFFFF == target:
            print(("LBRA" if op == 0x16 else "LBSR"), f"from {cpu:04X}")
