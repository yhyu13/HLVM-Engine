// GBuffer Pixel Shader - TestFullDeferredShading2
// Writes material properties to GBuffer render targets

struct PS_INPUT
{
    float4 Position : SV_Position;
    float2 UV : TEXCOORD0;
    float3 WorldNormal : TEXCOORD1;
    float3 WorldPos : TEXCOORD2;
};

// GBuffer outputs - 4 render targets
// MRT0: Position, MRT1: Albedo+Specular, MRT2: Normal+Roughness, MRT3: Emissive+Depth(alpha)
struct GBufferOutput
{
    float4 MRT0 : SV_Target0;  // Position (world space)
    float4 MRT1 : SV_Target1;  // Albedo RGB + Specular (A)
    float4 MRT2 : SV_Target2;  // Normal XYZ + Roughness (A)
    float4 MRT3 : SV_Target3;  // Emissive RGB + Depth (A)
};

cbuffer MaterialConstants : register(b0)
{
    float4 AlbedoColor;
    float SpecularIntensity;
    float Roughness;
    float Pad1;
    float Pad2;
};

GBufferOutput main(PS_INPUT input)
{
    GBufferOutput output;

    // MRT0: World Position
    output.MRT0 = float4(input.WorldPos, 1.0);

    // MRT1: Albedo RGB + Specular (A)
    output.MRT1 = float4(AlbedoColor.rgb, SpecularIntensity);

    // MRT2: Normal XYZ + Roughness (A) - pack to [0,1]
    float3 normal = normalize(input.WorldNormal) * 0.5 + 0.5;
    output.MRT2 = float4(normal, Roughness);

    // MRT3: Emissive RGB + Depth (A) - pack depth in alpha
    float ndcDepth = input.Position.z / input.Position.w;
    output.MRT3 = float4(0.0f, 0.0f, 0.0f, ndcDepth);  // Black emissive, depth in alpha

    return output;
}