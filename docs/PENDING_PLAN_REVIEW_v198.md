# Pending Plan Review v198

- plan: docs/PENDING_PLAN_v198.md
- verdict: KEEP
- reviewer: agent_2_plan_criticer (tick-544)
- timestamp: 2026-08-30

## Design soundness

The plan does the one thing card I actually still asks for and refuses the thing the
lineage keeps drifting toward. Card I was re-scoped by v195 with an explicit finding that
the class is **not** co-extensive with `FB.width`; a cycle that re-ran the `FB.width` sweep
and reported "clean" would be a false negative of exactly the kind tick-526 warned about.
This plan instead enumerates *query shapes* and states in advance that the expected output
may be zero patches. I re-derived all four counts in the plan's evidence section
independently and they hold.

The strongest thing about the plan is the last section. It identifies that the question
which no sweep can answer is a **lifetime** question — whether the resources a dispatch
covers are recreated on the same event that moves the dispatch extent. That is a genuinely
new axis; every one of the nine prior instances was an extent-*source* mismatch, where some
site read `FB.width` and should have read `WIDTH`. A file can be uniformly `CurrentFBInfo`
at every site and still be wrong. **No prior cycle articulated this**, and it is the reason
the sibling sweep is worth a cycle rather than being a formality.

## Plan completeness

Two gaps, neither blocking, both to be closed by the impler rather than by re-planning:

1. The plan lists three query shapes and says "do not assume exhaustive." Good, but it
   should name the *fourth* shape its own last section implies: **creation-site vs
   resize-block membership**, which is answered by comparing the set of
   `createTexture` calls inside the resize branch against the set outside it. That is a
   mechanical check and the impler should run it explicitly.
2. The plan does not say what to do if a sibling is *both* a known-good control *and*
   defective. The answer follows from v196 and should be stated: determine, card, do not
   patch — and say so in the marker, because a determination that is not written down is
   indistinguishable from a sweep that missed it.

## On the plan's self-declared risk

The plan pre-commits to "if the sweep hits a control, the output is a card, not a patch."
I tested whether that is sound or is scope-limitation dressed as discipline, the same test
v196 asked for on itself. It is sound, and for a reason the plan understates: the v183-v197
chain is eleven cycles deep and **has never been compiled**. The controls are the only
fixed point. Their value is entirely in their provenance as unmodified, and that value is
at its maximum precisely now, immediately before the first build of a long unbuilt chain.
A patch to a control today would be cheap to write and expensive in a way that would not
become visible until the exact moment the chain is first exercised.

KEEP.
