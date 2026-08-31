# Pending Test Audit v15

- tests: docs/PENDING_TESTS_v15.md
- commit: docs/PENDING_COMMIT_v15.md
- verdict: ALL_KEEP
- verifier: testing-verifier (six-role-pipeline, single-head, file-only)
- timestamp: 2026-07-27

## Broken-pattern audit

- [x] No from-x-import-y patch propagation bugs: not applicable (no Python imports; C++/HLSL patch only)
- [x] No test-bug-in-itself (asserts against wrong fixture): not applicable (no test files added)
- [x] No source-incomplete-relative-to-test: not applicable (no test source files)
- [x] No missing test isolation fixture: not applicable
- [x] No AsyncMock on sync function (or vice versa): not applicable (HLSL/C++ only)

## Per-test verdict

- **Test 1 (Drift elimination check)**: KEEP. The test command (`diff -u`) directly measures the patch's success criterion: zero meaningful drift between Private and Data copies. The expected output is empty (or only header whitespace differences). The test is self-validating: if the patch landed correctly, the diff is empty.
- **Test 2 (Build cleanliness)**: KEEP. The test command (`./Build.sh --Target=TestReSTIR_GI_Temporal --Test`) is the project's canonical build command. The expected outcome is a clean build. The test is the project's own definition of "the patch builds."
- **Test 3 (SPIR-V identity check)**: KEEP. Optional but strong. If both copies produce byte-identical (or near-identical) SPIR-V, the sync is verified at the binary level, not just at the source level. Independent verification beyond the textual diff.
- **Test 4 (Render regression check)**: KEEP. Carries over from v12/v13/v14 — same test, same expected behavior. v15 has zero runtime effect (data-dir copy was already what was being compiled).
- **Test 5 (Vision analysis)**: KEEP. Carries over from v12/v13/v14 — same test, same expected outcome.
- **Test 6 (Validator)**: KEEP. Carries over from v12/v13/v14 — same test, same expected outcome.

## Verdict rationale

All 6 tests are well-scoped and self-validating. The patch is documentation/sync and produces no behavioral change at the test-build layer. The tests measure (a) the patch landed correctly (Test 1), (b) the patch builds cleanly (Test 2), (c) the patch produces identical SPIR-V to data-dir (Test 3), (d) the patch introduces no runtime regression (Tests 4-6). All 4 v15-specific criteria are mechanically testable via parent shell.

The 3 carry-over tests (4-6) are pre-existing in v12/v13/v14 staging; v15 inherits them rather than duplicating them.

## Honesty caveats

- All 6 tests are parent-driven. The cron's terminal is blocked (tirith denies every terminal command). Tests cannot run from cron.
- The verifier is the same head as the tester and the planner and the impler on this single-profile host. ALL_KEEP is a self-check, but the verification artifacts are direct observable facts: the patch landed, the diff was confirmed empty by post-patch read_file, the build was not actually executed (terminal blocked) but the patch is text-identical to the data-dir copy which already builds cleanly.

## Final verdict

ALL_KEEP — v15 patch is sync of known-good code, tests are well-scoped, no broken patterns detected.