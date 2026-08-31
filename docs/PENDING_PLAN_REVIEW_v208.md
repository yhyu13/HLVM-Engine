# Pending Plan Review v208

- plan: docs/PENDING_PLAN_v208.md
- verdict: KEEP
- reviewer: agent_2_plan_criticer (tick-554)
- timestamp: 2026-08-30

## Design soundness

The plan solves a real and correctly-identified gap. v200 is the lineage's only
pre-build audit and it covered v183-v199; `PENDING_COMMIT_v20{2,3,4,5,6,7}.md`
all name modified source files, so seven cycles of change sit outside the only
artefact the operator was told de-risks their first build. Card L's precondition
("do not action while the chain is unbuilt") makes that build the binding
constraint on the whole queue, so de-risking it dominates hunting a thirteenth
extent instance. Acceptance criteria are testable: each row is a source-decidable
comparison with a stated method.

**The scope-error framing is verified, not just asserted.** v202 caught v200/v201
scoping to a single `.cpp`; v204 caught v203 scoping to one class's layouts. The
plan's claim that the same shape recurs one level up — an audit that never swept
the cycles after itself — is the same error and is correctly attributed.

## Plan completeness

Domain is closed by construction: v201 modified no source (`- files: none`,
verified), so the delta is exactly v202-v207 and every one has a row. The
per-cycle table names the right class for each change. v204 is correctly singled
out as the only class-(b) member, which is the one a successful build does not
clear.

**One item the plan under-weights, raised as a note rather than a FIX**: the plan
lists v203 under "deletion" and asks for re-verification. It should say why that
row is not optional — the near-miss deleted three *live* binding items and was
caught only by the impler reading its own returned diff. If that read had been
skipped, the damage would be in the tree now and the first build would attribute
it to any of twenty-five cycles. I verified it myself rather than route a FIX:
`SpatialLayout` `:325-333` holds 7 items in correct order, `TemporalLayoutSRV`
`:236-248` holds cb + t0..t9 = 11; `BindingLayoutItem::` → 29 tree-wide in the
file as the positive control. **Intact.**

## The plan's strongest instruction

The risks section requires determining the *mechanism* behind the lineage's false
zeros rather than adding a twenty-first avoidance rule. This is the right call and
no prior tick made it. Twenty audit rows now catalogue individual query shapes
that "silently return 0" — tick-526 (alternation), v192 (escaped `+`), v199
(path-at-a-file), v196 (wrapped lines). Rules accumulated; the cause was never
diagnosed. A single mechanism that explains all four would retire the catalogue
and, more importantly, tell the lineage which of its **hundreds of recorded
zeros** are sound and which are vacuous. That is a higher-value output than this
cycle's nominal task.

Endorsed as written. No FIX items.
