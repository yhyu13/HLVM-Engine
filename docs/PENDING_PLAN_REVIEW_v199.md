# Pending Plan Review v199

- plan: docs/PENDING_PLAN_v199.md
- verdict: KEEP
- reviewer: agent_2_plan_criticer (tick-545)
- timestamp: 2026-08-30

## Design soundness

The plan's load-bearing claim is that card L splits into a build-gated half (the
`TestCornellBoxGI.cpp` remedy) and a non-build-gated half (the two sibling checks), and
that taking only the second half honours the card's precondition rather than evading it.
**I tested that claim against the card text rather than accepting the plan's paraphrase.**
The card's precondition is scoped by its own wording — *"do NOT action while the v183-v198
chain is unbuilt"* — and the object of "action" is the remedy it has just described
("move the `:955-1010` creation block into the resize branch"). The two sibling targets
appear in a separate sentence, as a check, with the words *"need the same check"*. A check
that modifies nothing cannot perturb the control, which is the entire rationale the
precondition rests on. **The split is sound and the precondition is honoured, not
circumvented.** Endorsed.

Acceptance criteria are testable: for each target, either the resize block contains every
extent-sized creation site, or it does not, and the partition is decidable by reading.

## Plan completeness

**One thing the plan did not check, and it determines whether this cycle is worth
running at all.** If either target pinned its window the way `TestPathTraceGI` does
(`Resizable = false`, which is what made v196's card J doubly moot), the set difference
would be vacuous and the cycle would be theatre. I checked before endorsing:

- `TestRTReflections.cpp:1342` — `WindowProps.Resizable = true;`
- `TestRenderSponza.cpp:614` — `WindowProps.Resizable = true;`

**Both are resizable, so both checks are non-vacuous.** Had either been pinned I would
have returned FIX to have the plan say so up front, because "clean" and "moot" are
different findings and the marker must not merge them.

**Second gap, raised but not blocking.** The plan's `test_strategy` correctly requires
the partition be done by reading rather than by query, per v198's new checklist row. It
does not say what the tester should do about a target whose resize block is *empty of
creations by design* — `TestRenderSponza.cpp:549-553` nulls a pipeline and clears the
binding cache and creates nothing. That is not the same shape as an incomplete resize
block, and the marker must distinguish "the set difference is empty because everything is
contained" from "the set difference is empty because there is nothing extent-sized to
contain." The impler should state which of the two holds for each target. Not a FIX —
the plan's `risks` already anticipates the mis-target case in substance.

## Feedback for planner (FIX only)

None. KEEP.
