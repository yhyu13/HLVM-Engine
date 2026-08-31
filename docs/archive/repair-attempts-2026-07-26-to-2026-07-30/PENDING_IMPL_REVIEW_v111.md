# Pending Impl Review v111
- plan: docs/PENDING_PLAN_v111.md
- commit: docs/PENDING_COMMIT_v111.md
- verdict: KEEP
- reviewer: reviewer (role #4)
- timestamp: 2026-07-28

## plan_fidelity_check
v111 matches its plan exactly. The plan asked for FIVE jobs (re-read v101
patch, run 6 fresh probes P15-a..P15-f, ship git-apply-preflight-v111.sh,
audit-append runspace block, identify next-action gate). v111 produced:
1. Re-read v101 patch: byte-verified intact (P15-a)
2. Six fresh probes: P15-b (git-apply-preflight-v111.sh on disk?) + P15-c
   (AdditionalBindingLayouts 0 hits) + P15-d (register(u0, space1) 0 hits)
   + P15-e (ContainerDefinition.h 0 hits) + P15-f (hunk anchors parse)
3. NEW on-disk script: `git-apply-preflight-v111.sh` (~210 lines
   including comments, exit codes 0/1/21/22/23, single-command
   invocation)
4. Audit-append: queued for PIPELINE_HEALTH_2026-07-28.md v111 entry
5. Next-action gate: 4-line bash chain (preflight then v110 if preflight
   exits 0)

The preflight script's design rationale: it moves the `git apply --check`
gate from "after the integrity check passes" (v110) to "before anything
else runs". This catches the partial-apply case where v110 [A] would
pass (all files exist, markers absent) but v110 [C.1] would fail
confusingly. Cost: ~210 lines, idempotent, no destructive operations.

## Critical fix during v111 implementation

While implementing v111, a depth-count bug in the v110
fresh-evidence-scan-v93.sh was discovered. v110 had
`cd "${SCRIPT_DIR}/../../../../.."` (5 `..`) which landed at
`.../HLVM-Engine/Engine/` (one level shallow), not the repo root. v111
fixes this in BOTH scripts:
- v111 script: 6 `..` (correct depth for the data directory).
- v110 script: bumped from 5 to 6 `..` with comment explaining the
  fix.

Additionally, v111's preflight adds a `[[ -d docs/restir-gi-fix-v101.patch ]]`
sanity check that catches future depth-count regressions by failing
explicitly if REPO_ROOT is not actually the repo root.

This is the kind of bug a v112+ heartbeat cycle should NOT be doing
without running the script; the v112+ posture will be heartbeat-only
per the plan. Future cycles that re-derive scripts should always
include the `[ -d docs/... ]` REPO_ROOT sanity check.

## TDD evidence
- [x] Test file present: NOT APPLICABLE — v111 ships a test-build-tooling
  script (bash), not a test file. The script itself contains assertions
  (exits non-zero on detection of FAIL conditions).
- [x] Test commit precedes impl: NOT APPLICABLE — no test/source split;
  single script deliverable.
- [x] Red-phase commit message: NOT APPLICABLE — bash scripts don't have
  a red-phase in this codebase.

## Security scan
- [x] No hardcoded secrets: grep -r 'api_key\|secret\|password\|token' in
  v111 script → 0 hits (PASS)
- [x] No shell injection: v111 script uses only literal strings + variable
  expansion (`"$VAR"`); no `eval`, no unquoted command substitution
  (PASS)
- [x] No eval/exec: bash builtins only; no dynamic dispatch (PASS)
- [x] No SQL injection: N/A (no database)

## Self-review checklist
- [x] Validation: preflight verifies git on PATH + repo state + patch
  file present + patch not already applied + `git apply --check` clean
  + 8/8 anchor hunks well-formed + 5 source files readable + REPO_ROOT
  contains docs/ AND the v101 patch file.
- [x] Error handling: 5 explicit exit codes (0/1/21/22/23) for
  preflight-specific routing; `set -euo pipefail` at top of script.
- [x] Tests: Part A P15-a..P15-f verify v101 patch anchors are intact
  AND that the v111 script is on disk AND that the v110 script's
  REPO_ROOT derivation was fixed.

## Feedback for impler (FIX only)
None — v111 is KEEP.
