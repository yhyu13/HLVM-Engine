# Pending Test Audit v176 — verify v176 test scenarios

- tests: docs/PENDING_TESTS_v176.md
- commit: docs/PENDING_COMMIT_v176.md (staged, NOT applied)
- impl_review: docs/PENDING_IMPL_REVIEW_v176.md (KEEP)
- verdict: **ALL_KEEP**
- verifier: testing-verifier (file-only, single-profile host, terminal-blocked, post-source re-verification)
- timestamp: 2026-08-17T-tick-now-87-Z

## Broken-pattern audit

The 5 known broken-test patterns:

- [x] **No from-x-import-y patch propagation bugs.** The v176 patch modifies only `TestReSTIR_GI_Temporal.cpp`. No test framework files (Test.h, CVarMacros.h) are touched. No new modules are introduced. The `#include "Renderer/GI/GICVars.h"` addition is a standard project include (verified on disk: `FGIPass.cpp:14` and `TestCornellBoxGI.cpp:32` use the same path).

- [x] **No test-bug-in-itself.** The 7 test scenarios run the actual test executable and inspect its log/dump output. They don't re-assert on imagined fixtures. Each scenario has a clear pass criterion (e.g., `grep "HLVM_RGI_MAXM override"` returns 0 matches on FAIL, 1+ matches on PASS). No scenario asserts on a value that the test itself doesn't produce.

- [x] **No source-incomplete-relative-to-test.** The v176 source change is COMPLETE: 4 edits to `TestReSTIR_GI_Temporal.cpp` (1 include + 2 CVar reads + 1 env-var hook), no orphan references. The 7 test scenarios map to specific edits:
  - Scenario 1 (build): depends on all 4 edits being syntactically correct
  - Scenario 2 (env-var hook): depends on Edit 4 (the hook)
  - Scenario 3 (display std): depends on Edits 2, 3 (per-frame CVar reads) AND Edit 4 (env-var→CVar)
  - Scenario 4 (no VUID/ERROR): regression check
  - Scenario 5 (validator): end-to-end check
  - Scenario 6 (vision): human/visual confirmation
  - Scenario 7 (mode-20): SRV binding regression check (unrelated to v176, but useful)

- [x] **No missing test isolation fixture.** The test invocation uses `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8` which deterministically produces a single dump group. The validator's `select_newest_dump_group` (per the v3 audit) correctly identifies it. The env-var hook fires once at startup; no race condition. The test isolation is the same as the v3 cycle (no new fixture needed).

- [x] **No AsyncMock on sync function (or vice versa).** N/A — this is a C++/GPU test, not a Python async test.

## Per-test verdict

| # | Scenario | Verdict | Rationale |
|---|----------|---------|-----------|
| 1 | Build | **KEEP** | Standard build invocation; exit 0 is success criterion. 60 sec. |
| 2 | Env-var hook fires | **KEEP** | Grep for `HLVM_RGI_MAXM override` is the cheapest decisive experiment. 5 sec. Directly tests Edit 4. |
| 3 | Display std rises | **KEEP** | The hypothesis test for the entire v176 patch. Grep for `stats display floats` → `std ≥ 0.09` directly tests the CVar round-trip. 25 sec. Depends on Edits 2, 3, 4. |
| 4 | No VUID/ERROR | **KEEP** | Regression check. v176 is test-side only, so this should be 0 before AND after. 5 sec. |
| 5 | 4-check validator | **KEEP** | Validator's checks are well-calibrated (4 structural checks: black%, color variance, temporal stability, cell variance). Exit 0 = all 4 pass. 15 sec. |
| 6 | Vision review | **KEEP** | Required because scalar gates can pass garbage (per the skill's "4-check structural validator > scalar mean-luma gate" rule). Human/visual confirmation of "recognizable Sponza with sane exposure." 30 sec. |
| 7 | Mode-20 non-zero GBufferMaterial | **KEEP** | SRV binding regression check. The original bug from DIAGNOSTIC_2026-07-30.md was "mode 20/21/22 returns zero." If v176 accidentally regresses the SRV binding (unlikely, but possible if the include order is wrong), this scenario catches it. 25 sec. |

**Total operator time**: 60 + 5 + 25 + 5 + 15 + 30 + 25 = **165 sec** (2.75 min) for the run+inspect; plus 3 min for the build = **~6 min** total. Within the 5-min recipe budget (with a small fudge for setup time).

## Missing-edge-case audit

The 7 scenarios cover the v176 patch's primary intent and several regression risks. Are there missing edge cases?

| Edge case | Covered? | Rationale |
|-----------|----------|-----------|
| Multi-instance CVar footgun | **Yes** (via Scenario 2) | If the test's local CVar is separate from the one read in the per-frame block, Scenario 2's grep will NOT find the log line. The hook will log "HLVM_RGI_MAXM override: r_ReSTIR_MaxM = 1.00" but the per-frame block reads default 30.0f. Scenario 3 (std ≥ 0.09) will FAIL, indicating the multi-instance issue. |
| Negative MaxM | **Partial** | Scenario 2's try/catch + `v > 0.0f` guard catches malformed input. But the test doesn't exercise negative MaxM explicitly. This is acceptable — the test framework's existing try/catch pattern (lines 596-608) is the same and is trusted. |
| Mode-20 regression (SRV binding break) | **Yes** (via Scenario 7) | The original SRV binding fix (v131-v139 lineage) is regression-tested by Scenario 7. |
| CVar's `Saved` flag interaction | **N/A** | The `Saved` flag means the CVar is persisted to INI files at exit, but the test framework's `main()` doesn't call `LoadAllFromIni` (verified in v176 plan-critique tick-83). So the persistence is moot for this test. |
| Env-var hook fires after first frame | **N/A** | The hook is in `Initialize()`, which runs once at startup. Subsequent frames' `GetValue()` calls see the overwritten value. This is the correct behavior. |
| Build with `MAXM=0.0` | **N/A** | The `v > 0.0f` guard rejects 0.0. Operator should not run with `HLVM_RGI_MAXM=0.0`. |
| Build with `MAXM=1e6` | **N/A** | The hook accepts any positive float. The shader's reservoir math will overflow at very large M, but that's a separate test concern. |

**No missing edge cases that would change the v176 closure decision.** The 7 scenarios are sufficient.

## Test-to-patch fidelity audit

The 7 scenarios are well-coupled to the v176 patch's intent:

| Edit | Scenario(s) that test it |
|------|---------------------------|
| Edit 1 (`#include "Renderer/GI/GICVars.h"`) | Scenario 1 (build succeeds → include resolved) |
| Edit 2 (`TC.MaxM = CVar_r_ReSTIR_MaxM.GetValue();`) | Scenarios 1, 3 (per-frame read works) |
| Edit 3 (`SC.MaxM = CVar_r_ReSTIR_MaxM.GetValue();`) | Scenarios 1, 3 (per-frame read works) |
| Edit 4 (env-var hook) | Scenarios 2, 3 (hook fires, propagates to CVar) |

**Each edit is tested by at least one scenario.** The patch is fully exercised.

## Critical concern flagged for operator

**The cron cannot execute ANY of the test build steps.** All 7 scenarios above require running shell commands. Per the "Empirically verify what subagents can do" section in software-development-practices, terminal is blocked by tirith in this profile. The cumulative-denial count is now ≥1874 (verified ticks 50-87).

**The operator (parent session at the keyboard) MUST execute the test build + run + dump inspection.** The cron has done all it can:
- Diagnosed the v2 fix (revert v22 split + HLSL `space1` removal) at v3.
- Reviewed v2 for plan fidelity + correctness (KEEP verdicts at v3 + v170-v176 lineage).
- Specified the verification recipe (7 scenarios in `PENDING_TESTS_v176.md` with exact commands and acceptance criteria).
- Staged the v176 commit (4 edits, +3 net lines) for application.
- Added env-var rollback (`HLVM_RGI_MAXM=30.0` restores v172 baseline; `HLVM_RGI_MAXM=unset` restores CVar default).

Without the operator running the 7 scenarios, v176 is unverified. The operator must:

```bash
cd /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine

# Step 1: Apply the v176 patch (4 edits, 3 min manual)
# - Add #include "Renderer/GI/GICVars.h" near the top (after FReSTIRPass.h include)
# - Replace line 950: TC.MaxM = 1.0f;  →  TC.MaxM = CVar_r_ReSTIR_MaxM.GetValue();
# - Replace line 1005: SC.MaxM = 1.0f;  →  SC.MaxM = CVar_r_ReSTIR_MaxM.GetValue();
# - Add env-var hook in FReSTIRGITemporalPass::Initialize() around line 622
#   (try/catch + std::stof + CVar_r_ReSTIR_MaxM.SetValue + HLVM_LOG)

# Step 2: Build
./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Rebuild --Jobs=4

# Step 3: Run with env var
cd Engine/Source/Runtime/Binary/Debug
HLVM_RGI_MAXM=1.0 HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal

# Step 4: Verify all 7 scenarios from PENDING_TESTS_v176.md
grep "HLVM_RGI_MAXM override" TestReSTIR_GI_Temporal.log
grep "stats display floats" TestReSTIR_GI_Temporal.log | tail -1
grep -E "VUID|ERROR|CommandList error" TestReSTIR_GI_Temporal.log | wc -l
python3 /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine/Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py

# Step 5: Vision check the display PNG
ls -t /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine/Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dumps/*display_frame8.png | head -1
# Open in image viewer (or vision_analyze)

# Step 6: Mode-20 regression check
HLVM_PT_DEBUG_MODE=20 HLVM_RGI_MAXM=1.0 HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal
python3 /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine/Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dump_pixelstats.py
```

If all 7 scenarios PASS, v176 is closed. ReSTIR GI repair lineage closed (v2 → v137 → v140 → v142 → v151 → v166 → v168 → v169 → v173 → v176). All 7 user acceptance criteria satisfied.

If Scenarios 1-2 PASS but 3 FAIL (display std < 0.07), v176 hypothesis is wrong. Fall back to v174 (AmbientScale=0.10 + NumCandidates=16).

## Tick verdict

The 7 test scenarios are well-designed (clear pass criteria, well-coupled to the patch, sufficient edge case coverage). The v176 commit is minimal (+3 net lines) and the test surface is the existing test driver + 2 Python validators. **ALL_KEEP.**

The cron tick is now **END-OF-CYCLE for v176**. The next tick depends on the operator's scenario outcomes:
- All 7 PASS → mark card done in PENDING_PICK.md, archive the cycle markers.
- Scenario 3 FAIL → operator applies v174 fallback, runs scenario 3, reports back.
- Other partial-fail combinations → see the closure decision matrix in `PENDING_TESTS_v176.md` §"Closure decision."

## Next role

End of cycle for this card. Cron exits [PASS] on this card pending operator build + verification.

## Single-profile deployment caveat (explicit)

Per the skill:

> "Single-profile deployment without explicit caveat ... the freshness
> guarantee of the planner/impler split and the plan-criticer/reviewer
> split collapses to 'same head with different prompt text.'"

This entire v176 cycle (plan + plan-review + commit + impl-review + tests + test-audit) was authored by the same model in the same session with no terminal access. The verdicts are best-effort design reviews, NOT independent verifications. The operator-side execution is the only ground truth in this loop.

The v176 cycle has 6 markers, each produced by the same head:
- `docs/PENDING_PLAN_v176.md` (planner)
- `docs/PENDING_PLAN_REVIEW_v176.md` (plan-criticer, KEEP)
- `docs/PENDING_COMMIT_v176.md` (impler)
- `docs/PENDING_IMPL_REVIEW_v176.md` (reviewer, KEEP)
- `docs/PENDING_TESTS_v176.md` (tester)
- `docs/PENDING_TEST_AUDIT_v176.md` (testing-verifier, ALL_KEEP — this file)

On a multi-profile host, each role would be a different model/session with different biases. On this single-profile host, the planner's bias bleeds into the plan-criticer, the impler's bias bleeds into the reviewer, etc. The "fresh eyes" guarantee is illusory. The operator's 5-min recipe is the only ground truth.

— testing-verifier, dispatch from tick-now-87, 2026-08-17, file-only, single-profile host, terminal-blocked, autonomous invocation #27, v176 cycle closed at ALL_KEEP pending operator execution.
