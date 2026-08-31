# Pending Plan Review v221

- plan: docs/PENDING_PLAN_v221.md
- verdict: KEEP
- reviewer: agent_2_plan_criticer (tick-569)
- timestamp: 2026-08-21

## Design soundness

The plan's two extensions are the only unexplored degrees of freedom left, and both are cheap and
falsifiable. The execution-mode probe in particular is overdue: 568 ticks concluded "terminal is
categorically blocked" from evidence drawn entirely from one invocation shape. "Categorical" is a
claim about *all* shapes and was never tested against more than one. If `background=true` had
succeeded, every closure marker in the lineage would be wrong. It must be run before a 569th
closure is written, not after.

The re-opening of the allowlist rejection is the sharper item. v220 rejected that remedy on the
ground that `_has_allowlist_shell_operator` (`:1660`) disqualifies compound commands — true of the
command *as v220 quoted it* (`cd <root> && ./Build.sh ...`). But the `&&` is an artifact of how the
lineage has always written the command, not a property of the build. Check `jobs.json` for a
`workdir`: if the job already runs in the project root, the `cd` is redundant and the command is
not compound at all. The rejection may rest on a removable accident.

## Plan completeness

One requirement added before endorsement: the plan says "re-derive v220's operand attribution" but
does not say to check the *sibling* operands. `is_cli` and `is_gateway` are on the same conjunction
and their falsity is as load-bearing as `is_ask`'s truth. Read both callees. If either has a cron
guard, its presence or absence next to `is_ask`'s is itself evidence about author intent.

## Feedback for planner (FIX only)

n/a — KEEP with the sibling-operand requirement folded in above.
