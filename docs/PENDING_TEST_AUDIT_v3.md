# Pending Test Audit v3 — verify v2 + (contingent) verify v3 diagnostic

- tests: docs/PENDING_TESTS_v3.md
- commit: docs/PENDING_COMMIT_v3.md (staged, NOT applied)
- verdict: ALL_KEEP
- verifier: tester+testing-verifier (single-profile host; same head)
- timestamp: 2026-08-17 (estimated wall-clock; cron session)

## Broken-pattern audit

The 5 known broken-test patterns:

- [x] **No from-x-import-y patch propagation bugs.** The test plan
      doesn't modify any test source files; it only runs commands.

- [x] **No test-bug-in-itself.** The tests run the actual test
      executable and inspect its output. They don't re-assert on imagined
      fixtures.

- [x] **No source-incomplete-relative-to-test.** The contingent v3
      source change (handle-id extension + mode 23 sentinel-compare) is
      COMPLETE: C++ × 2 + HLSL × 2 edited, no orphan references.

- [x] **No missing test isolation fixture.** The test invocation uses
      `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8` which deterministically
      produces a single dump group. The validator's
      `select_newest_dump_group` correctly identifies it.

- [x] **No AsyncMock on sync function (or vice versa).** N/A — this is
      a C++/GPU test, not a Python async test.

## Per-test verdict

| Test | Verdict | Rationale |
|------|---------|-----------|
| Test 1a: Build | KEEP | Standard build invocation; exits 0 is success criterion. |
| Test 1b: SRV sentinel mode 20 | KEEP | Per-channel std > 5/255 directly tests the SRV-binding-read-zero bug. |
| Test 1c: 4-check validator | KEEP | Validator's checks are well-calibrated. Exit 0 = all 4 pass. |
| Test 1d: Vision review | KEEP | Required because scalar gates can pass garbage. |
| Test 2: Apply v3 changes | KEEP | Mechanical changes following PENDING_COMMIT_v3.md. |
| Test 3a: Handle-identity | KEEP | Per-frame handle log is the cheapest decisive experiment. |
| Test 3b: Sentinel-compare mode 23 | KEEP | Single-dump distinction between handle mismatch and descriptor mismatch. |

## Critical concern flagged for operator

**The cron cannot execute ANY of the test build steps.** All seven tests
above require running shell commands. Per the

> "Empirically verify what subagents can do (don't trust the skill's 'MAY'
> wording)" section in software-development-practices, terminal is
> blocked by tirith in this profile. The 2026-07-03 host finding (and
> confirmed again on 2026-08-17 in this session) shows:
> "subagent shell is blocked by tirith with the same 'User denied this
> command' error."

**The operator (parent session at the keyboard) MUST execute the test
build + run + dump inspection.** The cron has done all it can:
- Diagnosed the v2 fix (revert v22 split + HLSL `space1` removal).
- Reviewed v2 for plan fidelity + correctness (KEEP verdicts).
- Specified the verification recipe (4 sub-tests in Test 1 + 3
  sub-tests in Test 3 with exact commands and acceptance criteria).
- Staged the v3 contingent changes (handle-id extension + mode 23
  sentinel-compare) for if v2 didn't work.

Without the operator running Test 1, v2 is unverified. The operator
must:

```bash
cd /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine
rm -f Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/GIPathTracing.sblob
./Build.sh --Rebuild --Config=Debug --Target=TestReSTIR_GI_Temporal --Test
HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 HLVM_PT_DEBUG_MODE=20 HLVM_RGI_DIAG=1 \
    ./Binary/Debug/.../TestReSTIR_GI_Temporal
python3 -c "from PIL import Image; import numpy as np; \
    img = np.array(Image.open('<DUMP_DIR>/gi_raw_frame8.png')); \
    print('std:', img[:,:,0].std(), img[:,:,1].std(), img[:,:,2].std(), img[:,:,3].std())"
python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py --show
```

Then inspect /tmp/rgi_mode20.log and the dumps directory. If mode 20
dump is non-black AND validator passes AND vision review shows Sponza,
v2 worked. Otherwise, apply v3 changes and proceed to Test 3.

## Tick verdict

The verification recipe is well-designed (4 + 3 sub-tests with exact
acceptance criteria). The v3 contingent changes are minimal debug
instrumentation gated behind env vars. ALL_KEEP.

The cron tick is now END-OF-CYCLE for v3. The next tick depends on the
operator's Test 1 outcome:

- Test 1 PASS (v2 worked) → mark card done in PENDING_PICK.md, archive
  the cycle markers.
- Test 1 FAIL (v2 didn't work) → operator applies v3 changes, runs
  Test 3, reports back. Next cron tick routes based on Test 3 verdict.

## Next role

End of cycle for this card. Cron exits [PASS] on this card pending
operator build + verification.

## Single-profile deployment caveat (explicit)

Per the skill:

> "Single-profile deployment without explicit caveat ... the freshness
> guarantee of the planner/impler split and the plan-criticer/reviewer
> split collapses to 'same head with different prompt text.'"

This entire cycle (plan + plan-review + commit + impl-review + tests +
test-audit) was authored by the same model in the same session with no
terminal access. The verdicts are best-effort design reviews, NOT
independent verifications. The operator-side execution is the only
ground truth in this loop.
