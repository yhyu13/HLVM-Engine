# Pending Impl Review v195

- plan: docs/PENDING_PLAN_v195.md
- commit: docs/PENDING_COMMIT_v195.md
- verdict: KEEP
- reviewer: agent_4_reviewer (tick-541)
- timestamp: 2026-08-30

## plan_fidelity_check

The impl matches the plan exactly: two functional substitutions at the two
planned sites, no third site, no shader edit. `## Plan Deviations` declares
none, and I verified that independently — `UpdateViewConstants(` returns exactly
2 hits (the call at `:787`, the definition at `:2427`), so there is no second
call site the plan could have missed, and the raster viewport is the only
`nvrhi::Viewport` construction in the file's GBuffer path.

## Independent re-derivation

**The `gbScale` chain, end to end.** I did not accept the plan's version:

- numerator: `VC.Size = {float(W), float(H)}` → `ViewConstantsBuffer` →
  `ViewConstants.RenderTargetSize` (`GIPathTracing.hlsl:76`, 4th member after
  three `float4x4`, matching the C++ `FVC` member order).
- denominator: `DispatchRaysDimensions()`, set by
  `RTPipeline.DispatchRays(CmdList, Desc.OutputWidth, Desc.OutputHeight, ...)`
  at `FGIPass.cpp:741`, from `Desc.OutputWidth = HalfResWidth` (`:793`),
  assigned `W / 2` at `:1649` where `const uint32_t W = WIDTH` (`:1617`).
- consumer: `:498-499` forms `gbPixel`, used at `:501-503` for all three
  GBuffer reads.

Variable over fixed. The defect is real and this is the **ninth** instance of
the class — but the first that reaches the GPU through a *constant buffer* and
a *shader-side division* rather than a C++-side dispatch argument. Every prior
instance was visible by reading one C++ statement; this one required crossing
into HLSL to see. That is why eight sweeps missed it, and it is the strongest
argument yet for card I being done properly rather than by re-grepping
`FB.width`.

**The corrected shadowing argument is right, and the correction matters.** v194
carried forward "positional ordering is grep-checkable, prefer it." The impler
noticed that rule **fails here** — all three `W`/`H` declarations (1617, 1743,
1836) *precede* the `RenderGBuffer` patch at `:2356`, so positional ordering
would have wrongly suggested shadowing. The containment argument is correct:
`RenderGBuffer` opens at `:2165` and the nearest preceding declaration is at
`:1836`, outside it. Catching that a carried-forward rule was non-general, one
cycle after it was promoted, is the best gate catch in this lineage's recent
history.

**The `LastWidth` non-substitution is the most important judgement in the
cycle.** I checked the counterfactual: substituting the variable makes `:754`
read `if (FB.width != WIDTH || ...)`, which no longer detects a resize *back to*
800x600 and, worse, fires permanently at any other size, clearing the binding
cache every frame. Both directions are bugs. The impler changed the use and not
the variable, and said so. **This also falsifies card I's central proposal in
advance**: an `HLVM_ENSURE(FB.width == WIDTH)` at the top of `Render()` would
abort on an ordinary user resize that the cache-clear path exists to handle
gracefully. Card I should be re-scoped from "assert the invariant" to "assert
the invariant *for the fixed-extent passes*, which is not the same predicate."

## Severity, and a correction to the class's standing narrative

Prior cycles recorded this class as "latent — not exercised by the recipe." For
this instance that is **only half true**. The recipe runs at 800x600, where
`gbScale == 2.0` either way, so the acceptance run is unaffected — correct. But
the failure mode under a resize is worse than any prior instance:

- widened: `gbPixel` exceeds the fixed GBuffer extent, `Load` returns 0,
  `length(worldPos) < 0.001` at `:518` is satisfied, and the pixel is shaded as
  **sky** using the same corrupted `RenderTargetSize` at `:525-526`.
- narrowed: the tracer samples only a sub-rect of the GBuffer, magnifying it.

Neither raises a VUID, neither leaves a hole, and both produce an image that
looks *deliberate*. Every prior instance produced either an obvious black band,
stale rows, or an OOB store. **This one produces a plausible wrong image**,
which is the hardest failure to catch by eye — precisely the trap
`software-development-practices §4-check structural validator` warns about.

## Security scan

- [x] No hardcoded secrets
- [x] No shell injection
- [x] No eval/exec
- [x] No SQL injection

## Self-review checklist

- [x] Validation: no input parsing changed
- [x] Error handling: unchanged; no new failure paths
- [x] Tests: `produces_test_files: no`, so HARD INVARIANT #2 is not engaged;
      the reviewer ran anyway (`skip_impl_review: no`)

## NOT established

That the file compiles or that any pixel changes. `./Build.sh` was refused
(`tirith:unknown`) at a third distinct invocation shape this tick. **No build,
no run, no image.** The patch is a two-token substitution of a file-scope
`static const uint32_t` for a struct member of the same type in a
`float()`-cast and a `uint32_t` parameter position, so it is type-compatible by
inspection — but inspection is not compilation.

## Cards

- **H** — closed by this cycle. Its factual premise ("the callee uses its two
  parameters for exactly one thing") was **wrong**, not merely its judgement.
- **I** — re-scoped by this cycle's finding (see above); the blanket
  `HLVM_ENSURE(FB.width == WIDTH)` it proposes is unsafe as stated.
- **J** — opened below.

### NEW card J — the sibling `TestPathTraceGI.cpp` has the same shape, unswept

`TestPathTraceGI.cpp:857-858` sets `Constants.RenderTargetSize[0/1] = float(W)`
and `:422` calls `UpdateViewConstants(CurrentFBInfo.width, CurrentFBInfo.height)`
— swapchain-derived, same as the bug just fixed. **Whether it is a defect
depends on whether that test's RT dispatch extent is independently fixed**, which
I did not determine and must not assume: if its render target and dispatch both
follow the swapchain, the ratio stays consistent and there is no bug. This is
exactly the "read the callee before deferring" discipline the last three cards
failed. Source-decidable; does not need a build. Deliberately not bundled —
different target, and bundling would make v195's own single-file enumeration
unverifiable.
