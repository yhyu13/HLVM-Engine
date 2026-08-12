// ReSTIR GI — Temporal Reuse Pass (RealEngine-modeled).
//
// Merges the current-frame reservoir (from Generation) with the reprojected
// history reservoir (previous frame's Temporal output) using weighted
// reservoir sampling:
//
//   w_hist = target(radiance_hist) * W_hist * M_hist
//   M = M_curr + M_hist
//   select hist sample with probability w_hist / (target_curr + w_hist)
//   W = (target_curr + w_hist) / (M * target_selected)
//   M = min(M, MaxM)
//
// Reprojection uses the real inverse-current-view-proj and prev-view-proj
// matrices. With a static camera the composition is identity, so prevUV ==
// currUV; with a moving camera the matrices carry the motion (the depth
// channel conversion below is exact only for a static camera — see
// PLAN.md §G3 for the follow-up).
//
// Depth/normal validation rejects history pixels that belong to different
// geometry (disocclusion / silhouette).

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
    float SceneYaw;      // Phase C: scene Y-rotation this frame (deg)
    float PrevSceneYaw;  // Phase C: scene Y-rotation previous frame (deg)
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
Texture2D<float4> gCurrRadiance : register(t8);
Texture2D<float4> gHistRadiance : register(t9);

RWTexture2D<float4> gOutReservoir0 : register(u0, space1);
RWTexture2D<float4> gOutReservoir1 : register(u1, space1);
RWTexture2D<float4> gOutRadiance : register(u2, space1);

float Luminance(float3 c)
{
    return dot(c, float3(0.2126, 0.7152, 0.0722));
}

// PCG-style hash -> [0,1)
float Hash01(uint2 p, uint f)
{
    uint state = p.x * 747796405u + p.y * 2891336453u + f * 277803737u;
    state = (state >> 13u) ^ state;
    state *= 0x85ebca6bu;
    state ^= state >> 16u;
    return float(state & 0xFFFFFFu) / float(0x1000000u);
}

// Octahedral direction packing (Phase B) — matches ReSTIR_Generate_cs.hlsl.
float2 OctEncode(float3 n)
{
    n /= (abs(n.x) + abs(n.y) + abs(n.z));
    if (n.z < 0.0)
        n.xy = (1.0 - abs(n.yx)) * (n.xy >= 0.0 ? 1.0 : -1.0);
    return n.xy * 0.5 + 0.5;
}
float3 OctDecode(float2 e)
{
    e = e * 2.0 - 1.0;
    float3 n = float3(e, 1.0 - abs(e.x) - abs(e.y));
    if (n.z < 0.0)
        n.xy = (1.0 - abs(n.yx)) * (n.xy >= 0.0 ? 1.0 : -1.0);
    return normalize(n);
}

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    float2 outputSize = gConstants.OutputSize;
    if (dispatchThreadID.x >= (uint)outputSize.x || dispatchThreadID.y >= (uint)outputSize.y)
        return;

    int2 pixel = int2(dispatchThreadID.xy);

    // =====================================================================
    // Current frame reservoir (from Generation: M=1, W=1)
    // =====================================================================
    float4 currR0 = gCurrReservoir0.Load(int3(pixel, 0));
    float4 currR1 = gCurrReservoir1.Load(int3(pixel, 0));
    float3 currRadiance = currR0.rgb;
    float currHitT = currR0.a;
    float currM = currR1.x;
    float currW = currR1.y;
    float3 currDir = OctDecode(currR1.zw);
    float targetCurr = max(Luminance(currRadiance), 1e-6f);

    // =====================================================================
    // Reproject current pixel to the previous frame
    // =====================================================================
    float currDepth = gDepth.Load(int3(pixel, 0));
    float2 uv = (float2(pixel) + 0.5f) * gConstants.RcpOutputSize;
    float2 ndc = float2(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f);

    // Exact NDC z from the linear view-space depth for the RH-ZO (GLM)
    // perspective used by UpdateViewConstants (near/far passed in Pad):
    //   ndc.z = (far+near)/(far-near) - 2*far*near/((far-near)*viewDepth)
    // With an approximate ndc.z the reprojected pixel drifts by a few pixels
    // and the depth/normal validation rejects every history sample (M never
    // accumulates). This form is exact for the static camera and correct for
    // camera motion once prev view-proj is real.
    float nearP = gConstants.Pad[0];
    float farP = gConstants.Pad[1];
    float ndcZ = 0.0f;
    if (currDepth > 1e-6f)
        ndcZ = (farP + nearP) / (farP - nearP) - (2.0f * farP * nearP) / ((farP - nearP) * currDepth);

    float4 worldPos = mul(gConstants.InverseCurrViewProj, float4(ndc.x, ndc.y, ndcZ, 1.0f));
    worldPos.xyz /= worldPos.w;

    // Phase C: object-aware reprojection for the scene turntable. The scene
    // rotates around Y between frames, so the previous-frame world position is
    // R_y(PrevSceneYaw - SceneYaw) * worldPos before applying PrevViewProj.
    float yawDelta = radians(gConstants.PrevSceneYaw - gConstants.SceneYaw);
    float yawCos = cos(yawDelta);
    float yawSin = sin(yawDelta);
    float3 objPrevPos = float3(
        yawCos * worldPos.x + yawSin * worldPos.z,
        worldPos.y,
        -yawSin * worldPos.x + yawCos * worldPos.z);

    float4 prevClip = mul(gConstants.PrevViewProj, float4(objPrevPos, 1.0f));
    prevClip.xyz /= prevClip.w;

    float2 prevUV = float2(prevClip.x * 0.5f + 0.5f, -prevClip.y * 0.5f + 0.5f);
    int2 prevPixel = int2(prevUV * outputSize);

    // =====================================================================
    // Depth/normal validation
    // =====================================================================
    bool historyValid = false;
    if (all(prevPixel >= int2(0, 0)) && all(prevPixel < int2(outputSize)))
    {
        float prevDepth = gPrevDepth.Load(int3(prevPixel, 0));
        float3 prevNormal = gPrevNormals.Load(int3(prevPixel, 0)).rgb * 2.0f - 1.0f;
        float3 currNormal = gNormals.Load(int3(pixel, 0)).rgb * 2.0f - 1.0f;

        float depthDiff = abs(prevDepth - currDepth);
        float normalDot = dot(normalize(currNormal), normalize(prevNormal));

        historyValid = (depthDiff < gConstants.DepthThreshold && normalDot > gConstants.NormalThreshold);
    }

    // =====================================================================
    // Weighted reservoir merge (RealEngine reservoir.hlsli semantics)
    // =====================================================================
    float M = currM;
    float W = currW;
    float3 selectedRadiance = currRadiance;
    float3 selectedDirection = currDir;
    float selectedTarget = targetCurr;
    float selectedHitT = currHitT;
    float sumWeight = targetCurr * currW * currM;

    if (historyValid)
    {
        float4 hR0 = gHistReservoir0.Load(int3(prevPixel, 0));
        float4 hR1 = gHistReservoir1.Load(int3(prevPixel, 0));
        float3 histRadiance = hR0.rgb;
        float histHitT = hR0.a;
        float histM = hR1.x;
        float histW = hR1.y;
        float3 histDir = OctDecode(hR1.zw);

        float targetHist = max(Luminance(histRadiance), 1e-6f);
        float wHist = targetHist * histW * histM;

        sumWeight += wHist;
        M = currM + histM;

        float rng = Hash01(uint2(pixel), uint(gConstants.FrameIndex));
        if (rng < wHist / max(sumWeight, 1e-6f))
        {
            selectedRadiance = histRadiance;
            selectedDirection = histDir;
            selectedTarget = targetHist;
            selectedHitT = histHitT;
        }

        W = sumWeight / max(M * selectedTarget, 1e-6f);
        M = min(M, gConstants.MaxM);
    }

    gOutReservoir0[pixel] = float4(selectedRadiance, selectedHitT);
    gOutReservoir1[pixel] = float4(M, W, OctEncode(selectedDirection));
    // Resolve: radiance * W estimates the integrated radiance (unbiased
    // under the luminance target — same weight applies to all channels).
    gOutRadiance[pixel] = float4(selectedRadiance * W, 1.0f);
}
