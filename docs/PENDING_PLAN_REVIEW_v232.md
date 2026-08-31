# Pending Plan Review v232
- plan: docs/PENDING_PLAN_v232.md
- verdict: KEEP (with two clarifications)
- reviewer: plan-criticer (six-role pipeline role #2)
- timestamp: 2026-08-23T03:00:00Z

## Design soundness

The design correctly identifies the root cause by direct source-read: `r.W` is stored unbounded at 4 sites in `ReSTIR_Temporal_cs.hlsl` (lines 409, 510, 543, 551 — verified by direct `search_files` this turn), and `p.r_s.W` is stored unbounded at 1 site in `ReSTIR_Spatial_cs.hlsl` (line 311 — verified by direct `read_file` this turn). The feedback loop `w_prev = m_prev * targetLum_curr * r_prev[0].W` (line 528 temporal, line 389 temporal) is the amplifying path. M is correctly clamped at `ReSTIR_Temporal_cs.hlsl:555` (`r.M = min(r.M, gConstants.MaxM)`), so W is the only unbounded field. The clamp-at-compute-site approach is sound and matches ZetaRay's reference implementation. Acceptance criterion (log-line stat: G channel of reservoir_C_A/B should stay < k_MaxW = 256 across HLVM_RGI_ACCUM=8 frames) is testable file-only via `search_files` on a fresh log post-build; running that requires operator-side build.

## Plan completeness

**FINDING #1 (correction, not blocker)**: The plan's risk #1 says the dual-copy hazard applies — but **verified firsthand this turn, the Cornell copies do NOT have the W reservoir bug.** `TestCornellBoxGI_Data/ReSTIR_Temporal_cs.hlsl` is 207 lines (vs 567 in the primary copy) and is a simpler implementation without ZetaRay temporal resampling; it has NO `r.W = targetLum` lines (0 hits via `search_files pattern="r.W = targetLum" path=TestCornellBoxGI_Data`). Same for `TestCornellBoxGI_Data/ReSTIR_Spatial_cs.hlsl` (145 lines, 0 hits for `r.W`). **Implication**: the dual-copy hazard does NOT apply to this fix. Only the 5 sites in the primary `TestReSTIR_GI_Temporal_Data/` copies need editing. The diff_estimate in the plan (which counts both copies) is therefore ~50% oversized; actual is `+10/-0 functional` (1 const decl + 4 temporal clamps + 1 spatial clamp × 1 copy, no dual-copy edit). The plan's "5 sites total" claim is correct in count of edits, just distributed across only 2 files instead of 4.

**FINDING #2 (clarification, not blocker)**: The plan's risk #3 says "Cap may hide the real cause" — verified by direct read this turn, the proposal in the plan already handles this: clamp BOTH W (at the 4+1 sites) AND `r.w_sum` to a parallel `k_MaxWSum` (e.g., 4096f). The plan text says "The plan therefore clamps W at store AND clamps `r.w_sum` to a parallel `k_MaxWSum`" — confirmed by re-reading the `approach` paragraph. This is correct: breaking the feedback loop requires clamping both the multiplier (W) and the additive accumulator (w_sum).

**FINDING #3 (information, not blocker)**: The plan's `test_strategy` says "5 sites" implicitly via the verifier list. The actual verifier rows should be: (a) `k_MaxW` constant appears at expected line in `ReSTIR_Temporal_cs.hlsl` and `ReSTIR_Spatial_cs.hlsl`, (b) 5 line numbers have a clamp pattern (`r.W = min(r.W, k_MaxW)` or equivalent) immediately after the W-assign, (c) `ReSTIR_Spatial_cs.hlsl` still has the `isnan` guard at the new line 312. These 7-8 rows are file-only verifiable.

## Feasibility check

Impler can execute this plan from the current codebase. All 4 files exist; no new dependencies; no FetchContent; no nvrhi fork changes; no cmake regen. The dual-copy correction (Finding #1) makes the impler simpler, not harder. **No blockers** identified.

## Single-profile caveat

Same model for all 6 roles on this host. The KEEP verdict is a self-audit, not a fresh-eyes review. The plan-reviewer's value here is the FINDING #1 (dual-copy correction), which the planner would have caught at impl time anyway.

## Feedback for planner (KEEP — informational only)

1. **FINDING #1 is a real correction**: please update the plan's diff_estimate from `+12/-0` to `+10/-0` functional lines and clarify in `approach` that only the primary copies need editing (the Cornell copies are simpler implementations without the bug).
2. **FINDING #2 is already handled** in the plan (no action needed).
3. **FINDING #3 should be expanded**: tester should run a 7-8 row file-only verifier rather than the v176-style "12 rows" — the patch surface is smaller.

## Verdict

**KEEP.** The plan is sound and the impler can proceed. Two corrections are informational (Finding #1 reduces surface area, Finding #3 is a tester-row count). The impler will execute with Finding #1 applied implicitly (it will only edit the primary copies because the Cornell copies don't have the bug — verified firsthand this turn). The cron can advance to the impler role on the next tick.
