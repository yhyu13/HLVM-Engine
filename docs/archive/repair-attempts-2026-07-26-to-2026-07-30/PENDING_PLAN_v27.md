# Pending Plan v27

- task: structural confirmation that v22 binding-layout-split patch (the load-bearing fix candidate) is still intact + that no prior patch has regressed since v26 audit. Read-only verification cycle, identical structural shape to v25/v26.
- source: no bundle — direct edit (file-only static audit)
- approach: walk v22's load-bearing sites (FGIPass.h:106, FGIPass.cpp:183/311/596, FRayTracingPipeline.h:188+194, FRayTracingPipeline.cpp:357+361) and confirm presence + shape. Cross-check v3/v5/v7/v8/v11/v12/v13/v14/v15/v17/v18/v19 sentinel sites. 0 source-code changes.
- diff_estimate: +0 / -0 lines (read-only)
- skip_plan_review: no — audit results inform next-cycle routing
- test_strategy: cron verifies via search_files + read_file at documented line ranges; Part B is parent-driven (terminal blocked in cron; tirith `pending_approval` denies every terminal probe in this trajectory)
- risks:
  1. Line-number drift from v22 patch landed — mitigation: read_file with surrounding context to confirm structural shape
  2. Patch site partially present — mitigation: cross-reference declaration + initialization + use sites for each patch

## Why this is the right next cycle (per cron user instruction "continue cycles... until the acceptance criteria are actually met")

Terminal access is blocked by tirith (`pending_approval: tirith:unknown`) on every probe in this trajectory — `pwd`, `echo`, `ls`, `cat`, `git status`, `Build.sh`, etc. Effective toolset is file-only. The pipeline has done 26 cycles of file-only work. Without parent-driven terminal evidence, the only mechanically-actionable file-only step is a v27 re-audit confirming v22 binding-layout-split remains in source. v22 is the highest-confidence fix candidate per `software-development-practices §Path-Tracing / RT Debugging Methodology` + `gpu-rendering-bisect-debug` skill's "split binding layout into SRV-only and UAV-only sets" recipe.

## Audit checklist (planned)

1. **v22 patch sites (load-bearing fix candidate)**:
   - `UAVBindingLayout` member present in FGIPass.h:106 (confirmed this tick)
   - `UAVBindingLayout = nullptr;` in FGIPass.cpp:183
   - `Device->createBindingLayout(UAVLayoutDesc)` in FGIPass.cpp:311
   - `Device->createBindingSet(UAVBuilder.Build(), UAVBindingLayout)` in FGIPass.cpp:596
   - new `DispatchRays(..., SRVBindingSet, UAVBindingSet)` overload in FRayTracingPipeline.h:188+194 (confirmed this tick)
   - `State.addBindingSet(SRVBindingSet.Get())` + `State.addBindingSet(UAVBindingSet.Get())` in FRayTracingPipeline.cpp new overload
2. **v3 diagnostic sites**: confirm LogGI markers in FGIPass.cpp; Pre-GIPass in TestReSTIR_GI_Temporal.cpp
3. **v5 HLVM-bypass removal**: confirm NOTE comment present
4. **v11/v12 cerr writes default-ON**: confirm both cerr writes present (TestReSTIR_GI_Temporal.cpp:384 ✓, FGIPass.cpp:487 ✓), 0 `HLVM_FORCE_CERR_LOGGING` macros
5. **v13/v15 case 6u**: confirm case 6u in BOTH HLSL copies at line 593
6. **v14 line references**: confirm "line 691" at TestReSTIR_GI_Temporal.cpp (691 ✓ — executeCommandList site)
7. **bug-088 fix intact**: confirm TestReSTIR_GI_Temporal.cpp:691 (`NvrhiDevice->executeCommandList(CommandList)`) ✓

## What this audit does NOT do

- Does NOT modify any source code (read-only)
- Does NOT create Kanban cards (per cron instruction)
- Does NOT commit, push, or rewrite history (per cron instruction)
- Does NOT replace parent-driven verification (build + run + log + validator + vision)
- Does NOT advance the renderer toward acceptance criteria without terminal access

## Honest scope clarification

Per `gpu-rendering-bisect-debug` "Don't fabricate findings": the structural terminal block (tirith denies every shell probe in this cron trajectory) is environmental, not architectural. The pipeline has done all the file-only work it can do. The next corrective step is irreducibly parent-driven: rebuild, run, capture stderr, analyze `gi_raw`, run validator, vision-analyze display_frame8.png. Without terminal, no fix can be verified and no new evidence can be gathered.

If the structural terminal block persists, this re-audit pattern is the only mechanically-actionable file-only work remaining. It satisfies HARD INVARIANT #6 ("Never silently exit") without fabricating evidence.

## What comes after v27

- If clean KEEP: pipeline stays at this heartbeat, parent runs `run_rgi_diagnostic.sh`
- If regression flagged: v28 stages the surgical restore to the prior known-good state
- If terminal block persists indefinitely: heartbeats continue per "Never silently exit" hard rule; no fabricated progress markers will be written