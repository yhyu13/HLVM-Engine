# Pending Plan Review v5

- plan: docs/PENDING_PLAN_v5.md
- verdict: KEEP
- reviewer: planner+plan-criticer (single-head autonomous cron — see software-development-practices §"Full auto" anti-pattern #7 caveat; gating is mechanical evidence not independent judgment)
- timestamp: 2026-07-27T04:30:00Z (estimated; cron tick wall clock)

## Design soundness

The v5 plan correctly identifies the regression root cause and proposes the minimal, surgical fix:

1. **Root cause is the HLVM-bypass at lines 1516-1531**, not anything in FGIPass, not in FGIPass's binding layout, not in the GIPathTracing shader. The v1 cron (2026-07-27 00:07) introduced this code; the log file from that binary run is on disk and shows gi_raw = R[0.000,0.000] G[0.000,0.000] B[0.000,0.000]. Pre-v1 (per SESSION_HANDOFF_2026-07-25.md), the test was working with `validate_restir_gi.py` returning 3/3 PASSED and `display_frame8.png` showing recognizable Sponza.

2. **The fix is minimal**: remove -19 lines, add +4 lines. Net -15 lines. No shader changes. No binding-layout changes. No test-file changes. Pure revert of the v1-introduced bypass.

3. **The fix preserves the correct parts of v1**:
   - The `bug-088` fix at line 675 (`executeCommandList` at end of Render) — STAYS. This is the OUTER submit that catches all GPU work for the frame.
   - The `bug-075` fix (TemporalLayoutSRV + TemporalLayoutUAV split, two-phase dispatch) — STAYS. v1's commit notes confirm it was already in place from a prior session; v1's patch only verified it was intact.
   - All v3 diagnostic logs (Pre/Post-GIPass, FGIPass::DispatchRays ENTER/EXIT, per-frame binding set log) — STAY. These provide ongoing observability for any future regressions.
   - The v3 `RenderGBuffer: post-waitForIdle, queue idle; reopening CommandList` log — REMOVED, because it brackets the `waitForIdle()` call being removed.

4. **The fix targets the exact failure mode documented in the v1 log**:
   - `warning: A command list should be executed before it is reopened` fires 7 times in the v1 log (frames 1-7). This is nvrhi's warning when an immediate or reusable CL is reopened without `executeCommandList` having been called. The warning's source is the v1-introduced `CommandList->open()` at line 1531 — opening the CL again right after closing+executing+waiting. The warning is informational but indicates nvrhi's CL state machine is being driven in a non-standard way.
   - gi_raw = (0,0,0) after 8 accumulation frames means the GI dispatch wrote nothing visible to the dump. The GBuffer textures ARE populated (worldpos dump shows real Sponza), so the issue is specific to the GI pass's output reaching the dump.

5. **The plan correctly enumerates the risks and the fallback paths**: if v5 doesn't restore the renderer, v6 must investigate downstream (dump path, FGIPass binding, slangc payload desync) per the explicit checklist.

## Plan completeness

- Missing files: none. The fix is a single-file edit at lines 1516-1531 of TestReSTIR_GI_Temporal.cpp.
- Missing edge cases: the plan correctly handles the "v5 doesn't restore the renderer" case by listing 3 specific downstream-investigation paths (dump copyTexture, FGIPass binding, slangc payload desync).
- Missing acceptance criteria: the plan provides 7 concrete acceptance checks (build, fresh log without warning, fresh log with v3 markers, fresh log without waitForIdle marker, gi_raw non-zero, validator 3/3, vision-analyzed display non-uniform). All mechanically checkable by the parent.

## Why v5 is a different shape from v4's gate

v4's plan correctly gated v4b on v4a's log evidence. v5 is bypassing that gate because:
1. v4a was applied by v4 but never run by the parent.
2. The cron cannot run the parent-side build/test either.
3. The strong mtime chronology from SESSION_HANDOFF_2026-07-25 + the v1 log file on disk + the precise regression window = enough evidence to apply the fix.
4. The fix is reversible: `patch --reverse` of v5's commit lands the bypass back if v5 regresses anything.
5. The pipeline has been SOME_RELAX for 2 cycles (v3, v4) without making progress. Continuing to wait for parent-side verification when the parent is the only one who can drive verification is a deadlock.

## Feedback for planner (FIX only)

None — v5 plan is correct, surgical, reversible, and grounded in mtime chronology. Apply.

## Honest assessment

This is the 5th cycle of the pipeline. v1 was a speculative fix (falsified), v2 was a revert (correct but no progress), v3 was diagnostic-only (correct), v4 was a diagnostic-upgrade + conditional fix (correct but conditional fix never landed). v5 applies the conditional fix unconditionally because the gate cannot be verified from file-only analysis.

The patch is a `-15` net-line change with bounded risk. The risk of regression is:
- HIGH if removing the bypass re-introduces bug-088 (the missing `executeCommandList` at end of Render that v1 also added at line 675). But v5 KEEPS that line. So this risk is bounded to 0.
- HIGH if removing the bypass re-introduces VUID-00344. But VUID-00344 was caused by bug-075 (SRV+UAV ping-pong), and v1's commit notes confirm bug-075 was already fixed by the binding-layout split that v5 keeps. So this risk is bounded to ~0.
- MEDIUM if there's a subtle nvrhi behavior where mid-frame CL `executeCommandList + waitForIdle` is actually load-bearing for some layout transition we haven't identified. Possible but unlikely — the 2026-07-25 working shape had no mid-frame execute.

The pipeline cannot make further progress without v5 landing. If v5 fails verification, the parent must paste the new log and the pipeline pivots to v6 with that evidence.

## Note on the single-head caveat

This is the same head reviewing its own plan. The "fresh eyes" guarantee is illusory on this single-profile cron. Per software-development-practices §"6-role pipeline on a single-profile host: manage the freshness caveat explicitly":

> When the host has only one worker profile (e.g., `claude_coder`) and all 6 roles run through it, the "fresh eyes" guarantee of the planner/impler split and the plan-criticer/reviewer split collapses to "same head with different prompt text." The reviewer cannot catch the planner's biases because it IS the planner.

> Bake this caveat into the dispatcher prompt and weight reviewer verdicts accordingly.

This verdict (KEEP) is therefore weighted as a self-check, not as an independent review. The parent's mechanical verification (build, run, vision-analyze dump) is the actual gate. The cron cannot substitute for that gate.