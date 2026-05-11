/*
 * Copyright (c) 2024-2026. MIT License. All rights reserved.
 *
 * Ray tracing shader library for RT triangle test.
 * Contains: RayGen, ClosestHit, Miss entry points.
 */

// Ray tracing payload structure
struct HitInfo
{
    float4 ShadedColorAndHitT : SHADED_COLOR_AND_HIT_T;
};

// Ray tracing attributes (barycentric coordinates)
struct Attributes
{
    float2 uv;
};

// Acceleration structure - bound at runtime by NVRHI
RaytracingAccelerationStructure SceneBVH : register(t0);

// Ray generation shader output (written to UAV)
RWTexture2D<float4> RTOutput : register(u1);

// Ray generation shader
[shader("raygeneration")]
void RayGen()
{
    uint2 LaunchIndex = DispatchRaysIndex().xy;
    uint2 LaunchDimensions = DispatchRaysDimensions().xy;
    
    // Setup the ray - normalized device coordinates mapping
    RayDesc ray;
    ray.Origin = float3(
        lerp(-1.0, 1.0, float(LaunchIndex.x) / float(LaunchDimensions.x)),
        lerp(-1.0, 1.0, float(LaunchIndex.y) / float(LaunchDimensions.y)),
        0.0);
    ray.Direction = float3(0.0, 0.0, 1.0);
    ray.TMin = 0.1;
    ray.TMax = 1000.0;
    
    // Initialize payload - red color, no hit
    HitInfo payload;
    payload.ShadedColorAndHitT = float4(1.0, 0.0, 0.0, 0.0);
    
    // Trace the ray using DXR HLSL syntax
    TraceRay(
        SceneBVH,
        RAY_FLAG_NONE,
        0xFF,
        0,
        0,
        0,
        ray,
        payload);
    
    // Write output color
    RTOutput[LaunchIndex.xy] = float4(payload.ShadedColorAndHitT.rgb, 1.0);
}

// Closest hit shader
[shader("closesthit")]
void ClosestHit(inout HitInfo payload : SV_RayPayload,
    Attributes attrib : SV_IntersectionAttributes)
{
    // Compute barycentric coordinates from hit attributes
    // attrib.uv contains barycentric coords (u, v) where w = 1 - u - v
    float3 barycentrics = float3(1.0 - attrib.uv.x - attrib.uv.y, attrib.uv.x, attrib.uv.y);

    payload.ShadedColorAndHitT = float4(barycentrics, RayTCurrent());
}

// Miss shader
[shader("miss")]
void Miss(inout HitInfo payload : SV_RayPayload)
{
    // Gray color, -1.0 indicates no hit
    payload.ShadedColorAndHitT = float4(0.2, 0.2, 0.2, -1.0);
}

// Note: Library compiled with lib_6_5 - entry points RayGen, ClosestHit, Miss are
// automatically exported. NVRHI finds them by shader type, not by name string.