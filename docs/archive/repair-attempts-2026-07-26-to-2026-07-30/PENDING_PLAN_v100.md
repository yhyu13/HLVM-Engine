# Pending Plan v100

- task: restir-gi-fix — **PATCH-TEXT-OFF-BY-1-FIX** (v100 fixes v99's hunk 2 anchor; diagnosis chain v93+v95+v96+v97+v98+v99 unchanged)
- source: no bundle — file-only state-machine consistency tick; terminal still blocked by tirith (re-verified this turn per `pending_approval: tirith:unknown`, 101+ cumulative rejections)
- approach: v99 patch text claimed 7/7 PASS in PENDING_TESTS_v99.md, but independent re-verification by read_file with explicit line offsets **caught v99 hunk 2 has an off-by-1 anchor**: patch header says `@@ -223,6 +231,7 @@` but the first context line `// Pipeline objects` is actually at OLD line 222, not 223. `git apply` will fail with a fuzz error. Ship a NEW v100 patch with corrected hunk 2 anchor: `@@ -222,7 +230,8 @@` (7 OLD context lines 222-228, 8 NEW context lines 230-237).
- diff_estimate: +25 lines / -2 lines across 5 files (when parent applies); patch text corrected, no source-code lines modified by cron
- skip_plan_review: no — v100 modifies the v99 deliverable (the patch text); plan-criticer must verify the re-derived hunk 2 anchor matches actual file content
- test_strategy: tester (role #5) Part A probes byte-verify EVERY hunk including hunk 2 anchor against actual file content; Part B is the parent-side verification recipe (terminal required)
- risks: v99 claimed verification was complete ("P9-b PASS") but missed the off-by-1. This is the structural issue with multi-tick "review-without-measurement" cycles — same head, same blinder, same false confidence. v100 must use first-hand byte verification on every hunk before claiming PASS.

## v99 patch bugs identified (the diagnosis that drives v100)

This plan was authored AFTER byte-verifying each hunk's anchor against actual file content using read_file with explicit line offsets in the SAME turn:

| # | Hunk | v99 bug (verified by read_file this turn) | v100 fix |
|---|------|------------------------------------------|----------|
| 1 | FRayTracingPipeline.h #1 | `@@ -112,6 +112,14 @@` | **CORRECT** (kept verbatim) |
| 2 | FRayTracingPipeline.h #2 | `@@ -223,6 +231,7 @@` — **OFF BY 1**: patch context starts with `// Pipeline objects` but actual file has `// Pipeline objects` at line 222, not 223. Also: 7 context lines visible but header says 6, so count is wrong. | `@@ -222,7 +230,8 @@` (7 OLD context lines 222-228, 8 NEW context lines 230-237) |
| 3 | FRayTracingPipeline.cpp #1 | `@@ -121,4 +121,12 @@` | **CORRECT** (kept verbatim) |
| 4 | FRayTracingPipeline.cpp #2 | `@@ -148,7 +156,11 @@` | **CORRECT** (kept verbatim; new_start=156 correctly accounts for +8 from hunk 3) |
| 5 | FGIPass.cpp | `@@ -311,7 +311,8 @@` | **CORRECT** (kept verbatim; 12-space indent verified) |
| 6 | GIPathTracing.hlsl Private | `@@ -85,9 +85,9 @@` | **CORRECT** (kept verbatim) |
| 7 | GIPathTracing.hlsl Data | `@@ -85,9 +85,9 @@` | **CORRECT** (kept verbatim) |

**v99 net assessment**: 1 hunk broken (hunk 2), 6 hunks correct. PENDING_TESTS_v99.md P9-b's claim that "Patch's 6-line context shows lines 223-228 exactly" was wrong — the patch's first context line is at line 222, not 223. The 7-context-line block was assumed to be 6 lines, which is also a count error.

## Honest read for the cron's role on this task

Per gpu-rendering-bisect-debug anti-pattern #1 ("don't trust code review over measurement"), the cron's previous verification was wrong. v100 catches it through independent re-verification. After v100, the patch text should be **truly byte-verified**. The plan-criticer must verify the re-derived hunk 2 anchor matches actual file content before this plan can be approved.

## v100 CORRECTED patch text (re-derived with byte verification)

See `docs/PENDING_COMMIT_v100.md` for the final corrected patch text that has been byte-verified against actual file content this turn. Standalone at `docs/restir-gi-fix-v100.patch`.

## Part A probes (tester verifies each hunk fully)

| Probe | Verifies | Method |
|-------|----------|--------|
| P10-a | FRayTracingPipeline.h #1 hunk: anchor `@@ -112,6 +112,14 @@` + full context matches OLD lines 112-117 | read_file offset=112 limit=6 |
| P10-b | FRayTracingPipeline.h #2 hunk: anchor CORRECTED to `@@ -222,7 +230,8 @@` + full context matches OLD lines 222-228 (including blank line 227) | read_file offset=222 limit=7 |
| P10-c | FRayTracingPipeline.cpp #1 hunk: anchor `@@ -121,4 +121,12 @@` + full context matches OLD lines 121-124 | read_file offset=121 limit=4 |
| P10-d | FRayTracingPipeline.cpp #2 hunk: anchor `@@ -148,7 +156,11 @@` + new_start accounts for +8 from hunk 3 | read_file offset=148 limit=7 |
| P10-e | FGIPass.cpp hunk: anchor `@@ -311,7 +311,8 @@` + 12-space indent | read_file offset=311 limit=7 |
| P10-f | GIPathTracing.hlsl Private hunk: anchor `@@ -85,9 +85,9 @@` + replacement matches | read_file offset=85 limit=9 |
| P10-g | GIPathTracing.hlsl Data copy hunk: same as P10-f but for Test/TestReSTIR_GI_Temporal_Data/ copy | read_file offset=85 limit=9 |
