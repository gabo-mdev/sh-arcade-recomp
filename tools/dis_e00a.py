#!/usr/bin/env python3
"""Disassemble game bank pages at E000 / C000."""
from pathlib import Path

rom = Path("rom/mainrom.bin").read_bytes()


def dis_page(page: int, cpu_base: int, start_off: int, end_off: int) -> None:
    base = page * 0x2000
    a = start_off
    while a < end_off:
        pc = cpu_base + a

        def fetch() -> int:
            nonlocal a
            v = rom[base + a]
            a += 1
            return v

        op = fetch()
        if op == 0x16:
            hi, lo = fetch(), fetch()
            off = (hi << 8) | lo
            if off >= 0x8000:
                off -= 0x10000
            print(f"{pc:04X} LBRA ${(pc + 3 + off) & 0xFFFF:04X}")
        elif op == 0x17:
            hi, lo = fetch(), fetch()
            off = (hi << 8) | lo
            if off >= 0x8000:
                off -= 0x10000
            print(f"{pc:04X} LBSR ${(pc + 3 + off) & 0xFFFF:04X}")
        elif op == 0x7E:
            hi, lo = fetch(), fetch()
            print(f"{pc:04X} JMP ${hi:02X}{lo:02X}")
        elif op == 0xBD:
            hi, lo = fetch(), fetch()
            print(f"{pc:04X} JSR ${hi:02X}{lo:02X}")
        elif op in (0xCC, 0x8E, 0xCE):
            hi, lo = fetch(), fetch()
            mn = {0xCC: "LDD", 0x8E: "LDX", 0xCE: "LDU"}[op]
            print(f"{pc:04X} {mn} #${hi:02X}{lo:02X}")
        elif op in (0x86, 0xC6):
            mn = "LDA" if op == 0x86 else "LDB"
            print(f"{pc:04X} {mn} #${fetch():02X}")
        elif op in (0xB7, 0xF7, 0xB6, 0xF6, 0x7F, 0xFC, 0xFD, 0xBE, 0xBF):
            hi, lo = fetch(), fetch()
            mn = {
                0xB7: "STA",
                0xF7: "STB",
                0xB6: "LDA",
                0xF6: "LDB",
                0x7F: "CLR",
                0xFC: "LDD",
                0xFD: "STD",
                0xBE: "LDX",
                0xBF: "STX",
            }[op]
            print(f"{pc:04X} {mn} ${hi:02X}{lo:02X}")
        elif op in (
            0x20,
            0x27,
            0x26,
            0x2F,
            0x2D,
            0x22,
            0x23,
            0x24,
            0x25,
            0x2A,
            0x2B,
            0x2C,
            0x2E,
        ):
            off = fetch()
            if off >= 128:
                off -= 256
            mn = {
                0x20: "BRA",
                0x27: "BEQ",
                0x26: "BNE",
                0x2F: "BLE",
                0x2D: "BLT",
                0x22: "BHI",
                0x23: "BLS",
                0x24: "BCC",
                0x25: "BCS",
                0x2A: "BPL",
                0x2B: "BMI",
                0x2C: "BGE",
                0x2E: "BGT",
            }[op]
            print(f"{pc:04X} {mn} ${(pc + 2 + off) & 0xFFFF:04X}")
        elif op == 0x10:
            op2 = fetch()
            if op2 in (0x27, 0x26, 0x20, 0x25, 0x24, 0x2B, 0x2A):
                hi, lo = fetch(), fetch()
                off = (hi << 8) | lo
                if off >= 0x8000:
                    off -= 0x10000
                mn = {
                    0x27: "LBEQ",
                    0x26: "LBNE",
                    0x20: "LBRA",
                    0x25: "LBCS",
                    0x24: "LBCC",
                    0x2B: "LBMI",
                    0x2A: "LBPL",
                }[op2]
                print(f"{pc:04X} {mn} ${(pc + 4 + off) & 0xFFFF:04X}")
            elif op2 == 0x8E:
                hi, lo = fetch(), fetch()
                print(f"{pc:04X} LDY #${hi:02X}{lo:02X}")
            elif op2 == 0xCE:
                hi, lo = fetch(), fetch()
                print(f"{pc:04X} LDS #${hi:02X}{lo:02X}")
            else:
                print(f"{pc:04X} P2_${op2:02X}")
        elif op == 0x1F:
            print(f"{pc:04X} TFR ${fetch():02X}")
        elif op == 0x39:
            print(f"{pc:04X} RTS")
        elif op == 0x4F:
            print(f"{pc:04X} CLRA")
        elif op == 0x5F:
            print(f"{pc:04X} CLRB")
        elif op == 0x12:
            print(f"{pc:04X} NOP")
        elif op == 0x0F:
            print(f"{pc:04X} CLR ${fetch():02X}")
        elif op in (0x97, 0x96, 0xD7, 0xD6, 0x9F, 0x9E):
            mn = {
                0x97: "STA",
                0x96: "LDA",
                0xD7: "STB",
                0xD6: "LDB",
                0x9F: "STX",
                0x9E: "LDX",
            }[op]
            print(f"{pc:04X} {mn} ${fetch():02X}")
        elif op == 0x8D:
            off = fetch()
            if off >= 128:
                off -= 256
            print(f"{pc:04X} BSR ${(pc + 2 + off) & 0xFFFF:04X}")
        elif op in (0x34, 0x35):
            print(f"{pc:04X} {'PSHS' if op == 0x34 else 'PULS'} #${fetch():02X}")
        elif op in (0xA6, 0xA7, 0xAD, 0x6E, 0x30, 0x31, 0x32, 0xEC, 0xED):
            post = fetch()
            mn = {
                0xA6: "LDA",
                0xA7: "STA",
                0xAD: "JSR",
                0x6E: "JMP",
                0x30: "LEAX",
                0x31: "LEAY",
                0x32: "LEAS",
                0xEC: "LDD",
                0xED: "STD",
            }[op]
            print(f"{pc:04X} {mn} idx:${post:02X}")
            # consume possible offset bytes for common modes
            if post & 0x80:
                ll = post & 0x1F
                if ll in (0x08, 0x0C, 0x18, 0x1C):
                    fetch()
                elif ll in (0x09, 0x0D, 0x19, 0x1D, 0x1F):
                    fetch()
                    fetch()
        elif op == 0x1A:
            print(f"{pc:04X} ORCC #${fetch():02X}")
        elif op == 0x1C:
            print(f"{pc:04X} ANDCC #${fetch():02X}")
        elif op == 0x4A:
            print(f"{pc:04X} DECA")
        elif op == 0x4C:
            print(f"{pc:04X} INCA")
        elif op == 0x5A:
            print(f"{pc:04X} DECB")
        elif op == 0x5C:
            print(f"{pc:04X} INCB")
        elif op == 0x3A:
            print(f"{pc:04X} ABX")
        else:
            print(f"{pc:04X} DB ${op:02X}")


if __name__ == "__main__":
    import sys

    page = int(sys.argv[1], 16) if len(sys.argv) > 1 else 0x173
    cpu = int(sys.argv[2], 16) if len(sys.argv) > 2 else 0xE000
    start = int(sys.argv[3], 16) if len(sys.argv) > 3 else 0x0
    end = int(sys.argv[4], 16) if len(sys.argv) > 4 else 0x120
    print(f"=== page {page:03X} @ {cpu:04X}+{start:04X} ===")
    dis_page(page, cpu, start, end)
