/*
 * RT Reflections Shader - Single-Bounce Ray-Traced Reflections
 *
 * Pipeline:
 *   1. RayGen reads GBuffer world position and normal
 *   2. Casts reflection ray using reflect(-viewDir, normal)
 *   3. On hit: samples material from GBuffer using projected hit position
 *   4. On miss: returns sky color
 *
 * Purpose: Demonstrates RT reflections on a mirror sphere
 * with visible Sponza scene in the reflection.
 */

// =============================================================================
// Structures
// =============================================================================

struct ReflectionPayload {
    float3 color;      // Reflected color
    float3 position;  // Hit position
    bool hit;          // Did ray hit something?
};

struct LightingConstants {
    float4 LightDir;      // xyz = direction TO light, w = intensity
    float4 AmbientColor;  // xyz = ambient RGB, w unused
    float4 CameraPos;     // xyz = camera world pos, w = reflection strength
    float4 Padding1;
};

struct ViewConstants {
    float4x4 ModelMatrix;
    float4x4 ViewMatrix;
    float4x4 ProjMatrix;
    float2 RenderTargetSize; // xy = width, height
    float2 Padding;          // z, w unused
};

// =============================================================================
// Resources
// =============================================================================

ConstantBuffer<LightingConstants> g_Lighting : register(b0);
ConstantBuffer<ViewConstants> g_View : register(b1);

RWTexture2D<float4> Output : register(u0);

RaytracingAccelerationStructure SceneBVH : register(t0);

Texture2D<float4> GBufferWorldPos : register(t1);
Texture2D<float4> GBufferNormals  : register(t2);
Texture2D<float4> GBufferDiffuse  : register(t3);
Texture2D<float4> GBufferSpecular : register(t4); // RGB = F0, A = roughness

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
    float4 specularData = GBufferSpecular[pixel];

    // Background / sky pixel (worldPos near zero means no geometry)
    if (length(worldPos) < 0.001) {
        Output[pixel] = float4(0.1, 0.15, 0.3, 1.0);
        return;
    }

    float3 lightDir = normalize(g_Lighting.LightDir.xyz);
    float3 viewDir = normalize(g_Lighting.CameraPos.xyz - worldPos);
    float reflectWeight = g_Lighting.CameraPos.w; // Reflection strength (0.0-1.0)

    // =====================================================================
    // Reflection Ray
    // =====================================================================
    ReflectionPayload reflectionPayload;
    reflectionPayload.color = float3(0.1, 0.15, 0.3); // default: sky color
    reflectionPayload.position = float3(0.0, 0.0, 0.0);
    reflectionPayload.hit = false;

    float3 reflectDir = reflect(-viewDir, normal);

    RayDesc reflectionRay;
    reflectionRay.Origin = worldPos + normal * 0.01; // normal bias
    reflectionRay.Direction = reflectDir;
    reflectionRay.TMin = 0.001;
    reflectionRay.TMax = 1000.0;

    TraceRay(
        SceneBVH,
        RAY_FLAG_FORCE_OPAQUE,
        0xFF,
        0,  // sbtRecordOffset (hit group 0 = HitGroup)
        0,  // sbtRecordStride
        0,  // missIndex (Miss)
        reflectionRay,
        reflectionPayload);

    // =====================================================================
    // Shading: Diffuse + Reflections
    // =====================================================================
    float NdotL = max(dot(normal, lightDir), 0.0);
    float3 ambient = g_Lighting.AmbientColor.rgb;

    // Lambertian diffuse
    float3 diffuseContrib = diffuse * (ambient + NdotL * g_Lighting.LightDir.w);

    // Reflection contribution
    float3 reflectionContrib = reflectionPayload.color;

    // Blend diffuse and reflection based on weight
    float3 litColor = diffuseContrib * (1.0 - reflectWeight) + reflectionContrib * reflectWeight;

    Output[pixel] = float4(litColor, 1.0);
}

// =============================================================================
// Closest Hit - Samples GBuffer at projected hit position
// =============================================================================

[shader("closesthit")]
void ClosestHit(inout ReflectionPayload payload : SV_RayPayload, in float3 hitAttr : SV_HitT) {
    // Get the current ray info
    float3 worldPos = WorldRayOrigin();
    float3 worldDir = WorldRayDirection();
    float hitT = RayTCurrent();

    // Compute hit position
    float3 hitPosition = worldPos + worldDir * hitT;

    // Project hit position to view space, then to clip space
    float4 viewPos = mul(g_View.ViewMatrix, float4(hitPosition, 1.0));
    float4 clipPos = mul(g_View.ProjMatrix, viewPos);

    // Convert to UV coordinates (0 to 1)
    float2 projUV = (clipPos.xy / clipPos.w) * 0.5 + 0.5;

    // Flip Y because texture UV is inverted relative to clip space
    projUV.y = 1.0 - projUV.y;

    // Clamp to valid texture range to avoid out-of-bounds sampling
    projUV = clamp(projUV, 0.001, 0.999);

    // Sample GBuffer at the projected position
    // Note: We sample the diffuse texture at the hit location to get the material color
    float3 hitDiffuse = GBufferDiffuse[uint2(projUV * g_View.RenderTargetSize)].rgb;

    // Use the sampled diffuse color as the reflection contribution
    // This shows what the material looks like at the reflected hit location
    payload.color = hitDiffuse;
    payload.position = hitPosition;
    payload.hit = true;
}

// =============================================================================
// Miss - Returns sky color
// =============================================================================

[shader("miss")]
void Miss(inout ReflectionPayload payload : SV_RayPayload) {
    // Sky color - same as background
    payload.color = float3(0.1, 0.15, 0.3);
    payload.hit = false;
}