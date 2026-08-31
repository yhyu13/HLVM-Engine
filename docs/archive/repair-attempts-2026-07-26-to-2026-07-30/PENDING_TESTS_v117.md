# Pending Tests v117
- commit: docs/PENDING_COMMIT_v117.md
- tester: tester (role #5)
- timestamp: 2026-07-29

## Plan test strategy cited
Role #5 was required to establish an artifact frontier, build `TestReSTIR_GI_Temporal`, run a fresh `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8` test, inspect only appended log bytes, isolate the newest coherent frame-8 group for validation/statistics, directly inspect the display, and retain the v114 descriptor-contract controls.

## Tests written
- None. v117 is a verification-only retry and changed no production/test source; adding a test cannot bypass terminal authorization.

## Execution evidence
- Canonical build retried from the project root: `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal`.
- Actual tool result: `status=pending_approval`, `exit_code=-1`, `pattern_key=tirith:unknown`; no compiler output was produced.
- The tool reported the same authorization result on repeated terminal attempts, so further shell retries were stopped to avoid a blind loop.
- Consequently no fresh executable, ACCUM=8 process, appended log region, coherent dump group, validator result, structural statistics, or direct image inspection exists.

## Static controls
- PASS: `FRayTracingPipeline::AddBindingLayout` retains non-null additional layouts.
- PASS: `FinalizePipeline` appends additional layouts after the primary global layout and before optional bindless layout.
- PASS: `Shutdown` clears `AdditionalBindingLayouts`.
- PASS: `FGIPass.cpp` declares UAV layout slots as `FBindingLayoutBuilder::URegShift + 0/1` and calls `RTPipeline.AddBindingLayout(UAVBindingLayout)`.
- PASS: runtime and test-data `GIPathTracing.hlsl` copies declare `Output`/`DebugStatsTexture` at `register(u0/u1, space1)`.

## Coverage summary
- Module-direct: 0 new tests; 5 static contract controls PASS
- Runtime GPU acceptance: 0/6 verified

## TDD red-phase notes
No new behavior was implemented. Verification remains externally BLOCKED at build launch; no downstream PASS may be inferred.

## Testability gaps
A terminal-authorized runspace is required. Stale logs/dumps cannot substitute for a post-v114 build and direct inspection of a fresh display image.
