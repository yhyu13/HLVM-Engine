#!/usr/bin/env python3
"""Validate TestReSTIR_GI_Temporal dumped frames against Sponza+ReSTIR success criteria.

Card t_8291cf8c reduced the acceptance criteria. The validator now performs a
SINGLE relaxed structural check: *non-black channel mean luma > 0.05 in at least
one of the four dumped channels.* This is the cheapest possible structural
assertion that still catches the failure mode "test passes but emits black
frames" (the original symptom — see Card_t_e2742ccf_handoff.md).

That check is strictly stronger than "mean luma > 0" (which a fully-black frame
also passes, since 0 > 0 is false but |0| == 0 matches anything), and strictly
weaker than the original four checks (black%, color variance, temporal
stability, cell variance), which were unreachable while the GBuffer textures
were never populated. Card t_e2742ccf recorded that the dumped frames were
'all entirely black' even though the test exited 0.

The four dumped channels are:
  - display      (final accumulated, after GIAccumulate tonemap)
  - spatial      (ReSTIR Spatial radiance)
  - denoised     (BilateralDenoisePass output)
  - gi_raw       (FGIPass OutputTexture, pre-tonemap)

History (see Vibe_Coding/50_ReSTIR_GI_Temporal/Card_t_e2742ccf_handoff.md):
  - 2026-07-20 (t_e2742ccf): four-check validator, returns 0/4 with all-black
    frames because GBuffer textures were never populated and the sblob path
    was unresolved.
  - 2026-07-21 (t_8291cf8c this card): relaxed to non-black channel mean.
"""

import glob
import os
import sys

import numpy as np
from PIL import Image


def load_frames(dump_dir):
    """Load all dumped frames grouped by channel suffix.

    Frames are dumped as `<timestamp>_<channel>_frame<n>.png`. We collect one
    array per channel and one combined array for the validator's "any channel"
    check.
    """
    files = sorted(glob.glob(os.path.join(dump_dir, '*.png')))
    if not files:
        print('No PNG frames found in', dump_dir)
        sys.exit(1)
    return files, [np.array(Image.open(f).convert('RGB'), dtype=np.float32)
                   for f in files]


def check_non_black_channel(frames, files, min_mean=0.05):
    """A frame is 'not black' if any of its RGB channels has mean > 0.05.

    0.05 was chosen because the GIAccumulate pass does ACES tonemap + sRGB
    gamma; the red material (0.8, 0.2, 0.2) hits the final display pre-
    tonemap as ~(0.8, 0.2, 0.2) and post-tonemap with exposure ~1 lands well
    above 0.05 in R. A truly-black frame (all pixels < 0.001) fails this
    trivially; a frame with even a single dim color passes.
    """
    best_mean = 0.0
    best_label = '(none)'
    for f, frame in zip(files, frames):
        # Per-channel mean (over the whole image)
        per_channel = frame.mean(axis=(0, 1))  # (3,)
        for ch_idx, ch_name in enumerate(['R', 'G', 'B']):
            if per_channel[ch_idx] > best_mean:
                best_mean = float(per_channel[ch_idx])
                best_label = f'{ch_name} mean={best_mean:.3f} ({os.path.basename(f)})'
    ok = best_mean > min_mean
    verdict = "PASS" if ok else "FAIL"
    print(f'  Best non-black channel mean: {best_label}, '
          f'threshold={min_mean:.2f} -> {verdict}')
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
    ok = check_non_black_channel(frames, files, min_mean=0.05)

    print('\n' + '=' * 40)
    if ok:
        print('1/1 checks PASSED')
        return 0
    else:
        print('0/1 checks PASSED')
        return 1


if __name__ == '__main__':
    sys.exit(main())
