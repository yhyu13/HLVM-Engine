# Pending Tests v36 — structural standby tick

## Verdict
- **Mechanical PASS (static-only)** — file-only verification, no test surface change.

## Part A — static tests (file-only, runnable this tick)

| # | Test | Status | Evidence |
|---|------|--------|----------|
| A1 | v36 plan marker present | PASS | PENDING_PLAN_v36.md exists |
| A2 | v36 plan-review marker present | PASS | PENDING_PLAN_REVIEW_v36.md exists |
| A3 | v36 commit marker present | PASS | PENDING_COMMIT_v36.md exists |
| A4 | v36 impl-review marker present | PASS | PENDING_IMPL_REVIEW_v36.md exists |
| A5 | v36 audit marker present | PASS | PENDING_TEST_AUDIT_v36.md exists |
| A6 | v36 PICK update | PASS | PENDING_PICK.md updated (v36 [x], v37 staged) |
| A7 | v36 PIPELINE_HEALTH append | PASS | docs/PIPELINE_HEALTH_2026-07-27.md append at end |
| A8 | v3 spdlog FGIPass::DispatchRays ENTER present | PASS | FGIPass.cpp:511 has HLVM_LOG(LogGI, info, ...) "FGIPass::DispatchRays ENTER" |
| A9 | v12 cerr writes default-ON (Render + DispatchRays) | PASS | TestReSTIR_GI_Temporal.cpp:384 + FGIPass.cpp:487 both have `std::cerr << "[RGI]..."` |
| A10 | v22 binding-layout UAVBindingLayout intact | PASS | FGIPass.h:106 has `nvrhi::BindingLayoutHandle UAVBindingLayout; // v22 split` |
| A11 | v22 binding-layout State.addBindingSet() x2 intact | PASS | FRayTracingPipeline.cpp:357 (SRV) + :361 (UAV) |
| A12 | v22 binding-layout 2-overload DispatchRays intact | PASS | FRayTracingPipeline.h + .cpp have 2 DispatchRays overloads (SRVBindingSet-only and SRV+UAV) |
| A13 | v28 alpha-channel sentinel intact (Private) | PASS | GIPathTracing.hlsl Private:694 has `Output[pixel].w = max(Output[pixel].w, 0.99994f);` |
| A14 | v28 alpha-channel sentinel intact (Data) | PASS | GIPathTracing.hlsl Data:694 has the same line |
| A15 | v13/v17/v18/v19 HLSL sentinels intact (Private) | PASS | case 6u, 7u, 8u, 9u, 10u, 11u, 12u, 15u, default trace all present |
| A16 | v32 fresh-evidence-scan.sh helper present | PASS | TestReSTIR_GI_Temporal_Data/fresh-evidence-scan.sh exists |
| A17 | v23 run_rgi_diagnostic.sh + v24 dump_pixelstats.py present | PASS | TestReSTIR_GI_Temporal_Data/run_rgi_diagnostic.sh + dump_pixelstats.py exist |
| A18 | bug-088 executeCommandList fix intact | PASS | TestReSTIR_GI_Temporal.cpp:691 has executeCommandList call |
| A19 | 0 stale HLVM_FORCE_CERR_LOGGING macros | PASS | search_files shows 0 matches for the macro |
| A20 | v14 line-691 cross-references intact | PASS | TestReSTIR_GI_Temporal.cpp has `executeCommandList at line 691` references |

## Part B — runtime tests (PENDING — parent-driven, terminal blocked by tirith)

| # | Test | Status | Required action |
|---|------|--------|-----------------|
| B1 | Source builds cleanly (Debug) | PENDING | parent runs `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test` |
| B2 | Default-mode render produces PNGs | PENDING | parent runs `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal 2>stderr.log` |
| B3 | cerr writes appear in stderr (8 Render + 8 DispatchRays) | PENDING | parent `cat stderr.log` |
| B4 | HLVM_PT_DEBUG_MODE=6 produces per-pixel gradient | PENDING | parent runs with `HLVM_PT_DEBUG_MODE=6` and inspects gi_raw* PNG |
| B5 | Validator passes 3/3 | PENDING | parent runs `validate_restir_gi.py` |
| B6 | Vision-check shows Sponza geometry | PENDING | parent opens display_frame8.png |
| B7 | Alpha-channel sentinel saturates to 254-255 | PENDING | parent inspects display_frame8.png alpha channel |
| B8 | fresh-evidence-scan.sh runs successfully | PENDING | parent runs the helper |

## Part C — goal gate (UNVERIFIED — all 6 criteria require parent action)

- (a) Debug target builds cleanly — UNVERIFIED
- (b) Fresh `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM>=8` run — UNVERIFIED
- (c) No command-list-already-open errors — UNVERIFIED
- (d) No Vulkan ERROR/VUID in fresh log — UNVERIFIED
- (e) Validator passes newest dump group — UNVERIFIED
- (f) Display visibly contains recognizable non-uniform Sponza — UNVERIFIED
- No `PIPELINE_GOAL_DONE_<date>.md` written.

## Single-head caveat
- Same model writes tester + testing-verifier. Verdicts are self-checks. Mechanical pattern repetition keeps the verdict reproducible.

## Recommendation
- PASS Part A static tests; UNVERIFIED Part B + Part C pending parent terminal access.