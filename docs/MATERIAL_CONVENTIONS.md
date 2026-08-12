# Material Conventions — HLVM Engine (2026-08-10)

Written during the TestReSTIR_GI_Temporal material rework
(`Vibe_Coding/50_ReSTIR_GI_Temporal/PLAN_MATERIAL_REWORK_2026-08-10.md`).

## GBuffer material MRT packing (TestReSTIR_GI_Temporal GBuffer pass)

| MRT | Format | Contents |
|-----|--------|----------|
| MRT0 | RGBA32F | World position (xyz, 1.0) |
| MRT1 | RGBA32F | Normal encoded `n*0.5+0.5` (a=1) |
| MRT2 | RGBA32F | **Albedo RGB (linear) + Roughness in A** (gltf `roughnessFactor`) |
| MRT3 | R32F | Linear view-space depth |
| Depth | D32 | Hardware depth attachment (occlusion) |

Albedo comes from the mesh's real texture (sampled at UV); untextured meshes
use `baseColorFactor` (or neutral 0.7 gray for a white factor). Roughness is
the per-material gltf factor carried in `FInstanceInfo.Roughness`.

## FInstanceInfo (48-byte, shared C++ ↔ HLSL)

Layout is mirrored in `GBufferPT_VS.hlsl`, `GBufferPT_PS.hlsl` and
`GIPathTracing.hlsl` (`RTInstanceInfo`). Keep them in sync:

```
uint VertexOffset; uint IndexOffset; uint VertexCount; uint IndexCount;  // 0-15
float3 AlbedoColor;                                                     // 16-27
uint AlbedoTextureIndex;                                                // 28
uint MaterialFlags;       // bit0 = has real albedo texture              // 32
float Roughness;                                                        // 36
float Metallic;                                                         // 40
uint Pad;                                                               // 44
```

`AlbedoColor` semantics: for textured meshes it holds the **linear average
albedo of the texture** (feeds ray-trace bounce shading via
`RTInstanceInfo`); the GBuffer per-pixel path ignores it when the texture
flag is set. For untextured meshes it is the flat albedo.

## Texture pipeline

- Loader (`Scene3DLoader.cpp`) reads `aiTextureType_BASE_COLOR` etc. into
  `FPBRMaterial` paths (resolved against the scene dir).
- GPU upload: `FAsyncTextureLoader::LoadMaterialTexturesAsync` (async decode +
  batched upload; `.ktx2` variants preferred). Verified: 24/24 Sponza albedo
  textures load.
- sRGB: `VK_FORMAT_R8G8B8A8_SRGB` → `nvrhi::Format::SRGBA8_UNORM`, sampled
  values linearize automatically. CPU-side averages are linearized with
  `pow(x, 2.2)`.
- Per-mesh binding in the GBuffer pass: `t0` albedo + `s0` sampler; a 1×1
  white placeholder covers untextured meshes.

## Rules for future material work

1. **No palette/name-hash hacks.** Neutral materials use their factor or gray,
   never an arbitrary per-name color.
2. Keep the three HLSL mirrors of `FInstanceInfo` in sync with the C++ struct.
3. Roughness lives in `GBufferMaterial.a`; GI consumes it (roughness lobe in
   `GIPathTracing.hlsl`).
4. Bounce albedo comes from the texture average (`RTInstanceInfo.AlbedoColor`);
   per-texel bounce sampling (bindless/descriptor-indexing texture array) is
   the documented future upgrade.
