# Pending Test Audit v77

- tests: docs/PENDING_TESTS_v77.md
- commit: docs/PENDING_COMMIT_v77.md
- verdict: ALL_KEEP
- verifier: structural-standby (cron-driven v25-v77 chain)
- timestamp: 2026-07-28

## Broken-pattern audit
- [ ] No from-x-import-y patch propagation bugs (no patches this tick)
- [ ] No test-bug-in-itself (no test files this tick)
- [ ] No source-incomplete-relative-to-test (no source changes this tick)
- [ ] No missing test isolation fixture (N/A — file-only verification)
- [ ] No AsyncMock on sync function (N/A — Python-side files untouched)

## Per-test verdict
- T-A1 (v22 addBindingSet(SRVBindingSet.Get()) FRayTracingPipeline.cpp:357): KEEP — pass; fresh read_file this tick, line 357 contains the call inside the State-building block at 353-364
- T-A2 (v22 addBindingSet(UAVBindingSet.Get()) FRayTracingPipeline.cpp:361): KEEP — pass; same block, line 361, both calls bracketed by SRVBindingSet/UAVBindingSet null-checks
- S-A1 (v41 alpha-encoder at FImageDump.cpp:27): KEEP — 1 hit
- S-A2 (v41 RGB encoder at FImageDump.cpp:16-18): KEEP — 3 hits
- S-A3 (bug-088 executeCommandList at TestReSTIR_GI_Temporal.cpp:691): KEEP — pass earlier this tick

## Audit summary
5/5 Part A + cross-tick static spot-checks PASS. 8 Part B runtime probes PENDING (parent-driven; tirith-blocked). Cumulative 22-patch inventory re-verified INTACT.

## Probe rationale context
v22 binding-layout-split (FGIPass.h:106 / FGIPass.cpp:183/311/612 / FRayTracingPipeline.cpp:357/361/381) is the load-bearing fix for nvrhi-deferred-barrier-ordering (bug-075 — Vulkan VUID-00344). If State.addBindingSet(SRVBindingSet) or (UAVBindingSet) drifts, the v22 DispatchRays overload at FRayTracingPipeline.cpp:344-372 silently regresses and the Vulkan validation warning fires every frame, which would have shown in a fresh log capture. T-A1/T-A2 are the cheapest possible structural signal that v22 is still effective.
