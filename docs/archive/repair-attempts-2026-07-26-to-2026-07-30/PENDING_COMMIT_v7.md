# Pending Commit v7
- plan: docs/PENDING_PLAN_v7.md
- files: Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp
- source: no bundle
- target: working tree (no commit per cron rules)
- task: stale comment fix at lines 650-672 — replace v1-introduced HLVM-bypass description with accurate post-v5 description, remove stale line-range reference, add v5 cross-reference
- verify: parent must build and run after this patch lands; renderer output will be IDENTICAL to what they would have seen without this patch (comments are non-executing). The real verify is still the v5 acceptance criteria.
- skip_impl_review: yes — comment-only change; no behavior to review
- produces_test_files: no

## Implementation Evidence (impler fills this in)
- File modified: `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp` — comment block at lines 650-672 (the 3-line stale "HLVM-bypass to isolate raster pass" note + the bug-088 explanation paragraph).
- Change: 3-line stale block replaced with 3-line accurate post-v5 description (whole-frame submission). Bug-088 paragraph updated to remove the now-stale "close+execute+waitForIdle at lines 1486-1491" reference and add a Note pointing to v5's NOTE comment near line 1516. Patch verified via `read_file` (offset 640-664) before and after.
- Net diff: +6 / -7 lines (net -1 line after reformatting).
- No behavioral change. Pure documentation drift correction.

## What v7 explicitly does NOT do
- Does NOT change FGIPass, FGIPassDesc, or any other runtime code.
- Does NOT change shaders, binding layouts, or pipeline behavior.
- Does NOT change the test harness, validator, or build system.
- Does NOT commit/push (cron rules).
- Does NOT trigger any v6 sub-plan (v6a/b/c/d remain gated on parent's v5 verification).