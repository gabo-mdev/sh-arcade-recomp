#!/usr/bin/env python3
"""Compare a raw RGBA8888 frame dump against a reference PNG (or raw)."""

from __future__ import annotations

import argparse
import struct
from pathlib import Path


def load_raw(path: Path, w: int, h: int) -> bytes:
    data = path.read_bytes()
    need = w * h * 4
    if len(data) < need:
        raise SystemExit(f"{path}: expected {need} bytes, got {len(data)}")
    return data[:need]


def load_png_rgba(path: Path) -> tuple[int, int, bytes]:
    try:
        from PIL import Image  # type: ignore
    except ImportError as e:
        raise SystemExit("Pillow required for PNG compare: pip install pillow") from e
    im = Image.open(path).convert("RGBA")
    return im.width, im.height, im.tobytes()


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("ours")
    ap.add_argument("ref")
    ap.add_argument("--w", type=int, default=288)
    ap.add_argument("--h", type=int, default=224)
    args = ap.parse_args()
    ours_p = Path(args.ours)
    ref_p = Path(args.ref)
    ours = load_raw(ours_p, args.w, args.h)
    if ref_p.suffix.lower() == ".png":
        rw, rh, ref = load_png_rgba(ref_p)
        if (rw, rh) != (args.w, args.h):
            raise SystemExit(f"size mismatch {rw}x{rh} vs {args.w}x{args.h}")
    else:
        ref = load_raw(ref_p, args.w, args.h)
    mism = sum(1 for i in range(0, len(ours), 4) if ours[i : i + 4] != ref[i : i + 4])
    print(f"pixels mismatched: {mism} / {args.w * args.h}")
    return 1 if mism else 0


if __name__ == "__main__":
    raise SystemExit(main())
