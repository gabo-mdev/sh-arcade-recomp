#!/usr/bin/env python3
"""Diff MAME vs ours instruction traces (PC and registers)."""

from __future__ import annotations

import argparse
import re
from pathlib import Path

RE = re.compile(
    r"PC=([0-9A-Fa-f]+)\s+A=([0-9A-Fa-f]+)\s+B=([0-9A-Fa-f]+)\s+"
    r"X=([0-9A-Fa-f]+)\s+Y=([0-9A-Fa-f]+)\s+U=([0-9A-Fa-f]+)\s+"
    r"S=([0-9A-Fa-f]+)\s+CC=([0-9A-Fa-f]+)"
)


def parse(path: Path):
    rows = []
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        m = RE.search(line)
        if m:
            rows.append(tuple(int(g, 16) for g in m.groups()))
    return rows


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("mame_log")
    ap.add_argument("ours_log")
    ap.add_argument("--limit", type=int, default=50)
    args = ap.parse_args()
    a = parse(Path(args.mame_log))
    b = parse(Path(args.ours_log))
    n = min(len(a), len(b))
    mism = 0
    for i in range(n):
        if a[i] != b[i]:
            mism += 1
            if mism <= args.limit:
                print(f"mismatch @{i}: mame={a[i]} ours={b[i]}")
    print(f"compared {n} steps, mismatches={mism}, lens=({len(a)},{len(b)})")
    return 1 if mism else 0


if __name__ == "__main__":
    raise SystemExit(main())
