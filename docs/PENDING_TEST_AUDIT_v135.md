# Pending Test Audit v135 — ALL_KEEP (file-only verification only)

- tests: docs/PENDING_TESTS_v135.md
- commit: docs/PENDING_COMMIT_v135.md
- verdict: ALL_KEEP
- verifier: testing-verifier (file-only runspace; freshness-degraded per anti-pattern #7)
- timestamp: 2026-07-30

## Broken-pattern audit

The v135 patch is a single-file C++ edit (FGIPass.cpp only), adding a `commitBarriers()` call. The 5 known broken-test patterns don't apply:

- [x] No from-x-import-y patch propagation bugs (no Python imports; C++ GPU command)
- [x] No test-bug-in-itself (no test files added)
- [x] No source-incomplete-relative-to-test (the source IS the barrier commit; test = rebuild + run succeeds)
- [x] No missing test isolation fixture (no test fixtures needed)
- [x] No AsyncMock on sync function (no mocks; pure C++ command)

## Per-test verdict

The 8 file-only tests performed by the tester (PENDING_TESTS_v135.md) are all KEEP:

| Test | Verdict | Rationale |
|------|---------|-----------|
| Test 1: New `commitBarriers()` at correct location | KEEP | search_files confirms 2 matches at lines 562 (new) and 675 (existing v131); new one is between setTextureState and SRVBuilder. |
| Test 2: Existing commitBarriers() at line 675 INTACT | KEEP | search_files confirms the line 675 commitBarriers() is still present with its 6-line comment block. |
| Test 3: WriteConstants unchanged | KEEP | line 543 `WriteConstants(CmdList, Desc);` unchanged. |
| Test 4: Three setTextureState calls unchanged | KEEP | lines 547-555 unchanged (only one was at line 555 in original, now slightly shifted). |
| Test 5: Comment block explains v135 fix | KEEP | 6-line comment at lines 557-562 explains the nvrhi-deferred-barrier-ordering pattern. |
| Test 6: All 4 prior-cycle patches intact | KEEP | search_files confirms v131 commitBarriers at line 675, v132 createValidationLayer at DeviceManagerVk4_LifeCycle.cpp:88, v133 cmake FORCE at Engine/Source/Runtime/CMakeLists.txt:182, v134 add_library validation TUs at _deps/nvrhi-src/CMakeLists.txt:213-214. |
| Test 7: SRVBuilder chain unchanged | KEEP | SetConstantBuffer/SetTextureSRV calls at lines 565+ unchanged. |
| Test 8: createBindingSet calls unchanged | KEEP | lines 615-616 and 656-657 unchanged. |

The 6 parent-runspace tests are DEFERRED per EC-039:

| Test | Verdict | Rationale |
|------|---------|-----------|
| Test 9: mode 20 returns non-zero | DEFERRED | requires terminal + run. |
| Test 10: mode 22 returns non-zero | DEFERRED | requires terminal + run. |
| Test 11: validate_restir_gi.py passes | DEFERRED | requires terminal + python3. |
| Test 12: vision check shows Sponza | DEFERRED | requires terminal + vision_analyze. |
| Test 13: no Vulkan VUID/ERROR | DEFERRED | requires terminal + log grep. |
| Test 14: no command-list errors | DEFERRED | requires terminal + log grep. |

## Audit summary

- ALL_KEEP on the 8 file-only tests performed.
- DEFERRED on the 6 parent-runspace tests (cannot audit without terminal).
- No broken patterns detected.
- No MAJOR_DELETE / SOME_DELETE / SOME_RELAX needed.

The file-only verification is complete and clean. The behavioral verification is the parent-runspace's responsibility.

## What this audit confirms

The v135 patch is well-formed:
1. CMake syntax is N/A (C++ only).
2. Placement is correct (between setTextureState and SRVBuilder chain).
3. Defense-in-depth: existing commitBarriers() at line 675 is INTACT.
4. v131 + v132 + v133 + v134 patches are still intact (no collateral changes).
5. Comment block explains the rationale (future agents can trace why this is in this file).

## What this audit does NOT confirm

Per the honesty floor in PENDING_TESTS_v135.md:
- Whether the cmake reconfigure succeeds.
- Whether the validation symbols are now in libnvrhid.a.
- Whether the validation layer fires VUID.
- Whether mode 20 returns non-zero (the actual root-cause test).
- Any dump was analyzed.

These are all parent-runspace verification steps. The audit cannot proceed without them.

## Next-step recommendation

The v135 cycle is COMPLETE on the file-only side. The remaining verification is:
1. Parent runspace executes the recipe in PENDING_COMMIT_v135.md §"verify" (rebuild + run + log grep + dump analysis).
2. If mode 20 returns non-zero → root cause was barrier ordering, bisect closes.
3. If mode 20 still returns zero → barrier ordering was NOT the root cause; v136 would address the next hypothesis (slangc dead-strip, pipeline cache staleness, or Vulkan validation layer needed).

The cron cannot proceed further without terminal access. Per EC-039, the cron is structurally blocked. The right action is for the parent runspace to execute the recipe and report back via:
- A new log file with timestamp later than 2026-07-30 08:12:42 (file-only detector for the next tick).
- A new dump group with timestamp later than 20260730_081242 (file-only detector for the next tick).
- Updated PENDING_PICK.md indicating the new state.

## Cycle status

v135 cycle COMPLETE on the file-only side:
- ✅ PENDING_PLAN_v135.md (planner)
- ✅ PENDING_PLAN_REVIEW_v135.md (plan-criticer, verdict KEEP)
- ✅ PENDING_COMMIT_v135.md (impler)
- ✅ Source patch applied: `Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp` +7/-0 lines
- ✅ PENDING_IMPL_REVIEW_v135.md (reviewer, verdict KEEP)
- ✅ PENDING_TESTS_v135.md (tester, 8 file-only tests PASS)
- ✅ PENDING_TEST_AUDIT_v135.md (testing-verifier, verdict ALL_KEEP)

Per the state machine, this is Rule 9 ("full cycle complete → next item from PICK"). However, the file-only cycle is complete; the parent-runspace cycle is the next step.

The cron cannot directly verify the parent-runspace cycle. It must wait for either (a) terminal access granted in this runspace, or (b) parent runspace executes the recipe and a new log/dump group appears (file-only detector).

**End of v135 cycle audit.**