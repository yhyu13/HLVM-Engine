// Blit Fragment Shader - Texture sampling using Load (samplerless)
// Uses tex.GetDimensions() to get actual texture size for correct texel addressing
Texture2D BlitTexture : register(t0);

float4 main(float4 Position : SV_Position, float2 UV : TEXCOORD0) : SV_Target0
{
    uint width, height;
    BlitTexture.GetDimensions(width, height);
    return BlitTexture.Load(int3(uint2(UV * float2(width, height)), 0));
}