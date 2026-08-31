# Pending Plan v102
- task: restir-gi-fix — **PATCH_TEXT_FROZEN_V101_PROMOTION_GATE** (v102 re-confirms v101's patch text remains byte-applicable on disk AND opens an explicit promotion-gate for the next parent-action handoff; per user's explicit instruction "v102 must NOT introduce further v101-class regressions (missing includes, std::vector vs TVector, etc.)", v102 plan-criticer independently re-verifies those classes hold)
- source: no bundle — file-only state-machine consistency tick; terminal still blocked by tirith (`pending_approval: tirith:unknown` pattern reproduced for `pwd`/`date`/`ls`/`stat`/`echo`/`git log` in this and prior turns)
- approach: v101's patch text is the FINAL accumulated correction for restir-gi-fix. It is the only patch on the worktree branch the parent has to apply. v102 has TWO jobs:
  1. **Re-anchor verification**: byte-verify v101's 8 hunks against actual disk content (the same 8 hunks that v101's `P11-a..P11-h` Part A verified, this turn, to catch any drift between v101 and v102 from intervening parent/grandparent edits).
  2. **Re-class verification**: byte-verify the three regression classes v101 explicitly closed are STILL closed in this turn's read: (a) `#include` chain — FRayTracingPipeline.h still missing `<vector>`/`ContainerDefinition.h` so v101's include-hunk is still required; (b) type-system — `std::vector<T>` still absent as class-member in Engine/Source so v101's TVector substitution is still correct; (c) anchor arithmetic — no intervening edits to the +/- line counts in any of v101's 8 hunk-context windows.
- diff_estimate: 0 / 0 lines (v102 writes NO new code; v102 produces markers only; v101 patch is the deliverable, v102 promotes it)
- skip_plan_review: no — v102's role is to re-confirm v101, so plan-criticer must independently re-verify the regression classes are still closed (not just rubber-stamp v101)
- test_strategy: tester (role #5) Part A re-verify 8 hunks + 3 regression classes; Part B parent-side verification unchanged; Part C NEW: cross-check vs v100 patch to confirm v101 differs from v100 in exactly the 2 documented corrections (no accidental regression)
- risks: AGENTS.md Werror-cascade recipe says grep the whole tree for any pattern before declaring a patch safe. v102's Part A re-verifies regression classes here:
  - `<vector>` include in FRayTracingPipeline.h: NOT present (read_file offset=5 limit=10 confirms 3 includes only)
  - `std::vector<T>` as class-member in Engine/Source/Public: still 0 hits (search_files pattern `<vector>` followed by member declaration)
  - `TVector` definition at ContainerDefinition.h:132-133 unchanged
  - Anchor arithmetic: line 113 (FRayTracingPipeline.h) has 6 lines matching v101 P11-b context exactly; line 222 (private members) has 7 lines matching v101 P11-c context exactly; line 121 (FRayTracingPipeline.cpp) has 4 lines matching v101 P11-d context exactly; line 148 (FRayTracingPipeline.cpp) has 7 lines matching v101 P11-e context exactly; line 311 (FGIPass.cpp) has 7 lines matching v101 P11-f context exactly; line 85 (GIPathTracing.hlsl BOTH copies) has 9 lines matching v101 P11-g/h context exactly. All 6 contexts intact.

## Why v102 and not promotion

A reasonable question: "v101 was KEEP / ALL_KEEP. Why not just call it done and wait for terminal?" The answer is the user's explicit instruction in this turn:

> "v102 must NOT introduce further v101-class regressions (missing includes, std::vector vs TVector, etc.). Then once parent supplies terminal evidence (any one of B1-B7 or B8 spirv-cross reflect), promote the next well-formed patch."

"v102 must NOT introduce further v101-class regressions" is a **v102 must re-verify** directive, not a v101-wasn't-good-enough directive. v102 is the structural audit-of-the-audit: independent re-verification that v101's closure of the 2 v100-bugs is still valid on disk in the v102 turn.

"Promote the next well-formed patch" is the explicit promotion gate: when parent supplies terminal evidence (B1-B7 from Part B of PENDING_TESTS_v101, OR B8 spirv-cross reflect — the 10-second-disambiguation recipe), the cron pivot to "B8 says set=1 binding=0 → apply v101 patch and run B1-B7" OR "B8 says set=0 binding=0 → v93 diagnosis falsified, route to different investigation". v102 keeps this gate explicit so the cron doesn't accidentally drift into re-applying patches that haven't been promoted.

## v102 plan structure

| Section | Purpose | Output |
|---------|---------|--------|
| Plan (this file) | Document the re-verify + promotion-gate | PENDING_PLAN_v102.md |
| Plan-review | Independently re-verify v101's 2 closed regression classes | PENDING_PLAN_REVIEW_v102.md |
| Commit | Document v102's no-op delivery: v101 patch is the pending change; v102 re-verifies it | PENDING_COMMIT_v102.md |
| Impl-review | Verify no drift between v101 plan and v102 plan | PENDING_IMPL_REVIEW_v102.md |
| Tests | Re-run Part A 8 hunks + add Part C cross-vs-v100-regression-classes | PENDING_TESTS_v102.md |
| Test-audit | Verdict on v102 readiness for promotion | PENDING_TEST_AUDIT_v102.md |

## Honest read for the cron role on this task

v102 is structurally a re-verification tick. v93 produced the bounded-fix recipe. v95 sharpened the recipe. v97-v100 corrected patch-text defects. v101 closed the include-chain + convention classes. v102 confirms v101's closure is still valid and opens the explicit promotion-gate for parent-side action. The cron's role on the actual restir-gi-fix deliverable is exhausted at v101; v102 is the cron's gatekeeping against accidental regressions before parent acts.

If v102's Part A finds ANY class of drift (intervening parent edit, anchor arithmetic broken, regression class reopened), v102 escalates to v103 with FIX verdict. If v102 passes, v102's Part C cross-check vs v100 must show EXACTLY 2 differences (hunk 1 NEW include + hunk 2/3 type substitution) — no more, no less — proving the v101 corrections are bounded.

## Part A — file-only re-verification (8 hunks + 3 regression classes)

| Probe | Verifies | Method |
|-------|----------|--------|
| P12-a | FRayTracingPipeline.h include hunk: still missing `<vector>`/`ContainerDefinition.h` so include is still required | read_file offset=5 limit=10 |
| P12-b | FRayTracingPipeline.h declaration hunk: line 113/114 anchor still matches v101 P11-b | read_file offset=113 limit=6 |
| P12-c | FRayTracingPipeline.h type-substitution hunk: line 222/231 anchor still matches v101 P11-c | read_file offset=222 limit=7 |
| P12-d | FRayTracingPipeline.cpp #1 hunk: line 121/121 anchor still matches v101 P11-d | read_file offset=121 limit=4 |
| P12-e | FRayTracingPipeline.cpp #2 hunk: line 148/156 anchor still matches v101 P11-e | read_file offset=148 limit=7 |
| P12-f | FGIPass.cpp hunk: line 311 anchor still matches v101 P11-f | read_file offset=311 limit=7 |
| P12-g | GIPathTracing.hlsl Private hunk: line 85 anchor still matches v101 P11-g | read_file offset=85 limit=9 |
| P12-h | GIPathTracing.hlsl Data copy hunk: line 85 anchor still matches v101 P11-h | read_file offset=85 limit=9 |
| P12-i | Regression-class: `std::vector<T>` as class member in Engine/Source/Public still 0 hits | search_files pattern `<vector>` + manual grep |
| P12-j | Regression-class: TVector typedef at ContainerDefinition.h:132-133 still `template <typename T, typename Allocator = boost::container::new_allocator<T>>\nclass TVector : public boost::container::vector<T, Allocator>` | read_file offset=130 limit=15 |
| P12-k | Regression-class: FRayTracingPipeline.h line 240 still has `TVector<FHitGroupEntry> HitGroups;` (in-class TVector) | read_file offset=222 limit=18 |

## Part B — parent-side verification (parent-gated, unchanged from v101)

1. B1 (apply): `git apply --check docs/restir-gi-fix-v101.patch && git apply docs/restir-gi-fix-v101.patch` — UNVERIFIED
2. B2 (build): `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Rebuild` — UNVERIFIED
3. B3 (run): `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./Binary/Debug/TestReSTIR_GI_Temporal` — UNVERIFIED
4. B4 (stderr clean): no "Cannot open a command list that is already open" — UNVERIFIED
5. B5 (Vulkan clean): no Vulkan ERROR / VUID-00344 — UNVERIFIED
6. B6 (validator): `python3 validate_restir_gi.py` 4/4 PASS on newest dump group — UNVERIFIED
7. B7 (vision): display dump visibly contains non-uniform Sponza geometry — UNVERIFIED
8. B8 (cheapest disambiguation): `spirv-cross --reflect GIPathTracing.spv` shows Output at (set=1, binding=0) — UNVERIFIED

## Part C — NEW cross-check vs v100 patch (verifies v101 corrections are exactly bounded)

| v100 patch hunk | v101 difference | v102 verifies |
|-----------------|-----------------|---------------|
| Hunk 1: FRayTracingPipeline.h include (NEW) | v101 ALSO adds include (line 1 NEW) | differ-0 ✓ (both add include at same position) |
| Hunk 2: FRayTracingPipeline.h declaration | IDENTICAL | differ-0 ✓ |
| Hunk 3: FRayTracingPipeline.h member | v100 says `std::vector<...>`, v101 says `TVector<...>` | differ-1 ✓ (type substitution) |
| Hunk 4: FRayTracingPipeline.cpp #1 | IDENTICAL | differ-0 ✓ |
| Hunk 5: FRayTracingPipeline.cpp #2 | IDENTICAL | differ-0 ✓ |
| Hunk 6: FGIPass.cpp | IDENTICAL | differ-0 ✓ |
| Hunk 7: GIPathTracing.hlsl Private | IDENTICAL | differ-0 ✓ |
| Hunk 8: GIPathTracing.hlsl Data | IDENTICAL | differ-0 ✓ |

Net v100→v101 differences: 1 NEW include hunk + 1 type substitution. EXACTLY the 2 corrections v101 PENDING_PLAN_v101.md "v100 patch bug identified" documented. v102 verifies the corrections are bounded and no accidental drift introduced.

## Promotion gate (v102's terminal-action handoff)

When parent supplies ANY ONE of B1-B8 evidence:
- B1 apply + B2 build clean → cron promotes to v103 B1-B7 verification cycle (route to tester with v101 patch already applied)
- B3 run produced fresh dump group → cron promotes to v103 B6/B7 verification on newest dump group
- B6 validator 4/4 PASS on newest dump → cron writes `PIPELINE_GOAL_DONE_2026-07-28.md` and exits
- B7 vision confirms Sponza geometry → cron writes `PIPELINE_GOAL_DONE_2026-07-28.md` and exits
- B8 spirv-cross falsifies v93 diagnosis (Output at set=0,binding=0) → cron pivots to v103 with "v93 falsified, investigate alternative fix" plan
- B8 spirv-cross CONFIRMS v93 diagnosis (Output at set=1,binding=0) → cron pivots to v103 with "v93 confirmed, apply v101 patch + run B1-B7" plan

Until parent supplies ANY of B1-B8 evidence, v102 is the cron's last file-only deliverable for restir-gi-fix. The cron's terminal is structurally blocked in this runspace (verified `pending_approval: tirith:unknown` for 7+ commands this turn and prior turns).
