// Resolve_cs.hlsl — half-res → full-res upscale with depth/normal edge
// weights (PLAN_REALTIME_RESTIR_GAP Phase D). ReSTIR GI traces at half
// resolution; this pass upsamples the resolved radiance to the display
// resolution, preferring the half-res texel whose depth/normal best matches
// the full-res pixel.

Texture2D<float4> HalfResRadiance : register(t0);   // half-res radiance (rgb + hitT.a)
Texture2D<float>  FullResDepth    : register(t1);   // full-res linear depth
Texture2D<float4> FullResNormal   : register(t2);   // full-res encoded normal
RWTexture2D<float4> FullResOutput : register(u0);

cbuffer ResolveConstants : register(b0)
{
    float2 HalfSize;
    float2 RcpHalfSize;
    float2 RcpFullSize;
    float  DepthSigma;
    float  NormalSigma;
    float  Pad;
};

float3 DecodeNormal(float4 enc)
{
    return normalize(enc.rgb * 2.0 - 1.0);
}

[numthreads(8, 8, 1)]
void main(uint3 tid : SV_DispatchThreadID)
{
    float2 uv = (tid.xy + 0.5) * RcpFullSize;
    float centerDepth = FullResDepth.Load(int3(tid.xy, 0)).r;
    if (centerDepth <= 0.0)
    {
        FullResOutput[tid.xy] = float4(0.0, 0.0, 0.0, 0.0);
        return;
    }
    float3 centerNormal = DecodeNormal(FullResNormal.Load(int3(tid.xy, 0)));

    float2 halfUV = uv * HalfSize - 0.5;
    int2 base = int2(floor(halfUV));
    float2 fracPart = frac(halfUV);

    float3 sum = float3(0.0);
    float sumA = 0.0;
    float weightSum = 0.0;
    for (int dy = 0; dy <= 1; ++dy)
    {
        for (int dx = 0; dx <= 1; ++dx)
        {
            int2 hp = clamp(base + int2(dx, dy), int2(0, 0), int2(HalfSize) - 1);
            float4 s = HalfResRadiance.Load(int3(hp, 0));
            // Every half-res texel is written by its producer. Do NOT skip on
            // alpha: alpha is a hit distance (primary-ray hitT for the raw
            // trace, |x2 - x1| for the ReSTIR spatial estimate — v234), which
            // is legitimately 0 for rays that escape to the sky and for
            // invalid reservoirs. Skip only non-finite radiance.
            if (!all(isfinite(s.rgb)))
                continue;

            // Half-res texel's representative full-res depth/normal (its center).
            int2 fp = hp * 2 + 1;
            float sDepth = FullResDepth.Load(int3(fp, 0)).r;
            float3 sNormal = DecodeNormal(FullResNormal.Load(int3(fp, 0)));

            float bilinear = (dx == 0 ? (1.0 - fracPart.x) : fracPart.x)
                           * (dy == 0 ? (1.0 - fracPart.y) : fracPart.y);
            float depthW = exp(-abs(sDepth - centerDepth) * DepthSigma);
            float normalW = pow(max(dot(centerNormal, sNormal), 0.0), NormalSigma);
            float w = bilinear * depthW * normalW;
            sum += s.rgb * w;
            sumA += s.a * w;   // v234: propagate hit distance to ReBLUR
            weightSum += w;
        }
    }
    FullResOutput[tid.xy] = float4(weightSum > 1e-3 ? sum / weightSum : sum,
                                   weightSum > 1e-3 ? sumA / weightSum : sumA);
}
