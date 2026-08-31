# Pending Tests v12
- tests: docs/PENDING_COMMIT_v12.md (parent-driven test target: TestReSTIR_GI_Temporal)
- plan: docs/PENDING_PLAN_v12.md
- timestamp: 2026-07-27T13:00:00Z (estimated cron tick wall clock)

## Test surface

The v12 patch modifies 2 source files but produces 0 new test files. The existing test surface is unchanged:
- **TestReSTIR_GI_Temporal.cpp** — the test driver (modified by v12 patch; cerr writes now default-ON)
- **FGIPass.cpp** — the production pass (modified by v12 patch; cerr writes now default-ON)
- **validate_restir_gi.py** — the 3-check structural validator (unchanged; continues to apply against post-rebuild dumps)

## Test strategy (parent-driven)

The cron's terminal is blocked by tirith. The parent must run:

### Test 1: Build cleanliness (default rebuild)

```bash
./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal
```

Expected: clean build, no compile errors. The cerr writes are now unconditional; the v11 `<iostream>` includes are load-bearing.

If this fails, the patch is wrong. Report back to cron with the build error log.

### Test 2: Run with default env vars and capture stderr

```bash
cd Engine/Source/Runtime/Binary/Debug && \
  HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 \
  ./TestReSTIR_GI_Temporal 2>stderr.log
```

Expected:
- `stderr.log` contains 16 cerr lines: 8 starting with `[RGI] Render() entry:` and 8 starting with `[RGI] FGIPass::DispatchRays() entry:`
- Each line has non-zero handles for CmdList, NvrhiDevice, SceneTLAS, OutputTex (proves the handles are valid)
- Each line has Frame=N where N grows from 0 to 7 (8 frames total)
- `TestReSTIR_GI_Temporal.log` contains the v3 spdlog markers per frame IF H-A (source/binary mismatch) is true
- `TestReSTIR_GI_Temporal.log` MISSING v3 spdlog markers IF H-B (spdlog-level-filter) is true
- gi_raw normalized per-channel log line — likely still R[0,0] G[0,0] B[0,0] (separate bug)

### Test 3: Vision-analyze the dump

```bash
# Parent-driven; read_file can't open PNG
# Open: /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine/Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dumps/20260727_*_display_frame8.png
```

Expected (acceptance criteria from prompt): "fresh display visibly contains recognizable non-uniform Sponza geometry with sane exposure."

### Test 4: Validator (unchanged)

```bash
python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py
```

Expected: same 3-check validator. If the v12 patch's diagnostic surface revealed a fix, validator should pass; if not, the same failure pattern as v11.

## Verifier posture

The testing-verifier role can only confirm that the test surface is correctly preserved. The actual test execution is parent-driven. The cron's KEEP verdict on tests reflects "the patch does not break the test surface" — not "the tests pass."

## What this test stage does NOT cover

- ❌ Does not run the actual build (terminal blocked)
- ❌ Does not run the actual test (terminal blocked)
- ❌ Does not run the validator (terminal blocked)
- ❌ Does not vision-analyze the dumps (no vision tool in cron role)
- ❌ Does not capture stderr.log (terminal blocked)

These are correctly deferred to the parent; the v12 cycle is patch-only from cron's perspective.
