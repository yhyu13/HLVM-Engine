/*
 * GBuffer Sponza Pixel Shader - TestSponzaDeferred
 */

SamplerState LinearSampler : register(s0);
Texture2D DiffuseTexture : register(t0);

struct PSInput {
    float4 Position : SV_POSITION;
    float3 WorldPos : TEXCOORD0;
    float3 Normal : TEXCOORD1;
    float2 TexCoord : TEXCOORD2;
};

struct MRTOutput {
    float4 MRT0 : SV_TARGET0; // Diffuse RGB
    float4 MRT1 : SV_TARGET1; // Specular RGB + Roughness A
    float4 MRT2 : SV_TARGET2; // Normal XYZ [0,1]
    float4 MRT3 : SV_TARGET3; // Emissive RGB
};

MRTOutput main(PSInput input) {
    MRTOutput output;

    // Sample diffuse texture
    float4 diffuseColor = DiffuseTexture.Sample(LinearSampler, input.TexCoord);
    if (diffuseColor.a < 0.01f) {
        diffuseColor = float4(1.0, 1.0, 1.0, 1.0);
    }
    output.MRT0 = diffuseColor;

    // Simple specular
    output.MRT1 = float4(0.04, 0.04, 0.04, 0.5);

    // Encode normal from [-1,1] to [0,1]
    float3 worldNormal = normalize(input.Normal);
    output.MRT2 = float4(worldNormal * 0.5 + 0.5, 1.0);

    // No emissive
    output.MRT3 = float4(0.0, 0.0, 0.0, 1.0);

    return output;
}
