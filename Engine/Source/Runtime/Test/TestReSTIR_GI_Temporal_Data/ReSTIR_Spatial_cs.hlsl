// ReSTIR GI — Spatial Reuse Pass (RealEngine-modeled).
//
// Merges the center pixel's reservoir (from Temporal) with geometrically
// valid neighbor reservoirs using weighted reservoir sampling:
//
//   w_n = target(radiance_n) * W_n * M_n
//   sumWeight += w_n;  M += M_n
//   select sample n with probability w_n / sumWeight
//   W = sumWeight / (M * target_selected)
//   output = selected * W
//
// This is a true reservoir merge (not a box average): the winner is chosen
// by weighted reservoir sampling and the output is the W-weighted estimate.
// Depth/normal rejection excludes neighbors on different geometry.

struct FReSTIRSpatialConstants
{
    float2 OutputSize;
    float2 RcpOutputSize;
    float NormalThreshold;
    float DepthThreshold;
    float MaxM;
    float SpatialRadius;
    float DebugVis;
    float2 Pad;
};

cbuffer Constants : register(b0)
{
    FReSTIRSpatialConstants gConstants;
}

Texture2D<float4> gRadiance : register(t0);
Texture2D<float4> gReservoir0 : register(t1);
Texture2D<float4> gReservoir1 : register(t2);
Texture2D<float4> gNormals : register(t3);
Texture2D<float> gDepth : register(t4);

RWTexture2D<float4> gOutput : register(u0);

float Luminance(float3 c)
{
    return dot(c, float3(0.2126, 0.7152, 0.0722));
}

// PCG-style hash -> [0,1)
float Hash01(uint2 p, uint s)
{
    uint state = p.x * 747796405u + p.y * 2891336453u + s * 277803737u;
    state = (state >> 13u) ^ state;
    state *= 0x85ebca6bu;
    state ^= state >> 16u;
    return float(state & 0xFFFFFFu) / float(0x1000000u);
}

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    float2 outputSize = gConstants.OutputSize;
    if (dispatchThreadID.x >= (uint)outputSize.x || dispatchThreadID.y >= (uint)outputSize.y)
        return;

    int2 pixel = int2(dispatchThreadID.xy);

    // =====================================================================
    // Center pixel geometry + reservoir
    // =====================================================================
    float3 centerNormal = normalize(gNormals.Load(int3(pixel, 0)).rgb * 2.0f - 1.0f);
    float centerDepth = gDepth.Load(int3(pixel, 0));

    float4 cR0 = gReservoir0.Load(int3(pixel, 0));
    float4 cR1 = gReservoir1.Load(int3(pixel, 0));
    float3 selectedRadiance = cR0.rgb;
    float selectedHitT = cR0.a;
    float M = max(cR1.x, 1.0f);
    float W = max(cR1.y, 0.0f);
    float selectedTarget = max(Luminance(selectedRadiance), 1e-6f);
    float sumWeight = selectedTarget * W * M;

    // 2026-08-09 (Phase 4): weighted-average resolve. The old resolve output
    // selectedRadiance * W, which with W==1 (homogeneous radiance) was a pure
    // pass-through of gi_raw. Instead accumulate the MIS-style contributions
    // w_i = target_i * W_i * M_i and output their weighted average, so
    // neighboring reservoirs visibly contribute to every pixel.
    float3 resolveSum = selectedRadiance * sumWeight;
    float resolveWeight = sumWeight;

    int radius = int(gConstants.SpatialRadius);

    // =====================================================================
    // Neighbor merge (3x3 around center)
    // =====================================================================
    for (int dy = -1; dy <= 1; ++dy)
    {
        for (int dx = -1; dx <= 1; ++dx)
        {
            if (dx == 0 && dy == 0)
                continue;

            int2 nPixel = pixel + int2(dx, dy);
            if (any(nPixel < int2(0)) || any(nPixel >= int2(outputSize)))
                continue;

            float3 nNormal = normalize(gNormals.Load(int3(nPixel, 0)).rgb * 2.0f - 1.0f);
            float nDepth = gDepth.Load(int3(nPixel, 0));

            if (dot(centerNormal, nNormal) < gConstants.NormalThreshold)
                continue;
            // Relative-depth rejection (perspective-correct, fixed 2026-08-09):
            // absolute deltas over-reject far surfaces where adjacent pixels
            // differ by more than DepthThreshold in linear depth.
            float depthRatio = abs(nDepth - centerDepth) / max(centerDepth, 1e-3f);
            if (depthRatio > gConstants.DepthThreshold)
                continue;

            float4 nR0 = gReservoir0.Load(int3(nPixel, 0));
            float4 nR1 = gReservoir1.Load(int3(nPixel, 0));
            float3 nRadiance = nR0.rgb;
            float nM = max(nR1.x, 1.0f);
            float nW = max(nR1.y, 0.0f);

            float targetN = max(Luminance(nRadiance), 1e-6f);
            float wN = targetN * nW * nM;

            sumWeight += wN;
            M += nM;
            resolveSum += nRadiance * wN;
            resolveWeight += wN;

            uint seed = uint(dx + 1) * 13u + uint(dy + 1) * 7u;
            if (Hash01(uint2(pixel), seed) < wN / max(sumWeight, 1e-6f))
            {
                selectedRadiance = nRadiance;
                selectedTarget = targetN;
                selectedHitT = nR0.a;
            }
        }
    }

    W = sumWeight / max(M * selectedTarget, 1e-6f);
    M = min(M, gConstants.MaxM);

    // Resolve: MIS-weighted average of the center + accepted neighbors;
    // alpha carries the center sample's hit distance (ReBLUR consumption).
    float3 resolved = resolveSum / max(resolveWeight, 1e-6f);
    gOutput[pixel] = float4(resolved, selectedHitT);
}
