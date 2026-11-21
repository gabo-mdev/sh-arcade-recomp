#!/usr/bin/env python3
"""Run headless boot probe: status JSON + frame PNG via the C++ binary helpers."""
from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
EXE = ROOT / "build" / "splatterhouse.exe"


def main() -> int:
    frames = int(sys.argv[1]) if len(sys.argv) > 1 else 300
    out_dir = ROOT / "build" / "probe"
    out_dir.mkdir(parents=True, exist_ok=True)
    # Prefer dedicated probe flag once available; fall back to --frames.
    cmd = [str(EXE), "--headless", "--probe", str(frames), "--probe-out", str(out_dir)]
    r = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True)
    print(r.stdout)
    if r.returncode != 0:
        print(r.stderr, file=sys.stderr)
        # Fallback: plain frames if --probe not linked yet
        if "Unknown" in r.stderr or "Usage" in r.stdout or r.returncode != 0:
            r2 = subprocess.run(
                [str(EXE), "--headless", "--frames", str(frames)],
                cwd=ROOT,
                capture_output=True,
                text=True,
            )
            print(r2.stdout)
            print(r2.stderr, file=sys.stderr)
            return r2.returncode
        return r.returncode
    status = out_dir / "status.json"
    if status.exists():
        print(json.dumps(json.loads(status.read_text(encoding="utf-8")), indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
