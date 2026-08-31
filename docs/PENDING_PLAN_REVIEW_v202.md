# Pending Plan Review v202

- plan: docs/PENDING_PLAN_v202.md
- verdict: KEEP
- reviewer: agent_2_plan_criticer (tick-548)
- timestamp: 2026-08-30

## Design soundness

The plan identifies a seam that is genuinely uncovered rather than a
re-litigation of the exhausted extent seam. I verified the uncoveredness claim
before endorsing it: v200's marker checks "both HLSL copies" of the temporal and
spatial shaders; v201's checks are all inside `TestReSTIR_GI_Temporal.cpp`.
Neither ever evaluated `FReSTIRPass` **as a shared object against both of its
consumers at once**. That is a real and distinct scope.

The three questions are well-posed because each has a decidable answer in source
and each could go either way. I re-derived all three rather than accepting the
plan's framing — per v195's standing rule that a plan's description of code is
evidence about its author.

## Plan completeness

Complete, with one correction and one addition.

**CORRECTION to question 1's premise — and it strengthens the plan.** The plan
implies generation might need a `GBufferScale` like temporal and spatial. It
does not, and the reason is stronger than "it happens to work": the generation
shader **never reads a GBuffer texture at all**. `gWorldPos` (t1), `gNormals`
(t2) and `gDepth` (t3) each return exactly **1 hit** in the primary copy — the
declaration line — and 1 hit in the control copy. There is no `.Load` or `.Sample`
of any of the three in either file. The shader's `main` (lines 62-78, read in
full) touches only `gRadiance` and `gDirection`.

Both live inputs are **half-res**: `OutputTexture` (`:1675-1677`) and
`DirectionTexture` (`:1678-1680`) are both `CreateTexture2D(..., HalfW, HalfH,
...)`, and the dispatch is `HalfResWidth`/`HalfResHeight` (`:911-912`). Extents
agree, so no conversion is needed or wanted. **Generation is immune to the v183
class for the same reason v201 found the file immune to v198's class: by
construction, not by luck.** Adding a `GBufferScale` here would be cargo-culting.

**ADDITION — question 3 resolves to SAFE, and the plan should not have left it
open-ended enough to invite a patch.** `SpatialLayout` does mix five SRVs and one
UAV in one set (`:247-255`), unlike generation and temporal. But bug-075 is a
*layout-transition* hazard, and it requires a texture to be reachable as both SRV
and UAV in the same dispatch. Checked against the spatial call site (`:1083-1096`)
and the creation block (`:1686-1705`): the five SRVs are `OutputTexture`,
a `TemporalReservoir` pair chosen by frame parity, `GBufferNormal` and
`LinearDepthTexture`; the single UAV is `SpatialRadiance`, which is a distinct
texture that appears in **no** SRV slot. **No alias is possible, so there is
nothing for the split to fix.** The temporal pass needed the split precisely
because its history and output pairs could alias; spatial has no such pair.
Corroborated at the artifact level: `search_files path=Binary/Debug
pattern="00344"` → **0 hits**, i.e. the VUID the split exists to prevent has
never fired in any retained log.

**So two of the plan's three questions resolve to "safe, for a structural
reason." The third does not.**

## The finding that carries this cycle

Question 2 is a real defect, and I confirm the impler should treat it as such.

`GenerationLayoutSRV` unconditionally declares six items including
`Texture_SRV(4)` (`FReSTIRPass.cpp:157-164`). The two generation shaders
**disagree** about whether a t4 exists:

- `TestReSTIR_GI_Temporal_Data/ReSTIR_Generate_cs.hlsl:37` declares
  `gDirection : register(t4)` and reads it at `:71`.
- `TestCornellBoxGI_Data/ReSTIR_Generate_cs.hlsl` declares **t0-t3 only** —
  `gDirection` → **0 hits** in that file.

The layout is shared; the shaders are not. The control's pipeline is therefore
created from a layout advertising a binding its SPIR-V does not contain.

`Desc.DirectionTexture ? Desc.DirectionTexture : Desc.RadianceTexture`
(`:382`) keeps the *binding set* populated, and the control never sets
`DirectionTexture` (`GenDesc.DirectionTexture` → **0 hits** in
`TestCornellBoxGI.cpp`; its `GenDesc` block is `:1524-1532` and assigns six
fields, not seven), so the control binds its radiance texture twice. That
fallback is what makes this **latent rather than currently fatal** — it prevents
a null-handle binding — but it does not reconcile the layout with the shader.

This is the v182 dual-copy hazard in its **general** form, and the generalisation
is the net-new part: v182 concerned one shader existing in two places with the
same content, where the trap was editing only one copy. Here the two copies have
**deliberately different resource sets** and the object binding them is shared.
No dual-copy sweep phrased as "are the copies identical?" can express this,
because the copies are *supposed* to differ.

## Feedback for impler

1. Do **not** add `GBufferScale` to `FReSTIRConstants`. Establish the immunity in
   a comment instead, with the reason (no GBuffer texture is read; both live
   inputs are half-res like the dispatch).
2. Do **not** split `SpatialLayout`. Record why it is safe so the asymmetry with
   the other two layouts stops looking like an oversight to future cycles.
3. The t4 divergence lands in `TestCornellBoxGI_Data/` — **the known-good
   control**. Card L's precondition (do not modify the control while the
   v183-v199 chain is unbuilt) **applies**. Per v196, which faced this exact
   situation and correctly emitted no patch: **open a card, do not patch the
   control.** Document in the shared runtime file, which is not the control.
