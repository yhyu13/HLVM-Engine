/*
 * Bloom Upsample Compute Shader
 * Input: half-res blurred bloom texture
 * Output: full-res bloom texture (bilinear upsample + intensity scale)
 */

cbuffer BloomConstants : register(b0)
{
    float2 FullResSize;
    float2 HalfResSize;
    float  Threshold;
    float  Intensity;
    float  Sigma;
    float  Direction;
    int    BlurIterations;
    int    Pad0;
    float4 Pad1;
};

Texture2D<float4>   t_BloomHalfRes : register(t0);
SamplerState        LinearSampler  : register(s0);

RWTexture2D<float4> u_BloomFullRes : register(u0);

[numthreads(8, 8, 1)]
void main(uint2 dtid : SV_DispatchThreadID)
{
    if (dtid.x >= uint(FullResSize.x) || dtid.y >= uint(FullResSize.y))
        return;

    float2 uv = (float2(dtid) + 0.5) / FullResSize;
    float3 bloom = t_BloomHalfRes.SampleLevel(LinearSampler, uv, 0).rgb;

    u_BloomFullRes[dtid] = float4(bloom * Intensity, 1.0);
}
