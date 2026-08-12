#!/usr/bin/env python3
"""Per-channel structural pixel statistics for TestReSTIR_GI_Temporal dump groups.

Walks every dumps* directory in the data dir and emits per-frame per-channel:
  - mean (R, G, B)
  - std (across full frame)
  - unique value count (out of 256 possible byte values)
  - fraction-clamped-to-255 (saturated-high)
  - fraction-clamped-to-0 (saturated-low)

This is the canonical "fast first-look" companion to validate_restir_gi.py:
- Use dump_pixelstats.py when you cannot rebuild (terminal blocked, GPU driver
  issue, etc.) and want to know which channel is broken without running the
  test again.
- Use validate_restir_gi.py when you have fresh dumps and want a yes/no pass
  verdict on the calibrated 3-check structural thresholds.

Channels inspected (matched by filename prefix):
  display      -> dumps*/display_frame*.png  (final accumulated, post-tonemap)
  spatial      -> dumps*/spatial_frame*.png  (ReSTIR Spatial radiance)
  denoised     -> dumps*/denoised_frame*.png (BilateralDenoisePass output)
  gi_raw       -> dumps*/gi_raw_frame*.png   (FGIPass OutputTexture, pre-tonemap)

Usage:
  python3 dump_pixelstats.py [--data-dir <path>]
  (default --data-dir is the directory containing this script)

History (six-role-pipeline, 2026-07-27):
  v24: initial write. Cron-driven cycle addressing the structural gap that
  exists when terminal is blocked and vision tool is unavailable. The script
  enables "structural diagnosis on stale dumps" as the next-best signal before
  committing to a full rebuild+rerun cycle.

Exit codes:
  0 = inspected >=1 PNG (or no PNGs found and exited gracefully)
  1 = PIL or numpy not importable (env missing)

Notes on dump-encoder clamp detection (per gpu-rendering-bisect-debug
anti-pattern #6): if a channel shows sat255 > 50% AND unique values > 50, the
underlying data is almost certainly outside [0, 1] and being clamped by
FImageDump::DumpToPNG. The script emits a "CLAMP DETECTED" hint at the end of
each frame's block in this case. See references/dump-normalization-recipe.md
in the gpu-rendering-bisect-debug skill for the per-channel normalization
recipe.

v40 (six-role-pipeline, 2026-07-27): also inspect the alpha channel and
classify it against the v28 alpha-channel sentinel ladder. Pre-v40 the script
read `convert("RGB")` which stripped alpha entirely; the v28 sentinel
`Output[pixel].w = max(..., 0.99994f)` was therefore invisible to this
fast-first-look helper even though it is visible to v37's
`validate_restir_gi.py::check_alpha_sentinel`. v40 closes that gap by
reading in RGBA mode and emitting a `[v40-alpha]` line per frame. The
classification ladder (saturated / zero / mixed / low / unknown) matches
v37's verdict ladder so the two helpers classify evidence identically.
"""

import argparse
import glob
import os
import sys
from typing import List, Optional, Tuple

try:
    import numpy as np
    from PIL import Image
except ImportError as e:
    print(f"ERROR: dump_pixelstats.py requires numpy + PIL: {e}", file=sys.stderr)
    print("Install with: pip install numpy pillow", file=sys.stderr)
    sys.exit(1)


CHANNEL_PATTERNS = [
    ("display",  "display_frame*.png"),
    ("spatial",  "spatial_frame*.png"),
    ("denoised", "denoised_frame*.png"),
    ("gi_raw",   "gi_raw_frame*.png"),
]


def compute_stats(arr: np.ndarray) -> List[Tuple[float, float, int, float, float]]:
    """Return per-channel (mean, std, unique_count, frac_255, frac_0) list, length=3."""
    h, w = arr.shape[:2]
    npix = h * w
    results = []
    for c in range(3):
        chan = arr[:, :, c]
        mean = float(chan.mean())
        std = float(chan.std())
        unique = int(np.unique(chan).size)
        frac_high = float(np.sum(chan == 255)) / npix
        frac_low = float(np.sum(chan == 0)) / npix
        results.append((mean, std, unique, frac_high, frac_low))
    return results


def compute_alpha_stats(arr: np.ndarray) -> Optional[Tuple[float, float, int, float, float]]:
    """v40: per-alpha-channel (mean, std, unique_count, frac_255, frac_0).
    Returns None if input has no alpha channel (shape[-1] < 4)."""
    if arr.ndim < 3 or arr.shape[2] < 4:
        return None
    chan = arr[:, :, 3]
    h, w = chan.shape
    npix = h * w
    mean = float(chan.mean())
    std = float(chan.std())
    unique = int(np.unique(chan).size)
    frac_high = float(np.sum(chan == 255)) / npix
    frac_low = float(np.sum(chan == 0)) / npix
    return (mean, std, unique, frac_high, frac_low)


def classify_alpha_sentinel(stats: Tuple[float, float, int, float, float],
                            saturated_min: float = 0.95,
                            low_max: float = 0.95) -> str:
    """v40: classify alpha stats against the v28 sentinel ladder.

    Mirrors v37's `check_alpha_sentinel()` verdict ladder so the two
    helpers classify evidence identically:

      saturated    : frac_saturated (alpha==255) >= saturated_min
                     -> dispatch body ran; v28 sentinel confirmed
      zero         : frac_zero (alpha==0) >= saturated_min
                     -> dispatch body never ran; bug is upstream
      low          : frac_low (alpha<=50) >= low_max
                     -> pre-v28 binary; sentinel not in compiled shader
      mixed        : neither dominates (partial dispatch)
      unknown      : stats tuple is None (PNG has no alpha channel)

    The thresholds match v37 defaults (saturated_min=0.95, low_max=0.95).
    """
    if stats is None:
        return "unknown"
    _, _, _, frac_high, frac_low = stats
    if frac_high >= saturated_min:
        return "saturated"
    if frac_low >= saturated_min:
        return "zero"
    if frac_low >= low_max:
        # frac_low is frac_of_pixels_equal_to_0; if a large fraction are at
        # 0 but not enough to dominate as 'zero', classify as 'low'.
        return "low"
    return "mixed"


def emit_stats(label: str, path: str) -> None:
    try:
        img = Image.open(path).convert("RGB")
    except Exception as e:
        print(f"  {label} ({path}): FAILED to load ({e})")
        return
    arr = np.array(img, dtype=np.uint8)
    stats = compute_stats(arr)
    h, w = arr.shape[:2]
    print(f"  {label} ({path}) [{w}x{h}]:")
    names = ("R", "G", "B")
    clamp_detected = False
    for (mean, std, unique, frac_high, frac_low), name in zip(stats, names):
        print(f"    {name}: mean={mean:6.2f} std={std:6.2f} "
              f"unique={unique:3d} sat255={frac_high * 100.0:5.1f}% "
              f"sat0={frac_low * 100.0:5.1f}%")
        # Clamp heuristic: >50% pixels at 255 AND >50 unique values
        # suggests non-normalized data being clamped (per gpu-rendering-
        # bisect-debug anti-pattern #6).
        if frac_high > 0.5 and unique > 50:
            clamp_detected = True
    if clamp_detected:
        print("    ^ CLAMP DETECTED: data likely outside [0,1]; see "
              "FImageDump::DumpToPNG normalization recipe")

    # v40: also inspect the alpha channel if present. Re-open in RGBA
    # mode so we do not lose alpha when the original PNG is RGBA.
    try:
        img_rgba = Image.open(path)
        if img_rgba.mode in ("RGBA", "LA") or "transparency" in img_rgba.info:
            arr_rgba = np.array(img_rgba.convert("RGBA"), dtype=np.uint8)
            a_stats = compute_alpha_stats(arr_rgba)
            if a_stats is not None:
                mean, std, unique, frac_high, frac_low = a_stats
                print(f"    A: mean={mean:6.2f} std={std:6.2f} "
                      f"unique={unique:3d} sat255={frac_high * 100.0:5.1f}% "
                      f"sat0={frac_low * 100.0:5.1f}%")
                verdict = classify_alpha_sentinel(a_stats)
                # Map verdict -> human-readable diagnostic line that mirrors
                # v37's `check_alpha_sentinel()` print format so the two
                # helpers can be cross-referenced.
                if verdict == "saturated":
                    print(f"    [v40-alpha] PASS (dispatch body ran; "
                          f"alpha saturated {frac_high * 100.0:5.1f}%)")
                elif verdict == "zero":
                    print(f"    [v40-alpha] FAIL alpha=0 "
                          f"(dispatch body never ran; bug is upstream)")
                elif verdict == "low":
                    print(f"    [v40-alpha] FAIL alpha=low "
                          f"(pre-v28 binary; sentinel not in compiled shader; "
                          f"parent must rebuild)")
                elif verdict == "mixed":
                    print(f"    [v40-alpha] FAIL alpha=mixed "
                          f"(partial dispatch; saturated={frac_high * 100.0:5.1f}% "
                          f"zero={frac_low * 100.0:5.1f}%)")
                else:
                    print(f"    [v40-alpha] verdict={verdict} (unrecognized)")
    except Exception:
        # Alpha inspection is best-effort; never let it block RGB reporting.
        pass


def discover_dump_dirs(data_dir: str) -> List[str]:
    """Return all directories under data_dir whose name starts with 'dumps', sorted."""
    out = []
    for entry in sorted(os.listdir(data_dir)):
        full = os.path.join(data_dir, entry)
        if os.path.isdir(full) and entry.startswith("dumps"):
            out.append(full)
    return out


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Per-channel pixel statistics for TestReSTIR_GI_Temporal dumps."
    )
    parser.add_argument(
        "--data-dir",
        default=os.path.dirname(os.path.abspath(__file__)),
        help="Directory containing dumps* subdirectories (default: script's own dir)"
    )
    args = parser.parse_args()

    data_dir = args.data_dir
    if not os.path.isdir(data_dir):
        print(f"ERROR: data dir does not exist: {data_dir}", file=sys.stderr)
        return 1

    print(f"dump_pixelstats.py (six-role-pipeline v24 + v40) — data dir: {data_dir}\n")

    dump_dirs = discover_dump_dirs(data_dir)
    if not dump_dirs:
        print(f"No dumps* directories found in {data_dir}.")
        print("Nothing to inspect. Run the test or run_rgi_diagnostic.sh first.")
        return 0

    total_files = 0
    for d in dump_dirs:
        print(f"[{os.path.basename(d)}]")
        files_in_dir = 0
        for label, pattern in CHANNEL_PATTERNS:
            for path in sorted(glob.glob(os.path.join(d, pattern))):
                emit_stats(label, path)
                files_in_dir += 1
        if files_in_dir == 0:
            print("  (no frame*.png in this directory)")
        else:
            print(f"  -> {files_in_dir} files")
        print()
        total_files += files_in_dir

    print(f"Total: {total_files} PNG files inspected across "
          f"{len(dump_dirs)} dump directory/directories.")
    return 0


if __name__ == "__main__":
    sys.exit(main())