/*
 * RT Debug: Hit Distance (No GBuffer)
 * Shows actual ray tracing hit positions
 */

#pragma pack_matrix(row_major)

RWTexture2D<float4> Output : register(u0);
RaytracingAccelerationStructure SceneBVH : register(t0);

struct Payload {
    bool missed;
    float hitT;
};

[shader("raygeneration")]
void RayGen() {
    uint2 globalIdx = DispatchRaysIndex().xy;
    uint2 dim = DispatchRaysDimensions().xy;

    // Simple camera
    float2 uv = float2(globalIdx) / float2(dim);
    float2 ndc = uv * 2.0 - 1.0;

    float3 rayOrigin = float3(0.0, 5.0, 10.0);  // Camera position
    float3 rayDir = normalize(float3(ndc.x * 0.7, ndc.y, -1.0));

    RayDesc ray;
    ray.Origin = rayOrigin;
    ray.Direction = rayDir;
    ray.TMin = 0.01;
    ray.TMax = 1000.0;

    Payload payload;
    payload.missed = true;
    payload.hitT = -1.0;

    TraceRay(SceneBVH, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, 0xFF, 0, 0, 0, ray, payload);

    if (payload.missed) {
        // Sky - gradient blue
        Output[globalIdx] = float4(0.5, 0.7, 1.0, 1.0);
    } else {
        // Hit - show distance as grayscale
        float gray = clamp(payload.hitT / 30.0, 0.0, 1.0);
        Output[globalIdx] = float4(gray, gray, gray, 1.0);
    }
}

[shader("closesthit")]
void ClosestHit(inout Payload p : SV_RayPayload, in float3 hitAttr : SV_HitT) {
    p.missed = false;
    p.hitT = RayTCurrent();
}

[shader("miss")]
void Miss(inout Payload p : SV_RayPayload) {
    p.missed = true;
    p.hitT = -1.0;
}

[shader("miss")]
void ShadowMiss(inout Payload p : SV_RayPayload) {
    p.missed = true;
    p.hitT = -1.0;
}
