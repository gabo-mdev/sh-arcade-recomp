#!/usr/bin/env python3
"""Static analysis of mainrom 6809 code for recompilation.

Reads rom/mainrom.bin + optional analysis/coverage.bin and emits
analysis/static.json with code ranges and unresolved branches.
"""

from __future__ import annotations

import json
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

# Subset of 6809 opcodes used to walk linearly from covered seeds.
# Full decoder lives in tools/m6809_decode.py / transpile.
IMM8 = {0x86, 0xC6, 0x8E, 0xCE, 0x84, 0xC4, 0x8A, 0xCA, 0x88, 0xC8, 0x80, 0xC0, 0x8B, 0xCB}
IMM16 = {0x8E, 0xCE, 0xCC, 0x8C}  # note overlap handled carefully below
REL8 = {0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F}
DIRECT = {0x96, 0xD6, 0x9E, 0xDE, 0x97, 0xD7}
EXTENDED = {0xB6, 0xF6, 0xBE, 0xFE, 0xB7, 0xF7, 0x7E, 0xBD}
INDEXED = {0xA6, 0xE6, 0xAE, 0xEE, 0xA7, 0xE7, 0xAD, 0x6E}


def insn_len(op: int, rom: bytes, pc: int) -> int:
    if op in (0x10, 0x11):
        return 2 + max(0, insn_len(rom[pc + 1], rom, pc + 1) - 1) if pc + 1 < len(rom) else 2
    if op in REL8 or op in IMM8 or op in DIRECT:
        return 2
    if op in EXTENDED or op == 0x8E or op == 0xCE:
        return 3
    if op in INDEXED:
        return 2  # + postbyte; exact length refined in transpile
    return 1


def main() -> int:
    rom_path = ROOT / "rom" / "mainrom.bin"
    if not rom_path.exists():
        raise SystemExit("run tools/extract_rom.py first")
    rom = rom_path.read_bytes()
    cov_path = ROOT / "analysis" / "coverage.bin"
    covered = set()
    if cov_path.exists():
        bits = cov_path.read_bytes()
        for pc in range(0x10000):
            if bits[pc >> 3] & (1 << (pc & 7)):
                covered.add(pc)
    # Seed from PRG7 reset vector window (CPU view E000-FFFF -> last 8K of PRG7)
    # After CUS117 remap default bank7 = 0x3FF*0x2000 -> ROM offset 0x3FE000
    base = 0x3FE000
    window = rom[base : base + 0x2000]
    reset = (window[0x1FFE] << 8) | window[0x1FFF]
    seeds = sorted(covered) if covered else [reset & 0xFFFF]

    funcs = []
    for seed in seeds[:512]:
        funcs.append({"entry": f"0x{seed:04X}", "kind": "covered" if seed in covered else "reset"})

    out = {
        "arch": "mc6809e",
        "reset_vector_cpu": f"0x{reset:04X}",
        "functions": funcs,
        "notes": "Preliminary; expand with jump-table recovery in transpile pass.",
    }
    out_dir = ROOT / "analysis"
    out_dir.mkdir(parents=True, exist_ok=True)
    (out_dir / "static.json").write_text(json.dumps(out, indent=2), encoding="utf-8")
    print(f"wrote analysis/static.json ({len(funcs)} seeds, reset={reset:04X})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
