#!/usr/bin/env python3
"""Capture repeatable video probes without interrupting a running game."""
import argparse
import json
import subprocess
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
EXE = ROOT / "build" / "splatterhouse.exe"


def capture(frames: int, out: Path, coin_frame: int | None, start_frame: int | None,
            input_hold: int, right_frame: int | None, right_hold: int) -> None:
    frame_dir = out / f"frame_{frames:05d}"
    frame_dir.mkdir(parents=True, exist_ok=True)
    command = [str(EXE), "--probe", str(frames), "--probe-out", str(frame_dir)]
    if coin_frame is not None:
        command += ["--coin-frame", str(coin_frame)]
    if start_frame is not None:
        command += ["--start-frame", str(start_frame)]
    if coin_frame is not None or start_frame is not None:
        command += ["--input-hold", str(input_hold)]
    if right_frame is not None:
        command += ["--right-frame", str(right_frame), "--right-hold", str(right_hold)]
    subprocess.run(
        command,
        check=True,
        cwd=ROOT,
        stdout=subprocess.DEVNULL,
    )
    with Image.open(frame_dir / "last_frame.ppm") as image:
        image.save(frame_dir / "frame.png")
        image.resize((576, 448), Image.Resampling.NEAREST).save(frame_dir / "preview.png")
    st = json.loads((frame_dir / "status.json").read_text(encoding="utf-8"))
    print(
        f"f={frames} pc={st['main_pc']:04X} vram={st['vram_nz']} "
        f"latches={st['sprite_latch_requests']} -> {frame_dir}"
    )


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("frames", nargs="*", type=int, default=[2200, 3000, 4000, 6000])
    parser.add_argument("--out", type=Path, default=ROOT / "build" / "captures")
    parser.add_argument("--coin-frame", type=int)
    parser.add_argument("--start-frame", type=int)
    parser.add_argument("--input-hold", type=int, default=4)
    parser.add_argument("--right-frame", type=int)
    parser.add_argument("--right-hold", type=int, default=60)
    args = parser.parse_args()
    if args.coin_frame is not None and args.coin_frame < 0:
        parser.error("--coin-frame must be nonnegative")
    if args.start_frame is not None and args.start_frame < 0:
        parser.error("--start-frame must be nonnegative")
    if args.input_hold < 1:
        parser.error("--input-hold must be positive")
    if args.right_frame is not None and args.right_frame < 0:
        parser.error("--right-frame must be nonnegative")
    if args.right_hold < 1:
        parser.error("--right-hold must be positive")
    if (args.coin_frame is not None or args.start_frame is not None) and args.out == ROOT / "build" / "captures":
        args.out = ROOT / "build" / "captures" / f"coin_{args.coin_frame}_start_{args.start_frame}"
    for frames in args.frames:
        if frames <= 0:
            parser.error("frame counts must be positive")
        capture(frames, args.out, args.coin_frame, args.start_frame, args.input_hold,
                args.right_frame, args.right_hold)


if __name__ == "__main__":
    main()
