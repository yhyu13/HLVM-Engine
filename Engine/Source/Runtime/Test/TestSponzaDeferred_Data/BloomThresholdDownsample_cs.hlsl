/*
 * Bloom Threshold + Downsample Compute Shader
 * Input: full-res HDR texture
 * Output: half-res bloom pre-filtered texture
 * Samples HDR with bilinear filtering at half-res UVs (implicit 2x2 box filter)
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

Texture2D<float4>   t_HDR         : register(t0);
SamplerState        LinearSampler : register(s0);

RWTexture2D<float4> u_BloomHalfRes : register(u0);

[numthreads(8, 8, 1)]
void main(uint2 dtid : SV_DispatchThreadID)
{
    if (dtid.x >= uint(HalfResSize.x) || dtid.y >= uint(HalfResSize.y))
        return;

    float2 uv = (float2(dtid) + 0.5) / HalfResSize;
    float3 hdr = t_HDR.SampleLevel(LinearSampler, uv, 0).rgb;

    float luma = dot(hdr, float3(0.2126, 0.7152, 0.0722));
    float contrib = max(0.0, luma - Threshold);
    float3 bloom = hdr * (contrib / max(luma, 0.0001));

    u_BloomHalfRes[dtid] = float4(bloom, 1.0);
}
