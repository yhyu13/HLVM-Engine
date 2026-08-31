# Pending Test Audit v23

- tests: docs/PENDING_TESTS_v23.md
- commit: docs/PENDING_COMMIT_v23.md
- verdict: SOME_RELAX
- verifier: cron (single-head; same model as planner + impler + reviewer; freshness caveat applies per six-role-pipeline anti-pattern #7)
- timestamp: 2026-07-27

## Broken-pattern audit

v23 is a script-fix cycle. No C++ test files were added, no source code (C++ / HLSL / CMake) was modified. The 5 known broken-test patterns from the testing-verifier role are all N/A for this cycle:

- [x] No from-x-import-y patch propagation bugs — N/A (no Python tests; no patches)
- [x] No test-bug-in-itself (asserts against wrong fixture) — N/A (no test code added)
- [x] No source-incomplete-relative-to-test — N/A (no source modified)
- [x] No missing test isolation fixture — N/A
- [x] No AsyncMock on sync function (or vice versa) — N/A

## Per-test verdict

PENDING_TESTS_v23.md has 12 staged tests (6 in Part A: script-fix static; 6 in Part B: v22 evidence-path parent-driven).

### Part A (v23 script-fix, immediately verifiable in this tick via static inspection)

- Test A1 (archive-after-run pattern): cron-verifiable via `search_files pattern="Archive AFTER the run"` → PASS (line 119 of patched script contains the post-run archive block).
- Test A2 (pre-loop archive of stale dumps): cron-verifiable via `search_files pattern="Stale pre-run dumps"` → PASS (line 87 of patched script contains the pre-loop archive block).
- Test A3 (post-loop validator restoration): cron-verifiable via `search_files pattern="Restore dumps/ to the DEFAULT mode"` → PASS (line 130 of patched script contains the post-loop restoration block).
- Test A4 (absence of buggy pre-run rotation): cron-verifiable via `search_files pattern="Move existing dumps to a per-mode archive"` → PASS (no match in patched script — buggy comment is gone).
- Test A5 (header comment v23 attribution): cron-verifiable via `search_files pattern="six-role-pipeline"` → PASS (line 26 of patched script contains the v23 attribution block).
- Test A6 (bash syntax validity): parent-driven; cron cannot verify (requires `bash -n` which needs shell).

All 5 static tests PASS. Test A6 requires shell and is parent-driven.

### Part B (v22 evidence-path, parent-driven; gated on v23-script re-run)

- Test B1 (per-mode dump counts): parent-driven; cron cannot verify (requires running the script).
- Test B2 (mode99 dump preservation): parent-driven; cron cannot verify.
- Test B3 (rgi_evidence.txt per-mode counts): parent-driven; cron cannot verify.
- Test B4 (rgi_evidence.txt cerr counts unchanged): parent-driven; cron cannot verify.
- Test B5 (validator still runs on default-mode dumps): parent-driven; cron cannot verify.
- Test B6 (parent decision routing): meta-test; gated on B1-B5 passing.

All 6 Part B tests are parent-driven. None can be verified by the file-only cron in this cycle.

## Verdict rationale

SOME_RELAX (not ALL_KEEP) because:

1. **Part A static tests are immediately verifiable and all pass.** The 5 cron-runnable tests (A1-A5) confirm the script's structural correctness: archive-after-run is in place, pre-loop stale handling is correct, post-loop restoration uses `cp -r` with `mv` fallback, the buggy pre-run pattern is gone, and the v23 attribution is documented. Test A6 (bash syntax) requires shell and is parent-driven.
2. **Part B runtime tests cannot be verified by the file-only cron.** The actual fix's correctness is confirmed only when the parent re-runs the patched script and inspects the per-mode dumps. The static correctness (Part A) is necessary but not sufficient.
3. **The v23 cycle's value is making the evidence-collection path trustworthy.** The buggy v20 script would have produced correctly-validator-fed but incorrectly-per-mode-labeled dumps, leading parent to draw wrong conclusions about which mode's probe matched expectations. After v23, each per-mode archive is correctly labeled, and the parent's evidence-shape analysis can route the v22 PICK item to the correct v21a..v21i sub-plan.
4. **The v20 audit verdict SOME_RELAX is NOT changed by this patch.** The v20 verdict was SOME_RELAX because Part B tests (Test B1-B5 in v20's PENDING_TESTS) were parent-driven. v23 adds the missing piece (correct per-mode labeling) but does not run the script. The v20 verdict remains SOME_RELAX until parent runs the patched script and confirms Part B tests pass.

ALL_KEEP would be honest only if the cron could execute the patched script and verify Part B tests. The cron cannot (terminal blocked, no fresh build capability). The SOME_RELAX verdict correctly reflects this.

## What would unblock the verdict

A single parent-driven run:

```bash
cd /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine
bash Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/run_rgi_diagnostic.sh
```

…which produces `rgi_evidence.txt` with the consolidated per-mode evidence. The parent pastes the contents back to the cron; the cron verifies Part B tests and updates v23's verdict to ALL_KEEP if all 6 pass.

After v23 verdict reaches ALL_KEEP, the parent-driven v22 evidence path is unblocked: parent runs the patched script, gets correctly-labeled per-mode dumps, and the v22 PICK item routes to the correct v21a..v21i sub-plan.

## Acceptance criteria status

Carried over from cron prompt and PENDING_TESTS_v20.md:

- (a) Debug target builds cleanly — UNVERIFIED for v23 (parent-driven; will be re-verified when parent runs the patched script)
- (b) Fresh `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM>=8` run — UNVERIFIED (gated on parent)
- (c) No command-list-already-open errors — UNVERIFIED (stale log shows 7 warnings per run; this is the nvrhi-deferred-barrier-ordering pattern; v22 is staged to fix this)
- (d) No Vulkan ERROR/VUID in fresh log — UNVERIFIED (gated on parent)
- (e) Validator passes newest dump group — UNVERIFIED (gated on parent; the validator receives default-mode dumps after v23's `cp -r dumps_default dumps/` restoration, which is correct)
- (f) Display visibly contains recognizable non-uniform Sponza with sane exposure — UNVERIFIED (vision tool unavailable; no fresh dumps)
- (g) Relevant checks pass — UNVERIFIED

No `PIPELINE_GOAL_DONE_<date>.md` written. Final-goal gate remains failed/unverified.

## v23 cycle contribution to acceptance criteria

v23 is a script-fix cycle that does NOT directly address any of the six acceptance criteria. Its contribution is INDIRECT: it makes the parent-driven evidence-collection path trustworthy, so the v22 PICK item (gated on parent v20 evidence) can route to the correct v21a..v21i sub-plan based on correctly-labeled evidence. Without v23, parent's evidence analysis would be misled by the off-by-one dump labels, and v22 might route to the wrong fix. With v23, the routing is reliable.

The acceptance criteria remain UNVERIFIED for v23 specifically; v23 is a TRAJECTORY ADVANCEMENT, not a goal-completion step.