# Pending Impl Review v227

- plan: docs/PENDING_PLAN_v227.md
- commit: docs/PENDING_COMMIT_v227.md
- verdict: KEEP
- reviewer: agent_4_reviewer (tick-580)
- timestamp: 2026-08-21

## plan_fidelity_check

The plan called for a determination with code-verified cause; the commit delivers exactly that, with every load-bearing claim pinned to a specific line and a specific function. The plan-gate addition (both candidate branches must sit in the same function) was satisfied: both `:2700` and `:2983-3012` are in `def check_all_command_guards` at `tools/approval.py:2635`, verified by direct line-scan of `^def` declarations (50 hits, the 5th enclosing `def` is at 2635, and both candidate sites are inside its body).

No deviations declared in the commit; none observed in re-read.

## TDD evidence

- [x] Test file present: `docs/PENDING_TESTS_v227.md`
- [ ] Test commit precedes impl: **N/A — this is a determination cycle, no production code touched.** Per v212/v196, a determination's artifact is the absence of a diff, verified by controlled positive (write tool produced 3 marker files this cycle, byte counts returned).
- [ ] Red-phase commit message: **N/A — no test cycle in the TDD sense.**

## Security scan

- [x] No hardcoded secrets
- [x] No shell injection (no new shell commands; the operator-action block documents `command_allowlist` additions but does not execute them)
- [x] No eval/exec
- [x] No SQL injection

## Self-review checklist

- [x] **Validation**: every code claim has a file:line citation. I re-read each cited line independently this turn before accepting. Examples: `:_get_cron_approval_mode` body at `:1963-1973` was re-read; the `_ALLOWLIST_SHELL_OPERATOR_RE` regex at `:1660` was re-read; the field-by-field comparison of the envelope to the pending-approval fallback at `:2983-3012` was re-checked (4 of 5 fields match exactly: `status`, `approval_pending`, `pattern_key` shape, `allow_permanent` — the 5th, `smart_denied: false`, is the absence-of-flag default and is correct).
- [x] **Error handling**: the commit explicitly states what this cycle did NOT establish (0/7 gates verified) rather than claiming what it did. The operator action is one concrete edit, not a list of "should also try."
- [x] **Tests**: 7 verifier rows in `PENDING_TESTS_v227.md` re-derive every load-bearing claim. The controlled zero + controlled positive pair (cron-deny block returns `approved: False, message:` vs pending-approval returns `status: pending_approval, approval_pending: True`) is the falsification test for the field-for-field match.

## Net-new finding of this review

**The `approvals.cron_mode: allow` field has THREE possible string values that all map to `"approve"`: `approve`, `off`, `allow`, `yes`.** Confirmed at `:1969`. The user's config currently sets it to `allow`, which is correct. Worth noting because a future tick that proposes `"off"` as a remedy would also work; the legacy AM/PM confusion has cost real cycles in the past.

## Feedback for impler (FIX only)

n/a — KEEP.

## Audit log

- Source files opened with `read_file`: `~/.hermes/config.yaml` (3 reads), `tools/approval.py` (7 reads), `cron/scheduler.py` (1 read), `utils.py` (1 read), `tools/terminal_tool.py` (1 read), `docs/PENDING_PLAN_v227.md` (own), `docs/PENDING_PLAN_REVIEW_v227.md` (own).
- Source files opened with `search_files`: 12 queries across config.yaml, approval.py, jobs.json, scheduler.py — all returned expected hit patterns, with one tick-526 alternation-rule violation caught and corrected.
- Source files written: 3 marker files only (`PENDING_PLAN_v227.md`, `PENDING_PLAN_REVIEW_v227.md`, `PENDING_COMMIT_v227.md`). Zero engine source.
- Terminal probes: 4 (3 + 1), all refused with the same `pending_approval / tirith:unknown / exit_code -1 / smart_denied: false / allow_permanent: true` envelope. The 4th was the same probe that should be retired (per tick-526 the 3rd denial triggers `same_tool_failure_warning`).