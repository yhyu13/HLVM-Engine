/*
 * Copyright (c) 2026. MIT License. All rights reserved.
 *
 * Minimal path-tracing shader library for a single triangle.
 *
 * This is intentionally tiny: one ray generation shader, one closest-hit shader,
 * and one miss shader.  It exists to verify that the Vulkan ray-tracing pipeline
 * can hit a triangle and produce a stable image before layering on GI complexity.
 */

// Ray payload.  Color is written by the hit/miss shaders.
struct Payload
{
    float3 Color;
    float  HitT;
};

// Intersection attributes (barycentric coordinates).
struct Attributes
{
    float2 Barycentrics;
};

// TLAS bound at slot t0, output UAV at slot u1.
RaytracingAccelerationStructure SceneBVH : register(t0);
RWTexture2D<float4>             Output   : register(u1);

// Tiny cosine-weighted hemisphere sampling for a one-bounce "path trace".
float3 SampleHemisphereCosine(float3 normal, float2 u)
{
    float2 uv = 2.0f * u - 1.0f;
    float  radius, theta;
    if (abs(uv.x) > abs(uv.y))
    {
        radius = uv.x;
        theta = (3.14159265f / 4.0f) * (uv.y / uv.x);
    }
    else
    {
        radius = uv.y;
        theta = (3.14159265f / 2.0f) - (3.14159265f / 4.0f) * (uv.x / uv.y);
    }
    float2 d = radius * float2(cos(theta), sin(theta));
    float  z = sqrt(max(0.0f, 1.0f - dot(d, d)));

    float3 up = abs(normal.z) < 0.999f ? float3(0.0f, 0.0f, 1.0f) : float3(1.0f, 0.0f, 0.0f);
    float3 tangent = normalize(cross(up, normal));
    float3 bitangent = cross(normal, tangent);
    return tangent * d.x + bitangent * d.y + normal * z;
}

uint Hash32(uint seed)
{
    uint s = seed * 747796405u + 2891336453u;
    uint word = ((s >> ((s >> 28u) + 4u)) ^ s) * 277803737u;
    return (word >> 22u) ^ word;
}

float RandomFloat(inout uint seed)
{
    seed += 0x9E3779B9u;
    return float(Hash32(seed)) / float(0xFFFFFFFFu);
}

[shader("raygeneration")]
void RayGen()
{
    uint2 LaunchIndex = DispatchRaysIndex().xy;
    uint2 LaunchDimensions = DispatchRaysDimensions().xy;

    // Simple orthographic camera shooting rays in +Z from the z = 0 plane.
    float2 uv = (float2(LaunchIndex) + 0.5f) / float2(LaunchDimensions);
    uv = uv * 2.0f - 1.0f;
    uv.y = -uv.y;

    RayDesc ray;
    ray.Origin = float3(uv, 0.0f);
    ray.Direction = float3(0.0f, 0.0f, 1.0f);
    ray.TMin = 0.001f;
    ray.TMax = 1000.0f;

    Payload payload;
    payload.Color = float3(0.0f, 0.0f, 0.0f);
    payload.HitT = -1.0f;

    TraceRay(SceneBVH, RAY_FLAG_FORCE_OPAQUE, 0xFF, 0, 0, 0, ray, payload);

    Output[LaunchIndex.xy] = float4(payload.Color, 1.0f);
}

[shader("closesthit")]
void ClosestHit(inout Payload payload : SV_RayPayload,
    Attributes attr : SV_IntersectionAttributes)
{
    float3 bary = float3(
        1.0f - attr.Barycentrics.x - attr.Barycentrics.y,
        attr.Barycentrics.x,
        attr.Barycentrics.y);

    // The triangle lies in the z = 1 plane and faces back toward the camera.
    float3 normal = float3(0.0f, 0.0f, -1.0f);

    // Simple albedo based on barycentric coords so we can visually verify the hit.
    float3 albedo = float3(bary.x, bary.y, bary.z);

    // Hard-coded directional light (no shadow ray in this MVP).
    float3 lightDir = normalize(float3(0.5f, 1.0f, -0.5f));
    float  NdotL = saturate(dot(normal, lightDir));

    // One cosine-weighted indirect bounce.  If it misses the scene we add a tiny
    // sky contribution; this is what makes it a (very small) path trace.
    uint seed = DispatchRaysIndex().x * 73856093u + DispatchRaysIndex().y * 19349663u;
    float2 r = float2(RandomFloat(seed), RandomFloat(seed));
    float3 bounceDir = SampleHemisphereCosine(normal, r);

    Payload bouncePayload;
    bouncePayload.Color = float3(0.0f, 0.0f, 0.0f);
    bouncePayload.HitT = -1.0f;

    RayDesc bounceRay;
    bounceRay.Origin = WorldRayOrigin() + WorldRayDirection() * RayTCurrent() + normal * 0.0001f;
    bounceRay.Direction = bounceDir;
    bounceRay.TMin = 0.001f;
    bounceRay.TMax = 1000.0f;

    TraceRay(SceneBVH, RAY_FLAG_FORCE_OPAQUE, 0xFF, 0, 0, 0, bounceRay, bouncePayload);

    float3 indirect = bouncePayload.HitT < 0.0f ? float3(0.05f, 0.07f, 0.1f) : float3(0.0f, 0.0f, 0.0f);

    payload.Color = albedo * (0.1f + NdotL) + albedo * indirect * 0.3f;
    payload.HitT = RayTCurrent();
}

[shader("miss")]
void Miss(inout Payload payload : SV_RayPayload)
{
    payload.Color = float3(0.05f, 0.07f, 0.1f);
    payload.HitT = -1.0f;
}
