# Pending Plan Review v185

- plan: docs/PENDING_PLAN_v185.md
- verdict: KEEP
- reviewer: agent_2_plan_criticer (tick-532)
- timestamp: 2026-08-30

## Design soundness

I re-derived every load-bearing claim from source rather than accepting the
planner's line references, because on a single-profile host the planner and I
are the same model and inherited claims are worthless
(`§Anti-patterns §7`).

**The disagreement is real, and it is arithmetic, not judgement.** Two
independent quantities, read separately:

- dispatch grid: `FReSTIRPass.cpp:491-492` `(outputW + 7)/8` where `outputW =
  Desc.OutputWidth` (`:409`), set to `HalfResWidth` at
  `TestReSTIR_GI_Temporal.cpp:951`, itself `W/2` at `:1538-1541`.
- shader's screen-space constant: `TC.OutputSize = FB.width` at `:961`.

Same pass, same frame, one is half the other. No interpretation is needed.

**Severity check on each of the three consequences.** I agree with the
planner's ordering and confirm its most serious claim independently:

Consequence 3 is the one that matters. `ReSTIR_Temporal_cs.hlsl:170`
`prevPixel = int2(prevUV * outputSize)` with `outputSize = 800x600`, guarded
at `:176` by `prevPixel < int2(outputSize)` — the guard is written against
the *same wrong constant*, so it does not protect the reservoir `Load`s at
`:201-202`, which are 400x300 textures (`TestReSTIR_GI_Temporal.cpp:1557-1570`).
A guard that validates against the wrong bound is worse than no guard,
because it looks correct on inspection. This is a genuine out-of-bounds read
on the history reservoirs for any `prevPixel ≥ 400`, not a stylistic issue.

Consequence 2 I also confirm: `:136` `uv = (float2(pixel) + 0.5f) *
gConstants.RcpOutputSize`, `pixel` from `dispatchThreadID` in [0,400),
`RcpOutputSize = 1/800` (`:963`). uv maxes at ~0.5. Every reprojection is
computed in the wrong half of NDC.

**The positive control is the strongest part of the case.** Spatial, in the
same file 60 lines later, uses `HalfResWidth` at `:1021-1024`. Three call
sites, one updated. That is the signature of an omission, and it mirrors
exactly how v183 used `Resolve_cs.hlsl:60` as its witness. I checked the
spatial shader consumes it consistently (`ReSTIR_Spatial_cs.hlsl:71-72`
early-out, `:113` neighbour bounds) — it does, and at half-res those are
correct.

## Plan completeness

Checked for the failure modes that would make this cycle worthless:

- **Right file compiled?** Yes. `ShaderMake.cfg:5-7` compiles the three
  `ReSTIR_*_cs.hlsl` in `TestReSTIR_GI_Temporal_Data/`. Crucially the fix is
  **C++-only**, so the v182 trap (patching a shader copy nothing compiles)
  cannot apply here at all.
- **Negative controls named?** Yes, both, and I verified both. Spatial
  `:1021` already correct — must not be touched. Cornell
  `TestCornellBoxGI.cpp:1556-1559` uses `CurrentFBInfo.width` and dispatches
  full-res; correct there. A symmetric "fix" would regress two working call
  sites. The plan explicitly forbids this.
- **Is the generation change honestly scoped?** Yes, and I credit the plan
  for not overselling it. I read `ReSTIR_Generate_cs.hlsl` in full (72
  lines): `OutputSize` is used only at `:58-60` for the early-out, and
  `RcpOutputSize` is never read. At half-res dispatch no thread exceeds the
  full-res bound, so today it is genuinely inert. Including it is correct
  hygiene — it is the same latent-trap shape v184 just got bitten by — but
  it will not move a pixel, and the plan says so.
- **`skip_plan_review: no`** is the right call for a production-path change.

## Relationship to v183/v184 — checked, not assumed

These are three distinct defects of one class, in dependency order:

- v183: shaders indexed full-res GBuffer textures with half-res coords → added
  `GB()` + `GBufferScale`.
- v184: `GBufferScale` never arrived (cbuffer packing) → temporal half of
  v183 was inert.
- v185: the coordinate space `GB()` operates in is itself wrong, because
  `OutputSize`/`RcpOutputSize` describe the full-res grid.

v185 does not supersede either. It is required for v183 to be meaningful.
All three must be judged on one run — the plan states this.

## What would falsify this

Named, and it is a real risk, not a formality: if `M mean` does not rise from
2.93 after all three land, the half-res-reuse hypothesis is wrong. The plan
commits to recording that as a refutation. I hold it to that — v184's audit
already documented how close this lineage came to logging a null result
against an untested hypothesis.

## Feedback for planner

None blocking. One instruction carried into implementation: **do not touch
`:1021-1024` (spatial) or `TestCornellBoxGI.cpp`.** Over-application is the
single most likely way this cycle does harm, and both sites currently work.
