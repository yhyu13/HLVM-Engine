// ReSTIR Generation Pass - Streaming RIS with M candidates per pixel

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

// PCG hash (better quality than simple LCG)
uint pcg_hash(uint v)
{
    uint state = v * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

float2 hash22(float2 p)
{
    uint3 q = uint3(int2(p * 4194304.0), uint(gConstants.FrameIndex));
    uint n = pcg_hash(q.x + pcg_hash(q.y + pcg_hash(q.z)));
    return float2(n & 0xFFFFu, n >> 16u) / 65535.0;
}

float Luminance(float3 rgb)
{
    return dot(rgb, float3(0.2126, 0.7152, 0.0722));
}

// Sample tent distribution: maps uniform [0,1] to tent-shaped PDF
// Returns value in [-1, 1] with peak at 0
float2 SampleTent(float2 u)
{
    float2 v = 2.0 * u - 1.0;
    float2 s = sign(v);
    float2 tent = s * (1.0 - sqrt(1.0 - abs(v)));
    return tent;
}

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    float2 outputSize = gConstants.OutputSize;
    if (dispatchThreadID.x >= (uint)outputSize.x || dispatchThreadID.y >= (uint)outputSize.y)
        return;

    int2 pixel = int2(dispatchThreadID.xy);

    // Streaming RIS: accumulate M candidates
    float w_sum = 0.0;
    float2 y = float2(pixel); // selected sample position
    int M = int(gConstants.NumCandidates);
    float radius = 2.0; // candidate search radius in pixels

    for (int i = 0; i < M; ++i)
    {
        // Hash-based random for deterministic but varied sampling
        // Deterministic candidates for static-scene stability (no FrameIndex in offset)
        float2 h = hash22(float2(pixel) + float2(i, i * 13.0));
        float2 offset = SampleTent(h) * radius;
        int2 q = clamp(pixel + int2(offset + float2(0.5, 0.5)), int2(0, 0), int2(outputSize) - int2(1, 1));

        // Evaluate target function p_hat = luminance of radiance at candidate q
        float3 radiance = gRadiance.Load(int3(q, 0)).rgb;
        float p_hat = Luminance(radiance);

        // Streaming RIS update
        // q(y) = 1 / (4 * radius^2) for uniform tent, so w = p_hat / q(y) = p_hat * 4 * radius^2
        // But since q(y) is constant for all candidates, it cancels in the selection probability
        float w = p_hat;
        w_sum += w;

        // Select candidate with probability w / w_sum
        // Deterministic selection threshold for static-scene stability
        float2 h2 = hash22(float2(pixel) + float2(i + 100, i * 31.0));
        if (h2.x * w_sum < w)
        {
            y = float2(q);
        }
    }

    // Reservoir storage:
    // Reservoir0: xy = selected pixel y, z = w_sum, w = M
    // Reservoir1: x = W ( unbiased weight = w_sum / (M * p_hat(y)) ), y = pdf (p_hat(y)), zw = unused
    float3 selectedRadiance = gRadiance.Load(int3(int2(y), 0)).rgb;
    float selectedPhat = Luminance(selectedRadiance);
    float W = (selectedPhat > 0.0) ? (w_sum / (float(M) * selectedPhat)) : 0.0;

    gReservoir0[pixel] = float4(y, w_sum, float(M));
    gReservoir1[pixel] = float4(W, selectedPhat, 0.0, 0.0);
}
