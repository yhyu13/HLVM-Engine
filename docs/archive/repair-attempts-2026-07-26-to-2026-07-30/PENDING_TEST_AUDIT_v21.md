# Pending Test Audit v21

- tests: docs/PENDING_TESTS_v21.md
- commit: docs/PENDING_COMMIT_v21.md
- verdict: SOME_RELAX
- verifier: cron (single-head; same model as planner + impler + reviewer; freshness caveat applies per six-role-pipeline anti-pattern #7)
- timestamp: 2026-07-27

## Broken-pattern audit

v21 is a plan-only cycle. No C++ test files were added, no source code was modified. The 5 known broken-test patterns from the testing-verifier role are all N/A for this cycle:

- [x] No from-x-import-y patch propagation bugs — N/A (no Python tests; no patches)
- [x] No test-bug-in-itself (asserts against wrong fixture) — N/A (no test code added)
- [x] No source-incomplete-relative-to-test — N/A (no source modified)
- [x] No missing test isolation fixture — N/A
- [x] No AsyncMock on sync function (or vice versa) — N/A

## Per-test verdict

PENDING_TESTS_v21.md has 9 staged tests (4 in Part A: plan-only; 5 in Part B: v21a conditional):

### Part A (v21 plan-only cycle, immediately verifiable in this tick)

- Test A1 (marker completeness): cron-verifiable via `ls docs/PENDING_*_v21.md` → PASS (all 5 files present: PLAN, PLAN_REVIEW, COMMIT, IMPL_REVIEW, this audit).
- Test A2 (decision tree coverage): cron-verifiable via `grep` of v21 plan → PASS (v21a..v21i branches documented; v21a is the staged branch).
- Test A3 (hypothesis #1 evidence chain): cron-verifiable via `grep` of v21 plan → PASS (v22/v23 heartbeats' hypothesis #1 cited in "Why this cycle is correct" section).
- Test A4 (source-code gating): cron-verifiable via `git status` (parent-driven; cron can check via search_files that FGIPass.cpp/FRayTracingPipeline.cpp were not modified) → PASS (no source-code changes in the v21 cycle).

### Part B (v21a binding-layout-split, conditionally exercisable)

- Test B1 (build cleanliness): parent-driven; cron cannot verify (terminal blocked).
- Test B2 (DeviceManager.cpp:52 warning suppression): parent-driven; cron cannot verify.
- Test B3 (gi_raw non-zero): parent-driven; cron cannot verify (no vision tool, no fresh dumps).
- Test B4 (validator 3/3): parent-driven; cron cannot verify.
- Test B5 (display frame visible Sponza): parent-driven; cron cannot verify (no vision tool, no fresh dumps).

All 5 Part B tests are parent-driven. None can be verified by the file-only cron in this cycle.

## Verdict rationale

SOME_RELAX (not ALL_KEEP) because:

1. **Part A tests are immediately verifiable but were self-verified by the cron as the markers were written.** The tests' own existence on disk is the test; the marker pair (PLAN + PLAN_REVIEW + COMMIT + IMPL_REVIEW + this audit) is the deliverable.
2. **Part B tests cannot be verified by the file-only cron.** The actual binding-layout-split code change (v21a) is staged in PENDING_PLAN_v21.md but NOT applied. The cron correctly defers v21a application until parent confirms the v20 evidence shape.
3. **The v21 cycle's value is decision-tree preparation, not code change.** The cron has identified the highest-confidence root cause (nvrhi-deferred-barrier-ordering) and pre-staged the fix. When parent runs the v20 diagnostic and confirms the hypothesis, v21a can be applied in the next cycle without further investigation.

ALL_KEEP would be honest only if the cron could execute the v21a fix and verify it. The cron cannot (terminal blocked, no fresh build capability). The plan-only nature of v21 is the correct file-only action given the structural constraint.

## What would unblock the verdict

A single parent-driven run:

```bash
cd /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine
bash Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/run_rgi_diagnostic.sh
```

…which produces `rgi_evidence.txt` with the consolidated 9-branch evidence. The parent pastes the contents back to the cron; the cron routes to:

- **v21a** (if hypothesis #1 confirmed: nvrhi-deferred-barrier-ordering) → apply FGIPass binding-layout split
- **v21b** (if hypothesis #2 confirmed: AmbientColor uniform bind) → apply AmbientColor fallback
- **v21c..v21i** (other branches from the decision tree) → stage and apply respective fixes

After any v21a..v21i fix is applied, the parent re-runs the diagnostic to verify. If 3/3 validator + visible Sponza geometry, write `docs/PIPELINE_GOAL_DONE_2026-07-27.md` and mark v0 task `[x]`.

## Acceptance criteria status

Carried over from cron prompt and PENDING_TESTS_v20.md:

- (a) Debug target builds cleanly — UNVERIFIED for v21a fix (parent-driven; will be re-verified after v21a is applied and rebuilt)
- (b) Fresh `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM>=8` run — UNVERIFIED
- (c) No command-list-already-open errors — UNVERIFIED; if v21a is applied, this is the primary success criterion (warning count should drop from 7 to 0)
- (d) No Vulkan ERROR/VUID in fresh log — UNVERIFIED
- (e) Validator passes newest dump group — UNVERIFIED
- (f) Display visibly contains recognizable non-uniform Sponza — UNVERIFIED
- (g) Relevant checks pass — UNVERIFIED

No `PIPELINE_GOAL_DONE_<date>.md` written. Final-goal gate remains failed/unverified pending v20 evidence + v21a (or v21b..v21i) application + parent verification.
