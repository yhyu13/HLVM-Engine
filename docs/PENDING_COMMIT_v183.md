# Pending Commit v183

- plan: docs/PENDING_PLAN_v183.md
- plan_review: docs/PENDING_PLAN_REVIEW_v183.md (FIX → revised → KEEP)
- impler: agent_3_impler (tick-530)
- files: Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/ReSTIR_Temporal_cs.hlsl,
  Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/ReSTIR_Spatial_cs.hlsl,
  Engine/Source/Runtime/Public/Renderer/PostProcess/FReSTIRPass.h,
  Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp
- source: no bundle — direct edit
- target: (uncommitted working tree; this job does not commit or push)
- task: convert half-res ReSTIR dispatch coords to full-res GBuffer texels
  before sampling depth/normal in the temporal and spatial reuse passes
- verify: ./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Rebuild
  && cd Engine/Source/Runtime/Binary/Debug
  && HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal
- skip_impl_review: no
- produces_test_files: no

## Change applied (8 conversion sites + 1 helper per shader + 2 C++ writes)

**`ReSTIR_Temporal_cs.hlsl`**
- `:35` `float Pad[3]` → `float Pad[2]` + `float GBufferScale`
  (size unchanged: 3 floats before, 3 after).
- `:61-75` new `int2 GB(int2 p)` helper:
  `int s = max(int(gConstants.GBufferScale), 1); return p * s + (s >> 1);`
- Converted: `:129` `gDepth`, `:172` `gPrevDepth`, `:173` `gPrevNormals`,
  `:174` `gNormals`.

**`ReSTIR_Spatial_cs.hlsl`**
- `:25` `float2 Pad` → `float GBufferScale` + `float Pad` (size unchanged).
- `:45-56` same `GB()` helper.
- Converted: `:80` `gNormals`, `:81` `gDepth` (centre);
  `:116` `gNormals`, `:117` `gDepth` (neighbour).

**`FReSTIRPass.h`**
- `:44` `TFP32 Pad[3]` → `TFP32 Pad[2]` + `TFP32 GBufferScale`.
- `:57` `TFP32 Pad[2]` → `TFP32 GBufferScale` + `TFP32 Pad`.
- Both structs are byte-identical in size to before; only names changed.

**`TestReSTIR_GI_Temporal.cpp`**
- `:983` `TC.GBufferScale = float(FB.width / std::max(HalfResWidth, 1u));`
- `:1029` `SC.GBufferScale = ...` (same expression).
- `std::max(..., 1u)` matches the existing idiom at `:432`; no new include.

## Invariants honoured (from plan-review v183)

- **Zero layout delta.** No struct grew; only reserved padding was renamed.
  This removes the C++/HLSL size-mismatch failure mode entirely.
- **Convert only full-res sources.** Left deliberately unconverted:
  `gCurrReservoir0/1`, `gHistReservoir0/1`, `gRadiance`, `gReservoir0/1`
  (all half-res); `uv` from `RcpOutputSize`; `prevPixel`; every bounds check
  against `outputSize`.
- **Degradation guard.** `max(scale,1)` means an unset constant reproduces
  today's behaviour instead of collapsing all reads to texel (0,0).
- **Convention match.** At scale 2, `p*2 + 1` — byte-identical to the
  `int2 fp = hp * 2 + 1;` already in `Resolve_cs.hlsl:60`.
- **Scope.** `TestCornellBoxGI_Data/` copies of both shaders untouched
  (that test has no Phase-D half-res path).

## Plan Deviations

**One deviation, discovered during post-report verification and applied.**

The plan assumed setting `TC.GBufferScale` / `SC.GBufferScale` in the test
was sufficient to reach the GPU. It is not. `FReSTIRPass.cpp` marshals these
cbuffers **field-by-field into a flat `float ConstantsData[64]`** — it does
NOT `memcpy` the struct (`:424-444` temporal, `:518-532` spatial). Adding a
field to the header therefore has no effect on its own.

As written, the v183 patch was **inert**: `GBufferScale` stayed 0, `GB()`
clamped it to 1 via the `max(...,1)` guard, and every read reproduced the
old (buggy) behaviour. The guard is why this failed safe rather than
collapsing to texel (0,0) — but it also would have masked the defect as
"fix applied, no change observed."

Fixed by appending the missing fields to both marshallers:
- `FReSTIRPass.cpp:443-450` — `Pad[0]`, `Pad[1]`, `GBufferScale`
- `FReSTIRPass.cpp:540-543` — `GBufferScale`

**Pre-existing bug found in passing (NOT introduced by v183):** temporal
marshalling stopped at `PrevSceneYaw`, so `Pad[0]`/`Pad[1]` — the near/far
planes written at `TestReSTIR_GI_Temporal.cpp:976-977` and read at
`ReSTIR_Temporal_cs.hlsl:140-141` — **never reached the GPU**. The shader
computed `ndcZ` with `near = far = 0`, making the "exact NDC z" reconstruction
degenerate. That reconstruction's own comment says an approximate `ndcZ`
causes the reprojected pixel to drift and "the depth/normal validation rejects
every history sample (M never accumulates)." This is a second, independent
cause of the low `M mean=2.93`, and it is now also fixed.

Sibling-test safety: `TestCornellBoxGI` shares `FReSTIRPass.cpp`. Its shader
copies still declare `float Pad[3]` / `float2 Pad` at exactly the offsets the
new writes target, and never read them, so the extra floats land in declared
padding. Buffer capacity is unaffected: `ConstantsData[64]` (256 B) against a
temporal high-water mark of 46 floats and a 256-byte cbuffer (`:317`).

## Notes for the reviewer

- Verify the two `Pad` renames did not change field ORDER between the C++
  and HLSL mirrors — that is the one way a zero-size-delta rename can still
  break (temporal: `GBufferScale` must come AFTER both `Pad` entries; spatial:
  BEFORE the remaining `Pad`).
- `HalfResWidth` is assigned in `CreateGBufferTextures` (`:1531`) which runs
  before any dispatch, so it is non-zero at both write sites; the
  `std::max(...,1u)` is belt-and-braces against a divide-by-zero on an early
  or resized frame.
- **No runtime verification was possible.** No shell (tirith denies every
  `terminal` call, including `pwd`), no vision tool in this runspace. This
  commit is file-only. No acceptance gate may be reported as PASS on the
  strength of this patch until a post-2026-08-30 build+run log exists.
