# Pending Plan v193

- task: Card F — seventh instance of the Phase-D/extent class, in the accumulate
  pass. First instance of the class **on the acceptance path**.
- source: no bundle — direct edit, re-derived from source this tick
- approach: Replace the three swapchain-derived extents in the accumulate block
  of `TestReSTIR_GI_Temporal.cpp` (`AccC.Width`, `AccC.Height`, and the dispatch
  grid) with the fixed `WIDTH`/`HEIGHT` that every resource the pass touches is
  actually created from. `GIAccumulate_cs.hlsl` is **byte-unchanged**.
- diff_estimate: +3 / -3 functional, ~+20 comment
- skip_plan_review: no
- test_strategy: file-only greps confirming (a) all three substitutions present,
  (b) the blit at the end of Render still uses `FB.width`/`FB.height` — the one
  place that legitimately must, (c) shader byte-unchanged, (d) no new extent
  source introduced, (e) positive controls for every zero.
- risks: over-substitution. `FB.width` is *correct* at the blit site (`:1269`)
  and at `RenderGBuffer`/`UpdateViewConstants` (`:764`, `:773`). A blanket
  replace would break presentation. The patch must touch **only** the accumulate
  block `:1235-1255`.

## Why this instance is materially different from v189/v191/v192

Every prior instance was latent or dead-output. This one feeds the artifact the
acceptance criteria are written against:

`DisplayTexture` (`:1660`, created `W`x`H`) is dumped as `display`
(`:2571`), and `validate_restir_gi.py` runs all four structural checks on that
dump. Gate 6 (vision) inspects the same image.

## The mechanism — and why the kernel's existing guard does NOT save it

`GIAccumulate_cs.hlsl:63` has an extent guard:

```hlsl
if (pixel.x >= Width || pixel.y >= Height)
    return;
```

v192's resolve kernel had **no** guard at all, and that was recorded as making it
the most severe instance. **This kernel has a guard and is still unsafe, because
the guard is parameterised by the wrong extent.** `Width`/`Height` arrive from
`AccC.Width = FB.width` (`:1238-1239`) — the swapchain — while `AccumTexture`
(u0) and `DisplayTexture` (u1) are `WIDTH`x`HEIGHT`. The guard therefore tests
the dispatch against *itself*, and is a tautology at every extent:

| swapchain | dispatch grid | guard clips at | writes vs 800x600 resources | effect |
|---|---|---|---|---|
| 800x600 | 100x75 | 800x600 | exact | correct |
| 1200x900 | 150x113 | 1200x900 | **out of bounds** | guard passes pixels 800..1199; `AccumTexture[pixel]` / `DisplayTexture[pixel]` store OOB — **undefined behaviour**, same as v192 |
| 600x450 | 75x57 | 600x450 | **under-covers** | columns 600..799 and rows 450..599 of `DisplayTexture` are **never written** |

**A guard against the wrong extent is worse than no guard**, because it reads as
protection to anyone auditing the kernel. This is the finding of the cycle.

## The narrowing direction is the one that reaches the acceptance gates

`DumpRGBA32FTexture` sizes its readback from the **texture descriptor**
(`:2791-2793` `TexDesc.width`/`TexDesc.height`), not from the dispatch. So on a
narrowed swapchain the `display` dump is still 800x600 and the unwritten region
is included verbatim. That region is whatever `AccumTexture`/`DisplayTexture`
last held — for the never-written tail, the initial contents.

`validate_restir_gi.py` check 1 is black-pixel-ratio < 5%. A 600x450 window
leaves 43.75% of the display dump unwritten. **That is a validator failure
produced entirely by a window size, with no VUID, no error and no log line** —
and it would present as a rendering regression.

## Files

- Modify: `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp` — accumulate
  block only (`:1235-1255`)
- Unchanged: `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/GIAccumulate_cs.hlsl`

Shader untouched keeps this cycle clear of the v182 dual-copy trap (two copies of
each shader exist; only the `_Data` one is compiled per `ShaderMake.cfg`).

## Acceptance criteria for this cycle

1. Three substitutions present; no fourth.
2. `FB.width` still present at `:1269` (blit), `:764`, `:773`.
3. `GIAccumulate_cs.hlsl` byte-identical.
4. No-op at the default 800x600 (`AccC.Width` is 800 either way), so the
   v183/v184/v185 chain awaiting its single run cannot be perturbed.
