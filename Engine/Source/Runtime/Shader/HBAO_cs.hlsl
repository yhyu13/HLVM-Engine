/*
 * Horizon-Based Ambient Occlusion (HBAO) Compute Shader
 *
 * Algorithm: 4 directions x 6 steps with per-pixel random rotation.
 * For each direction, march along screen-space ray, track maximum horizon
 * elevation angle, integrate occlusion between tangent plane and horizon.
 *
 * Key features:
 *   - Normal-aware tangent plane (prevents false occlusion on flat surfaces)
 *   - Directional horizon sampling (coherent, smoother than random hemisphere)
 *   - Distance falloff (scene-scale radius)
 *   - Sky/background early-out
 */

cbuffer HBAOConstants : register(b0)
{
    float4x4 ProjMatrix;
    float4x4 InvProjMatrix;
    float4x4 ViewMatrix;
    float2   ScreenSize;
    float2   InvScreenSize;
    float    SampleRadius;
    float    AngleBias;
    float    MaxRadiusPixels;
    float    AttenuationScale;
    float    MinAO;
    int      DirectionCount;
    int      StepCount;
    float4   Pad0;
};

Texture2D<float>  t_Depth  : register(t0);
Texture2D<float4> t_Normal : register(t1);

RWTexture2D<float> u_HBAO  : register(u0);

// Reconstruct view-space position from depth + pixel coord
float3 ReconstructViewPos(uint2 pixelCoord, float depth, float4x4 invProj)
{
    float2 uv = (float2(pixelCoord) + 0.5) / ScreenSize;
    float2 ndc = uv * 2.0 - 1.0;
    // Vulkan NDC Y is down; flip Y for correct reconstruction
    float4 viewPosH = mul(invProj, float4(ndc.x, -ndc.y, depth, 1.0));
    return viewPosH.xyz / viewPosH.w;
}

// Pseudo-random hash for per-pixel direction jitter
float Hash(float2 p)
{
    return frac(sin(dot(p, float2(127.1, 311.7))) * 43758.5453);
}

[numthreads(8, 8, 1)]
void main(uint2 dispatchThreadId : SV_DispatchThreadID)
{
    uint2 pixelCoord = dispatchThreadId;
    if (pixelCoord.x >= uint(ScreenSize.x) || pixelCoord.y >= uint(ScreenSize.y))
        return;

    float depth = t_Depth[pixelCoord];

    // Early-out for sky/background (no geometry)
    // Depth of 1.0 means far plane; also catch NaN/invalid depth
    if (depth >= 1.0 || depth <= 0.0)
    {
        u_HBAO[pixelCoord] = 1.0;
        return;
    }

    // Reconstruct view-space position and normal
    float3 viewPos = ReconstructViewPos(pixelCoord, depth, InvProjMatrix);

    // Decode world normal [0,1] -> [-1,1] and transform to view space
    float3 worldNormal = normalize(t_Normal[pixelCoord].xyz * 2.0 - 1.0);
    float3 viewNormal = normalize(mul((float3x3)ViewMatrix, worldNormal));

    // Per-pixel random rotation angle
    float randomAngle = Hash(float2(pixelCoord)) * 6.28318530718;
    float cosR = cos(randomAngle);
    float sinR = sin(randomAngle);

    float occlusion = 0.0;
    float numDirections = float(DirectionCount);
    float numSteps = float(StepCount);

    // Precompute view direction (camera at origin, looking down -Z)
    float3 viewDir = float3(0.0, 0.0, -1.0);

    for (int d = 0; d < DirectionCount; d++)
    {
        // Base direction angle
        float baseAngle = (float(d) / numDirections) * 6.28318530718;
        // Apply random rotation
        float angle = baseAngle + randomAngle;
        float2 dir2D = float2(cos(angle), sin(angle));

        // Compute per-direction tangent angle from normal
        // Tangent vector = intersection of tangent plane with the plane spanned by viewDir and dir2D
        float3 bitangent = normalize(cross(viewDir, float3(dir2D, 0.0)));
        float3 tangent = cross(bitangent, viewNormal);
        float tangentLen = length(tangent);

        float tangentAngle;
        if (tangentLen < 0.001)
        {
            // Degenerate: direction is along the surface tangent; assume flat
            tangentAngle = 0.0;
        }
        else
        {
            tangent = tangent / tangentLen;
            tangentAngle = atan2(tangent.z, length(tangent.xy));
        }

        float horizonAngle = -1.57079632679; // -PI/2

        // March along direction with quadratic step spacing
        for (int s = 1; s <= StepCount; s++)
        {
            float t = float(s) / numSteps;
            // Quadratic spacing: more samples near the center
            float stepDistPixels = MaxRadiusPixels * t * t;

            float2 offsetPixels = dir2D * stepDistPixels;
            int2 samplePixel = int2(float2(pixelCoord) + offsetPixels + 0.5);

            // Clamp to screen bounds
            samplePixel = clamp(samplePixel, int2(0, 0), int2(ScreenSize) - 1);

            float sampleDepth = t_Depth[samplePixel];

            // Skip invalid depth
            if (sampleDepth >= 1.0 || sampleDepth <= 0.0)
                continue;

            float3 sampleViewPos = ReconstructViewPos(uint2(samplePixel), sampleDepth, InvProjMatrix);

            float3 delta = sampleViewPos - viewPos;
            float deltaLen = length(delta);

            // Distance falloff: attenuate distant occluders
            float falloff = saturate(1.0 - deltaLen / SampleRadius);
            falloff = falloff * falloff; // quadratic falloff

            if (falloff <= 0.0)
                continue;

            // Elevation angle of this sample relative to the pixel
            float elevation = atan2(delta.z, length(delta.xy));

            // Update horizon
            if (elevation > horizonAngle)
            {
                horizonAngle = elevation;
            }
        }

        // Integrate occlusion for this direction
        // Occlusion = area between tangent plane and horizon
        float sinHorizon = sin(horizonAngle);
        float sinTangent = sin(tangentAngle + AngleBias);
        float dirAO = max(0.0, sinHorizon - sinTangent);

        occlusion += dirAO;
    }

    // Average over directions and apply attenuation scale
    float ao = 1.0 - (occlusion / numDirections) * AttenuationScale;
    ao = max(ao, MinAO);
    u_HBAO[pixelCoord] = saturate(ao);
}
