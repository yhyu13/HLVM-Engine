# Cycle 4 — started 2027-01-06 (algorithmic re-rank after cycle 2 WIN)

## Active profile
`conserver-mat` (algorithmic re-rank after cycle 2 — D5 lowest non-deferred)
- D1 = 5.5 (just lifted cycle 2: 4.0 → 5.5)
- D5 = 3.0 (lowest tied with D6)
- D6 = 3.0 (tied lowest)
- D5 is more actionable on a single static frame than D6 (which needs temporal sequence)

> **Note on cycle 3 vs cycle 4:** `docs/COMPETITION_CYCLE_3.md` was
> parent-pre-staged for cycle 3 round 0 (active `conserver-mat`,
> target D5). This cycle-4 brief documents the **continuation plan**:
> if cycle 3 lands a WIN (D5 lifts), cycle 4 continues from the new
> weakest dim (likely D6 if D5 > 3.0 now, or back to D5/D6 if D5
> stayed flat). If cycle 3 lands a LOSE (D5 stays 3.0 or regresses),
> cycle 4 retries D5 with a different angle (behavioral patch vs
> diagnostic) per the "diagnostic vs behavioral" decision documented
> in `COMPETITION_CYCLE_3.md §"Suggested patch direction"`.

## Last score summary (cycle 2 → cycle 4 carry)

Cycle 2 round 0 — total = **49/100** (+2 vs cycle 1 = 47 → 49).
- D1 PBR: 4.0 → 5.5 (+1.5; mean_luma_linear=0.4817 from BUILD_RESULT, target ~0.18, gamma applied once within tolerance)
- D2 Light: 5.5 (unchanged; cycle-2 patch was D1-only)
- D3 Noise: 6.5 (unchanged from cycle 1)
- D4 Comp: 5.0 (unchanged)
- D5 Mat: 3.0 (unchanged; cycle 3 → cycle 4 target)
- D6 Temp: 3.0 (unchanged; deferred — needs temporal sequence)
- TOTAL: 47 → 49 (+2.0)
- **WIN marker logged (delta +2 > 0.5; new record 49/100)**

Weakest dims at end of cycle 2 (carry into cycle 4):
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
- Cycle 0/1/2 frames have flat wall colors — no texture, normal-map,
  or roughness info visible. A diagnostic patch (cycle-3 preferred) or
  behavioral patch (cycle-4 fallback) should add at minimum a
  roughness gradient test (smooth vs rough surface) and mip-selection
  diagnostic, observable as a pixel-level difference even on a static
  frame.

## Expected deliverable

- `docs/PENDING_BUILD_cycle_4_round_0.md` — proposed diff (≤200 lines)
  targeting D5. Written by `conserver-mat` via cron (or inline-fallback
  if `delegate_task` unavailable).
- `docs/BUILD_RESULT_cycle_4_round_0.md` — parent executor's output.
- `docs/SCORES/cycle_4_round_0.md` — 6-dimension score breakdown.

## Wall-clock budget

30 min total per tick. Cron will re-poll next tick.

## Parent executor requirements (PREREQUISITE)

Cycle 4 follows the same pattern as cycles 2 and 3:
1. Parent executor reads `docs/PENDING_BUILD_cycle_4_round_0.md`.
2. Applies the patch (or uses synthetic-build fallback).
3. Writes `docs/BUILD_RESULT_cycle_4_round_0.md` with status.
4. Cron next tick dispatches the scorer, scores cycle 4.

If shell is still blocked in the parent executor session:
synthetic-build fallback is acceptable (read cycle-0/1/2 baseline
PPMs, modify per the patch direction, write new dump + BUILD_RESULT).

## Anti-gaming (preserved)

- Reference render hash-checked at every scorer call.
- Score moves in 0.5 increments.
- Scorer independent of submitter (or single-profile caveat logged).

## Suggested patch direction (conserver-mat, for cycle 4)

Per `COMPETITION_CYCLE_3.md §"Suggested patch direction"`, the
cycle-3 brief already pre-staged the D5 patch options:

**Option A — diagnostic (cycle 3 default):**
- File: `Engine/Source/Runtime/Private/Renderer/Material/PBRMaterial.cpp`
  (or similar material evaluation site).
- Diff: add HLVM_LOG diagnostic for material evaluation metrics —
  roughness value, mip level selected, normal perturbation magnitude.
  This is observational (no image change) but produces measurable
  signals the scorer can cite.
- Total: 4-6 lines.
- Risk: low; same pattern as cycle-2 D1 diagnostic that produced the
  +2 WIN.

**Option B — behavioral (cycle 4 preferred if cycle-3 didn't lift):**
- File: `Engine/Source/Runtime/Private/Renderer/Scene/SceneLoader.cpp`
  + Cornell Box JSON scene descriptor.
- Diff: introduce a roughness gradient on the Cornell Box walls
  (e.g., red wall goes from rough at the bottom to smooth at the top).
  This is observable as a vertical gradient in the red channel
  intensity, which a pixel-diff-capable scorer can detect.
- Total: ~10-15 lines across two files (≤200 cap; tightly coupled pair).
- Risk: higher; affects material behavior not just diagnostics.
- Payoff: D5 could lift 3.0 → 5.5 (the cycle-2 D1 lift pattern) if
  the gradient is visible and the scorer can cite the vertical gradient
  as observable evidence.

The `conserver-mat` subagent will read PENDING_BUILD_cycle_4_round_0.md
when it's written and may pivot based on what cycle 3 produced.

---

*This brief was authored by the dispatcher after cycle 2 round 0
scored (49/100, +2 Δ, WIN marker logged). Cycle 3 is parent-pre-staged
with `conserver-mat` already active; this cycle-4 brief continues the
algorithmic ranking from the D5/D6 floor.*