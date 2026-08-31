# Pending Commit v200

- plan: docs/PENDING_PLAN_v200.md
- files: **none — zero source files modified**
- source: no bundle
- target: (no branch — cron does not commit)
- task: Pre-build compile-risk audit of the unbuilt v183-v199 chain
- verify: `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Rebuild`
- skip_impl_review: no
- produces_test_files: no
- notes: audit only; the deliverable is the ranked risk list below

## Result: the chain is compile-coherent. No build-breaking defect found.

### Risk class (a) — arity/signature breakage (v197)

v197 changed `RenderGBuffer`'s arity. An arity mismatch is a hard compile error,
which is why v197's own review called it the *safest* kind of unbuilt change.
Verified tree-wide, not just in the edited file:

`RenderGBuffer(` over `Engine/Source/Runtime` → **exactly 2 hits**:
`:2173` the definition `void RenderGBuffer()`, `:795` the call `RenderGBuffer();`.
Both zero-arg. No forward declaration, no second call site, no other target.

**Positive control for that zero:** the same query shape returns 13 hits for
`RenderGBuffer` unparenthesised in the same file, so the query is live and the
2 is a real 2.

### Risk class (b) — C++/HLSL cbuffer layout desync (the v183/v184 class)

This is the dangerous class: it **compiles clean and renders wrong**. The plan
review required a four-way check rather than a two-way one. Done.

**Temporal constants — tail order, all four expressions:**

| Expression | Tail order after `DebugVis` |
|---|---|
| `FReSTIRPass.h:51-59` (C++, shared) | SceneYaw, PrevSceneYaw, NearPlane, FarPlane, GBufferScale |
| `FReSTIRPass.cpp:441-456` (marshaller) | SceneYaw, PrevSceneYaw, NearPlane, FarPlane, GBufferScale |
| `TestReSTIR_GI_Temporal_Data/…:33-42` (HLSL) | SceneYaw, PrevSceneYaw, NearPlane, FarPlane, GBufferScale |
| `TestCornellBoxGI_Data/…:36-40` (HLSL) | SceneYaw, PrevSceneYaw, NearPlane, FarPlane, GBufferScale |

Four-way agreement, in order, all plain scalars. The v184 rule (never an array
in this tail — HLSL puts each cbuffer array element on a fresh 16-byte register)
holds in all four: zero array declarations in any tail.

**Spatial constants — same check:**

| Expression | Tail order |
|---|---|
| `FReSTIRPass.h:64-72` | …MaxM, SpatialRadius, DebugVis, GBufferScale, Pad |
| `FReSTIRPass.cpp:536-547` | …MaxM, SpatialRadius, DebugVis, GBufferScale |
| `TestReSTIR_GI_Temporal_Data/…:16-27` | …MaxM, SpatialRadius, DebugVis, GBufferScale, Pad |
| `TestCornellBoxGI_Data/…:10-…` | …MaxM, SpatialRadius, DebugVis, GBufferScale (v187 note) |

Agreement. Note the marshaller stops one field earlier than the structs — it
does not write `Pad`. That is **correct and not a defect**: `Pad` exists only to
round the declaration, is read by nothing, and `ConstantsData` is `memset` to 0
before marshalling, so the slot is defined.

**Buffer overflow check.** All three marshallers declare `float ConstantsData[64]`
(`:351`, `:424`, `:532`). The longest is temporal: 16+16 floats of matrices via
`memcpy`, then 13 scalars = **45 of 64**. 19 floats of headroom. The appended
v183/v184 fields cannot overflow.

### Risk class (c) — silent zero-assignment (raised by the plan review)

The layout being right is not sufficient: `{}`-initialization yields
`GBufferScale == 0`, and the shader's `max(int(s),1)` launders 0 into the
identity map — a silent revert of v183 that leaves every marker's claims intact.
All four call sites checked; **all four assign explicitly**:

- `TestReSTIR_GI_Temporal.cpp:1061` temporal — `WIDTH / max(HalfResWidth,1u)`
- `TestReSTIR_GI_Temporal.cpp:1109` spatial — same expression
- `TestCornellBoxGI.cpp:1592` temporal — `1.0f`
- `TestCornellBoxGI.cpp:1645` spatial — `1.0f`

The control's `1.0f` is correct on its own terms: that target does not run
Phase D half-res, so full-res-over-dispatch is 1.

### Risk class (d) — v182 dual-copy divergence

`GIPathTracing.hlsl` exists in two copies and only one is compiled
(`ShaderMake.cfg`). If v182's four-site probe fix reached one copy and not the
other, the acceptance-path probe would behave differently from the source of
truth. `gbPixel` → **12 hits in each copy, at identical line numbers**
(499, 501, 502, 503, 524, 584, 757, 763, 764, 765, 766, 793). The copies are in
sync.

## Ranked residual risk for the operator's first build

1. **Low — v197 arity.** Verified consistent; would be a loud compile error if wrong.
2. **Low — cbuffer layouts.** Four-way agreement on both structs; headroom 19 floats.
3. **Unquantifiable — everything else.** Seventeen cycles of edits have never been
   compiled. This audit covers the two classes the lineage has *demonstrated*; it
   cannot cover typos in lines no query shape targets. Expect the first build to
   need one or two trivial fixes.

## Plan Deviations

One, declared. The plan scoped classes (a) and (b). I added (c) and (d):
(c) because the plan review explicitly required it, and (d) because the v182
dual-copy hazard is the one failure mode in this lineage that is *invisible to
a layout check* — both copies can be internally coherent and still disagree
with each other. Neither addition changed any source file.
