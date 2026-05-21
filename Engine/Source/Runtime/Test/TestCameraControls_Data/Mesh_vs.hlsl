struct VSInput
{
    float3 Position : POSITION;
    float3 Normal   : NORMAL;
    float2 TexCoord : TEXCOORD;
};

struct PSInput
{
    float4 Position : SV_POSITION;
    float3 Color    : COLOR;
};

cbuffer MVPConstants : register(b0)
{
    float4x4 MVP;
    float3   ObjectColor;
    float    Pad;
};

PSInput main(VSInput input)
{
    PSInput output;
    output.Position = mul(MVP, float4(input.Position, 1.0));
    output.Color = ObjectColor;
    return output;
}
