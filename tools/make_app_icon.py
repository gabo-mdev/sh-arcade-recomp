#!/usr/bin/env python3
"""Crop the remaining-lives mask HUD sprite and build assets/app.ico."""

from __future__ import annotations

import io
import struct
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
# Frame with two clear remaining-lives masks in the top HUD.
FRAME = ROOT / "build/captures/health_early2/frame_03300/frame.png"
OUT_DIR = ROOT / "assets"
PNG_OUT = OUT_DIR / "app_icon.png"
ICO_OUT = OUT_DIR / "app.ico"
# Exact bbox of the leftmost lives-mask (measured from the capture).
MASK_BBOX = (194, 8, 205, 22)  # 11x14


def is_mask_body(r: int, g: int, b: int) -> bool:
  """Neutral gray/white mask shading (keeps black eye holes opaque)."""
  if abs(r - g) > 12 or abs(g - b) > 12:
    return False
  return r >= 70  # includes mid grays; pure black eyes handled separately


def make_transparent(img: Image.Image) -> Image.Image:
  out = img.convert("RGBA")
  px = out.load()
  w, h = out.size
  # Keep near-black pixels that sit inside the mask silhouette (eyes / holes).
  body = [[is_mask_body(*px[x, y][:3]) for x in range(w)] for y in range(h)]
  for y in range(h):
    for x in range(w):
      r, g, b, _a = px[x, y]
      if body[y][x]:
        continue
      # Opaque black if surrounded by (or next to) mask body → eye / vent hole.
      if r + g + b < 80:
        near = False
        for dy in (-1, 0, 1):
          for dx in (-1, 0, 1):
            nx, ny = x + dx, y + dy
            if 0 <= nx < w and 0 <= ny < h and body[ny][nx]:
              near = True
        if near:
          px[x, y] = (0, 0, 0, 255)
          continue
      px[x, y] = (0, 0, 0, 0)
  return out


def write_ico(path: Path, images: list[Image.Image]) -> None:
  """Write a multi-size ICO (PNG-compressed entries)."""
  entries: list[tuple[int, int, bytes]] = []
  for im in images:
    w, h = im.size
    buf = io.BytesIO()
    im.save(buf, format="PNG")
    entries.append((w if w < 256 else 0, h if h < 256 else 0, buf.getvalue()))

  offset = 6 + 16 * len(entries)
  parts = [struct.pack("<HHH", 0, 1, len(entries))]
  data_blobs: list[bytes] = []
  for w, h, blob in entries:
    parts.append(struct.pack("<BBBBHHII", w, h, 0, 0, 1, 32, len(blob), offset))
    data_blobs.append(blob)
    offset += len(blob)
  path.write_bytes(b"".join(parts) + b"".join(data_blobs))


def main() -> None:
  if not FRAME.is_file():
    raise SystemExit(f"missing capture frame: {FRAME}")
  src = Image.open(FRAME).convert("RGBA")
  mask = make_transparent(src.crop(MASK_BBOX))
  print(f"mask size: {mask.size}")

  OUT_DIR.mkdir(parents=True, exist_ok=True)
  # Square canvas so Windows doesn't stretch the 11x14 sprite.
  canvas = Image.new("RGBA", (16, 16), (0, 0, 0, 0))
  ox = (16 - mask.width) // 2
  oy = (16 - mask.height) // 2
  canvas.paste(mask, (ox, oy), mask)
  master = canvas.resize((64, 64), Image.NEAREST)
  master.save(PNG_OUT)

  sizes = [16, 24, 32, 48, 64, 128, 256]
  icons = [canvas.resize((s, s), Image.NEAREST) for s in sizes]
  write_ico(ICO_OUT, icons)
  print(f"wrote {PNG_OUT.relative_to(ROOT)}")
  print(f"wrote {ICO_OUT.relative_to(ROOT)}")


if __name__ == "__main__":
  main()
