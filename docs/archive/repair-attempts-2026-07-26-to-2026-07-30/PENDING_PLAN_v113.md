# Pending Plan v113
- task: restir-gi-fix — audit the disputed repository-root depth calculation in both parent-side verification scripts before source-patch promotion
- source: no bundle — direct edit of existing verification tooling; `docs/restir-gi-fix-v101.patch` remains the pending source change
- approach: Review and, only if component counting confirms it, correct `REPO_ROOT` in `git-apply-preflight-v111.sh` and `fresh-evidence-scan-v93.sh`. The v111 markers claim five parent traversals are shallow; the reviewer must independently resolve the path before retaining any executable change. Preserve the v101 source patch unchanged and remove stale depth-count claims that disagree with the verified path.
- diff_estimate: comment-only correction in two shell scripts after reviewer verification; marker/audit files excluded
- skip_plan_review: no
- test_strategy: Role #5 must independently resolve the path-component count from `.../Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data` to the project root, compare both scripts with known-good sibling scripts, and confirm the v101 patch remains byte-stable at 102 lines / 3975 bytes. If terminal becomes available, run `bash .../git-apply-preflight-v111.sh`; otherwise label execution UNVERIFIED rather than PASS.
- risks: Shell comments and prior v111/v112 markers may be arithmetically wrong; blindly changing executable paths can regress the unblock recipe. Do not apply the v101 source patch in this cycle, do not touch unrelated working-tree changes, and do not claim any GPU acceptance criterion from static checks.

## Fresh evidence (planner)
- Both scripts currently use `${SCRIPT_DIR}/../../../../..` while their comments say six traversals are required.
- This contradiction requires independent reviewer arithmetic before any executable change is accepted.
- The terminal probe for this tick was denied with `pending_approval: tirith:unknown`, so build/run/validator/PNG acceptance remains UNVERIFIED.
