# Pending Plan v183

- task: ReSTIR temporal/spatial passes read FULL-RES depth/normal with HALF-RES coordinates (Phase-D scaling gap)
- source: no bundle — direct edit, found by first-hand source audit this tick
- planner: agent_1_planner (tick-530)
- timestamp: 2026-08-30

## Why this card exists (and why it is NOT the refuted v182 card)

v182 closed the *instrumentation* bug: debug probes 20/21/22/31 indexed the
GBuffer in dispatch space while production reads used `gbPixel`. That patch is
confined to `#ifdef HLVM_RGI_DEBUG_VIS` and cannot change a production pixel.

This card is the same CLASS of bug found in the **production** path, in
different files, and it has never been raised in 529 ticks of lineage.

## The defect

Phase D (`PLAN_REALTIME_RESTIR_GAP`) made ReSTIR trace and reuse at HALF
resolution. Verified on disk this tick:

- `TestReSTIR_GI_Temporal.cpp:1529-1531` — `HalfW = W/2`, `HalfH = H/2`.
- `:1544-1563` — every reservoir + `SpatialRadiance` is `HalfW x HalfH`.
- `:951-952` (temporal) and `:1011-1012` (spatial) dispatch at
  `HalfResWidth/HalfResHeight`, and `SC.OutputSize` (`:1015-1016`) is half-res.

But the depth/normal textures those passes consume are FULL-RES:

- `:1513-1515` — `LinearDepthTexture` created at `W x H` (it is a GBuffer MRT).
- `GBufferNormal` is likewise a full-res GBuffer MRT.
- `:946-949` — `Td.DepthTexture / NormalTexture / PrevDepthTexture /
  PrevNormalTexture` are all set to those full-res textures.
- `:1008-1009` — `Sd.NormalTexture / DepthTexture` likewise.

And both shaders index them with the half-res dispatch coordinate directly:

- `ReSTIR_Temporal_cs.hlsl:114` `gDepth.Load(int3(pixel,0))`,
  `:157-159` `gPrevDepth/gPrevNormals/gNormals.Load(int3(prevPixel|pixel,0))`.
- `ReSTIR_Spatial_cs.hlsl:68-69` `gNormals/gDepth.Load(int3(pixel,0))`,
  `:104-105` the same for `nPixel`.

At 800x600 GBuffer / 400x300 dispatch the scale factor is exactly 2, so both
passes validate history and neighbours against the **top-left quadrant** of the
depth/normal buffers — geometrically unrelated to the pixel being shaded.

## Proof this is a real asymmetry, not intended design

`Resolve_cs.hlsl:60` — the one Phase-D-aware pass — explicitly converts before
sampling the same full-res buffers:

```hlsl
int2 fp = hp * 2 + 1;                       // half-res texel -> full-res center
float sDepth = FullResDepth.Load(int3(fp, 0)).r;
```

The resolve author knew the conversion was required. The temporal and spatial
passes were never updated when Phase D landed. Same conclusion from the C++
side: `:1042-1043` passes `RcpFullW/RcpFullH` to the resolve, while the temporal
pass gets only `RcpOutputSize` (half-res) — it has no way to address full-res.

## Predicted symptom, and why it matches the observed evidence

Depth/normal validation compares unrelated texels, so `historyValid`
(`ReSTIR_Temporal_cs.hlsl:164`) and the spatial neighbour tests
(`:107`, `:113`) reject nearly everything. Temporal reuse degenerates toward
M≈1 and spatial reuse toward pass-through.

The freshest log records `ReSTIR summary: M mean=2.93 max=9.0 (MaxM=30)`. That
is the fingerprint: not zero (some texels coincidentally agree near the top-left
quadrant), but ~10% of the configured cap. Prior ticks read `M=2.93` as
"accumulation works"; it is better read as "accumulation is mostly rejected."

## Approach (REVISED after plan-review v183 FIX — all 4 items adopted)

Introduce an explicit half-res→full-res conversion in both shaders, matching the
`Resolve_cs.hlsl:60` centre-of-texel convention (`hp*2+1` at scale 2), driven by
a `GBufferScale` constant from C++ rather than a hardcoded `2`.

**ZERO-LAYOUT-DELTA MANDATE (plan-review item 1).** No struct grows. The
constant reuses padding that already exists in both mirrors:

| Struct | Slot reused | Evidence it is free |
|---|---|---|
| `FReSTIRTemporalConstants` | `Pad[2]` → `GBufferScale` | `FReSTIRPass.h:44` `Pad[3]`; `.cpp:976-977` uses only `Pad[0]`/`Pad[1]` (near/far) |
| `FReSTIRSpatialConstants` | `Pad[0]` → `GBufferScale` | `FReSTIRPass.h:56` `Pad[2]`; `.cpp:1014-1020` sets no `Pad` |

Adding a new field is FORBIDDEN in this cycle — it would reintroduce the
C++/HLSL struct-mismatch class the padding reuse eliminates. HLSL mirrors
(`ReSTIR_Temporal_cs.hlsl:35`, `ReSTIR_Spatial_cs.hlsl:25`) must be renamed in
step so both sides agree field-for-field.

**CONVERSION INVARIANT (plan-review item 2).** Convert ONLY when sampling a
texture that is full-res. Everything else stays in half-res space.

Convert (full-res sources):
- `ReSTIR_Temporal_cs.hlsl` — `:114` `gDepth`, `:157` `gPrevDepth`,
  `:158` `gPrevNormals`, `:159` `gNormals`
- `ReSTIR_Spatial_cs.hlsl` — `:68` `gNormals`, `:69` `gDepth`,
  `:104` `gNormals`, `:105` `gDepth`

Do NOT convert (genuinely half-res — changing these breaks reprojection):
- `:102-103` `gCurrReservoir0/1`, and all `gHistReservoir` / `gRadiance` /
  `gReservoir0/1` loads
- `:115` `uv` (derived from `RcpOutputSize`, half-res by construction)
- `:149` `prevPixel` and every bounds check against `outputSize`
  (`:155`, spatial `:101`)

**GUARD (plan-review item 3).** The helper clamps the scale so an unset
constant degrades to today's behaviour instead of collapsing every read to
texel (0,0):

```hlsl
int2 GB(int2 p)
{
    int s = max(int(gConstants.GBufferScale), 1);
    return p * s + (s >> 1);   // s==2 -> p*2+1, identical to Resolve_cs.hlsl:60
}
```

Files:

1. `.../TestReSTIR_GI_Temporal_Data/ReSTIR_Temporal_cs.hlsl` — rename `Pad[2]`
   slot, add `GB()`, apply at the 4 sites above.
2. `.../TestReSTIR_GI_Temporal_Data/ReSTIR_Spatial_cs.hlsl` — rename `Pad.x`,
   add `GB()`, apply at the 4 sites above.
3. `.../Public/Renderer/PostProcess/FReSTIRPass.h` — rename the two padding
   slots. Size unchanged.
4. `.../Test/TestReSTIR_GI_Temporal.cpp` — set both from
   `FB.width / HalfResWidth`.

- diff_estimate: +34 / -12
- skip_plan_review: no (this revision answers the FIX)
- produces_test_files: no

## test_strategy

File-only structural verifier (the only executable thing in this runspace):
(1) each of the 8 full-res `.Load` sites goes through `GB()`;
(2) no reservoir/radiance load is wrapped;
(3) `uv`/`prevPixel`/bounds remain half-res;
(4) the `max(..,1)` guard is present in both copies;
(5) struct sizes are unchanged on both sides and field order matches;
(6) `TestCornellBoxGI_Data/` copies are untouched.

## risks

- **Cannot be runtime-verified from this job.** No shell (tirith), no vision
  (tick-528). A build+run is required before claiming ANY acceptance gate.
  No later tick may report gate 5/6/7 PASS without a post-2026-08-30 log.
- Struct sizes are now unchanged by construction, so the alignment risk noted
  in the first draft is retired — but C++ and HLSL field ORDER must still be
  verified field-for-field (verifier row 5).
- `TestCornellBoxGI_Data/` has its own copies of both shaders. Explicitly OUT
  of scope: that test has no Phase-D half-res path, so converting there would
  break a currently-passing test.
