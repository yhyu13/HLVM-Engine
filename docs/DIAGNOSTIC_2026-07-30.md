# TestReSTIR_GI_Temporal — interactive diagnostic (2026-07-30)

## Method
Two fresh `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8` runs with the new HLVM_PT_DEBUG_MODE
flags I added (modes 20/21/22 read GBuffer textures from the GI shader's
SRV bindings, bypassing the path-trace logic). Vision + numpy per-pixel
analysis on every dump. Log+grep for errors.

## What I changed this turn
- `Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl`:
  added cases 20u/21u/22u to the debug switch:
  - case 20: `GBufferMaterial.Load(int3(pixel, 0)).rgb`
  - case 21: `GBufferNormal.Load(int3(pixel, 0)).rgb * 0.5 + 0.5`
  - case 22: `GBufferWorldPos.Load(int3(pixel, 0)).rgb * 0.25 + 0.5`
  These read the GI shader's SRV bindings directly, bypassing the path
  trace, so they isolate "is the SRV bound and readable" from "is the
  path trace correct."
- Also fixed the pre-existing `case 8u` shader bug (used `rayDir` and
  `rayOrigin` which are out of scope outside the SPP loop) by re-deriving
  them via `sampleHemisphereCosine(normal, float2(0.5, 0.5))` and
  `OffsetRayOrigin(...)`. Sync'd to the test data dir copy.
- `Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp`:
  replaced the `nvrhi::BindingType::Texture_UAV` (does not exist) with
  `nvrhi::ResourceType::Texture_UAV` (correct enum for this nvrhi fork).
- `Engine/Source/Runtime/Private/Renderer/DeviceManagerVk4_LifeCycle.cpp`:
  stubbed the `nvrhi::validation::createValidationLayer` call to
  `m_ValidationLayer = nullptr;` because that symbol exists in nvrhi's
  source but is not compiled into `libnvrhi_vkd.a`. The validation layer
  is gated off by default; the stub is only linked, never called. This
  unblocks the linker.

## What the new modes show

### `HLVM_PT_DEBUG_MODE=20` (GBufferMaterial SRV read from GI shader)
`gi_raw_frame8.png`: solid black `(0,0,0,255)` for every pixel.
Numpy: per-channel mean = 0,0,0,255; unique values per channel = 1,1,1,1.
The GI shader's `Texture2D<float4> GBufferMaterial : register(t3)` SRV
read returns zero for every pixel.

### `HLVM_PT_DEBUG_MODE=21` (GBufferNormal SRV read)
Same result: solid black `(0,0,0,255)`. The GBufferNormal SRV read
also returns zero.

### `HLVM_PT_DEBUG_MODE=22` (GBufferWorldPos SRV read)
Same result: solid black. GBufferWorldPos SRV read returns zero.

### Compare: gbuffer_material/worldpos/normal direct dumps
- `dumps/gbuffer_material_frame8.png`: 45.5% (255,255,255,255), 54.5%
  (254,254,254,255). All Sponza surfaces written with albedo ≈ 1.0.
- `dumps/gbuffer_worldpos_frame8.png`: real Sponza geometry visible
  (back wall, upper gallery arches, lower floor arches).
- `dumps/gbuffer_normal_frame8.png`: well-formed normals, all surface
  orientations visible.

The textures have real data (per direct CPU staging copy). The GI
shader's SRV reads of those textures return zero. This is a binding
issue, not a rasterization or layout issue.

## Root cause: GI shader's GBuffer SRV bindings are not actually bound

The C++ binding set creation at `Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp:547-572`
builds:
```cpp
SRVBuilder.SetConstantBuffer(0, ...)
          .SetConstantBuffer(1, ...)
          .SetRayTracingAccelStruct(0, ...)
          .SetTextureSRV(1, Desc.GBufferWorldPos)    // t1
          .SetTextureSRV(2, Desc.GBufferNormal)      // t2
          .SetTextureSRV(3, Desc.GBufferMaterial)    // t3
          .SetStructuredBufferSRV(5, Desc.RTVertices)
          .SetStructuredBufferSRV(6, Desc.RTIndices)
          .SetStructuredBufferSRV(7, ActiveLightsBuffer)
          .SetStructuredBufferSRV(8, Desc.RTInstanceInfo)
          .SetSampler(2, Desc.LinearSampler);
```

The shader declares:
```hlsl
Texture2D<float4> GBufferWorldPos : register(t1);
Texture2D<float4> GBufferNormal   : register(t2);
Texture2D<float4> GBufferMaterial : register(t3);
```

The binding layout at lines 285-294 has the same registers. The dispatch
passes both `SRVBindingSet` and `UAVBindingSet`. Everything looks
correct from the C++ side.

But mode 20/21/22 returns zero. So the binding is set up but the
shader's SRV read returns zero. Possible causes (in order of likelihood):

1. **slangc compiled the debug-mode switch wrong** — if slangc dead-stripped
   the case labels (per anti-pattern #7 from `gpu-rendering-bisect-debug`),
   the read might not be reaching the case. But this would be a fresh
   bug introduced by my edit, and I added the cases 20/21/22 with unique
   arithmetic so they shouldn't be dead-stripped. Verify by running
   `spirv-cross --reflect Engine/Source/Runtime/Binary/Debug/shaders/GIPathTracing.spv`
   and checking that the case labels are present.

2. **Image layout transition is wrong** — the binding set requires
   `SHADER_READ_ONLY_OPTIMAL` for the SRV reads, but the textures
   were last transitioned to `ShaderResource` not
   `SHADER_READ_ONLY_OPTIMAL`. Vulkan validation layer would catch this
   and we'd see the VUID-00344 error, but the latest log has no VUIDs
   at all. So either the layout is fine, or nvrhi is doing the right
   transition silently.

3. **The RHI is silently dropping the second binding set** when both
   SRV + UAV are bound. The fix at v22 was specifically for this. But
   the binding set creation succeeds and the dispatch returns normally
   (log shows "FGIPass::DispatchRays EXIT").

4. **The actual textures in `Desc.GBufferWorldPos/Normal/Material` are
   different from the ones the rasterizer wrote to.** If the GBuffer
   pass recreates the textures mid-frame (e.g., on resize), the GI
   pass would have stale handles pointing to old textures. The texture
   is `keepInitialState=true` so recreation is possible. Need to verify
   handle identity between RenderGBuffer and DispatchRays.

## Recommended next step (this is what the next session / cron should do)

The bisect now has only one variable left: are the texture handles
the GI shader sees the same ones the raster pass wrote to?

Cheapest probe: a debug mode that outputs the GI shader's
`GBufferWorldPos[pixel]` value AND compares with a CPU staging read.
The shader-side read is mode 22; the staging read is the existing
`DumpRGBA32FTexture(GBufferWorldPos, ...)` call. If they differ, the
handles are mismatched; if they match, the binding is wrong at the
descriptor level (navigate to option 5 below).

Other bisect paths:

5. **Run `spirv-cross --reflect GIPathTracing.spv`** and confirm the
   shader actually has the SRV bindings at t1/t2/t3. If they're
   missing, the binding layout is wrong.

6. **Add a debug mode that reads `GBufferWorldPos[0,0]` literally**
   (single specific pixel, not the working pixel) and outputs a
   single magenta flag if the value is non-zero. If that mode shows
   magenta, the binding works for some pixels; if it shows zero,
   the binding is universally broken.

7. **Check the `bindingOffsets.constantBufferOffset = 0` in the
   test code's call to `SetBindingOffsets`** — anti-pattern from
   AGENTS.md: "constantBufferOffset defaults to 256, set explicitly
   to 0 for GLSL binding 0." If the constant buffer is offset wrong,
   the shader's `register(t1)` might be reading the wrong descriptor.

8. **Run with Vulkan validation layer enabled** (the `r.Vulkan.Validation`
   CVar or env var) and look for binding mismatch warnings. This
   requires recompiling nvrhi's validation TU, which is the bigger fix
   the cron was attempting.

## Crons (paused, will resume after this diagnostic lands)
- `4d9ef7842c63` HLVM ReSTIR six-role autonomous pipeline (5m): PAUSED
- `f76d8941aaad` HLVM ReSTIR goal-loop watchdog (10m): PAUSED