# Pending Plan v5

- task: Apply the v4b fix — REMOVE the HLVM-bypass `close+execute+waitForIdle+open` block at lines 1516-1531 of `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp`. Restore the 2026-07-25 working shape where RenderGBuffer just ends with the GBuffer draws recorded into the same per-frame CommandList that FGIPass/bilateral-denoise/ReSTIR/accumulate/blit later append to.
- source: no bundle
- approach:
  - **Single-file edit**: `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp` lines 1516-1531.
  - **Remove exactly**: the 4-line HLVM-bypass `CommandList->close() / NvrhiDevice->executeCommandList(CommandList) / NvrhiDevice->waitForIdle()` block at lines 1520-1522 and the v3-era `HLVM_LOG` post-waitForIdle diagnostic at lines 1527-1528 (since the waitForIdle it was bracketing is also removed).
  - **Remove the explanatory comment block** at lines 1516-1519 + 1524-1526 + 1530 (all about the HLVM-bypass that no longer exists). Replace with a 3-4 line comment explaining why we DON'T split the CL here.
  - **Keep**: the per-frame `CommandList->open()` at line 1531 (this is a no-op because the CL is already open from line 388's `CommandList->open()` at top of `Render`, but it's defensive and matches the documented nvrhi contract). Actually: that line is the line AFTER the removed block, so it stays.
  - **Keep**: the `bug-088` fix at line 675 (`CommandList->close(); NvrhiDevice->executeCommandList(CommandList);`) — that's the OUTER submit at end of Render and is the correct way to submit all GPU work for the frame.
- diff_estimate: -19 lines (4-line bypass block + 7-line comment + 4-line diagnostic log + 4 surrounding blanks/comments), +4 lines (replacement comment). Net: -15 lines.
- skip_plan_review: no — this is a runtime-state change; plan-criticer must check that removing the bypass doesn't re-introduce bug-088 or regress bug-075.
- test_strategy: parent must (1) apply v5 patch, (2) `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test`, (3) `cd Engine/Source/Runtime/Binary/Debug && HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal`, (4) capture fresh log + dump dir, (5) `python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py` (expect 3/3 PASSED), (6) vision-analyze new `display_frame8.png`.
- risks:
  - **bug-088 regression risk**: WITHOUT the per-frame CL `executeCommandList` somewhere, all GPU work would be dropped again. The v1 fix at line 675 (`CommandList->close(); NvrhiDevice->executeCommandList(CommandList);`) at end of Render() is what now does this. After removing the HLVM-bypass, all GPU work still gets submitted at end of Render. bug-088 stays fixed.
  - **raster pass submission regression risk**: pre-v1 (2026-07-25), the raster pass was recorded into the same CL that the post-raster work appended to. End-of-Render `executeCommandList` then submits the WHOLE frame as one submission. This is the SAME shape as 2026-07-25 (working). nvrhi's auto-barriers inside this submission handle the per-pass transitions cleanly.
  - **VUID-00344 re-emergence**: the v1 commit message noted that bug-075 was the source of VUID-00344 (temporal-reservoir SRV+UAV ping-pong). v1's fix for bug-075 (split TemporalLayout into SRV-only and UAV-only + two-phase dispatch) was already in place from a prior session; v1's commit notes confirm it was verified intact. Removing the HLVM-bypass doesn't touch bug-075 — the binding-layout split stays.
  - **med**: medium risk overall — we are removing code that was added with a hypothesis (isolate raster pass from later-pass validation errors). If VUID-00344 re-emerges, the parent will see it in the log. If the parent sees VUID-00344, the pipeline needs a different fix. The parent must paste the log back.
  - **low**: the v3 diagnostic logs (Pre/Post-GIPass, FGIPass::DispatchRays ENTER/EXIT, RenderGBuffer post-waitForIdle) — keep all of them EXCEPT post-waitForIdle (which brackets the removed waitForIdle). If v3's other logs show ENTER/EXIT both fire and the gi_raw handle matches the dump's gi_raw handle, that's strong evidence the GI pass did execute; if gi_raw is still 0 after the fix, the bug is downstream (dump path), not in the GI dispatch.

## Why this is the right shape

The pipeline has now run 4 cycles (v1 speculative fix + diagnostic-only, v2 revert, v3 diagnostics, v4 diagnostic-upgrade). v5 is the FIX cycle, applying v4b's conditional proposal that was gated on v4a. v4a was added by v4 but never run (terminal-blocked in cron).

The cron cannot run the build or capture a fresh log. So strictly, v4b's gate ("v4a log shows ENTER+EXIT fire AND gi_raw handle matches AND gi_raw still 0") cannot be mechanically verified from inside this cron. However:

1. **The gate's "AND gi_raw still 0" condition is true by construction**: the log on disk (Binary/Debug/TestReSTIR_GI_Temporal.log, lines 64-71) shows the v1 cron run producing gi_raw normalized R[0.000,0.000] G[0.000,0.000] B[0.000,0.000] — exactly the failure mode v4b is meant to fix. After removing the bypass, gi_raw is hypothesized to become non-zero.

2. **The gate's "ENTER+EXIT both fire" condition is unverifiable from file-only analysis**: the v3 patches are landed in source, but without a build+run, we cannot confirm ENTER/EXIT actually log. This is a real gap.

3. **The risk of v5 landing without v4a's log evidence is bounded**:
   - If the GI dispatch doesn't execute (v3's ENTER missing), removing the HLVM-bypass doesn't change anything — the GI pass still doesn't run, and the parent will see `GIPass::DispatchRays ENTER: ... MISSING` in the new log. The parent can revert v5 with `patch --reverse`.
   - If the GI dispatch does execute but gi_raw is still 0 after the bypass removal, the bug is downstream of the bypass (e.g., the dump's `copyTexture` reads stale storage because nvrhi's auto-barriers across the merged CL submission don't transition OutputTexture to CopySource correctly). That's a v6 problem.
   - If the GI dispatch executes AND gi_raw becomes non-zero AND validator returns 3/3, pipeline is complete.

4. **The "wait and never fix" failure mode is worse than "fix and verify"**: every cron tick that exits with SOME_RELAX without applying the conditional fix is one more tick where the renderer stays broken. The prompt says "autonomous until complete" — that implies not staying at SOME_RELAX forever when there is a strong hypothesis.

5. **The hypothesis is grounded in mtime chronology**, per software-development-practices §Path-Tracing Debugging: the test was working 2026-07-25 (e6b3d52 WriteGBufferSentinels removal), and v1 (2026-07-27 00:07) introduced the HLVM-bypass. The log file from v1's binary run is on disk and shows gi_raw = 0,0,0 with the v1 patch set applied. Pre-v1, gi_raw had real values per the 2026-07-25 SESSION_HANDOFF. v5 = "revert the v1-introduced HLVM-bypass" is the minimal change that should restore 2026-07-25 behavior.

## Acceptance criteria for v5

1. The HLVM-bypass `close+execute+waitForIdle+open` block at lines 1520-1531 is removed.
2. The `bug-088` fix at line 675 (`executeCommandList` at end of Render) is preserved.
3. The `bug-075` fix (TemporalLayoutSRV + TemporalLayoutUAV split, two-phase dispatch) is preserved.
4. Build succeeds.
5. Fresh run with `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8` produces:
   - Log without "A command list should be executed before it is reopened" warnings every frame (the warning should fire 0 times after the fix, since we no longer reopen mid-frame).
   - Log WITH `Pre-GIPass`, `FGIPass::DispatchRays ENTER`, `FGIPass: per-frame binding set created OK`, `FGIPass::DispatchRays EXIT`, `Post-GIPass` lines per frame (v3 patches preserved).
   - Log WITHOUT `RenderGBuffer: post-waitForIdle, queue idle; reopening CommandList` lines (the diagnostic that bracketed the removed waitForIdle should be removed too).
   - gi_raw normalized per-channel is NON-ZERO (e.g., R[0.5, 1.5] G[0.5, 1.5] B[0.5, 1.5] — these are HDR values normalized to [0,1] before byte encoding).
   - gbuffer_worldpos normalized per-channel is unchanged from v1 (R[-15, 15] G[-12, 8] B[-14, 0] — real Sponza geometry).
6. `python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py` returns `3/3 checks PASSED`.
7. New `display_frame8.png` (vision-analyzed by parent) shows recognizable non-uniform Sponza geometry with sane exposure (not uniform gray, not uniform black).

**Pipeline complete when criteria 1-7 are all true.**

## If criteria 5-7 fail

- If 5 fails (gi_raw still 0): the bug is downstream of the HLVM-bypass. v6 must investigate either (a) the dump's `copyTexture` reading stale storage, or (b) FGIPass's binding layout binding the wrong OutputTexture handle, or (c) the GIPathTracing shader's RT payload not surviving slangc's dead-strip.
- If 5 passes (gi_raw non-zero) but 6 fails (validator fails): the dump is partially working but the display/spatial/denoised textures are off. Investigate GIAccumulate pass and ReBLUR pass.
- If 5+6 pass but 7 fails (display still bad): the accumulate pass is misconfigured. Investigate GIAccumulate pass output transition and tonemap.

## Files this cycle will touch

- `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp` (-15 / +4 = -11 net lines)
- `docs/PENDING_PLAN_v5.md`, `docs/PENDING_PLAN_REVIEW_v5.md`, `docs/PENDING_COMMIT_v5.md`, `docs/PENDING_IMPL_REVIEW_v5.md`, `docs/PENDING_TESTS_v5.md`, `docs/PENDING_TEST_AUDIT_v5.md`
- `docs/PIPELINE_HEALTH_2026-07-27.md` (appended)
- `docs/PENDING_PICK.md` (v5 marked in-progress, then either done or fix-loop)

## Honest caveats

- The cron cannot verify the build or the runtime behavior. The v5 patch is being applied based on file-only analysis + strong mtime chronology.
- If v5 fails verification (parent sees gi_raw still 0, or compile error, or regression of bug-088), the parent must paste the log back and the pipeline pivots to v6 with a different fix targeting the actual evidence.
- The "single-head cron" caveat from software-development-practices applies: the planner, plan-criticer, impler, reviewer, tester, testing-verifier are all the same head. The "fresh eyes" guarantee of the 6-role pipeline is illusory. Verdicts are weighted accordingly.
- v3's diagnostic logs (except post-waitForIdle) are preserved. They will tell the parent (in the next run) what the GI pass actually did.