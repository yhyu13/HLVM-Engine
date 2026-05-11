/*
 * RT Shadows GBuffer Shader - Shadow Rays Only
 *
 * Pipeline:
 *   1. RayGen reads GBuffer world position, normal, diffuse
 *   2. Casts shadow ray toward animated light direction
 *   3. Outputs Lambertian shaded color with hard shadows
 *
 * No primary rays are traced - geometry info comes from rasterized GBuffer.
 */

// =============================================================================
// Structures
// =============================================================================

struct ShadowPayload {
    bool inShadow;
};

struct LightingConstants {
    float4 LightDir;      // xyz = direction TO light, w = intensity
    float4 AmbientColor;  // xyz = ambient RGB, w unused
    float4 Padding0;
    float4 Padding1;
};

// =============================================================================
// Resources
// =============================================================================

ConstantBuffer<LightingConstants> g_Lighting : register(b0);

RWTexture2D<float4> Output : register(u0);

RaytracingAccelerationStructure SceneBVH : register(t0);

Texture2D<float4> GBufferWorldPos : register(t1);
Texture2D<float4> GBufferNormals  : register(t2);
Texture2D<float4> GBufferDiffuse  : register(t3);

// =============================================================================
// Ray Generation Shader
// =============================================================================

[shader("raygeneration")]
void RayGen() {
    uint2 pixel = DispatchRaysIndex().xy;
    uint2 dim = DispatchRaysDimensions().xy;

    // Read GBuffer
    float3 worldPos = GBufferWorldPos[pixel].rgb;
    float3 normal = normalize(GBufferNormals[pixel].rgb * 2.0 - 1.0);
    float3 diffuse = GBufferDiffuse[pixel].rgb;

    // Background / sky pixel (worldPos near zero means no geometry)
    if (length(worldPos) < 0.001) {
        Output[pixel] = float4(0.1, 0.15, 0.3, 1.0);
        return;
    }

    float3 lightDir = normalize(g_Lighting.LightDir.xyz);

    // Cast shadow ray toward light
    ShadowPayload payload;
    payload.inShadow = true; // default: shadowed

    RayDesc shadowRay;
    shadowRay.Origin = worldPos + normal * 0.01; // normal bias to avoid self-intersection
    shadowRay.Direction = lightDir;
    shadowRay.TMin = 0.001;
    shadowRay.TMax = 1000.0;

    TraceRay(
        SceneBVH,
        RAY_FLAG_FORCE_OPAQUE,
        0xFF,
        0,  // sbtRecordOffset (hit group 0)
        0,  // sbtRecordStride
        1,  // missIndex 1 = ShadowMiss
        shadowRay,
        payload);

    // Lambertian shading
    float NdotL = max(dot(normal, lightDir), 0.0);
    float3 ambient = g_Lighting.AmbientColor.rgb;
    float3 litColor = diffuse * (ambient + (payload.inShadow ? 0.0 : NdotL * g_Lighting.LightDir.w));

    Output[pixel] = float4(litColor, 1.0);
}

// =============================================================================
// Closest Hit Shader - Shadow ray hit = occluded
// =============================================================================

[shader("closesthit")]
void ClosestHit(inout ShadowPayload payload : SV_RayPayload, in float3 hitAttr : SV_HitT) {
    payload.inShadow = true;
}

// =============================================================================
// Miss Shader - Unused (shadow rays use miss index 1)
// =============================================================================

[shader("miss")]
void Miss(inout ShadowPayload payload : SV_RayPayload) {
    // Primary rays not used in this pipeline
}

// =============================================================================
// Shadow Miss Shader - Shadow ray missed = lit
// =============================================================================

[shader("miss")]
void ShadowMiss(inout ShadowPayload payload : SV_RayPayload) {
    payload.inShadow = false;
}
