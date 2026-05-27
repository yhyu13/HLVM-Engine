/*
 * GBuffer Sponza Vertex Shader - TestSponzaDeferred
 * Supports both world-space (baked) and local-space geometry via per-mesh transform buffer
 *
 * For world-space geometry (baked transforms): bind identity matrix to t10
 * For local-space geometry: bind per-mesh transform matrix to t10
 */

cbuffer ViewConstants : register(b0) {
    float4x4 ModelMatrix;  // Fallback: identity for world-space, unused for local-space
    float4x4 ViewMatrix;
    float4x4 ProjMatrix;
    float4 CameraPos;
};

// Per-mesh transform buffer (bound to t10)
// For world-space geometry: bind 4 float4 values representing identity matrix
// For local-space geometry: bind the per-mesh world transform
StructuredBuffer<float4> g_MeshMatrices : register(t10);

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
    float3 Tangent : TEXCOORD3;
};

PSInput main(VSInput input) {
    PSInput output;

    // Read per-mesh transform from structured buffer
    // Layout: row0 at [0], row1 at [1], row2 at [2], row3 at [3]
    float4 row0 = g_MeshMatrices[0];
    float4 row1 = g_MeshMatrices[1];
    float4 row2 = g_MeshMatrices[2];
    float4 row3 = g_MeshMatrices[3];
    float4x4 meshMatrix = float4x4(row0, row1, row2, row3);

    float4 worldPos = mul(meshMatrix, float4(input.Position, 1.0));
    output.WorldPos = worldPos.xyz;

    float4 viewPos = mul(ViewMatrix, worldPos);
    output.Position = mul(ProjMatrix, viewPos);

    output.Normal = normalize(mul((float3x3)meshMatrix, input.Normal));
    output.Tangent = normalize(mul((float3x3)meshMatrix, input.Tangent));
    output.TexCoord = input.TexCoord;

    return output;
}