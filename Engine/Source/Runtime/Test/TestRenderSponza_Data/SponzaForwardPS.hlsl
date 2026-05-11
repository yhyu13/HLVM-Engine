/*
 * Sponza Forward Pixel Shader
 *
 * Samples diffuse texture and applies simple Lambertian + ambient lighting.
 */

SamplerState LinearSampler : register(s0);
Texture2D DiffuseTexture : register(t0);

cbuffer LightConstants : register(b1) {
    float3 LightPosition;
    float Pad0;
    float3 CameraPosition;
    float Pad1;
};

struct PSInput {
    float4 Position : SV_POSITION;
    float3 WorldPos : TEXCOORD0;
    float3 Normal : TEXCOORD1;
    float2 TexCoord : TEXCOORD2;
};

float4 main(PSInput input) : SV_TARGET {
    float4 diffuseColor = DiffuseTexture.Sample(LinearSampler, input.TexCoord);

    float3 N = normalize(input.Normal);
    float3 L = normalize(LightPosition - input.WorldPos);
    float NdotL = max(dot(N, L), 0.0);

    float3 ambient = float3(0.15, 0.15, 0.15);
    float3 diffuse = diffuseColor.rgb * NdotL;

    float3 finalColor = diffuseColor.rgb * ambient + diffuse;

    return float4(finalColor, 1.0);
}
