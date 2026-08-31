# Pending Commit v198

- plan: docs/PENDING_PLAN_v198.md
- files: **none** — no source file modified
- source: no bundle
- target: no branch (no commit made)
- task: Card I remaining half — sibling-target sweep under non-`FB.width` query shapes
- verify: `search_files path=Engine/Source/Runtime/Test pattern="v198"` → 0 hits in any `.cpp`/`.hlsl`
- skip_impl_review: no
- produces_test_files: no
- notes: **A tenth instance of the extent class was found, in `TestCornellBoxGI.cpp`, and deliberately NOT patched.** It is the known-good control. Determination + new card L below.

## What the sweep found

The plan's fourth query shape — the one the plan gate required be made explicit,
**creation-site vs resize-block membership** — is the shape that found it. The first three
shapes found nothing, and would have reported the file clean.

`TestCornellBoxGI.cpp` reads correct under every extent-source query: every dispatch extent
and every texture creation uses `CurrentFBInfo`/`GBufferWidth`, self-consistently. The
defect is **not** an extent-source mismatch. It is a lifetime mismatch:

| Set | Members |
|---|---|
| `createTexture` calls **inside** the resize block (`:1160-1256`) | `:1180` `:1184` `:1187` `:1190` `:1193` `:1199` (GBuffer MRTs) + `:1233` HDR + `:1237` DenoisedHDR + `:1243` staging |
| `createTexture` calls **outside**, at init only (`:954-1010`) | `:968` `:970` `:972` `:974` `:976` `:978` Reservoir0/1/History/Merged + `:983` `ReSTIROutput` + `:985` `TemporalRadiance` + `:987` `RadianceHistory` + `:1004` `PrevDepth` + `:1008` `PrevNormal` + `:932/:934` ReBLUR history |

**Fourteen textures are created once, from the startup framebuffer extent, and are never
recreated.** The resize block recreates nine and stops. Yet the passes that write them are
dispatched at the *current* extent:

- `:1608-1609` `TempDesc.OutputWidth/Height = CurrentFBInfo.width/height` → writes
  `Reservoir0Merged`, `Reservoir1Merged`, `TemporalRadiance` (all fixed-size)
- `:1654-1655` `SpatDesc.OutputWidth/Height = CurrentFBInfo.width/height` → writes
  `ReSTIROutput` (fixed-size)
- `:1531` `GenDesc.OutputWidth = CurrentFBInfo.width` → writes `Reservoir0/1` (fixed-size)

`WindowProps.Resizable = true` (`:1917`), and the resize path demonstrably runs — it
recreates nine textures and calls `BindingCache.Clear()`. So on any resize the ReSTIR
dispatches launch a swapchain-sized grid over startup-sized UAVs.

## Why no query shape in card I's original list could have found this

Card I's premise was that the class is co-extensive with `FB.width`. v195 falsified that and
added the cbuffer-laundering shape. **This instance falsifies the weaker premise too**: it
is not visible in *any* extent-source sweep, because there is no wrong extent anywhere in
the file. Every site reads the right variable. The bug is in which sites the resize branch
contains. The query that finds it is a **set difference between two creation sites**, not a
pattern match.

That is a third, distinct discovery shape for this class, and it retires card I's framing of
"enumerate the query shapes" as sufficient.

## Severity, and the guard that hides it

`ReSTIR_Temporal_cs.hlsl:63-64` and `ReSTIR_Spatial_cs.hlsl:58-60` both guard with
`dispatchThreadID >= gConstants.OutputSize`. `OutputSize` is marshalled from
`OutputWidth`/`OutputHeight` — the same two values the dispatch grid was computed from.
**This is exactly the tautological guard v193 found in `GIAccumulate_cs.hlsl`: the guard
clips the dispatch against itself.** It offers no protection against the UAV being smaller
than the grid. Widening the window is therefore an unguarded out-of-bounds UAV store.

v193 recorded that a guard keyed to the wrong extent is worse than no guard, because it
survives an audit sweep. This is the second instance of that sub-pattern, in a different
target, and it corroborates the ranking.

## Plan Deviations

**None on scope.** One deliberate non-action, pre-authorised by the plan and the plan gate:

The plan pre-committed to "if the sweep hits a control, determine and card, do not patch,"
and the plan gate upheld that with the reason that the v183-v197 chain is eleven cycles deep
and has never been compiled. `TestCornellBoxGI` is that chain's known-good control
(`software-development-practices §Path-Tracing / RT Debugging Methodology` rule 4). **No
source file was modified this cycle.** The gradient to patch was strong here — the remedy is
mechanical, ~14 lines moved into the resize block — and it is being refused on the same
grounds v196 refused a three-character fix to the same file.

This is the second cycle in the lineage to close with zero source change, and the first to do
so while holding a *reproducible defect* in hand rather than a determination of no-defect.

## NEW card L — tenth instance of the extent class; the FIRST that is a lifetime mismatch rather than an extent-source mismatch

`TestCornellBoxGI.cpp` creates 14 ReSTIR/denoise textures once at init from the startup
framebuffer extent and never recreates them, while dispatching over them at
`CurrentFBInfo` every frame; `Resizable = true`. Both compute guards are tautological
(clipped against the dispatch's own extent), so a widened window is an unguarded OOB UAV
store. **Remedy is mechanical** — move the `:954-1010` creation block into the resize
branch, or drive it from a fixed constant as the primary target does. **Do NOT action while
the v183-v197 chain is unbuilt**: this is the known-good control, and its value is entirely
its provenance as unmodified. Correct sequencing is (1) build and run the primary chain,
(2) then fix the control. Source-decidable; does not need a build to *diagnose*, but the
build ordering is the whole reason it is deferred.

**Note for whoever actions it**: the sibling `TestRTReflections.cpp` (`:892-899`) and
`TestRenderSponza.cpp` (`:413-416`) have the same resize-block shape and should be checked
with the same set-difference query, not with an extent-source sweep.
