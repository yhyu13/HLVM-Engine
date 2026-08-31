# Pending Plan Review v204

- plan: docs/PENDING_PLAN_v204.md
- verdict: KEEP
- reviewer: agent_2_plan_criticer (tick-550)
- timestamp: 2026-08-30

## Design soundness

Sound, and it is the correct next job under v203's own standing rule ("when a
cycle produces a new invariant, the NEXT cycle's default job is to sweep that
invariant's domain"). v203 applied the invariant to six pairs, all inside
`FReSTIRPass`. The domain was never closed at the level the invariant actually
ranges over, which is *every shared pass class*, not *every layout of one
class*. That is the same scope error v202 identified in v200/v201 (both scoped
to a single `.cpp`), recurring one level up.

I required one addition before endorsing, and the plan adopted it: sweep for
**both** established invariants per class, not just v202's. A class can agree
with both consumers' shader declarations and still be defective if one consumer
hands it textures of two different extents — the binding-agreement check cannot
see extents. Restricting the sweep to v202's invariant alone would have
produced a "clean" verdict on a defective pass.

## Plan completeness

Complete with that addition. The three named risks are the right three: they are
the exact mechanisms that produced the v182 dual-copy incident, the card-J
control-protection ruling, and the v203 near-miss respectively.

One further constraint I imposed: the enumeration of shared classes must be
derived from an actual query over the consumer directory, not from memory of
which passes exist. A sweep whose domain is recalled rather than derived can be
silently incomplete, and its "clean" rows would be unfalsifiable.
