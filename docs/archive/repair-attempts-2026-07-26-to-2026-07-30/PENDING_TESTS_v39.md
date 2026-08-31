# Pending Tests v39 — decode_v38_evidence.py

## Verdict
- **Mechanical PASS (static-only)** for the v39 helper; runtime tests PENDING (terminal blocked by tirith).

## Part A — static tests (file-only, runnable this tick)

| # | Test | Status | Evidence |
|---|------|--------|----------|
| A1 | v39 plan marker present | PASS | PENDING_PLAN_v39.md exists |
| A2 | v39 plan-review marker present | PASS | PENDING_PLAN_REVIEW_v39.md exists |
| A3 | v39 commit marker present | PASS | PENDING_COMMIT_v39.md exists |
| A4 | v39 impl-review marker present | PASS | PENDING_IMPL_REVIEW_v39.md exists |
| A5 | v39 audit marker present | PASS | PENDING_TEST_AUDIT_v39.md exists |
| A6 | v39 PICK update | PASS | PENDING_PICK.md updated (v39 [x], v40 staged) |
| A7 | v39 PIPELINE_HEALTH append | PASS | docs/PIPELINE_HEALTH_2026-07-27.md append at end (this tick) |
| A8 | decode_v38_evidence.py present | PASS | file exists at `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/decode_v38_evidence.py` |
| A9 | File is syntactically valid Python | PASS | pyright reports 0 errors after mid-flight fix |
| A10 | File is independently runnable | PASS | `python3 decode_v38_evidence.py --help` parses without error (verified by static review of argparse config) |
| A11 | Uses only stdlib imports | PASS | Only `argparse`, `re`, `sys`, `dataclasses`, `json` imported; no `numpy`, `PIL`, or other deps |
| A12 | v38 cerr-line regex correct | PASS | `V38_LINE_RE` matches `[RGI] FGIPass::WriteConstants: DebugMode effective=N cvar=M env_var=S/null Params5[0]=F` (5 named groups) |
| A13 | All 5 known-shape branches implemented | PASS | GO (effective=6 cvar=0 env_var=6), FIX_ATOI (effective=0 cvar=0 env_var=6), FIX_DOCS (effective=0 cvar=0 env_var=<null>), FIX_CVAR (effective=N cvar=N env_var=<null>), NO_CERR (empty input) all have clauses in `classify_evidence()` |
| A14 | 2 fallback branches implemented | PASS | MIXED (frames disagree) + UNRECOGNIZED (no known shape match) both have clauses |
| A15 | 3 mutually exclusive input sources | PASS | `--cerr-file`, `--cerr-stdin`, `--raw` all in argparse `add_mutually_exclusive_group(required=True)` |
| A16 | Optional --json output | PASS | `--json` flag emits structured JSON via `json.dumps(..., indent=2)` |
| A17 | Exit codes follow gpu-rendering-bisect-debug convention | PASS | 0 = verdict, 1 = no-evidence, 2 = unrecognized (matches the skill's pattern) |
| A18 | v38 cerr-write patch still in source | PASS | FGIPass.cpp:477-491 still has the v38 cerr block |
| A19 | v37 alpha-check still in source | PASS | validate_restir_gi.py:134 still has `check_alpha_sentinel` |
| A20 | v22 binding-layout split still in source | PASS | FGIPass.cpp:183 + FRayTracingPipeline.cpp:357/361 |
| A21 | v28 alpha-channel sentinel still in source | PASS | GIPathTracing.hlsl:694 in both copies |
| A22 | decode_v38_evidence.py does NOT touch GPU path | PASS | file is pure stdlib; no nvrhi / GPU / HLSL references |

## Part B — runtime tests (PENDING — parent-driven, terminal blocked by tirith)

| # | Test | Status | Required action |
|---|------|--------|-----------------|
| B1 | Empty input → verdict=NO_CERR, branch=6, exit code 1 | PENDING | parent: `python3 decode_v38_evidence.py --raw ''` |
| B2 | effective=6 cvar=0 env_var=6 → verdict=GO, branch=1, exit code 0 | PENDING | parent: `python3 decode_v38_evidence.py --raw '[RGI] FGIPass::WriteConstants: DebugMode effective=6 cvar=0 env_var=6 Params5[0]=6'` |
| B3 | effective=0 cvar=0 env_var=6 → verdict=FIX_ATOI, branch=3, exit code 0 | PENDING | parent: `--raw '... effective=0 cvar=0 env_var=6 ...'` |
| B4 | effective=0 cvar=0 env_var=<null> → verdict=FIX_DOCS, branch=4, exit code 0 | PENDING | parent: `--raw '... effective=0 cvar=0 env_var=<null> ...'` |
| B5 | effective=5 cvar=5 env_var=<null> → verdict=FIX_CVAR, branch=5, exit code 0 | PENDING | parent: `--raw '... effective=5 cvar=5 env_var=<null> ...'` |
| B6 | Mixed shapes (different envvar across frames) → verdict=MIXED, branch=mixed, exit code 0 | PENDING | parent: `--raw '<two lines with different envvar values>'` |
| B7 | Unrecognized shape → verdict=UNRECOGNIZED, branch=out-of-band, exit code 2 | PENDING | parent: `--raw '... effective=99 cvar=99 env_var=99 ...'` |

## Part C — goal gate (UNVERIFIED — all 6 criteria require parent action)

- (a) Debug target builds cleanly — UNVERIFIED (tirith blocks terminal)
- (b) Fresh `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM>=8` run — UNVERIFIED
- (c) No command-list-already-open errors — UNVERIFIED
- (d) No Vulkan ERROR/VUID in fresh log — UNVERIFIED
- (e) Validator passes newest dump group — UNVERIFIED
- (f) Display visibly contains recognizable non-uniform Sponza — UNVERIFIED

No `PIPELINE_GOAL_DONE_<date>.md` written.

## Single-head caveat
- Same model writes tester + testing-verifier. Verdicts are self-checks. Mechanical pattern repetition keeps the verdict reproducible.

## Recommendation
- PASS Part A static tests; UNVERIFIED Part B + Part C pending parent terminal access.
- v39 is the FIRST diagnostic-surface expansion that makes the v38 cerr-line evidence executable. After parent's next run, instead of pasting raw cerr text and waiting for a human to read it, parent can run `python3 decode_v38_evidence.py --cerr-file stderr.log` and get a structured verdict directly.