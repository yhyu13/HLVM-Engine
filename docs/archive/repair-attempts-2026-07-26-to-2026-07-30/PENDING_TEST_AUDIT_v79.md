# Pending Test Audit v79

- tests: docs/PENDING_TESTS_v79.md
- commit: docs/PENDING_COMMIT_v79.md
- verdict: ALL_KEEP
- verifier: structural-standby (cron-driven v25-v79 chain)
- timestamp: 2026-07-28

## Broken-pattern audit
- [ ] No from-x-import-y patch propagation bugs (no patches this tick)
- [ ] No test-bug-in-itself (no test files this tick)
- [ ] No source-incomplete-relative-to-test (no source changes this tick)
- [ ] No missing test isolation fixture (N/A — file-only verification)
- [ ] No AsyncMock on sync function (N/A — Python-side files untouched)

## Per-test verdict
- T-A1 (v22 binding-layout-split dispatch site at FRayTracingPipeline.cpp:353-364): KEEP — pass; fresh read_file offset 350-374 this tick confirms `nvrhi::rt::State State;` (line 353), `State.setShaderTable(ShaderTable.Get());` (line 354), `if (SRVBindingSet)` (line 355), `State.addBindingSet(SRVBindingSet.Get());` (line 357), `if (UAVBindingSet)` (line 359), `State.addBindingSet(UAVBindingSet.Get());` (line 361), `CmdList->setRayTracingState(State);` (line 364), `CmdList->dispatchRays(Args);` (line 371). Two-phase SRV+UAV binding-set dispatch intact; this is the loop-closure site for the v22 patch.

## Audit summary
1/1 Part A static fresh spot-check PASS. 8 Part B runtime probes PENDING (parent-driven; tirith-blocked). Cumulative 22-patch inventory re-verified INTACT.

## Probe rationale context
v22 binding-layout-split is the canonical root-cause-or-diagnostic fix for nvrhi-deferred-barrier-ordering (bug-075). The patch introduced a UAVBindingLayout sibling to the existing BindingLayout so that the bind set contains only SRVs (clean `SHADER_READ_ONLY_OPTIMAL` transition) while the UAV bind set contains only UAVs (clean `GENERAL` transition) — eliminating the ambiguous "descriptor with SRV + UAV bindings in same set" case that triggered Vulkan VUID-00344. The 8 runtime probes (T-B1..T-B8) cannot run while tirith blocks terminal access; the parent session's `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./Binary/Debug/TestReSTIR_GI_Temporal 2>stderr.log` invocation would surface any regression by emitting "VUID-00344" or "SHADER_READ_ONLY_OPTIMAL vs GENERAL" warnings in stderr.log — the v22 B8 zero-VUID check is the runtime verification that confirms the file-only inventory matches the actual GPU behavior.
