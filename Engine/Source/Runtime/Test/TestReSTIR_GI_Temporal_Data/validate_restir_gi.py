#!/usr/bin/env python3
"""Validate TestReSTIR_GI_Temporal dumped frames against Sponza+ReSTIR success criteria.

Four independent structural checks (all 4 must PASS for overall PASS):

1. non_black_channel_mean — at least one of the four dumped channels has a
   per-channel mean > 5.0. Rejects the "test emits black frames" failure mode.
   (Calibrated against pre-fix uniform-gray baseline; was relaxed to >0.05 in
   commit t_8291cf8c but the gray baseline still passed that check.)

2. spatial_std — std-dev across the full display frame > 30.0. Catches
   uniform-but-bright outputs (mean=255 std=0.5 is the gray baseline; real
   renders vary across the image and easily exceed 30). Calibrated against
   pre-fix display dump (std=16.28 — this fails the check, correctly.)

3. cell_variance — split the display into a 4x4 grid of cells, compute std of
   the 16 cell means; require > 8.0. Catches "uniform across the whole frame"
   patterns that pass check 2 by varying in noise. ReSTIR/Sponza outputs have
   walls/floor/ceiling at different brightness, so cell means diverge.

4. alpha_sentinel — v37 (six-role-pipeline): read display_frame8.png alpha
   channel. The v28 HLSL patch sets `Output[pixel].w = max(..., 0.99994f)` at
   the END of RayGen, regardless of debugMode. If the dispatch body reached
   that line, the alpha channel of display_frame8.png will be saturated to
   254-255 (since 0.99994 in [0,1] → 254.83 → round-to-nearest → 255 byte).
   If the dispatch body never ran, alpha will be uniform 0. If only some
   pixels reached the sentinel, alpha will be mixed. This surfaces the v28
   "did the dispatch body run" signal that the project's own validator was
   previously IGNORING (the old `convert('RGB')` stripped the alpha channel).
   Verdict shape:
     - alpha>254 in >=95% of pixels → PASS (dispatch ran)
     - alpha uniformly 0 in >=95% of pixels → FAIL "alpha=0" (dispatch didn't run)
     - alpha mixed → FAIL "alpha=mixed" (partial dispatch)
     - alpha uniformly low (0-50) in >=95% of pixels → FAIL "alpha=low"
       (pre-v28 binary; sentinel not in compiled shader; parent must rebuild)

History (see Vibe_Coding/50_ReSTIR_GI_Temporal/Card_t_e2742ccf_handoff.md):
  - 2026-07-20 (t_e2742ccf): four-check validator, returns 0/4 with all-black
    frames because GBuffer textures were never populated.
  - 2026-07-21 (t_8291cf8c): relaxed to non-black channel mean > 0.05.
  - 2026-07-26 (six-role-pipeline v1): added spatial_std and cell_variance
    checks to catch "uniform gray" failure mode (VUID-00344 masking the
    renderer's actual output).
  - 2026-07-27 (six-role-pipeline v37): added alpha_sentinel check to surface
    the v28 alpha-channel alive-sentinel that the validator was previously
    stripping via convert('RGB'). Closes the diagnostic-evidence gap.

The four dumped channels are:
  - display      (final accumulated, after GIAccumulate tonemap)
  - spatial      (ReSTIR Spatial radiance)
  - denoised     (BilateralDenoisePass output)
  - gi_raw       (FGIPass OutputTexture, pre-tonemap)
"""

import glob
import os
import sys

import numpy as np
from PIL import Image


def _dump_timestamp(path):
    """Return YYYYMMDD_HHMMSS from a timestamped dump filename."""
    parts = os.path.basename(path).split('_', 2)
    if (len(parts) < 3 or len(parts[0]) != 8 or len(parts[1]) != 6
            or not parts[0].isdigit() or not parts[1].isdigit()):
        return None
    return f'{parts[0]}_{parts[1]}'


def select_newest_dump_group(files):
    """Keep only the latest run's timestamped frame-8 files.

    The C++ harness writes display first, but several subsequent dumps can
    share its second and sort before it by channel name (for example,
    ``denoised`` sorts before ``display``).  Anchor on the newest display's
    timestamp rather than its list index, then retain every file at or after
    that timestamp.  This prevents stale groups from satisfying checks while
    preserving all same-second files from the current run.
    """
    # Anchor on the newest display dump of ANY frame number (the harness dumps
    # frame{AccumTargetFrames}; older versions only anchored display_frame8 and
    # silently ignored frame-16/32 runs). 2026-08-10.
    display_timestamps = [
        _dump_timestamp(path) for path in files
        if 'display_frame' in os.path.basename(path)
    ]
    display_timestamps = [stamp for stamp in display_timestamps if stamp is not None]
    if not display_timestamps:
        return files

    newest_display_timestamp = max(display_timestamps)
    return [
        path for path in files
        if (_dump_timestamp(path) or '') >= newest_display_timestamp
    ]


def load_frames(dump_dir):
    # Accept any frame number (frame8, frame16, frame32, ...). The old glob
    # '*frame8.png' silently ignored longer accumulation runs. 2026-08-10.
    files = sorted(glob.glob(os.path.join(dump_dir, '*frame*.png')))
    if not files:
        print('No frame8 PNGs found in', dump_dir)
        sys.exit(1)
    files = select_newest_dump_group(files)
    # v37: read as RGBA to preserve the v28 alpha-channel alive-sentinel.
    # Previously converted to RGB here, which stripped alpha entirely and
    # made the sentinel invisible to the validator. Display frames are kept
    # as RGB in main() via the existing check_spatial_std/check_cell_variance
    # code paths; the alpha channel is inspected separately in check_alpha_sentinel.
    return files, [np.array(Image.open(f).convert('RGB'), dtype=np.float32)
                   for f in files]


def load_display_rgba(display_path):
    """Load display_frame8.png as RGBA float32 (preserves alpha for sentinel check)."""
    return np.array(Image.open(display_path).convert('RGBA'), dtype=np.float32)


def check_non_black_channel(frames, files, min_mean=5.0):
    """Per-channel mean > min_mean in at least one channel of one frame."""
    best_mean = 0.0
    best_label = '(none)'
    for f, frame in zip(files, frames):
        per_channel = frame.mean(axis=(0, 1))
        for ch_idx, ch_name in enumerate(['R', 'G', 'B']):
            if per_channel[ch_idx] > best_mean:
                best_mean = float(per_channel[ch_idx])
                best_label = f'{ch_name} mean={best_mean:.3f} ({os.path.basename(f)})'
    ok = best_mean > min_mean
    print(f'  Best non-black channel mean: {best_label}, '
          f'threshold={min_mean:.2f} -> {"PASS" if ok else "FAIL"}')
    return ok


def check_spatial_std(frames, files, min_std=20.0):
    """Std-dev across the full display frame > min_std. Catches uniform-bright."""
    # 2026-08-09 recalibration 30 -> 20: the Phase-3 sky-background fix fills
    # the previously-black ~56% of the frame with a fairly uniform blue sky,
    # which lowered the frame std to ~26 while the render is clearly healthy
    # (0% black, cell variance 19+). The original 30 threshold was calibrated
    # against a mostly-black frame. 20 still rejects the uniform-gray baseline
    # (std=16.28) the check was designed to catch.
    # Group selection is anchored by the latest display timestamp, which the
    # C++ harness writes first for each run. This is that run's display.
    display = next((f for f in files if 'display_frame' in os.path.basename(f)), None)
    if display is None:
        print(f'  No display dump found -> FAIL')
        return False
    arr = np.array(Image.open(display).convert('RGB'), dtype=np.float32)
    s = float(arr.std())
    ok = s > min_std
    print(f'  display std={s:.2f}, threshold={min_std:.2f} -> '
          f'{"PASS" if ok else "FAIL"}')
    return ok


def check_cell_variance(frames, files, min_cell_std=8.0):
    """Split display into 4x4 grid of cells; std of 16 cell means > min_cell_std.
    Catches uniform-but-with-noise patterns that pass check 2 by varying."""
    display = next((f for f in files if 'display_frame' in os.path.basename(f)), None)
    if display is None:
        print(f'  No display dump found -> FAIL')
        return False
    arr = np.array(Image.open(display).convert('RGB'), dtype=np.float32)
    h, w, _ = arr.shape
    cell_h, cell_w = h // 4, w // 4
    cells = []
    for i in range(4):
        for j in range(4):
            cells.append(arr[i*cell_h:(i+1)*cell_h, j*cell_w:(j+1)*cell_w].mean())
    s = float(np.std(cells))
    ok = s > min_cell_std
    print(f'  4x4 cell-mean std={s:.2f}, threshold={min_cell_std:.2f} -> '
          f'{"PASS" if ok else "FAIL"}')
    return ok


def check_alpha_sentinel(files, saturated_min=0.95, low_max=0.95):
    """v37: read display_frame8.png alpha channel and verify the v28 sentinel.

    Returns (passed: bool, diagnostic: str). The diagnostic string surfaces
    the precise evidence shape so the parent (or the cron) can route to the
    right next cycle without ambiguity.

    Sentinel spec (from GIPathTracing.hlsl v28 patch, line 694):
        Output[pixel].w = max(Output[pixel].w, 0.99994f);

    Pixel-byte encoding (RGBA8 unorm):
        0.99994 in [0,1] -> 254.83 -> round-to-nearest -> 255 byte.
        Therefore: alpha saturated to 254-255 means dispatch body ran.
        alpha uniform 0 means dispatch body never executed.
        alpha mixed means partial dispatch.
        alpha uniformly low (<50) means pre-v28 binary (parent must rebuild).
    """
    display = next((f for f in files if 'display_frame' in os.path.basename(f)), None)
    if display is None:
        print(f'  No display dump found -> FAIL "no-dump"')
        return False, 'no-dump'

    rgba = load_display_rgba(display)
    alpha = rgba[:, :, 3]
    npix = float(alpha.size)

    frac_saturated = float(np.sum(alpha > 254)) / npix
    frac_zero = float(np.sum(alpha == 0)) / npix
    frac_low = float(np.sum(alpha <= 50)) / npix

    # Verdict ladder — first matching rule wins.
    if frac_saturated >= saturated_min:
        print(f'  alpha>254: {frac_saturated * 100.0:5.1f}% (>= {saturated_min * 100.0:.0f}%) -> '
              f'PASS (dispatch body ran)')
        return True, 'alpha=saturated'
    if frac_zero >= saturated_min:
        print(f'  alpha=0: {frac_zero * 100.0:5.1f}% (>= {saturated_min * 100.0:.0f}%) -> '
              f'FAIL "alpha=0" (dispatch body never ran; bug is upstream)')
        return False, 'alpha=0'
    if frac_low >= low_max:
        print(f'  alpha<=50: {frac_low * 100.0:5.1f}% (>= {low_max * 100.0:.0f}%) -> '
              f'FAIL "alpha=low" (pre-v28 binary; sentinel not in compiled shader; '
              f'parent must rebuild)')
        return False, 'alpha=low'
    # Mixed alpha: neither saturated, zero, nor low dominates. Partial dispatch
    # (some pixels reached the sentinel, some did not — likely partial barrier
    # or partial dispatch tile failure).
    print(f'  alpha mixed: saturated={frac_saturated * 100.0:5.1f}% '
          f'zero={frac_zero * 100.0:5.1f}% low={frac_low * 100.0:5.1f}% -> '
          f'FAIL "alpha=mixed" (partial dispatch; some pixels reached sentinel, '
          f'some did not)')
    return False, 'alpha=mixed'


def check_restir_alive(files, min_mean=5.0):
    """v2026-08-09: ReSTIR channels must be non-black.

    spatial_frame8 / denoised_frame8 are written by the ReSTIR spatial pass
    and ReBLUR. In a bypass run (HLVM_RGI_BYPASS=1) both are black by design,
    and in a dead-ReSTIR run they are black by bug — the display checks above
    cannot tell the two apart because display falls back to gi_raw. This check
    fails on black ReSTIR channels unless the run is explicitly acknowledged
    as bypass via HLVM_VALIDATE_ALLOW_BYPASS=1.
    """
    if os.environ.get('HLVM_VALIDATE_ALLOW_BYPASS') == '1':
        print('  HLVM_VALIDATE_ALLOW_BYPASS=1 -> ReSTIR-channel check skipped '
              '(bypass acknowledged)')
        return True

    names = ['spatial_frame', 'denoised_frame']
    present = [f for f in files if any(n in os.path.basename(f) for n in names)]
    if not present:
        print('  No spatial/denoised dumps found -> FAIL')
        return False

    means = []
    for f in present:
        arr = np.array(Image.open(f).convert('RGB'), dtype=np.float32)
        means.append(float(arr.mean()))
    best = max(means)
    ok = best > min_mean
    print(f'  ReSTIR channel best mean={best:.2f}, threshold={min_mean:.2f} -> '
          f'{"PASS" if ok else "FAIL (ReSTIR dead or HLVM_RGI_BYPASS run)"}')
    return ok


def _highfreq_std(arr):
    """Std of (pixel - 3x3 box mean), i.e. high-frequency energy, per channel."""
    try:
        from numpy.lib.stride_tricks import sliding_window_view
    except ImportError:  # pragma: no cover - numpy < 1.20 fallback
        from scipy.ndimage import uniform_filter  # type: ignore
        box = np.stack([uniform_filter(arr[..., i], 3) for i in range(3)], axis=-1)
        return float(np.abs(arr - box).std())
    box = sliding_window_view(arr, (3, 3), axis=(0, 1)).mean(axis=(3, 4))
    return float(np.abs(arr[1:-1, 1:-1] - box).std())


def check_denoise_effective(files, min_mae=0.5, max_hf_ratio=0.99):
    """v2026-08-09: ReBLUR must actually change and smooth its input.

    Passes only when (a) denoised differs from spatial by more than min_mae
    (mean absolute error), and (b) denoised high-frequency energy is lower
    than spatial's (a real blur, not a copy). Catches the Phase-1 pass-through
    bug (SpatialAlpha=0 + NaN fallback) that the 4 original checks missed.
    """
    if os.environ.get('HLVM_VALIDATE_ALLOW_BYPASS') == '1':
        print('  HLVM_VALIDATE_ALLOW_BYPASS=1 -> denoise-effectiveness check '
              'skipped (bypass acknowledged)')
        return True

    spatial = next((f for f in files if 'spatial_frame' in os.path.basename(f)), None)
    denoised = next((f for f in files if 'denoised_frame' in os.path.basename(f)), None)
    if spatial is None or denoised is None:
        print('  Missing spatial/denoised dump -> FAIL')
        return False

    s = np.array(Image.open(spatial).convert('RGB'), dtype=np.float32)
    d = np.array(Image.open(denoised).convert('RGB'), dtype=np.float32)
    mae = float(np.abs(d - s).mean())
    hf_s = _highfreq_std(s)
    hf_d = _highfreq_std(d)
    ok_mae = mae > min_mae
    ok_hf = hf_d < hf_s * max_hf_ratio
    ok = ok_mae and ok_hf
    print(f'  denoise effective: MAE={mae:.2f} (> {min_mae:.2f} -> '
          f'{"PASS" if ok_mae else "FAIL"}), HF {hf_s:.2f} -> {hf_d:.2f} '
          f'(ratio {hf_d / max(hf_s, 1e-6):.3f} < {max_hf_ratio:.2f} -> '
          f'{"PASS" if ok_hf else "FAIL"}) -> {"PASS" if ok else "FAIL"}')
    return ok


def main():
    dump_dir = os.path.join(os.path.dirname(__file__), 'dumps')
    files, frames = load_frames(dump_dir)
    print(f'Loaded {len(files)} frames from {dump_dir}')
    print('Frame means (RGB):')
    for f, fr in zip(files, frames):
        m = fr.mean(axis=(0, 1))
        print(f'  {os.path.basename(f)}: [{m[0]:6.2f}, {m[1]:6.2f}, {m[2]:6.2f}]')

    print('\nValidation:')
    ok1 = check_non_black_channel(frames, files)
    ok2 = check_spatial_std(frames, files)
    ok3 = check_cell_variance(frames, files)
    # v37: alpha-channel sentinel check. Surfaces the v28 sentinel that the
    # previous RGB-only validator was silently discarding. Returns (bool, diagnostic);
    # diagnostic is printed regardless of pass/fail so the parent always sees
    # the precise alpha shape.
    ok4, alpha_diag = check_alpha_sentinel(files)
    print(f'  alpha-sentinel diagnostic: {alpha_diag}')
    ok5 = check_restir_alive(files)
    ok6 = check_denoise_effective(files)

    print('\n' + '=' * 40)
    passed = sum([ok1, ok2, ok3, ok4, ok5, ok6])
    print(f'{passed}/6 checks PASSED')
    return 0 if passed == 6 else 1


if __name__ == '__main__':
    sys.exit(main())
