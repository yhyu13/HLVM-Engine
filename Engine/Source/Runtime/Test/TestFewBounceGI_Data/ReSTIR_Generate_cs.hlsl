// ReSTIR GI — Reservoir Generation Compute Pass (Phase 7a)
//
// Reads denoised GI radiance and GBuffer data to build per-pixel reservoirs.
// Each pixel generates exactly 1 sample (itself).
// Reservoir stores: world position, unbiased weight, weight sum, sample count.
//
// Phase 7b will add temporal reuse by merging with history reservoirs.

// =============================================================================
// Constants Buffer (b0 -> 256)
// =============================================================================

struct FReSTIRConstants
{
    float2 OutputSize;
    float2 RcpOutputSize;
    float FrameIndex;
    float DebugVis;       // 1.0 = output debug grayscale, 0.0 = normal
    float2 Pad;
};

cbuffer Constants : register(b0)
{
    FReSTIRConstants gConstants;
};

// =============================================================================
// Texture Bindings
// =============================================================================

// t0: Denoised HDR radiance (RGB = radiance, A = hit distance)
Texture2D<float4> gRadiance : register(t0);
// t1: GBuffer world position
Texture2D<float4> gWorldPos : register(t1);
// t2: GBuffer normals (RGB packed [0,1])
Texture2D<float4> gNormals : register(t2);
// t3: Linear depth
Texture2D<float> gDepth : register(t3);

// u0: Reservoir0 (RGB = world position, A = unbiased weight W)
RWTexture2D<float4> gReservoir0 : register(u0);
// u1: Reservoir1 (R = w_sum, G = M, B = pdf, A = unused)
RWTexture2D<float4> gReservoir1 : register(u1);
// u2: Debug visualization (grayscale reservoir weight)
RWTexture2D<float4> gDebugOutput : register(u2);

// =============================================================================
// Utility Functions
// =============================================================================

float Luminance(float3 rgb)
{
    return dot(rgb, float3(0.2126, 0.7152, 0.0722));
}

// =============================================================================
// Reservoir Generation
// =============================================================================

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    float2 outputSize;
    outputSize.x = gConstants.OutputSize[0];
    outputSize.y = gConstants.OutputSize[1];

    // Early out if thread is outside output bounds
    if (dispatchThreadID.x >= (uint)outputSize.x || dispatchThreadID.y >= (uint)outputSize.y)
    {
        return;
    }

    int2 pixel = int2(dispatchThreadID.xy);

    // Read inputs
    float4 radianceHitDist = gRadiance.Load(int3(pixel, 0));
    float3 radiance = radianceHitDist.rgb;
    float hitDist = radianceHitDist.a;

    float4 worldPosPacked = gWorldPos.Load(int3(pixel, 0));
    float3 worldPos = worldPosPacked.rgb;

    float4 normalPacked = gNormals.Load(int3(pixel, 0));
    float3 normal = normalPacked.rgb * 2.0 - 1.0;

    float depth = gDepth.Load(int3(pixel, 0)).r;

    // Handle sky / invalid pixels
    if (depth == 0.0 || all(radiance == float3(0.0, 0.0, 0.0)))
    {
        // Zero reservoir for sky/background
        gReservoir0[pixel] = float4(0.0, 0.0, 0.0, 0.0);
        gReservoir1[pixel] = float4(0.0, 0.0, 0.0, 0.0);
        gDebugOutput[pixel] = float4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    // Target function p_hat(y) = luminance of radiance
    // This is the quantity we want to importance-sample over the pixel grid.
    float p_hat = Luminance(radiance);
    p_hat = max(p_hat, 1e-6); // Prevent zero weights

    // For Phase 7a, each pixel generates exactly 1 sample (itself).
    // Sampling PDF p(y) = 1.0 (uniform over pixel grid).
    // Therefore: w_sum = p_hat / p(y) = p_hat
    float w_sum = p_hat;
    float M = 1.0;
    float pdf = 1.0;

    // Unbiased contribution weight: W = w_sum / (M * p(y))
    // Since p(y) = 1.0: W = w_sum / M = p_hat
    float W = w_sum / max(M, 1e-6);

    // Pack and write reservoir data
    gReservoir0[pixel] = float4(worldPos, W);
    gReservoir1[pixel] = float4(w_sum, M, pdf, hitDist);

    // Debug visualization: grayscale based on W (log-scaled for visibility)
    if (gConstants.DebugVis > 0.5)
    {
        float logW = log10(1.0 + W);
        float debugGray = saturate(logW / 2.0); // Normalize roughly
        gDebugOutput[pixel] = float4(debugGray, debugGray, debugGray, 1.0);
    }

}
