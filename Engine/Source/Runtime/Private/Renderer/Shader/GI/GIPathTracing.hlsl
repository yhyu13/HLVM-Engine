// GIPathTracing.hlsl - Few-bounce GI ray-tracing shader library.
//
// Task 1.5 of ReSTIR/GI separation sprint-1: migrated real shader bodies from
// TestFewBounceGI_Data/FewBounceGI.hlsl, adapted to FGIPass binding layout.
//
// Entry points (all compiled into one RT library):
//   - RayGen       [shader("raygeneration")]
//   - ClosestHit   [shader("closesthit")]
//   - Miss         [shader("miss")]
//   - ShadowMiss   [shader("miss")]
//
// Resources (bound via FGIPass binding layout):
//   b0  GIConstants
//   b1  ViewConstants
//   t0  SceneBVH
//   t1  GBufferWorldPos
//   t2  GBufferNormal
//   t3  GBufferMaterial   (rgb = albedo/diffuse)
//   t5  RTVertices
//   t6  RTIndices
//   t7  Lights      (StructuredBuffer<FLight>)
//   t8  RTInstanceInfo
//   s2  LinearSampler
//   u0  OutputTexture (radiance)
//   u1  DebugStatsTexture (optional, gated by GI_DEBUG_STATS)

#include "Common/FLight.hlsl"

// =============================================================================
// Payloads
// =============================================================================

// Explicitly padded payload. HLSL aligns float3 to 16 bytes in ray payloads,
// so every float3 is followed by a 4-byte scalar and every uint group is a uint4.
struct GIPayload {
    float3 throughput;
    float  _pad0;
    float3 radiance;
    float  _pad1;
    float3 origin;
    float  _pad2;
    float3 direction;
    float  _pad3;
    float3 hitNormal;
    float  hitDistance;
    uint   bounceCount;
    uint   flags;            // bit 0 = continue path, bit 1 = terminated by RR
    uint   seed;
    uint   debugNormalFlags; // 1 = per-vertex normal invalid, 2 = geometric fallback invalid
    float3 debugVertexNormal;
    float  _pad4;
    float3 debugGeoNormal;
    float  _pad5;
};

struct ShadowPayload {
    bool occluded;
};

// =============================================================================
// Constant buffers
// =============================================================================

struct GIConstants {
    float4 LightDir;
    float4 AmbientColor;
    float4 CameraPos;
    float4 Params;   // x=MaxBounces, y=SPP, z=ShadowTMin, w=ShadowTMax
    float4 Params2;  // x=AmbientScale, y=RayTMin, z=RayTMax, w=ShadowEnable
    float4 Params3;  // x=EnableRR, y=RussianRouletteMinSurvival, z=DebugStatsEnabled, w=LightCount
    float4 Params4;  // x=EnableNEE, y=MISPower, z=SingleLightNEE, w=BSDFDirectMIS
    float4 Params5;  // x=DebugMode, yzw=unused
};

struct ViewConstants {
    float4x4 ModelMatrix;
    float4x4 ViewMatrix;
    float4x4 ProjMatrix;
    float2   RenderTargetSize;
    float    FrameIndex;
    float    Pad;
};

ConstantBuffer<GIConstants> g_GI : register(b0);
ConstantBuffer<ViewConstants> g_View : register(b1);

// =============================================================================
// Resources
// =============================================================================

RWTexture2D<float4> Output : register(u0);

#if GI_DEBUG_STATS
RWTexture2D<float4> DebugStatsTexture : register(u1);
#endif

RaytracingAccelerationStructure SceneBVH : register(t0);

Texture2D<float4> GBufferWorldPos   : register(t1);
Texture2D<float4> GBufferNormal     : register(t2);
Texture2D<float4> GBufferMaterial   : register(t3);

StructuredBuffer<FLight>        Lights          : register(t7);
StructuredBuffer<FRTVertex>     RTVertices      : register(t5);
StructuredBuffer<uint>          RTIndices       : register(t6);
StructuredBuffer<FInstanceInfo> RTInstanceInfo  : register(t8);

SamplerState LinearSampler : register(s2);

// =============================================================================
// Vertex / instance data (must match C++ FRTVertex / FInstanceInfo)
// =============================================================================

struct FRTVertex {
    float3 Position;
    float  Padding0;
    float3 Normal;
    float  Padding1;
    float2 UV;
    float2 Padding2;
};

struct FInstanceInfo {
    uint   VertexOffset;
    uint   IndexOffset;
    uint   VertexCount;
    uint   IndexCount;
    float3 AlbedoColor;
    uint   AlbedoTextureIndex;
    uint   MaterialFlags;
    uint3  Padding;
};

// =============================================================================
// Luminance helper
// =============================================================================

float Luminance(float3 rgb) {
    return dot(rgb, float3(0.2126, 0.7152, 0.0722));
}

// =============================================================================
// Constants
// =============================================================================

static const float k_PI          = 3.14159265;
static const float k_INV_PI      = 1.0 / 3.14159265;
static const float k_PI_OVER_2   = 1.57079633;
static const float k_PI_OVER_4   = 0.78539816;

// =============================================================================
// Random Number Generation (PCG-style, per-dimension seeds)
// =============================================================================

uint Hash32(uint seed) {
    uint s = seed * 747796405u + 2891336453u;
    uint word = ((s >> ((s >> 28u) + 4u)) ^ s) * 277803737u;
    return (word >> 22u) ^ word;
}

float random_float(inout uint seed) {
    seed += 0x9E3779B9u;
    return float(Hash32(seed)) / float(0xFFFFFFFFu);
}

float2 random_float2(inout uint seed) {
    float x = random_float(seed);
    float y = random_float(seed);
    return float2(x, y);
}

// =============================================================================
// 2D Sampling (concentric disk, cosine-weighted hemisphere)
// =============================================================================

float2 ConcentricDiskSample(float2 u) {
    float2 uOffset = 2.0f * u - 1.0f;
    if (abs(uOffset.x) < 1e-10f && abs(uOffset.y) < 1e-10f)
        return float2(0.0f, 0.0f);

    float radius, theta;
    if (abs(uOffset.x) > abs(uOffset.y)) {
        radius = uOffset.x;
        theta = k_PI_OVER_4 * (uOffset.y / uOffset.x);
    } else {
        radius = uOffset.y;
        theta = k_PI_OVER_2 - k_PI_OVER_4 * (uOffset.x / uOffset.y);
    }
    return radius * float2(cos(theta), sin(theta));
}

float3 sampleHemisphereCosine(float3 normal, float2 u) {
    float2 d = ConcentricDiskSample(u);
    float z = sqrt(max(0.0f, 1.0f - dot(d, d)));

    float3 up = (abs(normal.z) < 0.999f) ? float3(0.0f, 0.0f, 1.0f) : float3(1.0f, 0.0f, 0.0f);
    float3 tangent = normalize(cross(up, normal));
    float3 bitangent = cross(normal, tangent);

    return tangent * d.x + bitangent * d.y + normal * z;
}

float DiffusePDF(float NdotL) {
    return max(NdotL, 0.0f) * k_INV_PI;
}

float3 EvalDiffuseBRDF(float3 albedo, float NdotL) {
    return albedo * max(NdotL, 0.0f) * k_INV_PI;
}

// =============================================================================
// Ray-Origin Offset (signed to avoid self-intersection on either side)
// =============================================================================

float3 OffsetRayOrigin(float3 position, float3 normal, float3 direction) {
    float3 offset = normal * g_GI.Params2.y;
    if (dot(normal, direction) < 0.0f)
        offset = -offset;
    return position + offset;
}

// =============================================================================
// Environment Sampling
// =============================================================================

float3 SampleSky(float3 direction) {
    float3 sunDir = normalize(g_GI.LightDir.xyz);
    float sunDot = dot(direction, sunDir);

    float3 zenithColor = float3(0.4, 0.6, 1.0);
    float3 horizonColor = float3(0.7, 0.8, 0.9);
    float3 groundColor = float3(0.15, 0.12, 0.1);

    float3 skyColor = (direction.y > 0.0)
        ? lerp(horizonColor, zenithColor, direction.y)
        : lerp(horizonColor, groundColor, -direction.y);

    float sunDisk = pow(max(sunDot, 0.0), 512.0);
    float3 sunColor = float3(1.0, 0.95, 0.8) * g_GI.LightDir.w;
    float sunGlow = pow(max(sunDot, 0.0), 6.0) * 0.15;
    float skyIntensity = 0.5;

    return (skyColor + sunColor * sunDisk + sunColor * sunGlow) * skyIntensity;
}

// =============================================================================
// Light sampling
// =============================================================================

// Generalized power-heuristic MIS weight: w_a = pdfA^p / (pdfA^p + pdfB^p)
float MISWeight(float pdfA, float pdfB, float power) {
    float pA = pow(max(pdfA, 1e-10f), power);
    float pB = pow(max(pdfB, 1e-10f), power);
    return pA / (pA + pB + 1e-10f);
}

// Sample a point on the light and return raw emission Li (without BRDF cosine).
// For area lights this consumes two random numbers from `seed`.
float3 SampleLight(FLight light, float3 origin, inout uint seed,
                   out float3 outL, out float pdfNEE, out float outDistance) {
    pdfNEE = 1.0f;
    outL = float3(0.0f, 0.0f, 0.0f);
    outDistance = 1e20f;

    switch (light.type) {
        case Directional: {
            outL = normalize(light.direction);
            return light.color * light.intensity;
        }

        case Point: {
            float3 toLight = light.position - origin;
            float r2 = max(dot(toLight, toLight), 1e-6f);
            float r = sqrt(r2);
            outL = toLight / r;
            outDistance = r;
            float atten = (r < light.range) ? (1.0f / r2) : 0.0f;
            return light.color * light.intensity * atten;
        }

        case Spot: {
            float3 toLight = light.position - origin;
            float r2 = max(dot(toLight, toLight), 1e-6f);
            float r = sqrt(r2);
            outL = toLight / r;
            outDistance = r;
            float atten = (r < light.range) ? (1.0f / r2) : 0.0f;

            float3 spotDir = normalize(light.direction);
            float cosTheta = dot(-outL, spotDir);
            float cosInner = cos(light.innerConeAngle * 0.5f);
            float cosOuter = cos(light.outerConeAngle * 0.5f);
            float spotAtten = smoothstep(cosOuter, cosInner, cosTheta);

            return light.color * light.intensity * atten * spotAtten;
        }

        case Area: {
            float3 lightNormal = normalize(light.direction);
            float3 up = (abs(lightNormal.y) < 0.999f) ? float3(0.0f, 1.0f, 0.0f) : float3(1.0f, 0.0f, 0.0f);
            float3 tangent = normalize(cross(up, lightNormal));
            float3 bitangent = cross(lightNormal, tangent);

            // Uniform sample on the rectangle area.
            float2 uv = random_float2(seed) - 0.5f;
            float3 samplePos = light.position
                + tangent * uv.x * light.areaWidth
                + bitangent * uv.y * light.areaHeight;

            float3 toLight = samplePos - origin;
            float r2 = max(dot(toLight, toLight), 1e-6f);
            float r = sqrt(r2);
            outL = toLight / r;
            outDistance = r;

            float cosThetaLight = max(dot(-outL, lightNormal), 0.0f);
            float area = light.areaWidth * light.areaHeight;
            pdfNEE = r2 / (area * cosThetaLight + 1e-6f);

            return light.color * light.intensity;
        }
    }

    return float3(0.0f, 0.0f, 0.0f);
}

// Solid-angle pdf for an area light in direction `wi` from `origin`.
bool AreaLightPdf(FLight light, float3 origin, float3 wi, out float outPdf, out float outDistance) {
    outPdf = 0.0f;
    outDistance = 0.0f;

    float3 lightNormal = normalize(light.direction);
    float denom = dot(wi, lightNormal);
    if (denom >= 0.0f)
        return false;

    float t = dot(light.position - origin, lightNormal) / denom;
    if (t <= 0.0f)
        return false;

    float3 hitPoint = origin + wi * t;
    float3 up = (abs(lightNormal.y) < 0.999f) ? float3(0.0f, 1.0f, 0.0f) : float3(1.0f, 0.0f, 0.0f);
    float3 tangent = normalize(cross(up, lightNormal));
    float3 bitangent = cross(lightNormal, tangent);

    float2 uv = float2(dot(hitPoint - light.position, tangent), dot(hitPoint - light.position, bitangent));
    if (abs(uv.x) > light.areaWidth * 0.5f || abs(uv.y) > light.areaHeight * 0.5f)
        return false;

    float cosThetaLight = max(dot(-wi, lightNormal), 0.0f);
    float area = light.areaWidth * light.areaHeight;
    outPdf = (t * t) / (area * cosThetaLight + 1e-6f);
    outDistance = t;
    return true;
}

float TraceShadowRay(float3 origin, float3 direction, float tMin, float tMax);

// Single direct-lighting estimate with NEE + optional BSDF sampling / MIS.
// Supports single-light selection (pdfSelect = 1 / lightCount) to stay O(1) in light count.
float3 EstimateDirectLighting(float3 origin, float3 normal, float3 albedo,
                              inout uint seed) {
    float3 Ld = float3(0.0f, 0.0f, 0.0f);

    uint lightCount = (uint)g_GI.Params3.w;
    if (lightCount == 0u || g_GI.Params4.x < 0.5f)
        return Ld;

    bool singleLight = g_GI.Params4.z > 0.5f;
    bool bsdfDirect  = g_GI.Params4.w > 0.5f;
    float misPower   = g_GI.Params4.y;
    float selectionPdf = 1.0f;
    if (singleLight && lightCount > 1u)
        selectionPdf = 1.0f / float(lightCount);

    uint startLi = 0u;
    uint endLi   = lightCount;
    if (singleLight && lightCount > 1u) {
        float r = random_float(seed);
        startLi = min((uint)(r * float(lightCount)), lightCount - 1u);
        endLi   = startLi + 1u;
    }

    for (uint li = startLi; li < endLi; ++li) {
        FLight light = Lights[li];

        // ----- NEE: sample a point/direction on the light -----
        float3 L;
        float lightDistance;
        float lightPdf;
        float3 Li = SampleLight(light, origin, seed, L, lightPdf, lightDistance);
        if (length(Li) > 1e-5f && lightPdf > 1e-6f) {
            float NdotL = max(dot(normal, L), 0.0f);
            if (NdotL > 0.0f || light.type == Directional) {
                float3 brdf = EvalDiffuseBRDF(albedo, NdotL);
                float bsdfPdf = DiffusePDF(NdotL);
                float wNEE = 1.0f;
                if (light.type == Area)
                    wNEE = MISWeight(lightPdf, bsdfPdf, misPower);

                float shadowTMax = min(lightDistance - g_GI.Params2.y, g_GI.Params.w);
                shadowTMax = max(shadowTMax, g_GI.Params.z + 1e-4f);
                float rawVisibility = TraceShadowRay(OffsetRayOrigin(origin, normal, L), L,
                                                     g_GI.Params.z, shadowTMax);
                float visibility = lerp(1.0f, rawVisibility, g_GI.Params2.w);

                Ld += (brdf * Li * visibility * wNEE / lightPdf) / selectionPdf;
            }
        }

        // ----- BSDF-sampled direct hit (with MIS) for area lights -----
        if (bsdfDirect && light.type == Area) {
            float2 u = random_float2(seed);
            float3 wi = sampleHemisphereCosine(normal, u);
            float NdotL2 = max(dot(normal, wi), 0.0f);
            if (NdotL2 > 0.0f) {
                float tLight, pdfLightBSDF;
                if (AreaLightPdf(light, origin, wi, pdfLightBSDF, tLight)) {
                    float3 brdfBSDF = EvalDiffuseBRDF(albedo, NdotL2);
                    float bsdfPdf2 = DiffusePDF(NdotL2);
                    float wBSDF = MISWeight(bsdfPdf2, pdfLightBSDF, misPower);

                    float shadowTMaxBSDF = max(tLight - g_GI.Params2.y, g_GI.Params.z + 1e-4f);
                    float rawVisBSDF = TraceShadowRay(OffsetRayOrigin(origin, normal, wi), wi,
                                                      g_GI.Params.z, shadowTMaxBSDF);
                    float visibilityBSDF = lerp(1.0f, rawVisBSDF, g_GI.Params2.w);

                    Ld += (brdfBSDF * Li * visibilityBSDF * wBSDF / bsdfPdf2) / selectionPdf;
                }
            }
        }
    }

    return Ld;
}

float TraceShadowRay(float3 origin, float3 direction, float tMin, float tMax) {
    ShadowPayload shadowPayload;
    shadowPayload.occluded = true;

    RayDesc shadowRay;
    shadowRay.Origin = origin;
    shadowRay.Direction = direction;
    shadowRay.TMin = tMin;
    shadowRay.TMax = tMax;

    TraceRay(
        SceneBVH,
        RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER | RAY_FLAG_FORCE_OPAQUE,
        0xFF, 0, 0, 1,
        shadowRay,
        shadowPayload);

    return shadowPayload.occluded ? 0.0 : 1.0;
}

// =============================================================================
// Ray Generation Shader
// =============================================================================

[shader("raygeneration")]
void RayGen() {
    uint2 pixel = DispatchRaysIndex().xy;

    float3 worldPos = GBufferWorldPos[pixel].rgb;
    float3 normal = normalize(GBufferNormal[pixel].rgb * 2.0 - 1.0);
    float3 diffuse = GBufferMaterial[pixel].rgb;

    if (length(worldPos) < 0.001) {
        Output[pixel] = float4(0.0, 0.0, 0.0, 1.0);
        return;
    }



    uint spp         = (uint)g_GI.Params.y;
    uint maxBounces  = (uint)g_GI.Params.x;
    float ambientScale = g_GI.Params2.x;

    uint pixelSeed = pixel.x * 73856093u + pixel.y * 19349663u + (uint)g_View.FrameIndex * 83492791u;

    // Primary-surface direct lighting with ky-style NEE + optional BSDF/MIS.
    float3 primaryDirect = float3(0.0f, 0.0f, 0.0f);
    if (g_GI.Params4.x > 0.5f) {
        uint seed = pixelSeed;
        primaryDirect = EstimateDirectLighting(worldPos, normal, diffuse, seed);
    }

    float3 primaryAmbient = diffuse * g_GI.AmbientColor.rgb * ambientScale;
    float3 result = primaryDirect + primaryAmbient;

    // Multi-sample indirect GI
    float3 indirect = float3(0.0f, 0.0f, 0.0f);
    float avgFirstHitDist = 0.0f;

#if GI_DEBUG_STATS
    float debugTotalBounces = 0.0f;
    float debugTotalSamples = 0.0f;
    float debugRRTerminated = 0.0f;
    float debugPrimaryHits = 0.0f;
    float debugTotalRadianceLuma = 0.0f;
#endif

    for (uint s = 0; s < spp; ++s) {
        GIPayload payload;
        payload.radiance = float3(0.0f, 0.0f, 0.0f);
        payload.throughput = float3(1.0f, 1.0f, 1.0f);
        payload.bounceCount = 0u;
        payload.flags = 0u;
        payload.hitDistance = 0.0f;
        payload.seed = pixelSeed + s * 7919u;

        uint sampleSeed = pixelSeed + s * 7919u;
        float3 rayDir = sampleHemisphereCosine(normal, random_float2(sampleSeed));
        float3 rayOrigin = OffsetRayOrigin(worldPos, normal, rayDir);

        float firstHitDist = 0.0f;
        for (uint bounce = 0; bounce < maxBounces; ++bounce) {
            RayDesc ray;
            ray.Origin = rayOrigin;
            ray.Direction = rayDir;
            ray.TMin = g_GI.Params2.y;
            ray.TMax = g_GI.Params2.z;

            TraceRay(
                SceneBVH,
                RAY_FLAG_FORCE_OPAQUE,
                0xFF, 0, 0, 0,
                ray,
                payload);

            if (bounce == 0)
                firstHitDist = payload.hitDistance;

            if ((payload.flags & 0x01) == 0)
                break;

            // Direct lighting at the bounce vertex (NEE + BSDF MIS).
            // payload.throughput already includes the surface albedo from ClosestHit.
            if (g_GI.Params4.x > 0.5f) {
                uint bounceSeed = pixelSeed + s * 7919u + (bounce + 1) * 1733u;
                indirect += EstimateDirectLighting(
                    payload.origin, payload.hitNormal, payload.throughput,
                    bounceSeed);
            }

            rayOrigin = payload.origin;
            rayDir = payload.direction;
            payload.bounceCount = bounce + 1;
        }

        indirect += payload.radiance;
        avgFirstHitDist += firstHitDist;

#if GI_DEBUG_STATS
        debugTotalBounces += float(payload.bounceCount);
        debugTotalSamples += 1.0f;
        if ((payload.flags & 0x02) != 0)
            debugRRTerminated += 1.0f;
        if (payload.hitDistance > 0.0f)
            debugPrimaryHits += 1.0f;
        debugTotalRadianceLuma += Luminance(payload.radiance);
#endif
    }

    result += indirect / max(float(spp), 1.0f);
    avgFirstHitDist /= max(float(spp), 1.0f);

    // Safety clamp: mark NaN/inf pixels red for debugging.
    if (any(isnan(result)) || any(isinf(result)))
        result = float3(10.0f, 0.0f, 0.0f);
    if (isnan(avgFirstHitDist) || isinf(avgFirstHitDist))
        avgFirstHitDist = 0.0f;

    // Debug visualisation modes (r_GI_DebugMode).
    uint debugMode = (uint)(g_GI.Params5.x + 0.5f);
    float3 debugColor = result;
    if (debugMode != 0u) {
        GIPayload dbgPayload;
        dbgPayload.radiance = float3(0.0f, 0.0f, 0.0f);
        dbgPayload.throughput = float3(1.0f, 1.0f, 1.0f);
        dbgPayload.bounceCount = 0u;
        dbgPayload.flags = 0u;
        dbgPayload.hitDistance = 0.0f;
        dbgPayload.seed = pixelSeed;

        uint dbgSeed = pixelSeed;
        float3 dbgDir = sampleHemisphereCosine(normal, random_float2(dbgSeed));
        float3 dbgOrigin = OffsetRayOrigin(worldPos, normal, dbgDir);

        RayDesc dbgRay;
        dbgRay.Origin = dbgOrigin;
        dbgRay.Direction = dbgDir;
        dbgRay.TMin = g_GI.Params2.y;
        dbgRay.TMax = g_GI.Params2.z;
        TraceRay(SceneBVH, RAY_FLAG_FORCE_OPAQUE, 0xFF, 0, 0, 0, dbgRay, dbgPayload);

        switch (debugMode) {
            case 1u:  debugColor = diffuse; break;
            case 2u:  debugColor = normal * 0.5f + 0.5f; break;
            case 3u:  debugColor = primaryDirect; break;
            case 4u:  debugColor = indirect / max(float(spp), 1.0f); break;
            case 5u:  debugColor = float3(avgFirstHitDist, avgFirstHitDist, avgFirstHitDist) * 0.1f; break;
            case 6u:  {
                float3 vn = dbgPayload.debugVertexNormal;
                if (any(isnan(vn)) || any(isinf(vn)) || length(vn) < 1e-6f)
                    debugColor = float3(1.0f, 0.0f, 1.0f);
                else
                    debugColor = normalize(vn) * 0.5f + 0.5f;
                break;
            }
            case 7u:  {
                float3 gn = dbgPayload.debugGeoNormal;
                if (any(isnan(gn)) || any(isinf(gn)) || length(gn) < 1e-6f)
                    debugColor = float3(1.0f, 0.0f, 1.0f);
                else
                    debugColor = normalize(gn) * 0.5f + 0.5f;
                break;
            }
            case 8u:  {
                if (dbgPayload.debugNormalFlags & 0x02u)
                    debugColor = float3(1.0f, 0.0f, 0.0f); // safety net triggered
                else if (dbgPayload.debugNormalFlags & 0x01u)
                    debugColor = float3(0.0f, 1.0f, 0.0f); // geometric fallback triggered
                else
                    debugColor = float3(0.0f, 0.0f, 1.0f); // vertex normal valid
                break;
            }
            default: break;
        }
    }

    // Write raw HDR radiance; temporal accumulation + tonemap are done in a separate pass.
    Output[pixel] = float4(debugColor, avgFirstHitDist);

#if GI_DEBUG_STATS
    if (g_GI.Params3.z > 0.5f && debugTotalSamples > 0.0f) {
        DebugStatsTexture[pixel] = float4(
            debugTotalBounces / debugTotalSamples,
            1.0f - debugRRTerminated / debugTotalSamples,
            debugPrimaryHits / debugTotalSamples,
            debugTotalRadianceLuma / debugTotalSamples);
    }
#endif
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
    payload.hitDistance = hitT;

    uint instanceIdx = InstanceIndex();
    uint primitiveIdx = PrimitiveIndex();

    FInstanceInfo info = RTInstanceInfo[instanceIdx];

    uint i0 = RTIndices[info.IndexOffset + primitiveIdx * 3 + 0];
    uint i1 = RTIndices[info.IndexOffset + primitiveIdx * 3 + 1];
    uint i2 = RTIndices[info.IndexOffset + primitiveIdx * 3 + 2];

    FRTVertex v0 = RTVertices[info.VertexOffset + i0];
    FRTVertex v1 = RTVertices[info.VertexOffset + i1];
    FRTVertex v2 = RTVertices[info.VertexOffset + i2];

    float3 bary = float3(1.0 - attr.barycentrics.x - attr.barycentrics.y, attr.barycentrics.x, attr.barycentrics.y);

    float3 localNormal = v0.Normal * bary.x + v1.Normal * bary.y + v2.Normal * bary.z;
    float3 vertexNormal = localNormal;
    payload.debugVertexNormal = vertexNormal;
    float3 hitNormal = normalize(vertexNormal);

    float3 p0w = mul(ObjectToWorld3x4(), float4(v0.Position, 1.0)).xyz;
    float3 p1w = mul(ObjectToWorld3x4(), float4(v1.Position, 1.0)).xyz;
    float3 p2w = mul(ObjectToWorld3x4(), float4(v2.Position, 1.0)).xyz;
    float3 geoNormal = cross(p1w - p0w, p2w - p0w);
    payload.debugGeoNormal = geoNormal;

    uint normalFlags = 0u;

    // Fallback to geometric normal if the interpolated normal is invalid.
    if (any(isnan(hitNormal)) || any(isinf(hitNormal))) {
        normalFlags |= 0x01u;
        hitNormal = normalize(geoNormal);
    }

    // Absolute safety net: any remaining NaN/inf would poison the path.
    if (any(isnan(hitNormal)) || any(isinf(hitNormal))) {
        normalFlags |= 0x02u;
        hitNormal = float3(0.0f, 1.0f, 0.0f);
    }

    payload.debugNormalFlags = normalFlags;
    payload.hitNormal = hitNormal;



    float3 albedo = info.AlbedoColor;

    // Direct lighting is handled exclusively by EstimateDirectLighting in RayGen.
    payload.throughput *= albedo;

    // Russian Roulette path termination
    if (g_GI.Params3.x > 0.5f) {
        float rrProb = max(payload.throughput.r, max(payload.throughput.g, payload.throughput.b));
        float rrMinSurvival = g_GI.Params3.y;
        rrProb = clamp(rrProb, rrMinSurvival, 1.0f);

        uint rrSeed = payload.seed + 101u;
        if (random_float(rrSeed) > rrProb) {
            payload.flags &= ~0x01;  // terminate path
            payload.flags |= 0x02;   // mark RR termination
            return;
        }
        payload.throughput /= rrProb;
    }

    payload.seed = payload.seed + payload.bounceCount * 13u + 17u;
    payload.direction = sampleHemisphereCosine(hitNormal, random_float2(payload.seed));
    payload.origin = OffsetRayOrigin(hitPosition, hitNormal, payload.direction);

    payload.flags |= 0x01;
}

// =============================================================================
// Miss Shaders
// =============================================================================

[shader("miss")]
void Miss(inout GIPayload payload : SV_RayPayload) {
    float3 skyRadiance = SampleSky(WorldRayDirection());
    payload.radiance += payload.throughput * skyRadiance;
    payload.flags &= ~0x01;
}

[shader("miss")]
void ShadowMiss(inout ShadowPayload payload : SV_RayPayload) {
    payload.occluded = false;
}
