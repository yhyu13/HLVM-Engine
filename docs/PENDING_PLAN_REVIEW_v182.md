# Pending Plan Review v182

- plan: docs/PENDING_PLAN_v182.md
- verdict: KEEP
- reviewer: agent_2_plan_criticer (tick-529)
- timestamp: 2026-08-30

## Design soundness

The design solves the actual stated problem for the first time in the lineage.
Ticks 526-528 declared the card's premise "refuted" but left an unexplained
observation on the table: if the t3 SRV is correctly bound (which 527 proved
at the layout/set/handle level), why did mode 20 return solid black? A refutation
that cannot explain the original evidence is incomplete. This plan supplies the
missing explanation — the probe read a different address than the production
code — which reconciles BOTH facts without contradiction: the binding is fine
(527 was right) AND the mode-20 black was real (v24 observed it correctly, but
misattributed it).

Acceptance is testable: after the patch, mode 20 should return the same
per-material variation the direct `gbuffer_material` dump shows
(std ~[0.16,0.16,0.13]) rather than solid black.

## Plan completeness

Risks are acknowledged and each is checkable file-only. One caveat the plan
states correctly: this patch touches only `HLVM_RGI_DEBUG_VIS`-gated code, so
it cannot by itself change the display/validator gates — it makes the diagnostic
instrument honest, which is the prerequisite for the bisect to mean anything.
Note the plan does NOT claim the acceptance gates now pass; that separation is
correct and is why this is KEEP rather than FIX.

## Feedback for planner (FIX only)

n/a — KEEP.
