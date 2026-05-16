/*
 * Screen-Space Ambient Occlusion (SSAO) Compute Shader
 * Simple hemisphere sampling with view-space position reconstruction
 */

cbuffer SSAOConstants : register(b0)
{
    float4x4 ProjMatrix;       // View -> Clip
    float4x4 InvProjMatrix;    // Clip -> View (for position reconstruction)
    float4x4 ViewMatrix;       // World -> View (for normal transformation)
    float2   ScreenSize;
    float    SampleRadius;
    float    Bias;
    int      SampleCount;
    float    MinAO;
    float2   Pad0;
};

Texture2D<float>  t_Depth   : register(t0);
Texture2D<float4> t_Normal  : register(t1);

RWTexture2D<float> u_SSAO   : register(u0);

// Reconstruct view-space position from depth + pixel coord
float3 ReconstructViewPos(uint2 pixelCoord, float depth, float4x4 invProj)
{
    float2 uv = (float2(pixelCoord) + 0.5) / ScreenSize;
    float2 ndc = uv * 2.0 - 1.0;
    // Vulkan NDC Y is down; flip Y for correct reconstruction (matches lighting pass)
    float4 viewPosH = mul(invProj, float4(ndc.x, -ndc.y, depth, 1.0));
    return viewPosH.xyz / viewPosH.w;
}

// Pseudo-random hash for sample jitter
float Hash(float2 p)
{
    return frac(sin(dot(p, float2(127.1, 311.7))) * 43758.5453);
}

// 8 precomputed directions on a unit hemisphere (z-up)
static const float3 HemisphereSamples[8] = {
    float3( 0.5000,  0.5000,  0.7071),
    float3(-0.5000,  0.5000,  0.7071),
    float3( 0.5000, -0.5000,  0.7071),
    float3(-0.5000, -0.5000,  0.7071),
    float3( 0.7071,  0.0000,  0.7071),
    float3(-0.7071,  0.0000,  0.7071),
    float3( 0.0000,  0.7071,  0.7071),
    float3( 0.0000, -0.7071,  0.7071)
};

[numthreads(8, 8, 1)]
void main(uint2 dispatchThreadId : SV_DispatchThreadID)
{
    uint2 pixelCoord = dispatchThreadId;
    if (pixelCoord.x >= uint(ScreenSize.x) || pixelCoord.y >= uint(ScreenSize.y))
        return;

    float depth = t_Depth[pixelCoord];

    // Reconstruct view-space position
    float3 viewPos = ReconstructViewPos(pixelCoord, depth, InvProjMatrix);

    // Decode world normal [0,1] -> [-1,1] and transform to view space
    float3 worldNormal = normalize(t_Normal[pixelCoord].xyz * 2.0 - 1.0);
    float3 viewNormal = normalize(mul((float3x3)ViewMatrix, worldNormal));

    // Build TBN around viewNormal
    float3 tangent = normalize(cross(viewNormal, float3(0.0, 1.0, 0.0)));
    if (length(tangent) < 0.001)
        tangent = float3(1.0, 0.0, 0.0);
    float3 bitangent = cross(viewNormal, tangent);

    // Random rotation angle based on pixel position
    float randomAngle = Hash(float2(pixelCoord)) * 6.2831853;
    float cosA = cos(randomAngle);
    float sinA = sin(randomAngle);

    float occlusion = 0.0;

    for (int i = 0; i < SampleCount; i++)
    {
        float3 dir = HemisphereSamples[i];

        // Rotate direction in tangent-bitangent plane
        float3 rotatedDir;
        rotatedDir.x = dir.x * cosA - dir.y * sinA;
        rotatedDir.y = dir.x * sinA + dir.y * cosA;
        rotatedDir.z = dir.z;

        // Transform from local hemisphere to view space
        float3 offset = tangent * rotatedDir.x
                      + bitangent * rotatedDir.y
                      + viewNormal * rotatedDir.z;
        offset *= SampleRadius;

        // Offset position in view space
        float3 samplePos = viewPos + offset;

        // Project back to screen space
        float4 clip = mul(ProjMatrix, float4(samplePos, 1.0));
        float3 ndc = clip.xyz / clip.w;

        // NDC to UV (Vulkan DX-style viewport: flip Y)
        float2 sampleUV;
        sampleUV.x = ndc.x * 0.5 + 0.5;
        sampleUV.y = -ndc.y * 0.5 + 0.5;

        // Check UV bounds
        if (sampleUV.x < 0.0 || sampleUV.x > 1.0 || sampleUV.y < 0.0 || sampleUV.y > 1.0)
            continue;

        // Check depth bounds
        if (ndc.z < 0.0 || ndc.z > 1.0)
            continue;

        // Sample depth at projected screen position
        uint2 samplePixel = uint2(sampleUV * ScreenSize + 0.5);
        float sampleDepth = t_Depth[samplePixel];

        // Reconstruct view-space position of the geometry at samplePixel
        float3 sampleViewPos = ReconstructViewPos(samplePixel, sampleDepth, InvProjMatrix);

        // Distance-based range check (ignore far-away geometry)
        float dist = length(sampleViewPos - viewPos);
        float rangeCheck = smoothstep(0.0, 1.0, SampleRadius / max(dist, 0.001));

        // Occlusion test: if actual geometry is closer than sample position (by more than bias)
        // In left-handed view space, smaller Z means closer to camera
        if (sampleViewPos.z < samplePos.z - Bias)
        {
            occlusion += rangeCheck;
        }
    }

    float ao = 1.0 - (occlusion / float(SampleCount));
    u_SSAO[pixelCoord] = saturate(ao);
}
