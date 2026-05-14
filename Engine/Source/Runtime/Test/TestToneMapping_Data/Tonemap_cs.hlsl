/*
 * Tone Mapping Compute Shader - TestToneMapping
 * ACES filmic curve + Reinhard + gamma correction
 * Input: procedural HDR gradient (left=-5 EV, right=+5 EV)
 * Output: linear RGB in RGBA32_FLOAT (blit handles display)
 */

cbuffer TonemapConstants : register(b0)
{
    float Exposure;       // EV adjustment (default: 1.0)
    float Gamma;          // Gamma correction (default: 2.2)
    int   TonemapMode;    // 0=ACES, 1=Reinhard, 2=None
    float Pad;
    float2 TextureSize;   // (width, height) for procedural gradient
};

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
    
    // Procedural HDR gradient: left = -5 EV, right = +5 EV
    float t = float(pixelCoord.x) / TextureSize.x;
    float ev = lerp(-5.0, 5.0, t);
    float3 hdr = pow(2.0, ev);
    
    // Apply exposure
    hdr *= Exposure;
    
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
