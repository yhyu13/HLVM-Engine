# Pending Test Audit v13

- tests: docs/PENDING_TESTS_v13.md
- commit: docs/PENDING_COMMIT_v13.md
- verdict: ALL_KEEP
- verifier: testing-verifier (six-role-pipeline, single-head, file-only)
- timestamp: 2026-07-27

## Broken-pattern audit

- [x] No from-x-import-y patch propagation bugs: N/A (no Python test files; HLSL patch only)
- [x] No test-bug-in-itself (asserts against wrong fixture): N/A (no new tests; existing test harness applies unchanged)
- [x] No source-incomplete-relative-to-test: N/A (the patch is in source; tests are parent-driven)
- [x] No missing test isolation fixture: N/A (no new tests)
- [x] No AsyncMock on sync function (or vice versa): N/A (no mocks)

## Per-test verdict

| Test | Verdict | Rationale |
|------|---------|-----------|
| 1: Build cleanliness | KEEP | Standard build verification; no new test surface |
| 2: Default-mode run (v12 evidence) | KEEP | Reuses v12's parent-driven test plan; still relevant |
| 3: v13 mode-6 run (the new test) | KEEP | The decisive test for the dispatch body; per-pixel-stats analysis distinguishes all 4 evidence shapes |
| 4: Vision analysis | KEEP | Per gpu-rendering-bisect-debug skill, vision analysis is required for "is the image actually correct" |
| 5: Validator | KEEP | The project's own validator is the only thing that gets to say "suite green" |

## Verdict summary

ALL_KEEP. The test plan is sound. The v13 patch's effect is observable only when (a) the binary is rebuilt, AND (b) `HLVM_PT_DEBUG_MODE=6` is set. Both conditions require parent action. The test plan covers all four evidence shapes from the v13 plan's decision matrix (per-pixel gradient, all zeros, garbage, single uniform value).

The v13 patch is a probe, not a fix. The test plan's pass criteria are about producing diagnostic information, not about the renderer being correct. The cron cannot claim the v13 patch "fixes" the renderer — it claims the patch is the next maximally-informative file-only step given the structural terminal block.

## Honesty caveats

- All 6 roles are the same head (single-profile, single-prompt host). KEEP verdicts are self-checks.
- The cron's terminal is still blocked. Tests cannot run from cron.
- The v12 cerr patch and v13 mode-6 patch are in source but their effects require a parent rebuild to observe.
- The v12 + v13 evidence will tell us whether the bug is in (a) the dispatch body (H-A or H-B: source/binary mismatch or spdlog config), (b) the lighting/payload math (mode-6 shows gradient, mode-0 gi_raw=0), or (c) downstream of the dispatch (mode-6 shows garbage or uniform value).
