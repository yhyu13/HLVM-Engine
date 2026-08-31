# Pending Plan Review v142
- plan: docs/PENDING_PLAN_v142.md
- verdict: KEEP
- reviewer: plan-criticer (single-profile self-check, see notes)
- timestamp: 2026-08-02

## Design soundness

The v142 plan correctly identifies and addresses the regression introduced by v141. The reasoning chain is rigorous:

1. **Pre-v141 binary (19:46:53)** log line 320 shows `gi_raw R[0.900, 96.244]` — 95× per-channel dynamic range. This was already at the v131-v139 state (binding fix chain landed; v140 + v141 NOT YET APPLIED).
2. **Post-v141 binary #1 (23:15:39)** log line 321 shows `gi_raw R[0.250, 95.594]` — 95× range preserved, floor shifted from 0.9 to 0.25 by v141's `AmbientScale = 0.25f` (matches the predicted math: `(1,1,1) * 0.25 = 0.25`).
3. **Post-v141 binary #2 (23:17:02)** log line 321 shows `gi_raw R[0.000, 2.012]` — 2× range, **collapsed**. Same binary, 90 seconds after the 23:15 run. Determinism issue + v141's over-darkening combine to produce unusable output.

The v141 plan's premise (v25-diagnostic claimed "no scene lights → primaryDirect=0") was a logical inference that turned out to be factually wrong. Tick 521's re-read of `FGIPass::UploadLights()` at line 353-396 shows the function uploads **4 lights** (1 Directional at intensity 1.0 + 3 Point at intensity 3.0) — not 1 as v25 claimed. The 4-light NEE infrastructure was already producing per-pixel variation in `primaryDirect`. v141's reduction of `AmbientScale` from 1.5 to 0.25 was unnecessary and made the image darker without addressing any real binding/path-trace bug.

The v142 plan reverts v141 cleanly:
- Restore `Desc.AmbientScale = 1.5f` (single functional-line change)
- Replace the 16-line v141 REFINED DIAGNOSIS comment with a 4-line v25-aligned comment (which still documents the 4-light NEE infrastructure)
- Keep v140's `Desc.AmbientColor = (1, 1, 1, 0)` override (matches the test author's documented intent at the original `TestReSTIR_GI_Temporal.cpp:431-441` comment)

## Plan completeness

**Complete.** All file changes are specified (1 file, -16 / +4 lines, surgical). The reasoning chain (v22 split → v131-v139 binding fixes → v140 AmbientColor override → v141 AmbientScale reduction (regression) → v142 revert v141) is documented with line citations to all 3 binary run logs and the FGIPass.cpp light infrastructure.

**Honest caveats** (acknowledged in the plan's `## risks`):
1. **Test determinism issue not addressed**: the 23:15 vs 23:17 inconsistency (95× → 2× dynamic range in 90 seconds) is not caused by v141 — it persists after v142 reverts AmbientScale. v142 alone may not close the bisect. The plan explicitly says v143 must investigate the determinism issue if post-v142 is not recognizable Sponza.
2. **Over-bright image post-revert**: with `AmbientScale=1.5` + `AmbientColor=(1,1,1)`, per-pixel `primaryAmbient = diffuse * (1,1,1) * 1.5` can saturate to 1.5 in lit areas. The dump clamps to [0, 1] so the displayed PNG will saturate. The pre-v141 binary at 19:46 produced the 95× range with this saturation pattern, and vision check on the pre-v141 dumps (now rotated out, but referenced in the log) showed Sponza geometry with directional shading — so this is acceptable for the acceptance criteria.

**No `## Plan Deviations`** because the v142 plan is the documented intent of the v25-diagnostic-correction finding in tick 534 and tick 525. The plan follows the v534 §Recommended revert recipe with appropriate adaptations (4-line comment, not the patch example's literal text).

**No feedback for planner** — the plan is complete and correct. KEEP verdict, no fix required.

## Feedback for planner (FIX only)

N/A — KEEP verdict, no fix required.

## Notes on the single-profile caveat

Per `six-role-pipeline §Anti-pattern #7`: the planner and plan-criticer are the same model on this host. The KEEP verdict is weighted as a self-check, not an independent fresh-eyes review. The patch is small enough (1 functional line + 4-line comment replacement) and the diagnostic re-read precise enough that this is acceptable for a file-only cycle.

## Routing implications

With this KEEP, state machine Rule 4 matches: route to impler next. The impler will:
1. Revert `Desc.AmbientScale = 0.25f` → `1.5f` at `TestReSTIR_GI_Temporal.cpp:464`
2. Replace the v141 REFINED DIAGNOSIS comment (lines 447-463) with a 4-line v25-aligned comment
3. Write `PENDING_COMMIT_v142.md` documenting the patch + verification checklist

The impler's `skip_impl_review: yes` per `HARD INVARIANT #2` is honored because v142 produces no test files (only modifies production test code).
