/*
 * Screen-Space Reflections (SSR) Compute Shader
 *
 * Algorithm: view-space ray march along reflection vector with
 * projected depth comparison and binary search refinement.
 *
 * Outputs RGBA16_FLOAT: RGB = reflected color, A = reflection weight.
 * Traces at half resolution for performance; full-res GBuffer/HDR inputs.
 */

#define SSR_BINARY_SEARCH_STEPS 3

cbuffer SSRConstants : register(b0)
{
    float4x4 ProjMatrix;
    float4x4 InvProjMatrix;
    float4x4 ViewMatrix;
    float2   FullScreenSize;
    float2   InvFullScreenSize;
    float2   HalfScreenSize;
    float2   InvHalfScreenSize;
    int      MaxSteps;
    float    StepSize;
    float    MaxDistance;
    float    Thickness;
    float4   Pad0;
};

Texture2D<float>    t_Depth    : register(t0);
Texture2D<float4>   t_Normal   : register(t1);
Texture2D<float4>   t_Material : register(t2);
Texture2D<float4>   t_HDR      : register(t3);

SamplerState PointSampler  : register(s0);
SamplerState LinearSampler : register(s1);

RWTexture2D<float4> u_SSR : register(u0);

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

float3 ReconstructViewPosFromUV(float2 uv, float depth, float4x4 invProj)
{
    float2 ndc = uv * 2.0 - 1.0;
    // Vulkan NDC Y is down; flip Y for correct reconstruction
    float4 viewPosH = mul(invProj, float4(ndc.x, -ndc.y, depth, 1.0));
    return viewPosH.xyz / viewPosH.w;
}

float3 ReconstructViewPos(uint2 pixelCoord, float depth, float4x4 invProj, float2 screenSize)
{
    float2 uv = (float2(pixelCoord) + 0.5) / screenSize;
    return ReconstructViewPosFromUV(uv, depth, invProj);
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

[numthreads(8, 8, 1)]
void main(uint2 dispatchThreadId : SV_DispatchThreadID)
{
    uint2 pixelCoord = dispatchThreadId;

    // Early out if outside half-res bounds
    if (pixelCoord.x >= uint(HalfScreenSize.x) || pixelCoord.y >= uint(HalfScreenSize.y))
        return;

    // Sample full-res GBuffer at the center of the 2x2 block this half-res pixel represents
    float2 startUV = (float2(pixelCoord) * 2.0 + 0.5) * InvFullScreenSize;

    float  depth     = t_Depth.SampleLevel(PointSampler, startUV, 0);
    float4 material  = t_Material.SampleLevel(PointSampler, startUV, 0);
    float4 normalData = t_Normal.SampleLevel(PointSampler, startUV, 0);

    float metallic  = material.r;
    float roughness = material.g;

    // Early out for non-reflective surfaces
    if (metallic < 0.05 || depth >= 1.0 || depth <= 0.0)
    {
        u_SSR[pixelCoord] = float4(0.0, 0.0, 0.0, 0.0);
        return;
    }

    // Decode world normal and transform to view space
    float3 worldNormal = normalize(normalData.xyz * 2.0 - 1.0);
    float3 viewNormal  = normalize(mul((float3x3)ViewMatrix, worldNormal));

    // Reconstruct view-space position
    float3 viewPos = ReconstructViewPosFromUV(startUV, depth, InvProjMatrix);

    // View and reflection vectors
    float3 viewDir    = normalize(-viewPos);
    float3 reflectDir = reflect(-viewDir, viewNormal);

    // Ray march in view space
    float3 rayPos = viewPos;
    float3 rayStep = reflectDir * StepSize;

    float2 hitUV = 0.0;
    bool hit = false;
    float3 prevRayPos = rayPos;

    for (int i = 0; i < MaxSteps; i++)
    {
        prevRayPos = rayPos;
        rayPos += rayStep;

        // Project to screen space
        float4 clipPos = mul(ProjMatrix, float4(rayPos, 1.0));
        clipPos.xyz /= clipPos.w;

        float2 uv = clipPos.xy * 0.5 + 0.5;
        uv.y = 1.0 - uv.y;

        // Out of bounds
        if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0)
            break;

        // Sample depth at projected position
        float sampleDepth = t_Depth.SampleLevel(PointSampler, uv, 0);

        // Reconstruct view-space Z at sampled depth
        float3 sampleViewPos = ReconstructViewPosFromUV(uv, sampleDepth, InvProjMatrix);

        float rayViewZ    = rayPos.z;
        float sampleViewZ = sampleViewPos.z;
        float prevRayViewZ = prevRayPos.z;

        // Check if ray crossed the surface
        // "In front" = closer to camera = larger Z (less negative)
        // "Behind" = farther from camera = smaller Z (more negative)
        if (rayViewZ < sampleViewZ && prevRayViewZ >= sampleViewZ - Thickness)
        {
            hit = true;
            hitUV = uv;
            break;
        }
    }

    // Binary search refinement
    if (hit)
    {
        float3 low = prevRayPos;
        float3 high = rayPos;

        for (int b = 0; b < SSR_BINARY_SEARCH_STEPS; b++)
        {
            float3 mid = (low + high) * 0.5;
            float4 midClip = mul(ProjMatrix, float4(mid, 1.0));
            midClip.xyz /= midClip.w;
            float2 midUV = midClip.xy * 0.5 + 0.5;
            midUV.y = 1.0 - midUV.y;

            float midDepth = t_Depth.SampleLevel(PointSampler, midUV, 0);
            float3 midSampleViewPos = ReconstructViewPosFromUV(midUV, midDepth, InvProjMatrix);

            if (mid.z < midSampleViewPos.z)
                high = mid;
            else
                low = mid;
        }

        float4 finalClip = mul(ProjMatrix, float4(high, 1.0));
        finalClip.xyz /= finalClip.w;
        hitUV = finalClip.xy * 0.5 + 0.5;
        hitUV.y = 1.0 - hitUV.y;
    }

    // Sample reflected color and compute weight
    float4 ssrColor = float4(0.0, 0.0, 0.0, 0.0);
    if (hit)
    {
        float3 color = t_HDR.SampleLevel(LinearSampler, hitUV, 0).rgb;

        // Distance fade
        float dist = length(rayPos - viewPos);
        float distFade = saturate(1.0 - dist / MaxDistance);

        // Edge fade (screen-space border)
        float2 edgeDist = min(hitUV, 1.0 - hitUV);
        float edgeFade = saturate(min(edgeDist.x, edgeDist.y) * 20.0);

        // Roughness fade (less aggressive so rough surfaces still show reflections)
        float roughnessFade = saturate(1.0 - roughness * 0.5);

        // Metallic scales overall reflection intensity
        float weight = metallic * distFade * edgeFade * roughnessFade;

        ssrColor = float4(color, weight);
    }

    u_SSR[pixelCoord] = ssrColor;
}
