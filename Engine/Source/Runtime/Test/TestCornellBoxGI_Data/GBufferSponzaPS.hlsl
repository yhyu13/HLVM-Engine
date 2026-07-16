/*
 * GBuffer Sponza Pixel Shader - RT Shadows Test
 *
 * Outputs to 5 MRTs:
 *   MRT0: Diffuse RGBA (RGBA8)
 *   MRT1: Specular F0 RGB + Roughness A (RGBA16F)
 *   MRT2: Normal XYZ encoded [0,1] + 1.0 A (RGBA16F)
 *   MRT3: Emissive RGBA (RGBA16F)
 *   MRT4: WorldPos XYZ + 1.0 A (RGBA16F)
 */

// =============================================================================
// Samplers & Textures
// =============================================================================

SamplerState LinearSampler : register(s0);
Texture2D DiffuseTexture : register(t0);

// =============================================================================
// Pixel Input
// =============================================================================

struct PSInput {
    float4 Position : SV_POSITION;
    float3 WorldPos : TEXCOORD0;
    float3 Normal : TEXCOORD1;
    float2 TexCoord : TEXCOORD2;
};

// =============================================================================
// MRT Output
// =============================================================================

struct MRTOutput {
    float4 MRT0 : SV_TARGET0; // Diffuse RGBA
    float4 MRT1 : SV_TARGET1; // Specular F0 RGB + Roughness A
    float4 MRT2 : SV_TARGET2; // Normal XYZ [0,1] + 1.0 A
    float4 MRT3 : SV_TARGET3; // Emissive RGBA
    float4 MRT4 : SV_TARGET4; // WorldPos XYZ + 1.0 A
};

// =============================================================================
// Pixel Shader
// =============================================================================

MRTOutput main(PSInput input) {
    MRTOutput output;

    // Sample diffuse texture
    float4 diffuseColor = DiffuseTexture.Sample(LinearSampler, input.TexCoord);
    if (diffuseColor.a < 0.01f) {
        diffuseColor = float4(1.0, 1.0, 1.0, 1.0);
    }
    output.MRT0 = diffuseColor;

    // Specular: dielectric F0 + roughness
    output.MRT1 = float4(0.04, 0.04, 0.04, 0.5);

    // Normal: encode from [-1,1] to [0,1]
    float3 worldNormal = normalize(input.Normal);
    output.MRT2 = float4(worldNormal * 0.5 + 0.5, 1.0);

    // Emissive: none
    output.MRT3 = float4(0.0, 0.0, 0.0, 1.0);

    // WorldPos: pass through for RT shadow ray origin
    output.MRT4 = float4(input.WorldPos, 1.0);

    return output;
}
