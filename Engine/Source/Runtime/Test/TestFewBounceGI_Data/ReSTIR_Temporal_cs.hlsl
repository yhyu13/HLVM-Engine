// ReSTIR GI — Temporal Reservoir Reuse Compute Pass (Phase 7b)
//
// Merges current frame reservoirs with reprojected previous frame reservoirs.
// Each pixel combines its current sample with the temporally reprojected history.
//
// Phase 7a generates reservoirs. Phase 7b merges them temporally.
// Phase 8 will add spatial reuse.

// =============================================================================
// Constants Buffer (b0 -> 256)
// =============================================================================

struct FReSTIRTemporalConstants
{
    float4x4 InverseCurrViewProj;  // Current frame inverse ViewProj
    float4x4 PrevViewProj;         // Previous frame ViewProj
    float2 OutputSize;
    float2 RcpOutputSize;
    float FrameIndex;
    float MaxM;
    float DebugVis;       // 1.0 = show M, 2.0 = show W
    float Pad;
};

cbuffer Constants : register(b0)
{
    FReSTIRTemporalConstants gConstants;
};

// =============================================================================
// Texture Bindings
// =============================================================================

// t0: Current Reservoir0 (worldPos.xyz, W)
Texture2D<float4> gCurrReservoir0 : register(t0);
// t1: Current Reservoir1 (w_sum, M, pdf, hitDist)
Texture2D<float4> gCurrReservoir1 : register(t1);
// t2: History Reservoir0
Texture2D<float4> gHistReservoir0 : register(t2);
// t3: History Reservoir1
Texture2D<float4> gHistReservoir1 : register(t3);
// t4: Linear depth
Texture2D<float> gDepth : register(t4);

// u0: Merged Reservoir0
RWTexture2D<float4> gOutReservoir0 : register(u0);
// u1: Merged Reservoir1
RWTexture2D<float4> gOutReservoir1 : register(u1);
// u2: Debug visualization
RWTexture2D<float4> gDebugOutput : register(u2);

// =============================================================================
// Constants
// =============================================================================

static const float MAX_M = 30.0;

// =============================================================================
// Utility Functions
// =============================================================================

float hash(uint2 p, uint frame)
{
    uint n = p.x * 15843u + p.y * 23457u + frame * 14325u;
    n ^= n >> 16;
    n *= 0x7feb352dU;
    n ^= n >> 15;
    n *= 0x846ca68bU;
    n ^= n >> 15;
    return float(n) / float(0xFFFFFFFFU);
}

// Reconstruct view-space position from UV and depth
float3 ReconstructViewPos(float2 uv, float depth)
{
    float4 clipPos = float4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    float4 viewPos = mul(gConstants.InverseCurrViewProj, clipPos);
    return viewPos.xyz / viewPos.w;
}

// Reproject UV to previous frame
float2 ReprojectUV(float2 uv, float depth)
{
    // Current clip -> world -> previous clip
    float4 clipPos = float4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    float4 worldPos = mul(gConstants.InverseCurrViewProj, clipPos);
    worldPos /= worldPos.w;
    float4 prevClip = mul(gConstants.PrevViewProj, worldPos);
    prevClip /= prevClip.w;
    return prevClip.xy * 0.5 + 0.5;
}

// =============================================================================
// Reservoir Structure
// =============================================================================

struct Reservoir
{
    float3 y;       // Sample position (worldPos)
    float W;        // Unbiased contribution weight
    float w_sum;    // Weight sum
    float M;        // Sample count
    float pdf;      // Sampling PDF
    float hitDist;  // Hit distance
};

Reservoir LoadReservoir(Texture2D<float4> r0, Texture2D<float4> r1, int2 pixel)
{
    Reservoir r;
    float4 d0 = r0.Load(int3(pixel, 0));
    float4 d1 = r1.Load(int3(pixel, 0));
    r.y = d0.xyz;
    r.W = d0.w;
    r.w_sum = d1.x;
    r.M = d1.y;
    r.pdf = d1.z;
    r.hitDist = d1.w;
    return r;
}

void StoreReservoir(Reservoir r, int2 pixel)
{
    gOutReservoir0[pixel] = float4(r.y, r.W);
    gOutReservoir1[pixel] = float4(r.w_sum, r.M, r.pdf, r.hitDist);
}

// =============================================================================
// Reservoir Merge
// =============================================================================

Reservoir MergeReservoirs(Reservoir curr, Reservoir hist, uint2 pixel)
{
    // If history is invalid, return current
    if (hist.M <= 0.0)
    {
        return curr;
    }

    // Combined sample count (capped)
    float M_combined = min(curr.M + hist.M, gConstants.MaxM);

    // Combined weight sum
    // Since p(y) = 1.0 (uniform), the Jacobian for temporal reprojection is ~1.0
    float w_sum_combined = curr.w_sum + hist.w_sum;

    // Select winner proportional to weights
    float rand = hash(pixel, (uint)gConstants.FrameIndex);
    float selectHist = hist.w_sum / max(w_sum_combined, 1e-6);

    Reservoir merged;
    if (rand < selectHist)
    {
        merged.y = hist.y;
        merged.hitDist = hist.hitDist;
    }
    else
    {
        merged.y = curr.y;
        merged.hitDist = curr.hitDist;
    }

    merged.w_sum = w_sum_combined;
    merged.M = M_combined;
    merged.pdf = 1.0; // Uniform PDF

    // Unbiased weight: W = w_sum / (M * p(y))
    // p(y) = 1.0 for our simplified model
    merged.W = merged.w_sum / max(merged.M, 1e-6);

    return merged;
}

// =============================================================================
// Main Entry Point
// =============================================================================

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    float2 outputSize;
    outputSize.x = gConstants.OutputSize[0];
    outputSize.y = gConstants.OutputSize[1];

    if (dispatchThreadID.x >= (uint)outputSize.x || dispatchThreadID.y >= (uint)outputSize.y)
    {
        return;
    }

    int2 pixel = int2(dispatchThreadID.xy);

    // Read current reservoir
    Reservoir curr = LoadReservoir(gCurrReservoir0, gCurrReservoir1, pixel);

    // Read depth and reproject to previous frame
    float depth = gDepth.Load(int3(pixel, 0)).r;
    float2 uv = (dispatchThreadID.xy + 0.5) * float2(gConstants.RcpOutputSize[0], gConstants.RcpOutputSize[1]);
    float2 historyUv = ReprojectUV(uv, depth);

    // Load history reservoir
    Reservoir hist;
    hist.M = 0.0; // Default invalid

    bool historyValid = all(historyUv > 0.0) && all(historyUv < 1.0);
    if (historyValid)
    {
        int2 histPixel = int2(historyUv * outputSize);
        hist = LoadReservoir(gHistReservoir0, gHistReservoir1, histPixel);

        // Additional validation: check depth consistency
        float histDepth = gDepth.Load(int3(histPixel, 0)).r;
        float depthDiff = abs(depth - histDepth);
        if (depthDiff > 0.05 || hist.M <= 0.0)
        {
            historyValid = false;
            hist.M = 0.0;
        }
    }

    // Merge reservoirs
    Reservoir merged = MergeReservoirs(curr, hist, pixel);

    // Store merged result
    StoreReservoir(merged, pixel);

    // Debug visualization
    if (gConstants.DebugVis > 0.5)
    {
        if (gConstants.DebugVis > 1.5)
        {
            // Show W as grayscale (log scaled)
            float logW = log10(1.0 + merged.W);
            float debugGray = saturate(logW / 2.0);
            gDebugOutput[pixel] = float4(debugGray, debugGray, debugGray, 1.0);
        }
        else
        {
            // Show M as grayscale (linear, normalized to MaxM)
            float debugGray = saturate(merged.M / gConstants.MaxM);
            gDebugOutput[pixel] = float4(debugGray, debugGray, debugGray, 1.0);
        }
    }
}
