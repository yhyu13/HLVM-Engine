/*
 * RT Shadows Shader - Simple Forward Ray Traced Shadows
 *
 * Uses explicit Vulkan bindings:
 *   [[vk::push_constant]] CameraConstants (Push Constants)
 *   t0: RayTracingAccelerationStructure (TLAS)
 *   u1: RWTexture2D (Output)
 */

#pragma pack_matrix(row_major)

// =============================================================================
// Structures
// =============================================================================

struct ShadowPayload {
    bool missed;
    float hitT;  // Hit distance for diagnostic
};

// =============================================================================
// Push Constants
// =============================================================================

[[vk::push_constant]]
cbuffer CameraConstants {
    float4 CameraPos;    // xyz = camera position, w = unused
    float4 CameraRight;  // x = right vector x (for debug), rest unused
    float4 CameraUp;     // y = up vector y (for debug), rest unused
};

// =============================================================================
// Resources - Explicit Vulkan bindings
// =============================================================================

// Output texture - binding 1, space 0 (UAV)
RWTexture2D<float4> Output : register(u1);

// Scene acceleration structure - binding 0, space 0 (SRV/TLAS)
RaytracingAccelerationStructure SceneBVH : register(t0);

// =============================================================================
// Ray Generation Shader
// =============================================================================

[shader("raygeneration")]
void RayGen() {
    uint2 globalIdx = DispatchRaysIndex().xy;
    uint2 dim = DispatchRaysDimensions().xy;

    // Normalized pixel coordinates [0, 1]
    float2 uv = float2(globalIdx) / float2(dim);

    // Convert UV to ray direction
    float2 ndc = uv * 2.0 - 1.0;
    float aspectRatio = float(dim.x) / float(dim.y);
    float fov = 1.0;  // tan(FOV/2)

    // Camera looks in -Z direction, at z=0 screen plane
    // Ray origin from constant buffer
    float3 rayOrigin = CameraPos.xyz;

    // Ray direction - simple pinhole camera
    float3 rayDir;
    rayDir.x = ndc.x * aspectRatio * fov;
    rayDir.y = ndc.y * fov;
    rayDir.z = -1.0;

    // Transform to world space (camera looks in -Z)
    rayDir = normalize(rayDir);

    // Setup primary ray - cast into scene
    RayDesc primaryRay;
    primaryRay.Origin = rayOrigin;
    primaryRay.Direction = rayDir;
    primaryRay.TMin = 0.01;
    primaryRay.TMax = 1000.0;

    // Trace primary ray
    ShadowPayload primaryPayload;
    primaryPayload.missed = true;
    primaryPayload.hitT = -1.0;

    TraceRay(
        SceneBVH,
        RAY_FLAG_CULL_BACK_FACING_TRIANGLES,
        0xFF,
        0,
        0,
        0,
        primaryRay,
        primaryPayload);

    // Diagnostic: color based on hit distance
    // Miss = blue gradient (sky)
    // Hit = red (close) to green (far) with sharp curve for better distinction
    float3 debugColor;
    if (primaryPayload.missed) {
        // Sky gradient - blue
        debugColor = float3(0.1, 0.1, 0.4 + uv.y * 0.3);
    } else {
        // Hit: red (close) to green (far), using sqrt for sharper curve
        float t = sqrt(saturate(primaryPayload.hitT / 30.0));  // Sharper curve, 30 unit range
        debugColor = lerp(float3(1.0, 0.0, 0.0), float3(0.0, 1.0, 0.0), t);
    }

    Output[globalIdx] = float4(debugColor, 1.0);
}

// =============================================================================
// Miss Shader
// =============================================================================

[shader("miss")]
void Miss(inout ShadowPayload payload : SV_RayPayload) {
    payload.missed = true;
    payload.hitT = -1.0;
}

// =============================================================================
// Closest Hit Shader
// =============================================================================

[shader("closesthit")]
void ClosestHit(inout ShadowPayload payload : SV_RayPayload, in float3 hitAttr : SV_HitT) {
    // Hit something - capture hit distance
    payload.missed = false;
    payload.hitT = RayTCurrent();
}
