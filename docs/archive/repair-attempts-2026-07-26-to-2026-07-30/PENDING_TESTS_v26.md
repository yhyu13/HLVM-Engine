# Pending Tests v26

- commit: docs/PENDING_COMMIT_v26.md
- verifier: cron (file-only role #5, six-role-pipeline)
- timestamp: 2026-07-27T15:00:00Z

## Test surface

v26 is an audit-only cycle; 0 source-code changes. Tests are split into Part A (cron-verifiable via static inspection) and Part B (parent-driven; terminal required).

### Part A — cron-verifiable static tests (10/10 PASS)

| # | Test | Verification | Status |
|---|------|--------------|--------|
| A1 | v22 UAVBindingLayout member in FGIPass.h | `grep UAVBindingLayout Engine/Source/Runtime/Public/Renderer/GI/FGIPass.h` → line 106 | PASS |
| A2 | v22 UAVBindingLayout init in FGIPass.cpp | `grep UAVBindingLayout Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp` → line 183 | PASS |
| A3 | v22 createBindingLayout(UAVLayoutDesc) | line 311 | PASS |
| A4 | v22 createBindingSet(UAVBuilder.Build(), UAVBindingLayout) | line 596 | PASS |
| A5 | v22 DispatchRays overload signature in FRayTracingPipeline.h | line 188 (Desc form), 194 (W/H/D form) | PASS |
| A6 | v22 State.addBindingSet calls in new overload | FRayTracingPipeline.cpp:357 (SRV), :361 (UAV) | PASS |
| A7 | v3 FGIPass::DispatchRays ENTER log | FGIPass.cpp:511 | PASS |
| A8 | v12 cerr writes default-ON | TestReSTIR_GI_Temporal.cpp:384, FGIPass.cpp:487 | PASS |
| A9 | v13/v17/v18/v19 sentinels in BOTH HLSL copies | case 6u/7u/8u/9u/10u/11u/12u/15u + default-case trace in Private master; case 6u in data-dir copy | PASS |
| A10 | v14 line-691 doc-drift fix | TestReSTIR_GI_Temporal.cpp:408, 662, 1537 | PASS |
| A11 | v15 Private↔Data HLSL sync | case 6u at line 593 in both copies | PASS |
| A12 | v23 dump-rotation archive-after-run | run_rgi_diagnostic.sh:126 (`mv ... dumps_${mode_name}` AFTER run) | PASS |
| A13 | v24 dump_pixelstats.py presence | TestReSTIR_GI_Temporal_Data/dump_pixelstats.py | PASS |
| A14 | bug-088 fix intact | TestReSTIR_GI_Temporal.cpp:691 (`NvrhiDevice->executeCommandList(CommandList)`) | PASS |
| A15 | 0 stale `HLVM_FORCE_CERR_LOGGING` macros | `grep` returns 0 matches in source | PASS |

### Part B — parent-driven runtime tests (unchanged from v25)

| # | Test | Status | Notes |
|---|------|--------|-------|
| B1 | Build exits 0 | PENDING | parent-driven; requires `Build.sh` execution |
| B2 | Default run produces 16 cerr lines | PENDING | parent-driven; requires stderr capture |
| B3 | Mode 6 produces per-pixel gradient | PENDING | parent-driven; per v13 evidence shape |
| B4 | Validator returns 3/3 on default-mode dumps | PENDING | parent-driven |
| B5 | dump_pixelstats.py runs on fresh dumps | PENDING | parent-driven |
| B6 | rgi_evidence.txt pasted back to cron | PENDING | parent-driven |
| B7 | display_frame8.png shows recognizable Sponza | PENDING | parent-driven; vision analysis required |

## What this test surface does NOT do

- Does NOT generate fresh dump groups (terminal blocked)
- Does NOT run `validate_restir_gi.py` (terminal blocked)
- Does NOT vision-analyze images (no vision tool)
- Does NOT execute `Build.sh` (terminal blocked)
- Does NOT advance the renderer toward acceptance criteria without terminal access

## Test artifacts

None produced (audit-only cycle).