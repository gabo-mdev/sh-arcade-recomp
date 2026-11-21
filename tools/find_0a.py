from pathlib import Path

rom = Path("rom/mainrom.bin").read_bytes()
BASE = 0x1B4 * 0x2000
chunk = rom[BASE : BASE + 0x2000]
target = 0xDADD
for i in range(len(chunk) - 2):
    op = chunk[i]
    cpu = 0xC000 + i
    if op == 0x20:
        rel = chunk[i + 1]
        if rel >= 128:
            rel -= 256
        if (cpu + 2 + rel) & 0xFFFF == target:
            print(f"BRA {cpu:04X}")
    if op in (0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F):
        rel = chunk[i + 1]
        if rel >= 128:
            rel -= 256
        if (cpu + 2 + rel) & 0xFFFF == target:
            print(f"Bcc {op:02X} {cpu:04X}")
    if op == 0x10 and chunk[i + 1] in (0x26, 0x27):
        rel = (chunk[i + 2] << 8) | chunk[i + 3]
        if rel >= 0x8000:
            rel -= 0x10000
        if (cpu + 4 + rel) & 0xFFFF == target:
            print(f"LBNE/EQ {cpu:04X}")
print("DA00", rom[BASE + 0xA00 : BASE + 0xA50].hex(" "))
idx = rom.find(bytes([0xCC, 0x01, 0x7E, 0xFD, 0xE0, 0x00]), BASE + 0x900, BASE + 0xB00)
print("017E at", hex(0xC000 + (idx - BASE)) if idx >= 0 else None)
# dump around bitwalk verify bytes a1 84 26
for i in range(0xA40, 0xAE0):
    if chunk[i : i + 2] == bytes([0xA1, 0x84]):
        print(f"{0xC000+i:04X}", chunk[i : i + 8].hex(" "))
