/*
 * Temporal Anti-Aliasing Compute Shader
 *
 * Input:  Current frame HDR, history frame, depth buffer
 * Output: TAA-filtered frame
 *
 * Algorithm:
 * 1. Reconstruct world position from depth + inverse view-proj
 * 2. Project to previous frame to compute motion vector
 * 3. Sample history at reprojected UV (bilinear)
 * 4. 3x3 neighborhood clamp in YCoCg color space
 * 5. Blend history with current frame
 */

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

cbuffer TAAConstants : register(b0)
{
    float4x4 InverseCurrViewProj;
    float4x4 PrevViewProj;
    float2   OutputSize;
    float2   RcpOutputSize;
    float    BlendFactor;
    float    DepthThreshold;
    float2   Pad;
};

// ---------------------------------------------------------------------------
// Resources
// ---------------------------------------------------------------------------

Texture2D<float4>   t_CurrentFrame  : register(t0);
Texture2D<float4>   t_HistoryFrame  : register(t1);
Texture2D<float>    t_Depth         : register(t2);

SamplerState LinearClamp : register(s0);

RWTexture2D<float4> u_TAAOutput : register(u0);

// ---------------------------------------------------------------------------
// Color Space Conversion (YCoCg)
// ---------------------------------------------------------------------------

float3 RGBToYCoCg(float3 rgb)
{
    float Y  = dot(rgb, float3(0.25,  0.50,  0.25));
    float Co = dot(rgb, float3(0.50,  0.00, -0.50));
    float Cg = dot(rgb, float3(-0.25, 0.50, -0.25));
    return float3(Y, Co, Cg);
}

float3 YCoCgToRGB(float3 ycocg)
{
    float Y  = ycocg.x;
    float Co = ycocg.y;
    float Cg = ycocg.z;
    float r = Y + Co - Cg;
    float g = Y + Cg;
    float b = Y - Co - Cg;
    return float3(r, g, b);
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

[numthreads(8, 8, 1)]
void main(uint2 dispatchThreadId : SV_DispatchThreadID)
{
    uint2 pixelCoord = dispatchThreadId;

    if (pixelCoord.x >= uint(OutputSize.x) || pixelCoord.y >= uint(OutputSize.y))
        return;

    float2 uv = (float2(pixelCoord) + 0.5) * RcpOutputSize;
    float4 current = t_CurrentFrame[pixelCoord];

    // =====================================================================
    // Motion Vector Computation (from depth + matrices)
    // =====================================================================

    float depth = t_Depth[pixelCoord];

    // Reconstruct world position
    float4 ndc = float4(uv * 2.0 - 1.0, depth, 1.0);
    float4 worldPos = mul(InverseCurrViewProj, ndc);
    worldPos.xyz /= worldPos.w;

    // Project to previous frame
    float4 prevClip = mul(PrevViewProj, float4(worldPos.xyz, 1.0));
    float2 prevUV = prevClip.xy / prevClip.w * float2(0.5, -0.5) + 0.5;

    float2 motion = prevUV - uv;
    float2 historyUV = uv + motion;

    // =====================================================================
    // Disocclusion Detection
    // =====================================================================

    bool bDisocclusion = false;

    // Off-screen history
    if (any(historyUV < 0.0) || any(historyUV > 1.0))
        bDisocclusion = true;

    // Depth mismatch (approximate: sample depth at reprojected location)
    if (!bDisocclusion)
    {
        int2 historyPixel = int2(historyUV * OutputSize);
        historyPixel = clamp(historyPixel, int2(0, 0), int2(OutputSize) - 1);
        float historyDepth = t_Depth[historyPixel];
        if (abs(historyDepth - depth) > DepthThreshold)
            bDisocclusion = true;
    }

    // =====================================================================
    // Sample History
    // =====================================================================

    float4 history = t_HistoryFrame.SampleLevel(LinearClamp, historyUV, 0);

    // =====================================================================
    // Neighborhood Clamp (YCoCg)
    // =====================================================================

    float3 ycocgCurrent = RGBToYCoCg(current.rgb);
    float3 ycocgMin = ycocgCurrent;
    float3 ycocgMax = ycocgCurrent;

    for (int dy = -1; dy <= 1; dy++)
    {
        for (int dx = -1; dx <= 1; dx++)
        {
            if (dx == 0 && dy == 0)
                continue;

            int2 neighbor = int2(pixelCoord) + int2(dx, dy);
            if (any(neighbor < 0) || any(neighbor >= int2(OutputSize)))
                continue;

            float3 ycocg = RGBToYCoCg(t_CurrentFrame[neighbor].rgb);
            ycocgMin = min(ycocgMin, ycocg);
            ycocgMax = max(ycocgMax, ycocg);
        }
    }

    // Clamp history to neighborhood
    float3 ycocgHistory = RGBToYCoCg(history.rgb);
    ycocgHistory = clamp(ycocgHistory, ycocgMin, ycocgMax);
    history.rgb = YCoCgToRGB(ycocgHistory);

    // =====================================================================
    // Blend
    // =====================================================================

    float blend = bDisocclusion ? 1.0 : BlendFactor;
    float4 output = lerp(history, current, blend);

    u_TAAOutput[pixelCoord] = output;
}
