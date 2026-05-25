/*
 * GBuffer Instanced Vertex Shader - TestGPUInstancing
 * Reads per-instance model matrices from structured buffer
 */

cbuffer ViewConstants : register(b0) {
    float4x4 ModelMatrix;
    float4x4 ViewMatrix;
    float4x4 ProjMatrix;
    float4 CameraPos;
};

StructuredBuffer<float4> g_InstanceMatrices : register(t10);

struct VSInput {
    float3 Position : POSITION;
    float3 Normal   : NORMAL;
    float2 TexCoord : TEXCOORD0;
    float3 Tangent  : TANGENT;
};

struct PSInput {
    float4 Position : SV_POSITION;
    float3 WorldPos : TEXCOORD0;
    float3 Normal   : TEXCOORD1;
    float2 TexCoord : TEXCOORD2;
    float3 Tangent  : TEXCOORD3;
};

PSInput main(VSInput input, uint instanceID : SV_InstanceID) {
    PSInput output;

    float4 row0 = g_InstanceMatrices[instanceID * 4 + 0];
    float4 row1 = g_InstanceMatrices[instanceID * 4 + 1];
    float4 row2 = g_InstanceMatrices[instanceID * 4 + 2];
    float4 row3 = g_InstanceMatrices[instanceID * 4 + 3];
    float4x4 modelMatrix = float4x4(row0, row1, row2, row3);

    float4 worldPos = mul(modelMatrix, float4(input.Position, 1.0));
    output.WorldPos = worldPos.xyz;

    float4 viewPos = mul(ViewMatrix, worldPos);
    output.Position = mul(ProjMatrix, viewPos);

    output.Normal = normalize(mul((float3x3)modelMatrix, input.Normal));
    output.Tangent = normalize(mul((float3x3)modelMatrix, input.Tangent));
    output.TexCoord = input.TexCoord;

    return output;
}
