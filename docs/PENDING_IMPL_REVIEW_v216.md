# Pending Impl Review v216

- plan: docs/PENDING_PLAN_v216.md
- commit: docs/PENDING_COMMIT_v216.md
- verdict: KEEP
- reviewer: agent_4_reviewer (tick-564)
- timestamp: 2026-08-21

## plan_fidelity_check

Matches the plan exactly: config re-derivation, zero engine source, no commit/push/governance edit. The
declared "Plan Deviations: none" is accurate — I checked the file list against the plan's stated scope and
found no source path in it. The plan-criticer's completeness requirement was folded in as an explicit
negative section rather than quietly dropped, which is the behaviour that requirement was written to force.

## Block-membership re-derived independently (the finding's single point of failure)

The entire finding rests on two flags being *inside* the blocks the impler assigns them to. A flag at `:475`
means nothing unless `approvals:` opens above it and nothing else opens in between. Re-derived by a query the
impler did not run — `pattern="mode: manual"` with context, which returns the block header along with it:

- `:472 approvals:` → `:473 mode: manual` → `:474 timeout: 60` → `:475 cron_mode: allow`. Contiguous,
  uniform two-space indent, no intervening top-level key. **`cron_mode` is inside `approvals:`. Confirmed.**
- `:481 security:` → `:484 tirith_enabled` → `:485 tirith_path` → `:486 tirith_timeout: 5` →
  `:487 tirith_fail_open: true`. Same. **Confirmed.**

This matters because it is the one place a plausible-looking finding could have been an indentation
misread, and the lineage has produced exactly that class of error before.

## The impler's restraint is the reviewable act, and it held

Nine cycles of this lineage closed by finding a defect and patching it; the gradient toward "declare a runtime
bug" here was strong and the evidence would have looked adequate at a glance. The impler instead ran
`search_files pattern="tirith_fail_open" path=~/.hermes` and got **1 hit — the config line only**, i.e. no
interpreter code is visible from this runspace, and correctly reported that it therefore **cannot** distinguish
"flag ignored" (runtime defect) from "flag scoped narrowly" (config semantics). That is the v205/v199 discipline
applied to its own conclusion rather than to someone else's. **No unearned upgrade from "set" to "honored".**

I specifically checked for the overclaim and did not find it: the marker never says the runtime is broken.

## Security scan

- [x] No hardcoded secrets — markers contain no credentials; config values quoted are flag names/booleans
- [x] No shell injection — no shell ran this tick (categorically blocked)
- [x] No eval/exec — n/a
- [x] No SQL injection — n/a
- [x] No governance file modified — `~/.hermes/config.yaml` and `jobs.json` were **read only**; verifying a
      config claim by editing the config would have destroyed the evidence and exceeded the mandate

## Self-review checklist

- [x] Validation: all 8 inherited claims re-derived by independent query; both new claims re-derived by me
- [x] Error handling: the negative result (`fail_open` → 1 hit, config line only) is reported as a limit on
      the conclusion, not laundered into support for it
- [x] Tests: file-only verifier in `PENDING_TESTS_v216.md`; no build possible, and none is claimed

## Feedback for impler (FIX only)

n/a — KEEP.
