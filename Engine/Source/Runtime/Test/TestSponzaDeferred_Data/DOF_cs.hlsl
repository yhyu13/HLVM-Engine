/*
 * Depth of Field Compute Shader
 *
 * Input:  Color texture, depth buffer
 * Output: DOF-blurred color
 *
 * Algorithm:
 * 1. Compute CoC from depth difference to focal plane
 * 2. Early-out for in-focus pixels
 * 3. Gather 16 samples in 4 concentric rings
 */

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

cbuffer DOFConstants : register(b0)
{
    float FocalDepth;
    float Aperture;
    float DepthScale;
    float MaxBlurRadius;
    float2 OutputSize;
    float2 RcpOutputSize;
};

// ---------------------------------------------------------------------------
// Resources
// ---------------------------------------------------------------------------

Texture2D<float4>   t_Color : register(t0);
Texture2D<float>    t_Depth : register(t1);

SamplerState LinearClamp : register(s0);

RWTexture2D<float4> u_DOF : register(u0);

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
    // Circle of Confusion
    // =====================================================================

    float depth = t_Depth[pixelCoord];
    float coc = abs(depth - FocalDepth) * DepthScale * Aperture;
    coc = min(coc, MaxBlurRadius);

    // Early-out for in-focus pixels
    if (coc < 1.0)
    {
        u_DOF[pixelCoord] = centerColor;
        return;
    }

    // =====================================================================
    // Gather samples in 4 concentric rings
    // =====================================================================

    float4 accum = centerColor;
    float totalWeight = 1.0;

    [unroll]
    for (int i = 0; i < 16; i++)
    {
        int ring = i / 4;
        int sampleInRing = i % 4;

        float angle = (float(sampleInRing) / 4.0) * 6.28318530718 + (float(ring) * 0.5);
        float radius = (float(ring + 1) / 4.0) * coc;

        float2 offset = float2(cos(angle), sin(angle)) * radius * RcpOutputSize;
        float2 sampleUV = uv + offset;

        float4 sampleColor = t_Color.SampleLevel(LinearClamp, sampleUV, 0);
        accum += sampleColor;
        totalWeight += 1.0;
    }

    u_DOF[pixelCoord] = accum / totalWeight;
}
