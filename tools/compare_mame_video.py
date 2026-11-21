#!/usr/bin/env python3
"""Render one MAME video-state dump locally and report exact pixel differences."""
import argparse
import subprocess
from pathlib import Path

from PIL import Image, ImageChops

ROOT = Path(__file__).resolve().parents[1]


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("frame", type=int, help="frame number in MAME dump filenames")
    parser.add_argument("reference", type=Path, help="matching MAME snapshot PNG")
    parser.add_argument("--dump-dir", type=Path, default=ROOT / "build/mame_ref/state")
    parser.add_argument("--out-dir", type=Path, default=ROOT / "build/mame_ref/compared")
    args = parser.parse_args()
    args.out_dir.mkdir(parents=True, exist_ok=True)
    ppm = args.out_dir / f"frame_{args.frame:05d}.ppm"
    subprocess.run(
        [str(ROOT / "build/sh_render_state.exe"), str(ROOT / "rom"),
         str(args.dump_dir), str(args.frame), str(ppm)],
        check=True,
        cwd=ROOT,
    )
    with Image.open(ppm) as rendered, Image.open(args.reference) as reference:
        rendered_rgb = rendered.convert("RGB")
        reference_rgb = reference.convert("RGB")
    if rendered_rgb.size != reference_rgb.size:
        parser.error(f"image sizes differ: {rendered_rgb.size} vs {reference_rgb.size}")
    diff = ImageChops.difference(rendered_rgb, reference_rgb)
    count = sum(pixel != (0, 0, 0) for pixel in diff.get_flattened_data())
    rendered_rgb.save(args.out_dir / f"frame_{args.frame:05d}.png")
    diff.save(args.out_dir / f"frame_{args.frame:05d}_diff.png")
    print(f"frame {args.frame}: {count} different pixels; bounds {diff.getbbox()}")


if __name__ == "__main__":
    main()
