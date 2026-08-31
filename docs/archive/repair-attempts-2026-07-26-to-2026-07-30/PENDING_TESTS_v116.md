# Pending Tests v116
- commit: docs/PENDING_COMMIT_v116.md
- tester: tester (role #5)
- timestamp: 2026-07-29

## Plan test strategy cited
Role #5 was required to build `TestReSTIR_GI_Temporal`, capture a pre-run artifact frontier, run a fresh `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8` capture, scan only the fresh log region, isolate the newest coherent frame-8 dump group for validation/statistics, inspect the display image, and retain the v114 static descriptor-contract controls.

## Tests written
- None. v116 is a verification-only retry and changed no production or test source; inventing a new test would not remove the terminal-authorization blocker.

## Execution evidence
- Canonical build attempted from the project root: `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal`.
- Actual tool result: `status=pending_approval`, `exit_code=-1`, `pattern_key=tirith:unknown`; no compiler output was produced.
- Because no fresh executable was built, the ACCUM=8 target run, fresh-log scan, newest-group validator, structural statistics, and visual display inspection could not truthfully run.

## Static controls
- PASS: `FRayTracingPipeline.cpp` appends every `AdditionalBindingLayouts` entry to `PipelineDesc.globalBindingLayouts`.
- PASS: `FRayTracingPipeline.cpp` clears `AdditionalBindingLayouts` during shutdown.
- PASS: `FGIPass.cpp` declares its two UAV layout slots as `FBindingLayoutBuilder::URegShift + 0/1` (384/385 contract).
- PASS: the runtime and test-data `GIPathTracing.hlsl` copies declare `Output` and `DebugStatsTexture` in `space1`.
- PASS: the validator still globs historical `*frame8.png` files, confirming that newest-group isolation is mandatory once execution is available.

## Coverage summary
- Module-direct: 0 new tests; 5 static contract checks PASS
- TestClient-layer: 0 (not applicable)
- Router-wiring: 0 (not applicable)
- Runtime GPU acceptance: 0/6 verified in this tick

## TDD red-phase notes
No new behavior was implemented in v116. The decisive verification remains RED/BLOCKED at build launch: terminal authorization prevented the target from executing, so no downstream PASS can be claimed.

## Testability gaps
The blocker is external execution authorization, not source testability. A terminal-authorized role must run the unchanged v116 command and inspect the fresh PNG rather than trusting the scalar validator.
