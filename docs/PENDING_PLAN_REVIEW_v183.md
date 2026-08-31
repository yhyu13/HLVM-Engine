# Pending Plan Review v183

- plan: docs/PENDING_PLAN_v183.md
- verdict: KEEP
- reviewer: agent_2_plan_criticer (tick-530)
- timestamp: 2026-08-30
- note: first pass was FIX; the planner's revision adopts all four items
  (zero-layout-delta via existing padding, explicit convert/do-not-convert
  lists, `max(scale,1)` guard, `Resolve_cs.hlsl:60`-matching form). Re-read
  the revised plan and verified each item is present and specific. KEEP.

## Design soundness

The defect is real and independently re-verified by this reviewer against the
files themselves, not against the planner's summary:

- Half-res reservoirs: `TestReSTIR_GI_Temporal.cpp:1529-1531`, `:1544-1563`.
- Full-res depth/normal: `:1513-1515` (`LinearDepthTexture` at `W x H`);
  `GBufferNormal` is a GBuffer MRT at full res.
- Cross-wiring: `:946-949` and `:1008-1009` hand those full-res handles to
  passes dispatched at half res (`:951-952`, `:1011-1012`).
- Unconverted indexing: `ReSTIR_Temporal_cs.hlsl:114,157,158,159` and
  `ReSTIR_Spatial_cs.hlsl:68,69,104,105`.

The `Resolve_cs.hlsl:60` (`int2 fp = hp * 2 + 1;`) contrast is the strongest
part of the argument: the same codebase, consuming the same two full-res
textures from a half-res grid, performs the conversion explicitly. That makes
the omission in temporal/spatial an inconsistency within the project's own
conventions rather than a reviewer's preference.

Acceptance criteria are testable at the file level, and the planner correctly
refuses to claim any runtime gate. The `M mean=2.93 (MaxM=30)` reading is
plausible as corroboration but is explicitly labelled as interpretation, not
proof — appropriate.

Scope exclusion of `TestCornellBoxGI_Data/` is correct: that test has no
Phase-D half-res path, so its copies must not be touched.

## Plan completeness — why FIX rather than KEEP

Three defects in the plan, one of them load-bearing:

**(1) The struct-layout risk is mis-stated, and the safe option was missed.**
The plan says "struct layout change must keep the cbuffer 16-byte aligned,"
implying new fields. No new field is needed. Both mirrors already carry unused
padding:

- `FReSTIRPass.h:44` `TFP32 Pad[3]` — and `TestReSTIR_GI_Temporal.cpp:976-977`
  already repurposes `Pad[0]`/`Pad[1]` as near/far, leaving **`Pad[2]` free**.
- `FReSTIRPass.h:56` `TFP32 Pad[2]` (spatial) — **both entries free**;
  `:1014-1020` sets no `Pad`.

Reusing existing padding makes the change **zero-layout-delta**, which removes
the entire class of C++/HLSL struct-mismatch failure. The plan must mandate
this and forbid adding fields. Note the HLSL mirrors must be updated in step
(`ReSTIR_Temporal_cs.hlsl:35`, `ReSTIR_Spatial_cs.hlsl` equivalent) since a
named field replaces a `Pad` slot on the HLSL side.

**(2) A required call site is missing from the file list.** The plan lists
`:114,157,158,159` for temporal, but `:159`
(`gNormals.Load(int3(pixel,0))` — current-frame normal) sits inside the same
`historyValid` block as `:157-158`. More importantly the plan does not state
what must NOT change: `:102-103` (`gCurrReservoir0/1`), `:115`
(`uv` from `RcpOutputSize`), `:149` (`prevPixel` from `outputSize`) are all
genuinely half-res and must stay in half-res space. The conversion must be
applied at the point of sampling a full-res texture, never to the reservoir
grid itself. Without this stated explicitly an implementer can plausibly
"fix" `:115`/`:149` and break reprojection.

**(3) Zero-default hazard.** If the new constant is left at its default `0`
for any code path, `GB(p)` collapses to `int2(0,0)` and every pass reads
texel (0,0) — strictly worse than today's bug and it would look like a
catastrophic regression. The shader must clamp the scale to `>= 1`.

## Feedback for planner (FIX)

- Reuse existing padding; **do not** change any struct size. Temporal: use
  `Pad[2]` (rename to `GBufferScale`). Spatial: use `Pad[0]`
  (rename to `GBufferScale`), keep `Pad[1]`. Update the HLSL mirrors to match.
- State the invariant explicitly: **convert only when sampling a full-res
  texture** (`gDepth`, `gNormals`, `gPrevDepth`, `gPrevNormals`). Reservoir,
  radiance, `uv`, `prevPixel`, and all bounds checks stay in half-res space.
- Require a `max(scale, 1.0)` guard in the helper so an unset constant
  degrades to today's behaviour rather than to a single-texel read.
- Use the `p * scale + scale/2` centre-of-texel form so the result matches
  `Resolve_cs.hlsl:60` exactly at scale 2 (`hp*2+1`).
- Keep the "no runtime gate may be claimed" note verbatim in the commit.
