/*
 * Bloom Gaussian Blur Compute Shader (Separable)
 * Input: half-res bloom texture
 * Output: half-res blurred bloom texture
 * Direction constant selects horizontal (1.0) or vertical (0.0) pass
 * 9-tap Gaussian kernel with analytic weights
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

Texture2D<float4>   t_Input        : register(t0);
SamplerState        LinearSampler  : register(s0);

RWTexture2D<float4> u_Output       : register(u0);

static const int KERNEL_RADIUS = 4;

float GaussianWeight(float x, float sigma)
{
    return exp(-(x * x) / (2.0 * sigma * sigma));
}

[numthreads(8, 8, 1)]
void main(uint2 dtid : SV_DispatchThreadID)
{
    if (dtid.x >= uint(HalfResSize.x) || dtid.y >= uint(HalfResSize.y))
        return;

    float2 invSize = 1.0 / HalfResSize;
    float2 uv = (float2(dtid) + 0.5) * invSize;

    float2 dir = (Direction > 0.5)
        ? float2(invSize.x, 0.0)
        : float2(0.0, invSize.y);

    float3 color = float3(0, 0, 0);
    float weightSum = 0.0;

    [unroll]
    for (int i = -KERNEL_RADIUS; i <= KERNEL_RADIUS; i++)
    {
        float w = GaussianWeight(float(i), Sigma);
        float2 sampleUV = uv + dir * float(i);
        color += t_Input.SampleLevel(LinearSampler, sampleUV, 0).rgb * w;
        weightSum += w;
    }

    u_Output[dtid] = float4(color / weightSum, 1.0);
}
