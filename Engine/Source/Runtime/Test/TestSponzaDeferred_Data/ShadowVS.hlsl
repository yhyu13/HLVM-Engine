/*
 * Shadow Depth Vertex Shader - TestSponzaDeferred
 * Transforms geometry to light clip space for shadow map depth rendering
 */

cbuffer ShadowConstants : register(b0) {
    float4x4 ModelMatrix;
    float4x4 LightViewProj;
};

struct VSInput {
    float3 Position : POSITION;
    float3 Normal : NORMAL;
    float2 TexCoord : TEXCOORD0;
    float3 Tangent : TANGENT;
};

float4 main(VSInput input) : SV_POSITION {
    float4 worldPos = mul(ModelMatrix, float4(input.Position, 1.0));
    return mul(LightViewProj, worldPos);
}
