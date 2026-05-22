/*
 * Screen-Space Contact Shadows Compute Shader
 * Ray-marches a short distance along the light direction in screen space
 * to find nearby occluders that the shadow map misses.
 *
 * Input: GBuffer depth texture
 * Output: Full-res R8_UNORM contact shadow mask (1.0 = lit, 0.0 = shadowed)
 */

cbuffer ContactShadowConstants : register(b0)
{
    float4x4 ViewProj;
    float4x4 InvViewProj;
    float3   LightDir;
    float    MaxDistance;
    float2   ScreenSize;
    float2   RcpScreenSize;
    int      StepCount;
    float    Thickness;
    float2   Pad;
    float4   Pad2[5];
};

Texture2D<float> t_Depth : register(t0);
SamplerState PointSampler : register(s0);

RWTexture2D<float> u_ContactShadow : register(u0);

[numthreads(8, 8, 1)]
void main(uint2 dispatchThreadId : SV_DispatchThreadID)
{
    uint2 pixelCoord = dispatchThreadId;

    if (pixelCoord.x >= uint(ScreenSize.x) || pixelCoord.y >= uint(ScreenSize.y))
        return;

    // Early exit: disabled state (write all 1.0s)
    if (StepCount <= 0)
    {
        u_ContactShadow[pixelCoord] = 1.0;
        return;
    }

    float depth = t_Depth[pixelCoord];

    // Sky / far plane = fully lit
    if (depth >= 1.0 || depth <= 0.0)
    {
        u_ContactShadow[pixelCoord] = 1.0;
        return;
    }

    // Reconstruct world position from depth
    float2 uv = (float2(pixelCoord) + 0.5) * RcpScreenSize;
    float2 ndc = uv * 2.0 - 1.0;
    float4 worldPosH = mul(InvViewProj, float4(ndc.x, -ndc.y, depth, 1.0));
    float3 worldPos = worldPosH.xyz / worldPosH.w;

    // Ray direction: from surface toward light (negate light direction)
    float3 rayDir = normalize(-LightDir);
    float stepSize = MaxDistance / float(StepCount);

    // Ray march along light direction
    for (int i = 1; i <= StepCount; ++i)
    {
        float dist = stepSize * float(i);
        float3 sampleWorldPos = worldPos + rayDir * dist;

        // Project sample to screen space
        float4 screenPos = mul(ViewProj, float4(sampleWorldPos, 1.0));
        screenPos.xyz /= screenPos.w;
        float2 sampleUV = screenPos.xy * 0.5 + 0.5;
        sampleUV.y = 1.0 - sampleUV.y; // Flip Y for DX-style viewport

        // Off-screen = skip
        if (sampleUV.x < 0.0 || sampleUV.x > 1.0 || sampleUV.y < 0.0 || sampleUV.y > 1.0)
            continue;

        // Sample depth at ray's screen position
        float sampleDepth = t_Depth.SampleLevel(PointSampler, sampleUV, 0);

        // Compare in NDC Z [0,1]: if scene depth < ray depth, occluder found
        if (sampleDepth < screenPos.z - Thickness)
        {
            // Distance-based fade: shadow is strongest near the surface
            float fade = 1.0 - saturate(dist / MaxDistance);
            u_ContactShadow[pixelCoord] = 1.0 - fade;
            return;
        }
    }

    u_ContactShadow[pixelCoord] = 1.0;
}
