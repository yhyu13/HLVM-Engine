# Pending Plan v9 — diagnose "GIPass::DispatchRays never logged" + v6a branching

- task: continue v6a diagnostic refinement now that parent has run v5; surface the new finding that Pre-GIPass / FGIPass::DispatchRays log lines do not fire despite being present in source.
- source: no bundle — pure source/log analysis from `Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal.log` (parent run at 2026-07-27 00:07:01-00:07:08).
- approach: pure documentation / hypothesis-tree cycle (no code change). Document the new evidence, falsify v6a-1 and v6a-3, narrow v6a-2 hypothesis with one focused question for parent. NO source patch this cycle — patch is gated on parent answer.

## Evidence captured from parent's v5 run

The `TestReSTIR_GI_Temporal.log` (parent-driven, freshest) and the rotated `TestReSTIR_GI_Temporal_1.log` (older but same shape) both show the SAME pattern:

1. **gi_raw = R[0.000, 0.000] G[0.000, 0.000] B[0.000, 0.000]** (TestReSTIR_GI_Temporal.log:76). The dump's per-channel summary confirms the GI pass output texture contains nothing. **v6a branch is confirmed.**
2. **gbuffer_worldpos normalized per-channel — R[-15.228, 15.264] G[-11.811, 8.193] B[-14.291, 0.025]** (TestReSTIR_GI_Temporal.log:78). Real Sponza geometry in the GBuffer. Dump normalization works (commit `2fab7d6`).
3. **"A command list should be executed before it is reopened" warning fires every frame** (TestReSTIR_GI_Temporal.log:64-72). v5's HLVM-bypass removal did NOT eliminate this. The pattern is identical to pre-v5.
4. **ZERO `Pre-GIPass`, `Post-GIPass`, `FGIPass::DispatchRays ENTER`, `FGIPass: per-frame binding set`, or `FGIPass::DispatchRays EXIT` log lines** anywhere in the log (grepped across all 3 log files). Yet these log calls are present in source at TestReSTIR_GI_Temporal.cpp:435, 442 and FGIPass.cpp:473, 555, 564.
5. **8 frames ran** (display_frame8.png timestamp 00:07:06.775 + frame number in dump filename). v3 diagnostic logs would have fired 8 times each if the dispatch was reached.

## Why this is the v6a branch (decision matrix execution)

The decision matrix in `PENDING_PLAN_v6.md`:
- v5-fixed-everything → pipeline complete (v6d). **FALSIFIED**: command list warning still fires + gi_raw still 0.
- gi_raw non-zero but validator < 3/3 → v6b. **FALSIFIED**: gi_raw is 0, not non-zero.
- validator = 3/3 but display bad → v6c. **FALSIFIED**: validator never passed.
- gi_raw still 0,0,0 → **v6a (CONFIRMED)**.

Within v6a, three sub-hypotheses were originally proposed. Update each with the new log evidence:

### v6a-1 — Output texture recreation bug (LOW probability per PENDING_PLAN_v6.md)

The original rationale: "if v3's Pre-GIPass OutputTex handle ≠ v4a's dump gi_raw handle, then the test is dumping the wrong texture."

**v6a-1 now META-FALSIFIED by the missing-log evidence.** If the test-class's OutputTexture handle (created at TestReSTIR_GI_Temporal.cpp:922, assigned to Desc.OutputTexture at :410, stored in FGIPass::OutputTexture at FGIPass.cpp:567, dumped at :1635) was the bug, the Pre-GIPass log at :435 should still fire — it logs the handle regardless of whether the handle matches. The fact that Pre-GIPass doesn't fire means control flow doesn't reach :435, not that the handle is wrong. **v6a-1 is structurally unprovable from the missing-log evidence.**

### v6a-3 — slangc RT payload dead-strip (LOW-MEDIUM probability per PENDING_PLAN_v6.md)

The original rationale: "if RT instance IDs are correct but radiance is zero, slangc dead-strip of ClosestHit payload fields may be at play."

**v6a-3 now FALSIFIED.** slangc dead-strip would cause rays to fire but produce garbage values. It would NOT prevent DispatchRays from being called. The missing Pre-GIPass log proves DispatchRays is never called — so the dispatch never reaches the slangc-compiled shader anyway. **v6a-3 is structurally impossible from the missing-log evidence.**

### v6a-2 — nvrhi auto-barrier ordering bug (MEDIUM probability per PENDING_PLAN_v6.md)

The original rationale: "if v3's ENTER and EXIT both fire, and OutputTex matches, then the GI pass wrote but the dump's `copyTexture` reads stale storage."

**v6a-2 cannot be falsified by static analysis alone** — it requires observing the ENTER/EXIT logs to confirm dispatch actually fired. Those logs are missing, so v6a-2 is the ONLY remaining candidate hypothesis. But its mechanism ("pass wrote but dump reads stale") requires the dispatch to have run — which is what we're trying to prove happened. **v6a-2 is the candidate, but its acceptance criterion (dispatch ran + dump is stale) cannot be verified without logs firing.**

## New finding: Pre-GIPass / FGIPass::DispatchRays logs not firing

This is the v9-critical finding. Source has the log calls (verified via read_file at TestReSTIR_GI_Temporal.cpp:435, 442 and FGIPass.cpp:473, 555, 564). Binary's log file doesn't contain them. Yet the test runs to completion (frame 8 dumps successfully), so the program doesn't crash.

**Three possible explanations:**

(a) **Source/binary mismatch.** The binary at `Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal` was compiled before v3's diagnostic logs were added. The parent ran the binary; the binary doesn't have those log calls. This is the most likely explanation because:
   - The v3 patches were applied at some point after the prior binary was built
   - The parent's "verification run" log shows TestReSTIR_GI_Temporal.log lines from 00:07:01-00:07:08 with RenderGBuffer logs at the source line number 1500 (matching source) — but those lines were already in the prior binary
   - If the source matches what was compiled, then either Pre-GIPass was added AFTER the binary was built, OR the Pre-GIPass log is somehow filtered at runtime
   - The lack of build log artifacts for v3's recent patches (no fresh build_test_Debug_TestReSTIR_GI_Temporal.log with a recent timestamp) supports this

(b) **Runtime log-level filter.** The spdlog config sets LogTest to a level above `info` for the per-frame Render() scope. But other `LogTest, info` messages DO fire in the same log file (line 62-63 RenderGBuffer logs, line 73-82 dump logs). A selective filter on per-frame Render() block seems implausible.

(c) **Dispatch short-circuits BEFORE the log call.** The only paths inside DispatchRays that can return before the log call at line 473 are the two guards at lines 458-462 (bIsInitialized || RTPipeline.IsInitialized) and 465-468 (missing handles). Both would log a `warning` (`EARLY-RETURN`) or `err` (`missing required handles`) — but those don't appear either. So if explanation (c) were true, we'd see those early-return log lines. We don't.

**Therefore explanation (a) — source/binary mismatch — is the most consistent with the evidence.**

## Mechanically actionable fix (v9 hypothesis)

The cron's terminal is blocked by tirith. Without terminal, the cron CANNOT rebuild the binary or run a fresh verification. But the cron CAN apply a code patch that forces the missing log evidence to appear — IF the parent then rebuilds and runs.

**v9 proposal: NO source patch this cycle.** Reasoning:
- Patching source without parent rebuilding doesn't change the binary. The patch is invisible to the running binary.
- Patching source to add a fresh, distinct log message would only help IF parent rebuilds — but parent hasn't rebuilt since v5.
- The cron cannot verify the patch effect without terminal access.
- Adding more logs when v3's logs aren't firing is meta-problematic — the diagnostic surface itself is broken.

Instead, v9 documents this finding and asks parent for a 3-line answer:

1. **Did you rebuild after v5's patch?** (binary's mtime vs source's mtime would tell, but cron can't run `ls -la` due to tirith)
2. **What does `nm TestReSTIR_GI_Temporal | grep GIPass` show?** (binary contains the FGIPass symbol — but the v3 log strings being absent would prove (a) source/binary mismatch)
3. **Run with `HLVM_LOG_LEVEL=trace` and capture a fresh log** — would expose any selective info-level filtering and bypass spdlog's default level setting if (b) is the explanation

If parent confirms source/binary mismatch, the pipeline state-machine becomes:
- v10 = parent rebuilds + re-runs (mechanical action by parent, not cron).
- After fresh run, all v3 logs should appear.
- If logs STILL don't appear after a confirmed rebuild, that's a different bug entirely (the dispatch call is being elided by the optimizer — but that's far-fetched for a Debug build).

## diff_estimate

0 lines. Pure documentation cycle.

## skip_plan_review

no — v9 introduces a new diagnostic hypothesis (source/binary mismatch) that needs plan-criticer eyes before recording in PENDING_PICK.

## test_strategy

No tests needed — documentation-only cycle. Parent-driven verification remains the gate.

## risks

- Low. No code change. No regression risk.
- The v9 diagnosis (source/binary mismatch) might be wrong if a future parent run shows the logs firing after rebuild. In that case, v9 was wrong but harmless.
- A parent rebuild might reveal the log evidence is fine and the actual bug is elsewhere (v6a-2 or a new hypothesis). v9 explicitly doesn't foreclose that.

## files

- `docs/PENDING_PLAN_v9.md` (this file)
- `docs/PENDING_PLAN_REVIEW_v9.md` (plan-critique)
- `docs/PENDING_COMMIT_v9.md` (impl — note "0 lines changed")
- `docs/PENDING_IMPL_REVIEW_v9.md`
- `docs/PENDING_TESTS_v9.md`
- `docs/PENDING_TEST_AUDIT_v9.md`
- `docs/PIPELINE_HEALTH_2026-07-27.md` (append a new tick section)

## What parent must do

The v9 cycle has zero code change but requires the same parent actions to advance:

1. **Confirm whether the binary at `Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal` was rebuilt after v3's diagnostic logs were added to TestReSTIR_GI_Temporal.cpp:435, 442.** Run `ls -la Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp` and report.
2. **If not rebuilt, rebuild and re-run the test.** After rebuild, the next run's log should contain the Pre-GIPass / Post-GIPass / FGIPass::DispatchRays ENTER+EXIT markers.
3. **Paste the new log lines back to the cron.** Once logs are confirmed firing per frame, the next cycle (v10) can re-evaluate the v6a hypothesis tree based on whether:
   - Logs fire + gi_raw still 0 → some dispatch body error, inspect FGIPass.cpp logs for any `err` lines
   - Logs fire + gi_raw non-zero but wrong → v6a-2 (auto-barrier) is the active hypothesis; needs a barrier-insertion patch
   - Logs fire + gi_raw non-zero + display looks correct → pipeline complete (v6d)
   - Logs STILL don't fire after confirmed rebuild → unknown; requires deeper diagnosis

## Decision matrix based on parent's v9 evidence (forward-looking)

| Parent's answer | Next cycle action |
|-----------------|-------------------|
| Binary was rebuilt after v3 logs added, logs still missing | v10 = add an HLVM_ASSERT or std::cerr unconditional write at the start of Render() to confirm control flow. Diagnoses log-level filter vs dispatch elision. |
| Binary was NOT rebuilt, parent rebuilds + runs, logs now fire + gi_raw still 0 | v10 = investigate FGIPass.cpp's `err` log paths (RTPipeline not initialized, missing handles) — v6a-2 hypothesis becomes dominant |
| Binary was NOT rebuilt, parent rebuilds + runs, logs fire + gi_raw non-zero | pipeline may be complete or close to it (v6d) |
| Parent cannot rebuild | pipeline stuck at v9; cron records the structural limitation honestly |