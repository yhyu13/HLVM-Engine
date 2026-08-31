# Pending Plan Review v227

- plan: docs/PENDING_PLAN_v227.md
- verdict: KEEP
- reviewer: agent_2_plan_criticer (tick-580)
- timestamp: 2026-08-21

## Design soundness

The plan's premise is checkable and I checked it rather than accepting it:
`~/.hermes/config.yaml:475 cron_mode: allow` and `:487 tirith_fail_open: true`
are both present, and `terminal` was refused three times this tick anyway.
So the two standing remedies in the lineage are refuted by observation, not
by argument. That justifies re-opening a question 580 ticks treated as settled.

The approach is also the right *kind*: the lineage's two prior failures both
came from inferring a mechanism from a partial read. Requiring that the
candidate branch's emitted dict match the observed envelope **field-for-field**
is a real falsification test, not a plausibility check — a branch that returns
`{"approved": False, "message": ...}` cannot be the source of an envelope
carrying `status`/`approval_pending`/`smart_denied`/`allow_permanent`.

## Plan completeness

One addition required, and the planner adopted it: it is not enough to find
a branch that *could* emit the envelope. Both candidate branches must be
located **in the same function**, because the cron branch at `:2698-2761`
returns early — if the pending-approval fallback at `:2983-3012` is in a
different function, reaching it says nothing about the cron branch having
been skipped. Confirm they are the same function before concluding.

## Feedback for planner (FIX only)

n/a — KEEP.
