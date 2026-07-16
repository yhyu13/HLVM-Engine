#!/usr/bin/env python3
"""Validate TestCornellBoxGI dumped frames against Cornell Box success criteria."""

import glob
import os
import sys

import numpy as np
from PIL import Image


def load_frames(dump_dir):
    files = sorted(glob.glob(os.path.join(dump_dir, '*.png')))
    if not files:
        print('No PNG frames found in', dump_dir)
        sys.exit(1)
    frames = [np.array(Image.open(f).convert('RGB'), dtype=np.float32) for f in files]
    return files, np.stack(frames, axis=0)  # (F, H, W, 3)


def check_no_black(frames, threshold=0.05):
    black = (frames == 0).all(axis=-1)
    black_pct = 100.0 * black.sum() / (frames.shape[1] * frames.shape[2])
    ok = black_pct < threshold
    print(f'  Black pixels: {black_pct:.2f}% (limit {threshold:.2f}%) -> {"PASS" if ok else "FAIL"}')
    return ok


def check_temporal_stability(frames, max_mean_delta=5.0, max_temporal_std_ratio=0.20):
    # Mean colour per frame
    means = frames.mean(axis=(1, 2))  # (F, 3)
    max_mean_change = np.abs(np.diff(means, axis=0)).max()
    mean_ok = max_mean_change < max_mean_delta
    print(f'  Max mean delta between frames: {max_mean_change:.2f} (limit {max_mean_delta:.1f}) -> {"PASS" if mean_ok else "FAIL"}')

    # Per-pixel temporal std (averaged over all pixels/channels)
    temporal_std = frames.std(axis=0).mean()
    mean_intensity = frames.mean()
    std_ratio = temporal_std / mean_intensity if mean_intensity > 0 else float('inf')
    std_ok = std_ratio < max_temporal_std_ratio
    print(f'  Mean temporal std: {temporal_std:.2f} / mean intensity {mean_intensity:.2f} = {std_ratio:.2%} (limit {max_temporal_std_ratio:.0%}) -> {"PASS" if std_ok else "FAIL"}')
    return mean_ok and std_ok


def check_intensity_range(frames, max_high_low_ratio=5.0):
    # High/low ratio of per-frame mean intensities
    mean_gray = frames.mean(axis=-1).mean(axis=(1, 2))
    ratio = mean_gray.max() / max(mean_gray.min(), 1e-6)
    ok = ratio < max_high_low_ratio
    print(f'  High/low mean intensity ratio: {ratio:.2f} (limit {max_high_low_ratio:.1f}) -> {"PASS" if ok else "FAIL"}')
    return ok


def check_color_bleeding(frames, red_threshold=15, green_threshold=15, min_floor_pixels=1000):
    """Look for red/green tinted pixels in the lower half (floor region)."""
    h, w = frames.shape[1:3]
    floor = frames[:, h // 2 :, :, :]  # lower half across all frames
    avg_floor = floor.mean(axis=0)  # (H/2, W, 3)

    r, g, b = avg_floor[:, :, 0], avg_floor[:, :, 1], avg_floor[:, :, 2]
    red_bleed = (r > g + red_threshold) & (r > b + red_threshold)
    green_bleed = (g > r + green_threshold) & (g > b + green_threshold)

    red_count = int(red_bleed.sum())
    green_count = int(green_bleed.sum())
    ok = red_count >= min_floor_pixels and green_count >= min_floor_pixels
    print(f'  Floor red-bleed pixels: {red_count}, green-bleed pixels: {green_count} -> {"PASS" if ok else "FAIL"}')
    return ok


def main():
    dump_dir = os.path.join(os.path.dirname(__file__), 'dumps')
    files, frames = load_frames(dump_dir)
    print(f'Loaded {len(files)} frames from {dump_dir}')
    print('Frame means:')
    for f, m in zip(files, frames.mean(axis=(1, 2))):
        print(f'  {os.path.basename(f)}: [{m[0]:.1f}, {m[1]:.1f}, {m[2]:.1f}]')

    print('\nValidation:')
    results = []
    results.append(check_no_black(frames))
    results.append(check_temporal_stability(frames))
    results.append(check_intensity_range(frames))
    results.append(check_color_bleeding(frames))

    print('\n' + ('=' * 40))
    if all(results):
        print('All checks PASSED')
        return 0
    else:
        print('Some checks FAILED')
        return 1


if __name__ == '__main__':
    sys.exit(main())
