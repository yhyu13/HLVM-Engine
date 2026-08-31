# Pending Plan v196

- task: Card J — determine whether `TestPathTraceGI.cpp` is a tenth instance of the Phase-D/extent class
- source: no bundle — direct source determination
- skip_plan_review: no
- diff_estimate: TBD by determination; candidate range +0/-0 functional to +4/-4
- test_strategy: file-only row-table over the extent chain in `TestPathTraceGI.cpp`, with every zero paired to a same-shape positive (tick-526 rule); no `|` alternation; cite symbols, not `:NNNN` (v195 finding 6)
- risks: **`TestPathTraceGI` is the lineage's known-good control** (`software-development-practices §Path-Tracing / RT Debugging Methodology` rule 4: "Keep a known-good control"). A patch here is not free the way the prior nine were — it perturbs the one target used to exonerate the driver, nvrhi, slangc and the binding layer. The plan must justify a change here to a higher bar than "same query shape matched."

## approach

Card J says: *"Whether this is a defect depends entirely on whether that target's
render target and RT dispatch extent are independently fixed or both follow the
swapchain. If both follow the swapchain the ratio stays consistent and there is
no bug; if the dispatch is pinned to a constant while the numerator floats, it
is v195 again. I did not determine which."*

Determine which. Read the chain end-to-end before proposing any edit, per the
standing rule promoted in v195: *a card's description of code is evidence about
the card's author, not about the code.*

### The chain, read this cycle

**Numerator** — `Render()` calls `UpdateViewConstants(CurrentFBInfo.width,
CurrentFBInfo.height)`; the callee writes `Constants.RenderTargetSize[0] =
float(W)` / `[1] = float(H)`. Swapchain-derived. Same shape as v195.

**Denominator** — `Desc.OutputWidth = CurrentFBInfo.width` /
`Desc.OutputHeight = CurrentFBInfo.height`, and `FGIPass::DispatchRays` passes
those verbatim to `RTPipeline.DispatchRays(CmdList, Desc.OutputWidth,
Desc.OutputHeight, 1, SRVBindingSet)`. So `DispatchRaysDimensions()` is
**also** swapchain-derived.

**Therefore `gbScale = RenderTargetSize / DispatchRaysDimensions()` is
identically 1 in this target, at every window size.** Card J's first branch
holds: both operands follow the swapchain, the ratio stays consistent.
**This is NOT v195 again.** The v195 defect was a *variable numerator over a
fixed denominator*; here both float together.

### But the resources do not float

Every resource this target touches is created from the file-scope constants:

- `GBufferWorldPos` / `GBufferNormal` / `GBufferMaterial` — `CreateGBufferTextures`
  calls `CreateTexture2D(NvrhiDevice, WIDTH, HEIGHT, ...)`.
- `OutputTexture`, `AccumTexture`, `DisplayTexture` — all `CreateTexture2D(...,
  WIDTH, HEIGHT, ...)`.
- `WIDTH = 800`, `HEIGHT = 600`, file-scope `static const uint32_t`.

So a swapchain extent other than 800x600 would give: an RT dispatch grid larger
than the fixed `OutputTexture` (v192's unguarded-OOB-store shape); an accumulate
dispatch grid `(CurrentFBInfo.width + 7) / 8` over the fixed `AccumTexture` /
`DisplayTexture` UAVs, whose shader guard is fed `AccumConstantsData.Width =
CurrentFBInfo.width` — **the same tautological self-clipping guard v193 found**
(guard keyed to the dispatch, not to the resource); and `FillGBufferTextures(W,
H)` building `W * H`-sized staging vectors and calling `writeTexture` with a
`W * sizeof(float) * 4` row pitch into an 800-wide texture.

### The decisive fact — and it is a difference from every prior instance

`WindowProps.Resizable = false`, with `WindowProps.Extent = { WIDTH, HEIGHT }`.

The nine prior instances all lived in `TestReSTIR_GI_Temporal.cpp`, where
`WindowProps.Resizable = true` and `Render()` carries a live resize-detection
path (`FB.width != LastWidth` → `BindingCache.Clear()`) precisely because the
extent *does* vary at runtime. **In `TestPathTraceGI` the swapchain extent is
pinned at creation and there is no user path that changes it.** The target's
own resize-detection block exists, but nothing can trigger it.

### Proposed verdict and action

**Card J is NOT a defect.** Two independent reasons, either sufficient:

1. The `gbScale` ratio — the specific quantity v195 fixed — is self-consistent
   here because both operands are swapchain-derived. Card J's own first branch.
2. The swapchain extent is non-resizable and equal to `WIDTH`/`HEIGHT` by
   construction, so every swapchain-derived quantity in this target is a
   compile-time-equal alias for the fixed one.

**Proposed action: close card J with the determination, and change no source.**

The lineage's instinct is to substitute `WIDTH`/`HEIGHT` for defence in depth,
as v191-v195 each did. **I am proposing not to, and the reason is specific to
this file, not general timidity:** this target is the known-good control. Its
value is that it is *unmodified* — every time the lineage exonerated the driver,
nvrhi, slangc or the binding layer, it did so by pointing at this target
working. A no-op-by-proof edit still resets that provenance, and the v183-v195
chain is **already unbuilt**, so a compile error introduced here would be
discovered at the same moment as, and confounded with, the first build of the
chain it is supposed to exonerate.

The latent caveat is real but unreachable, and belongs in a comment at most.
**Recommend the plan-criticer rule on exactly this point**: is "do not touch the
known-good control" a sound reason to leave a provably-unreachable latent
mismatch in place, or is it the ninth consecutive cycle rationalising its own
scope? I have argued the former; the gate should test it.
