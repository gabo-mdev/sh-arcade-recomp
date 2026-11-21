#!/usr/bin/env python3
"""RMS compare of two WAV files (mono or stereo)."""

from __future__ import annotations

import argparse
import math
import wave
from pathlib import Path


def read_wav(path: Path):
    with wave.open(str(path), "rb") as w:
        nch = w.getnchannels()
        sw = w.getsampwidth()
        rate = w.getframerate()
        n = w.getnframes()
        raw = w.readframes(n)
    if sw != 2:
        raise SystemExit("only 16-bit PCM supported")
    import array

    samples = array.array("h")
    samples.frombytes(raw)
    return rate, nch, samples


def rms(a) -> float:
    if not a:
        return 0.0
    s = sum(float(x) * float(x) for x in a) / len(a)
    return math.sqrt(s)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("ours")
    ap.add_argument("mame")
    ap.add_argument("--tol", type=float, default=0.01, help="max relative RMS error")
    args = ap.parse_args()
    _, _, a = read_wav(Path(args.ours))
    _, _, b = read_wav(Path(args.mame))
    n = min(len(a), len(b))
    a = a[:n]
    b = b[:n]
    diff = array_diff_rms(a, b)
    ref = rms(b) or 1.0
    rel = diff / ref
    print(f"rms_diff={diff:.4f} rel={rel:.6f} tol={args.tol}")
    return 0 if rel <= args.tol else 1


def array_diff_rms(a, b) -> float:
    s = 0.0
    for x, y in zip(a, b):
        d = float(x) - float(y)
        s += d * d
    return math.sqrt(s / len(a)) if a else 0.0


if __name__ == "__main__":
    raise SystemExit(main())
