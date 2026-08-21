#!/usr/bin/env python3
"""
validate_restir_gi.py — 4-check structural validator for TestReSTIR_GI_Temporal dumps.

Implements the validator cited in:
  - TestReSTIR_GI_Temporal.cpp:41-42 ("Validation: see TestReSTIR_GI_Temporal_Data/validate_restir_gi.py")
  - DIAGNOSTIC_2026-08-01-v25.md (v25 finding: scalar mean-luma gate lets garbage pass)
  - software-development-practices §"4-check structural validator > scalar mean-luma gate"

Acceptance criteria (the 4 structural checks):
  1. Black-pixel ratio < 5%       — shadows allowed, full-black is not
  2. Color variance > some floor  — per-channel spatial std over the whole frame
  3. Temporal stability < ceiling — max step between consecutive frame means (skipped if only 1 frame)
  4. Cell variance > some floor   — split image into NxN grid; std of cell-means

Usage:
  python3 validate_restir_gi.py <dump_dir>
  python3 validate_restir_gi.py <dump_dir> --verbose
  python3 validate_restir_gi.py <dump_dir> --display-only

Exit codes:
  0  PASS  — all 4 checks pass on newest dump group's display.png
  1  FAIL  — at least one check failed (see stderr for which)
  2  USAGE — bad args, missing dir, no dumps found
  3  MISS  — newest dump group is missing required textures (display/spatial/gi_raw/gbuffer_material)
  4  VULK  — caller (recipe.sh) detected Vulkan VUID/ERROR in log; aborted validation
"""
import argparse
import os
import re
import sys
from pathlib import Path
from typing import List, Optional, Tuple

# Optional imports — fail gracefully if numpy/PIL missing
try:
    import numpy as np
    HAS_NUMPY = True
except ImportError:
    HAS_NUMPY = False

try:
    from PIL import Image
    HAS_PIL = True
except ImportError:
    HAS_PIL = False


# --- 4-check thresholds (calibrated from v25/v176 patch empirical runs) ---
BLACK_PIXEL_RATIO_MAX = 0.05       # < 5% of pixels may be near-black
COLOR_VARIANCE_MIN    = 0.005      # per-channel std floor (0..1 scale)
TEMPORAL_STEP_MAX     = 0.15       # max per-frame-mean step between consecutive frames
CELL_VARIANCE_MIN     = 0.003      # NxN cell-mean std floor
CELL_GRID_N           = 8          # NxN grid for cell-variance check
PIXEL_DARK_THRESH     = 8          # pixel value <= 8/255 considered "black"


def _load_png_as_float(path: Path) -> Optional["np.ndarray"]:
    """Load PNG as (H, W, 4) float32 in [0, 1]."""
    if not HAS_PIL or not HAS_NUMPY:
        return None
    try:
        img = Image.open(str(path)).convert("RGBA")
        arr = np.asarray(img, dtype=np.float32) / 255.0
        return arr
    except Exception as e:
        print(f"WARN: failed to load {path}: {e}", file=sys.stderr)
        return None


def find_dump_groups(dump_dir: Path) -> List[str]:
    """Group dump files by their timestamp prefix (YYYYMMDD_HHMMSS).

    Returns a sorted list of timestamps (newest last).
    """
    if not dump_dir.is_dir():
        return []
    prefix_re = re.compile(r"^(\d{8}_\d{6})_")
    groups: set = set()
    for p in dump_dir.iterdir():
        if p.suffix.lower() != ".png":
            continue
        m = prefix_re.match(p.name)
        if m:
            groups.add(m.group(1))
    return sorted(groups)


def check_black_ratio(arr: "np.ndarray") -> Tuple[bool, float]:
    """Check 1: black-pixel ratio < 5%."""
    if arr is None or arr.size == 0:
        return False, 1.0
    luminance = arr[..., :3].mean(axis=2)
    n_black = int((luminance <= PIXEL_DARK_THRESH / 255.0).sum())
    total = int(luminance.size)
    ratio = n_black / total if total > 0 else 1.0
    return ratio < BLACK_PIXEL_RATIO_MAX, ratio


def check_color_variance(arr: "np.ndarray") -> Tuple[bool, float]:
    """Check 2: per-channel spatial std > some floor (catches uniform-color frames)."""
    if arr is None or arr.size == 0:
        return False, 0.0
    per_channel_std = float(arr[..., :3].std())
    return per_channel_std > COLOR_VARIANCE_MIN, per_channel_std


def check_temporal_stability(dump_dir: Path, ts: str) -> Tuple[bool, float, int]:
    """Check 3: per-frame mean changes by < ceiling between consecutive frames.

    Returns (passed, max_step, n_frames). Returns (True, 0.0, 1) if only 1 frame.
    """
    if not HAS_NUMPY:
        return True, 0.0, 0
    pattern = re.compile(rf"^{re.escape(ts)}_display_frame(\d+)\.png$")
    frames: List[Tuple[int, Path]] = []
    for p in dump_dir.iterdir():
        m = pattern.match(p.name)
        if m:
            frames.append((int(m.group(1)), p))
    frames.sort()
    if len(frames) < 2:
        return True, 0.0, len(frames)
    means = []
    for _, p in frames:
        arr = _load_png_as_float(p)
        if arr is not None:
            means.append(float(arr[..., :3].mean()))
    if len(means) < 2:
        return True, 0.0, len(frames)
    steps = [abs(means[i+1] - means[i]) for i in range(len(means) - 1)]
    max_step = max(steps) if steps else 0.0
    return max_step < TEMPORAL_STEP_MAX, max_step, len(frames)


def check_cell_variance(arr: "np.ndarray", n: int = CELL_GRID_N) -> Tuple[bool, float]:
    """Check 4: split image into NxN grid; std of cell-means > floor.

    Catches 'uniform noise' (high whole-image std but every cell averages the same gray).
    """
    if arr is None or arr.size == 0:
        return False, 0.0
    h, w = arr.shape[:2]
    if h < n or w < n:
        return False, 0.0
    cell_h, cell_w = h // n, w // n
    cell_means = []
    for r in range(n):
        for c in range(n):
            cell = arr[r*cell_h:(r+1)*cell_h, c*cell_w:(c+1)*cell_w, :3]
            cell_means.append(float(cell.mean()))
    cell_std = float(np.std(cell_means))
    return cell_std > CELL_VARIANCE_MIN, cell_std


def find_dump_file(dump_dir: Path, ts: str, name: str) -> Optional[Path]:
    """Find <ts>_<name>_frame*.png (typically returns the last/highest-frame)."""
    pattern = re.compile(rf"^{re.escape(ts)}_{re.escape(name)}_frame(\d+)\.png$")
    best: Optional[Tuple[int, Path]] = None
    for p in dump_dir.iterdir():
        m = pattern.match(p.name)
        if m:
            frame_n = int(m.group(1))
            if best is None or frame_n > best[0]:
                best = (frame_n, p)
    return best[1] if best else None


def find_newest_frame(dump_dir: Path) -> int:
    """Newest frame number across all dumps (frame files can straddle seconds)."""
    pattern = re.compile(r"_frame(\d+)\.png$")
    best = -1
    for p in dump_dir.iterdir():
        if p.suffix.lower() != ".png":
            continue
        m = pattern.search(p.name)
        if m:
            best = max(best, int(m.group(1)))
    return best


def find_frame_in_group(dump_dir: Path, ts: str) -> int:
    """Frame number of the (highest-frame) texture in timestamp group ts."""
    pattern = re.compile(rf"^{re.escape(ts)}_\w+_frame(\d+)\.png$")
    best = -1
    for p in dump_dir.iterdir():
        m = pattern.match(p.name)
        if m:
            best = max(best, int(m.group(1)))
    return best


def find_group_for_frame(dump_dir: Path, frame_n: int, name: str) -> Optional[str]:
    """Newest timestamp group containing <name>_frame<frame_n>.png."""
    pattern = re.compile(rf"^(\d{{8}}_\d{{6}})_{re.escape(name)}_frame{frame_n}\.png$")
    best: Optional[str] = None
    for p in dump_dir.iterdir():
        m = pattern.match(p.name)
        if m and (best is None or m.group(1) > best):
            best = m.group(1)
    return best


def validate(dump_dir: Path, verbose: bool = False, display_only: bool = False) -> int:
    """Run the 4-check validator on the newest dump group.

    Returns 0 on PASS, non-zero on FAIL/USAGE/MISS.
    """
    if not dump_dir.is_dir():
        print(f"ERROR: dump dir not found: {dump_dir}", file=sys.stderr)
        return 2

    groups = find_dump_groups(dump_dir)
    if not groups:
        print(f"ERROR: no dump groups found in {dump_dir}", file=sys.stderr)
        return 2
    # The dump writer stamps each texture with the wall-clock second, so one
    # frame's files can span several timestamp groups (v210). Select the
    # newest timestamp group, take ITS frame number, then resolve each
    # required texture for that frame across groups — a fresh run's frame 16
    # must win over an older run's frame 32.
    newest_ts = groups[-1]
    newest_frame = find_frame_in_group(dump_dir, newest_ts)
    if newest_frame < 0:
        newest_frame = find_newest_frame(dump_dir)
    if newest_frame < 0:
        print(f"ERROR: no frame dumps found in {dump_dir}", file=sys.stderr)
        return 2
    display_ts = find_group_for_frame(dump_dir, newest_frame, "display")
    if display_ts is None:
        print(f"ERROR: no display dump for frame {newest_frame} in {dump_dir}", file=sys.stderr)
        return 3
    newest = display_ts
    if verbose:
        print(f"Found {len(groups)} dump group(s); validating newest frame {newest_frame} (ts {newest})", file=sys.stderr)

    # Required textures for full validation
    if display_only:
        required = ["display"]
    else:
        required = ["display", "spatial", "gi_raw", "gbuffer_material"]
    missing = [name for name in required if find_group_for_frame(dump_dir, newest_frame, name) is None]
    if missing:
        print(f"ERROR: newest frame {newest_frame} is missing required textures: {missing}", file=sys.stderr)
        return 3

    # Load display (always required) and run all 4 checks
    display_path_opt = find_dump_file(dump_dir, newest, "display")
    if display_path_opt is None:
        print(f"ERROR: no display image found in {newest}", file=sys.stderr)
        return 3
    display = _load_png_as_float(display_path_opt)
    if display is None:
        print(f"ERROR: cannot load display image: {display_path_opt}", file=sys.stderr)
        return 2

    results = []
    ok, val = check_black_ratio(display)
    results.append(("black_ratio < 5%", ok, val))
    ok, val = check_color_variance(display)
    results.append(("color_variance > floor", ok, val))
    ok, val2, nframes = check_temporal_stability(dump_dir, newest)
    results.append(("temporal_stability (max step < ceiling)", ok, val2))
    ok, val = check_cell_variance(display)
    results.append(("cell_variance > floor", ok, val))

    all_pass = all(r[1] for r in results)
    print(f"=== validate_restir_gi: {newest} (n_frames={nframes}) ===")
    for name, ok, val in results:
        status = "PASS" if ok else "FAIL"
        print(f"  [{status}] {name}: {val:.6f}")
    print(f"  newest dump: {display_path_opt}")
    return 0 if all_pass else 1


def main() -> int:
    parser = argparse.ArgumentParser(description="Validate newest TestReSTIR_GI_Temporal dump group.")
    parser.add_argument("dump_dir", type=Path, help="Directory containing dump PNGs (typically .../TestReSTIR_GI_Temporal_Data/dumps)")
    parser.add_argument("--verbose", action="store_true", help="Verbose output")
    parser.add_argument("--display-only", action="store_true", help="Only validate the display.png (skip spatial/gi_raw/gbuffer_material requirement)")
    args = parser.parse_args()
    return validate(args.dump_dir, verbose=args.verbose, display_only=args.display_only)


if __name__ == "__main__":
    sys.exit(main())
