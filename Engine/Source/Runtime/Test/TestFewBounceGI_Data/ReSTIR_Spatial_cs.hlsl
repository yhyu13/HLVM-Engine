// ReSTIR Spatial Reuse Pass
// Merges temporally-merged reservoirs with 3x3 spatial neighbors

// =============================================================================
// Bindings
// =============================================================================
// b0: Constants
struct ReSTIRSpatialConstants
{
    float2 OutputSize;
    float2 RcpOutputSize;
    float NormalSigma;
    float PlaneSigma;
    float DepthSigma;
    float MaxM;
    float SpatialRadius;
    float DebugVis;
    float2 _pad;
};
ConstantBuffer<ReSTIRSpatialConstants> gConstants : register(b0);

// t0: Merged reservoir data (packed)
Texture2D<float4> gMergedReservoir0 : register(t0);
// t1: Merged reservoir data (packed)
Texture2D<float4> gMergedReservoir1 : register(t1);
// t2: Normal + roughness
Texture2D<float4> gNormalRoughness : register(t2);
// t3: Depth
Texture2D<float> gDepth : register(t3);
// t4: Input radiance (denoised HDR)
Texture2D<float4> gRadiance : register(t4);

// u0: Spatially stable radiance (RGB = center radiance, A = W_merged)
RWTexture2D<float4> gOutRadiance : register(u0);
// u1: Debug visualization
RWTexture2D<float4> gDebugOutput : register(u1);

// =============================================================================
// Utility Functions
// =============================================================================

// Simple hash for deterministic random numbers
uint hash(uint2 p, uint seed)
{
    uint h = p.x * 374761393u + p.y * 668265263u + seed;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

float3 ReconstructViewPos(float2 uv, float depth)
{
    // Simple perspective reconstruction (assumes standard projection)
    // Not used directly, but kept for compatibility
    return float3(0.0, 0.0, depth);
}

float3 DecodeNormal(float3 packedNormal)
{
    return packedNormal * 2.0 - 1.0;
}

// Reservoir structure (packed into two float4 textures)
struct Reservoir
{
    float3 y;           // Sampled point/direction
    float hitDist;      // Hit distance
    float w_sum;        // Weighted sum
    float M;            // Sample count
    float W;            // Final weight
    uint seed;          // Random seed
};

Reservoir LoadReservoir(Texture2D<float4> r0, Texture2D<float4> r1, int2 pixel)
{
    Reservoir r;
    float4 data0 = r0.Load(int3(pixel, 0));
    float4 data1 = r1.Load(int3(pixel, 0));
    
    r.y = data0.xyz;
    r.hitDist = data0.w;
    r.w_sum = data1.x;
    r.M = data1.y;
    r.W = data1.z;
    r.seed = asuint(data1.w);
    
    return r;
}

void StoreReservoir(RWTexture2D<float4> r0, RWTexture2D<float4> r1, int2 pixel, Reservoir r)
{
    r0[pixel] = float4(r.y, r.hitDist);
    r1[pixel] = float4(r.w_sum, r.M, r.W, asfloat(r.seed));
}

// =============================================================================
// Spatial Merge: Merge center reservoir with 3x3 neighbors
// =============================================================================
Reservoir SpatialMerge(Reservoir center, int2 centerPixel, float3 centerNormal, float centerDepth)
{
    Reservoir spatial = center;
    float totalW = center.w_sum;
    
    float maxM = gConstants.MaxM;
    float normalSigma = gConstants.NormalSigma;
    float planeSigma = gConstants.PlaneSigma;
    float depthSigma = gConstants.DepthSigma;
    int radius = int(gConstants.SpatialRadius);
    
    // 3x3 spatial neighbors
    for (int dy = -radius; dy <= radius; dy++)
    {
        for (int dx = -radius; dx <= radius; dx++)
        {
            if (dx == 0 && dy == 0)
                continue;
                
            int2 neighborPixel = centerPixel + int2(dx, dy);
            
            // Bounds check
            float2 outputSize = float2(gConstants.OutputSize[0], gConstants.OutputSize[1]);
            if (any(neighborPixel < 0) || any(neighborPixel >= int2(outputSize)))
                continue;
            
            // Load neighbor data
            Reservoir neighbor = LoadReservoir(gMergedReservoir0, gMergedReservoir1, neighborPixel);
            float3 neighborNormal = DecodeNormal(gNormalRoughness.Load(int3(neighborPixel, 0)).rgb);
            float neighborDepth = gDepth.Load(int3(neighborPixel, 0)).r;
            
            // 1. Normal weight
            float normalDot = dot(centerNormal, neighborNormal);
            float normalAngle = acos(saturate(normalDot));
            float nWeight = exp(-normalAngle * normalSigma * 10.0);
            
            // 2. Plane distance weight
            // Skip plane distance check - use simpler approximation
            float pWeight = 1.0;
            
            // 3. Depth weight
            float depthDiff = abs(centerDepth - neighborDepth);
            float dWeight = exp(-depthDiff * depthSigma * 100.0);
            
            // Combined geometric weight
            float geomWeight = nWeight * pWeight * dWeight;
            
            // Reject if geometry differs too much
            if (geomWeight < 0.01)
                continue;
            
            // Merge neighbor reservoir into spatial
            float neighborW = neighbor.w_sum * geomWeight;
            spatial.M = min(spatial.M + neighbor.M, maxM);
            totalW += neighborW;
            
            // Select winner proportional to weights
            float rand = hash(centerPixel, (uint)(spatial.M + (dx + 3) * 7 + (dy + 3) * 13));
            if (rand < neighborW / totalW)
            {
                spatial.y = neighbor.y;
                spatial.hitDist = neighbor.hitDist;
            }
        }
    }
    
    // Update weight
    if (totalW > 0.0)
    {
        spatial.W = spatial.w_sum / totalW;
    }
    
    return spatial;
}

// =============================================================================
// Entry Point
// =============================================================================
[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    float2 outputSize = float2(gConstants.OutputSize[0], gConstants.OutputSize[1]);
    if (dispatchThreadID.x >= (uint)outputSize.x || dispatchThreadID.y >= (uint)outputSize.y)
    {
        return;
    }

    int2 pixel = int2(dispatchThreadID.xy);
    
    // Load center data
    Reservoir center = LoadReservoir(gMergedReservoir0, gMergedReservoir1, pixel);
    float3 centerNormal = DecodeNormal(gNormalRoughness.Load(int3(pixel, 0)).rgb);
    float centerDepth = gDepth.Load(int3(pixel, 0)).r;
    
    // Skip background pixels
    if (centerDepth == 0.0)
    {
        gOutRadiance[pixel] = float4(0.0, 0.0, 0.0, 0.0);
        gDebugOutput[pixel] = float4(0.0, 0.0, 0.0, 1.0);
        return;
    }
    
    // Spatial merge with neighbors
    Reservoir spatial = SpatialMerge(center, pixel, centerNormal, centerDepth);
    
    // Output radiance from original denoised input (not neighbor colors)
    float3 centerRadiance = gRadiance.Load(int3(pixel, 0)).rgb;
    gOutRadiance[pixel] = float4(centerRadiance, spatial.W);
    
    // Debug visualization
    float debugVis = gConstants.DebugVis;
    if (debugVis > 0.5)
    {
        // Visualize spatial merge weight
        float visW = spatial.W / 30.0;
        gDebugOutput[pixel] = float4(visW, visW * 0.5, 1.0 - visW, 1.0);
    }
    else
    {
        gDebugOutput[pixel] = float4(centerRadiance, 1.0);
    }
}
