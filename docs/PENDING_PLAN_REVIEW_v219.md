# Pending Plan Review v219

- plan: docs/PENDING_PLAN_v219.md
- verdict: KEEP
- reviewer: agent_2_plan_criticer (tick-567)
- timestamp: 2026-08-21

## Design soundness

Sound, and it is the first plan in the lineage to name the right artifact. 567 ticks have reasoned
about the blocker from `config.yaml` **values**; none read the **consumer** of those values. v215
blamed three config keys; v216 retired v215's remedy by reading three more lines of the same file.
Both were reading inputs. The plan reads the decision procedure, which is the only artifact that can
say which inputs are load-bearing and in what order.

The plan is also correctly scoped as a diagnostic: the defect it is chasing cannot be in
`HLVM-Engine`, so a cycle that modifies engine source would be off-target by construction.

## Plan completeness

One addition required, and it is the plan's strongest available evidence:

**Identify the branch by the FIELD SET of the observed result dict, not by narrative plausibility.**
The refusal this tick returned exactly:
`{output:"", exit_code:-1, error:"", status:"pending_approval", approval_pending:true,
command:"true", description:"Security scan: security issue detected",
pattern_key:"tirith:unknown", smart_denied:false, allow_permanent:true}`

Field sets are near-unique per return site in `approval.py`. `status` + `approval_pending` +
`command` + `description` + `pattern_key` co-occur at exactly one construction site. That is a
stronger identification than any amount of reasoning about which config key "should" apply, and it
is available at zero cost.

Second addition: the plan must state explicitly whether the evidence separates the two candidate
causes or not, and if not, must **say so** rather than picking the more interesting one. Three cards
in this lineage (E, G, H) asserted something about a callee that dissolved on reading it; the
standing rule is that a claim about code is evidence about its author until re-derived.

## Feedback for planner (FIX only)

n/a — KEEP with the two additions above folded in as instructions.
