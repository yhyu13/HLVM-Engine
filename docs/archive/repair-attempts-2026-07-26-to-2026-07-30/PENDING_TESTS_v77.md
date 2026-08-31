# Pending Tests v77

- plan: docs/PENDING_PLAN_v77.md
- commit: docs/PENDING_COMMIT_v77.md
- approach: 1 Part A static fresh-probe via read_file; 0 runtime probes (terminal-blocked)

## Part A — static fresh spot-check (cron-driven; NOT by-reference to v76 audit)
- T-A1: v22 addBindingSet(SRVBindingSet.Get()) at FRayTracingPipeline.cpp:357 — PASS via read_file offset 350-369 (fresh this tick)
- T-A2: v22 addBindingSet(UAVBindingSet.Get()) at FRayTracingPipeline.cpp:361 — PASS via same read_file range, both calls visible

## Part B — runtime probes (parent-driven; tirith-blocked)
- T-B1 through T-B8: parent runs `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test` + `cd Binary/Debug && HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal 2>stderr.log` + validates with `validate_restir_gi.py` + vision-analyzes `display_frame8.png` + reports B8 zero-VUID check on stderr.log

## Cross-tick cross-checks (earlier this turn, still fresh)
- S-A1: v41 std::clamp(rgbaData[i*4+3] * 255.0f, 0, 255) at Private/Image/FImageDump.cpp:27 — PASS via search_files (1 hit)
- S-A2: v41 RGB std::clamp at FImageDump.cpp:16-18 — PASS via search_files (3 hits)
- S-A3: bug-088 executeCommandList at TestReSTIR_GI_Temporal.cpp:691 — PASS via read_file offset 685-699

## Probe target rationale
v22 binding-layout-split is the load-bearing root-cause-or-diagnostic fix for nvrhi-deferred-barrier-ordering (bug-075). If State.addBindingSet wiring ever drifts, the v22 fix is silently broken without obvious symptoms; checking both calls in one block confirms the v22 DispatchRays overload (FRayTracingPipeline.cpp:344-372) still routes through the two-binding-set pattern that bypasses nvrhi's bindDescriptorSets-before-commitBarriers bug.
