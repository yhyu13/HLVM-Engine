// GBuffer Vertex Shader - TestFullDeferredShading2
// Transforms vertex positions and passes attributes to pixel shader

struct VS_INPUT
{
    float3 Position : POSITION;   // Location 0
    float2 UV : TEXCOORD0;        // Location 1
    float3 Normal : NORMAL;       // Location 2
};

struct VS_OUTPUT
{
    float4 Position : SV_Position;
    float2 UV : TEXCOORD0;
    float3 WorldNormal : TEXCOORD1;
    float3 WorldPos : TEXCOORD2;
};

// View constants - b2 with bRegShift 256 → SPIR-V binding 258
cbuffer ViewConstants : register(b2)
{
    float4x4 ViewProj;  // Combined view * projection matrix
    float4x4 Model;      // Model matrix
};

// Get normal matrix (inverse transpose of 3x3 rotation/scale part)
float3x3 GetNormalMatrix(float4x4 model)
{
    // Extract rotation/scale part (upper 3x3) and transpose
    // For uniform scale + rotation: inverse transpose = rotation matrix
    return float3x3(
        model[0].xyz,
        model[1].xyz,
        model[2].xyz
    );
}

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;

    // World position - transform by Model matrix
    float4 worldPos = mul(Model, float4(input.Position, 1.0));
    output.WorldPos = worldPos.xyz;
    output.Position = mul(ViewProj, worldPos);

    // Pass through UV
    output.UV = input.UV;

    // Transform normal to world space using normal matrix
    float3x3 normalMatrix = GetNormalMatrix(Model);
    output.WorldNormal = normalize(mul(normalMatrix, input.Normal));

    return output;
}