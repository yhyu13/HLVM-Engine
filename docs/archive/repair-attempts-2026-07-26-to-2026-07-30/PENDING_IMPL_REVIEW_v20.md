# Pending Impl Review v20

- plan: docs/PENDING_PLAN_v20.md
- commit: docs/PENDING_COMMIT_v20.md
- verdict: KEEP
- reviewer: cron (single-head; same model as planner + impler; freshness caveat applies per six-role-pipeline anti-pattern #7)
- timestamp: 2026-07-27

## plan_fidelity_check

The v20 implementation matches the v20 plan's intent: a single new bash script that orchestrates one rebuild + 10 mode runs + 1 validator + 1 evidence-summary emission. No source-code modifications; no renderer fixes. The script's purpose is to give the parent a one-shot command that captures the full v20 9-branch decision-matrix evidence.

Three documented mid-flight enhancements (PENDING_COMMIT_v20.md "Plan Deviations"):
1. Added spdlog-marker counts per mode to the evidence summary (extends the cerr-fire check from "is stderr reaching stream" to "are the v3 spdlog markers actually firing per frame").
2. Switched from associative-array iteration to indexed-array iteration to guarantee deterministic mode order in the evidence output.
3. Added `set -euo pipefail` early for fail-fast behavior (matches project convention from Build.sh).

All three enhancements are additive (more data captured, more reliable ordering, more robust failure handling). None deviate from the plan's core intent.

## TDD evidence

- [ ] Test file present: N/A (this is a runner script, not a test file). The runner orchestrates the existing TestReSTIR_GI_Temporal test binary and the existing validate_restir_gi.py validator.
- [ ] Test commit precedes impl: N/A
- [ ] Red-phase commit message: N/A

## Security scan

- [x] No hardcoded secrets — script contains no API keys, tokens, passwords.
- [x] No shell injection — script uses array literals and explicit variables; no `eval`, no `bash -c` with user input.
- [x] No eval/exec — `set -euo pipefail` is enabled; no dynamic command construction.
- [x] No SQL injection — N/A (no SQL).
- [x] No destructive ops outside documented scope — the script's `rm -f "$EVIDENCE"`, `rm -rf "$DATA_DIR/dumps"` and `mv "$DATA_DIR/dumps"` are all within the data_dir; they clean the runner's own output area. The `Build.sh` invocation is the project's documented build command.
- [x] Path traversal safe — script resolves REPO_ROOT from its own location via `cd "$(dirname ...)"` and never uses user input for paths.

## Self-review checklist

- [x] Validation: `set -euo pipefail` enables fail-fast on any command failure; each phase's output is tee'd to a log file for post-mortem.
- [x] Error handling: pre-flight checks for Build.sh presence and binary existence; build failure aborts before runs; per-mode run failures are logged but don't abort subsequent modes (the whole point is to capture evidence from all modes regardless of individual outcomes).
- [x] Tests: parent-driven (cron terminal blocked); the script's own self-tests are documented in its leading comments.

## Feedback for impler (FIX only)

None. The script's structure is sound, the deviations are documented and additive, and the safety checks are present. Single-head freshness caveat applies.