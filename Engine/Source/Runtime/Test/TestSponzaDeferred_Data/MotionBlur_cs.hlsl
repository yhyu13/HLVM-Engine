/*
 * Camera Motion Blur Compute Shader
 *
 * Input:  Color texture (TAA output), depth buffer
 * Output: Motion-blurred color
 *
 * Algorithm:
 * 1. Reconstruct world position from depth + inverse view-proj
 * 2. Project to previous frame to compute motion vector
 * 3. Early-out for near-zero motion
 * 4. Sample 8 taps along motion direction with Gaussian weights
 */

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

cbuffer MotionBlurConstants : register(b0)
{
    float4x4 InverseCurrViewProj;
    float4x4 PrevViewProj;
    float2   OutputSize;
    float2   RcpOutputSize;
    float    VelocityScale;
    float    MinVelocity;
    float2   Pad;
};

// ---------------------------------------------------------------------------
// Resources
// ---------------------------------------------------------------------------

Texture2D<float4>   t_Color : register(t0);
Texture2D<float>    t_Depth : register(t1);

SamplerState LinearClamp : register(s0);

RWTexture2D<float4> u_MotionBlur : register(u0);

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

[numthreads(8, 8, 1)]
void main(uint2 dispatchThreadId : SV_DispatchThreadID)
{
    uint2 pixelCoord = dispatchThreadId;

    if (pixelCoord.x >= uint(OutputSize.x) || pixelCoord.y >= uint(OutputSize.y))
        return;

    float2 uv = (float2(pixelCoord) + 0.5) * RcpOutputSize;
    float4 centerColor = t_Color[pixelCoord];

    // =====================================================================
    // Motion Vector Computation (same reconstruction as TAA)
    // =====================================================================

    float depth = t_Depth[pixelCoord];

    float4 ndc = float4(uv * 2.0 - 1.0, depth, 1.0);
    float4 worldPos = mul(InverseCurrViewProj, ndc);
    worldPos.xyz /= worldPos.w;

    float4 prevClip = mul(PrevViewProj, float4(worldPos.xyz, 1.0));
    float2 prevUV = prevClip.xy / prevClip.w * float2(0.5, -0.5) + 0.5;

    float2 motion = prevUV - uv;
    float velocity = length(motion) * OutputSize.x * VelocityScale;

    // =====================================================================
    // Early-out for near-zero motion
    // =====================================================================

    if (velocity < MinVelocity)
    {
        u_MotionBlur[pixelCoord] = centerColor;
        return;
    }

    // =====================================================================
    // Sample along motion direction
    // =====================================================================

    float2 dir = normalize(motion);
    float2 step = dir * velocity * RcpOutputSize * 0.125; // 1/8

    float4 accum = float4(0.0, 0.0, 0.0, 0.0);
    float totalWeight = 0.0;

    [unroll]
    for (int i = 0; i < 8; i++)
    {
        float t = (float(i) / 7.0) - 0.5; // [-0.5, 0.5]
        float2 sampleUV = uv + step * t * 2.0;

        // Gaussian weight (sigma = 0.5)
        float weight = exp(-t * t * 4.0);

        float4 sampleColor = t_Color.SampleLevel(LinearClamp, sampleUV, 0);
        accum += sampleColor * weight;
        totalWeight += weight;
    }

    u_MotionBlur[pixelCoord] = accum / totalWeight;
}
