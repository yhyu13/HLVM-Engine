// ReSTIR Spatial Pass — Pairwise MIS spatial reuse
// Replaces naive reservoir merge with proper MIS-weighted combination of
// all valid neighbor reservoirs. Each reservoir contributes proportional to
// its w_sum, normalized by total M*p_hat across the neighborhood.
//
// When p_hat is position-independent (as in our GI luminance target),
// the Jacobian correction is 1.0 and pairwise MIS collapses to a simple
// weighted sum:  L = sum(w_sum_i * radiance_i) / sum(M_i * p_hat_i)

struct FReSTIRSpatialConstants
{
    float2 OutputSize;
    float2 RcpOutputSize;
    float NormalThreshold;
    float DepthThreshold;
    float MaxM;
    float SpatialRadius;
    float DebugVis;
    // v187: FReSTIRPass::DispatchSpatial marshals field-by-field into a flat
    // float[64] and writes GBufferScale at float 9 UNCONDITIONALLY, for every
    // caller including this test. This copy previously stopped at DebugVis and
    // declared `float2 Pad`, so that constant landed in Pad.x and was silently
    // swallowed. Byte-for-byte the wire is unchanged by this edit — float 9 is
    // written either way — but the field now has the name and the kind the
    // shared C++ header gives it (FReSTIRPass.h:70-72), matching the
    // TestReSTIR_GI_Temporal_Data copy verbatim.
    //
    // NOTE: this pass dispatches at FULL res here (TestCornellBoxGI.cpp
    // sets SpatDesc.OutputWidth/OutputHeight from CurrentFBInfo, the same
    // resolution as the GBuffer MRTs it samples), so the scale is 1 and no
    // GB() conversion is needed. Do NOT add one without also making the
    // dispatch half-res.
    float GBufferScale;
    float Pad;
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

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    float2 outputSize = gConstants.OutputSize;
    if (dispatchThreadID.x >= (uint)outputSize.x || dispatchThreadID.y >= (uint)outputSize.y)
        return;

    int2 pixel = int2(dispatchThreadID.xy);

    // =====================================================================
    // Load center pixel geometry
    // =====================================================================
    float3 centerNormal = normalize(gNormals.Load(int3(pixel, 0)).rgb * 2.0 - 1.0);
    float centerDepth = gDepth.Load(int3(pixel, 0));
    float maxM = gConstants.MaxM;

    // =====================================================================
    // Pairwise MIS accumulator
    //   numerator   = sum_i( w_sum_i * radiance(y_i) )
    //   denominator = sum_i( M_i * p_hat(y_i) )
    //   output      = numerator / denominator
    // =====================================================================
    float3 numerator = 0.0;
    float denominator = 0.0;

    // -----------------------------------------------------------------
    // Center pixel reservoir (always included)
    // -----------------------------------------------------------------
    {
        float4 r0 = gReservoir0.Load(int3(pixel, 0));
        float wSum = r0.z;
        float M = r0.w;

        if (M > maxM)
        {
            wSum *= maxM / M;
            M = maxM;
        }

        int2 samplePixel = clamp(int2(r0.xy + 0.5), int2(0), int2(outputSize) - int2(1));
        float3 rad = gRadiance.Load(int3(samplePixel, 0)).rgb;
        float phat = Luminance(rad);

        numerator += wSum * rad;
        denominator += M * phat;
    }

    // -----------------------------------------------------------------
    // Neighbor reservoirs (3x3, geometric rejection)
    // -----------------------------------------------------------------
    for (int dy = -1; dy <= 1; dy++)
    {
        for (int dx = -1; dx <= 1; dx++)
        {
            if (dx == 0 && dy == 0)
                continue;

            int2 nPixel = pixel + int2(dx, dy);
            if (any(nPixel < int2(0)) || any(nPixel >= int2(outputSize)))
                continue;

            float3 nNormal = normalize(gNormals.Load(int3(nPixel, 0)).rgb * 2.0 - 1.0);
            float nDepth = gDepth.Load(int3(nPixel, 0));

            if (dot(centerNormal, nNormal) < gConstants.NormalThreshold)
                continue;
            if (abs(nDepth - centerDepth) > gConstants.DepthThreshold)
                continue;

            float4 nR0 = gReservoir0.Load(int3(nPixel, 0));
            float nWSum = nR0.z;
            float nM = nR0.w;

            if (nM > maxM)
            {
                nWSum *= maxM / nM;
                nM = maxM;
            }

            int2 nSamplePixel = clamp(int2(nR0.xy + 0.5), int2(0), int2(outputSize) - int2(1));
            float3 nRad = gRadiance.Load(int3(nSamplePixel, 0)).rgb;
            float nPhat = Luminance(nRad);

            numerator += nWSum * nRad;
            denominator += nM * nPhat;
        }
    }

    float3 outRadiance = (denominator > 0.0) ? (numerator / denominator) : 0.0;
    gOutput[pixel] = float4(outRadiance, 1.0);
}
