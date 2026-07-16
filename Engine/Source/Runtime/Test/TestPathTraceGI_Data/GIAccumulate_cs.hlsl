// GIAccumulate_cs.hlsl - Temporal accumulation + ACES tonemap + sRGB gamma for TestPathTraceGI.
//
// Input:    one raw HDR radiance sample per frame (from GIPathTracing.hlsl Output)
// Accum:    running sum of raw samples (RGBA32_FLOAT)
// Output:   tonemapped, gamma-corrected sRGB average ready for the swapchain
//
// The swapchain is configured with VK_COLOR_SPACE_SRGB_NONLINEAR_KHR, so the
// final pixel values must be sRGB-encoded. Without this gamma step the image is
// technically correct in linear space but looks dark on screen.

// =============================================================================
// Constants
// =============================================================================

cbuffer AccumConstants : register(b0)
{
    uint  FrameCount;   // 1-based count of accumulated frames
    uint  Width;
    uint  Height;
    float Exposure;     // pre-tonemap exposure multiplier
};

// =============================================================================
// Resources
// =============================================================================

Texture2D<float4>   InputTexture  : register(t0);
RWTexture2D<float4> AccumTexture  : register(u0);
RWTexture2D<float4> DisplayTexture : register(u1);

// =============================================================================
// ACES filmic tonemap (same curve as GIPathTracing.hlsl)
// =============================================================================

float3 ACESFilm(float3 x)
{
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

// =============================================================================
// Linear -> sRGB gamma encoding
// =============================================================================

float3 LinearToSRGB(float3 linear)
{
    // Match the convention used by TestToneMapping: simple 2.2 gamma.
    return pow(linear, 1.0f / 2.2f);
}

// =============================================================================
// Main
// =============================================================================

[numthreads(8, 8, 1)]
void main(uint2 dispatchThreadId : SV_DispatchThreadID)
{
    uint2 pixel = dispatchThreadId;
    if (pixel.x >= Width || pixel.y >= Height)
        return;

    // GIPathTracing.hlsl outputs raw HDR radiance; accumulate raw samples,
    // then apply ACES tonemapping and sRGB gamma so the result looks correct
    // on an sRGB swapchain.
    float3 sample = InputTexture[pixel].rgb;

    // FrameCount is 1-based. On the first frame there is no previous accumulation.
    float3 accum = (FrameCount <= 1u) ? sample : (AccumTexture[pixel].rgb + sample);
    AccumTexture[pixel] = float4(accum, 1.0);

    float3 average = accum / float(FrameCount);
    float3 tonemapped = ACESFilm(average * Exposure);
    float3 srgb = LinearToSRGB(tonemapped);
    DisplayTexture[pixel] = float4(srgb, 1.0);
}
