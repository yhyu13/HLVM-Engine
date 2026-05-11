// Simple Fragment Shader for Cube
// Passes through color from vertex shader

struct PS_INPUT
{
    float4 Position : SV_Position;
    float3 Color : COLOR0;
};

float4 main(PS_INPUT Input) : SV_Target0
{
    return float4(Input.Color, 1.0);
}