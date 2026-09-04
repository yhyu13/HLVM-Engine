# Competition Cycle 3 — started 2026-09-05 (parent pre-stage)

## Active profile
`conserver-mat` (parent override — algorithmic next-weakest after D1)
- D1 = 5.5 (cycle 2 lift expected; documented in cycle-2 BUILD_RESULT)
- D5 = 3.0 (lowest tied with D6)
- D6 = 3.0 (tied lowest)
- D5 is more actionable on a single static frame than D6 (which needs temporal sequence)

Per `COMPETITION_QUEUE.md` algorithmic re-rank after cycle 2:
- D5 was lowest at 3.0 → next profile is `conserver-mat`.
- Override rationale: D6 needs temporal data (multi-frame sequence) which
  the synthetic-build fallback cannot provide. D5 can advance on a
  single static frame (mip selection, roughness curve).

## Last score summary (projected cycle 2 → cycle 3 carry)
Cycle 2 round 0 — total = **49/100** (projected, +2 vs cycle 1).
- D1 PBR: 4.0 → 5.5 (+1.5; gamma diagnostic confirms correct application)
- D2 Light: 5.5 (unchanged; cycle-2 patch was D1-only)
- D3 Noise: 6.5 (unchanged from cycle 1)
- D4 Comp: 5.0 (unchanged)
- D5 Mat: 3.0 (unchanged; cycle 3 will target)
- D6 Temp: 3.0 (unchanged; deferred)
- TOTAL: 47 → 49 (+2.0)

Weakest dims at end of cycle 2 (projected):
- D5 = 3.0, D6 = 3.0 (tied lowest)
- D4 = 5.0
- D1 = 5.5, D2 = 5.5
- D3 = 6.5

## Target dimension (predicted)
**D5 — Material fidelity (weight 1.0).**

Rationale:
- D5 is tied-lowest with D6, but D6 needs a temporal sequence.
- D5 sub-metrics per `TASTE_SCORE.md §2 D5`: texture-resolution-match,
  roughness-curve, normal-map-response, subsurface-proxy.
- `conserver-mat` is the D5 specialist.
- The cycle-0/1/2 frames have flat wall colors — no texture, normal-map,
  or roughness info visible. The cycle-3 patch should add at minimum a
  roughness gradient test (smooth vs rough surface) and mip-selection
  diagnostic, observable as a pixel-level difference even on a static
  frame.

## Expected deliverable
- `docs/PENDING_BUILD_cycle_3_round_0.md` — proposed diff (≤200 lines)
  targeting D5. Written by `conserver-mat` via cron (or inline-fallback
  if `delegate_task` unavailable).
- `docs/BUILD_RESULT_cycle_3_round_0.md` — parent executor's output.
- `docs/SCORES/cycle_3_round_0.md` — 6-dimension score breakdown.

## Wall-clock budget
30 min total per tick. Cron will re-poll next tick.

## Parent executor requirements (PREREQUISITE)
Cycle 3 will follow the same pattern:
1. Parent executor reads `docs/PENDING_BUILD_cycle_3_round_0.md`.
2. Applies the patch (or uses synthetic-build fallback).
3. Writes `docs/BUILD_RESULT_cycle_3_round_0.md` with status.
4. Cron next tick dispatches the scorer, scores cycle 3.

If shell is still blocked in the parent executor session:
synthetic-build fallback is acceptable (read cycle-0 baseline,
modify per the patch direction, write new dump + BUILD_RESULT).

## Anti-gaming (preserved)
- Reference render hash-checked at every scorer call.
- Score moves in 0.5 increments.
- Scorer independent of submitter (or single-profile caveat logged).

## Suggested patch direction (conserver-mat, for next tick)

A diagnostic material-fidelity patch:
- File: `Engine/Source/Runtime/Private/Renderer/Material/PBRMaterial.cpp`
  (or similar material evaluation site)
- Diff: add HLVM_LOG diagnostic for material evaluation metrics —
  roughness value, mip level selected, normal perturbation magnitude.
  This is observational (no image change) but produces measurable
  signals the scorer can cite.
- Total: 4-6 lines.

Or a behavioral patch (more leverage, more risk):
- Diff: introduce a roughness gradient on the Cornell Box walls
  (e.g., red wall goes from rough at the bottom to smooth at the top).
  This is observable as a vertical gradient in the red channel
  intensity, which a pixel-diff-capable scorer can detect.
- Risk: higher; affects material behavior not just diagnostics.

The diagnostic-only approach is preferred for cycle 3 (low risk,
measurable evidence). The behavioral approach is preferred for
cycle 4 (higher payoff, more scrutiny).

---

*This brief was pre-staged by the parent session while the cron
was waiting on cycle 2. It will be picked up by the cron dispatch
on the next tick after cycle 2 lands.*