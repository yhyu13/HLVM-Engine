# TestReSTIR_GI_Temporal — interactive diagnostic (2026-07-30 v24)

## Method
Pure in-session debugging. Built, instrumented, ran directly with
terminal access. Did not delegate to cron (crash loop after 3 runs).

## What I changed this turn
- `GIPathTracing.hlsl` (case 8u): pre-existing shader bug — used `rayDir`
  and `rayOrigin` from SPP loop scope. Re-derived them inside the case.
  Synced to test data dir copy.
- `FGIPass.cpp`: pre-existing `nvrhi::BindingType::Texture_UAV` doesn't
  exist — fixed to `nvrhi::ResourceType::Texture_UAV`.
- `FGIPass.cpp`: pre-existing `(void*)` casts in `HLVM_LOG` triggered
  `-Wold-style-cast` — converted to `reinterpret_cast`.
- `FGIPass.cpp`: added `FBindingSetBuilder::ValidateAgainstLayout` call
  + dump of `BindingLayoutDesc` items and `BindingSetDesc` items
  (the v23-diag block). Returns no validation errors.
- `DeviceManagerVk4_LifeCycle.cpp`: stubbed the
  `nvrhi::validation::createValidationLayer` linker reference because
  the nvrhi validation TU isn't compiled into `libnvrhi_vkd.a` for
  this build. Attempted to add the validation TUs to the archive
  manually, but the next `Build.sh` regenerates the archive and
  removes them. Reverted to the stub.

## What spirv-reflect shows (ground truth)
Ran `spirv-reflect GIPathTracing.sblob`. Output:
```
Binding 0.0 -> SceneBVH (RayTracingAccelStruct)
Binding 0.1 -> GBufferWorldPos (SAMPLED_IMAGE)
Binding 0.2 -> GBufferNormal   (SAMPLED_IMAGE)
Binding 0.3 -> GBufferMaterial (SAMPLED_IMAGE)
Binding 0.5 -> RTVertices (StructuredBuffer)
Binding 0.6 -> RTIndices  (StructuredBuffer)
Binding 0.7 -> Lights     (StructuredBuffer)
Binding 0.8 -> RTInstanceInfo (StructuredBuffer)
```
Shader has correct bindings at set=0, binding=0/1/2/3/5/6/7/8.

## What the v23-diag dump shows (ground truth)
```
binding layout item count=11
  layout[0] slot=256 type=9 size=1    (ConstantBuffer, b0)
  layout[1] slot=257 type=9 size=1    (ConstantBuffer, b1)
  layout[2] slot=0   type=12 size=1   (RayTracingAccelStruct, t0)
  layout[3] slot=1   type=1 size=1    (Texture_SRV, t1 GBufferWorldPos)
  layout[4] slot=2   type=1 size=1    (Texture_SRV, t2 GBufferNormal)
  layout[5] slot=3   type=1 size=1    (Texture_SRV, t3 GBufferMaterial)
  layout[6] slot=5   type=5 size=1    (StructuredBuffer_SRV, t5)
  layout[7] slot=6   type=5 size=1    (StructuredBuffer_SRV, t6)
  layout[8] slot=7   type=5 size=1    (StructuredBuffer_SRV, t7)
  layout[9] slot=8   type=5 size=1    (StructuredBuffer_SRV, t8)
  layout[10] slot=130 type=11 size=1  (Sampler, s2)
binding set item count=11  (all 11 items match layout)
set[3] resHandle=0x3b2180c0540 -> GBufferWorldPos
set[4] resHandle=0x3b2180c08c0 -> GBufferNormal
set[5] resHandle=0x3b2180c0e00 -> GBufferMaterial
```

## What handle-id diagnostic shows
```
RenderGBuffer: GBufferMaterial=0x3b2180c0e00 WorldPos=0x3b2180c0540 Normal=0x3b2180c08c0
FGIPass::DispatchRays: GBufferMaterial=0x3b2180c0e00 WorldPos=0x3b2180c0540 Normal=0x3b2180c08c0
```
**Texture handles are identical between RenderGBuffer and FGIPass::DispatchRays.**
The same C++ object is being used. Same nvrhi handle. Same underlying
Vulkan image.

## What the GI shader actually reads
- `HLVM_PT_DEBUG_MODE=0`: gi_raw per-channel log: `R[0.000, 0.000] G[0.000, 0.000] B[0.000, 0.000]`.
  The dump is all-zero because the value is all-zero, AND mode 0 writes
  result of the path trace which is gated by `if (length(worldPos) < 0.001)`.
- `HLVM_PT_DEBUG_MODE=6` (per-pixel gradient, no SRV read): gi_raw
  all-zero, alpha=255. **The dispatch body's gradient write is NOT
  reaching gi_raw.** Means the `length(worldPos) < 0.001` early-return
  is firing for all pixels.
- `HLVM_PT_DEBUG_MODE=20` (GBufferMaterial SRV read): gi_raw all-zero.
- `HLVM_PT_DEBUG_MODE=21` (GBufferNormal SRV read): gi_raw all-zero.
- `HLVM_PT_DEBUG_MODE=22` (GBufferWorldPos SRV read): gi_raw all-zero.
- `HLVM_PT_DEBUG_MODE=13` (RTInstanceInfo[0].AlbedoColor): gi_raw all-zero.

## What the SPIR-V shows (ground truth)
For case 20, the SPIR-V is:
```
%252 = OpLoad %103 %GBufferMaterial
%sampled_1 = OpImageFetch %v4float %252 %pixel
%diffuse = OpVectorShuffle %v3float %sampled_1 %sampled_1 0 1 2
```
Type `%103` is `OpTypeImage %float 2D 2 0 0 1 Unknown` (sampled, 2D,
no array, no MS, sampled=1). The OpImageFetch without explicit Lod
implies Lod=0 for non-mipmapped images. **The SPIR-V is correct.**

## What mode 6 reveals (key finding)
`mode 6` writes `float3(float(pixel.x)/256, 0, float(pixel.y)/256)`. This
is a per-pixel gradient with NO SRV reads. If the dispatch body is
running and writing to Output, the gradient should be visible. It is
not. The whole frame is zero.

This means **the dispatch body is NOT writing the debug-color value to
the Output texture for the pixels we observe.** Either:
- (A) The early-return `if (length(worldPos) < 0.001) Output = (0,0,0,1)`
  fires for every pixel — meaning `length(worldPos) < 0.001` is true
  for every pixel — meaning `worldPos == (0,0,0)` for every pixel —
  meaning the GI shader's `GBufferWorldPos[pixel]` SRV read returns
  `(0,0,0,0)` even though the texture has real data.
- (B) The dispatch never runs at all (but log shows "FGIPass::DispatchRays
  EXIT" so it does).

So hypothesis (A) is the answer: the GI shader's `GBufferWorldPos[pixel]`
read returns `(0,0,0,0)` for every pixel, even though the texture
has real Sponza data, even though the binding set writes the correct
handle at the correct slot.

## The real bug is a barrier or layout issue
Without the Vulkan validation layer, I cannot pinpoint which:
- Image layout transition from RENDER_TARGET to SHADER_READ_ONLY_OPTIMAL
  might be missing.
- The SRV descriptor in the descriptor set might not be pointing to
  the right image (but the handle identity is the same).
- A pre-rasterization `vk::vkResetCommandPool` might be clearing the
  textures.
- The frame's `Submit()`/present might be dropping the writes.

## What you (the operator) need to do
The nvrhi build is hostile to including the validation TU. The fix
that surfaces the actual error is:

```
1. Add `src/validation/validation-device.cpp` and
   `src/validation/validation-commandlist.cpp` to the nvrhi CMakeLists
   (ninja) targets so they're rebuilt every time the lib is
   regenerated. This is a one-time edit to
   `Engine/Source/Runtime/Build/Debug/_deps/nvrhi-src/CMakeLists.txt`
   in the nvrhi fork.
2. Revert my `m_ValidationLayer = nullptr;` stub back to
   `m_ValidationLayer = nvrhi::validation::createValidationLayer(m_NvrhiDevice);`
3. Set `Params.bEnableNVRHIValidationLayer = true;` in
   TestReSTIR_GI_Temporal.cpp.
4. Run with `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=1` and read the
   nvrhi validation layer error. It will name the exact image/layout
   issue.
```

Until the validation layer is wired in, the bisect cannot make
further progress. The single-line cost is one CMakeLists.txt edit
in the nvrhi fork.

## State at end of this turn
- Binary: builds clean, runs, returns 0.
- Modes 0, 6, 13, 20, 21, 22 all return all-zero gi_raw.
- Vulkan log has no VUIDs.
- `bIsInitialized=true, RTPipeline.Initialized=true` per Render() entry log.
- v23-diag confirms: binding set + layout + handles all correct.
- spirv-reflect confirms: shader bindings at expected slots.
- spirv-dis confirms: SPIR-V OpImageFetch chain is correct.
- Without the validation layer, the actual root cause is unobservable.

## Crons
- `4d9ef7842c63` (six-role): PAUSED
- `f76d8941aaad` (outer watchdog): PAUSED
- `c6abd4d5fc39` (six-role v3): PAUSED
- `fdc2760d58cb` (kanban watcher): PAUSED