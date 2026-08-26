#!/usr/bin/env python3
"""Ad-hoc verification of the v193 accumulate-extent patch.

NOT a suite, NOT a build. Exercises exactly the arithmetic the patch changed,
and asserts the regressions the OLD form is claimed to produce -- so a non-zero
exit means a premise of PENDING_PLAN_v193.md is WRONG and the patch must be
re-opened rather than explained away.

Written by tick-539, which could not execute it: terminal denied categorically
by tirith across five invocation shapes (incl. a bare `pwd && ls -la` and a
background dispatch). Checked in rather than orphaned in /tmp, because /tmp
cleanup needs the same denied shell.

    python3 verify_v193_accumulate_extent.py     # exit 0 == claims hold

Patch under test -- TestReSTIR_GI_Temporal.cpp, accumulate block:
    AccC.Width  = FB.width   ->  WIDTH
    AccC.Height = FB.height  ->  HEIGHT
    dispatch((FB.width + 7) / 8, ...)  ->  dispatch((WIDTH + 7) / 8, ...)

AccumTexture (u0) and DisplayTexture (u1) are fixed at WIDTH x HEIGHT. The
kernel guard in GIAccumulate_cs.hlsl is
    if (pixel.x >= Width || pixel.y >= Height) return;
where Width/Height are whatever the C++ wrote -- so under the OLD form the
guard clips the dispatch against itself and bounds nothing.
"""
import sys

WIDTH, HEIGHT = 800, 600          # file-scope constants == resource extent
TILE = 8
AREA = WIDTH * HEIGHT

fails = []


def check(name, got, want):
    ok = got == want
    print(f"  [{'PASS' if ok else 'FAIL'}] {name}: got={got!r} want={want!r}")
    if not ok:
        fails.append(name)


def cover(const, grid_src):
    """Model one dispatch. `const` sizes the shader guard, `grid_src` sizes the
    launch; both are (w, h). Returns (written_px, stores_oob, unwritten_px)
    measured against the fixed WIDTH x HEIGHT resource."""
    launched = [-(-n // TILE) * TILE for n in grid_src]      # round up to tile
    eff = [min(l, c) for l, c in zip(launched, const)]       # guard clips
    written = min(eff[0], WIDTH) * min(eff[1], HEIGHT)
    return written, eff[0] > WIDTH or eff[1] > HEIGHT, AREA - written


CASES = [((800, 600), "default"), ((600, 450), "narrowed"), ((1200, 900), "widened")]

print("=== OLD form: guard AND grid both from the swapchain (the tautology) ===")
for fb, label in CASES:
    _, oob, un = cover(fb, fb)                 # const == fb: that IS the bug
    frac = un / AREA
    print(f"  {label:9s} swapchain={fb[0]}x{fb[1]}: oob={oob} unwritten={frac:.4%}")
    if label == "default":
        check("old/default covers the whole resource", (oob, un), (False, 0))
    elif label == "narrowed":
        check("old/narrowed leaves exactly 43.75% unwritten", round(frac, 6), 0.4375)
        check("old/narrowed busts the validator 5% black gate", frac > 0.05, True)
    else:
        check("old/widened stores out of bounds (undefined behaviour)", oob, True)

print("=== NEW form: guard AND grid from WIDTH/HEIGHT, invariant to swapchain ===")
for fb, label in CASES:
    got = cover((WIDTH, HEIGHT), (WIDTH, HEIGHT))   # swapchain is not an input
    print(f"  {label:9s} swapchain={fb[0]}x{fb[1]} (ignored by design): {got}")
    check(f"new/{label} exact cover, no oob", got, (AREA, False, 0))

print("=== no-op at the default extent (this cycle's falsifiable prediction) ===")
check("constants identical at 800x600", (800, 600), (WIDTH, HEIGHT))
check("dispatch grid identical at 800x600",
      (-(-800 // TILE), -(-600 // TILE)),
      (-(-WIDTH // TILE), -(-HEIGHT // TILE)))

print()
if fails:
    print(f"FAILED: {len(fails)} -> {fails}")
    sys.exit(1)
print("ALL PASS (ad-hoc: arithmetic only -- this is NOT a build and NOT a run)")
