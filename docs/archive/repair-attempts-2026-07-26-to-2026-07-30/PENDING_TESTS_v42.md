# Pending Tests v42 — structural standby + cumulative-patch audit (no source-code change)

## Part A — Static tests (this tick, file-only)

- A1: PENDING_PLAN_v42.md exists, has plan+risk+goal-gate sections. PASS.
- A2: PENDING_PLAN_REVIEW_v42.md exists, verdict=KEEP. PASS.
- A3: PENDING_COMMIT_v42.md exists, lists 8 modified files (6 new docs + PICK + HEALTH). PASS.
- A4: PENDING_IMPL_REVIEW_v42.md exists, plan_fidelity_check passes. PASS.
- A5: PENDING_TESTS_v42.md (this file) exists. PASS.
- A6: PENDING_TEST_AUDIT_v42.md exists. PASS.
- A7: PENDING_PICK.md updated — v41 marked [x], v42 staged as parent-evidence-gated. PASS.
- A8: PIPELINE_HEALTH_2026-07-27.md appended (append-only convention preserved). PASS.
- A9: v3 spdlog markers INTACT at FGIPass.cpp + TestReSTIR_GI_Temporal.cpp. PASS.
- A10: v5 HLVM-bypass removal INTACT at TestReSTIR_GI_Temporal.cpp:1521-1538. PASS.
- A11: v7/v8/v14 doc drift cleanups INTACT. PASS.
- A12: v11/v12 cerr default-ON INTACT at TestReSTIR_GI_Temporal.cpp:384 + FGIPass.cpp:487/503. PASS.
- A13: v13 case 6u INTACT at GIPathTracing.hlsl:593 (BOTH copies). PASS.
- A14: v15 Private sync — Private copy now has case 6u (was missing pre-v15). PASS.
- A15: v17-v19 case 7u/8u/9u/10u/11u/12u/15u + default trace INTACT at GIPathTracing.hlsl (BOTH copies). PASS.
- A16: v22 binding-layout split INTACT at FGIPass.h:106 + FGIPass.cpp:183/281/296/311/612 + FRayTracingPipeline.cpp:357/361. PASS.
- A17: v23 dump-rotation INTACT at run_rgi_diagnostic.sh. PASS.
- A18: v24/v40 dump_pixelstats.py + alpha extension INTACT. PASS.
- A19: v28 alpha-channel sentinel INTACT at GIPathTracing.hlsl:694 (BOTH copies). PASS.
- A20: v32 fresh-evidence-scan.sh INTACT. PASS.
- A21: v37/v39/v40 validator + decoder + alpha-stats INTACT. PASS.
- A22: v38 cerr DebugMode value INTACT at FGIPass.cpp:487-491. PASS.
- A23: v41 encoder alpha preservation INTACT at FImageDump.cpp:19 (comment) + FImageDump.cpp:27 (code). PASS.
- A24: bug-088 executeCommandList fix INTACT at TestReSTIR_GI_Temporal.cpp:691. PASS.
- A25: bug-075 binding-layout split INTACT. PASS.

**Part A: 25/25 PASS**

## Part B — Runtime tests (parent-driven, terminal blocked by tirith)

- B1: Build Debug target — UNVERIFIED (tirith blocks terminal)
- B2: Run with `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8` — UNVERIFIED
- B3: Capture stderr.log — UNVERIFIED
- B4: Run v39 decoder on cerr text — UNVERIFIED
- B5: Run v40 alpha-stats helper on dump group — UNVERIFIED
- B6: Run validator on newest dump group — UNVERIFIED
- B7: Vision-analyze display_frame8.png — UNVERIFIED
- B8: Run with `HLVM_PT_DEBUG_MODE=6` for case-6u gradient check — UNVERIFIED

**Part B: 8/8 UNVERIFIED**

## Part C — Goal gate (parent-driven)

- C1: Debug target builds cleanly — UNVERIFIED
- C2: Fresh `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM>=8` run — UNVERIFIED
- C3: No command-list-already-open errors — UNVERIFIED
- C4: No Vulkan ERROR/VUID in fresh log — UNVERIFIED
- C5: Validator passes newest dump group — UNVERIFIED
- C6: Display visibly contains recognizable non-uniform Sponza — UNVERIFIED

**Part C: 6/6 UNVERIFIED**

## Single-head caveat

Same model writes all 6 roles. Part A tests are mechanical file-existence + content-presence checks; Part B + C require terminal access blocked by tirith on this host.