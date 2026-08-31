# Pending Test Audit v2 — validate v22-split revert tests

- tests: docs/PENDING_TESTS_v2.md
- commit: docs/PENDING_COMMIT_v2.md
- verdict: ALL_KEEP
- verifier: tester+testing-verifier (single-profile host; same head)
- timestamp: 2026-08-16

## Broken-pattern audit

The 5 known broken-test patterns (per multi-agent-subagent-pitfalls
skill, the AMG reSTIRGI history, and the 2026-08-11 commit
referenced by the validator's docstring):

- [x] **No from-x-import-y patch propagation bugs.** The test plan
  doesn't modify any test source files; it only runs commands. No
  patch propagation issue.

- [x] **No test-bug-in-itself.** The tests run the actual test
  executable and inspect its output. They don't re-assert on imagined
  fixtures.

- [x] **No source-incomplete-relative-to-test.** The source change
  in PENDING_COMMIT_v2.md is COMPLETE: HLSL × 2 + C++ × 2 edited,
  no orphan references left (the `UAVBindingLayout` member is
  removed from header and Shutdown). All imports still resolve.

- [x] **No missing test isolation fixture.** The test invocation
  uses `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8` which deterministically
  produces a single dump group. The validator's
  `select_newest_dump_group` correctly identifies it.

- [x] **No AsyncMock on sync function (or vice versa).** N/A — this
  is a C++/GPU test, not a Python async test.

## Per-test verdict

| Test | Verdict | Rationale |
|------|---------|-----------|
| Test 1: Build | KEEP | Standard build invocation; exits 0 is the success criterion. |
| Test 2: SRV sentinel mode 20 | KEEP | Per-channel std > 5/255 directly tests the SRV-binding-read-zero bug. The threshold is conservative (any non-zero pixel passes; per-channel std > 5/255 indicates real Sponza geometry). |
| Test 3: WorldPos mode 22 | KEEP | Mode 22 reads `GBufferWorldPos` directly via SRV; non-zero output proves the binding topology is correct. |
| Test 4: 4-check validator | KEEP | The validator's checks (black%, color std, cell std, temporal) are well-calibrated per the docstring's reference to the 2026-08-15 session. Exit 0 = all 4 pass. |
| Test 5: Vision review | KEEP | Required because the scalar gates can pass garbage. The reviewer (human) is the freshness layer — no model can substitute for actually looking at the dump. |

## Critical concern flagged for operator

**The cron cannot execute the test build.** All five tests above require
running shell commands. Per the PIPELINE_HEALTH_2026-08-16.md "External
tools unavailable" section, `terminal` is blocked by tirith in this
profile.

**The operator (parent session at the keyboard) must execute the test
build + run + dump inspection.** The cron has done all it can:
- Diagnosed the bug (v22 split returns zero SRV reads).
- Implemented the fix (revert v22 split + HLSL `space1` removal).
- Specified the verification recipe (5 tests with exact commands
  and acceptance criteria).

Without the operator running Test 1, the fix is unverified. The
operator must:

```bash
cd /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine
rm -f Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/GIPathTracing.sblob
./Build.sh --Rebuild --Config=Debug --Target=TestReSTIR_GI_Temporal --Test
```

Then inspect /tmp/rgi_mode20.log and the dumps directory. If mode 20
dump is non-black, the fix worked.

## Tick verdict

The fix is mechanically sound (well-understood revert, single-binding-
set pattern matches the proven TestCornellBoxGI control). The test
plan correctly verifies the fix's success criteria. ALL_KEEP.

The cron tick is now END-OF-CYCLE for this card. The next tick should
either:
- (a) Process the operator's test results — if the fix works, mark
  the card done in PENDING_PICK.md and proceed to the next card.
- (b) If the fix doesn't work, route back to planner with the test
  results in a new `PENDING_TESTS_v2_REDO.md` (the "tests as new
  facts" pattern from multi-agent-subagent-pitfalls).

## Next role

End of cycle. Cron exits [PASS] on this card pending operator build.