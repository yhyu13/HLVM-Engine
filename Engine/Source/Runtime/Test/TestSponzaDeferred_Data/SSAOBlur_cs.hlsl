/*
 * SSAO Box Blur Compute Shader
 * 3x3 box blur with edge-aware bounds checking
 */

cbuffer BlurConstants : register(b0)
{
    float2 ScreenSize;
    float2 Pad;
};

Texture2D<float>   t_Input  : register(t0);
RWTexture2D<float> u_Output : register(u0);

[numthreads(8, 8, 1)]
void main(uint2 dispatchThreadId : SV_DispatchThreadID)
{
    uint2 pixelCoord = dispatchThreadId;
    if (pixelCoord.x >= uint(ScreenSize.x) || pixelCoord.y >= uint(ScreenSize.y))
        return;

    float sum = 0.0;
    int count = 0;

    [unroll]
    for (int y = -1; y <= 1; y++)
    {
        [unroll]
        for (int x = -1; x <= 1; x++)
        {
            int2 sampleCoord = int2(pixelCoord) + int2(x, y);
            // Bounds check to avoid out-of-bounds reads at screen edges
            if (all(sampleCoord >= 0) && all(sampleCoord < int2(ScreenSize)))
            {
                sum += t_Input[uint2(sampleCoord)];
                count++;
            }
        }
    }

    u_Output[pixelCoord] = sum / float(count);
}
