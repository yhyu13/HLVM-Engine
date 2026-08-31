# Pending Commit v23

- plan: docs/PENDING_PLAN_v23.md
- files: Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/run_rgi_diagnostic.sh
- source: no bundle — direct edit (file-only patch via `patch` tool)
- target: working tree (no commit/push per cron instruction)
- task: fix the off-by-one dump-rotation bug in `run_rgi_diagnostic.sh` so per-mode dumps are correctly labeled and mode99's output is preserved.
- verify: parent-driven — re-read the patched script and confirm:
  1. Pre-loop block (lines 81-89): stale pre-run dumps moved to `dumps_prerun`; fresh `dumps/` created.
  2. Inside loop: `dumps/` cleared before each run; archive created AFTER the run (`dumps_${mode_name}`).
  3. Post-loop block (lines 124-131): `cp -r dumps_default dumps/` (preserves archive); `mv` fallback if `cp -r` fails.
  4. Header comment (lines 26-30): v23 attribution added.
  5. Total file size grew from 7232 to 8799 bytes (+1567 bytes; +19 lines net after comment additions and structural reorganization).
- skip_impl_review: no — the script is user-facing evidence-collection; the off-by-one bug was flagged by the v24 outer-watchdog and the fix must be verified.
- produces_test_files: no
- notes:
  - v23 is a script-fix cycle only. No C++ / HLSL / CMake source touched.
  - The fix does NOT depend on terminal access. The cron applied it file-only.
  - The fix is fully reversible: `git checkout run_rgi_diagnostic.sh` restores the buggy v20 version.
  - The v20 audit verdict SOME_RELAX is NOT changed by this patch — the verdict remains SOME_RELAX until parent re-runs the fixed script and confirms the per-mode dumps are now correctly labeled.
  - The v22 PICK item status (gated on parent v20 evidence) is NOT changed by this patch — v22 remains `[ ]` and gated, but the gating now points at a working evidence-collection path.
  - The v22 PICK description should be updated to reference v23's fix when the parent re-runs the diagnostic, since v23 fixes the script that v22's evidence depends on.

## Plan Deviations (impler fills this in if it deviated from the plan)

None. The v23 impl produced exactly the patches specified:
1. Pre-loop archive (lines 81-89): stale pre-run `dumps/` moved to `dumps_prerun` before the first iteration.
2. Inside-loop archive (lines 113-121): archive created AFTER each run, named with the mode that produced the output.
3. Post-loop restoration (lines 124-131): `cp -r` with `mv` fallback preserves `dumps_default` as an archive.
4. Header comment (lines 26-30): v23 attribution added.

No source-code files modified outside the single shell script. No other markers modified except the v23 cycle markers themselves.

## File-level changes

```
M Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/run_rgi_diagnostic.sh
  - File grew from 161 lines (7232 bytes) to 199 lines (8799 bytes).
  - Net +38 lines after the patch.
  - Pre-loop block (was 5 lines): 9 lines (4-line archive-of-stale + 5-line inline comment).
  - Inside-loop rotation (was 5-line move-before-run): 9-line rm-mkdir (clears dumps/, creates fresh) + 9-line archive-after-run.
  - Post-loop restoration (was 5-line mv): 7-line cp-r with mv fallback.
  - Header comment (was 2 lines): 6-line v23 attribution block.
```

No other files touched. The v23 cycle is fully reversible: `git checkout run_rgi_diagnostic.sh` restores the buggy v20 version.

## What's next

The v23 cycle pauses at the reviewer stage (per Rule 5 of the six-role-pipeline state machine, since the impl-review marker is missing). After the reviewer issues KEEP, the tester writes PENDING_TESTS_v23.md, and the testing-verifier writes PENDING_TEST_AUDIT_v23.md.

After v23 closes, the parent-driven v22 evidence path is unblocked: parent runs `bash run_rgi_diagnostic.sh` and gets correctly-labeled per-mode dumps. The v22 PICK item then routes to v21a (the binding-layout-split fix) if hypothesis #1 (nvrhi-deferred-barrier-ordering) is confirmed by the correctly-labeled evidence.