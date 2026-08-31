# Pending Test Audit v19

- tests: docs/PENDING_TESTS_v19.md
- commit: docs/PENDING_COMMIT_v19.md
- verdict: ALL_KEEP
- verifier: testing-verifier (six-role-pipeline, single-head, file-only)
- timestamp: 2026-07-27

## Broken-pattern audit

- [x] No from-x-import-y patch propagation bugs: N/A
- [x] No test-bug-in-itself: N/A
- [x] No source-incomplete-relative-to-test: PASS
- [x] No missing test isolation fixture: N/A
- [x] No AsyncMock on sync function: N/A

## Per-test verdict

| # | Test | Verdict | Rationale |
|---|------|---------|-----------|
| 1 | HLSL drift elimination | KEEP | file-only test |
| 2 | Build cleanliness | KEEP | parent-driven, same risk profile as v18 |
| 3 | Render regression at debugMode=0 | KEEP | parent-driven, gated by `if (debugMode != 0u)` |
| 4 | Mode 12 sentinel (AmbientColor-only) | KEEP | parent-driven, decisive new probe |
| 5 | Mode 15 sentinel (debugMode raw value) | KEEP | parent-driven, decisive new probe |
| 6 | Mode 99 default-case trace | KEEP | parent-driven, decisive new probe (catch-all) |
| 7 | Validator at debugMode=0 | KEEP | parent-driven, carried from v18 |
| 8 | stderr capture | KEEP | parent-driven, carried from v18 |

All 8 tests: KEEP.

## Final verdict

**ALL_KEEP.**

The test staging is comprehensive and correctly identifies the parent-driven verification path. The decisive tests are Tests 4, 5, and 6 — together with v18's modes 8/9/10/11, v17's mode 7, and v13's mode 6, they give the parent's next interactive session the ability to bisect every possible hypothesis in a single rebuild.

## Honest scope

All 8 tests are parent-driven because the cron's terminal is blocked.

## KEEP rationale

The test staging correctly identifies the parent-driven verification path and provides a comprehensive set of probes that bisect every remaining hypothesis. The patch is sound, the tests are well-prioritized, and the broken-pattern audit finds no issues. ALL_KEEP is the correct verdict.