/*
 * Exposure Adaptation Compute Shader
 * Computes log-average luminance from scene color and temporally adapts it.
 * Input: Scene color texture (HDR, pre-tone-mapping)
 * Output: 1x1 R32_FLOAT adapted luminance texture (persisted across frames)
 */

cbuffer ExposureConstants : register(b0)
{
    float AdaptationSpeed;
    float KeyValue;
    float2 Pad;
};

Texture2D<float4> t_SceneColor : register(t0);
SamplerState LinearSampler : register(s0);

RWTexture2D<float> u_AdaptedLuminance : register(u0);

[numthreads(1, 1, 1)]
void main(uint2 dispatchThreadId : SV_DispatchThreadID)
{
    // Sample 16x16 grid uniformly across the scene color texture
    float logSum = 0.0;
    [unroll]
    for (int y = 0; y < 16; ++y)
    {
        [unroll]
        for (int x = 0; x < 16; ++x)
        {
            float2 uv = (float2(x, y) + 0.5) / 16.0;
            float3 color = t_SceneColor.SampleLevel(LinearSampler, uv, 0).rgb;
            float luma = max(dot(color, float3(0.2126, 0.7152, 0.0722)), 0.0001);
            logSum += log2(luma);
        }
    }

    float avgLogLuma = logSum / 256.0;
    float currentLuminance = exp2(avgLogLuma);

    // Read previous adapted luminance (0 on first frame)
    float prevLuminance = u_AdaptedLuminance[int2(0, 0)];

    // Handle first frame: if previous is 0 or negative, use current directly
    if (prevLuminance <= 0.0)
    {
        prevLuminance = currentLuminance;
    }

    // Temporal adaptation
    float adaptedLuminance = lerp(prevLuminance, currentLuminance, AdaptationSpeed);

    u_AdaptedLuminance[int2(0, 0)] = adaptedLuminance;
}
