# Pending Tests v37 — alpha-channel sentinel awareness in validate_restir_gi.py

## Verdict
- **Mechanical PASS (static-only)** for the validator modification; runtime tests PENDING (terminal blocked).

## Part A — static tests (file-only, runnable this tick)

| # | Test | Status | Evidence |
|---|------|--------|----------|
| A1 | v37 plan marker present | PASS | PENDING_PLAN_v37.md exists |
| A2 | v37 plan-review marker present | PASS | PENDING_PLAN_REVIEW_v37.md exists |
| A3 | v37 commit marker present | PASS | PENDING_COMMIT_v37.md exists |
| A4 | v37 impl-review marker present | PASS | PENDING_IMPL_REVIEW_v37.md exists |
| A5 | v37 audit marker present | PASS | PENDING_TEST_AUDIT_v37.md exists |
| A6 | v37 PICK update | PASS | PENDING_PICK.md updated (v37 [x], v38 staged) |
| A7 | v37 PIPELINE_HEALTH append | PASS | docs/PIPELINE_HEALTH_2026-07-27.md append at end |
| A8 | Validator contains `check_alpha_sentinel` | PASS | validate_restir_gi.py:134 has the function |
| A9 | Validator contains `load_display_rgba` | PASS | validate_restir_gi.py:77 has the helper |
| A10 | main() wires ok4 + alpha_diag | PASS | validate_restir_gi.py:205 has `ok4, alpha_diag = check_alpha_sentinel(files)` |
| A11 | main() emits 4/4 verdict | PASS | validate_restir_gi.py:209-210 has `sum([ok1,ok2,ok3,ok4])` + `'4/4 checks PASSED'` |
| A12 | Diagnostic string printed always | PASS | validate_restir_gi.py:206 has `print(f'  alpha-sentinel diagnostic: {alpha_diag}')` |
| A13 | 5-alpha-pattern verdict ladder present | PASS | validate_restir_gi.py:165-185 has 5 return branches (saturated, zero, low, mixed, no-dump) |
| A14 | Docstring documents the 4 checks | PASS | validate_restir_gi.py:4 has "Four independent structural checks" |
| A15 | Existing 3 RGB checks unchanged | PASS | check_non_black_channel, check_spatial_std, check_cell_variance are byte-identical to v36 cycle |
| A16 | v22 binding-layout-split intact | PASS | FGIPass.h:106 has `UAVBindingLayout`; FRayTracingPipeline.cpp:357/361 has State.addBindingSet x2 |
| A17 | v28 alpha-channel sentinel intact (Private) | PASS | Private GIPathTracing.hlsl:694 has `Output[pixel].w = max(Output[pixel].w, 0.99994f);` |
| A18 | v28 alpha-channel sentinel intact (Data) | PASS | Data GIPathTracing.hlsl:694 has the same line |
| A19 | v12 cerr writes default-ON | PASS | TestReSTIR_GI_Temporal.cpp:384 + FGIPass.cpp:487 |
| A20 | bug-088 executeCommandList fix intact | PASS | TestReSTIR_GI_Temporal.cpp:691 |
| A21 | v37 patch imports/uses PIL.Image correctly | PASS | `from PIL import Image` at line 60; `Image.open(display).convert('RGBA')` at line 79 |
| A22 | v37 patch uses numpy correctly | PASS | `np.array(...dtype=np.float32)` at line 79; `np.sum`, `np.unique` not used but `np.sum(alpha > 254)` at line 160 is correct |

## Part B — runtime tests (PENDING — parent-driven, terminal blocked by tirith)

| # | Test | Status | Required action |
|---|------|--------|-----------------|
| B1 | Validator runs on existing dumps without crashing | PENDING | parent: `python3 validate_restir_gi.py` |
| B2 | Existing pre-v28 dumps → `alpha=low` FAIL | PENDING | parent: run B1 |
| B3 | RGB 3-check semantics unchanged for existing dumps | PENDING | parent: run B1 |
| B4 | v37 audit verdict `alpha=low` for 20260727 dumps | PENDING | parent: run B1 |
| B5 | After parent rebuild + run, `alpha=saturated` PASS | PENDING | parent: rebuild + run + B1 |
| B6 | After parent rebuild + run, RGB 3-check also PASS | PENDING | parent: rebuild + run + B1 |
| B7 | Validator returns 4/4 on working renderer | PENDING | parent: rebuild + run + B1 |
| B8 | fresh-evidence-scan.sh still passes (cumulative inventory intact) | PENDING | parent: run the v32 helper |

## Part C — goal gate (UNVERIFIED — all 6 criteria require parent action)

- (a) Debug target builds cleanly — UNVERIFIED (tirith blocks terminal)
- (b) Fresh `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM>=8` run — UNVERIFIED
- (c) No command-list-already-open errors — UNVERIFIED
- (d) No Vulkan ERROR/VUID in fresh log — UNVERIFIED
- (e) Validator passes newest dump group — UNVERIFIED; v37's alpha-check is now verifiable on parent's next run (will surface the precise alpha evidence shape)
- (f) Display visibly contains recognizable non-uniform Sponza — UNVERIFIED

No `PIPELINE_GOAL_DONE_<date>.md` written.

## Single-head caveat
- Same model writes tester + testing-verifier. Verdicts are self-checks. Mechanical pattern repetition keeps the verdict reproducible.

## Recommendation
- PASS Part A static tests; UNVERIFIED Part B + Part C pending parent terminal access.
- v37 is the FIRST cycle that modifies the validator itself. After parent's next run, the validator will surface alpha evidence shape and the pipeline can route to v38+ based on whether `alpha=saturated`, `alpha=0`, `alpha=mixed`, or `alpha=low`.