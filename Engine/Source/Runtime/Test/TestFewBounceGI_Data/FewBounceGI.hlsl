/*
 * Few-bounce GI Shader - Sequential Bounce Tracing for RESTIR GI
 *
 * Pipeline:
 *   1. RayGen reads GBuffer world position and normal
 *   2. Casts rays sequentially (bounce 0, 1, 2, ...)
 *   3. Each bounce: sample material, accumulate radiance, continue if bounceCount < MAX_BOUNCES
 *   4. On miss: return black
 *
 * Purpose: Provides few-bounce GI infrastructure for RESTIR.
 * Not full path tracing - just 2-4 bounces, cosine sampling, miss=black.
 */

// =============================================================================
// Configuration
// =============================================================================

static const uint MAX_BOUNCES = 3;  // 0, 1, 2 = 3 bounces total

// =============================================================================
// Structures
// =============================================================================

struct GIPayload {
    float3 throughput;    // dims on each bounce
    float3 radiance;     // accumulated result
    float3 origin;       // current ray origin (for next bounce)
    float3 direction;    // current ray direction (for next bounce)
    uint bounceCount;    // current bounce (0 to MAX_BOUNCES)
    uint flags;         // bit 0 = hit flag
};

struct GIConstants {
    float4 LightDir;      // xyz = direction TO light, w = intensity
    float4 AmbientColor;  // xyz = ambient RGB, w unused
    float4 CameraPos;     // xyz = camera world pos, w = bounce strength
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

ConstantBuffer<GIConstants> g_GI : register(b0);
ConstantBuffer<ViewConstants> g_View : register(b1);

RWTexture2D<float4> Output : register(u0);

RaytracingAccelerationStructure SceneBVH : register(t0);

Texture2D<float4> GBufferWorldPos : register(t1);
Texture2D<float4> GBufferNormals  : register(t2);
Texture2D<float4> GBufferDiffuse  : register(t3);
Texture2D<float4> GBufferSpecular : register(t4);

// =============================================================================
// Random Number Generation (simple hash-based)
// =============================================================================

float hash(uint seed) {
    // Simple hash function
    seed ^= seed >> 16;
    seed *= 0x21f0aaadU;
    seed ^= seed >> 15;
    seed *= 0x735a2d97U;
    seed ^= seed >> 15;
    return float(seed) / float(0xFFFFFFFFU);
}

float random(uint pixelSeed, uint bounce, uint sampleIdx) {
    return hash(pixelSeed + bounce * 17 + sampleIdx * 31);
}

// =============================================================================
// Cosine-Weighted Hemisphere Sampling
// =============================================================================

float3 sampleHemisphereCosine(float3 normal, float r1, float r2) {
    float phi = 2.0 * 3.14159265 * r2;
    float cosTheta = sqrt(1.0 - r1);
    float sinTheta = sqrt(r1);

    // Build local coordinate frame
    float3 up = (abs(normal.z) < 0.999) ? float3(0.0, 0.0, 1.0) : float3(1.0, 0.0, 0.0);
    float3 tangent = normalize(cross(up, normal));
    float3 bitangent = cross(normal, tangent);

    // Sample in local space and transform
    return tangent * cos(phi) * sinTheta + bitangent * sin(phi) * sinTheta + normal * cosTheta;
}

// =============================================================================
// Ray Generation Shader - Sequential Bounce Tracing
// =============================================================================

[shader("raygeneration")]
void RayGen() {
    uint2 pixel = DispatchRaysIndex().xy;
    uint2 dim = DispatchRaysDimensions().xy;

    // Create seed from pixel coordinates
    uint pixelSeed = pixel.x * 1920u + pixel.y;

    // Read GBuffer
    float3 worldPos = GBufferWorldPos[pixel].rgb;
    float3 normal = normalize(GBufferNormals[pixel].rgb * 2.0 - 1.0);
    float3 diffuse = GBufferDiffuse[pixel].rgb;

    // Background / sky pixel (worldPos near zero means no geometry)
    if (length(worldPos) < 0.001) {
        Output[pixel] = float4(0.0, 0.0, 0.0, 1.0); // BLACK - no GI for sky
        return;
    }

    float3 lightDir = normalize(g_GI.LightDir.xyz);
    float3 viewDir = normalize(g_GI.CameraPos.xyz - worldPos);

    // Initialize GI payload
    GIPayload payload;
    payload.radiance = float3(0.0);
    payload.throughput = float3(1.0);
    payload.bounceCount = 0;
    payload.flags = 0;

    // =====================================================================
    // BOUNCE 0: Initial ray from camera
    // =====================================================================
    float3 rayOrigin = worldPos + normal * 0.01; // normal bias
    float3 rayDir = sampleHemisphereCosine(
        normal,
        random(pixelSeed, 0, 0),
        random(pixelSeed, 0, 1));

    RayDesc ray;
    ray.Origin = rayOrigin;
    ray.Direction = rayDir;
    ray.TMin = 0.001;
    ray.TMax = 1000.0;

    TraceRay(
        SceneBVH,
        RAY_FLAG_FORCE_OPAQUE,
        0xFF,
        0,  // sbtRecordOffset
        0,  // sbtRecordStride
        0,  // missIndex
        ray,
        payload);

    // Check if we hit or missed
    if (payload.flags & 0x01) {
        // =====================================================================
        // BOUNCE 1: Continue if bounceCount < MAX_BOUNCES
        // =====================================================================
        payload.bounceCount++;
        if (payload.bounceCount >= MAX_BOUNCES) {
            Output[pixel] = float4(payload.radiance, 1.0);
            return;
        }

        // Continue from hit position
        rayOrigin = payload.origin;
        rayDir = payload.direction;

        ray.Origin = rayOrigin;
        ray.Direction = rayDir;
        ray.TMin = 0.001;
        ray.TMax = 1000.0;

        TraceRay(
            SceneBVH,
            RAY_FLAG_FORCE_OPAQUE,
            0xFF,
            0,
            0,
            0,
            ray,
            payload);

        if (payload.flags & 0x01) {
            // =====================================================================
            // BOUNCE 2: Continue if bounceCount < MAX_BOUNCES
            // =====================================================================
            payload.bounceCount++;
            if (payload.bounceCount >= MAX_BOUNCES) {
                Output[pixel] = float4(payload.radiance, 1.0);
                return;
            }

            // Continue from hit position
            rayOrigin = payload.origin;
            rayDir = payload.direction;

            ray.Origin = rayOrigin;
            ray.Direction = rayDir;
            ray.TMin = 0.001;
            ray.TMax = 1000.0;

            TraceRay(
                SceneBVH,
                RAY_FLAG_FORCE_OPAQUE,
                0xFF,
                0,
                0,
                0,
                ray,
                payload);

            // NOTE: Can add more bounces here if MAX_BOUNCES > 3
        }
    }

    Output[pixel] = float4(payload.radiance, 1.0);
}

// =============================================================================
// Closest Hit - Sample material and continue path
// =============================================================================

[shader("closesthit")]
void ClosestHit(inout GIPayload payload : SV_RayPayload, in float3 hitAttr : SV_HitT) {
    // Get the current ray info
    float3 worldPos = WorldRayOrigin();
    float3 worldDir = WorldRayDirection();
    float hitT = RayTCurrent();

    // Compute hit position
    float3 hitPosition = worldPos + worldDir * hitT;

    // Get hit normal (from attributes)
    float3 hitNormal = normalize(hitAttr);

    // Sample material albedo (simplified - use constant gray)
    float3 albedo = float3(0.7); // Gray material

    // Accumulate radiance with throughput weighting
    float3 lightDir = normalize(g_GI.LightDir.xyz);
    float NdotL = max(dot(hitNormal, lightDir), 0.0);

    // Add direct lighting contribution
    payload.radiance += payload.throughput * albedo * NdotL * g_GI.LightDir.w;

    // Add ambient contribution
    payload.radiance += payload.throughput * albedo * g_GI.AmbientColor.rgb * 0.1;

    // Update throughput for next bounce
    payload.throughput *= albedo * 0.5; // energy preservation

    // Continue path with cosine-weighted hemisphere
    uint2 pixel = DispatchRaysIndex().xy;
    uint pixelSeed = pixel.x * 1920u + pixel.y;
    float r1 = random(pixelSeed, payload.bounceCount + 1, 0);
    float r2 = random(pixelSeed, payload.bounceCount + 1, 1);
    payload.direction = sampleHemisphereCosine(hitNormal, r1, r2);
    payload.origin = hitPosition + hitNormal * 0.01; // normal bias

    // Set hit flag
    payload.flags |= 0x01;
}

// =============================================================================
// Environment Sampling
// =============================================================================

float3 SampleSky(float3 direction)
{
    float3 sunDir = normalize(g_GI.LightDir.xyz);
    float sunDot = dot(direction, sunDir);

    // Horizon gradient: blue sky above, warmer near horizon
    float3 zenithColor = float3(0.4, 0.6, 1.0);
    float3 horizonColor = float3(0.7, 0.8, 0.9);
    float3 groundColor = float3(0.15, 0.12, 0.1);

    float3 skyColor;
    if (direction.y > 0.0)
    {
        // Upper hemisphere: blend zenith to horizon
        skyColor = lerp(horizonColor, zenithColor, direction.y);
    }
    else
    {
        // Lower hemisphere: blend horizon to ground
        skyColor = lerp(horizonColor, groundColor, -direction.y);
    }

    // Sun disk
    float sunDisk = pow(max(sunDot, 0.0), 512.0);
    float3 sunColor = float3(1.0, 0.95, 0.8) * g_GI.LightDir.w;

    // Sun glow / corona
    float sunGlow = pow(max(sunDot, 0.0), 6.0) * 0.15;

    // Sky intensity scale (tuned to not overpower direct lighting)
    float skyIntensity = 0.3;

    return (skyColor + sunColor * sunDisk + sunColor * sunGlow) * skyIntensity;
}

// =============================================================================
// Miss Shader - Sample environment
// =============================================================================

[shader("miss")]
void Miss(inout GIPayload payload : SV_RayPayload) {
    // Sample procedural sky
    float3 skyRadiance = SampleSky(WorldRayDirection());
    payload.radiance += payload.throughput * skyRadiance;
    payload.flags &= ~0x01; // clear hit flag
}
