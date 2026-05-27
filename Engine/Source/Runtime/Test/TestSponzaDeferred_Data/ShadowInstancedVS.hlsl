/*
 * Shadow Instanced Vertex Shader - TestSponzaDeferred
 * Reads per-instance model matrices from structured buffer
 */

cbuffer ShadowConstants : register(b0) {
    float4x4 ModelMatrix;
    float4x4 LightViewProj;
};

StructuredBuffer<float4> g_InstanceMatrices : register(t10);

struct VSInput {
    float3 Position : POSITION;
    float3 Normal : NORMAL;
    float2 TexCoord : TEXCOORD0;
    float3 Tangent : TANGENT;
};

float4 main(VSInput input, uint instanceID : SV_InstanceID) : SV_POSITION {
    float4 row0 = g_InstanceMatrices[instanceID * 4 + 0];
    float4 row1 = g_InstanceMatrices[instanceID * 4 + 1];
    float4 row2 = g_InstanceMatrices[instanceID * 4 + 2];
    float4 row3 = g_InstanceMatrices[instanceID * 4 + 3];
    float4x4 modelMatrix = float4x4(row0, row1, row2, row3);

    float4 worldPos = mul(modelMatrix, float4(input.Position, 1.0));
    return mul(LightViewProj, worldPos);
}