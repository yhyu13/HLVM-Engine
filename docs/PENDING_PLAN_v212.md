# Pending Plan v212

- task: Complete v211's deliberately-deferred scope — sweep the **remaining
  members of v182's dual-copy domain** that v211 enumerated but left for a
  separate cycle, and close the domain rather than leave it half-swept.
- source: no bundle — direct edit
- skip_plan_review: no
- diff_estimate: +0 / -0 expected (a determination cycle, not a patch cycle)

## Routing

`PENDING_TEST_AUDIT_v211.md:5` → `verdict: ALL_KEEP`, so the v211 cycle is
closed 6/6 and **Rule 9** fires (audit complete → next PICK item).

`PENDING_PICK.md` has **3** actionable `- [ ]` cards (L, M, N). All three carry
the same explicit precondition — card L: *"do NOT action while the v183-v198
chain is unbuilt"* — and that build is unreachable. `terminal` was probed
first-hand this tick (`pwd && ls -la | head -20 && date`) and refused at the
tool boundary with `pending_approval / tirith:unknown / exit_code -1`. This
is the ≥558th such refusal in the lineage.

So Rule 9 has three cards it cannot route. Per v191's precedent, and rather
than emit a 558th closure document (`§Anti-patterns §6` drift), the planner
re-derives from source.

## Why this scope and not a new seam

**v211 left its own domain half-swept, and said so.** Its `## risks` section
closes with: *"`GIAccumulate_cs.hlsl`'s two copies are in the same domain and
are deliberately left for a separate cycle, so this cycle's own 'three copies,
all enumerated' row stays verifiable."*

That deferral was correct discipline at the time — bundling would have made
v211's own enumeration unverifiable. But it leaves a **standing debt**, and
v203's standing rule (*when a cycle produces a new invariant, the NEXT cycle's
default job is to sweep that invariant's domain*) applies with more force to a
domain a prior cycle **explicitly named and skipped** than to a fresh seam.

This is v212's job: finish the sweep v211 started.

## Domain — re-derived, not inherited (audit row 20)

v211's own audit adopted row 20: *a cardinality claim is a fact about the tree,
not a fact about the lineage.* So the domain is re-enumerated here from the
tree, not copied from v211's table.

`search_files target=files pattern="*.hlsl" path=Engine/Source` → 205 files.
Excluding `_deps/`, `vcpkg/buildtrees/` and `Build/` vendor trees, the
first-party set partitions by basename into these multi-copy groups:

| Shader | Copies | Paths |
|---|---|---|
| `BilateralDenoise_cs.hlsl` | 3 | `Shader/`, `TestCornellBoxGI_Data/`, `TestReSTIR_GI_Temporal_Data/` |
| `GIPathTracing.hlsl` | 2 | `Private/Renderer/Shader/GI/`, `TestReSTIR_GI_Temporal_Data/` |
| `GIAccumulate_cs.hlsl` | 2 | `TestPathTraceGI_Data/`, `TestReSTIR_GI_Temporal_Data/` |
| `ReBLUR_cs.hlsl` | 2 | `TestCornellBoxGI_Data/`, `TestReSTIR_GI_Temporal_Data/` |
| `ReSTIR_Generate_cs.hlsl` | 2 | `TestCornellBoxGI_Data/`, `TestReSTIR_GI_Temporal_Data/` |
| `ReSTIR_Temporal_cs.hlsl` | 2 | `TestCornellBoxGI_Data/`, `TestReSTIR_GI_Temporal_Data/` |
| `ReSTIR_Spatial_cs.hlsl` | 2 | `TestCornellBoxGI_Data/`, `TestReSTIR_GI_Temporal_Data/` |
| `GBufferSponzaPS.hlsl` | **6** | 6 `Test*_Data/` dirs — **added at the plan gate** |
| `GBufferSponzaVS.hlsl` | **6** | same 6 dirs — **added at the plan gate** |

**The re-derivation corrects v211's table.** v211 listed **four** groups. There
are **nine**. Three it omitted are the entire ReSTIR trio — the part of the
tree v183 through v188 spent six cycles on. v211's table was not wrong about
the four it listed; it was **incomplete**, and it did not say so.

## Row 20 recursion — this cycle committed the error it diagnosed

**Stated plainly rather than quietly corrected, because it is the cycle's
substantive finding.** The first draft of this plan named **seven** groups and
made a point of correcting v211 from four to seven, citing row 20 while doing
so. The plan-criticer found that the draft had itself omitted the two largest
groups in the tree — `GBufferSponzaPS.hlsl` and `GBufferSponzaVS.hlsl`, six
copies each, three times the modal group size — and returned **FIX**.

So row 20 was adopted one cycle ago to stop exactly this, and it fired on the
very next cycle, against the cycle that adopted it, *in the paragraph that
cited it.*

**Row 20 as written is insufficient and needs extending.** It says: *re-derive
a cardinality in the cycle that relies on it.* Both v211 and this plan's first
draft obeyed that literally — each re-derived the copy count **of every group
it had already named**. Neither re-derived **which groups exist**. The
enumeration was run, quoted, and never partitioned.

Proposed extension, for the audit to adopt as row 21: *re-derive the domain's
membership, not only each member's count. An enumeration you ran but did not
partition is not evidence about the domain.*

## What the sweep found — 9 of 9 CLEAN

**1. `GIAccumulate_cs.hlsl` — CLEAN, byte-identical.**
Both copies read **79 lines / 3,006 bytes**, and a full end-to-end read of both
(not a diff, not a count) shows identical text including the header comment,
the `AccumConstants` cbuffer (4 scalars: `FrameCount`, `Width`, `Height`,
`Exposure`), the `t0`/`u0`/`u1` register set, `ACESFilm`, `LinearToSRGB` and
`main`. No divergence to fix.

**2. Its C++↔HLSL cbuffer agreement — CLEAN, and never checked before.**
This is the net-new work in the cycle. `TestReSTIR_GI_Temporal.cpp:1273`
declares the marshalling struct **locally**:
`struct FAccumC { uint32_t FrameCount; uint32_t Width; uint32_t Height; float Exposure; };`
— four scalars, same order, same kinds, no array and no vector in the tail, so
neither the v184 array rule nor the straddle rule is engaged. Written whole at
`:1297` via `writeBuffer(AccumulateConstants, &AccC, sizeof(AccC))`, not
field-by-field into a flat `float[64]` — so the entire v186/v187/v188 class of
marshaller-vs-shader drift **cannot arise here by construction**.

Worth stating because this shader is the one that writes `DisplayTexture`.

**3. The ReSTIR trio — CLEAN, already reconciled, and the divergences that
remain are documented and correct.**

- `ReSTIR_Generate_cs.hlsl`: both copies declare `float Pad0; float Pad1;` as
  plain scalars, with v186's rationale written into both. The primary
  additionally declares `gDirection : register(t4)`; that is card M's known
  ternary-fallback divergence, already carded, not a new finding.
- `ReSTIR_Temporal_cs.hlsl`: both declare the five-scalar tail
  (`SceneYaw`, `PrevSceneYaw`, `NearPlane`, `FarPlane`, `GBufferScale`) in the
  same order — v188 reconciled this. The control lacks `GB()` and lacks
  `t8`/`t9`; both are card N, already carded.
- `ReSTIR_Spatial_cs.hlsl`: both declare `GBufferScale` then `Pad`. v187
  reconciled it. The control lacks `GB()` **and this is correct** — its own
  comment at `:28-32` records that `TestCornellBoxGI.cpp` dispatches this pass
  at full res, so the scale is 1 and a `GB()` there would be wrong.

**4. `GIPathTracing.hlsl` / `ReBLUR_cs.hlsl` / `BilateralDenoise_cs.hlsl`** —
verified clean by v211 (the first two) and patched by v211 (the third).

**5. `GBufferSponzaPS.hlsl` (6 copies) — CLEAN. The six partition into TWO
LEGITIMATE VARIANT FAMILIES, not one file with five stale siblings.**

This is the group the plan gate added, and the determination is the reason it
mattered: a naive "six copies of one shader" reading would have concluded four
of them were stale and produced four wrong patches. All six read in full:

| Variant | Size | Copies | Contract |
|---|---|---|---|
| 5-MRT RT | 2,270 B / 70 L | `TestReSTIR_GI_Temporal_Data`, `TestCornellBoxGI_Data`, `TestRTReflections_Data`, `TestRTShadowsGBuffer_Data` | MRT0-4, MRT4 = WorldPos for RT ray origin; `t0` only |
| 4-MRT PBR | 2,674 B / 77 L | `TestSponzaDeferred_Data`, `TestGPUInstancing_Data` | MRT0-3, MRT1 = Metallic/Rough/AO; `t0`-`t4` + tangent + `b1` |

**Within each family the copies are byte-identical**; across families they are
deliberately different shaders that happen to share a filename. The partition
is closed by two disjoint discriminating queries whose counts sum exactly to
the group size: `MRT4 : SV_TARGET4` → **4 hits** (exactly the RT family, one
per file, all at `:39`) and `Tangent : TEXCOORD3` → **5 hits**, of which 2 are
`GBufferSponzaPS.hlsl` (the PBR family) and 3 are VS/instanced-VS siblings.
4 + 2 = 6, with no file in both sets and none in neither.

**6. `GBufferSponzaVS.hlsl` (6 copies) — CLEAN, same two families**, confirmed
by the same `Tangent : TEXCOORD3` discriminator landing on the
`TestSponzaDeferred_Data` and `TestGPUInstancing_Data` VS copies — i.e. the VS
family split is *consistent with* the PS family split, directory for directory,
which is the property that would break first if one copy had drifted.

**7. `GBufferPT_PS.hlsl` — a SINGLETON, and it settles the job instruction's
card at the producer end.** Raised by the plan gate. `Resolve_cs.hlsl` → 1 hit;
`GBufferPT_PS.hlsl` is likewise outside v182's domain, so no dual-copy sweep
would ever have reached it. Read in full: `MRT2 : SV_TARGET2` is documented as
`GBufferMaterial (RGBA32F)` at `:9`/`:63` and written at `:77` as
`float4(albedo, Roughness)`, with `albedo` at `:75` a real texture sample
selected by `MaterialFlags` bit 0.

So the **producer** of the very MRT this card is named after writes real
per-pixel data, from a file that exists in exactly one copy and therefore
cannot be stale. Combined with tick-527 (layout↔set↔handle) and tick-528/v182
(consumer `gbPixel` dataflow, and the half-res probe bug that produced the
original black reading), **all four links of the card's chain — producer,
transport, consumer, and the probe that reported it broken — are now each
verified first-hand by some cycle.**

## Determination

**v182's dual-copy domain is now fully swept, and the correct output of this
cycle is NO PATCH.**

Nine groups, nine clean. The three open divergences (cards M, N and the
control's deliberate `GB()` absence) are each already carded or already
documented in-source with a stated reason. The two 6-copy groups are two
coherent variant families, not staleness.

Per v196's precedent — the cycle that closed card J with zero source change —
this is recorded as a determination, and the gradient toward manufacturing a
patch to justify the cycle is explicitly resisted. **That gradient was real
this cycle**: the 6-copy groups are exactly the shape that invites four
"bring the stale copies into agreement" patches, and reading them first showed
those patches would each have been wrong.

The lineage's own worst failure mode after 212 cycles in a thinning seam is
inventing work.

## test_strategy

File-only, every row falsifiable, and the rows must include the **negative**
claims since this cycle's output is a determination:
- the 7-group domain re-derived from the file enumeration, not from v211;
- both `GIAccumulate` copies read **in full** and compared, not counted;
- the `FAccumC` field set read at its declaration site, not grepped;
- **zero source files modified** — the load-bearing row, since a determination
  cycle that silently patched something would be indistinguishable from a
  patch cycle that under-reported;
- every zero controlled by a same-shape positive (v205);
- `limit_reason` read on every zero (v209).

## risks

- **Inventing a patch to justify the cycle.** The single largest risk. v196
  faced it and resisted; this cycle must too.
- **Trusting v211's four-group table.** Already fired — the table was
  incomplete. Re-derive.
- **Alternation false zeros** (tick-526). One term per query, `path` at a
  directory. Fired twice during v211 and once during this cycle's own sweep
  (`GB\(` → `grep: Unmatched ( or \(` — a *reported error*, correctly not read
  as a zero, per v205's rule).
