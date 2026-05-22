/*
 * Tone Mapping Compute Shader - TestSponzaDeferred
 * ACES filmic curve + Reinhard + gamma correction + bloom + exposure adaptation
 * Input: HDR texture from lighting pass + Bloom texture + SSR + Adapted Luminance
 * Output: SDR RGBA8 (blit handles display)
 */

cbuffer TonemapConstants : register(b0)
{
    float Exposure;              // EV adjustment (default: 1.0)
    float Gamma;                 // Gamma correction (default: 2.2)
    int   TonemapMode;           // 0=ACES, 1=Reinhard, 2=None
    float BloomIntensity;        // Bloom additive intensity (default: 0.4)
    float2 TextureSize;
    float KeyValue;              // Target middle gray (default: 0.18)
    int   UseExposureAdaptation; // 0=disabled, 1=enabled
};

Texture2D<float4> t_HDRInput  : register(t0);
Texture2D<float4> t_Bloom     : register(t1);
Texture2D<float4> t_SSR       : register(t2);
Texture2D<float>  t_AdaptedLuminance : register(t3);

SamplerState LinearSampler : register(s0);

RWTexture2D<float4> u_SDROutput : register(u0);

// ---------------------------------------------------------------------------
// Tone Map Operators
// ---------------------------------------------------------------------------

float3 ACESFilm(float3 x)
{
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

float3 Reinhard(float3 x)
{
    return x / (1.0 + x);
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

[numthreads(8, 8, 1)]
void main(uint2 dispatchThreadId : SV_DispatchThreadID)
{
    uint2 pixelCoord = dispatchThreadId;

    if (pixelCoord.x >= uint(TextureSize.x) || pixelCoord.y >= uint(TextureSize.y))
        return;

    float3 hdr = t_HDRInput[pixelCoord].rgb;

    // Add SSR (sample half-res SSR with linear filter at full-res UV)
    float2 uv = (float2(pixelCoord) + 0.5) / TextureSize;
    float4 ssr = t_SSR.SampleLevel(LinearSampler, uv, 0);
    hdr += ssr.rgb * ssr.a;

    // Add bloom
    float3 bloom = t_Bloom[pixelCoord].rgb;
    hdr += bloom * BloomIntensity;

    // Apply exposure (fixed or adapted)
    float exposure = Exposure;
    if (UseExposureAdaptation != 0)
    {
        float adaptedLuminance = t_AdaptedLuminance[int2(0, 0)];
        exposure = KeyValue / max(adaptedLuminance, 0.0001);
    }
    hdr *= exposure;

    // Tone mapping
    float3 mapped;
    if (TonemapMode == 0)
        mapped = ACESFilm(hdr);
    else if (TonemapMode == 1)
        mapped = Reinhard(hdr);
    else
        mapped = saturate(hdr);

    // Gamma correction (linear -> sRGB)
    float3 sdr = pow(mapped, 1.0 / Gamma);

    u_SDROutput[pixelCoord] = float4(sdr, 1.0);
}
