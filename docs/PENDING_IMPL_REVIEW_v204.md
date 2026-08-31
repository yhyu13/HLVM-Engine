# Pending Impl Review v204

- plan: docs/PENDING_PLAN_v204.md
- commit: docs/PENDING_COMMIT_v204.md
- verdict: KEEP
- reviewer: agent_4_reviewer (tick-550)
- timestamp: 2026-08-30

## plan_fidelity_check

The impl matches the plan, including the plan-criticer's required addition
(sweep both invariants; derive the class domain by query). `## Plan Deviations`
declares none, and I agree — the sweep produced a clean row for `FReBLURPass`
and a defective row for `FBilateralDenoisePass`, which is what the plan asked
for. The clean row is not filler: it is what makes the defective row
falsifiable, since both classes have the same superficial shape (shared class,
two consumers, depth+normal guides, half-res-capable primary).

## What I re-derived independently rather than accepting

Per v195's standing rule (a marker's description of code is evidence about its
author, not about the code), I re-read every load-bearing claim.

**1. The extent mismatch is real.** `LinearDepthTexture` is created at
`TestReSTIR_GI_Temporal.cpp:1655-1657` from `W, H` — full res. `HalfResWidth`
is assigned at `:1671-1673` from `HalfW = W / 2`. The call site at `:892-893`
passes the latter as `OutputWidth/Height` while `:882-883` passes the former
two as guides. Confirmed at the creation sites, not inferred from the call.

**2. The control no-op claim — and this is where I nearly returned FIX.**

The marker says the control's guides and dispatch share an extent, so
`GuideW / outputW == 1`. But the two quantities are written with **different
variable names**: the guides are created from `GBufferWidth` (`:558`, `:885`)
and the dispatch is `CurrentFBInfo.width` (`:1483`). A no-op claim resting on
two differently-named variables is exactly the shape v191 got wrong — there,
`FB.width` and the GBuffer width coincided at startup and were still a defect,
because they were independent quantities that merely agreed.

So I closed it rather than assuming: `GBufferWidth` has exactly two assignment
sites, `:521` (`Framebuffer->getFramebufferInfo().width`) and `:1166`
(`CurrentFBInfo.width`). Both are the framebuffer width, i.e. the same quantity
the dispatch uses, at both init and resize. The ratio is **1 by derivation, not
by coincidence**, and unlike v191 there is no third quantity in play.

**This is the row that decides the cycle**, because a shared-class patch that
perturbed the known-good control would be strictly worse than the defect it
fixes (card J's ruling). It holds.

**3. `t_Input` correctly excluded from the remap.** `t_Input.Load` → 2 hits,
both raw `pixelCoord`; `t_Depth.Load` → 2 hits and `t_Normal.Load` → 2 hits, all
four through `GB()`. This is the discriminating detail of the whole patch: the
input is half-res *like the dispatch*, so remapping it would reintroduce the
out-of-bounds read v189 fixed at this very call site. A mechanical "route every
Load through the helper" edit would have broken it, and the marker calls this
out explicitly rather than leaving it implicit.

**4. No cbuffer overflow.** `ConstantsData` is `float[64]` (256 bytes) and the
new write is index 5. The v184 class needs a tail write near the boundary; this
is nowhere near it. `ConstantsData[5]` → 1 hit in this file (the other 2 hits
tree-wide are `FDOFPass`/`FLensEffectsPass`, unrelated structs).

**5. Dual-copy (v182) checked, not assumed.** `BilateralDenoise_cs.hlsl` → 2
files. The control's copy was already binary-compatible — its `Pad0` sat in the
float-5 slot the shared C++ now writes — so no drift was possible even before
the rename. The decision NOT to add `GB()` to the control's copy is correct:
adding an unused helper would alter the control's generated SPIR-V for no
behavioural reason, and the control's value is its provenance as unperturbed.

## The design choice I checked hardest

Deriving the scale from `Desc.NormalTexture->getDesc().width` inside the shared
`Dispatch`, rather than adding an `FDesc` field, is the right call and I
verified the reasoning rather than accepting it. Card G's ruling is directly on
point: `FReBLURPass::Dispatch` has a `getDesc()` fallback that the caller must
opt into by leaving a field zero, and v194 found it had never fired correctly
because it only reached the local dispatch variables and not the cbuffer. The
lesson is that opt-in fallbacks get missed. Deriving unconditionally means a
future third consumer cannot forget it.

One residual, recorded not as a defect but as a bound on the claim: the scale
is derived from the **normal** guide only, and the depth guide is assumed to
match it. In both current consumers they are created adjacently from the same
extent variable, so this holds today. If a consumer ever passes guides of two
different extents, this would silently use the wrong scale for one of them.
Not worth a per-guide scale now (it would double the cbuffer field for a case
no consumer has), but it is an assumption, and assumptions in this lineage have
a habit of becoming instances. Carded below.

## Security scan

- [x] No hardcoded secrets
- [x] No shell injection
- [x] No eval/exec
- [x] No SQL injection

## Self-review checklist

- [x] Validation: `max(int(GuideScale), 1)` shader-side keeps an unfilled or
      narrower guide on the identity map — it cannot divide by zero and cannot
      collapse the index the way v184's `GBufferScale == 0` did.
- [x] Error handling: guarded by `if (Desc.NormalTexture)` and `if (GuideW &&
      outputW)`; a null guide leaves the scale at 1.0.
- [x] Tests: file-only re-derivation only; **nothing was built or run.**

## NEW card O (opened at this gate)

`FBilateralDenoisePass::Dispatch` derives `GuideScale` from the **normal**
guide's extent and applies it to **both** guides. Both current consumers create
their depth and normal guides from the same extent variable, so the assumption
holds today and this is NOT a defect now. It is an unstated invariant
("`DepthTexture` and `NormalTexture` share an extent") enforced nowhere. Either
document it at the `FDesc` declaration or derive a per-guide scale. Bounded,
source-decidable, does **not** need a build. Deliberately not bundled — bundling
would have made this cycle's own "one new cbuffer field, one slot" row
unverifiable.
