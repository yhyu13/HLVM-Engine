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
    // v188: this copy previously declared `float Pad[3]` here and stopped.
    // FReSTIRPass::DispatchTemporal marshals field-by-field into a flat
    // float[64] and writes FIVE more scalars past DebugVis, unconditionally,
    // for every caller including this test (FReSTIRPass.cpp:441-456). The
    // shared C++ header (FReSTIRPass.h:40-60) and the TestReSTIR_GI_Temporal
    // copy both declare all five; this one did not.
    //
    // The array form was not merely short, it was displaced: HLSL puts each
    // constant-buffer ARRAY element on its own 16-byte register (the v184
    // rule), so with DebugVis at float 40 (register 10, slot .x), Pad[0] could
    // not sit at 41 — it landed at 44, Pad[1] at 48, Pad[2] at 52, against C++
    // writes at 41..45. Plain scalars pack tightly and none of these five
    // straddles a 16-byte boundary (41,42,43 = register 10 .y/.z/.w;
    // 44,45 = register 11 .x/.y), so C++ write order == HLSL read order.
    //
    // If a VECTOR is ever appended to this tail, the straddle rule bites
    // before the array rule does — a float2 starting at 43 would be bumped to
    // 44. Keep this tail scalar, in this order, matching the header.
    //
    // None of these five is read by this shader today; they are declared so
    // the layout agrees, not so the values are consumed.
    float SceneYaw;
    float PrevSceneYaw;
    float NearPlane;
    float FarPlane;
    float GBufferScale;
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
// v230 (card N): sync SRV count to FReSTIRPass::TemporalLayoutSRV (16 SRVs +
// RT AS). The shared layout advertises t8..t16; the previous version of this
// file declared only t0..t7, so its pipeline was built against a layout the
// SPIR-V did not contain. t8..t9 are the current/history radiance inputs,
// t10..t11 are Reservoir2 (Phase-2 octahedral-direction reservoir), t12..t13
// are the full-res primary surface (worldpos + material), t14..t15 are the
// true prev-frame counterparts, t16 is the segment-visibility TLAS.
Texture2D<float4> gCurrRadiance   : register(t8);
Texture2D<float4> gHistRadiance   : register(t9);
Texture2D<float4> gCurrReservoir2 : register(t10);
Texture2D<float4> gHistReservoir2 : register(t11);
Texture2D<float4> gWorldPos       : register(t12);
Texture2D<float4> gMaterial       : register(t13);
Texture2D<float4> gPrevWorldPos   : register(t14);
Texture2D<float4> gPrevMaterial   : register(t15);
RaytracingAccelerationStructure g_bvh : register(t16);

// v230 (card N): sync UAV count + space to FReSTIRPass::TemporalLayoutUAV.
// The shared layout declares 4 UAVs at registers u384..u387 (SPIR-V set 1,
// i.e. `register(uN, space1)`); the previous version of this file declared
// only 2 UAVs at `register(u0)`/`register(u1)` in the DEFAULT space (set 0).
// Without the explicit `space1`, Vulkan would put them in the wrong set and
// the descriptor-set binding would fail at dispatch. The control's
// TempDesc currently leaves u2/u3 at null → fall back to the dummy textures
// FReSTIRPass wires in, so the binding set is still populated and Vulkan
// validation stays green; the temporal pass is data-starved on these slots
// which is acceptable for this known-good control.
RWTexture2D<float4> gOutReservoir0 : register(u0, space1);
RWTexture2D<float4> gOutReservoir1 : register(u1, space1);
RWTexture2D<float4> gOutReservoir2 : register(u2, space1);
RWTexture2D<float4> gOutRadiance   : register(u3, space1);

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
