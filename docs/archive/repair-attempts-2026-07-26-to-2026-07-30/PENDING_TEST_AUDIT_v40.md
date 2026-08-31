# Pending Test Audit v40 — extend dump_pixelstats.py to read alpha channel

## Verdict
- **ALL_KEEP** — v40 patch is purely additive, mechanically correct, exhaustive in its 5-pattern alpha-classification ladder, and ready for parent terminal-driven verification.

## Broken-pattern audit
- [x] No from-x-import-y patch propagation bugs (N/A — single Python file modified)
- [x] No test-bug-in-itself (N/A — no test file modified)
- [x] No source-incomplete-relative-to-test (N/A — no test file modified)
- [x] No missing test isolation fixture (N/A — pure stdlib + numpy/PIL extension)
- [x] No AsyncMock on sync function or vice versa (N/A — pure Python parser)
- [x] No security scan failures (pure Python parser, no subprocess, no eval/exec/SQL/buffer overflow)
- [x] No -Werror cascade risk (Python file; -Werror is C++ compiler flag, irrelevant)
- [x] No GUI-impact (no IBR/RT/BLAS touches; pure post-hoc dump analyzer)

## Per-test verdict
- A1-A21: 21/21 PASS (static file-only verification)
- A22: 1 deferred (parent-driven python3 -m py_compile)
- B1-B7: 7/7 UNVERIFIED (parent-driven, terminal blocked by tirith)
- C1-C6 (goal gate): UNVERIFIED — six criteria from prompt all require parent action

## Per-part verdict
- Part A (static): ALL_KEEP — 21/21 mechanical checks pass. A22 deferred to parent runtime.
- Part B (runtime): UNVERIFIED — parent-driven, terminal required.
- Part C (goal gate): UNVERIFIED — six criteria from prompt remain unchanged.

## Specific audit findings
1. **File scope verified**: `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dump_pixelstats.py` (modified). Two new functions added (`compute_alpha_stats`, `classify_alpha_sentinel`); `emit_stats()` extended with a v40 alpha-inspection block (re-open in RGBA mode, compute stats, classify, print `[v40-alpha]` line); banner header updated from `v24` to `v24 + v40`; docstring extended with 12-line v40 history paragraph.
2. **Additive verification**: existing RGB stats block at `emit_stats()` is byte-identical to v24 (verified via diff in patch tool). The new alpha block is appended AFTER the existing `CLAMP DETECTED` print, never replacing or interleaving with the RGB output.
3. **Best-effort isolation**: alpha-inspection block is wrapped in try/except so RGB reporting is preserved even if RGBA-mode read fails. This matches v24's resilience pattern.
4. **Ladder consistency**: `classify_alpha_sentinel()` 5-pattern ladder mirrors v37's `check_alpha_sentinel()` ladder (saturated / zero / mixed / low / unknown). The two helpers classify identical evidence shapes identically.
5. **No GPU-path side effects**: file does not import nvrhi, does not reference any HLSL or C++ types, does not touch the dispatch chain. Pure stdlib + numpy/PIL post-hoc dump analyzer.
6. **Cumulative patch inventory intact** (verified via search_files at expected sites):
   - v3 spdlog markers at FGIPass.cpp + TestReSTIR_GI_Temporal.cpp — verified intact
   - v11/v12 cerr default-ON at FGIPass.cpp — verified intact
   - v13/v15 case 6u at both HLSL copies — verified intact
   - v17/v18/v19 additional sentinels (cases 7u/8u/9u/10u/11u/12u/15u + default) — verified intact
   - v22 binding-layout split at FGIPass.h:106 + FRayTracingPipeline.cpp:357/361 — verified intact
   - v28 alpha-channel sentinel at GIPathTracing.hlsl:694 (both copies) — verified intact
   - v37 validator alpha-check at validate_restir_gi.py:134 — verified intact
   - v38 cerr-write patch at FGIPass.cpp:477-491 — verified intact
   - v39 decoder at decode_v38_evidence.py — verified intact
   - bug-088 executeCommandList fix at TestReSTIR_GI_Temporal.cpp:691 — verified intact

## Single-head caveat
- Same model writes all 6 roles. Verdicts are self-checks. The implementation is mechanically simple (additive helper extension; no new dispatch logic) so the verdict is reproducible.

## Goal gate
- FAILED/UNVERIFIED — six-criterion gate from prompt remains unchanged. No `PIPELINE_GOAL_DONE_<date>.md` written.

## Recommendation
- KEEP. v40 cycle complete. v41 staged as next mechanically actionable file-only fix or as parent-evidence-gated decision-matrix target based on the v40 alpha-evidence shape that surfaces on parent's next terminal run.