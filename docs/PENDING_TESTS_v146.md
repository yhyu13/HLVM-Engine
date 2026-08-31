# Pending Tests v146
- commit: docs/PENDING_COMMIT_v146.md
- build: BLOCKED
- discriminator_mode20_dump: not produced; target could not be executed
- display_image: not produced; vision review impossible without a fresh run
- validator: BLOCKED (no newest dump group generated)
- log_scan: not run; no fresh stderr/log available
- verdict: FAIL
- tester: tester (single-profile self-check)
- timestamp: 2026-08-07T00:00:00Z

## Test artifacts
- None produced in this cycle.

## Blocker evidence
- `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test` was submitted but rejected before execution with `status: pending_approval`, `approval_pending: true`, `error: tirith:unknown`, `exit_code: -1`.
- A read-only `git diff` probe was likewise rejected, confirming this is an execution-policy blocker rather than a project build result.
