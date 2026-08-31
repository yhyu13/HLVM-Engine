# Pending Plan v185

- task: ReSTIR temporal/generation cbuffer `OutputSize` is FULL-res while the
  dispatch grid is HALF-res (Phase D) — the third and most consequential
  instance of the Phase-D omission class found in v182/v183/v184
- source: no bundle — direct source read
- planner: agent_1_planner (tick-532)
- timestamp: 2026-08-30

## Why this card and not the queued follow-up

The PICK queue's one actionable `[ ]` item is tick-531's follow-up:
`FReSTIRPass.h:28 TFP32 Pad[2]` vs `ReSTIR_Generate_cs.hlsl:22 float2 Pad`.
I re-verified it first-hand this tick and it is **real but inert**, exactly as
tick-531 recorded: the generation marshaller stops at
`FReSTIRPass.cpp:363` (`Constants.DebugVis`, offset 9) so the field is never
written, and `search_files pattern="gConstants.Pad"` over the whole Runtime
tree returns **0 hits**, so it is never read either. It cannot move a pixel.

While auditing that boundary I found a defect in the same file, same pass,
same Phase-D class as v182/v183/v184 — but on the **production** path and
with a far larger blast radius. Per `six-role-pipeline §Anti-patterns §5`
(don't spend a 6-role cycle on a 2-line inert patch) v185 takes the live
defect. The inert `Pad` card stays queued.

## The defect

Phase D made the ReSTIR passes dispatch at HALF resolution. The dispatch grid
is derived from `Desc.OutputWidth`:

- `FReSTIRPass.cpp:491-492` — `dispatchX = (outputW + 7) / 8`, `outputW =
  Desc.OutputWidth`
- `TestReSTIR_GI_Temporal.cpp:951-952` — `Td.OutputWidth = HalfResWidth`
  (`:1538-1541` `HalfW = W / 2`)

But the **constant** the shader uses for all its screen-space arithmetic is
set from the FULL-res framebuffer:

- `TestReSTIR_GI_Temporal.cpp:961-964` — `TC.OutputSize = FB.width/height`,
  `TC.RcpOutputSize = 1/FB.width`
- `TestReSTIR_GI_Temporal.cpp:875-878` — same for generation (`C.OutputSize`)

At 800x600 / 400x300 the two disagree by exactly 2. Three consequences in
`ReSTIR_Temporal_cs.hlsl`, in increasing severity:

1. `:114-115` the early-out never fires (dispatch max 400 < 800). Benign.
2. `:136-137` `uv = (pixel + 0.5) * RcpOutputSize` — with `pixel` in [0,400)
   and `Rcp = 1/800`, uv spans only **[0, 0.5]**. The reconstructed NDC
   therefore covers only the top-left quadrant of the screen, so every pixel
   reprojects against geometry from the wrong quarter of the frame.
3. `:170-176` `prevPixel = prevUV * outputSize` with `outputSize = 800x600`
   yields `prevPixel` up to 800, and the bounds test `prevPixel <
   int2(outputSize)` **accepts** it — but the history reservoirs
   (`TemporalReservoir0..3`, `:1557-1570`) are 400x300. Every `prevPixel`
   ≥ 400 is an out-of-bounds `Load` on the reservoirs, and `GB(prevPixel)` =
   `prevPixel*2+1` reaches ~1600 on an 800-wide GBuffer. Out-of-bounds
   `Load` returns 0, so `prevDepth`/`prevNormal` are zero and history is
   rejected.

This is a **stronger and more direct** explanation of the lineage's
long-quoted `ReSTIR summary: M mean=2.93 max=9.0 (MaxM=30)` — ~10% of the
configured cap — than v183's texel-scale story alone.

## The decisive evidence that it is an omission, not intent

The **spatial** pass — same file, same Phase-D change, adjacent code — was
updated correctly:

```
TestReSTIR_GI_Temporal.cpp:1021-1024
    SC.OutputSize[0]    = float(HalfResWidth);   // Phase D
    SC.RcpOutputSize[0] = 1.0f / float(HalfResWidth);
```

Temporal at `:961` and generation at `:875` still read `FB.width`. Spatial is
the in-file positive control, exactly as `Resolve_cs.hlsl:60` was the witness
for v183. Someone updated one of the three call sites.

## Approach

Change 8 lines in `TestReSTIR_GI_Temporal.cpp` — `FB.width/FB.height` →
`HalfResWidth/HalfResHeight` in the temporal (`:961-964`) and generation
(`:875-878`) constant fills, matching the spatial block at `:1021-1024`
verbatim. No shader edits: every shader consumer already assumes
`OutputSize` == the dispatch grid, which is what makes the current values
wrong.

- diff_estimate: +8 / -8 lines, one file
- skip_plan_review: no — production path, and this cycle must not repeat
  v183's error of shipping a fix whose premise was never independently
  re-derived
- test_strategy: file-only static verification — three-way agreement between
  dispatch-grid source, constant source, and every shader consumer of
  `OutputSize`/`RcpOutputSize`; plus the spatial and Cornell negative
  controls.

## Risks

- **Over-application.** Spatial (`:1021`) is already correct; "fixing" it
  symmetrically would break it. Cornell (`TestCornellBoxGI.cpp:1556-1559`)
  dispatches at FULL res, so `CurrentFBInfo.width` is correct there. Both
  must be left alone — this is the same restraint v184 exercised.
- **Generation is benign today** (`OutputSize` only feeds the early-out at
  `ReSTIR_Generate_cs.hlsl:58-60`, and no thread exceeds the half-res grid).
  It is included for consistency and because it is a live trap for the next
  field added; flagged as such rather than oversold.
- **Cannot be executed here.** `terminal` is denied by tirith (probed twice
  this tick). No build, no run, no validator, no image. The falsifiable
  prediction below must be checked by the operator.
- **Interaction with v183/v184.** v184 made v183's temporal `GBufferScale`
  live; v185 makes the coordinate space that scale operates in correct.
  All three must be evaluated in one run.

## Falsifiable prediction

With v183+v184+v185 all present, `ReSTIR summary: M mean` should rise
materially from the long-standing 2.93 toward `MaxM=30`. **If it does not
move, the half-res-reuse hypothesis is wrong and must be recorded as a
refutation, not explained away.**
