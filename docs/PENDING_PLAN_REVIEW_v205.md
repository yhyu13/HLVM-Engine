# Pending Plan Review v205

- plan: docs/PENDING_PLAN_v205.md
- verdict: KEEP
- reviewer: agent_2_plan_criticer (tick-551)
- timestamp: 2026-08-30

## Design soundness

The plan is endorsed, and specifically its refusal to accept the card's
framing. I verified the plan's suspicion myself before endorsing rather than
letting the impler discover it: `FBilateralDenoisePass.h:27` declares
`NormalTexture` "(optional)", the layout comment at `FBilateralDenoisePass.cpp:90`
independently repeats "Normal guide - optional", and `Dispatch` guards it with
`if (Desc.NormalTexture)` while `DepthTexture` has **no** such guard anywhere in
the class. Three independent statements of optionality, one of them the guard
whose existence is the defect.

So the card's two options are both answers to a lesser question. The scale is
sourced from the one guide that is permitted to be null. A consumer that omits
the normal guide — which the API explicitly invites — takes the `else` path,
gets `GuideScale = 1.0`, and its **depth** guide is then indexed with the
identity map at full-res extents. That is v204's defect restored in full, by
the branch that appears to be handling the null case. It is worse than card O's
scenario because it requires no future consumer to do anything unusual.

## Plan completeness

Two additions required, both met by the impler:

1. Fix the source operand first (`DepthTexture`), then document the invariant.
   Documenting alone would leave a comment that is false for any consumer
   taking the optional path.
2. Confirm the "optional normal guide" path is a real API affordance and not
   dead. It is: `FReBLURPass.cpp:290-291` shows the same codebase manufacturing
   dummy guides precisely so an optional guide need not be supplied. The
   affordance is idiomatic here, not hypothetical.

## Verified before endorsing

- `DepthTexture` set by both consumers: primary `TestReSTIR_GI_Temporal.cpp:882`,
  control `TestCornellBoxGI.cpp:1480`. No consumer omits it.
- Both consumers' guide pairs share an extent, so card O's stated invariant does
  hold today and the swap is a **no-op at every current call site** — assignment
  sets closed, not names compared (v204 row 17): primary depth `:1655`/`:1664`
  from `W, H`; primary normal `:1630` from `WpDesc` (`:1620`, `W, H`); control
  both from one `Desc` block, `:1187` and `:1199`.
- Therefore the known-good control is unperturbed by derivation.

## Feedback for planner (FIX only)

None. KEEP.
