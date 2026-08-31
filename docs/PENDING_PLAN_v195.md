# Pending Plan v195

- task: card H — camera aspect ratio via `UpdateViewConstants(FB.width, FB.height)`
- source: no bundle — direct edit
- approach: Card H asks whether the camera aspect should follow the window.
  Reading the callee shows the question is **mis-framed**: `W`/`H` do not feed
  only the aspect ratio. `UpdateViewConstants` also writes `VC.Size = {W, H}`
  into `ViewConstantsBuffer`, which the RT shader reads as
  `g_View.RenderTargetSize` and uses at `GIPathTracing.hlsl:498` to compute
  `gbScale = RenderTargetSize / DispatchRaysDimensions()` — the **v182/v183
  Phase-D scale factor**. So a swapchain-derived `W` corrupts the GBuffer
  sampling coordinate for every ray, which is an index defect of exactly the
  class this lineage has been closing, not a matter of taste. Additionally
  `RenderGBuffer` rasterises through `nvrhi::Viewport Vp(0, LastWidth, 0,
  LastHeight)` into the **fixed-size** GBuffer MRTs, and `LastWidth` is
  assigned from `FB.width`. Fix both by sourcing from `WIDTH`/`HEIGHT`.
- diff_estimate: +6 / -4 functional (plus comments)
- skip_plan_review: no
- test_strategy: file-only verifier — enumerate the complete `FB.width`,
  `LastWidth`, `LastHeight`, `RenderTargetSize` candidate sets and classify
  every hit; verify the aspect question is answered in-source; verify the
  blit is untouched.
- risks: (a) the aspect-ratio question is real *in addition* to the scale bug —
  must answer it explicitly rather than silently pick a side; (b) `LastWidth`
  is also the resize-detection state and the log field — changing its
  *assignment* would break resize detection, so only the **viewport
  construction** may change, not the variable; (c) must be a no-op at 800x600
  so the unbuilt v183-v194 chain is unperturbed.

## Root cause

Three distinct consumers, one wrong source:

| Consumer | Site | Effect of swapchain extent |
|---|---|---|
| camera aspect | `glm::perspective(..., float(W)/float(H), ...)` | anamorphic stretch vs fixed MRT |
| `VC.Size` → `g_View.RenderTargetSize` | written to `ViewConstantsBuffer` | corrupts `gbScale` at `GIPathTracing.hlsl:498` **and** the sky unprojection at `:525-526` |
| raster viewport | `Vp(0, LastWidth, 0, LastHeight)` | rasterises a wrong-sized window into fixed MRTs |

`gbScale = RenderTargetSize / DispatchRaysDimensions()`. The dispatch is
`HalfResWidth = W / 2` off the **file-scope `WIDTH`** (`:1649`), so the
denominator is fixed at 400. A 1200-wide window makes `gbScale = 3.0` and
`gbPixel` runs to 1200 against an 800-wide GBuffer — every `Load` past 800
returns 0, so `worldPos` is zero and the `length(worldPos) < 0.001` early-out
at `:518` sends those pixels down the **sky** path. A 600-wide window makes
`gbScale = 1.5`, sampling only the left 600 columns.

## Why the aspect question resolves to WIDTH/HEIGHT

Read the blit, as card H instructed. `BlitVS` emits a **fixed fullscreen NDC
quad** with `UV = position * (0.5,-0.5) + 0.5`, and `BlitPS` samples the source
with that UV. There is no letterbox term and no aspect correction anywhere in
the shader or in `BlitTexture` — the viewport is simply set to the destination
extent. **The blit therefore stretches.** Given that, the render target is
always presented stretched to the window, so the correct upstream aspect is the
aspect of the **render target**, i.e. `WIDTH/HEIGHT`. Making the camera follow
the window would double-apply the window's aspect: once in the projection and
once in the stretch. Card H's three options (wider FOV / letterbox / stretch)
collapse to one because the presentation stage already committed to stretch.

## Sites

1. `Render()` — `UpdateViewConstants(FB.width, FB.height)` → `WIDTH, HEIGHT`
2. `RenderGBuffer` viewport — `LastWidth`/`LastHeight` → `WIDTH`/`HEIGHT`

Explicitly NOT changed: the `LastWidth`/`LastHeight` assignments at `:756-757`
(resize detection for `BindingCache.Clear()`), the log field at `:2385`, and
the blit at `:1304` (its destination genuinely is the swapchain).
