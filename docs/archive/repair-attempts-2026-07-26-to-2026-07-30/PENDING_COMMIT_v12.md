# Pending Commit v12
- plan: docs/PENDING_PLAN_v12.md
- files: Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp, Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp
- source: no bundle — pure on-disk patch synthesized from v11 patch + v9 evidence chain
- target: working tree (no commit, no push, no rewrite per user instruction)
- task: un-gate v11 cerr-patch (remove `#ifdef HLVM_FORCE_CERR_LOGGING` guards) so the next parent rebuild produces default-ON stderr diagnostic regardless of spdlog configuration
- verify: see "verify" section below — terminal-blocked in cron; verify is parent-driven
- skip_impl_review: no — patch changes source semantics (cerr writes now unconditional)
- produces_test_files: no — no test files produced or modified
- notes: patch is minimal (-4 lines total across 2 files); the v11 `<iostream>` includes are now load-bearing (not conditional); the cerr writes are informative (bIsInitialized, RTPipeline.Initialized, SceneTLAS ptr, OutputTex ptr, Frame) and fire BEFORE the early-return guards so they always run when the function body is entered

## Patch summary

**File 1: Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp**
- Removed `#ifdef HLVM_FORCE_CERR_LOGGING` (was at line 457)
- Removed `#endif` (was at line 469)
- Updated comment block (5 lines) to v12 explanation: default-ON cerr write, distinguish H-A (source/binary mismatch) from H-B (spdlog-level-filter)
- Net: -2 lines (1 `#ifdef` + 1 `#endif` removed; 5 comment lines replaced with 5 new comment lines, net 0)
- Verified via patch tool diff + read_file at offset 453-477

**File 2: Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp**
- Removed `#ifdef HLVM_FORCE_CERR_LOGGING` (was at line 379)
- Removed `#endif` (was at line 388)
- Updated comment block (4 lines) to v12 explanation
- Net: -2 lines (1 `#ifdef` + 1 `#endif` removed; 4 comment lines replaced with 4 new comment lines, net 0)
- Verified via patch tool diff + read_file at offset 375-394

**Total: -4 lines across 2 source files. 0 new lines added. Patch is subtractive on the macro guard, comment-update on the explanation.**

## Behavior change

**Before v12 (post-v11)**: cerr writes only fire when the binary is built with `-DHLVM_FORCE_CERR_LOGGING`. Default build has no cerr output.

**After v12**: cerr writes fire unconditionally on every dispatch. 8 frames × 2 cerr lines/frame = 16 cerr lines per 8-frame run. Each line ~150 bytes. Negligible.

The dispatch pipeline itself is unchanged. The v3 spdlog markers (Pre-GIPass, Post-GIPass, FGIPass::DispatchRays ENTER/EXIT, per-frame binding set OK) are unchanged. v5's HLVM-bypass removal is unchanged. Bug-088 fix at line 675 is unchanged. Bug-075 binding-layout split is unchanged. v3 ENTER/EXIT/binding-set/EARLY-RETURN/missing-handles spdlog calls are unchanged.

## Verification (parent-driven, terminal blocked in cron)

The cron cannot run the build or the test (terminal blocked by tirith "unknown" security issue, verified 12+ cron ticks). The parent must:

1. **Build the target**: `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal`
   - Expected: clean build, no compile errors. The v12 patch is unconditional; the v11 `<iostream>` includes are load-bearing.
   - If compile error, the patch is wrong (rare; syntax is straightforward).

2. **Run with default env vars and capture stderr**:
   ```
   cd Engine/Source/Runtime/Binary/Debug && \
     HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 \
     ./TestReSTIR_GI_Temporal 2>stderr.log
   ```
   - Expected: `stderr.log` contains 8 lines starting with `[RGI] Render() entry:` and 8 lines starting with `[RGI] FGIPass::DispatchRays() entry:` (16 cerr lines total)
   - Expected: `TestReSTIR_GI_Temporal.log` contains the v3 spdlog markers per frame IF H-A (source/binary mismatch) is true
   - Expected: `TestReSTIR_GI_Temporal.log` MISSING v3 spdlog markers IF H-B (spdlog-level-filter) is true
   - The gi_raw normalized per-channel log line — likely still R[0,0] G[0,0] B[0,0] (separate bug from the diagnostic surface)

3. **Vision-analyze `display_frame8.png`** for recognizable non-uniform Sponza geometry (parent-driven; read_file can't open PNG).

4. **Run validator**: `python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py`. Expected: same 3-check validator; same failure pattern as v11 unless the v12-patch-induced rebuild fixes something.

5. **Report v12 evidence back to cron** with one of:
   - "cerr fires + v3 spdlog NOW fire + gi_raw still 0" → H-A confirmed. Cron routes to v12a (investigate dispatch body).
   - "cerr fires + v3 spdlog STILL don't fire + gi_raw still 0" → H-B confirmed. Cron routes to v12e (spdlog config fix).
   - "cerr fires + v3 spdlog NOW fire + gi_raw non-zero + display correct + validator 3/3" → pipeline complete (v6d). Revert cerr writes in follow-up cycle.
   - "cerr does NOT fire after confirmed rebuild" → v12c investigates (control flow upstream).

## What v12 does NOT change

- Does NOT change the dispatch pipeline (v3 instrumentation, v5 HLVM-bypass removal, bug-088, bug-075 all preserved)
- Does NOT change the validator
- Does NOT change the test driver control flow
- Does NOT add new test files
- Does NOT commit or push (per user instruction)
- Does NOT touch any unrelated working-tree changes
