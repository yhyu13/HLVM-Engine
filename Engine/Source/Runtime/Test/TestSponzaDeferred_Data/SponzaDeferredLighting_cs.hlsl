/*
 * Sponza Deferred Lighting Compute Shader - TestSponzaDeferred
 * Simple Lambertian diffuse + ambient lighting
 */

cbuffer LightingConstants : register(b0)
{
    float3 LightDir;
    float LightIntensity;
    float3 CameraPos;
    float ShadowHardness;
    float3 AmbientColor;
    float Pad;
};

Texture2D<float4> t_Diffuse : register(t0);
Texture2D<float4> t_Specular : register(t1);
Texture2D<float4> t_Normal : register(t2);
Texture2D<float4> t_Emissive : register(t3);

RWTexture2D<float4> u_HDROutput : register(u0);

[numthreads(8, 8, 1)]
void main(uint2 dispatchThreadId : SV_DispatchThreadID)
{
    uint2 pixelCoord = dispatchThreadId;

    float4 diffuseData = t_Diffuse[pixelCoord];
    float4 normalData = t_Normal[pixelCoord];

    // Decode normal from [0,1] to [-1,1]
    float3 worldNormal = normalData.xyz * 2.0f - 1.0f;

    // Simple Lambertian diffuse
    float3 L = normalize(-LightDir);
    float NdotL = max(dot(worldNormal, L), 0.0f);

    float3 diffuseTerm = diffuseData.rgb * NdotL * LightIntensity;
    float3 ambientTerm = diffuseData.rgb * AmbientColor;

    float3 finalColor = diffuseTerm + ambientTerm;

    u_HDROutput[pixelCoord] = float4(finalColor, 1.0f);
}
