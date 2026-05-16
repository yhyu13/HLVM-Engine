// Blit Vertex Shader - Simple fullscreen quad
// Matches Donut fullscreen_vs.hlsl pattern
struct VS_OUTPUT
{
    float4 Position : SV_Position;
    float2 UV : TEXCOORD0;
};

VS_OUTPUT main(uint VertexIndex : SV_VertexID)
{
    VS_OUTPUT Output;

    // Fullscreen quad vertices (matches SPIR-V order)
    // VertexIndex: 0,1,2,3
    float2 vertices[4] = {
        float2(-1.0, -1.0),  // 0: bottom-left
        float2( 1.0, -1.0),  // 1: bottom-right
        float2(-1.0,  1.0),  // 2: top-left
        float2( 1.0,  1.0)   // 3: top-right
    };

    // Simple UV coordinates matching Donut pattern
    float2 uvs[4] = {
        float2(0.0, 0.0),  // 0: bottom-left
        float2(1.0, 0.0),  // 1: bottom-right
        float2(0.0, 1.0),  // 2: top-left
        float2(1.0, 1.0)   // 3: top-right
    };

    Output.Position = float4(vertices[VertexIndex], 0.0, 1.0);
    Output.UV = uvs[VertexIndex];

    return Output;
}
