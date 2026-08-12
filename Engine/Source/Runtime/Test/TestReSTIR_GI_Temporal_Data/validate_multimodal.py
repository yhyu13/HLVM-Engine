#!/usr/bin/env python3
"""Multimodal structural judge for TestReSTIR_GI_Temporal dumps.

No mean/variance/noise-ratio scalars are used as pass gates. The picture is
judged by FOUR INDEPENDENT STRUCTURAL MODALITIES that must ALL pass, plus a
mandatory human visual review (M5):

  M1 GEOMETRY (edge alignment):   Sobel edges of the display must coincide
                                  with the geometry silhouette edges derived
                                  from the GBuffer worldpos. A gray blob or a
                                  flat image fails (few/no matching edges).
  M2 SPATIAL LAYOUT (occupancy):  the display's lit-pixel mask must match the
                                  GBuffer geometry mask (IoU). Wrong camera,
                                  black output, or uniform output fails.
  M3 CHROMATIC REGIONS (hue):     the display hue must follow the albedo hue
                                  from GBufferMaterial (GI = albedo x light
                                  preserves hue). Each albedo cluster's region
                                  must show its expected dominant hue; a
                                  grayscale or yellow-only image fails.
  M4 CHANNEL SANITY:              inside geometry no channel may be exactly
                                  zero everywhere, and the display must carry
                                  real color diversity (>= 4 distinct hues).
                                  Catches the (1,1,0) all-yellow bug class.
  M5 HUMAN REVIEW:                color-coded ASCII rendering of the display
                                  for the human to confirm Sponza structure.

Usage: python3 validate_multimodal.py [dump_dir]
Exit 0 only if M1-M4 all pass.
"""

import glob
import os
import sys

import numpy as np
from PIL import Image

import colorsys


def load_rgb(dump_dir, tag):
    files = sorted(glob.glob(os.path.join(dump_dir, '*%s_frame8.png' % tag)))
    if not files:
        return None
    return np.array(Image.open(files[0]).convert('RGB'), dtype=np.float32)


def sobel_magnitude(img):
    gx = np.zeros_like(img)
    gy = np.zeros_like(img)
    gx[1:-1, 1:-1] = (img[2:, 1:-1] - img[:-2, 1:-1]) / 2.0
    gy[1:-1, 1:-1] = (img[1:-1, 2:] - img[1:-1, :-2]) / 2.0
    return np.hypot(gx, gy)


def edge_mask(img, thresh):
    mag = sobel_magnitude(img)
    return mag > thresh * mag.max() if mag.max() > 0 else np.zeros_like(img, dtype=bool)


def f1(a, b):
    inter = np.logical_and(a, b).sum()
    denom = a.sum() + b.sum()
    if denom == 0:
        return 1.0 if a.sum() == 0 else 0.0
    return 2.0 * inter / denom


def iou(a, b):
    inter = np.logical_and(a, b).sum()
    union = np.logical_or(a, b).sum()
    return inter / union if union > 0 else 0.0


def hue_of(rgb):
    h, s, v = colorsys.rgb_to_hsv(rgb[0] / 255.0, rgb[1] / 255.0, rgb[2] / 255.0)
    return h if s > 0.08 else -1.0


def hue_dist(h1, h2):
    d = abs(h1 - h2)
    return min(d, 1.0 - d)


def check_m1_edges(display, geo_mask):
    lum = display.mean(axis=2) / 255.0
    d_edges = edge_mask(lum, 0.25)
    # Geometry silhouette edges: gradient of the mask (boundaries only).
    g = geo_mask.astype(np.float32)
    sil = np.zeros_like(g)
    sil[1:-1, 1:-1] = np.abs(g[2:, 1:-1] - g[:-2, 1:-1]) + np.abs(g[1:-1, 2:] - g[1:-1, :-2])
    g_edges = sil > 0.1
    score = f1(d_edges, g_edges)
    ok = score >= 0.45
    print('M1 geometry/edge alignment: edge F1 = %.3f (threshold 0.45) -> %s'
          % (score, 'PASS' if ok else 'FAIL'))
    return ok


def check_m2_layout(display, geo_mask):
    lum = display.mean(axis=2)
    lit = lum > 5.0
    score = iou(lit, geo_mask)
    ok = score >= 0.80
    print('M2 spatial layout: lit-mask IoU vs GBuffer geometry = %.3f (threshold 0.80) -> %s'
          % (score, 'PASS' if ok else 'FAIL'))
    return ok


def check_m3_chromatic(display, albedo, geo_mask, lum_thresh=10.0):
    # Cluster albedo pixels by quantized hue (the deterministic 8-color
    # palette); for each cluster with enough pixels, the display's dominant
    # hue inside that region must match the albedo hue.
    geo = np.where(geo_mask)
    alb_hues = np.array([hue_of(p) for p in albedo[geo]])
    disp_px = display[geo]
    disp_lum = disp_px.mean(axis=1)
    lit = disp_lum > lum_thresh

    clusters = {}
    for i, h in enumerate(alb_hues):
        if h < 0 or not lit[i]:
            continue
        key = round(h, 3)
        clusters.setdefault(key, []).append(i)

    results = []
    for key, idxs in clusters.items():
        if len(idxs) < 200:
            continue
        dh = np.array([hue_of(disp_px[i]) for i in idxs])
        valid = dh[dh >= 0]
        if len(valid) == 0:
            results.append((key, None, False))  # region lit but display hue absent
            continue
        dom = np.bincount((valid * 100).astype(int)).argmax() / 100.0
        d = hue_dist(key, dom)
        results.append((key, dom, d < 0.12))

    if not results:
        print('M3 chromatic regions: no albedo clusters found -> FAIL')
        return False
    passed = sum(1 for _, _, ok in results if ok)
    total = len(results)
    ok = passed / total >= 0.8
    print('M3 chromatic regions: %d/%d albedo clusters show their hue in the display '
          '(threshold 80%%, hue dist < 0.12) -> %s'
          % (passed, total, 'PASS' if ok else 'FAIL'))
    return ok


def check_m4_channels(display, geo_mask):
    geo = np.where(geo_mask)
    px = display[geo]
    lum = px.mean(axis=1)
    lit = lum > 5.0
    lit_px = px[lit]
    if len(lit_px) == 0:
        print('M4 channel sanity: no lit geometry pixels -> FAIL')
        return False
    per_ch = lit_px.max(axis=0)
    no_zero_channel = bool(np.all(per_ch > 0.0))
    hues = np.array([hue_of(p) for p in lit_px])
    n_hues = len(set(round(h, 2) for h in hues if h >= 0))
    diversity = n_hues >= 4
    ok = no_zero_channel and diversity
    print('M4 channel sanity: per-channel maxima = %s (no zero channel: %s), distinct hues = %d '
          '(>= 4: %s) -> %s'
          % (np.round(per_ch).astype(int).tolist(), no_zero_channel, n_hues, diversity,
             'PASS' if ok else 'FAIL'))
    return ok


def check_m5_human(display, geo_mask, width=96, height=32):
    """Color-coded ASCII: dominant-hue letter per cell (human review only)."""
    H, W, _ = display.shape
    letters = {0: 'R', 1: 'Y', 2: 'G', 3: 'C', 4: 'B', 5: 'M'}
    art = []
    for r in range(height):
        row = []
        for c in range(width):
            y0, y1 = int(r * H / height), int((r + 1) * H / height)
            x0, x1 = int(c * W / width), int((c + 1) * W / width)
            blk = display[y0:y1, x0:x1]
            lum = blk.mean()
            if lum < 5.0:
                row.append('.')
                continue
            avg = blk.reshape(-1, 3).mean(axis=0)
            h, s, v = colorsys.rgb_to_hsv(avg[0] / 255.0, avg[1] / 255.0, avg[2] / 255.0)
            if s < 0.10:
                row.append('#' if v > 0.5 else 'o')
            else:
                row.append(letters.get(int(h * 6) % 6, '?'))
        art.append(''.join(row))
    print('M5 human review — display color map (R=red Y=yellow G=green C=cyan B=blue M=magenta '
          '#=bright-gray o=dark-gray .=sky):')
    for line in art:
        print('   ' + line)


def main():
    dump_dir = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
        os.path.dirname(os.path.abspath(__file__)), 'dumps')
    display = load_rgb(dump_dir, 'display')
    albedo = load_rgb(dump_dir, 'gbuffer_material')
    normal = load_rgb(dump_dir, 'gbuffer_normal')
    if display is None or albedo is None or normal is None:
        print('Missing dumps (need display/gbuffer_material/gbuffer_normal). '
              'Run with HLVM_DUMP_RGI=1 first.')
        sys.exit(1)

    # Geometry mask from the (non-normalized) normal dump: sky pixels have no
    # fragments -> (0,0,0). The worldpos dump is per-channel NORMALIZED, so
    # sky pixels rescale to mid-range and must NOT be used as a mask.
    geo_mask = normal.mean(axis=2) > 1.0

    print('Multimodal structural judge — all four modalities must PASS:')
    ok1 = check_m1_edges(display, geo_mask)
    ok2 = check_m2_layout(display, geo_mask)
    ok3 = check_m3_chromatic(display, albedo, geo_mask)
    ok4 = check_m4_channels(display, geo_mask)
    check_m5_human(display, geo_mask)

    passed = sum([ok1, ok2, ok3, ok4])
    print('=' * 40)
    print('%d/4 structural modalities PASS' % passed)
    print('M5: human must visually confirm the color map reads as Sponza.')
    return 0 if passed == 4 else 1


if __name__ == '__main__':
    sys.exit(main())
