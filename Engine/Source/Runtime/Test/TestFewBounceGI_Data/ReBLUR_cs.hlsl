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

float3 ReconstructViewPos(float2 uv, float depth)
{
    float4 clipPos = float4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    float4 viewPos = mul(gConstants.InverseCurrViewProj, clipPos);
    return viewPos.xyz / viewPos.w;
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

// 3x3 bilateral spatial blur with normal/depth rejection
float3 SpatialBlur(int2 centerPixel, float3 centerPos, float3 centerNormal, float centerDepth)
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

            // Skip empty samples
            if (sampleDepth == 0.0)
                continue;

            float3 samplePos = ReconstructViewPos(
                (samplePixel + 0.5) * gConstants.RcpOutputSize, sampleDepth);

            // Spatial weight (Gaussian)
            float distSq = float(x * x + y * y);
            float spatialW = exp(-distSq * rcpRadius * rcpRadius * 0.5);

            // Normal rejection
            float normalW = pow(max(dot(centerNormal, sampleNormal), 0.0), gConstants.NormalWeight);

            // Plane distance rejection
            float planeDist = abs(dot(centerPos - samplePos, centerNormal));
            float planeW = exp(-planeDist * gConstants.PlaneWeight);

            float w = spatialW * normalW * planeW;

            sum += sampleRadiance.rgb * w;
            weightSum += w;
        }
    }

    return weightSum > 0.001 ? sum / weightSum : float3(0.0);
}

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    float2 texelSize = float2(gConstants.RcpOutputSize[0], gConstants.RcpOutputSize[1]);
    float2 pixelUv = (dispatchThreadID.xy + 0.5) * texelSize;

    float depth = gDepth.Load(int3(dispatchThreadID.xy, 0)).r;
    if (depth == 0.0)
    {
        gOutput[dispatchThreadID.xy] = float4(0.0, 0.0, 0.0, 0.0);
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

    float3 viewPos = ReconstructViewPos(pixelUv, depth);
    float viewZ = viewPos.z;
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
        temporal.radiance = SpatialBlur(
            dispatchThreadID.xy, viewPos, normal, depth);
    }

    gOutput[dispatchThreadID.xy] = float4(temporal.radiance, temporal.hitDist);
}
