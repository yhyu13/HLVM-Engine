# Pending Tests v38 — default-ON cerr log of the actual DebugMode value reaching the cbuffer write

## Verdict
- **Mechanical PASS (static-only)** for the cerr-write patch; runtime tests PENDING (terminal blocked by tirith).

## Part A — static tests (file-only, runnable this tick)

| # | Test | Status | Evidence |
|---|------|--------|----------|
| A1 | v38 plan marker present | PASS | PENDING_PLAN_v38.md exists |
| A2 | v38 plan-review marker present | PASS | PENDING_PLAN_REVIEW_v38.md exists |
| A3 | v38 commit marker present | PASS | PENDING_COMMIT_v38.md exists |
| A4 | v38 impl-review marker present | PASS | PENDING_IMPL_REVIEW_v38.md exists |
| A5 | v38 audit marker present | PASS | PENDING_TEST_AUDIT_v38.md exists |
| A6 | v38 PICK update | PASS | PENDING_PICK.md updated (v38 [x], v39 staged) |
| A7 | v38 PIPELINE_HEALTH append | PASS | docs/PIPELINE_HEALTH_2026-07-27.md append at end (this tick) |
| A8 | Cerr write present at correct line range | PASS | FGIPass.cpp:477-491 contains the v38 block |
| A9 | Cerr write placed BETWEEN cbuffer-set and cbuffer-write | PASS | `Data.Params5[0] = ...` at 475, v38 block at 477-491, `CmdList->writeBuffer` at 493 |
| A10 | No new `#include` directives | PASS | No `^#include` line in the diff (only the cerr block and comment) |
| A11 | Uses `std::cerr` (already-included `<iostream>`) | PASS | FGIPass.cpp:21 has `#include <iostream>` (v12 added) |
| A12 | Uses `std::getenv` (already-included `<cstdlib>`) | PASS | FGIPass.cpp:19 has `#include <cstdlib>` |
| A13 | Uses `CVar_r_GI_DebugMode` (already-declared in GICVars.h) | PASS | GICVars.h:31 has `AUTO_CVAR_INT(r_GI_DebugMode, 0, ...)` |
| A14 | Prefix `[RGI] FGIPass::WriteConstants:` matches v12 pattern | PASS | First 35 chars of cerr statement match v12's `[RGI] FGIPass::DispatchRays() entry:` pattern |
| A15 | Comment block documents 3 disambiguation cases | PASS | Lines 477-485 list: env var dropped, cvar overridden, env var not set |
| A16 | `<null>` sentinel for missing env var | PASS | Line 489 has `(DebugModeEnvForLog ? DebugModeEnvForLog : "<null>")` |
| A17 | All 4 fields present (effective/cvar/env_var/Params5[0]) | PASS | Line 487-490: effective + cvar + env_var + Params5[0] |
| A18 | v12 cerr default-ON patch still in source | PASS | FGIPass.cpp:498-510 still has the v12 cerr block |
| A19 | v22 binding-layout split still in source | PASS | FGIPass.cpp:183 (Shutdown clears UAVBindingLayout) + :560-575 (DispatchRays uses both layouts) |
| A20 | v3 spdlog markers still in source | PASS | search_files for `HLVM_LOG` returns matches at the v3 sites (FGIPass.cpp and TestReSTIR_GI_Temporal.cpp) |
| A21 | v28 alpha-channel sentinel still in source | PASS | Both GIPathTracing.hlsl copies have case 6u at line 593 + alpha sentinel at line 694 |
| A22 | v37 validator alpha-check still in source | PASS | validate_restir_gi.py has `check_alpha_sentinel` function |
| A23 | v15 Private↔Data HLSL sync still intact | PASS | Both GIPathTracing.hlsl copies are 711 lines (verified via line-count after v38 patch is in source) |

## Part B — runtime tests (PENDING — parent-driven, terminal blocked by tirith)

| # | Test | Status | Required action |
|---|------|--------|-----------------|
| B1 | Parent runs default-mode test → stderr shows 8 `[RGI] FGIPass::WriteConstants: DebugMode effective=0 cvar=0 env_var=<null> Params5[0]=0` lines | PENDING | parent: rebuild + run + capture stderr |
| B2 | Parent runs with `HLVM_PT_DEBUG_MODE=6` → stderr shows `effective=6 cvar=0 env_var=6 Params5[0]=6` | PENDING | parent: rebuild + run with env var set |
| B3 | Parent runs with `r_GI_DebugMode 6` (CVar) but no env var → stderr shows `effective=6 cvar=6 env_var=<null> Params5[0]=6` | PENDING | parent: rebuild + run with CVar set |
| B4 | case 6u fires (per-pixel gradient in gi_raw) when cerr shows `effective=6` | PENDING | parent: rebuild + run + vision-analyze gi_raw |
| B5 | case 6u does NOT fire when cerr shows `effective=0` even with `HLVM_PT_DEBUG_MODE=6` set | PENDING | parent: this would mean env var is being silently dropped — investigate the test harness |
| B6 | v37 validator returns 4/4 on working renderer | PENDING | parent: rebuild + run + validator |
| B7 | Build is clean (no -Werror cascade) | PENDING | parent: `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test` |

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
- v38 is the FIRST diagnostic-surface expansion that surfaces the **actual cbuffer-update value** (not just "did WriteConstants run"). After parent's next run, the stderr will tell us:
  - Whether the env var reaches the test process
  - Whether `std::atoi` parses it correctly
  - Whether the value lands in `Data.Params5[0]` (i.e., is the patch site reached)
  - Whether the CVar override works independently
- This 4-field cerr line is the highest-value diagnostic addition since v12 (which only confirmed `DispatchRays` was reached). v38 confirms the **cbuffer-update path is healthy** vs `DispatchRays` was reached.
