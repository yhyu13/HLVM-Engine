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
    float  GuideScale;   // full-res guide extent / dispatch extent (see GB() below)
    float  Pad1;
    float  Pad2;
};

// This is the SHARED/default copy of this shader, compiled by the
// Common_ShaderMake target from Engine/Source/Runtime/Shader/ShaderMake.cfg.
// Selection between this copy and the two further copies under Test/*_Data/
// is driven by each consumer's FBilateralDenoisePass::Initialize(...,
// InShaderDataDir) argument: the DataDir the consumer passes composes the
// .sblob path the pass loads (FBilateralDenoisePass.cpp constructs the path
// from InShaderDataDir, not from any global override). The override
// FCommonRenderPasses::SetShaderDataDir() governs BLIT resources only (its
// sole consumer is InitBlitResources(), which loads BlitVS/BlitPS); it does
// not select this shader. A consumer that does NOT pass its own DataDir lands
// on this file by default, and its extents are unknowable from here. Keep the
// guide handling here in agreement with the primary copy because two further
// copies exist under Test/*_Data/; whichever copy a consumer compiles, the
// invariant "GB() maps a dispatch texel to its guide footprint's centre"
// (declared below) must hold.
//
// A pass may dispatch at a LOWER resolution than its depth/normal GUIDES
// (the input radiance matches the dispatch; the guides may be full-res
// GBuffer MRTs). Indexing the guides with the raw dispatch coord then samples
// a corner of the guide at short stride -- geometrically unrelated texels, so
// every depth and normal weight in the kernel is computed against the wrong
// surface. There is no VUID and no error; the output is merely wrong.
// GB() maps a dispatch texel to the CENTRE of its footprint in the guide.
// GuideScale == 0 (an unfilled constant) must degrade to the identity map,
// never to a divide-by-zero or a collapsed index.
int2 GB(int2 p)
{
    int s = max(int(GuideScale), 1);
    return p * s + (s / 2);
}

Texture2D<float4> t_Input   : register(t0);  // Noisy HDR RGBA input (RGB + hitDist in alpha)
Texture2D<float>  t_Depth  : register(t1);   // Depth guide
Texture2D<float4> t_Normal : register(t2);   // Normal guide (RGB + 1.0)
SamplerState      PointSampler : register(s0);

RWTexture2D<float4> u_Output : register(u0); // Denoised HDR RGBA output (RGB + propagated hitDist)

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

    // Center pixel data. t_Input matches the dispatch resolution so it uses
    // the raw coord; the two GUIDES may be larger and must go through GB().
    float centerDepth = t_Depth.Load(int3(GB(int2(pixelCoord)), 0));
    float3 centerNormal = normalize(t_Normal.Load(int3(GB(int2(pixelCoord)), 0)).rgb * 2.0 - 1.0);
    float4 centerValue = t_Input.Load(int3(pixelCoord, 0));

    float4 sum = centerValue;
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

            // Depth weight (guide may be larger -> GB())
            float neighborDepth = t_Depth.Load(int3(GB(neighborPixel), 0));
            float depthDiff = abs(neighborDepth - centerDepth);
            float wDepth = depthWeight(depthDiff, DepthSigma);

            // Normal weight (guide may be larger -> GB())
            float3 neighborNormal = normalize(t_Normal.Load(int3(GB(neighborPixel), 0)).rgb * 2.0 - 1.0);
            float wNormal = normalWeight(centerNormal, neighborNormal, NormalSigma);

            // Combined weight
            float weight = wSpatial * wDepth * wNormal;

            // Accumulate
            float4 neighborValue = t_Input.Load(int3(neighborPixel, 0));
            sum += neighborValue * weight;
            weightSum += weight;
        }
    }

    u_Output[pixelCoord] = sum / weightSum;
}