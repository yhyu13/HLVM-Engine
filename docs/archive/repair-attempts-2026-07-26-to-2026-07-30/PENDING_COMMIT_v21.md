# Pending Commit v21

- plan: docs/PENDING_PLAN_v21.md
- files: (no source-code files; v21 is a plan-only cycle)
- source: no bundle — direct write of v21 plan + plan-review markers
- target: working tree (no commit/push)
- task: stage v21 as the decision-matrix outcome from v20 evidence. v21a (the highest-priority sub-plan: FGIPass binding-layout split for nvrhi-deferred-barrier-ordering) is documented but NOT applied until parent confirms v20 evidence shape matches hypothesis #1.
- verify: parent-driven — the v21 plan correctness is verified by:
  1. Reading PENDING_PLAN_v21.md and confirming the v21a fix is well-scoped, gated on v20 evidence, and matches the gpu-rendering-bisect-debug/nvrhi-deferred-barrier-ordering.md reference.
  2. Cross-checking the FGIPass.cpp:260-296 binding layout against the plan's "binding layout evidence" — the SRV + UAV mixing in one set should be visible in the actual source.
  3. Cross-checking the v22/v23 heartbeats' hypothesis #1 against the plan's "Why this cycle is correct" section — they should align.
- skip_impl_review: no
- produces_test_files: no
- notes:
  - v21 is a planning cycle only. No source code touched. No new files created (the plan and plan-review markers ARE the deliverables).
  - v21a code change is staged in PENDING_PLAN_v21.md but NOT applied. Application requires parent to run v20's diagnostic and paste back `rgi_evidence.txt`.
  - The 9-branch decision matrix from PENDING_PICK.md lines 141-150 maps to v21a..v21i sub-plans. v21a is the highest-priority (hypothesis #1 from v22/v23 heartbeats). v21b..v21i will be staged as v22-v29 in subsequent cycles if v20 evidence points elsewhere.
  - The v21 plan acknowledges the single-head freshness caveat (per six-role-pipeline anti-pattern #7).

## Plan Deviations (impler fills this in if it deviated from the plan)

None. The v21 cycle produced exactly the markers specified:
- `docs/PENDING_PLAN_v21.md` (the plan, 12214 bytes)
- `docs/PENDING_PLAN_REVIEW_v21.md` (the plan-review, 4800 bytes)

No source-code files modified. No new files outside the marker pair. The plan-only nature of v21 is documented in the plan's "Notes for impl-reviewer" section.

## File-level changes

```
+ docs/PENDING_PLAN_v21.md (new file, 12214 bytes)
+ docs/PENDING_PLAN_REVIEW_v21.md (new file, 4800 bytes)
```

No other files touched. The v21 cycle is fully reversible: deleting the two new marker files restores pre-v21 state.

## What's next

The v21 cycle pauses here. The state machine routes to the tester (role #5) per Rule 7 in the six-role-pipeline SKILL.md, since the impl-review marker is NOT needed for a plan-only cycle (no commit yet, no source change to review).

The tester will write `docs/PENDING_TESTS_v21.md` documenting the parent-driven verification protocol for v21a's binding-layout-split fix (which will only be exercised if v20 evidence confirms hypothesis #1). Then the testing-verifier will audit, then the v21 cycle closes at SOME_RELAX (parent-driven) until v20 evidence arrives.

If v20 evidence arrives and points to v21a, the cron will:
1. Apply the v21a code change (FGIPass.cpp:260-296 split + FRayTracingPipeline 2-binding-set overload)
2. Mark this PENDING_COMMIT_v21.md with the actual file diffs
3. Run the v21 impl-reviewer
4. Update PENDING_TESTS_v21.md to reference the actual rebuild
5. The parent rebuilds, runs the diagnostic, and reports the evidence

If v20 evidence points elsewhere (e.g., v21b AmbientColor uniform fix), v21a is skipped and v21b becomes v22.
