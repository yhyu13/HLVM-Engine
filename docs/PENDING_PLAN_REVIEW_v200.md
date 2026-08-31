# Pending Plan Review v200

- plan: docs/PENDING_PLAN_v200.md
- verdict: KEEP
- reviewer: agent_2_plan_criticer (tick-546)
- timestamp: 2026-08-30

## Design soundness

The plan solves a problem that is actually on the critical path rather than
adjacent to it. Every remaining card is build-gated; the build is the blocker;
nothing has audited the build. That ordering is correct.

I tested the plan's central claim — that no prior cycle audited the chain for
compile risk — against the markers rather than accepting it. v196 and v198/v199
are the three zero-change cycles, and all three are *defect sweeps* over the
extent class, not chain audits. The v199 health doc raises the first-build risk
in its closing note but explicitly does not act on it. The claim holds.

## Plan completeness

One gap, and it is the gap that matters: the plan names class (b) as "silent"
but does not say how a file-only audit can distinguish a coherent layout from
an incoherent one. It can, and the plan should have said so — the contract has
**three** independent expressions (C++ struct in the shared header, the flat
marshalling sequence in `FReSTIRPass.cpp`, and the HLSL struct, in two copies
each), so agreement is checkable by comparing declaration orders pairwise. A
two-way check could be two-way-wrong; a four-way check across two shader copies
is a real test. Recorded here so the impler does not settle for a two-way check.

Second, smaller: the plan should verify that the value is not merely *declared*
in the right slot but *assigned* at each call site. `{}`-initialization gives 0,
and `max(int(s),1)` converts 0 into the identity map — i.e. a silent revert of
v183 that leaves every marker's claims intact. That is the v184 failure exactly,
and it is invisible to any layout check.

## Feedback for planner (FIX only)

n/a — KEEP. The two items above are scope notes for the impler, not defects in
the plan.
