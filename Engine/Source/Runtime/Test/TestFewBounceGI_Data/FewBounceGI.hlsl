/*
 * Few-bounce GI Shader - Sequential Bounce Tracing for RESTIR GI
 *
 * Fixes:
 *   - ClosestHit uses barycentric vertex-normal interpolation from bound mesh data
 *   - Primary surface direct lighting added (uses GBufferDiffuse)
 *   - 4-SPP loop for reduced noise
 *   - Per-instance albedo color from material
 *   - Removed *0.5 throughput darkening
 */

// =============================================================================
// Configuration
// =============================================================================

static const uint MAX_BOUNCES = 3;
static const uint SPP = 8;          // samples per pixel

// =============================================================================
// Structures
// =============================================================================

struct GIPayload {
    float3 throughput;
    float3 radiance;
    float3 origin;
    float3 direction;
    uint bounceCount;
    uint flags;
};

struct GIConstants {
    float4 LightDir;
    float4 AmbientColor;
    float4 CameraPos;
    float4 Padding1;
};

struct ViewConstants {
    float4x4 ModelMatrix;
    float4x4 ViewMatrix;
    float4x4 ProjMatrix;
    float2 RenderTargetSize;
    float2 Padding;
};

struct FRTVertex {
    float3 Position;
    float3 Normal;
};

struct FInstanceInfo {
    uint VertexOffset;
    uint IndexOffset;
    uint VertexCount;
    uint IndexCount;
    float3 AlbedoColor;
    float Padding;
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

StructuredBuffer<FRTVertex>     RTVertices      : register(t5);
StructuredBuffer<uint>          RTIndices       : register(t6);
StructuredBuffer<FInstanceInfo> RTInstanceInfo  : register(t7);

// =============================================================================
// Random Number Generation
// =============================================================================

float hash(uint seed) {
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

    float3 up = (abs(normal.z) < 0.999) ? float3(0.0, 0.0, 1.0) : float3(1.0, 0.0, 0.0);
    float3 tangent = normalize(cross(up, normal));
    float3 bitangent = cross(normal, tangent);

    return tangent * cos(phi) * sinTheta + bitangent * sin(phi) * sinTheta + normal * cosTheta;
}

// ACES-inspired filmic tonemap (maps HDR to [0,1] without crushing shadows)
float3 aces(float3 x) {
    float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

// =============================================================================
// Ray Generation Shader
// =============================================================================

[shader("raygeneration")]
void RayGen() {
    uint2 pixel = DispatchRaysIndex().xy;
    uint2 dim = DispatchRaysDimensions().xy;
    uint pixelSeed = pixel.x * 1920u + pixel.y;

    float3 worldPos = GBufferWorldPos[pixel].rgb;
    float3 normal = normalize(GBufferNormals[pixel].rgb * 2.0 - 1.0);
    float3 diffuse = GBufferDiffuse[pixel].rgb;

    if (length(worldPos) < 0.001) {
        Output[pixel] = float4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    float3 lightDir = normalize(g_GI.LightDir.xyz);

    // Primary visible surface direct lighting
    float NdotL0 = max(dot(normal, lightDir), 0.0);
    float3 primaryDirect = diffuse * NdotL0 * g_GI.LightDir.w;
    float3 primaryAmbient = diffuse * g_GI.AmbientColor.rgb * 0.3;
    float3 result = primaryDirect + primaryAmbient;

    // Multi-sample indirect GI
    float3 indirect = float3(0.0);

    for (uint s = 0; s < SPP; ++s) {
        GIPayload payload;
        payload.radiance = float3(0.0);
        payload.throughput = float3(1.0);
        payload.bounceCount = 0;
        payload.flags = 0;

        float3 rayOrigin = worldPos + normal * 0.01;
        float3 rayDir = sampleHemisphereCosine(
            normal,
            random(pixelSeed + s * 7919u, 0, 0),
            random(pixelSeed + s * 7919u, 0, 1));

        for (uint bounce = 0; bounce < MAX_BOUNCES; ++bounce) {
            RayDesc ray;
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

            if (!(payload.flags & 0x01))
                break;

            rayOrigin = payload.origin;
            rayDir = payload.direction;
            payload.bounceCount = bounce + 1;
        }

        indirect += payload.radiance;
    }

    result += indirect / float(SPP);
    // Tonemap before output to keep values in sensible range
    Output[pixel] = float4(aces(result), 1.0);
}

// =============================================================================
// Closest Hit
// =============================================================================

struct Attributes {
    float2 barycentrics;
};

[shader("closesthit")]
void ClosestHit(inout GIPayload payload : SV_RayPayload, in Attributes attr : SV_IntersectionAttributes) {
    float3 worldPos = WorldRayOrigin();
    float3 worldDir = WorldRayDirection();
    float hitT = RayTCurrent();

    float3 hitPosition = worldPos + worldDir * hitT;

    // Look up actual vertex normals from bound mesh data
    uint instanceIdx = InstanceIndex();
    uint primitiveIdx = PrimitiveIndex();

    FInstanceInfo info = RTInstanceInfo[instanceIdx];

    uint i0 = RTIndices[info.IndexOffset + primitiveIdx * 3 + 0];
    uint i1 = RTIndices[info.IndexOffset + primitiveIdx * 3 + 1];
    uint i2 = RTIndices[info.IndexOffset + primitiveIdx * 3 + 2];

    FRTVertex v0 = RTVertices[i0];
    FRTVertex v1 = RTVertices[i1];
    FRTVertex v2 = RTVertices[i2];

    float3 bary = float3(1.0 - attr.barycentrics.x - attr.barycentrics.y, attr.barycentrics.x, attr.barycentrics.y);

    float3 localNormal = v0.Normal * bary.x + v1.Normal * bary.y + v2.Normal * bary.z;
    float3 hitNormal = normalize(mul(ObjectToWorld3x4(), float4(localNormal, 0.0)).xyz);

    // Use per-instance albedo color from material
    float3 albedo = info.AlbedoColor;

    float3 lightDir = normalize(g_GI.LightDir.xyz);
    float NdotL = max(dot(hitNormal, lightDir), 0.0);

    payload.radiance += payload.throughput * albedo * NdotL * g_GI.LightDir.w;
    payload.radiance += payload.throughput * albedo * g_GI.AmbientColor.rgb * 0.5;

    payload.throughput *= albedo;

    uint2 pixel = DispatchRaysIndex().xy;
    uint pixelSeed = pixel.x * 1920u + pixel.y;
    float r1 = random(pixelSeed, payload.bounceCount + 1, 0);
    float r2 = random(pixelSeed, payload.bounceCount + 1, 1);
    payload.direction = sampleHemisphereCosine(hitNormal, r1, r2);
    payload.origin = hitPosition + hitNormal * 0.01;

    payload.flags |= 0x01;
}

// =============================================================================
// Environment Sampling
// =============================================================================

float3 SampleSky(float3 direction)
{
    float3 sunDir = normalize(g_GI.LightDir.xyz);
    float sunDot = dot(direction, sunDir);

    float3 zenithColor = float3(0.4, 0.6, 1.0);
    float3 horizonColor = float3(0.7, 0.8, 0.9);
    float3 groundColor = float3(0.15, 0.12, 0.1);

    float3 skyColor;
    if (direction.y > 0.0)
    {
        skyColor = lerp(horizonColor, zenithColor, direction.y);
    }
    else
    {
        skyColor = lerp(horizonColor, groundColor, -direction.y);
    }

    float sunDisk = pow(max(sunDot, 0.0), 512.0);
    float3 sunColor = float3(1.0, 0.95, 0.8) * g_GI.LightDir.w;
    float sunGlow = pow(max(sunDot, 0.0), 6.0) * 0.15;
    float skyIntensity = 0.5;

    return (skyColor + sunColor * sunDisk + sunColor * sunGlow) * skyIntensity;
}

// =============================================================================
// Miss Shader
// =============================================================================

[shader("miss")]
void Miss(inout GIPayload payload : SV_RayPayload) {
    float3 skyRadiance = SampleSky(WorldRayDirection());
    payload.radiance += payload.throughput * skyRadiance;
    payload.flags &= ~0x01;
}
