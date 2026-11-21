#!/usr/bin/env python3
"""Turn trace/coverage_pcs.txt into analysis/coverage.bin (bitset of 64K)."""

from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def main() -> int:
    src = ROOT / "trace" / "coverage_pcs.txt"
    out_dir = ROOT / "analysis"
    out_dir.mkdir(parents=True, exist_ok=True)
    bits = bytearray(0x10000 // 8)
    n = 0
    if src.exists():
        for line in src.read_text(encoding="utf-8").splitlines():
            line = line.strip()
            if not line or line.startswith(";"):
                continue
            pc = int(line, 16)
            bits[pc >> 3] |= 1 << (pc & 7)
            n += 1
    (out_dir / "coverage.bin").write_bytes(bits)
    print(f"coverage: {n} PCs -> analysis/coverage.bin")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
