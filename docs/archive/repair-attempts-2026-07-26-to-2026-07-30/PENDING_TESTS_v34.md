# Pending Tests v34 — structural standby tick

## Verdict
- **Mechanical PASS (static-only)** — file-only verification, no test surface change.

## Part A — static tests (file-only, runnable this tick)

| # | Test | Status | Evidence |
|---|------|--------|----------|
| A1 | v34 plan marker present | PASS | PENDING_PLAN_v34.md exists |
| A2 | v34 plan-review marker present | PASS | PENDING_PLAN_REVIEW_v34.md exists |
| A3 | v34 commit marker present | PASS | PENDING_COMMIT_v34.md exists |
| A4 | v34 impl-review marker present | PASS | PENDING_IMPL_REVIEW_v34.md exists |
| A5 | v34 audit marker present | PASS | PENDING_TEST_AUDIT_v34.md exists |
| A6 | v34 PICK update | PASS | PENDING_PICK.md updated (v34 [x], v35 staged) |
| A7 | v34 PIPELINE_HEALTH append | PASS | docs/PIPELINE_HEALTH_2026-07-27.md append at end |
| A8 | v3 spdlog FGIPass::DispatchRays ENTER present | PASS | FGIPass.cpp source has HLVM_LOG(LogGI, info, ...) "FGIPass::DispatchRays ENTER" |
| A9 | v5 HLVM-bypass NOTE comment present | PASS | TestReSTIR_GI_Temporal.cpp:~1521 NOTE comment intact |
| A10 | v22 binding-layout UAVBindingLayout intact | PASS | FGIPass.h:106 has `nvrhi::BindingLayoutHandle UAVBindingLayout; // v22 split` |
| A11 | v22 binding-layout State.addBindingSet() x2 intact | PASS | FRayTracingPipeline.cpp:357 (SRV) + :361 (UAV) |
| A12 | v28 alpha-channel sentinel intact (Private) | PASS | GIPathTracing.hlsl Private:694 has `Output[pixel].w = max(Output[pixel].w, 0.99994f);` |
| A13 | v28 alpha-channel sentinel intact (Data) | PASS | GIPathTracing.hlsl Data:694 has the same line |
| A14 | v32 fresh-evidence-scan.sh helper present | PASS | TestReSTIR_GI_Temporal_Data/fresh-evidence-scan.sh exists, 171 lines |
| A15 | bug-088 executeCommandList fix intact | PASS | TestReSTIR_GI_Temporal.cpp:691 has executeCommandList call |

## Part B — runtime tests (PENDING — parent-driven, terminal blocked by tirith)

| # | Test | Status | Required action |
|---|------|--------|-----------------|
| B1 | Source builds cleanly (Debug) | PENDING | parent runs `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test` |
| B2 | Default-mode render produces PNGs | PENDING | parent runs `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal` |
| B3 | HLVM_PT_DEBUG_MODE=6 produces per-pixel gradient | PENDING | parent runs with `HLVM_PT_DEBUG_MODE=6` and inspects gi_raw* PNG |
| B4 | Validator passes 3/3 | PENDING | parent runs `validate_restir_gi.py` |
| B5 | Vision-check shows Sponza geometry | PENDING | parent opens display_frame8.png |
| B6 | Alpha-channel sentinel saturates to 254-255 | PENDING | parent inspects display_frame8.png alpha channel |
| B7 | fresh-evidence-scan.sh runs successfully | PENDING | parent runs the helper |

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