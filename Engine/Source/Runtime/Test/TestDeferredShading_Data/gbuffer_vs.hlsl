// GBuffer Vertex Shader - HLVM Native
// Transforms vertex positions and passes attributes to pixel shader

struct VS_INPUT
{
    float3 Position : POSITION;
    float2 UV : TEXCOORD0;
    float3 Normal : NORMAL;
};

struct VS_OUTPUT
{
    float4 Position : SV_Position;
    float2 UV : TEXCOORD0;
    float3 WorldNormal : TEXCOORD1;
    float3 WorldPos : TEXCOORD2;
};

// View constants - binding b2 with bRegShift 256 → SPIR-V binding 258
cbuffer ViewConstants : register(b2)
{
    float4x4 ViewProj;  // Combined view * projection matrix
    float3 CameraPos;
    float _pad;
};

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;
    
    // Transform to clip space - mul(Matrix, vector) for SPIR-V column-vector semantics
    float4 worldPos = float4(input.Position, 1.0);
    output.WorldPos = input.Position;
    output.Position = mul(ViewProj, worldPos);
    
    // Pass through UV
    output.UV = input.UV;
    
    // Transform normal to world space (assuming no rotation in model matrix for now)
    output.WorldNormal = input.Normal;
    
    return output;
}