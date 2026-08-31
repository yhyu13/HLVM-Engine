# Pending Commit v11
- plan: docs/PENDING_PLAN_v11.md
- files: Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp, Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp
- source: no bundle — pure on-disk patch synthesized from PENDING_PLAN_v10.md (v10a cerr-patch sketch)
- target: working tree (no commit, no push, no rewrite per user instruction)
- task: apply v10a cerr-patch (dormant stderr writes gated by HLVM_FORCE_CERR_LOGGING)
- verify: see "verify" section below — terminal-blocked in cron; verify is parent-driven
- skip_impl_review: no — patch changes source (incl. 2 new `<iostream>` includes) even though default behavior is unchanged
- produces_test_files: no — no test files produced or modified
- notes: patch is dormant by default; binary behavior identical to v10 unless `HLVM_FORCE_CERR_LOGGING` is defined at compile time

## Patch summary

**File 1: Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp**
- +1 line: `#include <iostream>` added to standard library include block (after `<fstream>`, before `<vector>`)
- +13 lines: `#ifdef HLVM_FORCE_CERR_LOGGING` block at top of `FGIPass::DispatchRays()`, BEFORE the v3 EARLY-RETURN guard
- Net: +14 lines, 0 lines of behavior change when macro undefined

**File 2: Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp**
- +1 line: `#include <iostream>` added to standard library include block (after `<fstream>`, before `<thread>`)
- +10 lines: `#ifdef HLVM_FORCE_CERR_LOGGING` block at top of `Render()`, BEFORE the NvrhiDevice/Framebuffer early-return guard
- Net: +11 lines, 0 lines of behavior change when macro undefined

**Total: +25 / -0 lines across 2 source files.**

## Verification (parent-driven, terminal blocked in cron)

The cron cannot run the build or the test (terminal blocked by tirith "unknown" security issue, verified 11+ cron ticks). The parent must:

1. **Build the target**: `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal`
   - Expected: clean build, no compile errors. The new `<iostream>` include is well-supported by all modern compilers and the cerr/cout symbols are part of the standard library. If the build fails on `<iostream>`, report back to cron with the build error — would indicate a configuration problem unrelated to the patch.
2. **Run the test**: `cd Engine/Source/Runtime/Binary/Debug && HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal`
   - Expected (macro undefined): identical to v10 run; no behavioral change. spdlog should fire the v3 markers per frame if the rebuild fixed source/binary mismatch.
3. **Optional: rebuild with macro defined**: `CXXFLAGS=-DHLVM_FORCE_CERR_LOGGING ./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal && cd Engine/Source/Runtime/Binary/Debug && HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal 2>stderr.log`
   - Expected: stderr.log contains `[RGI] Render() entry: ...` and `[RGI] FGIPass::DispatchRays() entry: ...` markers, one per frame, with bIsInitialized=true, RTPipeline.Initialized=true, and non-zero handles for SceneTLAS, OutputTex, CmdList.

## What the patch DOES NOT do

- ❌ Does NOT change any default runtime behavior (macro is undefined by default)
- ❌ Does NOT bypass Vulkan validation (only spdlog)
- ❌ Does NOT fix the gi_raw=0,0,0 symptom (that's downstream of the diagnostic surface)
- ❌ Does NOT auto-rebuild the binary (terminal blocked)
- ❌ Does NOT auto-run the validator (terminal blocked)
- ❌ Does NOT commit, push, or rewrite git history (per user instruction)

## Plan Deviations (impler fills this in if it deviated)

None. The patch is exactly the v10a sketch from PENDING_PLAN_v10.md lines 47-57 + 91-94, adapted to the actual source file structure (added the missing `<iostream>` includes that v10a sketch omitted).

## File diff verification

- Pre-patch: 2 files, 0 cerr writes anywhere
- Post-patch: 2 files, 1 cerr write per function, both gated by HLVM_FORCE_CERR_LOGGING
- Pre-patch: 0 occurrences of `<iostream>` include in either file
- Post-patch: 1 occurrence of `<iostream>` include in each file (alphabetically ordered in the include block)
