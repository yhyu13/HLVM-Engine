# Pending Plan v192

- task: card E — the resolve pass is dispatched and scaled from the resizable
  swapchain while every resource it touches is fixed-size. Sixth instance of the
  Phase-D/extent class.
- source: no bundle — direct edit
- planner: agent_1_planner (tick-538, second cycle)
- timestamp: 2026-08-30

## The card left one question open. Reading the shader closes it.

Card E flagged `Resolve_cs.hlsl:60`'s hardcoded `int2 fp = hp * 2 + 1;` as "a
genuine design decision rather than an oversight" and said deciding whether to
parameterise it was a real choice. **Having now read the shader in full (74
lines), it is neither a choice nor an oversight — it is correct and must not be
touched.**

`fp = hp * 2 + 1` maps a half-res texel to the centre of its 2x2 full-res
footprint. That mapping is a property of the **half-res-is-exactly-half**
relationship between `OutputTexture` (400x300) and the GBuffer MRTs (800x600),
both created in `CreateGBufferTextures` from the same `W`. It has nothing to do
with the swapchain. Parameterising it would add a constant that can only ever
hold 2.

So the fix is **C++-only**. The shader stays byte-unchanged — which also keeps
this cycle clear of the v182 "patched a copy nothing compiles" trap entirely.

## The defect

Three C++ sites in `TestReSTIR_GI_Temporal.cpp`, all in the resolve block:

```cpp
RC.RcpFullW = 1.0f / static_cast<float>(FB.width);    // :1109
RC.RcpFullH = 1.0f / static_cast<float>(FB.height);   // :1110
...
CommandList->dispatch((FB.width + 7) / 8, (FB.height + 7) / 8, 1);   // :1138
```

Everything this pass touches is fixed-size:

- **Outputs.** `FullResGIRaw` and `FullResSpatial` are created at `:1633-1638`
  via `CreateTexture2D(NvrhiDevice, W, H, ...)` inside `CreateGBufferTextures`,
  i.e. `WIDTH`x`HEIGHT`. The shader writes `FullResOutput[tid.xy]` at `:34` and
  `:73` with no bounds test at all.
- **Guides.** `LinearDepthTexture` (t1) and `GBufferNormal` (t2), bound at
  `:1129-1130` — the fixed GBuffer MRTs.
- **Input.** `HalfResRadiance` (t0) = `OutputTexture`, 400x300, bounds-clamped by
  the shader itself at `:49` against `HalfSize`, which is correctly fed from
  `HalfResWidth`/`HalfResHeight` (`:1105-1106`). **This operand is already
  right** — which is exactly why the bug is invisible.

## Consequences, both silent

`RcpFullSize` is used at `:30` `uv = (tid.xy + 0.5) * RcpFullSize` and then at
`:39` `halfUV = uv * HalfSize - 0.5` — so it defines the mapping from output
pixel to input texel.

- **Widened swapchain (e.g. 1200).** The grid launches
  `(1200+7)/8 = 151` groups → `tid.xy` up to 1207 against 800-wide outputs.
  `FullResOutput[tid.xy]` is an **out-of-bounds UAV store** — unlike the `Load`s
  in v189/v191, which return 0 harmlessly, a write past the end is undefined
  behaviour, not a silent zero. The `Load`s at `:31`/`:37` also go out of range,
  returning depth 0, which trips the `:32` early-out and makes most of those
  threads take the `:34` store path.
- **Narrowed swapchain (e.g. 600).** `RcpFullSize = 1/600` while the output is
  800 wide, so `uv` reaches 1.33 and `halfUV` reaches `1.33 * 400`; the `:49`
  clamp pins every one of those threads to the last column of the input. The
  right third of the image becomes a smear of the input's right edge. And the
  grid only covers 600 of 800 columns, leaving the remaining 200 **never
  written** — stale contents from the previous frame.

Neither produces a VUID. The second produces a visibly wrong image with no error
at all.

## approach

Substitute the fixed extent at all three sites, exactly as v191 did:

```cpp
RC.RcpFullW = 1.0f / static_cast<float>(WIDTH);
RC.RcpFullH = 1.0f / static_cast<float>(HEIGHT);
CommandList->dispatch((WIDTH + 7) / 8, (HEIGHT + 7) / 8, 1);
```

`WIDTH`/`HEIGHT` are the extent the outputs and guides were created with, so all
four bindings then agree. Three functional lines. No shader change.

Deliberately NOT doing:

- **Not touching `fp = hp * 2 + 1`.** Argued above: correct, and invariant.
- **Not touching `HalfSize`/`RcpHalfSize`** (`:1105-1108`) — already correct.
- **Not making the pipeline resizable.** Same answer as v191: real design
  question, much larger scope, every pass in the frame.
- **Not touching Cornell** — it has no resolve pass (`Resolve_cs.hlsl` exists
  only under `TestReSTIR_GI_Temporal_Data`).

## diff_estimate

+3 / -3 functional, plus ~8 comment lines. **No line-number references in the
comment** — v191 nearly regressed v190's zero-stale-refs achievement and the
lesson is now explicit: symbol names only.

## skip_plan_review

no. The reasoning about which extent is correct is exactly what v187/v188/v190
got wrong, and this cycle additionally asserts a *negative* (that the shader
hardcode is right and must not change). Gate it.

## test_strategy

File-only static verification. Rows must:

1. Confirm all three sites use the fixed extent.
2. Confirm `FB.width`/`FB.height` no longer appear in the resolve block.
3. Confirm `Resolve_cs.hlsl` is byte-unchanged (`hp * 2 + 1` still present, and
   the file absent from `files:`).
4. Confirm `HalfSize` inputs still come from `HalfResWidth`/`HalfResHeight`.
5. Confirm `FullResGIRaw`/`FullResSpatial` are still created from `W`/`H`.
6. Confirm no new stale line-number cross-reference.
7. **Every zero-hit row must be confirmed against a positive control** — three
   distinct false-zero mechanisms are now documented on this runspace
   (alternation, `output_mode=count`, over-escaped regex).

## risks

- **`WIDTH`/`HEIGHT` shadowing at the resolve site.** Must be re-verified for
  `HEIGHT` specifically — v191 only cleared `WIDTH`.
- **This cannot be run.** Argued from source only.
- **It cannot explain `M mean=2.93`.** The resolve pass runs *after* ReSTIR reuse
  and cannot affect reservoir accumulation. Like v191, this is a latent defect;
  reporting it as the cause of the live symptom would be the fabrication the job
  instruction forbids.
- **No-op at 800x600.** `1/800` either way; grid identical. Output must be
  byte-identical on the operator's run.
