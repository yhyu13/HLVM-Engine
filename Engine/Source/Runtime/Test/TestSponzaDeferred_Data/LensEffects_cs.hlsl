/*
 * Lens Effects Compute Shader
 *
 * Combines three camera lens imperfections:
 * 1. Chromatic Aberration: RGB channel radial separation
 * 2. Vignette: Corner darkening
 * 3. Film Grain: Temporal per-pixel noise
 *
 * Input:  SDR texture (RGBA8_UNORM)
 * Output: Final display-ready image
 */

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

cbuffer LensEffectsConstants : register(b0)
{
    float ChromaticAmount;
    float VignetteIntensity;
    float GrainIntensity;
    int   FrameIndex;
    float2 OutputSize;
    float2 RcpOutputSize;
};

// ---------------------------------------------------------------------------
// Resources
// ---------------------------------------------------------------------------

Texture2D<float4>   t_SDR : register(t0);

SamplerState PointClamp  : register(s0);
SamplerState LinearClamp : register(s1);

RWTexture2D<float4> u_Output : register(u0);

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
    float2 centerOffset = uv - 0.5;
    float dist = length(centerOffset);

    // =====================================================================
    // Chromatic Aberration
    // =====================================================================

    float3 color;
    if (ChromaticAmount > 0.0)
    {
        float2 dir = normalize(centerOffset);
        float caAmount = dist * dist * ChromaticAmount;
        float2 caOffset = dir * caAmount * RcpOutputSize;

        color.r = t_SDR.SampleLevel(PointClamp, uv + caOffset, 0).r;
        color.g = t_SDR.SampleLevel(PointClamp, uv, 0).g;
        color.b = t_SDR.SampleLevel(PointClamp, uv - caOffset, 0).b;
    }
    else
    {
        color = t_SDR.SampleLevel(PointClamp, uv, 0).rgb;
    }

    // =====================================================================
    // Vignette
    // =====================================================================

    if (VignetteIntensity > 0.0)
    {
        float vignette = 1.0 - dist * dist * VignetteIntensity;
        vignette = saturate(vignette);
        color *= vignette;
    }

    // =====================================================================
    // Film Grain
    // =====================================================================

    if (GrainIntensity > 0.0)
    {
        uint hash = pixelCoord.x * 73856093u ^ pixelCoord.y * 19349663u ^ uint(FrameIndex) * 83492791u;
        float grain = float(hash % 1024u) / 1024.0;
        grain = (grain - 0.5) * GrainIntensity;
        color += grain;
    }

    u_Output[pixelCoord] = float4(saturate(color), 1.0);
}
