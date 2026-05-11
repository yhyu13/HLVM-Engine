/*
 * GBuffer Sponza Vertex Shader - TestSponzaDeferred
 */

cbuffer ViewConstants : register(b0) {
    float4x4 ModelMatrix;
    float4x4 ViewMatrix;
    float4x4 ProjMatrix;
    float4 CameraPos;
};

struct VSInput {
    float3 Position : POSITION;
    float3 Normal : NORMAL;
    float2 TexCoord : TEXCOORD0;
    float3 Tangent : TANGENT;
};

struct PSInput {
    float4 Position : SV_POSITION;
    float3 WorldPos : TEXCOORD0;
    float3 Normal : TEXCOORD1;
    float2 TexCoord : TEXCOORD2;
};

PSInput main(VSInput input) {
    PSInput output;

    float4 worldPos = mul(ModelMatrix, float4(input.Position, 1.0));
    output.WorldPos = worldPos.xyz;

    float4 viewPos = mul(ViewMatrix, worldPos);
    output.Position = mul(ProjMatrix, viewPos);

    output.Normal = normalize(mul((float3x3)ModelMatrix, input.Normal));
    output.TexCoord = input.TexCoord;

    return output;
}
