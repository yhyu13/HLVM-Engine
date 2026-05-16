// Blit Fragment Shader - Texture sampling using Load (samplerless)
// Uses tex.GetDimensions() to get actual texture size for correct texel addressing

// Blit mode: 0=Normal RGBA, 1=Depth (alpha as grayscale)
cbuffer BlitParams : register(b0)
{
    float Mode;
    float Pad1;
    float Pad2;
    float Pad3;
}

Texture2D BlitTexture : register(t0);

float4 main(float4 Position : SV_Position, float2 UV : TEXCOORD0) : SV_Target0
{
    uint width, height;
    BlitTexture.GetDimensions(width, height);
    float4 color = BlitTexture.Load(int3(uint2(UV * float2(width, height)), 0));

    // Mode 1 = Depth: show alpha as grayscale
    if (Mode > 0.5)
    {
        return float4(color.a, color.a, color.a, 1.0);
    }

    return color;
}
