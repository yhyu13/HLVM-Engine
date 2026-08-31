# Pending Commit v97

- plan: docs/PENDING_PLAN_v97.md
- files: docs/PENDING_PLAN_v97.md (contains verbatim Option-A patch text for parent `git apply`)
- source: no bundle — file-only state-machine consistency tick
- target: parent applies patch to working tree (NOT committed; user instruction forbids commit)
- task: ship verbatim Option-A patch text for `restir-gi-fix`
- verify: see PENDING_PLAN_v97.md "Parent-side apply + verify recipe" (3-command bash chain)
- skip_impl_review: yes — patch text only, no source-code lines written by cron
- produces_test_files: no
- notes: per user instruction "do not commit/push/rewrite history", the cron delivers the patch text as a marker; parent applies and verifies with terminal.

## Plan Deviations
None — impler delivered exactly what v97 plan asked for (verbatim Option-A patch text in PENDING_PLAN_v97.md).