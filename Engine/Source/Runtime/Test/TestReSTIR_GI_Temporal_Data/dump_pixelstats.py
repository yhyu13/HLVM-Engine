#!/usr/bin/env python3
"""
dump_pixelstats.py — Per-channel pixel statistics helper for dump PNGs.

Used by the operator vision check (gate 6 in v176-recipe.sh) when the
4-check validator passes but the operator wants per-channel numbers before
eyeballing the image.

Usage:
  python3 dump_pixelstats.py <png_path>
  python3 dump_pixelstats.py <dump_dir> --newest   # stats on newest display.png
"""
import argparse
import sys
from pathlib import Path
from typing import Optional

try:
    from PIL import Image
    import numpy as np
except ImportError as e:
    print(f"ERROR: {e}\nInstall: pip install numpy Pillow", file=sys.stderr)
    sys.exit(1)


def stats_for(path: Path) -> None:
    img = np.asarray(Image.open(str(path)).convert("RGBA"), dtype=np.float32) / 255.0
    h, w = img.shape[:2]
    print(f"=== {path.name} ({w}x{h}) ===")
    for ch_name, ch_idx in [("R", 0), ("G", 1), ("B", 2), ("A", 3)]:
        c = img[..., ch_idx]
        print(f"  {ch_name}: min={c.min():.3f}  max={c.max():.3f}  mean={c.mean():.3f}  std={c.std():.4f}")
    lum = img[..., :3].mean(axis=2)
    n_dark = int((lum <= 8 / 255.0).sum())
    total = int(lum.size)
    print(f"  luminance: min={lum.min():.3f}  max={lum.max():.3f}  mean={lum.mean():.3f}  std={lum.std():.4f}")
    print(f"  near-black pixels (lum<=8/255): {n_dark} / {total} ({100.0*n_dark/total:.2f}%)")


def find_newest_display(dump_dir: Path) -> Path:
    import re
    pattern = re.compile(r"^(\d{8}_\d{6})_display_frame(\d+)\.png$")
    best_ts: str = ""
    best_frame: int = -1
    best_p: Optional[Path] = None
    for p in dump_dir.iterdir():
        m = pattern.match(p.name)
        if m:
            ts = m.group(1)
            frame = int(m.group(2))
            if best_p is None or (ts, frame) > (best_ts, best_frame):
                best_ts = ts
                best_frame = frame
                best_p = p
    if best_p is None:
        print(f"ERROR: no display dump found in {dump_dir}", file=sys.stderr)
        sys.exit(2)
    return best_p


def main() -> int:
    parser = argparse.ArgumentParser(description="Per-channel pixel statistics for dump PNGs.")
    parser.add_argument("path", type=Path, help="PNG file OR dump directory (with --newest)")
    parser.add_argument("--newest", action="store_true", help="If path is a dir, pick newest *_display_frame*.png")
    args = parser.parse_args()
    p: Path = args.path
    if p.is_dir():
        if not args.newest:
            print("ERROR: path is a dir; pass --newest to pick newest display.png", file=sys.stderr)
            return 2
        p = find_newest_display(p)
    if not p.is_file():
        print(f"ERROR: not a file: {p}", file=sys.stderr)
        return 2
    stats_for(p)
    return 0


if __name__ == "__main__":
    sys.exit(main())
