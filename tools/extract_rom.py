#!/usr/bin/env python3
"""Extract and validate a Splatterhouse MAME World SH3 romset (splatter.zip) into rom/."""

from __future__ import annotations

import argparse
import hashlib
import shutil
import sys
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ROM_DIR = ROOT / "rom"

# CRC32 of expected files for SH3 (splatter). Sizes are mandatory; CRC optional warn.
REQUIRED_COMMON = {
    "cus64-64a1.mcu": 0x1000,
    "sh_chr-0.bin": 0x20000,
    "sh_chr-1.bin": 0x20000,
    "sh_chr-2.bin": 0x20000,
    "sh_chr-3.bin": 0x20000,
    "sh_chr-4.bin": 0x20000,
    "sh_chr-5.bin": 0x20000,
    "sh_chr-8.bin": 0x20000,
    "sh_obj-0.bin": 0x20000,
    "sh_obj-1.bin": 0x20000,
    "sh_obj-2.bin": 0x20000,
    "sh_obj-3.bin": 0x20000,
    "sh_obj-4.bin": 0x20000,
    "sh_obj-5.bin": 0x20000,
    "sh_obj-6.bin": 0x20000,
    "sh_obj-7.bin": 0x20000,
    "sh_voi-0.bin": 0x20000,
    "sh_voi-1.bin": 0x20000,
    "sh_voi-2.bin": 0x20000,
    "sh_voi-3.bin": 0x20000,
    "sh1_snd1.bin": 0x10000,
    "sh1_prg0.bin": 0x10000,
    "sh1_prg1.bin": 0x10000,
    "sh1_prg2.bin": 0x10000,
    "sh1_prg3.bin": 0x10000,
    "sh1_prg4.bin": 0x10000,
}

SET = {
    "name": "splatter",
    "desc": "World, new version (SH3)",
    "prg": [
        ("sh1_prg0.bin", 0x000000),
        ("sh1_prg1.bin", 0x080000),
        ("sh1_prg2.bin", 0x100000),
        ("sh1_prg3.bin", 0x180000),
        ("sh1_prg4.bin", 0x200000),
        ("sh3_prg5.bin", 0x280000),
        ("sh3_prg6.bin", 0x300000),
        ("sh3_prg7.bin", 0x380000),
    ],
    "snd0": "sh1_snd0b.bin",
    "extra": {
        "sh3_prg5.bin": 0x10000,
        "sh3_prg6.bin": 0x10000,
        "sh3_prg7.bin": 0x10000,
        "sh1_snd0b.bin": 0x10000,
    },
}


def sha1_file(path: Path) -> str:
    h = hashlib.sha1()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def detect_sh3(names: set[str]) -> None:
    need = {
        "sh3_prg5.bin",
        "sh3_prg6.bin",
        "sh3_prg7.bin",
        "sh1_snd0b.bin",
        "sh1_prg0.bin",
        "cus64-64a1.mcu",
    }
    if not need <= names:
        raise SystemExit(
            "Need the complete MAME World SH3 romset (splatter.zip). "
            "Clones splatter2.zip / splatterj.zip are not supported yet."
        )


def extract_zip(zip_path: Path, dest: Path) -> set[str]:
    dest.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(zip_path) as zf:
        for info in zf.infolist():
            if info.is_dir():
                continue
            raw = Path(info.filename).name
            out = dest / raw
            with zf.open(info) as src, out.open("wb") as dst:
                shutil.copyfileobj(src, dst)
    return {p.name for p in dest.iterdir() if p.is_file()}


def validate(files: dict[str, Path]) -> None:
    expected = dict(REQUIRED_COMMON)
    expected.update(SET["extra"])
    missing = []
    wrong = []
    for name, size in expected.items():
        path = files.get(name)
        if path is None:
            path = next((p for n, p in files.items() if n.lower() == name.lower()), None)
        if path is None:
            missing.append(name)
            continue
        got = path.stat().st_size
        if got != size:
            wrong.append(f"{name}: expected {size} bytes, got {got}")
    if missing or wrong:
        msg = []
        if missing:
            msg.append("Missing: " + ", ".join(missing))
        if wrong:
            msg.extend(wrong)
        raise SystemExit("ROM validation failed:\n  " + "\n  ".join(msg))


def load512(mainrom: bytearray, path: Path, start: int) -> None:
    data = path.read_bytes()
    if len(data) != 0x10000:
        raise SystemExit(f"{path.name}: expected 64KiB, got {len(data)}")
    for i in range(8):
        off = start + i * 0x10000
        mainrom[off : off + 0x10000] = data


def apply_prg7_bit16_swap(mainrom: bytearray) -> None:
    """Match MAME namcos1 driver_init PRG7 shuffle."""
    for i in range(0x380000, 0x400000):
        if (i & 0x010000) == 0:
            j = i + 0x010000
            mainrom[i], mainrom[j] = mainrom[j], mainrom[i]


def build_images(rom_dir: Path, files: dict[str, Path]) -> None:
    def find(name: str) -> Path:
        if name in files:
            return files[name]
        for n, p in files.items():
            if n.lower() == name.lower():
                return p
        raise SystemExit(f"Missing {name}")

    mainrom = bytearray(0x400000)
    for name, start in SET["prg"]:
        load512(mainrom, find(name), start)
    apply_prg7_bit16_swap(mainrom)
    (rom_dir / "mainrom.bin").write_bytes(mainrom)

    snd = bytearray()
    snd += find(SET["snd0"]).read_bytes()
    snd += find("sh1_snd1.bin").read_bytes()
    (rom_dir / "audiocpu.bin").write_bytes(snd)

    shutil.copyfile(find("cus64-64a1.mcu"), rom_dir / "mcu.bin")

    voice = bytearray()
    for i in range(4):
        voice += find(f"sh_voi-{i}.bin").read_bytes()
    (rom_dir / "voice.bin").write_bytes(voice)

    chr_data = bytearray()
    for i in range(6):
        chr_data += find(f"sh_chr-{i}.bin").read_bytes()
    (rom_dir / "c123tmap.bin").write_bytes(chr_data)
    shutil.copyfile(find("sh_chr-8.bin"), rom_dir / "c123mask.bin")

    obj = bytearray()
    for i in range(8):
        obj += find(f"sh_obj-{i}.bin").read_bytes()
    (rom_dir / "sprites.bin").write_bytes(obj)

    meta = rom_dir / "set.txt"
    meta.write_text(
        f"set={SET['name']}\n"
        f"desc={SET['desc']}\n"
        f"mainrom_sha1={hashlib.sha1(mainrom).hexdigest()}\n",
        encoding="utf-8",
    )


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument(
        "zip_or_dir",
        nargs="?",
        default=str(ROOT / "splatter.zip"),
        help="Path to splatter.zip or an already-extracted SH3 directory",
    )
    ap.add_argument("-o", "--out", type=Path, default=ROM_DIR)
    args = ap.parse_args()

    src = Path(args.zip_or_dir)
    out: Path = args.out
    if out.exists():
        for p in out.iterdir():
            if p.is_file():
                p.unlink()
    out.mkdir(parents=True, exist_ok=True)

    if src.is_file() and src.suffix.lower() == ".zip":
        name = src.name.lower()
        if name in ("splatter2.zip", "splatterj.zip"):
            raise SystemExit(
                f"{src.name} is a MAME clone (incomplete). "
                "Use the full World SH3 set: splatter.zip."
            )
        print(f"Extracting {src} -> {out}")
        extract_zip(src, out)
        names = {p.name for p in out.iterdir() if p.is_file()}
    elif src.is_dir():
        print(f"Copying files from {src} -> {out}")
        for p in src.iterdir():
            if p.is_file():
                shutil.copy2(p, out / p.name)
        names = {p.name for p in out.iterdir() if p.is_file()}
    else:
        raise SystemExit(f"Not found: {src}")

    files = {p.name: p for p in out.iterdir() if p.is_file()}
    detect_sh3({n.lower() for n in names})
    print(f"Detected set: {SET['name']} ({SET['desc']})")
    validate(files)
    build_images(out, files)
    print("Wrote:")
    for name in (
        "mainrom.bin",
        "audiocpu.bin",
        "mcu.bin",
        "voice.bin",
        "c123tmap.bin",
        "c123mask.bin",
        "sprites.bin",
        "set.txt",
    ):
        p = out / name
        print(f"  {p.name:16} {p.stat().st_size:8d}  sha1={sha1_file(p)[:12]}…")
    print("OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
