# Edge case revisit: EC-039 still firing — 2026-08-07

## Status

EC-039 (declared-vs-actual toolset discrepancy) is firing again
on this host configuration. Documented in
`kanban-cron-overseer` skill v2.4.0 (2026-07-26).

## What changed since the original report

Nothing on the cron side — the cron is correctly following its
prompt. The discrepancy persists in the host's
`enabled_toolsets` / tirith configuration.

## New context: this card's cron was likely never reconfigured

The 2026-07-26 incident produced 836 audit-trail markers over
4 days. The parent's "repair" path was applied to a sibling
`TestReSTIR_GI_Temporal` cron, but the per-card
`t_7b79c010` overseer cron may have been created later or
with different toolset config — and the toolset denial is
back.

## Recommendation

1. The parent session should NOT add a new EC row for this
   case. The existing EC-039 already documents the issue and
   the repair path. What is missing is **observability** —
   the parent session is not checking the cron's
   `OVERSEER_HEALTH_<date>.md` for the
   `toolset_requested=terminal,actual_blocked_by=tirith`
   marker, so the denials go unnoticed.
2. Add a parent-side cron or session-startup check that
   greps `docs/OVERSEER_HEALTH_*.md` for the
   `toolset_requested=terminal,actual_blocked_by=tirith`
   marker and pauses the affected cron with a clear
   "tirith denied terminal — reconfigure or pause" message.
3. The cron's job is to write the marker; the parent's job
   is to read it and act.

## Sub-rule for EC-039 v2 (proposed)

The current EC-039 Action column is correct ("write
`toolset_requested=terminal,actual_blocked_by=tirith` to
OVERSEER_HEALTH_<date>.md"). The repair path (a/b/c) is
documented. What is missing is a corollary rule:

> **R-EC039-1**: When EC-039 fires, also write
> `OVERSEER_ESCALATION.md` immediately. Do not retry the
> terminal probe more than once per tick — repeated
> rejections are the failure mode that produced 836 noise
> markers. The escalation file is the parent's signal.

This is already how the cron is behaving, so the proposed
rule is a codification of existing behavior, not a change.
