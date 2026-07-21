/*
 * GBufferPT_PS.hlsl — Path-trace GBuffer pixel shader for Sponza.
 *
 * Writes 3 MRTs that match the formats/consumption expectations of
 * GIPathTracing.hlsl:
 *
 *   MRT0 : GBufferWorldPos   (RGBA32F) — float4(worldPos.xyz, 1.0)
 *   MRT1 : GBufferNormal     (RGBA32F) — float4(n * 0.5 + 0.5, 1.0)
 *   MRT2 : GBufferMaterial   (RGBA32F) — float4(albedo.rgb, 1.0)
 *
 * Normal encoding: GIPathTracing.hlsl decodes normals as
 *   normal = GBufferNormal[i].rgb * 2.0 - 1.0
 * so the PS stores the *encoded* form (n * 0.5 + 0.5).
 *
 * WorldPos and Material are passed through unchanged (just an alpha=1
 * "hit present" sentinel for GBufferWorldPos).
 *
 * Note: the alpha channel of GBufferWorldPos carries the primary-hit
 * flag for the path tracer. We write 1.0 because every rasterized
 * fragment here IS a primary hit.
 */

struct PSInput {
    float4 Position : SV_POSITION;
    float3 WorldPos : TEXCOORD0;
    float3 Normal   : TEXCOORD1;
    float3 Albedo   : TEXCOORD2;
};

struct MRTOutput {
    float4 MRT0 : SV_TARGET0; // WorldPos  (RGBA32F)
    float4 MRT1 : SV_TARGET1; // Normal    (RGBA32F, encoded)
    float4 MRT2 : SV_TARGET2; // Material  (RGBA32F)
};

MRTOutput main(PSInput input) {
    MRTOutput output;

    // WorldPos — pass through.
    output.MRT0 = float4(input.WorldPos, 1.0);

    // Normal — encode [-1,1] -> [0,1]. GIPathTracing.hlsl decodes via *2-1.
    float3 n = normalize(input.Normal);
    output.MRT1 = float4(n * 0.5 + 0.5, 1.0);

    // Material — albedo color from per-instance FInstanceInfo.
    output.MRT2 = float4(input.Albedo, 1.0);

    return output;
}
