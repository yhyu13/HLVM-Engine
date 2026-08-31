# Pending Commit v5

- plan: docs/PENDING_PLAN_v5.md
- files: Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp
- source: no bundle
- target: working tree (no commit per cron rules)
- task: v5 — REMOVE the HLVM-bypass `close+execute+waitForIdle+open` block at lines 1516-1531 (originally lines 1516-1531 pre-v5) of TestReSTIR_GI_Temporal.cpp. Restore the 2026-07-25 working shape where RenderGBuffer just leaves the CommandList open so the post-raster work (FGIPass/bilateral/ReSTIR/accumulate/blit) appends into the same submission, and end-of-Render `executeCommandList` (preserved at line 675) submits the whole frame.
- verify: parent must run `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test` and `cd Engine/Source/Runtime/Binary/Debug && HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal` then check the fresh log + dump dir + validator result.
- skip_impl_review: no — this is a runtime-state change that interacts with nvrhi's CL state machine; the reviewer must check that bug-088's fix at line 675 is preserved and that bug-075's binding-layout split is not regressed.
- produces_test_files: no
- notes:
  - **Net change**: -14 lines removed, +8 lines added = -6 net lines. The patch replaces a 16-line HLVM-bypass block (4 lines of executable logic + 7 lines of explanatory comment + 4 lines of v3 diagnostic log + 1 line of "Reopen for the remaining passes" comment) with an 8-line NOTE comment explaining why we don't split the CL mid-frame.
  - **Preserved**: the bug-088 fix at line 675 (`CommandList->close(); NvrhiDevice->executeCommandList(CommandList);` at end of Render). Verified by reading the file at lines 672-677.
  - **Preserved**: v3's diagnostic logs (Pre/Post-GIPass at lines 435-443, FGIPass::DispatchRays ENTER/EXIT/binding-set in FGIPass.cpp). Only the post-waitForIdle log was removed (it bracketed the removed waitForIdle).
  - **Preserved**: bug-075's binding-layout split (TemporalLayoutSRV + TemporalLayoutUAV in FReSTIRPass.cpp/.h; HLSL declares `register(u0, space1)` and `register(u1, space1)`; two-phase dispatch from the C++ side).
  - **Preserved**: all shaders (GIPathTracing.hlsl, GIAccumulate_cs.hlsl, etc.) — v5 only touches C++ test code.

## Plan Deviations (impler fills this in if it deviated)

None — patch exactly matches v5's plan. Diff is -14/+8 (net -6 lines; my plan said -15 net, but the actual count is -6). The discrepancy is because my plan included removing an extra blank line and a trailing comment line that aren't in the file. The substantive change is identical: the HLVM-bypass logic is removed, and an explanatory comment replaces it.

## Implementation Evidence (impler fills this in)

- File modified: `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp` — lines 1516-1531 (pre-v5) replaced with 8-line NOTE comment.
- Verified by re-reading the file at lines 1505-1534 after the patch:
  - Line 1509-1514: GBuffer MRT → ShaderResource transitions (preserved).
  - Line 1516-1523: NEW NOTE comment explaining the v5 revert.
  - Line 1525-1533: subsequent GBuffer frame logs (preserved).
  - No `CommandList->close() / executeCommandList / waitForIdle / open` calls remain in RenderGBuffer.
- Build/run verification: BLOCKED by tirith in this cron (terminal approval denied for every command). Parent must drive the verify step.
- Expected post-build behaviour:
  - Fresh log should NOT contain "A command list should be executed before it is reopened" warnings (the warning fired 7 times in the v1 log).
  - Fresh log SHOULD still contain v3's diagnostic markers: `Pre-GIPass: ...`, `FGIPass::DispatchRays ENTER: ...`, `FGIPass: per-frame binding set created OK`, `FGIPass::DispatchRays EXIT: ...`, `Post-GIPass: returned Frame=N`.
  - Fresh log should NOT contain `RenderGBuffer: post-waitForIdle, queue idle; reopening CommandList` (the diagnostic that bracketed the removed waitForIdle is also removed).
  - gi_raw normalized per-channel SHOULD be non-zero (e.g., R[0.5, 1.5] G[0.5, 1.5] B[0.5, 1.5] — HDR values normalized to [0,1]).
  - gbuffer_worldpos normalized per-channel SHOULD be unchanged from v1 (real Sponza geometry: R[-15, 15] G[-12, 8] B[-14, 0]).
- Risk: if v5 regresses bug-088 (the missing-execute symptom returns), the parent must paste the new log back. The pipeline will pivot to v6 to investigate the actual evidence.

## What v5 explicitly does NOT do

- Does NOT change FGIPass.cpp or any other renderer code.
- Does NOT change any shader (GIPathTracing.hlsl, GIAccumulate_cs.hlsl, ReSTIR_*.hlsl, ReBLUR.hlsl, etc.).
- Does NOT change any binding layout (FReSTIRPass's TemporalLayoutSRV/UAV split stays).
- Does NOT change validate_restir_gi.py (the 3-check structural validator from v1 stays).
- Does NOT change the bug-088 fix at line 675 (verified intact).
- Does NOT commit/push (cron rules).