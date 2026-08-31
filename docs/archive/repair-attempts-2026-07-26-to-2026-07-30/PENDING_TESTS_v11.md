# Pending Tests v11
- tests: docs/PENDING_COMMIT_v11.md (parent-driven test target: TestReSTIR_GI_Temporal)
- plan: docs/PENDING_PLAN_v11.md
- timestamp: 2026-07-27T09:00:00Z (estimated cron tick wall clock)

## Test surface

The v11 patch modifies 2 source files but produces 0 new test files. The existing test surface is unchanged:

- **TestReSTIR_GI_Temporal.cpp** — the test driver (modified by v11 patch; behavior unchanged when macro undefined)
- **FGIPass.cpp** — the production pass (modified by v11 patch; behavior unchanged when macro undefined)
- **validate_restir_gi.py** — the 3-check structural validator (unchanged; continues to apply against post-rebuild dumps)

## Test strategy (parent-driven)

The cron's terminal is blocked. The parent must run:

### Test 1: Build cleanliness (default macro)

```bash
./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal
```

Expected: clean build, no compile errors. The new `<iostream>` include is well-supported. The cerr writes are wrapped in `#ifdef HLVM_FORCE_CERR_LOGGING` which is undefined in the default Build.sh, so the cerr code is not compiled.

If this fails, the patch is wrong (the `<iostream>` include or the macro gating has a syntax error). Report back to cron with the build error log.

### Test 2: Run with default macro (v10 behavior verification)

```bash
cd Engine/Source/Runtime/Binary/Debug && HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal
```

Expected: identical behavior to v10. The log file should fire the v3 spdlog markers per frame IF the rebuild fixed source/binary mismatch (per v9's hypothesis). If markers STILL don't fire after this rebuild, the bug is more subtle than source/binary mismatch and v11c is the next path.

### Test 3: Run with macro defined (cerr verification)

```bash
CXXFLAGS=-DHLVM_FORCE_CERR_LOGGING ./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal
cd Engine/Source/Runtime/Binary/Debug && HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal 2>stderr.log
```

Expected: stderr.log contains 8 lines starting with `[RGI] Render() entry:` and 8 lines starting with `[RGI] FGIPass::DispatchRays() entry:`, with non-zero handles for CmdList, NvrhiDevice, SceneTLAS, OutputTex. If cerr lines appear in stderr but v3 spdlog lines do NOT appear in the log file, the bug is spdlog-level-filter; v11e is the next path.

### Test 4: Validator (unchanged)

```bash
python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py
```

Expected: same 3-check validator as before. If the v11 patch's diagnostic surface revealed a fix, the validator should now pass; if not, the same failure pattern as v10.

## Cron cannot execute any of these

The terminal tool is blocked by tirith (verified 11+ cron ticks). The cron relies on the parent to run tests 1-4 and report back. The v11 patch is dormant by default and reversible in one follow-up cycle; the cost of "patch is wrong" is low.

## Verifier posture

The testing-verifier role can only confirm that the test surface is correctly preserved. The actual test execution is parent-driven. The cron's KEEP verdict on tests reflects "the patch does not break the test surface" — not "the tests pass."

## What this test stage does NOT cover

- ❌ Does not run the actual build (terminal blocked)
- ❌ Does not run the actual test (terminal blocked)
- ❌ Does not run the validator (terminal blocked)
- ❌ Does not vision-analyze the dumps (no vision tool in cron role)
- ❌ Does not capture stderr.log (terminal blocked)

These are correctly deferred to the parent; the v11 cycle is doc-and-patch-only from cron's perspective.
