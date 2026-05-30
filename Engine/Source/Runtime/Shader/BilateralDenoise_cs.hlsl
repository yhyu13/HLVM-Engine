/*
 * Bilateral Denoise Compute Shader - HDR RGB GI Denoising
 *
 * Performs depth-aware and normal-aware edge-preserving filtering for noisy
 * few-bounce GI results.
 *
 * Weight = spatial * depth * normal
 *   - spatial: exp(-dist^2 / (2 * sigma^2))
 *   - depth: exp(-depthDiff^2 / (2 * sigma^2))
 *   - normal: smoothstep based on normal dot product
 *
 * 5x5 kernel balances quality vs performance.
 */

cbuffer Constants : register(b0)
{
    float2 TexelSize;    // 1.0 / width, 1.0 / height
    float  DepthSigma;   // Depth tolerance (smaller = sharper edges)
    float  NormalSigma;  // Normal tolerance in radians (smaller = sharper edges)
    float  SpatialSigma; // Spatial falloff (larger = more blur)
    float  Pad0;
    float  Pad1;
    float  Pad2;
};

Texture2D<float3> t_Input   : register(t0);  // Noisy HDR RGB input
Texture2D<float>  t_Depth  : register(t1);   // Depth guide
Texture2D<float4> t_Normal : register(t2);   // Normal guide (RGB + 1.0)
SamplerState      PointSampler : register(s0);

RWTexture2D<float3> u_Output : register(u0); // Denoised HDR RGB output

// Gaussian weight for spatial distance
float spatialWeight(float distSq, float sigma)
{
    return exp(-distSq / (2.0 * sigma * sigma));
}

// Depth bilateral weight
float depthWeight(float depthDiff, float sigma)
{
    return exp(-(depthDiff * depthDiff) / (2.0 * sigma * sigma));
}

// Normal bilateral weight (based on dot product)
float normalWeight(float3 n1, float3 n2, float sigma)
{
    float dotProd = dot(n1, n2);
    // Convert sigma (radians) to dot product tolerance
    // sigmaSmall -> dot close to 1, sigmaLarge -> dot can be smaller
    float threshold = cos(sigma);
    // Smooth step from 0 at threshold to 1 at 1
    return smoothstep(0.0, 1.0, (dotProd - threshold) / (1.0 - threshold + 0.0001));
}

[NumThreads(8, 8, 1)]
void main(uint2 dispatchThreadId : SV_DispatchThreadID)
{
    uint2 pixelCoord = dispatchThreadId;
    uint2 outputSize = uint2(uint(1.0 / TexelSize.x), uint(1.0 / TexelSize.y));

    if (pixelCoord.x >= outputSize.x || pixelCoord.y >= outputSize.y)
        return;

    // Center pixel data
    float centerDepth = t_Depth[pixelCoord];
    float3 centerNormal = normalize(t_Normal[pixelCoord].rgb * 2.0 - 1.0);
    float3 centerValue = t_Input[pixelCoord];

    float3 sum = centerValue;
    float weightSum = 1.0;

    // 5x5 bilateral kernel
    // Kernel offsets: -2, -1, 0, 1, 2
    [unroll]
    for (int y = -2; y <= 2; y++)
    {
        [unroll]
        for (int x = -2; x <= 2; x++)
        {
            if (x == 0 && y == 0)
                continue;

            int2 neighborPixel = int2(pixelCoord) + int2(x, y);

            // Bounds check
            if (any(neighborPixel < 0) || any(neighborPixel >= int2(outputSize)))
                continue;

            // Distance^2 for spatial weight
            float distSq = float(x * x + y * y);

            // Spatial weight
            float wSpatial = spatialWeight(distSq, SpatialSigma);

            // Depth weight
            float neighborDepth = t_Depth[uint2(neighborPixel)];
            float depthDiff = abs(neighborDepth - centerDepth);
            float wDepth = depthWeight(depthDiff, DepthSigma);

            // Normal weight
            float3 neighborNormal = normalize(t_Normal[uint2(neighborPixel)].rgb * 2.0 - 1.0);
            float wNormal = normalWeight(centerNormal, neighborNormal, NormalSigma);

            // Combined weight
            float weight = wSpatial * wDepth * wNormal;

            // Accumulate
            float3 neighborValue = t_Input[uint2(neighborPixel)];
            sum += neighborValue * weight;
            weightSum += weight;
        }
    }

    u_Output[pixelCoord] = sum / weightSum;
}