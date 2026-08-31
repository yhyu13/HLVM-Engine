# Pending Commit v20

- plan: docs/PENDING_PLAN_v20.md
- files: Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/run_rgi_diagnostic.sh
- source: no bundle — direct write of consolidated diagnostic runner script
- target: working tree (no commit/push)
- task: write a one-shot bash diagnostic runner that builds TestReSTIR_GI_Temporal, runs modes 0/6/7/8/9/10/11/12/15/99, captures stderr+log per mode, runs the validator, and emits rgi_evidence.txt summary for v20 decision-matrix routing.
- verify: parent-driven: `bash Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/run_rgi_diagnostic.sh` (cron terminal blocked)
- skip_impl_review: no
- produces_test_files: no (the runner script is not a test file; it orchestrates runs of an existing test)
- notes:
  - Single new file. No source-code modifications to TestReSTIR_GI_Temporal.cpp, GIPathTracing.hlsl, FGIPass.cpp, or any other renderer file.
  - Script follows the project's bash style (mirrors Build.sh patterns: `set -euo pipefail`, path resolution via `$(dirname "${BASH_SOURCE[0]}")`).
  - Script assumes `python3` is on PATH for the validator.
  - Wall-clock estimate: ~7-9 minutes (1 Debug build + 10 mode runs + 1 validator).
  - The script's evidence file (`rgi_evidence.txt`) consolidates: build status, cerr-fire counts per mode, spdlog-marker counts per mode, per-mode PNG dump counts, and validator 3/3 verdict — all in one human-readable table.

## Plan Deviations (impler fills this in if it deviated from the plan)

None. The script was written exactly as the plan specified, with one minor adjustment noted below:

- **Mid-flight enhancement**: the plan's evidence summary section specified cerr fire + per-mode dump counts. The implementation adds two extra counters: v3 spdlog markers per mode (counted via `grep -cE 'LogGI|LogTest'`) and a per-mode stderr spdlog presence check. This makes the evidence file more directly useful for the v20 decision matrix (which needs to distinguish H-A: source/binary mismatch from H-B: spdlog-level-filter).
- **Mid-flight enhancement**: the plan's MODE_NAMES iteration uses bash associative-array iteration order which is not guaranteed. The implementation explicitly enumerates MODE_NAMES as a separate indexed array to ensure deterministic mode order in the evidence summary.
- **Mid-flight enhancement**: added `set -euo pipefail` early (matches project's Build.sh conventions, prevents silent failures on partial pipeline completion).

Total script size: 7232 bytes / 161 lines (vs plan estimate of +120 lines). The extra ~40 lines are the additions above plus more detailed per-phase logging.

## File-level changes

```
+ Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/run_rgi_diagnostic.sh   (new file, 7232B / 161 lines)
```

No other files touched. The script is reversible: deleting the file restores pre-v20 state.