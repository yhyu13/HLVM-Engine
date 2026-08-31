# Pending Plan Review v193

- plan: docs/PENDING_PLAN_v193.md
- verdict: KEEP
- reviewer: agent_2_plan_criticer (tick-539)
- timestamp: 2026-08-30

## Design soundness

The plan solves the stated problem with the minimum change and its central claim
is **stronger than the card's**, so I re-derived it independently rather than
accepting it.

**Re-derived, not inherited:**

1. `AccC.Width = FB.width` / `AccC.Height = FB.height` at `:1238-1239`, dispatch
   `(FB.width + 7) / 8, (FB.height + 7) / 8` at `:1255`. Three sites. Confirmed
   by reading `:1234-1255` in full, not by grep count.
2. `AccumTexture` and `DisplayTexture` are created at `:1658-1661` from `W`/`H`
   inside `CreateGBufferTextures`; `W = WIDTH = 800` (`:106`), fixed. Confirmed.
3. `BackBufferResizing()` (`:1328-1331`) is a two-line body: `BindingCache.Clear()`.
   **No texture is recreated.** Confirmed by reading the whole override, which is
   the only way to be sure of an absence.
4. `WindowProps.Resizable = true` (`:2950`). The divergence is reachable, not
   theoretical.
5. `AccumInput` is `FullResGIRaw`/`FullResSpatial`/`DenoisedTexture` — created at
   `:1651-1656` and `:1624-1626`, all from `W`/`H`. Fixed. So **every** resource
   in the pass is fixed-size and the swapchain is the sole outlier. Confirmed.

## The plan's key insight is correct and I verified the part that carries it

The plan claims the kernel's guard is a tautology. Read directly
(`GIAccumulate_cs.hlsl:59-64`): `main` takes `SV_DispatchThreadID`, and the guard
compares it against `Width`/`Height` from `cbuffer AccumConstants`. Those are
exactly the two values the C++ sets from `FB` and exactly the two the dispatch
grid is computed from. So the guard clips the dispatch to the dispatch. It cannot
constrain the write to the resource.

**This is a sharper finding than v192's.** v192 recorded "no guard at all" as the
most severe form. A guard keyed to the wrong extent is *worse*: it presents as
protection. Anyone auditing this kernel for the v189-v192 defect class would see
`:63`, tick the box, and move on. I nearly did.

## The severity claim is the first in this class that I could verify end to end

I checked the validator rather than trusting the plan's characterisation:

- `validate_restir_gi.py:207-208` — `check_black_ratio(display)`, first of four.
- `:89-93` — luminance over `arr[..., :3]`, ratio of dark pixels.
- `:197-201` — the array is the `display` dump, loaded from the newest group.
- `TestReSTIR_GI_Temporal.cpp:2571` — `display` is `DumpRGBA32FTexture(DisplayTexture, ...)`.
- `:2791-2793` — the dumper sizes readback from `TexDesc.width`/`height`, i.e.
  **the resource, not the dispatch**.

That last link is the one that makes the narrowing case reach the gate: the dump
is 800x600 regardless of window size, so unwritten rows are included verbatim.
The plan's 43.75%-at-600x450 figure follows arithmetically
(1 - (600*450)/(800*600)). Against a 5% threshold that is a decisive failure.

**So this is the first instance of the class with a demonstrated path from the
defect to a named acceptance gate.** Prior instances were argued to be latent.

## Plan completeness

- Files: complete. One file, one block.
- Risk section: correctly identifies the real risk (over-substitution) and names
  the sites that must NOT change. I verified `:1269` is the blit into the
  swapchain framebuffer — `FB.width` is right there and changing it would be a
  genuine regression. `:764`/`:773` likewise drive view constants and the GBuffer
  raster, out of this cycle's scope either way.
- No-op-at-default claim: sound. `FB.width == 800` at startup (`:2949`
  `WindowProps.Extent = { WIDTH, HEIGHT }`), so all three substituted expressions
  evaluate identically. The v183/v184/v185 chain cannot be perturbed.
- Shader byte-unchanged: correct call. The guard is not wrong *in the kernel* —
  it is correct given correct constants. Fixing the constants fixes the guard.
  Editing the shader would also re-enter the dual-copy hazard for no gain.

## One thing the plan does not claim, and should not

It does not claim the accumulate defect explains any *observed* symptom. The
freshest run is 800x600, where the patch is a no-op. This is a latent defect on
the acceptance path, not a diagnosis of the current dumps. The plan is honest
about this; I am recording it so a later reader does not over-read the cycle.

## Feedback for planner

None. Proceed to impler.
