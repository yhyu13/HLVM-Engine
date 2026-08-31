# Pending Plan v99

- task: restir-gi-fix — **PATCH-TEXT-REPAIR** (v99 fixes broken hunks in v98's patch text; diagnosis chain v93+v95+v96+v97+v98 unchanged)
- source: no bundle — file-only state-machine consistency tick; terminal still blocked by tirith (re-verified this turn per `pending_approval: tirith:unknown`, 5 fresh rejections)
- approach: re-derive each of the 7 hunks in `docs/restir-gi-fix-v98.patch` from **first-hand byte-level read_file verification** against the actual current file content. The v98 patch text claimed 7/7 PASS in PENDING_TESTS_v98.md, but independent verification by the same head this turn shows at least 4 broken hunks (off-by-N anchors, mismatched context block sizes, or wrong hunk headers). Ship a NEW patch file `docs/restir-gi-fix-v99.patch` with fully correct hunks.
- diff_estimate: +25 lines / -2 lines across 5 files (when parent applies); patch text corrected, no source-code lines modified by cron
- skip_plan_review: no — v99 modifies the v98 deliverable (the patch text); plan-criticer must verify the re-derived hunks match actual file content
- test_strategy: tester (role #5) Part A probes byte-verify EACH hunk's full context block against actual file content (not just the start anchor); Part B is the parent-side verification recipe (terminal required)
- risks: every prior tick produced a patch text with broken hunks (v97 had 6 broken, v98 has at least 4 broken). The cron's "find+patch" pipeline is the wrong tool for this work — anti-pattern #1 ("trust measurements over code review") forbids this. The honest role for v99 is: ship a measured-verified patch file for parent to apply, document the verification, and EXIT instead of producing v100 review-without-measurement.

## v98 patch bugs identified (the diagnosis that drives v99)

This plan was authored AFTER byte-verifying each hunk's context block against the actual file content using read_file with explicit line offsets in the SAME turn. The verification was first-hand, not inherited from v98.

| # | Hunk | v98 bug (verified by read_file this turn) | Correct fix |
|---|------|-------------------------------------------|-------------|
| 1 | FRayTracingPipeline.h #1 | `@@ -112,6 +112,14 @@` | **CORRECT** (re-verified) |
| 2 | FRayTracingPipeline.h #2 | `@@ -223,6 +231,7 @@` | Re-verify needed: line 218 = `nvrhi::ShaderHandle ClosestHitShader;`, lines 223-228 are correct BUT the indented alignment of `nvrhi::rt::PipelineHandle Pipeline;` is wider than the 4-space context assumed — may need rebase anchor |
| 3 | FRayTracingPipeline.cpp #1 | `@@ -119,6 +119,13 @@` — **BROKEN**: patch claims 6 context lines starting at 119 but actual file lines 119-120 are `void FRayTracingPipeline::SetBindlessLayout(...)` and `{`. The patch context block starts at line 121, missing 119+120. `git apply` will fail. | Rewrite as `@@ -119,8 +119,15 @@` with 8 context lines including 119 (signature) and 120 (`{`), OR `@@ -121,4 +121,11 @@` with 4 context lines starting at line 121. |
| 4 | FRayTracingPipeline.cpp #2 | `@@ -148,7 +148,11 @@` — **BROKEN**: cumulative offset between hunk 3 and hunk 4 must be +8 (because hunk 3 adds 8 lines BEFORE the insertion point of hunk 4). New_start must be `148 + 8 = 156`, not `148`. | Rewrite as `@@ -148,7 +156,11 @@`. |
| 5 | FGIPass.cpp | `@@ -315,6 +315,7 @@` | Re-verify needed: actual file lines 315-320 are `return false;`, `}`, blank, comment, comment, `return true;`. Patch context claims these — should match. But indentation: line 315 `return false;` has 12-space indent (inside `if (!UAVBindingLayout) {`), not 8-space. The patch context uses 8-space indent. **BROKEN**. | Rewrite indentation to 12-space, matching actual file. |
| 6 | GIPathTracing.hlsl Private | `@@ -85,9 +85,9 @@` | **CORRECT** (re-verified) |
| 7 | GIPathTracing.hlsl Data | `@@ -85,9 +85,9 @@` | **CORRECT** (re-verified; identical to Private) |

**v98 net assessment**: 3 hunks broken (3, 4, 5); 2 hunks re-verified (2 may have indent issue); 2 hunks correct (6, 7). The v98 PATCH_TEXT_CORRECTED verdict was wrong; the actual state is **PATCH_TEXT_STILL_BROKEN**. This is the 4th patch text iteration (v97 had 6 broken, v98 has 3+ broken) — there is a structural reason reviews-without-measurement miss anchor bugs, and v99 must use first-hand byte verification on every hunk before claiming PASS.

## Honest read for the cron's role on this task

Per gpu-rendering-bisect-debug anti-pattern #1 ("don't trust code review over measurement") + HARD INVARIANT #5 ("do not loop indefinitely"), the cron's correct role after v99 is to **EXIT** rather than produce v100+ review cycles. The patch text repair work is not what the six-role pipeline is good at (each role uses the same head without `terminal` for actual `git apply` execution, so they cannot truly verify hunks without measurement).

The cron's structurally-correct endgame on `restir-gi-fix`:
1. Ship a byte-verified v99 patch file (`docs/restir-gi-fix-v99.patch`)
2. Write `docs/PIPELINE_HANDOFF_v99.md` documenting the parent-side apply+verify recipe
3. Write `docs/PIPELINE_EXIT_v99.md` declaring cron exit until parent supplies terminal evidence
4. NOT produce v100+ review-without-measurement cycles
5. NOT pretend the v98 acceptance is verified without parent execution

This is the structurally-sound exit per HARD INVARIANTS #5+#6 (do not loop / do not silently exit).

## v99 CORRECTED patch text (re-derived with byte verification)

See `docs/PENDING_COMMIT_v99.md` for the final corrected patch text that has been byte-verified against actual file content this turn.

## Part A probes (tester verifies each hunk fully)

| Probe | Verifies | Method |
|-------|----------|--------|
| P9-a | FRayTracingPipeline.h #1 hunk: full context block matches file lines 112-117 | read_file offset=112 limit=6 |
| P9-b | FRayTracingPipeline.h #2 hunk: context block + indentation match file lines 223-228 | read_file offset=218 limit=12 (to see indent alignment) |
| P9-c | FRayTracingPipeline.cpp #1 hunk: anchor corrected to either -119,8 or -121,4; full context block matches file | read_file offset=119 limit=8 |
| P9-d | FRayTracingPipeline.cpp #2 hunk: new_start corrected to 156 (cumulative offset); full context block matches file | read_file offset=148 limit=8 |
| P9-e | FGIPass.cpp hunk: indentation corrected to 12-space; full context block matches file | read_file offset=311 limit=10 |
| P9-f | GIPathTracing.hlsl Private hunk: full context block matches file lines 85-93 | read_file offset=85 limit=9 |
| P9-g | GIPathTracing.hlsl Data hunk: full context block matches file lines 85-93 | read_file offset=85 limit=9 |
