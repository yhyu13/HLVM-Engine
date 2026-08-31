# Pending Plan v194

- task: Card G — the ReBLUR denoise pass's swapchain-derived extents. Eighth
  instance of the Phase-D/extent class.
- source: no bundle — direct edit
- approach: Replace the four `FB.width`/`FB.height` reads that feed
  `ReBLURConstants.OutputSize` / `RcpOutputSize` and the two that feed
  `ReBLURDesc.OutputWidth` / `OutputHeight` with the file-scope `WIDTH`/`HEIGHT`,
  matching v192/v193. Touches one file
  (`Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp`), one block (5.5).
  `ReBLUR_cs.hlsl` and `FReBLURPass.cpp` byte-unchanged.
- diff_estimate: +6 / -6 functional, plus comment
- skip_plan_review: no
- test_strategy: file-only row table (agent 5), same shape as v193: verify each
  substitution present, verify no over-substitution into the neighbouring blit
  or the history-copy, verify `WIDTH`/`HEIGHT` unshadowed at the patch site,
  verify both shader copies untouched, and control every zero.
- risks: see below.

## The defect

Six swapchain reads in the block, all feeding fixed-size resources:

| Site | Field | Currently |
|---|---|---|
| `ReBLURConstants.OutputSize[0]` | shader `gConstants.OutputSize.x` | `FB.width` |
| `ReBLURConstants.OutputSize[1]` | `.y` | `FB.height` |
| `ReBLURConstants.RcpOutputSize[0]` | shader `texelSize.x` | `1.0f / FB.width` |
| `ReBLURConstants.RcpOutputSize[1]` | `texelSize.y` | `1.0f / FB.height` |
| `ReBLURDesc.OutputWidth` | dispatch grid X | `FB.width` |
| `ReBLURDesc.OutputHeight` | dispatch grid Y | `FB.height` |

Every resource the pass touches is fixed-size:

- `OutputTexture = DenoisedTexture`, created `W`x`H` inside
  `CreateGBufferTextures` where `const uint32_t W = WIDTH, H = HEIGHT;`
- `CurrentRadianceTexture = FullResSpatial`, created `W`x`H` in the same function
- `HistoryTexture = ReBLURHistoryTexture[0]`, created with
  `Desc.width = WIDTH; Desc.height = HEIGHT;` at the initialisation site —
  note this one is written from `WIDTH`/`HEIGHT` *directly*, not via `W`/`H`
- `DepthTexture` / `NormalRoughnessTexture` are the fixed GBuffer MRTs

`BackBufferResizing` clears only the binding cache; no texture is recreated.
So `FB.width` is a variable that merely coincides with the correct value at
startup — the same shape as v191, v192 and v193.

## Card G asked a design question. It does not survive contact with the shader.

Card G says the fix is "a genuine choice between passing `WIDTH`/`HEIGHT`
explicitly and passing nothing to let the `FReBLURPass::Dispatch` fallback run,"
citing the fallback that derives the extent from `Desc.OutputTexture->getDesc()`
when `OutputWidth` is zero. **The fallback cannot fix this pass**, for a reason
the card did not check: it governs only the *dispatch grid*. It does not touch
`FReBLURConstants`, which the caller fills and `Dispatch` marshals verbatim into
`ConstantsData[offset++]` without ever consulting the output texture.

So `OutputSize` / `RcpOutputSize` would stay swapchain-derived under the fallback
route, and those are the two the shader actually reads. Taking the fallback
would fix the grid and leave the kernel's arithmetic wrong — **strictly worse
than either whole option**, because the mismatch would then be internal to one
dispatch. Explicit `WIDTH`/`HEIGHT` at all six sites is the only coherent choice.
Recording it here so the question is closed in the plan, not re-litigated.

## Why the severity is materially higher than card G estimated

Card G rated this *below* card F on the grounds that no gate reads the
`denoised` dump and the pass is skipped under `HLVM_RGI_BYPASS`. Both premises
are true and both are irrelevant to the actual severity, because the pass's
output is **not only** the dump:

    AccumInput = DenoisedTexture;      // end of the ReBLUR branch

`DenoisedTexture` becomes the SRV input to the accumulate pass, which writes
`DisplayTexture` — the artifact `validate_restir_gi.py` checks and the one gate 6
inspects. And `bBypass` is set only from `std::getenv("HLVM_RGI_BYPASS")`, so
under the acceptance recipe the branch is **taken**. This pass sits on the
acceptance path exactly as card F did, one stage upstream. Card G's own
"lower severity" note should be treated as retracted, with this plan as the
reason.

## Three distinct failure modes, one of which is new to this class

`ReBLUR_cs.hlsl` has **no extent guard at all** — no early-out on
`dispatchThreadID` versus any bound (`main` begins at the `texelSize` computation
and proceeds straight to a `gDepth.Load`), and all three `gOutput[...]` stores
are indexed by raw `dispatchThreadID.xy`. So:

1. **Window wider than 800** — unguarded out-of-bounds UAV stores through
   `gOutput`, i.e. undefined behaviour. Same as v192's resolve kernel.
2. **Window narrower than 800** — the trailing region of `DenoisedTexture` is
   never written, and since it is `AccumInput`, that stale region propagates
   into `DisplayTexture` and then into the dump, whose readback is sized from
   the texture descriptor.
3. **New to this class, and the reason this is not a mechanical repeat:**
   `RcpOutputSize` is not merely a bound, it is a *scale*. It produces
   `pixelUv = (dispatchThreadID.xy + 0.5) * texelSize`, whose only consumer is
   `IsHistoryValid(pixelUv)` — `all(uv > 0) && all(uv < 1)`. A too-small
   reciprocal (wider window) pushes the right/bottom of the image to `uv >= 1`,
   so `historyValid` goes false there and `TemporalAccumulation` returns
   `current` unblended: **the denoiser silently degrades to a pass-through in a
   band**, with no VUID, no error, and no out-of-bounds access to detect. Every
   prior instance in this class corrupted an *index*; this one corrupts a
   *weight*. Worth recording as its own variant.

## Risks

- **Over-substitution.** `FB.width`/`FB.height` are correct in the neighbouring
  blit block and must not be touched. The impler must confirm the blit still
  reads `FB.` afterwards.
- **Shadowing.** `WIDTH`/`HEIGHT` must be the file-scope constants at the patch
  site. The block is inside `Render()`; the three `const uint32_t W = WIDTH, H =
  HEIGHT;` declarations live in other member functions. Verify, do not assume.
- **Type.** `OutputSize`/`RcpOutputSize` are `float`; the existing
  `static_cast<float>(...)` must be preserved around `WIDTH`. `OutputWidth`/
  `OutputHeight` are `uint32_t`, matching `WIDTH` exactly.
- **Dual-copy hazard (v182).** `ReBLUR_cs.hlsl` exists in both
  `TestReSTIR_GI_Temporal_Data/` and `TestCornellBoxGI_Data/`. This cycle edits
  neither; the impler must confirm that rather than assume it.
- **No-op at 800x600**, so this cannot perturb the v183-v193 chain awaiting an
  operator run. Stated deliberately, as in v191/v192/v193.

## Acceptance-gate honesty

Source-decidable; does not need a build. Nothing in this cycle can be compiled,
run or viewed on this runspace — `terminal` was probed twice this tick and
denied categorically (`tirith:unknown`), including a bare `/bin/true`.
