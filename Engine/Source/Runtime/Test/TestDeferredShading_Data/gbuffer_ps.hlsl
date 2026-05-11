// GBuffer Pixel Shader - HLVM Native
// Writes material properties to GBuffer render targets

struct PS_INPUT
{
    float4 Position : SV_Position;
    float2 UV : TEXCOORD0;
    float3 WorldNormal : TEXCOORD1;
    float3 WorldPos : TEXCOORD2;
};

// Material constants - binding b0 with bRegShift 256 → SPIR-V binding 256
cbuffer MaterialConstants : register(b0)
{
    float4 AlbedoColor;     // Base color
    float SpecularIntensity;
    float Roughness;
    float2 _pad;
};

// GBuffer outputs - 3 render targets
struct GBufferOutput
{
    float4 GBuffer0 : SV_Target0;  // Albedo RGB + Specular
    float4 GBuffer1 : SV_Target1;  // Normal XYZ + Roughness
    float4 GBuffer2 : SV_Target2;  // Emissive RGB + Depth
};

GBufferOutput main(PS_INPUT input)
{
    GBufferOutput output;
    
    // Normalize normal
    float3 normal = normalize(input.WorldNormal);
    
    // RT0: Albedo + Specular
    output.GBuffer0 = float4(AlbedoColor.rgb, SpecularIntensity);
    
    // RT1: Normal + Roughness - pack normal to [0,1] range for storage
    output.GBuffer1 = float4(normal * 0.5 + 0.5, Roughness);
    
    // RT2: Emissive + Depth - pack depth in alpha channel
    float depth = input.Position.z / input.Position.w;  // Perspective divide
    output.GBuffer2 = float4(0.0, 0.0, 0.0, depth);  // No emissive for now
    
    return output;
}