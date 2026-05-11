// Simple Vertex Shader for Cube with MVP transformation
// Transforms vertex positions and passes through color from vertex buffer

struct VS_OUTPUT
{
    float4 Position : SV_Position;
    float3 Color : COLOR0;
};

// Uniform buffer matching FUniformBufferObject layout (std140, 16-byte aligned)
cbuffer UniformBufferObject : register(b0)
{
    float4x4 Model;
    float4x4 View;
    float4x4 Proj;
};

// Note: mul(M, v) is column-vector multiplication for SPIR-V
VS_OUTPUT main(float3 inPosition : inPosition, float3 inColor : inColor)
{
    VS_OUTPUT Output;
    
    // Transform to clip space using MVP (Model * View * Projection)
    float4 worldPos = mul(Model, float4(inPosition, 1.0));  // Apply model rotation
    float4 viewPos = mul(View, worldPos);
    float4 clipPos = mul(Proj, viewPos);
    Output.Position = clipPos;
    
    // Pass through color
    Output.Color = inColor;
    return Output;
}