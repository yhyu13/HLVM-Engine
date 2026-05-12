/*
 * PBR Lighting Compute Shader - TestPBRLighting
 * Procedural sphere with Cook-Torrance BRDF
 */

cbuffer LightingConstants : register(b0)
{
    float3 LightDir;
    float LightIntensity;
    float3 CameraPos;
    float Pad0;
    float3 AmbientColor;
    float Pad1;
};

RWTexture2D<float4> u_HDROutput : register(u0);

// Sphere parameters
static const float3 SphereCenter = float3(0.0, 0.0, 5.0);
static const float  SphereRadius = 1.5;

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
// Procedural Sphere
// ---------------------------------------------------------------------------

struct SurfaceData
{
    float3 WorldPos;
    float3 Normal;
    float3 Albedo;
    float  Metallic;
    float  Roughness;
    float  Band;
    bool   Hit;
};

SurfaceData RaySphere(float2 uv, float3 cameraPos)
{
    SurfaceData result;
    result.Hit = false;
    
    float3 rayDir = normalize(float3(uv.x, uv.y, 1.0));
    
    float3 oc = cameraPos - SphereCenter;
    float a = dot(rayDir, rayDir);
    float b = 2.0 * dot(oc, rayDir);
    float c = dot(oc, oc) - SphereRadius * SphereRadius;
    float discriminant = b * b - 4.0 * a * c;
    
    if (discriminant < 0.0)
    {
        result.Albedo = float3(0.05, 0.05, 0.08);
        result.Metallic = 0.0;
        result.Roughness = 1.0;
        result.Normal = float3(0, 1, 0);
        result.WorldPos = float3(0, 0, 0);
        return result;
    }
    
    float t = (-b - sqrt(discriminant)) / (2.0 * a);
    result.WorldPos = cameraPos + t * rayDir;
    result.Normal = normalize(result.WorldPos - SphereCenter);
    result.Hit = true;
    
    // Material bands by world-space Y (normal Y)
    result.Band = (result.Normal.y + 1.0) * 0.5;
    if (result.Band > 0.6)
    {
        result.Albedo = float3(1.0, 0.78, 0.34); // Gold
        result.Metallic = 1.0;
        result.Roughness = 0.1;
    }
    else if (result.Band > 0.3)
    {
        result.Albedo = float3(0.8, 0.2, 0.2);   // Red plastic
        result.Metallic = 0.0;
        result.Roughness = 0.3;
    }
    else
    {
        result.Albedo = float3(0.2, 0.3, 0.8);   // Blue rough
        result.Metallic = 0.0;
        result.Roughness = 0.8;
    }
    
    return result;
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

[numthreads(8, 8, 1)]
void main(uint2 dispatchThreadId : SV_DispatchThreadID)
{
    uint2 pixelCoord = dispatchThreadId;
    
    // Map pixel to [-1, 1] with Y pointing up in world space
    float2 uv = (float2(pixelCoord) / float2(800.0, 600.0)) * 2.0 - 1.0;
    // In image space, Y=0 is top. In world space, we want Y=+1 to be top.
    // uv.y = -1 at image top, +1 at image bottom.
    // We want ray Y to be +1 at image top (hits sphere top).
    // So flip: image top (uv.y=-1) → rayDir.y = +1
    uv.y = -uv.y;
    
    SurfaceData surface = RaySphere(uv, CameraPos);
    
    float3 N = surface.Normal;
    float3 V = normalize(CameraPos - surface.WorldPos);
    float3 L = normalize(-LightDir);
    float3 H = normalize(V + L);
    
    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.0);
    
    float3 albedo = surface.Albedo;
    float metallic = surface.Metallic;
    float roughness = surface.Roughness;
    
    float3 F0 = lerp(float3(0.04, 0.04, 0.04), albedo, metallic);
    
    float NDF = DistributionGGX(N, H, roughness);
    float G = GeometrySmith(N, V, L, roughness);
    float3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);
    
    float3 numerator = NDF * G * F;
    float denominator = 4.0 * NdotV * NdotL + 1e-5;
    float3 specular = numerator / denominator;
    
    float3 kS = F;
    float3 kD = float3(1.0, 1.0, 1.0) - kS;
    kD *= (1.0 - metallic);
    float3 diffuse = (kD * albedo) / 3.14159265;
    
    float3 Lo = (diffuse + specular) * NdotL * LightIntensity;
    float3 ambient = albedo * AmbientColor;
    float3 finalColor = ambient + Lo;
    
    u_HDROutput[pixelCoord] = float4(finalColor, 1.0);
}
