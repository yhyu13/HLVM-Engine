# Pending Commit v8
- plan: docs/PENDING_PLAN_v8.md
- files: Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp
- source: no bundle
- target: working tree (no commit/push per cron rules)
- task: comment-only cleanup of v4a diagnostic at lines 1685-1693 (stale HLVM-bypass reference)
- verify: parent-driven — `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test` then run with `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8`. Renderer output will be IDENTICAL to v7 (comments are non-executing text).
- skip_impl_review: yes — comment-only patch, <20 lines, no test files produced, no behavioral change
- produces_test_files: no
- notes: this is the second pass of stale-comment cleanup. v7 fixed lines 650-672; v8 fixes lines 1685-1693. Both reference the same v1-introduced HLVM-bypass that v5 removed. v7 missed v8's site because its PIPELINE_HEALTH search only enumerated the lines around the bug-088 paragraph; the v4a diagnostic comment was further down in the same file and was a separate downstream artifact of the v5/v6 mental model.

## Plan Deviations (impler fills this in if it deviated)
None. Patch matches plan exactly. Verified via patch tool diff and post-patch read_file at offset 1680-1705.