# Pending Plan Review v195

- plan: docs/PENDING_PLAN_v195.md
- verdict: KEEP
- reviewer: agent_2_plan_criticer (tick-541)
- timestamp: 2026-08-30

## Design soundness

The plan solves the stated problem and, more importantly, **re-frames the card
correctly**. I re-derived the load-bearing claim rather than accepting it:

- `GIPathTracing.hlsl:498` — `gbScale = g_View.RenderTargetSize.xy /
  DispatchRaysDimensions().xy`. Confirmed, 1 hit.
- The denominator: `FGIPass.cpp:741` dispatches
  `RTPipeline.DispatchRays(CmdList, Desc.OutputWidth, Desc.OutputHeight, 1, ...)`,
  and `Desc.OutputWidth = HalfResWidth` (`TestReSTIR_GI_Temporal.cpp:793`), which
  is assigned `W / 2` at `:1649` inside `CreateGBufferTextures` where
  `const uint32_t W = WIDTH`. **The denominator is fixed at 400 regardless of the
  window.**
- The numerator: `VC.Size = {float(W), float(H)}` is the 4th member of the C++
  `FVC` struct, matching `ViewConstants.RenderTargetSize` as the 4th member of
  the HLSL struct (`float4x4` ×3 then `float2`). Layout agrees.

So the ratio has a **variable numerator over a fixed denominator**, which is the
same defect shape as v191's `GBufferScale` — and notably the *inverse* of it:
v191 had the variable in the numerator of a C++-side ratio, this one has it in a
shader-side ratio. The plan is right that this is an index defect, not a
question of taste.

## Where I disagree with the card, and agree with the plan

Card H asserted the callee "uses its two parameters for exactly one thing: the
camera aspect ratio." That is false — `W`/`H` reach three consumers (aspect,
`VC.Size`, and transitively the raster viewport through `LastWidth`). **This is
the third card in a row whose deferral rationale dissolved on reading the
callee** (E, G, now H), and the first where the card's factual claim about the
callee — not merely its judgement — was wrong. The standing rule from the v194
audit should be strengthened: a card's *description of a callee* is unverified
until re-read, not just its design-choice claims.

## Plan completeness

Two gaps found and both are already handled correctly:

1. **`LastWidth` has a second role.** It is the resize-detection state at
   `:754-757` driving `BindingCache.Clear()`. The plan correctly scopes the
   change to the *viewport construction* at `:2319` and explicitly excludes the
   assignments. Had it changed the variable, resize detection would have become
   a dead branch (`WIDTH != WIDTH` never fires) and the binding cache would go
   stale on resize — a new bug. Good catch by the planner.
2. **The sky path.** The plan mentions `:525-526` uses `RenderTargetSize` for
   the unprojection but does not draw out the consequence. I checked: at a
   widened window `gbPixel` exceeds the GBuffer extent, `Load` returns 0,
   `length(worldPos) < 0.001` at `:518` is true, and those pixels take the
   **sky** branch — which then unprojects using the *same* wrong
   `RenderTargetSize`. So the failure is doubly wrong in that region, and it
   fails *silently and plausibly* (sky is a legitimate-looking output). Worth
   recording; does not change the fix.

## Aspect-ratio question — I re-read the blit and concur

`BlitVS` (`TestRayTracedTriangle_Data/BlitShader.hlsl:15-29`) emits a fixed
fullscreen NDC quad with `UV = positions * (0.5,-0.5) + 0.5`; `BlitPS` samples
with that UV; `FCommonRenderPasses::BlitTexture` sets the viewport to the passed
extent and draws 4 vertices. **No letterbox, no aspect term, no pillarbox.** The
presentation is an unconditional stretch. Therefore the camera aspect must be
the render target's, or the window's aspect is applied twice. The plan's
reasoning is sound and its conclusion follows.

This makes card H's "real design question" **also illusory** — but for a
different reason than E's and G's: those were dissolved by reading the callee,
this one by reading the *downstream consumer*. The question was well-posed; the
answer is simply forced by a decision already made in the blit.

## Verdict rationale

KEEP. Correct diagnosis, correctly scoped, no-op at 800x600, the exclusions are
right, and the design question the card raised is answered in source rather than
by preference.
