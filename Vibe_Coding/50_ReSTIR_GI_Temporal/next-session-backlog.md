# Next-Session Backlog (2026-07-20)

Pending items discovered while finishing `50_ReSTIR_GI_Temporal`. Each is
mechanically-actionable; nothing here is `requires_human` and nothing is
currently `blocked`. Anything subjective stays out of this list.

## Status 2026-08-26 (v235) — NEW ITEMS from the wall-framing incident

See FAIL_LOG_2026-08-26.md for the full story. The v215 session's Sponza
evidence was a featureless wall; with the camera fixed, real content exposes:

### 1. (HIGH) ReSTIR spatial estimate loses ~63-94% energy on real Sponza content
- **Where:** `TestReSTIR_GI_Temporal_Data/ReSTIR_Spatial_cs.hlsl` (+ temporal
  pairwise-MIS in `ReSTIR_Temporal_cs.hlsl` as second suspect).
- **Symptom:** static 48f run: spatial mean 0.039 vs gi_raw mean 0.104;
  bias-gate median ratio 0.061; spatial dump is a black "starfield". M
  saturates 9.95/10. Cornell is healthy (9/9, 7.4% vs PT).
- **Ruled out:** M cap (A/B `HLVM_RGI_MAXM=30`: bias median 0.063, unchanged).
- **Next steps:**
  1. Bisect the uncommitted-then-committed shader changes on real content:
     (a) remove the wave-anti-firefly clamp, (b) remove W/w_sum clamps,
     (c) remove Jacobian clamp — each is a one-block revert + ShaderMake
     rebuild + 48f static run. The wave clamp divides by
     `WaveGetLaneCount()-1` regardless of active-lane count — check first.
  2. If shader reverts don't move it, A/B the `inout` TemporalResample2 path
     (force candidate count 1) on static content.
  3. Re-derive the bias gate's calibration on real content once fixed.

### 2. (MEDIUM) Rotating real content fails the noise gate (denoised/raw CV = 1.095)
- **Symptom:** rotating 48f: cv_lit denoised 0.4276 vs gi_raw 0.3905. The
  v233 gate calibration (0.24 vs 0.25) was measured on a wall and does not
  transfer. Static passes (0.68).
- **Next steps:** after item 1 is fixed, re-measure. If rotating still fails,
  decide whether the gate needs a tolerance for turntable-induced history
  rejection or whether it is measuring real reservoir churn.

### 3. (MEDIUM) Redo the Sponza ReSTIR-vs-path-tracing converged comparison
- The v215 "5.4%" number is wall-vs-wall (void). After item 1, rerun the
  256-frame ReSTIR vs PT-reference pair with the fixed camera and report
  honest MAE / rel-err.

### 4. (LOW) frame_time gate never fires
- The run log has no `frame time: X ms/frame` line, so the validator's
  real-time gate is silently skipped (m_mean/frame_ms are optional).
  Either emit the line from the test or drop the gate.

## Status 2026-08-25 — ALL ITEMS RESOLVED (see FIX_LOG_2026-08-25.md)

1. ~~(HIGH) `TestReSTIR_GI_Temporal` runtime path bug~~ — resolved long
   before 2026-08-25; the test has been green since the v210 rework.
2. ~~(MEDIUM) Cross-link `Vibe_Coding/51_PathTraceGI_Debug` from the
   README~~ — done 2026-08-25 (`## References` section added).
3. ~~(LOW) Untracked working-tree clutter~~ — done 2026-08-25
   (`.gitignore` now covers the agent/IDE state dirs).
4. ~~(LOW) ReBLUR alpha-channel mismatch~~ — FIXED 2026-08-25, not just
   documented: `ReSTIR_Spatial_cs.hlsl` now writes the selected sample's
   hit distance `|x2 - x1|` into alpha and `Resolve_cs.hlsl` propagates it
   (weighted average) instead of forcing 1.0. ReBLUR's `GetNormHitDist`
   and its `hitDist > 0` history validation now receive real data.

## What is NOT in this backlog (deliberately)
- Anything in `50_ReSTIR_GI_Temporal/claude.md` (the original "REBUILD FROM ASH" diagnosis) — superseded by the corrected compute shaders now on disk.
- Items in `51_PathTraceGI_Debug/` — already completed and committed; backlinks only.
- Any `requires_human` / `blocked` state — intentionally not touched.
