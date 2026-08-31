# Pending Plan v26

- task: structural static-audit confirming v22 binding-layout-split + v3/v5/v7/v8/v11/v12/v13/v14/v15/v17/v18/v19 patches all remain intact in source tree as of this cron tick; the "any change since v25" check
- source: no bundle — direct edit (file-only structural audit)
- approach: walk every site touched by every prior cycle (v3, v5, v7, v8, v11, v12, v13, v14, v15, v17, v18, v19, v22) and confirm presence at documented line numbers via search_files + read_file. Also confirm the v22 plan's load-bearing assertions (UAVBindingLayout member, createBindingLayout(UAVLayoutDesc) call, createBindingSet(UAVBuilder.Build(), UAVBindingLayout), 6-arg DispatchRays overload, State.addBindingSet calls in new overload). 0 source modifications.
- diff_estimate: +0 / -0 lines (read-only audit)
- skip_plan_review: no — audit results inform next-cycle routing
- test_strategy: cron verifies via static inspection (search_files + read_file at documented line ranges); Part B is parent-driven (terminal blocked in cron)
- risks:
  1. Line-number drift from earlier cycles' documented sites — mitigation: read_file with context to confirm structural shape, not just exact line numbers
  2. Patch site partially present (e.g., declaration added but init missing) — mitigation: cross-reference declaration + initialization + use sites for each patch

## Why this is the right next cycle

Per v25 audit's last paragraph: "The next cycle's only mechanical step is another audit (v26 stages the 'any change since v25' check); otherwise the pipeline is stalled awaiting parent." Terminal access remains blocked by tirith (verified in 14+ ticks). Without parent-driven evidence, the only mechanically-actionable file-only step is a re-audit confirming the patches that previous cycles claimed to have landed are still in source.

## File-level changes

```
+ docs/PENDING_PLAN_v26.md
+ docs/PENDING_PLAN_REVIEW_v26.md
+ docs/PENDING_COMMIT_v26.md
+ docs/PENDING_IMPL_REVIEW_v26.md
+ docs/PENDING_TESTS_v26.md
+ docs/PENDING_TEST_AUDIT_v26.md
M docs/PIPELINE_HEALTH_2026-07-27.md  (tick section appended)
```

0 source-code lines changed. Pure structural verification.

## Audit checklist (planned)

1. **v22 patch sites (load-bearing fix candidate)**:
   - `UAVBindingLayout` member present in FGIPass.h:106
   - `UAVBindingLayout = nullptr;` in FGIPass.cpp:183
   - `Device->createBindingLayout(UAVLayoutDesc)` in FGIPass.cpp:311
   - `Device->createBindingSet(UAVBuilder.Build(), UAVBindingLayout)` in FGIPass.cpp:596
   - new `DispatchRays(..., SRVBindingSet, UAVBindingSet)` overload in FRayTracingPipeline.h + .cpp
   - `State.addBindingSet(SRVBindingSet.Get())` + `State.addBindingSet(UAVBindingSet.Get())` in FRayTracingPipeline.cpp new overload
2. **v3 diagnostic sites**: confirm LogGI markers in FGIPass.cpp; Pre-GIPass in TestReSTIR_GI_Temporal.cpp
3. **v5 HLVM-bypass removal**: confirm NOTE comment present
4. **v11/v12 cerr writes default-ON**: confirm both cerr writes present, 0 `HLVM_FORCE_CERR_LOGGING` macros
5. **v13/v17/v18/v19 sentinel probes**: confirm case 6u/7u/8u/9u/10u/11u/12u/15u in BOTH HLSL copies; default-case trace
6. **v15 Private↔Data HLSL sync**: confirm both copies have case 6u at line 593
7. **v14 line references**: confirm "line 691" at TestReSTIR_GI_Temporal.cpp
8. **v23 dump-rotation fix**: confirm run_rgi_diagnostic.sh archive-after-run pattern
9. **v24 dump_pixelstats.py**: confirm presence

## What this audit does NOT do

- Does NOT modify any source code (read-only)
- Does NOT create Kanban cards (per cron instruction)
- Does NOT commit, push, or rewrite history (per cron instruction)
- Does NOT replace parent-driven verification (build + run + log + validator + vision)
- Does NOT advance the renderer toward acceptance criteria without terminal access

## What comes after v26

- If clean KEEP: same as v25 — pipeline stays at this heartbeat, parent runs `run_rgi_diagnostic.sh`
- If regression flagged: v27 stages the surgical restore to the prior known-good state
- If terminal block persists indefinitely: heartbeats continue per "Never silently exit" hard rule