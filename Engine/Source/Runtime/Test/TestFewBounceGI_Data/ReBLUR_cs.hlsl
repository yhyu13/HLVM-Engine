// ReBLUR-style Diffuse GI Denoiser - Single pass with temporal accumulation

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

DiffSH TemporalAccumulation(DiffSH current, DiffSH history, bool historyValid, float confidence)
{
    if (!historyValid)
        return current;
    float blendFactor = saturate(confidence);
    DiffSH result;
    result.radiance = lerp(history.radiance, current.radiance, blendFactor);
    result.hitDist = lerp(history.hitDist, current.hitDist, blendFactor);
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

    float viewZ = ReconstructViewPos(pixelUv, depth).z;
    float normHitDist = GetNormHitDist(hitDist, abs(viewZ), roughness);

    DiffSH current = { radiance, normHitDist };

    float4 historyPacked = gHistory.Load(int3(dispatchThreadID.xy, 0));
    DiffSH history = { historyPacked.rgb, historyPacked.a };

    // Clamp NaN inputs to avoid invalidation on next frame
    if (isnan(current.hitDist) || isinf(current.hitDist))
        current.hitDist = 0.0;
    if (any(isnan(current.radiance)) || any(isinf(current.radiance)))
        current.radiance = float3(0.0, 0.0, 0.0);
    if (isnan(history.hitDist) || isinf(history.hitDist))
        history.hitDist = 0.0;
    if (any(isnan(history.radiance)) || any(isinf(history.radiance)))
        history.radiance = float3(0.0, 0.0, 0.0);

    float confidence = saturate((gConstants.FrameIndex - 1.0) / max(gConstants.HistoryFadeIn, 1.0));
    bool historyValid = history.hitDist > 0.0 && IsHistoryValid(pixelUv);

    DiffSH temporal = TemporalAccumulation(current, history, historyValid, confidence);

    float antiLagScale = ComputeAntiLagScale(current, history);
    if (antiLagScale < 0.1)
        temporal = current;

    gOutput[dispatchThreadID.xy] = float4(temporal.radiance, temporal.hitDist);
}
