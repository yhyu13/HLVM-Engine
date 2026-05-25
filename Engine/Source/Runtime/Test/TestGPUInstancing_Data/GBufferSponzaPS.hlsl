/*
 * GBuffer Sponza Pixel Shader - TestSponzaDeferred
 * MRT0: Diffuse RGBA
 * MRT1: Metallic(R) + Roughness(G) + AO(B) + 1.0(A)
 * MRT2: Normal XYZ [0,1]
 * MRT3: Emissive RGB
 */

SamplerState LinearSampler : register(s0);
Texture2D DiffuseTexture  : register(t0);
Texture2D NormalTexture   : register(t1);
Texture2D MetallicTexture : register(t2);
Texture2D RoughnessTexture : register(t3);
Texture2D AOTexture       : register(t4);

cbuffer MaterialConstants : register(b1) {
    float4 AlbedoTint;
    float  Metallic;
    float  Roughness;
    float  EmissiveStrength;
    float  Pad;
};

struct PSInput {
    float4 Position : SV_POSITION;
    float3 WorldPos : TEXCOORD0;
    float3 Normal : TEXCOORD1;
    float2 TexCoord : TEXCOORD2;
    float3 Tangent : TEXCOORD3;
};

struct MRTOutput {
    float4 MRT0 : SV_TARGET0; // Diffuse RGBA
    float4 MRT1 : SV_TARGET1; // Metallic(R) + Roughness(G) + AO(B)
    float4 MRT2 : SV_TARGET2; // Normal XYZ [0,1]
    float4 MRT3 : SV_TARGET3; // Emissive RGB
};

MRTOutput main(PSInput input) {
    MRTOutput output;

    // Sample diffuse texture and apply albedo tint
    float4 diffuseColor = DiffuseTexture.Sample(LinearSampler, input.TexCoord);
    if (diffuseColor.a < 0.01f) {
        diffuseColor = float4(1.0, 1.0, 1.0, 1.0);
    }
    output.MRT0 = diffuseColor * AlbedoTint;

    // Sample PBR material textures (placeholders return 1.0, so constants pass through)
    float metallic = MetallicTexture.Sample(LinearSampler, input.TexCoord).r * Metallic;
    float roughness = RoughnessTexture.Sample(LinearSampler, input.TexCoord).r * Roughness;
    float ao = AOTexture.Sample(LinearSampler, input.TexCoord).r;
    output.MRT1 = float4(metallic, roughness, ao, 1.0);

    // Normal mapping: sample tangent-space normal and transform to world space
    float3 worldNormal = normalize(input.Normal);
    float3 worldTangent = normalize(input.Tangent);
    // Reconstruct bitangent (assuming right-handed TBN, no mirror)
    float3 worldBitangent = cross(worldNormal, worldTangent);
    
    float3 tangentNormal = NormalTexture.Sample(LinearSampler, input.TexCoord).rgb;
    // Convert from [0,1] to [-1,1]
    tangentNormal = tangentNormal * 2.0 - 1.0;
    // Transform to world space
    float3 finalNormal = normalize(
        tangentNormal.x * worldTangent +
        tangentNormal.y * worldBitangent +
        tangentNormal.z * worldNormal);
    
    // Encode normal from [-1,1] to [0,1]
    output.MRT2 = float4(finalNormal * 0.5 + 0.5, 1.0);

    // Emissive from material strength (no texture yet)
    output.MRT3 = float4(EmissiveStrength, EmissiveStrength, EmissiveStrength, 1.0);

    return output;
}
