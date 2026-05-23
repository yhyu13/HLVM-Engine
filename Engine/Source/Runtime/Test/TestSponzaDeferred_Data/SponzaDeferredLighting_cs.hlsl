/*
 * Sponza Deferred Lighting Compute Shader - TestSponzaDeferred
 * Cook-Torrance PBR (GGX + Schlick + Smith) with world position reconstruction from depth
 * Shadow mapping: single directional light shadow map with hard shadows
 */

cbuffer LightingConstants : register(b0)
{
    float4x4 InvViewProj;      // 4 registers
    float3   LightDir;         // 1 register
    float    LightIntensity;
    float3   CameraPos;        // 1 register
    float    ShadowHardness;
    float3   AmbientColor;     // 1 register
    float    MinAO;
    float2   ScreenSize;       // 1 register
    float2   Pad1;
    float4x4 LightViewProj;    // 4 registers — NEW
    float    ShadowMapSize;    // NEW
    float    ShadowBias;       // NEW
    float2   Pad2;             // NEW
};

Texture2D<float4> t_Diffuse  : register(t0);
Texture2D<float4> t_Material : register(t1); // Metallic(R) + Roughness(G)
Texture2D<float4> t_Normal   : register(t2);
Texture2D<float4> t_Emissive : register(t3);
Texture2D<float>  t_Depth    : register(t4);
Texture2D<float>  t_ShadowMap      : register(t5); // NEW
#ifdef ENABLE_SSAO
Texture2D<float>  t_SSAO           : register(t6); // NEW
#endif
#ifdef ENABLE_CONTACT_SHADOWS
Texture2D<float>  t_ContactShadows : register(t7); // NEW
#endif

SamplerState ShadowSampler : register(s1); // NEW

RWTexture2D<float4> u_HDROutput : register(u0);

// ---------------------------------------------------------------------------
// BRDF Functions
// ---------------------------------------------------------------------------

float3 FresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + (1.0 - F0) * pow(max(1.0 - cosTheta, 0.0), 5.0);
}

float DistributionGGX(float3 N, float3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = 3.14159265 * denom * denom;
    return a2 / max(denom, 1e-5);
}

float GeometrySmith(float3 N, float3 V, float3 L, float roughness)
{
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx1 = NdotV / max(NdotV * (1.0 - k) + k, 1e-5);
    float ggx2 = NdotL / max(NdotL * (1.0 - k) + k, 1e-5);
    return ggx1 * ggx2;
}

// ---------------------------------------------------------------------------
// World Position Reconstruction from Depth
// ---------------------------------------------------------------------------

float3 ReconstructWorldPosition(uint2 pixelCoord, float depth)
{
    float2 uv = (float2(pixelCoord) + 0.5) / ScreenSize;
    float2 ndc = uv * 2.0 - 1.0;
    // Vulkan NDC: Y down, Z [0,1]. Flip Y for reconstruction.
    float4 worldPosH = mul(InvViewProj, float4(ndc.x, -ndc.y, depth, 1.0));
    return worldPosH.xyz / worldPosH.w;
}

// ---------------------------------------------------------------------------
// Shadow Sampling
// ---------------------------------------------------------------------------

// Shared helper: compute shadow UV and light depth from world position
void ComputeShadowCoords(float3 worldPos, out float2 shadowUV, out float lightDepth)
{
    float4 lightPos = mul(LightViewProj, float4(worldPos, 1.0));
    lightPos.xyz /= lightPos.w;
    shadowUV = lightPos.xy * 0.5 + 0.5;
    shadowUV.y = 1.0 - shadowUV.y;  // Flip Y for NVRHI DX-style viewport
    lightDepth = lightPos.z;
}

// Hard shadows (single sample, binary result)
float SampleShadowHard(float3 worldPos)
{
    float2 shadowUV;
    float lightDepth;
    ComputeShadowCoords(worldPos, shadowUV, lightDepth);
    
    // Check if outside shadow map bounds
    if (shadowUV.x < 0.0 || shadowUV.x > 1.0 || shadowUV.y < 0.0 || shadowUV.y > 1.0)
        return 1.0; // Outside shadow map = fully lit
    
    // Sample shadow map depth and compare with bias
    float shadowDepth = t_ShadowMap.SampleLevel(ShadowSampler, shadowUV, 0);
    return (lightDepth < shadowDepth + ShadowBias) ? 1.0 : 0.0;
}

// PCF 2x2: manual 4-tap kernel filtering for soft shadow edges
float SampleShadowPCF2x2(float3 worldPos)
{
    float2 shadowUV;
    float lightDepth;
    ComputeShadowCoords(worldPos, shadowUV, lightDepth);
    
    // Check if outside shadow map bounds
    if (shadowUV.x < 0.0 || shadowUV.x > 1.0 || shadowUV.y < 0.0 || shadowUV.y > 1.0)
        return 1.0; // Outside shadow map = fully lit
    
    float2 texelSize = 1.0 / ShadowMapSize;
    float result = 0.0;
    
    // 2x2 kernel: sample at texel corners and average
    [unroll]
    for (int y = 0; y < 2; y++)
    {
        [unroll]
        for (int x = 0; x < 2; x++)
        {
            float2 sampleUV = shadowUV + (float2(x, y) - 0.5) * texelSize;
            float shadowDepth = t_ShadowMap.SampleLevel(ShadowSampler, sampleUV, 0);
            result += (lightDepth < shadowDepth + ShadowBias) ? 1.0 : 0.0;
        }
    }
    
    return result * 0.25;
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

[numthreads(8, 8, 1)]
void main(uint2 dispatchThreadId : SV_DispatchThreadID)
{
    uint2 pixelCoord = dispatchThreadId;

    // Early out if outside screen bounds
    if (pixelCoord.x >= uint(ScreenSize.x) || pixelCoord.y >= uint(ScreenSize.y))
        return;

    float4 diffuseData  = t_Diffuse[pixelCoord];
    float4 materialData = t_Material[pixelCoord];
    float4 normalData   = t_Normal[pixelCoord];
    float4 emissiveData = t_Emissive[pixelCoord];
    float  depth        = t_Depth[pixelCoord];

    // Decode normal from [0,1] to [-1,1]
    float3 worldNormal = normalize(normalData.xyz * 2.0 - 1.0);

    // Reconstruct world position
    float3 worldPos = ReconstructWorldPosition(pixelCoord, depth);

    // Material properties
    float3 albedo = diffuseData.rgb;
    float metallic = materialData.r;
    float roughness = materialData.g;
    float materialAO = materialData.b;

    // View and light vectors
    float3 V = normalize(CameraPos - worldPos);
    float3 L = normalize(-LightDir);
    float3 H = normalize(V + L);

    float NdotL = max(dot(worldNormal, L), 0.0);
    float NdotV = max(dot(worldNormal, V), 0.0);

    // Fresnel base reflectance
    float3 F0 = lerp(float3(0.04, 0.04, 0.04), albedo, metallic);

    // Cook-Torrance specular
    float NDF = DistributionGGX(worldNormal, H, roughness);
    float G = GeometrySmith(worldNormal, V, L, roughness);
    float3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);

    float3 numerator = NDF * G * F;
    float denominator = 4.0 * NdotV * NdotL + 1e-5;
    float3 specular = numerator / denominator;

    // Energy conservation
    float3 kS = F;
    float3 kD = float3(1.0, 1.0, 1.0) - kS;
    kD *= (1.0 - metallic);

    // Diffuse Lambertian
    float3 diffuse = (kD * albedo) / 3.14159265;

    // Shadow factor: use PCF 2x2 for soft edges
    float shadowFactor = SampleShadowPCF2x2(worldPos);

    // Contact shadows: screen-space high-frequency shadow detail
#ifdef ENABLE_CONTACT_SHADOWS
    shadowFactor *= t_ContactShadows[pixelCoord];
#endif

    // Combined direct lighting (attenuated by shadow)
    float3 Lo = (diffuse + specular) * NdotL * LightIntensity * shadowFactor;

    // Ambient: modulated by SSAO with minimum clamp
    float ao = materialAO;
#ifdef ENABLE_SSAO
    ao *= t_SSAO[pixelCoord];
#endif
    float3 ambient = albedo * AmbientColor * max(ao, MinAO);

    float3 finalColor = ambient + Lo + emissiveData.rgb;

    // DEBUG: Uncomment to visualize shadow factor directly as grayscale.
    // This verifies PCF produces smooth values between 0 and 1.
    // u_HDROutput[pixelCoord] = float4(shadowFactor, shadowFactor, shadowFactor, 1.0);
    // return;

    u_HDROutput[pixelCoord] = float4(finalColor, 1.0);
}
