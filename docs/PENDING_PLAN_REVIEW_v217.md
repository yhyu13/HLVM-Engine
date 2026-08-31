# Pending Plan Review v217 (revision 2)

- plan: docs/PENDING_PLAN_v217.md (revision 2)
- verdict: KEEP
- reviewer: agent_2_plan_criticer (tick-565)
- timestamp: 2026-08-21

## Design soundness

Revision 2 adopts the falsification rather than arguing with it, and the replacement rule keys on the
variable that actually discriminates. I re-verified both causes myself rather than accepting the plan's
report of my own review:

- **Cause 2 confirmed independently**: `path=~/.hermes/config.yaml pattern="redact_secrets"` → **1 hit at
  `:483`**. The `file_glob`-scoped form of the same query returned 0. So the token exists, the file was in
  the glob, and the match was suppressed. Not a timeout — `~/.hermes` with a `config.yaml` glob is a
  trivial walk. Two genuinely distinct mechanisms, as the plan states.
- **Negative control run**: `path=~/.hermes/cron/jobs.json pattern="ZZZ_NO_SUCH_TOKEN"` → 0, against
  `enabled_toolsets` → 4 on the same file. File-scoped queries are not matching indiscriminately, so the
  4-hit positives are real.

## Plan completeness

The revision closes every item I raised:

1. v216's dead rows are named explicitly, and the distinction between *conclusion survives* and *support
   survives* is stated rather than blurred. This matters: v216's conclusion was **correct**, and its
   control was **invalid**. A cycle that recorded only "conclusion holds" would have carried the broken
   instrument forward, which is what the last 39 cycles did with tick-526's `file_glob` note.
2. The dead row is re-derived file-scoped with a same-file control, not inherited.
3. The `tirith`-absent claim is preserved with its control intact and explicitly protected from
   over-correction.
4. The rule is stated in a form that subsumes tick-526 (no alternation), v199 and v215 (scope) as special
   cases of one failure — an uncontrolled negative. That is the right generalisation: all three prior
   rules fixed a *symptom variable* and each was falsified within a few dozen cycles.

## The judgement I am ratifying, and its limit

The plan claims **no engine defect and no acceptance-gate movement**, and that is correct. This cycle's
output is a correction to the lineage's *instruments*, not to the renderer. That is worth a cycle here
specifically because 564 ticks of negatives were produced by those instruments — but it must not be
reported as progress toward the seven acceptance gates. The plan says so. Verified: the diff estimate is
0 engine source lines, and no shader, `.cpp`, `.h`, or recipe file is named for modification.

## Feedback for planner

None. Proceed to impler.
