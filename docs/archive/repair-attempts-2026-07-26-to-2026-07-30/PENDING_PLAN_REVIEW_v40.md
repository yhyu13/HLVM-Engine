# Pending Plan Review v40 — extend dump_pixelstats.py to read alpha channel

## Verdict: KEEP

## Design soundness
- v40 closes a real, observable gap: `dump_pixelstats.py` (v24) reads `Image.open(path).convert("RGB")` at line 88, stripping the alpha channel. The v28 alpha-channel sentinel (`Output[pixel].w = max(..., 0.99994f)`) is therefore invisible to `dump_pixelstats.py` even though it would be visible to the v37-extended `validate_restir_gi.py`.
- The patch is symmetric to v37 (validator side): both helpers now report alpha stats from any RGBA-mode PNG. The 5-pattern verdict ladder (saturated / zero / mixed / low / unknown) mirrors v37's `check_alpha_sentinel()` ladder, so the two helpers classify evidence identically.
- The patch is purely additive: the existing RGB stats path is unmodified. Frame blocks gain an additional `[v40-alpha]` line after the existing R/G/B lines.

## Plan completeness
- The plan enumerates 5 known alpha shapes (saturated / zero / mixed / low / unknown), 4 routing branches keyed to the next-parent-rebuild evidence, and the precise line where the patch goes (after line 88 in `dump_pixelstats.py`).
- No new dependencies (numpy + PIL already required by v24). No new GPU/dispatch behavior. No test file changes.
- The plan does NOT cover what happens if PIL's PNG decoder strips alpha from non-standard PNGs (rare edge case). Acceptable: v24's RGB-mode reads already handle this gracefully (fall through to RGB-only block).

## Feedback for planner (FIX only)
- (none — plan is well-scoped)

## Risks acknowledged
- Single-head host caveat: same model writes all 6 roles. Verdicts are self-checks. Patch is purely additive so verdicts are reproducible.
- RGBA-mode availability: PIL returns "RGBA" for PNGs with alpha, "RGB" for PNGs without. The patch handles both modes (alpha block skipped with a one-line message if alpha not present).
- The patch only ADDS an alpha block after each frame's RGB block; the existing RGB stats behavior is byte-identical to v24.
- Cron is file-only; runtime verification is parent-driven.

## Verdict: KEEP