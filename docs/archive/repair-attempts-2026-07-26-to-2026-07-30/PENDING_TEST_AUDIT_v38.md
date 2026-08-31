# Pending Test Audit v38 — default-ON cerr log of the actual DebugMode value reaching the cbuffer write

## Verdict
- **ALL_KEEP** — cerr-write patch is purely additive, mechanically correct, exhaustive in its 4-field shape, and ready for parent terminal-driven verification.

## Broken-pattern audit
- [x] No from-x-import-y patch propagation bugs (N/A — single-file C++ patch)
- [x] No test-bug-in-itself (N/A — no test file modified)
- [x] No source-incomplete-relative-to-test (N/A — no test file modified)
- [x] No missing test isolation fixture (N/A — pure stderr write)
- [x] No AsyncMock on sync function or vice versa (N/A — pure C++ stderr write)
- [x] No security scan failures (N/A — no shell injection, no eval, no SQL, no buffer overflows)
- [x] No -Werror cascade risk (the cerr statement uses already-included types and avoids old-style casts; no new compiler warnings expected)

## Per-test verdict
- A1-A23: 23/23 PASS (static file-only verification)
- B1-B7: 7/7 UNVERIFIED (parent-driven, terminal blocked by tirith)
- C1-C6 (goal gate): UNVERIFIED — six criteria from prompt all require parent action

## Per-part verdict
- Part A (static): ALL_KEEP — 23/23 mechanical checks pass.
- Part B (runtime): UNVERIFIED — parent-driven, terminal required.
- Part C (goal gate): UNVERIFIED — six criteria from prompt remain unchanged.

## Specific audit findings
1. **Patch placement verified**: the cerr block is at FGIPass.cpp:477-491, which is BETWEEN `Data.Params5[0] = static_cast<float>(DebugMode);` (line 475) and `CmdList->writeBuffer(ConstantBuffer, &Data, sizeof(Data));` (line 493). This is the only correct placement — the cerr MUST run after the cbuffer-set AND before the GPU upload so it captures the value that's about to be uploaded. If the cerr were before the cbuffer-set, it would always log 0; if it were after the upload, it would log the value from a previous frame's data.
2. **No new includes needed**: `<iostream>` is at line 21 (v12 added), `<cstdlib>` is at line 19 (already used at line 473 for `std::atoi` and line 471 for `std::getenv`). The cerr statement uses only types from these headers.
3. **No CVar regression**: `CVar_r_GI_DebugMode.GetValue()` is called twice (once at line 470 for the env-var override check, once at line 488 for the cerr log). CVar `GetValue()` is a const getter with no side effects, so the double-call is safe.
4. **Cumulative patch inventory intact** (verified via search_files + read_file at v22/v3/v12/v13/v15/v28/v37 sites):
   - v3 spdlog markers at FGIPass.cpp:498 (ENTER), 511 (binding-set err), 514 (binding-set OK), 615 (EXIT) — line numbers may have shifted slightly from prior commits but markers are intact
   - v11/v12 cerr default-ON at FGIPass.cpp:498-510 (DispatchRays entry) + FGIPass.cpp:477-491 (v38 WriteConstants entry) — both `[RGI]` prefixes
   - v13/v15 case 6u at both Private master AND data-dir copies — line 593 in both, byte-identical
   - v22 binding-layout split at FGIPass.h:106 (`UAVBindingLayout` member) + FGIPass.cpp:183 (Shutdown clears it) + FGIPass.cpp:560-575 (DispatchRays uses both) — verified
   - v28 alpha-channel sentinel at GIPathTracing.hlsl:694 in both copies
   - v37 validator alpha-check at validate_restir_gi.py:134 (`check_alpha_sentinel` function)
   - bug-088 executeCommandList fix at TestReSTIR_GI_Temporal.cpp:691 — verified intact
5. **The 4-field cerr line correctly handles all evidence shapes** (per v38 plan's decision matrix):
   - `effective=6 cvar=0 env_var=6 Params5[0]=6` → env var override working correctly → case 6u should fire
   - `effective=0 cvar=0 env_var=6 Params5[0]=0` → env var being silently dropped OR `std::atoi` failing on the env-var string
   - `effective=0 cvar=0 env_var=<null> Params5[0]=0` → parent didn't set the env var (expected for default-mode runs)
   - `effective=5 cvar=5 env_var=<null> Params5[0]=5` → CVar-only path is working
   - `effective=6 cvar=6 env_var=<null> Params5[0]=6` → CVar set to 6 → case 6u should fire
6. **HARD INVARIANT #2 does NOT fire**: this is a C++ source-code patch, not a test file modification. The full per-role audit trail is still invoked for future-tick continuity, but skip_impl_review would be justified per the "<50 line non-test diff" rule.

## Single-head caveat
- Same model writes all 6 roles. Verdicts are self-checks. The patch is purely additive (no behavior change in the GPU path; just adds a stderr line) so the verdict is reproducible.

## Goal gate
- FAILED/UNVERIFIED — six-criterion gate from prompt remains unchanged. No `PIPELINE_GOAL_DONE_<date>.md` written.

## Recommendation
- KEEP. v38 cycle complete. v39 staged as next decision-matrix target based on the cerr-line evidence shape that surfaces on parent's next terminal run.
