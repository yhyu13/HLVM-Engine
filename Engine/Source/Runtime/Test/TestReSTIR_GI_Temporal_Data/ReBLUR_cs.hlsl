// ReBLUR-style Diffuse GI Denoiser - Temporal accumulation + spatial blur

struct DiffSH
{
    float3 radiance;
    float hitDist;
};

struct FReBLURConstants
{
    float4x4 InverseCurrViewProj;
    float4x4 PrevViewProj;
    float4x4 ViewMatrix;
    float4x4 ProjMatrix;
    float2 OutputSize;
    float2 RcpOutputSize;
    float4 HitDistParams;
    float BlurRadius;
    float NormalWeight;
    float PlaneWeight;
    float RoughnessWeight;
    float AntiLagIntensity;
    float DarknessSensitivity;
    float FrameIndex;
    float HistoryFadeIn;
    float ConfidenceScale;
    float SpatialAlpha;
    float PassIndex;
    float Pad;
};

cbuffer Constants : register(b0)
{
    FReBLURConstants gConstants;
};

Texture2D<float4> gCurrentRadiance : register(t0);
Texture2D<float4> gHistory : register(t1);
Texture2D<float> gDepth : register(t2);
Texture2D<float4> gNormalRoughness : register(t3);

SamplerState gPointSampler : register(s0);
SamplerState gLinearSampler : register(s1);

RWTexture2D<float4> gOutput : register(u0);

float GetNormHitDist(float hitDist, float viewZ, float roughness)
{
    float A = gConstants.HitDistParams[0];
    float B = gConstants.HitDistParams[1];
    float D = gConstants.HitDistParams[3];
    float roughnessScale = exp2(D * roughness * roughness);
    float f = (A + abs(viewZ) * B) * roughnessScale;
    return saturate(hitDist / f);
}

bool IsHistoryValid(float2 historyUv)
{
    return all(historyUv > 0.0) && all(historyUv < 1.0);
}

DiffSH TemporalAccumulation(DiffSH current, DiffSH history, bool historyValid, float frameIndex, float historyFadeIn)
{
    if (!historyValid)
        return current;
    // Exponential moving average: early frames blend more current to avoid
    // ghosting from stale history; later frames converge to steady-state alpha.
    float alpha = 1.0 / max(frameIndex + 1.0, 1.0);
    float steadyAlpha = 1.0 / max(historyFadeIn, 1.0);
    alpha = max(alpha, steadyAlpha);
    DiffSH result;
    result.radiance = lerp(history.radiance, current.radiance, alpha);
    result.hitDist = lerp(history.hitDist, current.hitDist, alpha);
    return result;
}

float ComputeAntiLagScale(DiffSH current, DiffSH history)
{
    float3 delta = abs(current.radiance - history.radiance);
    float lumaDelta = dot(delta, float3(0.299, 0.587, 0.114));
    float darknessSensitivity = gConstants.DarknessSensitivity;
    float currentLuma = dot(current.radiance, float3(0.299, 0.587, 0.114));
    float historyLuma = dot(history.radiance, float3(0.299, 0.587, 0.114));
    float denominator = max(currentLuma, historyLuma) + darknessSensitivity;
    float deltaNorm = lumaDelta / denominator;
    float deltaSmooth = smoothstep(0.03, 0.2, deltaNorm);
    float intensity = gConstants.AntiLagIntensity;
    float scale = lerp(1.0, deltaSmooth, intensity);
    return scale;
}

// NxN bilateral spatial blur with normal + linear-depth rejection.
// 2026-08-09: the old plane-distance rejection reconstructed a view-space
// position from the linear-depth texture as if it were NDC z, which produced
// garbage positions, huge plane distances, near-zero neighbor weights, and
// therefore an identity blur. Replaced with a relative-depth bilateral weight
// |d_n - d_c| / d_c, which is well-defined for the R32F linear-depth GBuffer
// and perspective-correct for far surfaces.
float3 SpatialBlur(int2 centerPixel, float3 centerNormal, float centerDepth, float3 temporalRadiance)
{
    float3 sum = float3(0.0);
    float weightSum = 0.0;

    int radius = max(1, (int)gConstants.BlurRadius);
    float rcpRadius = 1.0 / max(radius, 1);

    for (int y = -radius; y <= radius; ++y)
    {
        for (int x = -radius; x <= radius; ++x)
        {
            int2 samplePixel = centerPixel + int2(x, y);
            samplePixel = clamp(samplePixel, int2(0, 0), int2(gConstants.OutputSize) - int2(1));

            float4 sampleRadiance = gCurrentRadiance.Load(int3(samplePixel, 0));
            float sampleDepth = gDepth.Load(int3(samplePixel, 0)).r;
            float4 sampleNormalRoughness = gNormalRoughness.Load(int3(samplePixel, 0));
            float3 sampleNormal = sampleNormalRoughness.xyz * 2.0 - 1.0;

            // Skip empty / non-finite samples (sky or uninitialized texels).
            if (sampleDepth == 0.0 || !isfinite(sampleDepth) || !all(isfinite(sampleNormal)))
                continue;

            // Spatial weight (Gaussian)
            float distSq = float(x * x + y * y);
            float spatialW = exp(-distSq * rcpRadius * rcpRadius * 0.5);

            // Normal rejection — saturate keeps the exponent base in [0,1] so
            // pow() is always finite (pow(0, 0) is NaN on some compilers).
            float normalW = pow(saturate(dot(centerNormal, sampleNormal)), gConstants.NormalWeight);

            // Linear-depth rejection: normalize by the center depth so far
            // surfaces (large absolute deltas, small relative deltas) are not
            // over-rejected — the standard perspective bilateral formulation.
            float depthDelta = abs(sampleDepth - centerDepth) / max(centerDepth, 1e-3);
            float planeW = isfinite(depthDelta) ? exp(-depthDelta * gConstants.PlaneWeight) : 0.0;

            float w = spatialW * normalW * planeW;

            sum += sampleRadiance.rgb * w;
            weightSum += w;
        }
    }

    // Finite-guarded weighted average; fall back to the temporal result if
    // no valid neighbors contributed (sky pixels, all samples rejected).
    if (weightSum > 1e-3 && isfinite(weightSum) && all(isfinite(sum)))
        return sum / weightSum;
    return temporalRadiance;
}

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    float2 texelSize = float2(gConstants.RcpOutputSize[0], gConstants.RcpOutputSize[1]);
    float2 pixelUv = (dispatchThreadID.xy + 0.5) * texelSize;

    float depth = gDepth.Load(int3(dispatchThreadID.xy, 0)).r;
    if (depth == 0.0)
    {
        // Sky pixels have no GBuffer geometry; RayGen now fills gi_raw with
        // SampleSky(primary ray). Pass that through so the swapchain shows a
        // non-black sky instead of zeroing it here (fixed 2026-08-09).
        float4 sky = gCurrentRadiance.Load(int3(dispatchThreadID.xy, 0));
        gOutput[dispatchThreadID.xy] = float4(sky.rgb, 1.0);
        return;
    }

    float4 radianceHitDist = gCurrentRadiance.Load(int3(dispatchThreadID.xy, 0));
    float3 radiance = radianceHitDist.rgb;
    float hitDist = radianceHitDist.a;

    if (all(radiance == float3(0.0, 0.0, 0.0)))
    {
        gOutput[dispatchThreadID.xy] = float4(0.0, 0.0, 0.0, 0.0);
        return;
    }

    float4 normalRoughness = gNormalRoughness.Load(int3(dispatchThreadID.xy, 0));
    float3 normal = normalRoughness.xyz * 2.0 - 1.0;
    float roughness = normalRoughness.w;

    float viewZ = depth; // linear depth == positive view-space distance for LH camera
    float normHitDist = GetNormHitDist(hitDist, abs(viewZ), roughness);

    DiffSH current = { radiance, normHitDist };

    // Clamp NaN inputs
    if (isnan(current.hitDist) || isinf(current.hitDist))
        current.hitDist = 0.0;
    if (any(isnan(current.radiance)) || any(isinf(current.radiance)))
        current.radiance = float3(0.0, 0.0, 0.0);

    // Temporal accumulation
    float4 historyPacked = gHistory.Load(int3(dispatchThreadID.xy, 0));
    DiffSH history = { historyPacked.rgb, historyPacked.a };
    if (isnan(history.hitDist) || isinf(history.hitDist))
        history.hitDist = 0.0;
    if (any(isnan(history.radiance)) || any(isinf(history.radiance)))
        history.radiance = float3(0.0, 0.0, 0.0);

    bool historyValid = history.hitDist > 0.0 && IsHistoryValid(pixelUv);
    DiffSH temporal = TemporalAccumulation(current, history, historyValid, gConstants.FrameIndex, gConstants.HistoryFadeIn);

    float antiLagScale = ComputeAntiLagScale(current, history);
    if (antiLagScale < 0.1)
        temporal = current;

    // Spatial blur (only when PassIndex < 0.5, i.e. combined pass)
    if (gConstants.BlurRadius > 0.5 && gConstants.PassIndex < 0.5)
    {
        float3 blurred = SpatialBlur(
            dispatchThreadID.xy, normal, depth, temporal.radiance);
        temporal.radiance = lerp(temporal.radiance, blurred, gConstants.SpatialAlpha);
    }

    gOutput[dispatchThreadID.xy] = float4(temporal.radiance, temporal.hitDist);
    return;
}
