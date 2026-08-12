// ReSTIR GI — Generation Pass (RealEngine-modeled).
//
// The per-pixel GI sample is produced by FGIPass (GIPathTracing.hlsl):
//   gRadiance.rgb = sample radiance, gRadiance.a = first-hit distance.
// This pass packages that sample into a reservoir:
//   Reservoir0 = float4(radiance.rgb, hitT)     — the selected sample
//   Reservoir1 = float4(M, W, 0, 0)             — M=1 (one sample), W=1
//
// The reservoir sample is a REAL ray sample (radiance + hit distance), not a
// screen-pixel index. Temporal and spatial passes then reuse it with proper
// weighted reservoir sampling (see RealEngine shaders/restir_gi/reservoir.hlsli).

struct FReSTIRConstants
{
    float2 OutputSize;
    float2 RcpOutputSize;
    float FrameIndex;
    float NumCandidates;
    float DepthThreshold;
    float NormalThreshold;
    float DebugVis;
    float2 Pad;
};

cbuffer Constants : register(b0)
{
    FReSTIRConstants gConstants;
}

Texture2D<float4> gRadiance : register(t0);
Texture2D<float4> gDirection : register(t4);   // primary GI ray direction (Phase B)
Texture2D<float4> gWorldPos : register(t1);
Texture2D<float4> gNormals : register(t2);
Texture2D<float> gDepth : register(t3);

// v151 (six-role-pipeline): match the temporal shader's set-1 placement
// for the UAVs (register(u0, space1) etc). The C++ side now composes two
// binding layouts (GenerationLayoutSRV = set 0, GenerationLayoutUAV = set 1)
// so the SRV reads and UAV writes get unambiguous layouts — the
// nvrhi-deferred-barrier-ordering fix that already landed on the temporal
// layout.
RWTexture2D<float4> gReservoir0 : register(u0, space1);
RWTexture2D<float4> gReservoir1 : register(u1, space1);

// Octahedral direction encoding — packs a unit vector into 2 floats so the
// reservoir can carry the real ray sample (Phase B).
float2 OctEncode(float3 n)
{
    n /= (abs(n.x) + abs(n.y) + abs(n.z));
    if (n.z < 0.0)
        n.xy = (1.0 - abs(n.yx)) * (n.xy >= 0.0 ? 1.0 : -1.0);
    return n.xy * 0.5 + 0.5;
}

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    float2 outputSize = gConstants.OutputSize;
    if (dispatchThreadID.x >= (uint)outputSize.x || dispatchThreadID.y >= (uint)outputSize.y)
        return;

    int2 pixel = int2(dispatchThreadID.xy);

    float4 sample = gRadiance.Load(int3(pixel, 0));
    float3 dir = gDirection.Load(int3(pixel, 0)).xyz;
    if (!all(isfinite(dir)) || length(dir) < 1e-4f)
        dir = float3(0.0, 1.0, 0.0);

    // Reservoir with a single sample: M = 1, W = 1, direction = the ray.
    gReservoir0[pixel] = float4(sample.rgb, sample.a);
    gReservoir1[pixel] = float4(1.0, 1.0, OctEncode(dir));
}
