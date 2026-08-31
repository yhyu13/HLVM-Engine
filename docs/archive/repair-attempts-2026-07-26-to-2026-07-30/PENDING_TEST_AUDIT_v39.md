# Pending Test Audit v39 — decode_v38_evidence.py

## Verdict
- **ALL_KEEP** — v39 helper is purely additive, mechanically correct, exhaustive in its 5+2 branch coverage, and ready for parent terminal-driven verification.

## Broken-pattern audit
- [x] No from-x-import-y patch propagation bugs (N/A — single new Python file)
- [x] No test-bug-in-itself (N/A — no test file modified)
- [x] No source-incomplete-relative-to-test (N/A — no test file modified)
- [x] No missing test isolation fixture (N/A — pure stdlib Python)
- [x] No AsyncMock on sync function or vice versa (N/A — pure Python parser)
- [x] No security scan failures (N/A — no shell injection, no eval, no SQL, no buffer overflows)
- [x] No -Werror cascade risk (Python file; -Werror is a C++ compiler flag, irrelevant here)

## Per-test verdict
- A1-A22: 22/22 PASS (static file-only verification)
- B1-B7: 7/7 UNVERIFIED (parent-driven, terminal blocked by tirith)
- C1-C6 (goal gate): UNVERIFIED — six criteria from prompt all require parent action

## Per-part verdict
- Part A (static): ALL_KEEP — 22/22 mechanical checks pass.
- Part B (runtime): UNVERIFIED — parent-driven, terminal required.
- Part C (goal gate): UNVERIFIED — six criteria from prompt remain unchanged.

## Specific audit findings
1. **File scope verified**: 10200-byte Python helper at `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/decode_v38_evidence.py`. Only stdlib imports (argparse, re, sys, dataclasses, json). No project-internal dependencies.
2. **Regex robustness verified**: `V38_LINE_RE` matches all 5 known cerr-line shapes documented in the v38 plan's decision matrix. Optional whitespace handling via `\s+` between fields.
3. **Branch coverage verified**: `classify_evidence()` handles 5 known shapes (GO / FIX_ATOI / FIX_DOCS / FIX_CVAR / NO_CERR) + 2 fallbacks (MIXED / UNRECOGNIZED). The 2 routing-condition branches (validator 4/4 PASS, parent cannot rebuild) are correctly out-of-script because they depend on validator output and parent-action state, not cerr-line shape.
4. **No GPU-path side effects**: file does not import nvrhi, does not reference any HLSL or C++ types, does not touch the dispatch chain. Pure stdlib parser.
5. **Cumulative patch inventory intact** (verified via search_files):
   - v3 spdlog markers at FGIPass.cpp + TestReSTIR_GI_Temporal.cpp — verified intact
   - v11/v12 cerr default-ON at FGIPass.cpp:498-510 — verified intact
   - v13/v15 case 6u at both HLSL copies — verified intact
   - v22 binding-layout split at FGIPass.cpp + FRayTracingPipeline.cpp — verified intact
   - v28 alpha-channel sentinel at GIPathTracing.hlsl:694 — verified intact
   - v37 validator alpha-check at validate_restir_gi.py:134 — verified intact
   - v38 cerr-write patch at FGIPass.cpp:477-491 — verified intact
   - bug-088 executeCommandList fix at TestReSTIR_GI_Temporal.cpp:691 — verified intact
6. **Decision matrix coverage**: 5 of 9 v39 branches in PENDING_PICK.md are cerr-shape-driven and handled by the script (branches 1, 3, 4, 5, 6). The remaining 4 (branches 2, 7, 8, 9) are routing-conditions:
   - Branch 2 (case 6u does NOT fire despite GO): routing-conditional, depends on `gi_raw` inspection
   - Branch 7 (upstream of WriteConstants): routing-conditional, depends on alpha-sentinel + cerr-shape combined
   - Branch 8 (validator 4/4 PASS): routing-conditional, depends on validator output
   - Branch 9 (parent cannot rebuild): routing-conditional, depends on parent action state
   The script correctly handles the cerr-shape-driven subset; the orchestrator handles the rest.
7. **Exit codes follow gpu-rendering-bisect-debug convention**: 0 = verdict, 1 = no-evidence, 2 = unrecognized. Matches the skill's documented pattern for diagnostic helpers.

## Single-head caveat
- Same model writes all 6 roles. Verdicts are self-checks. The implementation is mechanically simple (one new Python file with parser + classifier + argparse) so the verdict is reproducible.

## Goal gate
- FAILED/UNVERIFIED — six-criterion gate from prompt remains unchanged. No `PIPELINE_GOAL_DONE_<date>.md` written.

## Recommendation
- KEEP. v39 cycle complete. v40 staged as next mechanically actionable file-only fix or as parent-evidence-gated decision-matrix target based on the v39 decoder evidence shape that surfaces on parent's next terminal run.