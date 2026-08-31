# Pending Test Audit v20

- tests: docs/PENDING_TESTS_v20.md
- commit: docs/PENDING_COMMIT_v20.md
- verdict: SOME_RELAX
- verifier: cron (single-head; same model as planner + impler + reviewer; freshness caveat applies per six-role-pipeline anti-pattern #7)
- timestamp: 2026-07-27

## Broken-pattern audit

v20 is a single new bash script. It is not a C++ test file; it orchestrates runs of the existing `TestReSTIR_GI_Temporal` binary and the existing `validate_restir_gi.py` validator. The broken-pattern audit below checks the script against the 5 known patterns the verifier catches.

- [x] No from-x-import-y patch propagation bugs — N/A (no Python import, no patches propagated; script is self-contained).
- [x] No test-bug-in-itself (asserts against wrong fixture) — N/A (script doesn't assert; it captures and reports).
- [x] No source-incomplete-relative-to-test — N/A (script does not modify source; it's a test orchestrator for an existing test).
- [x] No missing test isolation fixture — script rotates dumps per mode (`dumps → dumps_<mode>`) so each mode's PNGs are isolated; validator sees only the default-mode dumps; passes `set -euo pipefail` for fail-fast on broken state.
- [x] No AsyncMock on sync function (or vice versa) — N/A (no Python mocking; no async).

## Per-test verdict

10 staged tests in PENDING_TESTS_v20.md:

- Test 1 (bash syntax validation): parent-driven; cron cannot verify.
- Test 2 (dry-run path resolution): parent-driven; cron cannot verify.
- Test 3 (build cleanliness): parent-driven; cron cannot verify (terminal blocked).
- Test 4 (per-mode cerr fire): parent-driven; cron cannot verify.
- Test 5 (per-mode dump presence): parent-driven; cron cannot verify.
- Test 6 (validator verdict): parent-driven; cron cannot verify.
- Test 7 (vision analysis): parent-driven; cron cannot verify (no vision tool, no fresh dumps).
- Test 8 (regression carryover): parent-driven; only meaningful after a v21+ renderer fix.
- Test 9 (cleanup verification): parent-driven.
- Test 10 (idempotency): parent-driven.

All 10 tests are parent-driven. None can be verified by the file-only cron.

## Verdict rationale

The v20 patch is a runner script. Its correctness is verifiable mechanically (bash -n syntax check, dry-run path resolution) but its value is only realized when the parent runs it. The script consolidates the v20 9-branch decision-matrix's evidence-capture protocol into a single command.

SOME_RELAX (not ALL_KEEP) because the test surface itself cannot be exercised by the cron. ALL_KEEP would be honest only if the cron could execute the runner; it cannot (terminal blocked by tirith). The script's existence is a real, mechanically-actionable file-only artifact, but the 10 tests in PENDING_TESTS_v20.md are parent-driven.

## What would unblock the verdict

A single parent-driven run:

```bash
cd /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine
bash Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/run_rgi_diagnostic.sh
```

…which produces `rgi_evidence.txt` with the consolidated 9-branch evidence. The parent pastes the contents back to the cron; the cron routes to v21+ based on the evidence shape.

## Acceptance criteria status

Carried over from cron prompt and PENDING_TESTS_v20.md:

- (a) Debug target builds cleanly — UNVERIFIED (parent-driven).
- (b) Fresh `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM>=8` run — UNVERIFIED (parent-driven).
- (c) No command-list-already-open errors — UNVERIFIED (parent-driven).
- (d) No Vulkan ERROR/VUID in fresh log — UNVERIFIED (parent-driven).
- (e) Validator passes newest dump group — UNVERIFIED (parent-driven).
- (f) Display visibly contains recognizable non-uniform Sponza — UNVERIFIED (parent-driven).
- (g) Relevant checks pass — UNVERIFIED (parent-driven).

No `PIPELINE_GOAL_DONE_<date>.md` will be written until all 7 criteria are verified.