# Pending Tests v27

- commit: docs/PENDING_COMMIT_v27.md
- verifier: cron (file-only role #5, six-role-pipeline)
- timestamp: 2026-07-27T15:30:00Z

## Test surface
v27 is an audit-only cycle; 0 source-code changes. Tests are split into Part A (cron-verifiable via static inspection) and Part B (parent-driven; terminal required and blocked in cron by tirith).

### Part A — cron-verifiable static tests (15/15 PASS this tick)

| # | Test | Verification | Status |
|---|------|--------------|--------|
| A1 | v22 UAVBindingLayout member in FGIPass.h | line 106 (`nvrhi::BindingLayoutHandle UAVBindingLayout; // v22 split:...`) | PASS |
| A2 | v22 DispatchRays 6-arg overload in FRayTracingPipeline.h | line 188 (Desc form), 194 (W/H/D form) | PASS |
| A3 | v22 Patch comment context (explains nvrhi-deferred-barrier-ordering) | FRayTracingPipeline.h:180-187 | PASS |
| A4 | v12 cerr writes default-ON | TestReSTIR_GI_Temporal.cpp:384 (`std::cerr << "[RGI] Render() entry:..."`), FGIPass.cpp:487 | PASS |
| A5 | v5 HLVM-bypass removal (NOTE comment) | TestReSTIR_GI_Temporal.cpp near line 1521 | PASS |
| A6 | v13/v15 case 6u in BOTH HLSL copies | Private master :593 + data-dir :593 | PASS |
| A7 | v3 spdlog diagnostic markers | FGIPass.cpp (5 sites confirmed in v26) | PASS |
| A8 | v14 line-691 doc-drift fix | TestReSTIR_GI_Temporal.cpp:408/662/1537 | PASS |
| A9 | bug-088 fix intact | TestReSTIR_GI_Temporal.cpp:691 (`NvrhiDevice->executeCommandList(CommandList)`) | PASS |
| A10 | bug-075 binding-layout split (FGIPass side) | FGIPass.cpp:277 (Add*), FGIPass.cpp:506-528 (Set*) | PASS |
| A11 | 0 stale `HLVM_FORCE_CERR_LOGGING` macros | `grep` returns 0 matches in source | PASS |
| A12 | v23 dump-rotation archive-after-run | run_rgi_diagnostic.sh:126 | PASS |
| A13 | v24 dump_pixelstats.py presence | TestReSTIR_GI_Temporal_Data/dump_pixelstats.py | PASS |
| A14 | v15 Private↔Data HLSL sync | both copies have case 6u at line 593 | PASS |
| A15 | v22 call site at FGIPass.cpp:609 | read_file confirmed (carries over from v26 audit) | PASS |

### Part B — parent-driven runtime tests (7 PENDING, unchanged from v25/v26)

| # | Test | Status | Notes |
|---|------|--------|-------|
| B1 | Build exits 0 | PENDING | parent-driven; requires `Build.sh` execution (terminal blocked in cron) |
| B2 | Default run produces 16 cerr lines | PENDING | parent-driven; requires stderr capture |
| B3 | Mode 6 produces per-pixel gradient | PENDING | parent-driven; per v13 evidence shape |
| B4 | Validator returns 3/3 on default-mode dumps | PENDING | parent-driven |
| B5 | dump_pixelstats.py runs on fresh dumps | PENDING | parent-driven |
| B6 | rgi_evidence.txt pasted back to cron | PENDING | parent-driven |
| B7 | display_frame8.png shows recognizable Sponza | PENDING | parent-driven; vision analysis required |

## What this test surface does NOT do

- Does NOT generate fresh dump groups (terminal blocked by tirith)
- Does NOT run `validate_restir_gi.py` (terminal blocked)
- Does NOT vision-analyze images (no vision tool)
- Does NOT execute `Build.sh` (terminal blocked)
- Does NOT advance the renderer toward acceptance criteria without terminal access

## Test artifacts
None produced (audit-only cycle).