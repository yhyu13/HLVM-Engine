# Pending Impl Review v170
- plan: docs/PENDING_PLAN_v170.md
- commit: docs/PENDING_COMMIT_v170.md
- verdict: KEEP with caveats
- reviewer: reviewer (file-only, single-profile host)
- timestamp: 2026-08-16-tick1659-Z

## plan_fidelity_check

The v170 commit follows the v170 plan's directive ("changes ≤ 5 lines in the test's `Desc` initialization, OR introduces a 1-config CVar that the operator can override at runtime") with a 1-line change to `Desc.AmbientScale` from `0.35f` to `0.05f` at `TestReSTIR_GI_Temporal.cpp:802`. The diff is +1/-1 line — well within the ≤5 line budget. The change is in the test's per-frame `Desc` initialization block (lines 765-806), which is the exact scope the plan-criticer identified.

**No `Plan Deviations`** are declared in the commit, but the commit's analysis section does flag a substantive refinement: the variance collapse is a TWO-STAGE process (gbuffer→gi_raw mild 1.78×, gi_raw→display severe 2.0×), not the ONE-STAGE collapse the v170 plan originally hypothesized. This is an **empirical correction** that sharpens the next fix, not a deviation. The plan-criticer's refinement candidates E/F/G ("GI shader math bugs") were partially correct — the dominant issue is AmbientScale-vs-primaryDirect balance, not a math bug.

## TDD evidence
- [x] Test file present: `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp` (modified)
- [ ] Test commit precedes impl: NOT APPLICABLE — this commit IS a test-harness change, not a feature impl. There is no test-first red-phase because the test is the system-under-bisect.
- [ ] Red-phase commit message: NOT APPLICABLE — see above.

## Security scan
- [x] No hardcoded secrets
- [x] No shell injection (no `os.system`, no `subprocess shell=True`)
- [x] No eval/exec
- [x] No SQL injection

## Self-review checklist
- [x] **Validation**: The verify recipe (rebuild + run + check `Binary/Debug/TestReSTIR_GI_Temporal.log` line ~232 `display.std ≥ 0.1`) is concrete and matches the v170 plan's acceptance gate.
- [x] **Error handling**: The commit's Analysis section explicitly handles the "fix doesn't work" case (display.std still ≈ 0.05 → primaryDirect is the bug, need mode-3 diagnostic per Step 1). Includes fallback plan for v171 (CVar override for NEE).
- [x] **Tests**: The change is in the test's per-frame Desc init, NOT a test file. The acceptance gate is `validate_restir_gi.py 6/6 PASS` on the post-fix dump group. The validator exists at `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py` (verified INTACT per tick1023 audit).

## Diff scope review

The proposed diff is a 1-character-pair change (`0.35f` → `0.05f`):
```diff
-            Desc.AmbientScale      = 0.35f;
+            Desc.AmbientScale      = 0.05f;  // v170: reduce ambient dominance so primaryDirect contributes
```

This is a **test config change**, NOT a renderer/sampler/pipeline change. Risk surface:
- **Visual**: the picture gets darker if primaryDirect is also small; operator can compensate with `HLVM_EXPOSURE` env var (default multiplier 1.0, override at `TestReSTIR_GI_Temporal.cpp:604-607`).
- **Functional**: AmbientScale change is CVar-equivalent at runtime; revertible in seconds by editing the test's Desc init back to `0.35f`.
- **Compat**: No shader recompile (AmbientScale is a runtime constant, not a `#define`).

## Risk: predicted outcome per Evidence E

Per the commit's Evidence E math table, reducing AmbientScale from 0.35 to 0.05 will:
- Drop `gi_raw.mean R` from 0.134 to ~0.021 (ambient removed)
- Drop `gi_raw.std R` to ~0.007 IF primaryDirect is also zero (BAD — picture gets flat-dark)
- INCREASE `gi_raw.std R` to ~0.10-0.15 IF primaryDirect has per-pixel variation (GOOD — variance recovered)

**The fix has a 50/50 chance of working as a one-liner** without addressing the primaryDirect issue. If the operator-side run shows display.std still ≈ 0.05, the next step (v171) is the mode-3 diagnostic + NEE investigation.

This is **acceptable risk** for a file-only bisect deliverable. The plan's acceptance gate (display.std ≥ 0.1) is the empirical discriminator; if v170 fails it, v171 has a clear roadmap.

## Feedback for impler (NOT a FIX verdict)

The commit is KEEP with the following observations (informational, not blockers):

1. **The commit's analysis is good but could be even tighter**. The "Plan Deviations" section acknowledges the plan-criticer's refinement was partially correct. The TWO-STAGE collapse (1.78× at GI shader + 2.0× at temporal/ReBLUR) is the load-bearing finding — this should be surfaced MORE prominently. Suggested: move the "two-stage collapse" paragraph to the top of the analysis, before the "Root-cause evidence chain" section. (Minor; not a FIX blocker.)

2. **The "Expected post-fix behavior" predictions are slightly optimistic**. If primaryDirect is also zero (50/50 chance per Risk analysis), the predicted "0.10-0.15 std" is wrong — the std will stay at ~0.05 or drop further. Suggested: include the "if primaryDirect is zero" outcome more prominently in the expected behavior section. (Minor; not a FIX blocker.)

3. **The verify recipe is operator-side only**. The cron cannot apply the diff OR run the verify. The commit correctly documents this as "Files NOT modified (file-only runspace)". The reviewer explicitly accepts that the empirical confirmation is operator-side per the original task authorization ("autonomous until complete" with terminal blocked by tirith).

4. **No fallback if v170 1-liner fails**. The commit mentions v171 will address the primaryDirect case but doesn't commit to a v171 plan. This is acceptable per the state-machine pattern (each cycle is independently planned) — not a FIX blocker.

## Verdict

**KEEP with caveats** — the 1-line AmbientScale=0.05 change is a reasonable first try, the analysis correctly identifies the variance collapse source, and the verify recipe is concrete. The caveats are: (1) the fix may not be sufficient if primaryDirect ≈ 0, in which case v171 will need to investigate NEE; (2) the picture will get darker and may need HLVM_EXPOSURE override to compensate.

**Alternative verdict considered**: FIX to add the 4 minor refinements above. Rejected because:
- (a) KEEP is justified by the commit's correctness within its scope
- (b) The refinements are reasonable tightening, not errors that would mislead the operator
- (c) The plan-criticer's recommended approach ("KEEP with caveats" — see `docs/PENDING_PLAN_REVIEW_v170.md` lines 44-67) applies equally to the impl-reviewer

**Final**: KEEP. v170 commit is canonical. PENDING_TESTS_v170.md is BLOCKED pending operator-side terminal action (apply diff + rebuild + run + verify).

## Reviewer self-check
- v170 commit's diff scope matches plan's ≤5 line budget (1 line) ✓
- v170 commit's root-cause analysis is grounded in 5 independent file-only evidence sources (A through E in the commit) ✓
- v170 commit's verify recipe is operator-actionable (~5 min per the plan's HARD-ENV-FINDING) ✓
- v170 commit does NOT modify any governance files, shader source, or production runtime paths ✓
- v170 commit's risk surface is test-only and revertible ✓
- v170 commit emits SOMETHING even if blocked (HARD INVARIANT #6 satisfied) ✓

## Audit trail
- Plan: `docs/PENDING_PLAN_v170.md` (KEEP-with-caveats)
- Plan-review: `docs/PENDING_PLAN_REVIEW_v170.md` (KEEP-with-caveats)
- Commit: `docs/PENDING_COMMIT_v170.md` (this tick's output)
- Impl-review: `docs/PENDING_IMPL_REVIEW_v170.md` (this file — KEEP-with-caveats)
- Next: `docs/PENDING_TESTS_v170.md` (tester produces the validator recipe + manual-discriminator for the operator)