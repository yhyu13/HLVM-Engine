/*
 * Copyright (c) 2024-2026. MIT License. All rights reserved.
 *
 * Fullscreen quad vertex shader for blit operations
 */

// Vertex shader output / pixel shader input
struct BlitVS_Output
{
    float4 Position : SV_Position;
    float2 UV : TEXCOORD0;
};

// Fullscreen quad vertex shader
BlitVS_Output BlitVS(uint VertexID : SV_VertexID)
{
    // Fullscreen quad positions (NDC)
    const float2 positions[4] = {
        float2(-1.0, -1.0),
        float2( 1.0, -1.0),
        float2(-1.0,  1.0),
        float2( 1.0,  1.0)
    };

    BlitVS_Output Output;
    Output.Position = float4(positions[VertexID], 0.0, 1.0);
    Output.UV = positions[VertexID] * float2(0.5, -0.5) + 0.5; // Flip Y for texture sampling
    return Output;
}

// Texture sampling pixel shader for blit operations
// Binding layout matches:
// - Binding 0: Texture2D (BlitTexture)
// - Binding 1: SamplerState (BlitSampler)
Texture2D BlitTexture : register(t0);
SamplerState BlitSampler : register(s0);

float4 BlitPS(BlitVS_Output Input) : SV_Target0
{
    // Simple texture sampling
    return BlitTexture.Sample(BlitSampler, Input.UV);
}