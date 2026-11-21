#!/usr/bin/env python3
"""Disassemble key Splatterhouse main-CPU boot / test regions."""
from pathlib import Path

rom = Path("rom/mainrom.bin").read_bytes()
# Boot maps $C000-$DFFF to page 0x184 (file 0x308000).
BASE_C = 0x308000


def r(addr: int) -> int:
    if 0xC000 <= addr <= 0xDFFF:
        return rom[BASE_C + (addr - 0xC000)]
    if 0xE000 <= addr <= 0xFFFF:
        return rom[BASE_C + 0x2000 + (addr - 0xE000)]
    raise ValueError(hex(addr))


OPS = {
    0x12: "NOP",
    0x13: "SYNC",
    0x19: "DAA",
    0x1D: "SEX",
    0x39: "RTS",
    0x3A: "ABX",
    0x3B: "RTI",
    0x3D: "MUL",
    0x40: "NEGA",
    0x43: "COMA",
    0x44: "LSRA",
    0x46: "RORA",
    0x47: "ASRA",
    0x48: "ASLA",
    0x49: "ROLA",
    0x4A: "DECA",
    0x4C: "INCA",
    0x4D: "TSTA",
    0x4F: "CLRA",
    0x50: "NEGB",
    0x53: "COMB",
    0x54: "LSRB",
    0x56: "RORB",
    0x57: "ASRB",
    0x58: "ASLB",
    0x59: "ROLB",
    0x5A: "DECB",
    0x5C: "INCB",
    0x5D: "TSTB",
    0x5F: "CLRB",
}

RMAP = {
    0: "D",
    1: "X",
    2: "Y",
    3: "U",
    4: "S",
    5: "PC",
    8: "A",
    9: "B",
    0xA: "CC",
    0xB: "DP",
}


def dis(start: int, end: int):
    a = start
    lines = []
    while a < end:
        pc = a

        def fetch():
            nonlocal a
            v = r(a)
            a += 1
            return v

        def imm8():
            return f"#${fetch():02X}"

        def imm16():
            hi = fetch()
            lo = fetch()
            return f"#${hi:02X}{lo:02X}"

        def dir_():
            return f"${fetch():02X}"

        def ext():
            hi = fetch()
            lo = fetch()
            return f"${hi:02X}{lo:02X}"

        def rel8():
            off = fetch()
            if off >= 128:
                off -= 256
            return f"${(a + off) & 0xFFFF:04X}"

        def rel16():
            hi = fetch()
            lo = fetch()
            off = (hi << 8) | lo
            if off >= 0x8000:
                off -= 0x10000
            return f"${(a + off) & 0xFFFF:04X}"

        def indexed():
            post = fetch()
            rr = ["X", "Y", "U", "S"][(post >> 5) & 3]
            if (post & 0x80) == 0:
                off = post & 0x1F
                if post & 0x10:
                    off -= 32
                return f"{off},{rr}"
            ll = post & 0x1F
            if ll == 0x00:
                return f",{rr}+"
            if ll == 0x01:
                return f",{rr}++"
            if ll == 0x02:
                return f",-{rr}"
            if ll == 0x03:
                return f",--{rr}"
            if ll == 0x04:
                return f",{rr}"
            if ll == 0x05:
                return f"B,{rr}"
            if ll == 0x06:
                return f"A,{rr}"
            if ll == 0x08:
                off = fetch()
                if off >= 128:
                    off -= 256
                return f"{off},{rr}"
            if ll == 0x09:
                hi = fetch()
                lo = fetch()
                off = (hi << 8) | lo
                if off >= 0x8000:
                    off -= 0x10000
                return f"{off},{rr}"
            if ll == 0x0B:
                return f"D,{rr}"
            if ll == 0x0C:
                off = fetch()
                if off >= 128:
                    off -= 256
                return f"{off},PC"
            if ll == 0x0D:
                hi = fetch()
                lo = fetch()
                off = (hi << 8) | lo
                if off >= 0x8000:
                    off -= 0x10000
                return f"{off},PC"
            if ll == 0x11:
                return f"[,{rr}++]"
            if ll == 0x13:
                return f"[,--{rr}]"
            if ll == 0x14:
                return f"[,{rr}]"
            if ll == 0x15:
                return f"[B,{rr}]"
            if ll == 0x16:
                return f"[A,{rr}]"
            if ll == 0x18:
                off = fetch()
                if off >= 128:
                    off -= 256
                return f"[{off},{rr}]"
            if ll == 0x19:
                hi = fetch()
                lo = fetch()
                off = (hi << 8) | lo
                if off >= 0x8000:
                    off -= 0x10000
                return f"[{off},{rr}]"
            if ll == 0x1B:
                return f"[D,{rr}]"
            if ll == 0x1F:
                hi = fetch()
                lo = fetch()
                return f"[${hi:02X}{lo:02X}]"
            return f"idx:${post:02X}"

        op = fetch()
        mnemonic = None
        arg = ""
        if op in OPS:
            mnemonic = OPS[op]
        elif op == 0x20:
            mnemonic, arg = "BRA", rel8()
        elif op == 0x22:
            mnemonic, arg = "BHI", rel8()
        elif op == 0x23:
            mnemonic, arg = "BLS", rel8()
        elif op == 0x24:
            mnemonic, arg = "BCC", rel8()
        elif op == 0x25:
            mnemonic, arg = "BCS", rel8()
        elif op == 0x26:
            mnemonic, arg = "BNE", rel8()
        elif op == 0x27:
            mnemonic, arg = "BEQ", rel8()
        elif op == 0x2A:
            mnemonic, arg = "BPL", rel8()
        elif op == 0x2B:
            mnemonic, arg = "BMI", rel8()
        elif op == 0x2C:
            mnemonic, arg = "BGE", rel8()
        elif op == 0x2D:
            mnemonic, arg = "BLT", rel8()
        elif op == 0x2E:
            mnemonic, arg = "BGT", rel8()
        elif op == 0x2F:
            mnemonic, arg = "BLE", rel8()
        elif op == 0x16:
            mnemonic, arg = "LBRA", rel16()
        elif op == 0x17:
            mnemonic, arg = "LBSR", rel16()
        elif op == 0x8D:
            mnemonic, arg = "BSR", rel8()
        elif op == 0x7E:
            mnemonic, arg = "JMP", ext()
        elif op == 0xBD:
            mnemonic, arg = "JSR", ext()
        elif op == 0x86:
            mnemonic, arg = "LDA", imm8()
        elif op == 0xC6:
            mnemonic, arg = "LDB", imm8()
        elif op == 0xCC:
            mnemonic, arg = "LDD", imm16()
        elif op == 0x8E:
            mnemonic, arg = "LDX", imm16()
        elif op == 0xCE:
            mnemonic, arg = "LDU", imm16()
        elif op == 0x10:
            op2 = fetch()
            if op2 == 0x8E:
                mnemonic, arg = "LDY", imm16()
            elif op2 == 0xCE:
                mnemonic, arg = "LDS", imm16()
            elif op2 == 0x8C:
                mnemonic, arg = "CMPY", imm16()
            elif op2 == 0x83:
                mnemonic, arg = "CMPD", imm16()
            elif op2 == 0x27:
                mnemonic, arg = "LBEQ", rel16()
            elif op2 == 0x26:
                mnemonic, arg = "LBNE", rel16()
            elif op2 == 0x25:
                mnemonic, arg = "LBCS", rel16()
            elif op2 == 0x24:
                mnemonic, arg = "LBCC", rel16()
            elif op2 == 0x2A:
                mnemonic, arg = "LBPL", rel16()
            elif op2 == 0x2B:
                mnemonic, arg = "LBMI", rel16()
            elif op2 == 0x93:
                mnemonic, arg = "CMPD", dir_()
            elif op2 == 0x9E:
                mnemonic, arg = "LDY", dir_()
            elif op2 == 0xAF:
                mnemonic, arg = "STY", indexed()
            elif op2 == 0xBF:
                mnemonic, arg = "STY", ext()
            elif op2 == 0xBE:
                mnemonic, arg = "LDY", ext()
            elif op2 == 0xEE:
                mnemonic, arg = "LDS", indexed()
            elif op2 == 0xEF:
                mnemonic, arg = "STS", indexed()
            elif op2 == 0xFF:
                mnemonic, arg = "STS", ext()
            elif op2 == 0xFE:
                mnemonic, arg = "LDS", ext()
            else:
                mnemonic = f"P2_${op2:02X}"
        elif op == 0x96:
            mnemonic, arg = "LDA", dir_()
        elif op == 0xD6:
            mnemonic, arg = "LDB", dir_()
        elif op == 0xDC:
            mnemonic, arg = "LDD", dir_()
        elif op == 0x9E:
            mnemonic, arg = "LDX", dir_()
        elif op == 0xDE:
            mnemonic, arg = "LDU", dir_()
        elif op == 0x97:
            mnemonic, arg = "STA", dir_()
        elif op == 0xD7:
            mnemonic, arg = "STB", dir_()
        elif op == 0xDD:
            mnemonic, arg = "STD", dir_()
        elif op == 0x9F:
            mnemonic, arg = "STX", dir_()
        elif op == 0xDF:
            mnemonic, arg = "STU", dir_()
        elif op == 0xB7:
            mnemonic, arg = "STA", ext()
        elif op == 0xF7:
            mnemonic, arg = "STB", ext()
        elif op == 0xFD:
            mnemonic, arg = "STD", ext()
        elif op == 0xBF:
            mnemonic, arg = "STX", ext()
        elif op == 0xFF:
            mnemonic, arg = "STU", ext()
        elif op == 0xB6:
            mnemonic, arg = "LDA", ext()
        elif op == 0xF6:
            mnemonic, arg = "LDB", ext()
        elif op == 0xFC:
            mnemonic, arg = "LDD", ext()
        elif op == 0xBE:
            mnemonic, arg = "LDX", ext()
        elif op == 0xFE:
            mnemonic, arg = "LDU", ext()
        elif op == 0x7F:
            mnemonic, arg = "CLR", ext()
        elif op == 0x7A:
            mnemonic, arg = "DEC", ext()
        elif op == 0x7C:
            mnemonic, arg = "INC", ext()
        elif op == 0x7D:
            mnemonic, arg = "TST", ext()
        elif op == 0x0F:
            mnemonic, arg = "CLR", dir_()
        elif op == 0x0A:
            mnemonic, arg = "DEC", dir_()
        elif op == 0x0C:
            mnemonic, arg = "INC", dir_()
        elif op == 0x0D:
            mnemonic, arg = "TST", dir_()
        elif op == 0x81:
            mnemonic, arg = "CMPA", imm8()
        elif op == 0xC1:
            mnemonic, arg = "CMPB", imm8()
        elif op == 0x8C:
            mnemonic, arg = "CMPX", imm16()
        elif op == 0x80:
            mnemonic, arg = "SUBA", imm8()
        elif op == 0xC0:
            mnemonic, arg = "SUBB", imm8()
        elif op == 0x8B:
            mnemonic, arg = "ADDA", imm8()
        elif op == 0xCB:
            mnemonic, arg = "ADDB", imm8()
        elif op == 0x84:
            mnemonic, arg = "ANDA", imm8()
        elif op == 0xC4:
            mnemonic, arg = "ANDB", imm8()
        elif op == 0x8A:
            mnemonic, arg = "ORA", imm8()
        elif op == 0xCA:
            mnemonic, arg = "ORB", imm8()
        elif op == 0x88:
            mnemonic, arg = "EORA", imm8()
        elif op == 0x85:
            mnemonic, arg = "BITA", imm8()
        elif op == 0x34:
            mnemonic, arg = "PSHS", f"#${fetch():02X}"
        elif op == 0x36:
            mnemonic, arg = "PSHU", f"#${fetch():02X}"
        elif op == 0x35:
            mnemonic, arg = "PULS", f"#${fetch():02X}"
        elif op == 0x37:
            mnemonic, arg = "PULU", f"#${fetch():02X}"
        elif op == 0x1F:
            post = fetch()
            mnemonic = "TFR"
            arg = f"{RMAP.get(post >> 4, '?')},{RMAP.get(post & 0xF, '?')}"
        elif op == 0x1E:
            post = fetch()
            mnemonic = "EXG"
            arg = f"{RMAP.get(post >> 4, '?')},{RMAP.get(post & 0xF, '?')}"
        elif op == 0x1A:
            mnemonic, arg = "ORCC", imm8()
        elif op == 0x1C:
            mnemonic, arg = "ANDCC", imm8()
        elif op == 0xA6:
            mnemonic, arg = "LDA", indexed()
        elif op == 0xE6:
            mnemonic, arg = "LDB", indexed()
        elif op == 0xA7:
            mnemonic, arg = "STA", indexed()
        elif op == 0xE7:
            mnemonic, arg = "STB", indexed()
        elif op == 0xEC:
            mnemonic, arg = "LDD", indexed()
        elif op == 0xED:
            mnemonic, arg = "STD", indexed()
        elif op == 0xAE:
            mnemonic, arg = "LDX", indexed()
        elif op == 0xAF:
            mnemonic, arg = "STX", indexed()
        elif op == 0xEE:
            mnemonic, arg = "LDU", indexed()
        elif op == 0xEF:
            mnemonic, arg = "STU", indexed()
        elif op == 0xAD:
            mnemonic, arg = "JSR", indexed()
        elif op == 0x6E:
            mnemonic, arg = "JMP", indexed()
        elif op == 0x30:
            mnemonic, arg = "LEAX", indexed()
        elif op == 0x31:
            mnemonic, arg = "LEAY", indexed()
        elif op == 0x32:
            mnemonic, arg = "LEAS", indexed()
        elif op == 0x33:
            mnemonic, arg = "LEAU", indexed()
        elif op == 0x9A:
            mnemonic, arg = "ORA", dir_()
        elif op == 0x9B:
            mnemonic, arg = "ADDA", dir_()
        elif op == 0x90:
            mnemonic, arg = "SUBA", dir_()
        elif op == 0x94:
            mnemonic, arg = "ANDA", dir_()
        elif op == 0x95:
            mnemonic, arg = "BITA", dir_()
        else:
            mnemonic = f"DB ${op:02X}"
        lines.append(f"{pc:04X} {mnemonic} {arg}".rstrip())
    return lines


if __name__ == "__main__":
    import sys

    ranges = [
        ("C0AB", 0xC0AB, 0xC160),
        ("C800", 0xC800, 0xC880),
        ("D350", 0xD350, 0xD430),
        ("D6D0", 0xD6D0, 0xD720),
    ]
    if len(sys.argv) >= 3:
        ranges = [("custom", int(sys.argv[1], 16), int(sys.argv[2], 16))]
    for label, s, e in ranges:
        print("====", label, "====")
        for line in dis(s, e):
            print(line)
        print()
