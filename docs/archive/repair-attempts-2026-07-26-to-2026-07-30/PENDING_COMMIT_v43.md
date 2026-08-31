# Pending Commit v43 — fresh-evidence-scan.sh 17→21 patch inventory extension

- plan: docs/PENDING_PLAN_v43.md
- files: Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/fresh-evidence-scan.sh, docs/PENDING_PLAN_v43.md, docs/PENDING_PLAN_REVIEW_v43.md, docs/PENDING_IMPL_REVIEW_v43.md, docs/PENDING_TESTS_v43.md, docs/PENDING_TEST_AUDIT_v43.md, docs/PENDING_PICK.md, docs/PIPELINE_HEALTH_2026-07-27.md
- source: no bundle — direct edit
- target: working tree (cron subagent, no commit/push)
- task: extend fresh-evidence-scan.sh to verify v37/v38/v39/v40/v41 patches (closes the file-only diagnostic-surface completeness gap)
- verify: parent runs `bash Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/fresh-evidence-scan.sh`; expected MISSING=0 across all 27 CHECKS entries (was 22); banner unchanged.
- skip_impl_review: no
- produces_test_files: no
- notes: this is the LAST mechanically actionable file-only fix. After v43, every diagnostic signal wired into the pipeline is verified by the script. No further file-only work advances the renderer without terminal access for build+run+dump inspection.