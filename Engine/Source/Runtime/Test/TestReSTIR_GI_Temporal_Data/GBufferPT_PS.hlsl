/*
 * GBufferPT_PS.hlsl — Path-trace GBuffer pixel shader for Sponza.
 *
 * Writes 4 MRTs that match the formats/consumption expectations of
 * GIPathTracing.hlsl and the ReSTIR temporal/spatial passes:
 *
 *   MRT0 : GBufferWorldPos   (RGBA32F) — float4(worldPos.xyz, 1.0)
 *   MRT1 : GBufferNormal     (RGBA32F) — float4(n * 0.5 + 0.5, 1.0)
 *   MRT2 : GBufferMaterial   (RGBA32F) — float4(albedo.rgb, 1.0)
 *   MRT3 : LinearDepth       (R32F)    — positive view-space depth (-viewPos.z)
 *
 * Normal encoding: GIPathTracing.hlsl decodes normals as
 *   normal = GBufferNormal[i].rgb * 2.0 - 1.0
 * so the PS stores the *encoded* form (n * 0.5 + 0.5).
 *
 * WorldPos and Material are passed through unchanged (just an alpha=1
 * "hit present" sentinel for GBufferWorldPos).
 *
 * LinearDepth (MRT3) is consumed by the ReSTIR temporal pass (depth
 * reprojection validation) and spatial pass (geometric rejection), and by
 * FBilateralDenoisePass. It must be a real per-pixel depth — a zero-filled
 * texture makes every depth comparison pass trivially.
 *
 * Note: the alpha channel of GBufferWorldPos carries the primary-hit
 * flag for the path tracer. We write 1.0 because every rasterized
 * fragment here IS a primary hit.
 *
 * 2026-08-10 (material rework Phase 1): MRT2 now samples the mesh's real
 * Sponza albedo texture (t0) when the instance flags it (MaterialFlags bit 0);
 * otherwise the per-instance AlbedoColor fallback is used (placeholder white
 * texture * AlbedoColor). This replaces the palette-hash "colored pillars" bug.
 */

Texture2D<float4> DiffuseTexture : register(t0);
SamplerState      LinearSampler  : register(s0);

// Per-instance material info (b1) — must match FInstanceInfo / GBufferPT_VS.
cbuffer PerInstanceInfo : register(b1) {
    uint   VertexOffset;
    uint   IndexOffset;
    uint   VertexCount;
    uint   IndexCount;
    float3 AlbedoColor;
    uint   AlbedoTextureIndex;
    uint   MaterialFlags;   // bit 0 = has real albedo texture (Phase 1)
    float  Roughness;       // gltf roughnessFactor (2026-08-10 Phase 2)
    float  Metallic;
    uint   Pad;
};

struct PSInput {
    float4 Position : SV_POSITION;
    float3 WorldPos : TEXCOORD0;
    float3 Normal   : TEXCOORD1;
    float3 Albedo   : TEXCOORD2;
    float  ViewDepth: TEXCOORD3;
    float2 UV       : TEXCOORD4;
};

struct MRTOutput {
    float4 MRT0 : SV_TARGET0; // WorldPos  (RGBA32F)
    float4 MRT1 : SV_TARGET1; // Normal    (RGBA32F, encoded)
    float4 MRT2 : SV_TARGET2; // Material  (RGBA32F)
    float  MRT3 : SV_TARGET3; // LinearDepth (R32F, positive view z)
};

MRTOutput main(PSInput input) {
    MRTOutput output;
    output.MRT0 = float4(input.WorldPos, 1.0);
    output.MRT1 = float4(input.Normal   * 0.5 + 0.5, 1.0);

    // Real Sponza albedo texture when the instance has one (MaterialFlags bit 0).
    // The fallback path multiplies the white placeholder by AlbedoColor.
    float3 texAlbedo = DiffuseTexture.Sample(LinearSampler, input.UV).rgb;
    float3 albedo = (MaterialFlags & 1u) != 0u ? texAlbedo : texAlbedo * input.Albedo;
    // Phase 2: alpha carries the material roughness (gltf roughnessFactor).
    output.MRT2 = float4(albedo, Roughness);

    output.MRT3 = input.ViewDepth;
    return output;
}
