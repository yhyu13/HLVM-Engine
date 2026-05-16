/*
 * Joint Bilateral Upsample Compute Shader
 *
 * Performs depth-aware edge-preserving filtering. Can be used for:
 *   - Same-resolution denoising (e.g., SSAO blur)
 *   - Upsampling lower-resolution effects (e.g., half-res bloom, SSR)
 *
 * Weights each neighbor by exp(-depthDiff^2 / (2*sigma^2)).
 * Large depth differences -> weight ~ 0 (edge preserved).
 * Small depth differences -> weight ~ 1 (smooth blending).
 */

cbuffer Constants : register(b0)
{
    float2 InputTexelSize;   // 1.0 / InputWidth, 1.0 / InputHeight
    float2 OutputTexelSize;  // 1.0 / OutputWidth, 1.0 / OutputHeight
    float  DepthSigma;
    float  Pad0;
    float  Pad1;
    float  Pad2;
};

Texture2D<float>   t_Input  : register(t0);
Texture2D<float>   t_Depth  : register(t1);
SamplerState       PointSampler : register(s0);

RWTexture2D<float> u_Output : register(u0);

[numthreads(8, 8, 1)]
void main(uint2 dispatchThreadId : SV_DispatchThreadID)
{
    uint2 pixelCoord = dispatchThreadId;
    uint2 outputSize = uint2(uint(1.0f / OutputTexelSize.x), uint(1.0f / OutputTexelSize.y));

    if (pixelCoord.x >= outputSize.x || pixelCoord.y >= outputSize.y)
        return;

    // Center pixel depth (from full-res depth guide)
    float centerDepth = t_Depth[pixelCoord];

    // UV for sampling input texture (handles resolution differences)
    float2 centerUV = (float2(pixelCoord) + 0.5) * OutputTexelSize;
    float centerValue = t_Input.SampleLevel(PointSampler, centerUV, 0);

    float sum = centerValue;
    float weightSum = 1.0;

    // 3x3 bilateral kernel
    [unroll]
    for (int y = -1; y <= 1; y++)
    {
        [unroll]
        for (int x = -1; x <= 1; x++)
        {
            if (x == 0 && y == 0)
                continue;

            int2 neighborPixel = int2(pixelCoord) + int2(x, y);

            // Bounds check
            if (any(neighborPixel < 0) || any(neighborPixel >= int2(outputSize)))
                continue;

            float neighborDepth = t_Depth[uint2(neighborPixel)];
            float depthDiff = neighborDepth - centerDepth;
            float weight = exp(-(depthDiff * depthDiff) / (2.0 * DepthSigma * DepthSigma));

            float2 neighborUV = (float2(neighborPixel) + 0.5) * OutputTexelSize;
            float neighborValue = t_Input.SampleLevel(PointSampler, neighborUV, 0);

            sum += neighborValue * weight;
            weightSum += weight;
        }
    }

    u_Output[pixelCoord] = sum / weightSum;
}
