// ReSTIR Temporal Pass - Merge current frame reservoirs with reprojected history
// NOTE: Does NOT do TAA-style radiance blending. Temporal accumulation is via reservoir M/w_sum only.

struct FReSTIRTemporalConstants
{
    float4x4 InverseCurrViewProj;
    float4x4 PrevViewProj;
    float2 OutputSize;
    float2 RcpOutputSize;
    float FrameIndex;
    float MaxM;
    float DepthThreshold;
    float NormalThreshold;
    float DebugVis;
    float Pad[3];
};

cbuffer Constants : register(b0)
{
    FReSTIRTemporalConstants gConstants;
}

Texture2D<float4> gCurrReservoir0 : register(t0);
Texture2D<float4> gCurrReservoir1 : register(t1);
Texture2D<float4> gHistReservoir0 : register(t2);
Texture2D<float4> gHistReservoir1 : register(t3);
Texture2D<float> gDepth : register(t4);
Texture2D<float4> gNormals : register(t5);
Texture2D<float> gPrevDepth : register(t6);
Texture2D<float4> gPrevNormals : register(t7);

RWTexture2D<float4> gOutReservoir0 : register(u0);
RWTexture2D<float4> gOutReservoir1 : register(u1);

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    float2 outputSize = gConstants.OutputSize;
    if (dispatchThreadID.x >= (uint)outputSize.x || dispatchThreadID.y >= (uint)outputSize.y)
        return;

    int2 pixel = int2(dispatchThreadID.xy);

    // =====================================================================
    // Load current frame reservoir
    // =====================================================================
    float4 currR0 = gCurrReservoir0.Load(int3(pixel, 0));
    float4 currR1 = gCurrReservoir1.Load(int3(pixel, 0));

    float2 currY = currR0.xy;
    float currWSum = currR0.z;
    float currM = currR0.w;
    float currW = currR1.x;
    float currPhat = currR1.y;

    // =====================================================================
    // Current pixel world position for reprojection
    // =====================================================================
    float currDepth = gDepth.Load(int3(pixel, 0));

    // NDC coordinates: [-1, 1]
    // Note: Vulkan viewport has y pointing down, but GLM projection expects y up.
    // We flip y when converting from pixel to NDC.
    float2 uv = (float2(pixel) + 0.5) * gConstants.RcpOutputSize;
    float2 ndc = float2(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0);

    // Unproject to world space using inverse view-proj
    // HLSL mul(matrix, vector) uses column-vector convention, matching GLM
    float4 worldPos = mul(gConstants.InverseCurrViewProj, float4(ndc.x, ndc.y, currDepth, 1.0));
    worldPos.xyz /= worldPos.w;

    // Project to previous frame
    float4 prevClip = mul(gConstants.PrevViewProj, float4(worldPos.xyz, 1.0));
    prevClip.xyz /= prevClip.w;

    // Previous frame pixel coordinates
    // Flip y back to screen space (pixel y=0 at top)
    float2 prevUV = float2(prevClip.x * 0.5 + 0.5, -prevClip.y * 0.5 + 0.5);
    int2 prevPixel = int2(prevUV * outputSize);

    // =====================================================================
    // Reservoir temporal merge
    // =====================================================================
    bool historyValid = false;
    float histWSum = 0.0;
    float histM = 0.0;
    float2 histY = float2(0, 0);
    float histPhat = 0.0;

    // Check if reprojected pixel is within bounds
    if (all(prevPixel >= int2(0, 0)) && all(prevPixel < int2(outputSize)))
    {
        float prevDepth = gPrevDepth.Load(int3(prevPixel, 0));
        float3 prevNormal = gPrevNormals.Load(int3(prevPixel, 0)).rgb * 2.0 - 1.0;
        float3 currNormal = gNormals.Load(int3(pixel, 0)).rgb * 2.0 - 1.0;

        // Validate with depth and normal thresholds
        float depthDiff = abs(prevDepth - currDepth);
        float normalDot = dot(normalize(currNormal), normalize(prevNormal));

        if (depthDiff < gConstants.DepthThreshold && normalDot > gConstants.NormalThreshold)
        {
            float4 hR0 = gHistReservoir0.Load(int3(prevPixel, 0));
            float4 hR1 = gHistReservoir1.Load(int3(prevPixel, 0));

            histY = hR0.xy;
            histWSum = hR0.z;
            histM = hR0.w;
            histPhat = hR1.y;
            historyValid = (histM > 0.0);
        }
    }

    // =====================================================================
    // Temporal merge: combine current and history reservoirs
    // =====================================================================
    float combinedWSum = currWSum;
    float combinedM = currM;
    float2 combinedY = currY;
    float combinedPhat = currPhat;

    if (historyValid)
    {
        combinedWSum += histWSum;
        combinedM += histM;

        // Select winner proportional to weights
        float totalWeight = currWSum + histWSum;
        if (totalWeight > 0.0)
        {
            uint seed = uint(gConstants.FrameIndex * 7919u) + uint(pixel.x * 65537u) + uint(pixel.y * 524287u);
            float r = frac(float(seed) * 0.6180339887);
            if (r * totalWeight < histWSum)
            {
                combinedY = histY;
                combinedPhat = histPhat;
            }
        }
    }

    // Clamp M to prevent explosion, scaling w_sum proportionally
    float maxM = gConstants.MaxM;
    if (combinedM > maxM)
    {
        combinedWSum *= maxM / combinedM;
        combinedM = maxM;
    }

    // Recompute W with the new combined values
    float combinedW = (combinedPhat > 0.0) ? (combinedWSum / (combinedM * combinedPhat)) : 0.0;

    gOutReservoir0[pixel] = float4(combinedY, combinedWSum, combinedM);
    gOutReservoir1[pixel] = float4(combinedW, combinedPhat, 0.0, 0.0);
}
