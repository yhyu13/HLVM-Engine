# Pending Tests v6

- commit: docs/PENDING_COMMIT_v6.md
- files: Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py (unchanged)
- test_strategy: parent-driven v5 verification — the v6 cycle has no executable changes to test. The acceptance criteria from v5 still apply. v6's sub-plans (v6a/b/c/d) will be tested once triggered.
- rationale: v6 is a contingency cycle. The acceptance check is v5's acceptance check (parent-driven build + run + log capture + validator + vision). The stale comment fix at TestReSTIR_GI_Temporal.cpp lines 395-398 has no behavioral effect; it does not need a separate test.
- red_phase: N/A — no behavioral change in v6.
- green_phase: N/A — same reason. The "pass" is v5 verification.

## What the parent must verify (carried over from v5)

1. **Build the test**: `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test`. Should succeed (no shader changes, no API surface changes; only a comment update in v6).
2. **Run with the canonical env vars**: `cd Engine/Source/Runtime/Binary/Debug && HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal`.
3. **Inspect the fresh log** (`TestReSTIR_GI_Temporal.log`):
   - Should NOT contain "A command list should be executed before it is reopened" warnings.
   - Should contain v3's diagnostic markers per frame (Pre-GIPass, FGIPass::DispatchRays ENTER/EXIT/binding-set, Post-GIPass).
   - Should NOT contain `RenderGBuffer: post-waitForIdle` (removed by v5).
   - gi_raw normalized per-channel should be non-zero if v5 fix worked.
   - gbuffer_worldpos normalized per-channel should be unchanged.
4. **Run the validator**: `cd Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data && python3 validate_restir_gi.py`.
5. **Vision-analyze display_frame8.png**.
6. **Report the verification outcome** to the cron — triggers the matching v6 sub-plan (or pipeline-complete notification if v5 fixed everything).

## What v6 did NOT do

- Did not write new test files.
- Did not modify validate_restir_gi.py.
- Did not modify shaders, binding layouts, or renderer passes.
- Did not commit/push (cron rules).
- Did not trigger any v6a/b/c/d sub-plan (waiting on parent's v5 verification).