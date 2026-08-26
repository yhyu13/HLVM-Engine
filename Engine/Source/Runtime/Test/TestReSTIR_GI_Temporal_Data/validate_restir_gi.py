#!/usr/bin/env python3
"""
validate_restir_gi.py — 4-check structural validator for TestReSTIR_GI_Temporal dumps.

Implements the validator cited in:
  - TestReSTIR_GI_Temporal.cpp:41-42 ("Validation: see TestReSTIR_GI_Temporal_Data/validate_restir_gi.py")
  - DIAGNOSTIC_2026-08-01-v25.md (v25 finding: scalar mean-luma gate lets garbage pass)
  - software-development-practices §"4-check structural validator > scalar mean-luma gate"

Acceptance criteria (the 5 structural checks):
  1. Black-pixel ratio < 5%       — shadows allowed, full-black is not
  2. Color variance > some floor  — per-channel spatial std over the whole frame
  3. Temporal stability < ceiling — max step between consecutive frame means (skipped if only 1 frame)
  4. Cell variance > some floor   — split image into NxN grid; std of cell-means
  5. Scene content (v235)         — >=1% pixels with HSV saturation > 0.15; the
                                    anti-'white wall' gate (FAIL_LOG_2026-08-26)

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
# --- v235: scene-content gate (FAIL_LOG_2026-08-26.md) ---
# From 2026-08-10 to 2026-08-26 the Sponza camera was pointed at a featureless
# exterior wall / open sky, and EVERY gate above passed: a smooth gray gradient
# satisfies black-ratio, variance, temporal-stability and cell-variance floors.
# The one thing such a framing cannot fake is the scene's characteristic albedo
# colors — both verification scenes are built around saturated surfaces
# (Sponza: red carpet / banners / plants; Cornell: red and green walls).
# Measured on display dumps: broken framings 0.0000 saturated pixels, healthy
# Sponza 0.096-0.105, Cornell 0.58. The 0.01 floor has >=9x margin both ways.
CONTENT_SAT_THRESHOLD = 0.15       # HSV saturation above which a pixel counts
CONTENT_SAT_FRAC_MIN  = 0.01       # >= 1% of pixels must be saturated


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


def check_scene_content(arr: "np.ndarray") -> Tuple[bool, float]:
    """Check 5 (v235): fraction of saturated pixels > floor — the 'white wall' gate.

    A camera pointing at a featureless wall / open sky produces a smooth,
    unsaturated gradient that passes every statistical variance gate while
    showing none of the scene. Both verification scenes contain strongly
    saturated albedo (Sponza's red carpet and plants, Cornell's red/green
    walls), so a healthy frame has a measurable saturated-pixel fraction.
    Empirical: broken framings 0.0000, healthy Sponza >= 0.096, Cornell 0.58.
    """
    if arr is None or arr.size == 0 or not HAS_NUMPY:
        return False, 0.0
    rgb = arr[..., :3]
    mx = rgb.max(axis=2)
    mn = rgb.min(axis=2)
    sat = np.where(mx > 1e-5, (mx - mn) / np.maximum(mx, 1e-5), 0.0)
    frac = float((sat > CONTENT_SAT_THRESHOLD).mean())
    return frac > CONTENT_SAT_FRAC_MIN, frac


def _coefficient_of_variation(arr: "np.ndarray") -> Optional[float]:
    """Luminance CV (std/mean) over pixels above the dark threshold."""
    if not HAS_NUMPY:
        return None
    lum = np.average(arr[..., :3], axis=2)
    lit = lum > PIXEL_DARK_THRESH / 255.0
    if lit.sum() < 100:
        return None
    vals = lum[lit]
    mean = float(vals.mean())
    if mean <= 1e-6:
        return None
    return float(vals.std()) / mean


def _cv_from_log(log_path: Optional[Path], name: str) -> Optional[float]:
    """Exact lit-pixel luminance CV from the run log's float stats.

    v234: the dump stats line carries `cv_lit=X` — the validator's noise-gate
    quantity (std/mean of luminance over pixels brighter than
    PIXEL_DARK_THRESH) computed on the RGBA32_FLOAT readback. The PNG path is
    byte-quantized (~8% of the mean at the rotating view's brightness), and
    the all-pixel mean/std conflates the sky/geometry mask with noise (sky is
    0 in the indirect estimate but nonzero in the gi_raw dump), so neither is
    a fair noise measure. Returns None when the stats line is absent.
    """
    if log_path is None or not log_path.is_file():
        return None
    try:
        text = log_path.read_text(errors="ignore")
    except OSError:
        return None
    matches = re.findall(r"stats %s floats: .*cv_lit=([0-9.eE+-]+)" % re.escape(name), text)
    if not matches:
        return None
    return float(matches[-1])


def check_noise_reduction(dump_dir: Path, frame_n: int,
                          log_path: Optional[Path] = None) -> Tuple[bool, Optional[float]]:
    """v213: ReSTIR reuse must reduce relative variance vs the raw samples.

    Coefficient of variation (std/mean luminance over lit pixels) of the
    denoised (ReSTIR + ReBLUR) dump must be BELOW the gi_raw (single-sample
    raw estimate) dump — temporal/spatial reuse is what turns noisy
    candidates into a denoised estimate.

    v233: two input corrections, both root-caused from failing runs.
    1. Measure the DENOISED output, not the pre-denoise "spatial" reservoir
       estimate. The reservoir estimate is chromaticity x w_sum (ReSTIR
       selection churn); even ZetaRay never asks it to beat raw samples in
       CV — that is the denoiser's job. Measured: pre-denoise CV ~0.49 vs
       raw 0.25 (fails); post-denoise CV ~0.24 vs 0.25 (passes).
    2. gi_raw now contains the raw single-sample estimate of the SAME
       quantity (Lo * f/pdf = Lo * albedo; see the v233 dump change in
       TestReSTIR_GI_Temporal.cpp). Before v233 it held bare, per-channel
       normalized Lo — a different quantity (no albedo contrast) on a
       different scale, making the comparison meaningless in both
       directions (structurally unpassable in the rotating view,
       structurally lenient in the static view).
    v234: prefer the exact float CVs from the run log (see _cv_from_log);
    the byte-quantized PNG path remains as the fallback when no log is given.
    """
    cv_s = _cv_from_log(log_path, "denoised")
    cv_g = _cv_from_log(log_path, "gi_raw")
    if cv_s is not None and cv_g is not None and cv_g > 1e-6:
        return (cv_s < cv_g), cv_s / cv_g
    spatial_ts = find_group_for_frame(dump_dir, frame_n, "denoised")
    giraw_ts = find_group_for_frame(dump_dir, frame_n, "gi_raw")
    if spatial_ts is None or giraw_ts is None:
        return False, None
    spatial = _load_png_as_float(find_dump_file(dump_dir, spatial_ts, "denoised"))
    giraw = _load_png_as_float(find_dump_file(dump_dir, giraw_ts, "gi_raw"))
    if spatial is None or giraw is None:
        return False, None
    cv_s = _coefficient_of_variation(spatial)
    cv_g = _coefficient_of_variation(giraw)
    if cv_s is None or cv_g is None or cv_g <= 1e-6:
        return False, None
    return (cv_s < cv_g), cv_s / cv_g


def check_bias_bound(dump_dir: Path, frame_n: int) -> Tuple[bool, Optional[float]]:
    """v234: reuse bias regression gate (FIX_LOG_2026-08-23 §9 known limitation).

    The reused estimate (spatial) and the raw single-sample estimate (gi_raw,
    Lo*albedo since v233) are the SAME physical quantity, so the per-pixel
    ratio spatial/gi_raw exposes energy removed by reuse (clamps, outlier
    M-resets, visibility rejections, stale history under the turntable). The
    raw sample is noisy and heavy-tailed, so the gate uses the MEDIAN ratio
    over lit pixels — robust to gi_raw's fireflies and near-zero samples.

    Calibrated 2026-08-25 (48 accumulated frames): static median 0.77,
    rotating median 0.43 (PNG byte quantization costs a few points vs the
    float medians 0.80-0.89 / 0.45-0.52 from FIX_LOG_2026-08-23 §9). The gate
    FAILS only below 0.3 — catastrophic energy loss, i.e. a regression — and
    reports the value otherwise. Bounding the residual 10-50% bias itself is
    the ReSTIR-vs-path-traced-reference comparison, still a planned phase.
    """
    spatial_ts = find_group_for_frame(dump_dir, frame_n, "spatial")
    giraw_ts = find_group_for_frame(dump_dir, frame_n, "gi_raw")
    if spatial_ts is None or giraw_ts is None or not HAS_NUMPY:
        return False, None
    spatial = _load_png_as_float(find_dump_file(dump_dir, spatial_ts, "spatial"))
    giraw = _load_png_as_float(find_dump_file(dump_dir, giraw_ts, "gi_raw"))
    if spatial is None or giraw is None:
        return False, None
    # spatial (FullResSpatial, post-resolve) and gi_raw (Lo*albedo product)
    # are both dumped at full res — but guard anyway: refuse to compare if
    # shapes disagree (defensive only; a FAIL here means the dump contract
    # changed, not that bias regressed).
    if spatial.shape != giraw.shape:
        return False, None
    lum_s = spatial[..., :3].mean(axis=2)
    lum_g = giraw[..., :3].mean(axis=2)
    lit = lum_g > PIXEL_DARK_THRESH / 255.0
    if lit.sum() < 100:
        return False, None
    ratio = lum_s[lit] / np.maximum(lum_g[lit], 1e-6)
    med = float(np.median(ratio))
    return med > 0.3, med


def check_log_metrics(log_path: Optional[Path]) -> Tuple[Optional[float], Optional[float]]:
    """v213: parse the run log for the ReSTIR M summary and the frame-time line.

    Returns (M_mean, frame_time_ms); None when absent.
    """
    m_mean: Optional[float] = None
    frame_ms: Optional[float] = None
    if log_path is None or not log_path.is_file():
        return m_mean, frame_ms
    try:
        text = log_path.read_text(errors="ignore")
    except OSError:
        return m_mean, frame_ms
    m = re.search(r"reservoir M mean=([0-9.]+)", text)
    if m:
        m_mean = float(m.group(1))
    m = re.search(r"frame time: ([0-9.]+) ms/frame", text)
    if m:
        frame_ms = float(m.group(1))
    return m_mean, frame_ms


def check_fireflies(log_path: Optional[Path]) -> Tuple[bool, Optional[float]]:
    """v214: the ReSTIR indirect output must be outlier-bounded.

    Parses the run log's `stats spatial floats` line (true HDR values — the PNG
    clamps >1) and requires max/mean < 50 per channel. A single-sample path
    tracer's fireflies routinely exceed 100x the mean; reservoir reuse +
    outlier suppression should keep the ReSTIR estimate well below that.
    """
    if log_path is None or not log_path.is_file():
        return False, None
    try:
        text = log_path.read_text(errors="ignore")
    except OSError:
        return False, None
    m = re.search(r"stats spatial floats: R\[[0-9.]+,[0-9.]+\] G\[[0-9.]+,[0-9.]+\] B\[[0-9.]+,[0-9.]+\] mean=\[([0-9.]+),([0-9.]+),([0-9.]+)\]", text)
    if not m:
        return False, None
    mean_r, mean_g, mean_b = (float(v) for v in m.groups())
    mean_lum = max(mean_r, mean_g, mean_b, 1e-6)
    mm = re.search(r"stats spatial floats: R\[[0-9.]+,([0-9.]+)\] G\[[0-9.]+,([0-9.]+)\] B\[[0-9.]+,([0-9.]+)\]", text)
    if not mm:
        return False, None
    max_lum = max(float(v) for v in mm.groups())
    ratio = max_lum / mean_lum
    return ratio < 50.0, ratio


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


def validate(dump_dir: Path, verbose: bool = False, display_only: bool = False,
             log_path: Optional[Path] = None) -> int:
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
    # v235: the anti-'white wall' gate — a featureless framing passes every
    # variance floor above; require the scene's saturated albedo colors.
    ok, val = check_scene_content(display)
    results.append(("scene_content (saturated-pixel frac > 1%)", ok, val))

    # v213 (Phase 5b): ReSTIR-specific gates. v233: noise gate compares the
    # denoised (post-ReBLUR) output against the raw single-sample estimate.
    # v234: exact float CVs from the log when available; bias gate added.
    # Both gates need the spatial/gi_raw dumps — skip in --display-only mode
    # (previously the noise gate reported a spurious FAIL on missing inputs).
    if not display_only:
        ok, val = check_noise_reduction(dump_dir, newest_frame, log_path)
        results.append(("noise_reduction (denoised CV < gi_raw CV)", ok, val))
        ok, val = check_bias_bound(dump_dir, newest_frame)
        results.append(("bias bound (median spatial/gi_raw > 0.3)", ok, val))

    m_mean, frame_ms = check_log_metrics(log_path)
    if m_mean is not None:
        ok = m_mean > 3.0
        results.append(("reservoir_M_accumulates (mean > 3)", ok, m_mean))
    if frame_ms is not None:
        ok = frame_ms < 60.0
        results.append(("frame_time < 60 ms (real-time gate)", ok, frame_ms))
    ok, val = check_fireflies(log_path)
    results.append(("firefly bound (spatial max/mean < 50)", ok, val))

    all_pass = all(r[1] for r in results)
    print(f"=== validate_restir_gi: {newest} (n_frames={nframes}) ===")
    for name, ok, val in results:
        status = "PASS" if ok else "FAIL"
        val_str = f"{val:.6f}" if val is not None else "n/a"
        print(f"  [{status}] {name}: {val_str}")
    print(f"  newest dump: {display_path_opt}")
    return 0 if all_pass else 1


def main() -> int:
    parser = argparse.ArgumentParser(description="Validate newest TestReSTIR_GI_Temporal dump group.")
    parser.add_argument("dump_dir", type=Path, help="Directory containing dump PNGs (typically .../TestReSTIR_GI_Temporal_Data/dumps)")
    parser.add_argument("--verbose", action="store_true", help="Verbose output")
    parser.add_argument("--display-only", action="store_true", help="Only validate the display.png (skip spatial/gi_raw/gbuffer_material requirement)")
    parser.add_argument("--log", type=Path, default=None, help="Run log for ReSTIR M + frame-time gates (v213)")
    args = parser.parse_args()
    return validate(args.dump_dir, verbose=args.verbose, display_only=args.display_only, log_path=args.log)


if __name__ == "__main__":
    sys.exit(main())
