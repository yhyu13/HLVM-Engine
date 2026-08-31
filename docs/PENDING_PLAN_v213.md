# Pending Plan v213

- task: `FBilateralDenoisePass::FDesc::NormalTexture` is documented OPTIONAL and is
  structurally REQUIRED at every one of the four levels that could honour the claim.
- source: no bundle — re-derived from source (PICK's 3 cards L/M/N are all
  precondition-gated on a build that `terminal` refuses; re-deriving rather than
  emitting a 559th closure doc, per v191's precedent and `§Anti-patterns §6`).
- approach: correct the contract to REQUIRED at the declaration, at the layout
  comment, and at the `GuideScale` rationale that rests on the false claim; add the
  missing null guard in the same early-out form the function already uses for
  invalid dimensions. No shader touched, no cbuffer touched, no signature changed.
- diff_estimate: +6 / -0 functional (one guard), ~+20 / -6 comment, 2 files
- skip_plan_review: no
- test_strategy: file-only. Enumerate the optional-resource domain and show the
  partition; confirm both shader copies still read `t_Normal` unconditionally;
  confirm zero shader/cbuffer bytes changed.
- risks: the guard is a new early-out — must not fire for either live consumer.

## The defect

`FBilateralDenoisePass.h:37` declares:

    nvrhi::TextureHandle NormalTexture; // Normals for edge detection (optional)

There are exactly four places that could make that word true. **None of them do.**

| Level | Site | Honours "optional"? |
|---|---|---|
| header contract | `FBilateralDenoisePass.h:37` | claims it |
| binding layout | `.cpp:97` `Texture_SRV(2)` + `:90` comment "optional" | **no** — a plain layout item |
| binding set | `.cpp:199` `Texture_SRV(2, Desc.NormalTexture)` | **no** — unconditional, no ternary |
| shader | all three copies read `t_Normal.Load(...)` twice, ungated | **no** |

`NormalTexture` → **3 hits** in the whole `.cpp`: two are the comments v205 wrote,
one is the unconditional bind. There is no `if (Desc.NormalTexture)` anywhere in
the class.

## Why this is not a style nit — the class holds the affordance in both hands

`FBilateralDenoisePass` is the **only** one of four optional-resource sites in the
renderer that lacks a fallback. The domain, enumerated and partitioned:

| Site | Optional resource | Fallback | Shader gate |
|---|---|---|---|
| `FGIPass` u1 | `DebugStatsTexture` | `DummyDebugStatsTexture` (`:613-626`) | `Params3.z` |
| `FGIPass` u2 | `OutputDirection` | falls back to `OutputTexture` (v207) | n/a — mandatory target |
| `FReBLURPass` | `DepthTexture`/`NormalRoughnessTexture` | `EnsureDummyTexture` (`:290-291`) | dummy is valid data |
| **`FBilateralDenoisePass`** | **`NormalTexture`** | **NONE** | **NONE** |

Three of four manufacture a substitute. The fourth promises the same affordance
and provides nothing. A consumer that reads the header and declines the guide
hands a null handle to `createBindingSet` against a layout that declares t2
unconditionally — the binding set fails to build, and the failure surfaces at
`setComputeState`, nowhere near the omission.

## Why v205 could not have caught this, and why it is v205's residue

v205's finding was that `GuideScale` must not be sourced from an optional guide,
and its fix moved the source to `DepthTexture`. Correct. But the **reason** it
gave — `.cpp:176-177`, "NormalTexture is declared optional in the header" —
takes the false claim as its premise and writes it into the tree twice more
(`.h:32`, `.cpp:176`). v205 removed the class's only `if (Desc.NormalTexture)`
in the process. So the cycle that reasoned *about* the optionality is the cycle
that left the word standing with nothing behind it.

**This is the fourth instance of the camouflage mechanism** (v193's tautological
guard, v204's reassuring comment, v205's optional-guide branch): the construct
that reads as the safety measure is the thing that is absent.

## Severity: LATENT. Stated without inflation.

Both live consumers set the guide from a real texture — `TestReSTIR_GI_Temporal.cpp:883`
and `TestCornellBoxGI.cpp:1481`. **No pixel moves and no acceptance gate clears.**
The value is that the contract stops advertising an affordance that would fail.

## Remedy — and the one that was rejected

Two options. **Rejected: manufacture a dummy normal**, matching `FReBLURPass`.
It would make the word true, but the shader has no gate, so a constant (0,0,1)
guide makes `normalWeight` uniform across the kernel and the filter silently
degrades to depth-only — *wrong weights with no diagnostic*, which is the exact
failure signature this lineage has chased for thirty cycles. Making a false
promise true by adding a silent-degradation path is worse than withdrawing it.

**Chosen: withdraw the promise.** Declare it required, state why at the
declaration, and add the null guard.

## REVISION after plan-review FIX — the guard's form was wrong

The plan originally said "add the null guard **in the form the function already
uses**", i.e. copy the `invalid output dimensions` early-out at `:149-153`:
a `warn` log and a bare `return`.

**The plan gate rejected that form, and the objection is on the acceptance path.**
This pass writes `DenoisedTexture`, which is assigned to `AccumInput`, which the
accumulate pass reads to write `DisplayTexture` — the `display` dump that
`validate_restir_gi.py` runs all four structural checks on and that gate 6 would
inspect. A `return` before `dispatch` leaves `DenoisedTexture` **unwritten**, so
the consumer silently denoises with whatever that texture last held.

Worse, it would be a **diagnosability regression against today's behaviour**.
Today a null `NormalTexture` reaches `createBindingSet` against a layout that
declares t2 unconditionally, which fails loudly at binding-set creation. The
proposed guard would convert a loud failure into a silent stale-output path —
producing exactly the "no VUID, no error, just wrong pixels" signature this
lineage has spent thirty cycles chasing, in the one pass that feeds the
acceptance artifact.

**Revised remedy:** the guard logs at `err` (not `warn`), names the field, and
states in the message that the output is left unwritten. It sits with the
existing dimension check so both preconditions are validated in one place before
any constant is uploaded. The point of the guard is not to make a null handle
survivable — it is to make it **attributable**, which is what the binding-set
failure it replaces was already doing by accident.
