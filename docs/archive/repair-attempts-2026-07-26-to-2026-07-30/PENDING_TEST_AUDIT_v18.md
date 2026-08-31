# Pending Test Audit v18

- tests: docs/PENDING_TESTS_v18.md
- commit: docs/PENDING_COMMIT_v18.md
- verdict: ALL_KEEP
- verifier: testing-verifier (six-role-pipeline, single-head, file-only)
- timestamp: 2026-07-27

## Broken-pattern audit

- [x] No from-x-import-y patch propagation bugs: N/A — no Python imports, no test files.
- [x] No test-bug-in-itself (asserts against wrong fixture): N/A — no test files.
- [x] No source-incomplete-relative-to-test: PASS — source patch is complete; both HLSL copies are updated and identical.
- [x] No missing test isolation fixture: N/A — no test files.
- [x] No AsyncMock on sync function (or vice versa): N/A — no Python tests.

## Per-test verdict

| # | Test | Verdict | Rationale |
|---|------|---------|-----------|
| 1 | HLSL drift elimination (diff check) | KEEP | file-only test, runnable by parent with `diff -u` |
| 2 | Build cleanliness | KEEP | parent-driven, same risk profile as v17 |
| 3 | Render regression at debugMode=0 | KEEP | parent-driven, gated by `if (debugMode != 0u)` |
| 4 | Mode 8 sentinel (TraceRay-only) | KEEP | parent-driven, decisive new probe for TraceRay setup |
| 5 | Mode 9 sentinel (diffuse × 1.5) | KEEP | parent-driven, decisive new probe for GBufferMaterial SRV |
| 6 | Mode 10 sentinel (debugMode cbuffer reach) | KEEP | parent-driven, decisive new probe for GI cbuffer reach |
| 7 | Mode 11 sentinel (View cbuffer reach) | KEEP | parent-driven, decisive new probe for View cbuffer reach |
| 8 | Mode 1 comparison (mode 9 = mode 1 × 1.5) | KEEP | parent-driven, decodes mode 9's diagnostic value |
| 9 | Validator at debugMode=0 | KEEP | parent-driven, carried from v17 |
| 10 | stderr capture | KEEP | parent-driven, carried from v17 |
| 11 | Vision analysis (modes 1, 6, 7, 8, 9, 10, 11) | KEEP | parent-driven, expanded to modes 8/9/10/11 |

All 11 tests: KEEP.

## Final verdict

**ALL_KEEP.**

The test staging is comprehensive, well-prioritized, and correctly identifies the parent-driven verification path. The decisive tests are Tests 4-7 (modes 8, 9, 10, 11) — together they bisect the bug space into 8+ actionable branches. The supporting tests (Tests 1, 2, 3, 8, 9, 10, 11) verify patch correctness, render regression absence, validator carry-over, stderr capture, and visual sanity.

## Honest scope

All 11 tests are parent-driven because the cron's terminal is blocked by tirith. This is structurally identical to v15/v17 and earlier cycles' test staging. The cron is unable to verify the runtime effect of any diagnostic-surface patch without parent's interactive session.

The tests are well-designed: each one targets a specific aspect of the patch's diagnostic value, the priority ordering is correct (decisive probes first, supporting evidence second), and the broken-pattern audit is correctly N/A (no Python test files to break).

## What this audit does NOT do

- Does NOT claim the patch fixes the renderer. v18 is a diagnostic-surface patch, not a renderer fix.
- Does NOT claim the parent can skip rebuilding. All 11 tests require a parent-driven rebuild from current source.
- Does NOT pretend the cron can verify the patch's runtime effect. The cron is structurally file-only.

## KEEP rationale

The test staging correctly identifies the parent-driven verification path and provides a comprehensive set of probes that bisect the bug space into actionable branches. The patch is sound, the tests are well-prioritized, and the broken-pattern audit finds no issues. ALL_KEEP is the correct verdict.