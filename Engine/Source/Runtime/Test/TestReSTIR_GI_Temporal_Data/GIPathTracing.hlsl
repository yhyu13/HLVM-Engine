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

// Compact 64-byte payload (same layout as the proven TestCornellBoxGI shader).
// Every field is both written and read on BOTH sides of the TraceRay boundary
// (raygen and closesthit). slangc compiles each entry point independently and
// can dead-strip payload fields that a given entry never uses; if that happens
// asymmetrically the two shaders disagree on the payload layout and every
// closesthit -> raygen value arrives as garbage (observed as red/black noise
// GI with valid IDs/barycentrics but corrupt normals and albedos).
struct GIPayload {
    float3 throughput;
    float3 radiance;
    float3 origin;
    float3 direction;
    float  hitDistance;
    uint   bounceCount;
    uint   flags;            // bit 0 = continue path, bit 1 = terminated by RR
    uint   seed;
};

struct ShadowPayload {
    // Use uint instead of bool: HLSL bool is implementation-defined in ray payloads
    // and has caused per-pixel corruption on some Vulkan drivers.
    uint occluded;
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

RWTexture2D<float4> Output : register(u0, space1);

// Primary sample ray direction (ReSTIR GI reservoirs). Written per-pixel as
// float4(direction, 1.0). The direction is the ray used for the pixel's
// primary GI sample so the reservoir can reproject/merge a real sample.
RWTexture2D<float4> OutputDirection : register(u2, space1);

#if GI_DEBUG_STATS
RWTexture2D<float4> DebugStatsTexture : register(u1, space1);
#endif

RaytracingAccelerationStructure SceneBVH : register(t0);

Texture2D<float4> GBufferWorldPos   : register(t1);
Texture2D<float4> GBufferNormal     : register(t2);
Texture2D<float4> GBufferMaterial   : register(t3);

// Per-texel bounce albedo textures (2026-08-10 material rework, Phase 3b):
// one slot per unique Sponza material, indexed by
// RTInstanceInfo.AlbedoTextureIndex. Bound t9..t40; slots without a texture
// must be bound to a white placeholder (the shader falls back to the
// instance's average AlbedoColor when the flag/index is invalid).
#define MAX_MATERIAL_TEXTURES 32
Texture2D<float4> MaterialTextures[MAX_MATERIAL_TEXTURES] : register(t9);

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
    float  Roughness;   // gltf roughnessFactor (2026-08-10 Phase 2)
    float  Metallic;
    uint   Pad;
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

// Phase 2 (2026-08-10): roughness-aware indirect lobe. roughness=1 → pure
// cosine hemisphere (Lambert); roughness→0 concentrates samples near the
// normal (glossier bounce). Crude (no PDF correction) — acceptable while the
// tracer uses unweighted throughput; replace with GGX sampling when specular
// lands. Sponza's gltf roughness ≈ 0.9, so the current scene is near-Lambert.
float3 sampleRoughnessLobe(float3 normal, float roughness, float2 u) {
    float3 cosineDir = sampleHemisphereCosine(normal, u);
    float glossy = 1.0f - saturate(roughness);
    return normalize(lerp(cosineDir, normal, glossy * glossy));
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
    shadowPayload.occluded = 1u;

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

    return shadowPayload.occluded != 0u ? 0.0 : 1.0;
}

// =============================================================================
// Ray Generation Shader
// =============================================================================

[shader("raygeneration")]
void RayGen() {
    uint2 pixel = DispatchRaysIndex().xy;

    // Phase D: the tracer dispatches at HALF resolution; scale the dispatch
    // pixel back to the full-res GBuffer before reading geometry.
    float2 gbScale = g_View.RenderTargetSize.xy / DispatchRaysDimensions().xy;
    int2 gbPixel = int2((float2(pixel) + 0.5f) * gbScale);

    float3 worldPos = GBufferWorldPos[gbPixel].rgb;
    float3 normal = normalize(GBufferNormal[gbPixel].rgb * 2.0 - 1.0);
    float3 diffuse = GBufferMaterial[gbPixel].rgb;

    // Debug-only bypass of the no-geometry early-return (modes 20/21/22/30/31
    // read the GBuffer directly). Compile-gated behind HLVM_RGI_DEBUG_VIS.
#ifdef HLVM_RGI_DEBUG_VIS
    uint debugModeEarly = (uint)(g_GI.Params5.x + 0.5f);
    bool bypassEarlyReturn = (debugModeEarly == 20u
                           || debugModeEarly == 21u
                           || debugModeEarly == 22u
                           || debugModeEarly == 30u
                           || debugModeEarly == 31u);
#else
    bool bypassEarlyReturn = false;
#endif

    if (!bypassEarlyReturn && length(worldPos) < 0.001) {
        // Background sky for pixels with no rasterized geometry (fixed
        // 2026-08-09): was pure black, making 56% of the display black.
        // Unproject the primary ray through this pixel (GLM RH-ZO) and
        // evaluate the same SampleSky used by the miss shader, so the
        // background is consistent with the GI sky.
        float2 px = float2(gbPixel) + 0.5f;
        float2 ndc = float2(px.x / g_View.RenderTargetSize.x * 2.0f - 1.0f,
                            1.0f - px.y / g_View.RenderTargetSize.y * 2.0f);
        // Diagonal-only unprojection: clip.x = P00*view.x, clip.y = P11*view.y.
        // P00/P11 are on the diagonal, so the row/column-major memory layout
        // of the GLM matrix does not matter for these two elements.
        float3 viewDir = float3(ndc.x / g_View.ProjMatrix[0][0],
                                ndc.y / g_View.ProjMatrix[1][1],
                                -1.0f);
        // mul(vec, M) = M^T * vec: the view rotation is orthonormal, so this
        // maps the view-space direction back to world space (translation is
        // killed by the w=0 component).
        float3 worldDir = normalize(mul(float4(viewDir, 0.0f), g_View.ViewMatrix).xyz);
        Output[pixel] = float4(SampleSky(worldDir), 1.0f);
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
    // Direction of the first sample's primary ray — used by ReSTIR GI to
    // build a reservoir that holds a real (radiance, direction, hitT) sample.
    float3 firstSampleDir = float3(0.0f, 1.0f, 0.0f);

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
        float roughness = GBufferMaterial[gbPixel].a;  // Phase 2 (GBuffer MRT2 alpha)
        float3 rayDir = sampleRoughnessLobe(normal, roughness, random_float2(sampleSeed));
        if (s == 0)
            firstSampleDir = rayDir;
        float3 rayOrigin = OffsetRayOrigin(worldPos, normal, rayDir);

        // Fully initialize every payload field: slangc compiles entry points
        // independently and can dead-strip fields an entry never touches,
        // which would desync the payload layout across the TraceRay boundary.
        payload.origin = rayOrigin;
        payload.direction = rayDir;

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

            // Direct lighting at each bounce vertex is evaluated inside
            // ClosestHit (valid local normal, keeps the payload compact)
            // and lands in payload.radiance.

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

    // Emit the primary sample ray direction for ReSTIR GI reservoir building.
    // Done unconditionally so downstream reservoirs always have a valid sample.
    OutputDirection[pixel] = float4(firstSampleDir, 1.0);

    // Safety clamp: mark NaN/inf pixels red for debugging.
    if (any(isnan(result)) || any(isinf(result)))
        result = float3(10.0f, 0.0f, 0.0f);
    if (isnan(avgFirstHitDist) || isinf(avgFirstHitDist))
        avgFirstHitDist = 0.0f;

#ifdef HLVM_RGI_DEBUG_VIS
    // Debug visualisation modes (r_GI_DebugMode). Compile-time gated behind
    // HLVM_RGI_DEBUG_VIS (2026-08-11): these 12 sentinel modes + the alpha
    // alive-sentinel were always-compiled, violating the Do-Not-Repeat rule
    // "debug visualisations must not survive the iteration". They also
    // override the spec-declared alpha semantic (avgFirstHitDist). Off by
    // default; re-enable with -D HLVM_RGI_DEBUG_VIS when debugging.
    uint debugMode = (uint)(g_GI.Params5.x + 0.5f);
    float3 debugColor = result;
    if (debugMode != 0u) {
        switch (debugMode) {
            case 1u:  debugColor = diffuse; break;
            case 2u:  debugColor = normal * 0.5f + 0.5f; break;
            case 3u:  debugColor = primaryDirect; break;
            case 4u:  debugColor = indirect / max(float(spp), 1.0f); break;
            case 5u:  debugColor = float3(avgFirstHitDist, avgFirstHitDist, avgFirstHitDist) * 0.1f; break;
            // v13 (six-role-pipeline, 2026-07-27): UAV-write sentinel. Writes
            // a UNIQUE, recognizable per-pixel constant (1.0, 0.0, 1.0) to
            // OutputTexture at the very start of the write, BEFORE any other
            // code. If gi_raw with HLVM_PT_DEBUG_MODE=6 shows magenta-like
            // values, the dispatch body is running and the UAV write is
            // landing in the texture. The bug is then in the lighting/payload
            // math downstream of this line. If gi_raw with mode=6 shows 0,
            // the dispatch is not running or the UAV write is being dropped
            // (desc-barrier, descriptor mismatch, no dispatch at all).
            case 6u:  debugColor = float3(float(pixel.x) / 256.0, 0.0, float(pixel.y) / 256.0); break;
            // v17 (six-role-pipeline, 2026-07-27): TraceRay-bypass sentinel.
            // If case 6u shows per-pixel gradient AND case 7u shows non-zero
            // scene-shape output, the entire non-ray-tracing pipeline works.
            // Bug is then constrained to TraceRay / payload / SRV-read chain.
            // If case 7u shows 0 or garbage, bug is in the post-TraceRay code
            // path (lighting math, payload write, accumulate). Uses the same
            // diffuse * g_GI.AmbientColor.rgb * ambientScale expression the
            // primary contribution uses (GIPathTracing.hlsl:486), so a non-zero
            // result is meaningful: it shows what the shader produces when
            // ray-tracing is bypassed. Predicted: mode 7 = mode 1 * 1.5.
            case 7u:  debugColor = diffuse * g_GI.AmbientColor.rgb * ambientScale; break;
// v18 (six-role-pipeline, 2026-07-27): TraceRay-only sentinel.
// Calls TraceRay with the same ray setup as the main loop (TMin/TMax
// from g_GI.Params2.y/.z, RAY_FLAG_FORCE_OPAQUE, 0xFF/0/0/0) but
// discards the payload results. If mode 8 crashes or produces
// garbage, the bug is in the TraceRay setup itself (RT flags,
// TMin/TMax, BVH traversal). If mode 8 produces a clean frame
// (i.e., the test doesn't crash and gi_raw isn't all-NaN), the
// ray-tracing setup is healthy; the bug is in the payload/result
// merge downstream.
            case 8u:
            {
                GIPayload tracePayload;
                tracePayload.radiance = float3(0.0f, 0.0f, 0.0f);
                tracePayload.throughput = float3(1.0f, 1.0f, 1.0f);
                tracePayload.bounceCount = 0u;
                tracePayload.flags = 0u;
                tracePayload.hitDistance = 0.0f;
                tracePayload.seed = pixelSeed;
                // Re-derive a ray from the primary surface (case 8 runs outside
                // the SPP loop, so rayDir/rayOrigin aren't in scope here).
                float3 sentinelDir = sampleHemisphereCosine(normal, float2(0.5f, 0.5f));
                float3 sentinelOrigin = OffsetRayOrigin(worldPos, normal, sentinelDir);
                tracePayload.origin = sentinelOrigin;
                tracePayload.direction = sentinelDir;
                RayDesc traceRay;
                traceRay.Origin = sentinelOrigin;
                traceRay.Direction = sentinelDir;
                traceRay.TMin = g_GI.Params2.y;
                traceRay.TMax = g_GI.Params2.z;
                TraceRay(SceneBVH, RAY_FLAG_FORCE_OPAQUE, 0xFF, 0, 0, 0, traceRay, tracePayload);
                debugColor = float3(tracePayload.hitDistance > 0.0f ? 1.0f : 0.0f,
                                    tracePayload.hitDistance * 0.1f,
                                    float(tracePayload.flags) / 8.0f);
                break;
            }
// v18: diffuse-only sentinel (mode 9 = mode 1 * 1.5). Verifies
// GBufferMaterial SRV independently of the AmbientColor/AmbientScale
// uniforms. If mode 9 produces a scene-shape image identical to mode 1,
// GBufferMaterial SRV is healthy. If mode 9 = 0, GBufferMaterial SRV
// has a binding issue. If mode 9 differs from mode 1 * 1.5, the
// multiplier is wrong (unlikely).
            case 9u:  debugColor = diffuse * 1.5f; break;
// v18: debugMode cbuffer reach sentinel. Writes g_GI.Params5.x
// (the debugMode uniform) directly to OutputTexture. If mode 10
// shows a recognizable value (e.g., ~0.04 for debugMode=10/256),
// the cbuffer reach is fine. If mode 10 = 0, the cbuffer is not
// bound or not being updated by FGIPass::WriteConstants. This is
// the canonical "is the C++-side constant-buffer update working"
// test.
            case 10u: debugColor = float3(g_GI.Params5.x / 256.0f, 0.0f, 0.0f); break;
// v18: View cbuffer reach sentinel. Writes g_View.FrameIndex /
// 256.0 to OutputTexture. Tests whether the ViewConstants cbuffer
// (b1) is bound. If mode 11 shows a non-zero value, View cbuffer
// reach is fine. If mode 11 = 0, ViewConstants has a binding issue.
            case 11u: debugColor = float3(g_View.FrameIndex / 256.0f, g_View.FrameIndex / 256.0f, g_View.FrameIndex / 256.0f); break;
// v19 (six-role-pipeline, 2026-07-27): AmbientColor-only sentinel.
// Writes ONLY g_GI.AmbientColor.rgb to OutputTexture (no diffuse,
// no ambientScale). If mode 12 = (1, 1, 1) per pixel, AmbientColor
// uniform is healthy. If mode 12 = 0, AmbientColor not bound.
// Combined with mode 7 (= mode 12 * diffuse * ambientScale) and
// mode 9 (= diffuse * 1.5), this fully bisects the uniform bind.
// Predicted: mode 12 = (1, 1, 1) since AmbientColor = (1, 1, 1, 1).
            case 12u: debugColor = g_GI.AmbientColor.rgb; break;
            case 13u: debugColor = RTInstanceInfo[0].AlbedoColor; break;         // SRV sanity read
            case 14u: debugColor = RTVertices[0].Position * 0.25f + 0.5f; break; // SRV sanity read
            case 20u: debugColor = GBufferMaterial.Load(int3(pixel, 0)).rgb; break; // SRV read of GBufferMaterial
            case 21u: debugColor = GBufferNormal.Load(int3(pixel, 0)).rgb * 0.5f + 0.5f; break; // SRV read of GBufferNormal (sanity compare to case 2)
            case 22u: debugColor = GBufferWorldPos.Load(int3(pixel, 0)).rgb * 0.25f + 0.5f; break; // SRV read of GBufferWorldPos (sanity compare)
// v128 (six-role-pipeline, tick 113, 2026-07-30): single-pixel sentinel
// at (0,0,0). If mode 30 shows albedo at (0,0,0) but mode 20 shows zero
// everywhere, the binding works at (0,0,0) but is masked elsewhere (e.g.,
// layout transition per ping-pong UAV/SRV). If mode 30 also shows zero,
// the binding is universally broken. Combined with mode 20/21/22 this
// discriminates "SRV universally broken" vs "SRV partially bound".
            case 30u:
            {
                float3 sentinelColor = GBufferMaterial.Load(int3(0, 0, 0)).rgb;
                if (any(sentinelColor > float3(0.001, 0.001, 0.001))) {
                    debugColor = float3(1.0, 0.0, 1.0); // magenta: binding works at (0,0,0)
                } else {
                    debugColor = float3(0.0, 0.0, 0.0); // black: binding universally broken
                }
                break;
            }
// v131 (six-role-pipeline, tick 151, 2026-07-30): slangc-dead-strip
// discriminator (Candidate A probe). Mode 31 reads GBufferMaterial with
// a non-trivial arithmetic transformation (r * 0.5 + 0.1) so the read
// result is observable to slangc's reachability analysis. If mode 31
// shows non-uniform color, the SRV reads are alive for mode 31 (rules
// out slangc dead-strip). If mode 31 still shows uniform zero, slangc
// IS dead-stripping the SRV reads (root cause is upstream: binding
// layout or pipeline state, not dead-strip).
            case 31u:
            {
                float3 aliveSentinel = GBufferMaterial.Load(int3(pixel, 0)).rgb * 0.5f + 0.1f;
                if (any(aliveSentinel > float3(0.1, 0.1, 0.1))) {
                    debugColor = aliveSentinel; // binding works, slangc keeps the read
                } else {
                    debugColor = float3(0.0, 0.0, 1.0); // blue: SRV read alive but value is zero (binding issue)
                }
                break;
            }
// v19: debugMode raw value (no /256 divide). Sanity check on mode 10.
// If mode 15 = 10.0, Params5.x is being set correctly. If mode 15 = 0,
// Params5.x is 0 (same as mode 10 = 0, cbuffer not updated). If mode
// 15 != 10 and != 0, Params5.x is being set to a wrong value.
            case 15u: debugColor = float3(g_GI.Params5.x, g_GI.Params5.x, g_GI.Params5.x); break;
// v19: default-case trace. If debugMode is some value not in {1..15}
// (e.g., 99) AND the switch is being entered, the default returns
// gray. If slangc dead-strips ALL case labels, this becomes the
// catch-all sentinel (every debugMode returns gray). If a valid
// debugMode is set, this default never runs (existing cases 1u-15u
// match first).
            default: debugColor = float3(0.5f, 0.5f, 0.5f); break;
        }
    }

    // Write raw HDR radiance; temporal accumulation + tonemap are done in a
    // separate pass. Debug mode overrides the color; the alpha stays
    // avgFirstHitDist except for the debug alive-sentinel below.
    Output[pixel] = float4(debugColor, avgFirstHitDist);

    // Debug alive-sentinel (compile-gated with the modes above).
    Output[pixel].w = max(Output[pixel].w, 0.99994f);
#else
    // Production path: raw HDR radiance, alpha = avgFirstHitDist (the spec
    // semantic). No debug sentinels survive.
    Output[pixel] = float4(result, avgFirstHitDist);
#endif

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

    float3 vertexNormal = v0.Normal * bary.x + v1.Normal * bary.y + v2.Normal * bary.z;
    float3 hitNormal = normalize(vertexNormal);

    // Fallback to the geometric normal if the interpolated normal is invalid.
    if (any(isnan(hitNormal)) || any(isinf(hitNormal))) {
        float3 p0w = mul(ObjectToWorld3x4(), float4(v0.Position, 1.0)).xyz;
        float3 p1w = mul(ObjectToWorld3x4(), float4(v1.Position, 1.0)).xyz;
        float3 p2w = mul(ObjectToWorld3x4(), float4(v2.Position, 1.0)).xyz;
        hitNormal = normalize(cross(p1w - p0w, p2w - p0w));
    }

    // Absolute safety net: any remaining NaN/inf would poison the path.
    if (any(isnan(hitNormal)) || any(isinf(hitNormal)))
        hitNormal = float3(0.0f, 1.0f, 0.0f);

    // Per-texel albedo from the instance's material texture (Phase 3b),
    // falling back to the texture's linear average (Phase 3) when the index
    // is out of range or the mesh is untextured.
    float3 albedo = info.AlbedoColor;
    if ((info.MaterialFlags & 1u) != 0u && info.AlbedoTextureIndex < MAX_MATERIAL_TEXTURES)
    {
        float2 hitUV = v0.UV * bary.x + v1.UV * bary.y + v2.UV * bary.z;
        // Explicit LOD (mip 0): ImageSampleImplicitLod is invalid in ray
        // tracing stages; SampleLevel compiles to the explicit-LOD variant.
        float3 texAlbedo = MaterialTextures[info.AlbedoTextureIndex].SampleLevel(LinearSampler, hitUV, 0.0f).rgb;
        if (!any(isnan(texAlbedo)) && !any(isinf(texAlbedo)) && length(texAlbedo) > 1e-6f)
            albedo = texAlbedo;
    }

    // Direct lighting at this bounce vertex (NEE + optional BSDF MIS), weighted
    // by the incoming path throughput. Evaluated here - not in RayGen - because
    // the valid hit normal only exists locally; this also keeps the payload
    // compact (no hitNormal field crossing the TraceRay boundary).
    if (g_GI.Params4.x > 0.5f) {
        uint directSeed = payload.seed + 401u;
        float3 direct = EstimateDirectLighting(hitPosition, hitNormal, albedo, directSeed);
        payload.radiance += payload.throughput * direct;
    }

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
    payload.direction = sampleRoughnessLobe(hitNormal, info.Roughness, random_float2(payload.seed));
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
    payload.occluded = 0u;
}
