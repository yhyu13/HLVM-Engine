# Pending Plan Review v213

- plan: docs/PENDING_PLAN_v213.md
- verdict: **FIX → KEEP** (FIX issued on the remedy's form; plan revised; re-reviewed KEEP)
- reviewer: agent_2_plan_criticer (tick-559)
- timestamp: 2026-08-30

## Design soundness

The defect is real and the plan's diagnosis is correct at every level I
re-derived independently rather than accepted.

**Re-derived, not inherited:**

- `Desc\.` → **29 hits** in `FBilateralDenoisePass.cpp`, every one classified.
  Exactly **one** is guarded by a null test: `Desc.DepthTexture` at `:183`, and
  that guard exists for `GuideScale`, not for binding. `Desc.NormalTexture`
  appears **once** outside comments, at `:199`, unguarded. So the plan's "no
  fallback" is not an absence I failed to find — it is an absence across a
  closed enumeration of the struct's uses.
- **Consumer set closed at exactly 2** tree-wide (`BilateralDenoisePass` → 42
  hits: 1 CMake, 1 shader comment, 6 log lines from four runs, the class's own
  definition, and two consumers). `TestReSTIR_GI_Temporal.cpp:883` and
  `TestCornellBoxGI.cpp:1481` both assign a real texture. **Latent confirmed**,
  and the plan is right not to inflate it.
- **The shader claim holds in all three copies**, checked per file, no
  alternation (row: tick-526): `t_Normal` → 3 hits in each of
  `Shader/`, `TestReSTIR_GI_Temporal_Data/`, `TestCornellBoxGI_Data/` — one
  declaration and **two ungated `Load`s** in every copy. There is no
  `if`, no branch, no null-substitute anywhere in the read path.

## Plan completeness

The rejected alternative (manufacture a dummy normal) is rejected for the right
reason and the reason is verifiable: `FReBLURPass.cpp:289` documents its dummy
as "dummy normal = (0,0,1)", and with `normalWeight` computed from
`dot(n1, n2)`, a constant guide makes every kernel weight identical — depth-only
filtering with no diagnostic. The plan is correct that this class cannot take
`FReBLURPass`'s approach, **because `FReBLURPass`'s shader tolerates it and this
one has no gate**.

## The FIX — issued on the remedy's form, not its direction

The plan proposed the guard "in the form the function already uses," pointing at
`:149-153`. I re-read that early-out: `warn` + bare `return`, **before**
`dispatch`.

Copying it here is wrong, and the reason is that this pass is **on the
acceptance path**, which the plan did not weigh:

    Bd.OutputTexture = DenoisedTexture   (TestReSTIR_GI_Temporal.cpp:884)
      → AccumInput = DenoisedTexture
        → accumulate writes DisplayTexture
          → the `display` dump
            → validate_restir_gi.py's four checks (gate 5) + gate 6's image

A pre-dispatch `return` leaves `DenoisedTexture` **stale, not blank**. The
downstream pass consumes it regardless and the validator sees a plausible image.

**And it is strictly worse than today.** Today, a null handle at `:199` meets a
layout that declares t2 unconditionally (`:97`), so `createBindingSet` fails
loudly. The proposed guard would replace a loud failure with a silent one — the
"no VUID, no error, wrong pixels" signature, introduced deliberately, in the one
pass feeding the acceptance artifact. That is the same camouflage mechanism the
plan itself identifies as the class's recurring theme (v193, v204, v205); the
plan would have made it a fifth instance while describing the fourth.

**Required change:** the guard must be loud — `err`, name the field, and state
that the output is left unwritten — so a null handle stays attributable. The
plan was revised accordingly and I re-reviewed it.

## Post-revision verdict: KEEP

The revised remedy preserves the one property the current code has by accident
(loud failure) while fixing the contract that misleads a caller into producing
it. Diff stays small, no shader byte moves, no cbuffer field moves — so the
v182 dual-copy hazard and the v184/v200 cbuffer-layout hazard are both
un-engaged, and this cycle cannot perturb the unbuilt v183-v212 chain's
first build beyond one C++ translation unit.

## Feedback for planner

Addressed in the revision. One standing note for the impler: **do not touch
`:183`'s `if (Desc.DepthTexture)`** — it is v205's fix, it sources `GuideScale`,
and it is not the guard being added.
