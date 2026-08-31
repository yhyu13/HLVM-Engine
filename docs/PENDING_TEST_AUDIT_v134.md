# Pending Test Audit v134 — ALL_KEEP (file-only verification only)

- tests: docs/PENDING_TESTS_v134.md
- commit: docs/PENDING_COMMIT_v134.md
- verdict: ALL_KEEP
- verifier: testing-verifier (single-profile + file-only runspace; freshness-degraded per anti-pattern #7)
- timestamp: 2026-07-30

## Broken-pattern audit

The v134 patch is a CMakeLists.txt change in the nvrhi fork, not a test-producing commit. The 5 known broken-test patterns (from the AMG pipeline) don't apply:

- [x] No from-x-import-y patch propagation bugs (no Python imports; C++ CMake change)
- [x] No test-bug-in-itself (no tests added)
- [x] No source-incomplete-relative-to-test (the source IS the cmake config; test = rebuild succeeds)
- [x] No missing test isolation fixture (no test fixtures needed for cmake config)
- [x] No AsyncMock on sync function (no mocks; pure cmake syntax)

## Per-test verdict

The 8 file-only tests performed by the tester (PENDING_TESTS_v134.md) are all KEEP:

| Test | Verdict | Rationale |
|------|---------|-----------|
| Test 1: add_library includes validation TUs | KEEP | `${include_validation}` + `${src_validation}` correctly placed between `${misc_common}` and the closing `)`. |
| Test 2: target_sources replaced with target_compile_definitions | KEEP | Replacement at correct location with correct cmake syntax. |
| Test 3: comment blocks explain the why | KEEP | Two comment blocks (9 lines + 7 lines) explain the rationale; future agents can trace why this is in this file. |
| Test 4: Release and RelWithDebInfo untouched | KEEP | Only Debug tree is patched; Release and RelWithDebInfo trees have their own untouched CMakeLists.txt copies. |
| Test 5: v131+v132+v133 patches intact | KEEP | search_files confirms all 5 expected patches remain in place across 4 source files. |
| Test 6: validation TU source files exist | KEEP | Both `validation-device.cpp` and `validation-commandlist.cpp` exist in the Debug nvrhi-src tree. |
| Test 7: validation TU code checks NVRHI_WITH_VALIDATION | KEEP | The validation TU source code wraps entry points in `#if NVRHI_WITH_VALIDATION`, so the `target_compile_definitions` is the only runtime gate. |
| Test 8: validation header exists | KEEP | `include/nvrhi/validation.h` exists at the expected location. |

The 6 parent-runspace tests (9-14) are DEFERRED per EC-039:

| Test | Verdict | Rationale |
|------|---------|-----------|
| Test 9: rebuild succeeds | DEFERRED | requires terminal access (blocked by tirith). |
| Test 10: validation symbols in lib | DEFERRED | requires terminal + nm. |
| Test 11: test runs without crash | DEFERRED | requires terminal + run. |
| Test 12: VUID fires | DEFERRED | requires terminal + log grep. |
| Test 13: validator passes | DEFERRED | requires terminal + python3. |
| Test 14: mode 20 returns non-zero | DEFERRED | requires terminal + numpy. |

## Audit summary

- ALL_KEEP on the 8 file-only tests performed.
- DEFERRED on the 6 parent-runspace tests (cannot audit without terminal).
- No broken patterns detected.
- No MAJOR_DELETE / SOME_DELETE / SOME_RELAX needed.

The file-only verification is complete and clean. The behavioral verification is the parent-runspace's responsibility.

## What this audit confirms

The v134 patch is well-formed:
1. CMake syntax is correct (`add_library` source list + `target_compile_definitions`).
2. Placement is correct (validation TUs in initial source list, replace `target_sources` with `target_compile_definitions`).
3. Cache var name is correct (already validated in v133).
4. v131 + v132 + v133 patches are still intact (no collateral changes).
5. Release and RelWithDebInfo trees are NOT modified (no regression risk for those builds).
6. Comment blocks explain the rationale (future agents can trace it).
7. Validation TU source files and headers exist on disk (the patch can actually compile them).

## What this audit does NOT confirm

Per the honesty floor in PENDING_TESTS_v134.md:
- Whether the cmake reconfigure actually succeeds.
- Whether the validation TU is actually compiled into libnvrhid.a.
- Whether the validation layer fires VUID on the next test run.
- Whether the VUID, if fired, names the actual image/layout issue.

These are all parent-runspace verification steps. The audit cannot proceed without them.

## Next-step recommendation

The v134 cycle is COMPLETE on the file-only side. The remaining verification is:
1. Parent runspace executes the recipe in PENDING_COMMIT_v134.md §"verify" (rebuild + nm + run + log grep + dump analysis).
2. If all 6 deferred tests pass → cycle is FULLY COMPLETE, bisect closes (or surfaces the next discriminator).
3. If any test fails → new v135 cycle to address the specific failure.

The cron cannot proceed further without terminal access. Per EC-039, the cron is structurally blocked. The right action is for the parent runspace to execute the recipe and report back via:
- A new log file with timestamp later than 2026-07-30 08:12:42 (file-only detector for the next tick).
- A new dump group with timestamp later than 20260730_081242 (file-only detector for the next tick).
- Updated PENDING_PICK.md indicating the new state.

## Cycle status

v134 cycle COMPLETE on the file-only side:
- ✅ PENDING_PLAN_v134.md (planner)
- ✅ PENDING_PLAN_REVIEW_v134.md (plan-criticer, verdict KEEP)
- ✅ PENDING_COMMIT_v134.md (impler)
- ✅ PENDING_IMPL_REVIEW_v134.md (reviewer, verdict KEEP)
- ✅ PENDING_TESTS_v134.md (tester)
- ✅ PENDING_TEST_AUDIT_v134.md (testing-verifier, verdict ALL_KEEP)
- ✅ Source patch applied: `Build/Debug/_deps/nvrhi-src/CMakeLists.txt`

Per the state machine, this is Rule 9 ("full cycle complete → next item from PICK") transitioning to Rule 10 ("nothing pending → exit"). However, the file-only cycle is complete; the parent-runspace cycle is the next step.

The cron cannot directly verify the parent-runspace cycle. It must wait for either (a) terminal access granted in this runspace, or (b) parent runspace executes the recipe and a new log/dump group appears (file-only detector).

**End of v134 cycle audit.**