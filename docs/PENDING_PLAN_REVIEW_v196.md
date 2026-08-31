# Pending Plan Review v196

- plan: docs/PENDING_PLAN_v196.md
- verdict: KEEP
- reviewer: agent_2_plan_criticer (tick-542)
- timestamp: 2026-08-30

## Design soundness

The plan's job was to *determine*, not to patch, and it determined. Its
conclusion — card J is not a defect — rests on two independent claims, and I
re-derived both from source rather than accepting them.

**Claim 1: both `gbScale` operands are swapchain-derived here.** Re-derived.
Numerator: `UpdateViewConstants(CurrentFBInfo.width, CurrentFBInfo.height)` →
`Constants.RenderTargetSize[0] = float(W)`. Denominator: `Desc.OutputWidth =
CurrentFBInfo.width` → `FGIPass::DispatchRays` → `RTPipeline.DispatchRays(CmdList,
Desc.OutputWidth, Desc.OutputHeight, 1, SRVBindingSet)`. Both trace to the same
`CurrentFBInfo`, so the ratio is identically 1. **This is the exact structural
inverse of v195**, where the denominator came off `HalfW = W / 2` ← `W = WIDTH`
and could not follow the numerator. Sound.

**Claim 2: the window is non-resizable.** Re-derived: `WindowProps.Resizable =
false` with `WindowProps.Extent = { WIDTH, HEIGHT }`. Contrast re-derived in the
sibling: `TestReSTIR_GI_Temporal.cpp` sets `WindowProps.Resizable = true`. The
plan asserted this contrast and it holds. Sound.

**I found one thing the plan understated, and it strengthens its conclusion.**
The plan says the resize-detection block "exists, but nothing can trigger it."
Stronger: `FillGBufferTextures` early-returns on `bGBufferFilled` after the
first call, so even a hypothetical resize would never re-fill the GBuffer — the
`W * sizeof(float) * 4` row-pitch hazard the plan lists is unreachable on the
*second* call regardless of extent. Two independent guards, not one.

## Plan completeness

Complete for a determination cycle. Chain read end-to-end; both branches of card
J's own conditional evaluated; the branch taken is named.

One gap, non-blocking: the plan did not enumerate `CurrentFBInfo` exhaustively
before concluding. I did — 12 hits, every one classified: resize-detect (`:415`,
`:417-418`), view constants (`:422`), CPU GBuffer fill (`:423`), RT dispatch
extent (`:438-439`), accumulate constants (`:471-472`), accumulate grid
(`:488-489`), blit destination (`:508-509`). **No site outside this set**, and
the blit destination is *correctly* swapchain-derived — it is the one quantity
that genuinely should follow the window. The candidate set is closed.

## On the question the plan asked me to rule on

The plan explicitly asked whether "do not touch the known-good control" is sound
or is self-serving scope-limitation. **It is sound, and the plan's reasoning is
better than it claims.**

The lineage's own methodology names this target's role: *"Keep a known-good
control. `TestCornellBoxGI` uses the same binding pattern as `TestPathTraceGI`
and works. That single fact eliminates the driver, nvrhi, slangc, and the whole
binding layer as suspects."* A control's value is entirely in its provenance. An
edit that is provably a no-op still consumes that provenance, and — the decisive
point — **the v183-v195 chain has never been built.** A compile error introduced
in the control would surface at exactly the same moment as the first build of
the chain the control exists to exonerate, and would be indistinguishable from
it. That is not timidity; it is refusing to correlate the instrument with the
measurement.

**And the counter-case fails on its own terms.** The nine prior substitutions
were all justified by *reachability* — a resizable window made each one live.
Here the window cannot resize. Substituting anyway would be applying the remedy
to the query shape rather than to the defect, which is precisely the error card
I's premise made and v195 falsified.

## Feedback for planner

None — KEEP. Two notes carried to the impler:

1. **The impler must write no source change.** If the impler finds itself
   editing `TestPathTraceGI.cpp`, the cycle has drifted; re-read this section.
2. A comment recording the invariant (`Resizable = false`, so swapchain ==
   `WIDTH`/`HEIGHT`) would itself be a source edit to the control. **Record the
   determination in the marker, where it cannot break a build.** The v192
   precedent — "the card's open question is now answered IN SOURCE so it cannot
   be re-litigated from the marker alone" — does **not** transfer here, because
   that cycle was already editing the file for functional reasons.
