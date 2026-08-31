# Pending Plan Review v212

- plan: docs/PENDING_PLAN_v212.md
- verdict: KEEP (revised; first pass returned FIX — history retained below)
- reviewer: agent_2_plan_criticer (tick-558)
- timestamp: 2026-08-30

## Second pass — KEEP

All five FIX points addressed, and I re-derived the new rows rather than
accept them:

1. **Domain corrected to nine groups.** Both 6-copy groups added and, more
   importantly, *determined* rather than merely listed.
2. **Row 20 recursion stated plainly**, including that it occurred in the
   paragraph citing row 20. Proposed row 21 forwarded to the audit.
3. **`ReSTIR_Spatial` control row re-attributed** to card N's verification.
4. **Producer finding added** as sweep row 7.
5. **No-patch determination kept.**

**The revision is stronger than the correction I asked for.** I asked for the
two groups to be determined; the plan came back with a *partition* showing the
six copies are two legitimate variant families, and closed it with two disjoint
discriminating queries summing exactly to the group size (4 + 2 = 6, no file in
both sets, none in neither). I re-ran both: `MRT4 : SV_TARGET4` → 4 hits, all
at `:39`; `Tangent : TEXCOORD3` → 5 hits, of which 2 are PS copies. Confirmed.

**This is the row that would have caused real damage.** Six copies of one
filename is exactly the shape that invites "four of these are stale, patch
them" — and with the chain unbuilt, four wrong shader patches would have
surfaced at the operator's first build and been misattributed to v183-v212.
The plan's decision to read all six in full *before* forming a hypothesis is
what prevented it. Recording it as the cycle's strongest methodological row.

**One limitation I require be carried into the tests marker**: byte-identity
within each family is asserted from **size + line count + full read**, not a
content hash — this runspace has no hashing tool (v211 hit the same wall). Two
files of equal size and line count that I have read end-to-end and compared is
strong, but it is not a hash, and the marker must not imply otherwise.

---

## First pass — FIX (retained for the record)

### Design soundness

The plan's *choice of work* is sound and I endorse it: v211 explicitly named a
deferred scope in its own `## risks` section, and finishing a domain a prior
cycle named-and-skipped is a better use of a cycle than opening an eighth seam.
v203's standing rule supports it, and v196's precedent legitimises a
determination cycle whose correct output is no patch.

The plan's *determination* — that the members it examined are clean — is also
correct, and I re-derived each one rather than accept it (see below).

**But the plan's domain table is incomplete, and it is incomplete in exactly
the way it accuses v211 of being incomplete.** That is why this is FIX.

## Plan completeness — the defect

The plan re-derives v182's domain as **seven** multi-copy groups and makes a
point of it: *"The re-derivation corrects v211's table. v211 listed four
groups. There are seven."* It then adopts row 20's framing to explain why v211
went wrong.

Re-running the partition myself, one basename per query, `path` at a directory:

- `GBufferSponzaPS.hlsl` → **6 copies** (`TestReSTIR_GI_Temporal_Data`,
  `TestCornellBoxGI_Data`, `TestSponzaDeferred_Data`, `TestRTReflections_Data`,
  `TestGPUInstancing_Data`, `TestRTShadowsGBuffer_Data`)
- `GBufferSponzaVS.hlsl` → **6 copies** (same six directories)

Neither appears in the plan's table. The domain is **nine** groups, not seven,
and the two omitted groups are the largest in the tree — six copies each, three
times the modal group size.

**The plan made row 20's error while citing row 20.** It re-derived the
cardinality *within* the four groups v211 named and one it added, but it never
re-derived the *group set itself* from the enumeration it claims to have run.
That is the same shape as v211's failure one level up: v211 assumed a file has
two copies; v212 assumed the domain has the groups the lineage has discussed.

I note this without treating it as a tenth false-instrument mechanism — the
enumeration was available, the plan cites it, and nobody partitioned it. Same
diagnosis row 20 already gives.

## Independent re-derivation of the plan's clean rows

I did not take these on the plan's word.

**`GIAccumulate_cs.hlsl` — CONFIRMED clean.** Both copies read in full:
79 lines / 3,006 bytes each, identical through the header comment, the
4-scalar `AccumConstants` cbuffer, the `t0`/`u0`/`u1` register set, `ACESFilm`,
`LinearToSRGB` and `main`.

**Its C++↔HLSL agreement — CONFIRMED clean, and I strengthen the plan's
argument.** The plan says the local `FAccumC` struct
(`TestReSTIR_GI_Temporal.cpp:1273`) matches field-for-field. True. The stronger
point the plan understates: it is written **whole**, via
`writeBuffer(AccumulateConstants, &AccC, sizeof(AccC))` at `:1297`, rather than
marshalled field-by-field into a flat `float[64]`. The entire v186/v187/v188
drift class requires a marshaller that can disagree with a declaration about
*offsets*; a whole-struct write of a locally-declared struct has no such
degree of freedom. This is immunity by construction, not a passing check —
the same distinction v201 drew for the primary target's resize behaviour, and
it is worth recording in those terms.

**The ReSTIR trio — CONFIRMED clean, with one correction to the plan's
framing.** The plan lists the control's missing `GB()` in `ReSTIR_Spatial` as
"documented and correct," citing the in-file comment. I verified the comment
exists (`TestCornellBoxGI_Data/ReSTIR_Spatial_cs.hlsl:28-32`) and that it
asserts the control dispatches that pass at full res. **The plan did not verify
the assertion, only the comment.** Per v195's standing rule — *a marker's
description of code is evidence about its author, not about the code* — a
comment is a description too. It happens to be corroborated by card N's
independent analysis, so the row survives, but the plan should say it is
resting on card N's verification rather than on the comment's say-so.

## Net-new finding at this gate — the card's premise refuted at the PRODUCER end

The job instruction's card is the "GBuffer SRV binding fix." Ticks 526-528
refuted it at the *consumer* end (layout↔set↔handle chain in `FGIPass.cpp`).
Nobody checked the **producer**: the shader that writes the MRT.

`GBufferPT_PS.hlsl` is a **singleton** — 1 copy, so it is outside v182's domain
entirely and no dual-copy sweep would ever have reached it. Read in full:
`MRT2 : SV_TARGET2` is documented as `GBufferMaterial (RGBA32F)` at `:9`/`:63`
and written at `:77` as `float4(albedo, Roughness)`, where `albedo` at `:75` is
a real texture sample. So the material MRT is written with real per-pixel data
by a shader that exists in exactly one copy and cannot be stale.

That closes the last structural place a "GBuffer SRV binding" defect could have
hidden: producer (this file), transport (v527's layout↔set↔handle), consumer
(v528's `gbPixel` dataflow), and the probe that reported it broken (v182's
half-res coordinate bug, which explained the original black reading). All four
links are now verified first-hand by some cycle.

## Feedback for planner (FIX)

1. **Correct the domain table to nine groups.** Add `GBufferSponzaPS.hlsl` (6)
   and `GBufferSponzaVS.hlsl` (6), and determine both before claiming the
   domain is swept. A "domain fully swept" claim with two of nine groups
   missing is the strongest row in the marker and it is currently false.
2. **State the row-20 recursion honestly.** Do not quietly fix the table —
   record that this cycle committed the error it diagnosed, one cycle after the
   rule was adopted. That is the substantive finding: row 20 as written says
   *re-derive a cardinality*; it needs extending to *re-derive the domain's
   membership, not only each member's count*.
3. **Re-attribute the `ReSTIR_Spatial` control row** to card N's verification
   rather than to the in-file comment, per v195.
4. **Add the producer finding** (`GBufferPT_PS.hlsl` singleton, MRT2 real) —
   it is net-new, it bears directly on the job instruction's card, and it came
   from this gate rather than from the plan.
5. **Keep the no-patch determination.** Nothing found at this gate changes it.
   Both newly-added groups must be *determined*, not patched, unless a real
   divergence appears.
