# Pending Commit v206

- plan: docs/PENDING_PLAN_v206.md
- files: Engine/Source/Runtime/Public/Renderer/PostProcess/FReBLURPass.h
- source: no bundle — direct source read
- target: working tree (no commit, no push — per job instruction)
- task: Card P — determine whether `FReBLURPass` has the guide-extent relation
  v205 pinned in `FBilateralDenoisePass`
- impler: agent_3_impler (tick-552)
- verify: none reachable — terminal categorically denied (see below)
- skip_impl_review: no
- produces_test_files: no

## DETERMINATION: `FReBLURPass` is CLEAN. No twelfth instance.

Card P asked whether this is a twelfth instance of the extent class on the
acceptance path. **It is not.** Both halves answered:

**Q1 — does it derive a scale, or index raw?** It indexes raw, and the absence
is threefold, not merely a missing call: `GB(` → **0** in
`TestReSTIR_GI_Temporal_Data/ReBLUR_cs.hlsl` (controlled by **22** in the same
directory, same query — `BilateralDenoise_cs.hlsl` 6, `ReSTIR_Spatial_cs.hlsl` 5,
`ReSTIR_Temporal_cs.hlsl` 5, plus declarations); no scale **field** in the
size-pinned `FReBLURConstants`; and both guide reads are bare —
`gDepth.Load(int3(dispatchThreadID.xy, 0))` at `:157`,
`gNormalRoughness.Load(int3(dispatchThreadID.xy, 0))` at `:178`, with the
neighbour pair at `:115`/`:116` clamped to `OutputSize - 1` at `:112`.

**Q2 — are its guides full-res under Phase D in the primary consumer?** No. Per
the plan gate's directive I enumerated all six operands **from the creation site
that sizes each**, not from pipeline position:

| Operand | Value at the call site | Sized at | Extent |
|---|---|---|---|
| `CurrentRadianceTexture` | `FullResSpatial` | `:1711-1713` `CreateTexture2D(…, W, H, …)` | W x H |
| `HistoryTexture` | `ReBLURHistoryTexture[0]` | `:563-564` `Desc.width = WIDTH; Desc.height = HEIGHT` | W x H |
| `DepthTexture` | `LinearDepthTexture` | `:1655-1657`, re-created `:1664` from its own desc | W x H |
| `NormalRoughnessTexture` | `GBufferNormal` | `:1630` from `NmDesc = WpDesc`, width set `:1620` to `W` | W x H |
| `OutputTexture` | `DenoisedTexture` | `:1681-1683` `CreateTexture2D(…, W, H, …)` | W x H |
| `OutputWidth/Height` | `WIDTH`/`HEIGHT` | `:1241-1242` (v194) | W x H |

`const uint32_t W = WIDTH, H = HEIGHT;` at `:1616`. **All six agree. Clean by
enumeration, which is the stronger form the plan gate required** — and notably
NOT clean by the ordering argument the plan originally offered, which I did not
rely on.

The ordering argument is nonetheless *why* it is clean, and worth recording:
ReBLUR sits at `:1185` "(5.5)", after the Phase-D resolve at `:1117` "(5.4)
half-res → full-res", so it is downstream of the point where the pipeline
returns to full resolution. The half-res passes (bilateral `:892-893`, spatial
`:1100-1109`, temporal) are all upstream. **Phase D drew its boundary between
(5.4) and (5.5), and ReBLUR is on the far side of it.**

## THE FINDING IS A CONTRACT DIVERGENCE, AND IT IS THE OPPOSITE OF ITS SIBLING'S

v205 wrote into `FBilateralDenoisePass.h` that its guides need **not** match
`OutputWidth`, since half-res-over-full-res is exactly what `GuideScale` exists
for — and warned that a future reader might otherwise "fix" it by forcing guides
to the dispatch extent, undoing v204.

`FReBLURPass` has the **inverse** contract: it indexes guides raw, so its guides
**must** match `OutputWidth` exactly. Two sibling classes, same directory, same
`postprocess` namespace, near-identical `FDesc` shapes — and opposite guide
rules, one documented as of v205 and the other documented nowhere. **The
documented one actively misleads about the undocumented one**: a reader who has
just absorbed v205's invariant carries precisely the wrong rule into ReBLUR, and
the failure would be silent in the usual way (no VUID, wrong region sampled,
wrong denoiser weights).

## FIX APPLIED — comment only, +14 lines, 0 functional

The invariant is now stated at the `FDesc` declaration, in the same place and
the same form v205 used for the sibling, including the explicit cross-reference
that names the sibling and says *do not carry that rule across*. Two member
comments upgraded to `MUST be OutputWidth x OutputHeight`.

**Nothing else changed.** No `.cpp`, no shader, neither `ReBLUR_cs.hlsl` copy
(the v182 dual-copy hazard is not engaged — 2 copies confirmed by query), and
`TestCornellBoxGI.cpp` untouched, preserving the known-good control's provenance
for the 24th unbuilt cycle.

## Plan Deviations

**One, and it removes work rather than adding it.** The plan's risk 2 asked me
to determine whether the control's `ReBLURDesc.OutputWidth = CurrentFBInfo.width`
(`TestCornellBoxGI.cpp:1464`) is a defect. The plan gate pre-empted this and I
followed the gate: it is **card L's instance seen from the other end**, already
carded and build-gated. `ReBLURHistoryTexture` → 13 hits in that file, creations
at `:932`/`:934` inside the init-time `if (CVar_r_ReBLUR_Enable)` block, teardown
nulls at `:1085-1086`/`:1777-1778`, uses at `:1428`/`:1460`/`:1470`/`:1471`,
declaration at `:1869` — **none in the resize block at `:1160-1256`**, whose own
recreations at `:1233`/`:1237` cover `HDRTexture` and `DenoisedHDRTexture` but
stop short of the history pair. Swapchain-derived extent driving a
never-recreated resource: card L exactly. Not actioned, per its precondition.

## Notes for the reviewer

1. The determination is a **negative**, and this lineage's one near-miss (v203)
   occurred under a "comment-only" banner. I read the returned diff: five
   members present, both scalars present, `struct FDesc` and its brace intact,
   the `old_string` anchored on `struct FDesc` — a statement boundary — and not
   on a comment adjacent to a braced initialiser.
2. **Compile risk is nil and expressible as such**: the only non-comment tokens
   in the diff are on the two member-comment lines, and the members themselves
   are byte-unchanged. Struct layout, size and member set are identical, so every
   consumer including the control is unaffected at the type level. Same reasoning
   class as v205's header argument.
3. **NOT ESTABLISHED**: that anything compiles, links, runs, renders or
   validates. Terminal is categorically denied — probes this tick: a compound
   command, a bare `pwd`, and a no-op `true`, all refused with
   `pending_approval / tirith:unknown / exit_code -1`. A refused no-op builtin
   rules out command content, path and working directory. Nothing was built,
   run, or viewed.
