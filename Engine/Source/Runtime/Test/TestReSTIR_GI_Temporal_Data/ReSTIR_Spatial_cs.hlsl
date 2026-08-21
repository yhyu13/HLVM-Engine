// ReSTIR GI — Spatial Resampling [ZetaRay ground truth, Phase 3]
//
// Port of ZetaRay's RGI_Util::PairwiseMIS / SpatialResample
// (Source/ZetaRenderPass/IndirectLighting/ReSTIR_GI/{PairwiseMIS,Resampling}.hlsli).
//
// The center reservoir (from the temporal pass) is merged with geometrically
// valid neighbor reservoirs via pairwise MIS:
//   m_i = (M_i * p_i_y_i) / (M_i * p_i_y_i / J + (M_c / k) * p_c_y_i)
//   m_c accumulates the complementary balance term
//   w_i = m_i * targetLum_i * W_i
// Final:
//   W = w_sum / (targetLum(selected) * (1 + k))
// The output is the indirect estimate target_z * W at the current pixel.
// Segment visibility is Phase 4.

struct FReSTIRSpatialConstants
{
    float2 OutputSize;
    float2 RcpOutputSize;
    float NormalThreshold;
    float DepthThreshold;
    float MaxM;
    float SpatialRadius;
    float DebugVis;
    float GBufferScale;
    float Pad;
};

cbuffer Constants : register(b0)
{
    FReSTIRSpatialConstants gConstants;
}

Texture2D<float4> gRadiance  : register(t0);
Texture2D<float4> gReservoir0 : register(t1);
Texture2D<float4> gReservoir1 : register(t2);
Texture2D<float4> gNormals   : register(t3);
Texture2D<float>  gDepth     : register(t4);
Texture2D<float4> gReservoir2 : register(t5);
Texture2D<float4> gWorldPos  : register(t6);
Texture2D<float4> gMaterial  : register(t7);
RaytracingAccelerationStructure g_bvh : register(t8);  // v211: segment visibility

RWTexture2D<float4> gOutput : register(u0);

static const float k_PI = 3.14159265f;

float Luminance(float3 c) { return dot(c, float3(0.2126, 0.7152, 0.0722)); }

float2 OctEncode(float3 n)
{
    n /= (abs(n.x) + abs(n.y) + abs(n.z));
    if (n.z < 0.0)
        n.xy = (1.0 - abs(n.yx)) * (n.xy >= 0.0 ? 1.0 : -1.0);
    return n.xy * 0.5 + 0.5;
}

float3 OctDecode(float2 e)
{
    e = e * 2.0 - 1.0;
    float3 n = float3(e, 1.0 - abs(e.x) - abs(e.y));
    if (n.z < 0.0)
        n.xy = (1.0 - abs(n.yx)) * (n.xy >= 0.0 ? 1.0 : -1.0);
    return normalize(n);
}

int2 GB(int2 p)
{
    int s = max(int(gConstants.GBufferScale), 1);
    return p * s + (s >> 1);
}

float Hash01(uint2 p, uint s)
{
    uint state = p.x * 747796405u + p.y * 2891336453u + s * 277803737u;
    state = (state >> 13u) ^ state;
    state *= 0x85ebca6bu;
    state ^= state >> 16u;
    return float(state & 0xFFFFFFu) / float(0x1000000u);
}

struct FReservoir
{
    float3 pos;
    float3 normal;
    float3 Lo;
    float3 targetZ;
    uint   ID;
    float  M;
    float  w_sum;
    float  W;
};

FReservoir LoadReservoir(int2 pixel)
{
    FReservoir r;
    float4 a = gReservoir0.Load(int3(pixel, 0));
    float4 b = gReservoir1.Load(int3(pixel, 0));
    float4 c = gReservoir2.Load(int3(pixel, 0));
    r.pos    = a.xyz;
    r.ID     = asuint(a.w);
    r.Lo     = b.rgb;
    r.M      = b.w;
    r.w_sum  = c.x;
    r.W      = c.y;
    r.normal = OctDecode(c.zw);
    r.targetZ = 0;
    return r;
}

float3 EvalF(float3 albedo, float3 normal, float3 wi)
{
    return albedo * (max(dot(normal, wi), 0.0f) * (1.0f / k_PI));
}

float JacobianReconnectionShift(float3 x2_normal, float3 x1_r, float3 x1_q, float3 x2_q)
{
    float3 v_r = x1_r - x2_q;
    const float t_r2 = dot(v_r, v_r);
    v_r = dot(v_r, v_r) == 0 ? v_r : v_r / max(sqrt(t_r2), 1e-6f);

    float3 v_q = x1_q - x2_q;
    const float t_q2 = dot(v_q, v_q);
    v_q = dot(v_q, v_q) == 0 ? v_q : v_q / max(sqrt(t_q2), 1e-6f);

    float cosPhi_r = dot(v_r, x2_normal);
    float cosPhi_q = dot(v_q, x2_normal);

    return (abs(cosPhi_r) * t_q2) / max(abs(cosPhi_q) * t_r2, 1e-6f);
}

// ---- v211 (Phase 4): ZetaRay Visibility_Segment port ----------------------
bool VisibilitySegment(float3 origin, float3 wi, float rayT, float3 normal, uint triID,
                       RaytracingAccelerationStructure bvh)
{
    if (triID == 0xFFFFFFFFu || rayT < 1e-6f)
        return false;
    float ndotwi = dot(normal, wi);
    if (ndotwi == 0.0f)
        return false;
    if (ndotwi < 0.0f)
        return false;

    float3 adjustedOrigin = origin + normal * 1e-4f;

    RayQuery<RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH |
             RAY_FLAG_SKIP_CLOSEST_HIT_SHADER |
             RAY_FLAG_FORCE_OPAQUE> rayQuery;

    RayDesc ray;
    ray.Origin = adjustedOrigin;
    ray.TMin = 3e-6;
    ray.TMax = max(rayT * 0.999f, 1e-5f);
    ray.Direction = wi;

    rayQuery.TraceRayInline(bvh, RAY_FLAG_NONE, 0xFF, ray);
    rayQuery.Proceed();

    if (rayQuery.CommittedStatus() == COMMITTED_TRIANGLE_HIT)
    {
        uint hitInst = rayQuery.CommittedInstanceID();
        uint hitPrim = rayQuery.CommittedPrimitiveIndex();
        uint sampleInst = (triID >> 24) & 0xFFu;
        uint samplePrim = triID & 0xFFFFFFu;
        return (hitInst == sampleInst && hitPrim == samplePrim);
    }
    return true;
}

static const float k_MaxPlaneDistReuse = 0.005f;

bool PlaneHeuristic(float3 samplePos, float3 currNormal, float3 currPos, float linearDepth)
{
    return abs(dot(currNormal, samplePos - currPos)) <= k_MaxPlaneDistReuse * linearDepth;
}

// ---- ZetaRay PairwiseMIS ----------------------------------------------------------

struct FPairwiseMIS
{
    FReservoir r_s;
    float m_c;
    float M_s;
    float k;
};

FPairwiseMIS InitPairwiseMIS(float numSamples, FReservoir r_c)
{
    FPairwiseMIS ret;
    ret.r_s.pos = 0;
    ret.r_s.normal = 0;
    ret.r_s.Lo = 0;
    ret.r_s.targetZ = 0;
    ret.r_s.ID = 0xFFFFFFFFu;
    ret.r_s.M = 0;
    ret.r_s.w_sum = 0;
    ret.r_s.W = 0;
    ret.m_c = 1.0f;
    ret.M_s = r_c.M;
    ret.k = numSamples;
    return ret;
}

float Compute_m_i(FReservoir r_c, float targetLum, FReservoir r_i, float jacobianNeighborToCurr, float k)
{
    const float p_i_y_i = r_i.W > 0 ? (r_i.M * r_i.w_sum) / max(r_i.W, 1e-6f) : 0;
    const float p_c_y_i = targetLum * r_c.M;
    float numerator = r_i.M * p_i_y_i;
    // ZetaRay: denominator = M_i * p_i_y_i / J + (M_c / k) * p_c_y_i
    float denom = numerator / max(jacobianNeighborToCurr, 1e-6f) + (r_c.M / max(k, 1e-6f)) * p_c_y_i;
    return denom > 0 ? numerator / max(denom, 1e-6f) : 0;
}

void Update_m_c(inout FPairwiseMIS p, FReservoir r_c, FReservoir r_i, float3 brdfCosTheta_i,
                float jacobianCurrToNeighbor)
{
    if (r_i.ID == 0xFFFFFFFFu)
    {
        p.m_c += 1.0f;
        return;
    }

    const float target_i = Luminance(r_c.Lo * brdfCosTheta_i);
    const float p_i_y_c = target_i * jacobianCurrToNeighbor;
    const float p_c_y_c = max(Luminance(r_c.targetZ), 0.0f);

    const float numerator = r_i.M * p_i_y_c;
    const bool denomGt0 = (p_c_y_c + numerator) > 0;
    p.m_c += denomGt0 ? 1.0f - numerator / max(numerator + (r_c.M / max(p.k, 1e-6f)) * p_c_y_c, 1e-6f) : 1.0f;
}

void Stream(inout FPairwiseMIS p, FReservoir r_c, float3 posW_c, float3 normal_c,
            float3 albedo_c, FReservoir r_i, float3 posW_i, float3 normal_i, float3 albedo_i,
            inout float rng)
{
    float3 target_curr = 0;
    float m_i = 0;

    if (r_i.ID != 0xFFFFFFFFu)
    {
        float3 wi = r_i.pos - posW_c;
        float t = length(wi);
        wi = t > 1e-6f ? wi / t : float3(0.0f, 1.0f, 0.0f);

        float3 brdfCosTheta_c = EvalF(albedo_c, normal_c, wi);
        target_curr = r_i.Lo * brdfCosTheta_c;
        // v211: segment visibility between the current pixel and the
        // neighbor's sample vertex.
        if (Luminance(target_curr) > 1e-5f)
            target_curr *= VisibilitySegment(posW_c, wi, t, normal_c, r_i.ID, g_bvh) ? 1.0f : 0.0f;

        const float targetLum = max(Luminance(target_curr), 0.0f);
        const float J_temporal_to_curr = JacobianReconnectionShift(r_i.normal, posW_c, posW_i, r_i.pos);
        m_i = Compute_m_i(r_c, targetLum, r_i, J_temporal_to_curr, p.k);
    }

    float3 brdfCosTheta_i = 0;
    if (r_c.ID != 0xFFFFFFFFu)
    {
        float3 wi = r_c.pos - posW_i;
        float t = length(wi);
        wi = t > 1e-6f ? wi / t : float3(0.0f, 1.0f, 0.0f);

        brdfCosTheta_i = EvalF(albedo_i, normal_i, wi);
        if (Luminance(r_c.Lo * brdfCosTheta_i) > 1e-5f)
            brdfCosTheta_i *= VisibilitySegment(posW_i, wi, t, normal_i, r_c.ID, g_bvh) ? 1.0f : 0.0f;

        float J_curr_to_temporal = JacobianReconnectionShift(r_c.normal, posW_i, posW_c, r_c.pos);
        Update_m_c(p, r_c, r_i, brdfCosTheta_i, J_curr_to_temporal);
    }

    if (r_i.ID != 0xFFFFFFFFu)
    {
        const float w_i = m_i * max(Luminance(target_curr), 0.0f) * r_i.W;
        // Reservoir stream
        p.r_s.w_sum += w_i;
        p.r_s.M += 1.0f;
        if (rng < w_i / max(1e-6f, p.r_s.w_sum))
        {
            p.r_s.pos = r_i.pos;
            p.r_s.normal = r_i.normal;
            p.r_s.ID = r_i.ID;
            p.r_s.Lo = r_i.Lo;
            p.r_s.targetZ = target_curr;
        }
    }

    p.M_s += r_i.M;
}

void End(inout FPairwiseMIS p, FReservoir r_c, float3 posW_c, inout float rng)
{
    float3 wi = r_c.pos - posW_c;
    float t = length(wi);
    wi = t > 1e-6f ? wi / t : float3(0.0f, 1.0f, 0.0f);

    const float w_c = max(Luminance(r_c.targetZ), 0.0f) * r_c.W * p.m_c;
    p.r_s.w_sum += w_c;
    p.r_s.M += 1.0f;
    if (rng < w_c / max(1e-6f, p.r_s.w_sum))
    {
        p.r_s.pos = r_c.pos;
        p.r_s.normal = r_c.normal;
        p.r_s.ID = r_c.ID;
        p.r_s.Lo = r_c.Lo;
        p.r_s.targetZ = r_c.targetZ;
    }

    p.r_s.M = p.M_s;
    const float targetLum = max(Luminance(p.r_s.targetZ), 0.0f);
    p.r_s.W = targetLum > 0 ? p.r_s.w_sum / (targetLum * (1.0f + p.k)) : 0;
    p.r_s.W = isnan(p.r_s.W) ? 0 : p.r_s.W;
}

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    float2 outputSize = gConstants.OutputSize;
    if (dispatchThreadID.x >= (uint)outputSize.x || dispatchThreadID.y >= (uint)outputSize.y)
        return;

    int2 pixel = int2(dispatchThreadID.xy);
    int2 gb = GB(pixel);

    float3 worldPos = gWorldPos.Load(int3(gb, 0)).rgb;
    float3 normal   = normalize(gNormals.Load(int3(gb, 0)).rgb * 2.0f - 1.0f);
    float3 albedo   = max(gMaterial.Load(int3(gb, 0)).rgb, 0.0f);
    float  viewZ    = gDepth.Load(int3(gb, 0));

    FReservoir r = LoadReservoir(pixel);
    if (r.ID == 0xFFFFFFFFu || r.M == 0)
    {
        gOutput[pixel] = float4(0.0f, 0.0f, 0.0f, 0.0f);
        return;
    }

    // Current sample's target at this pixel.
    {
        float3 wi = normalize(r.pos - worldPos);
        r.targetZ = r.Lo * EvalF(albedo, normal, wi);
    }

    // ---- ZetaRay SpatialResample: Hammersley offsets, rotated per pixel ----
    static const float2 k_hammersley[8] =
    {
        float2(0.0, -0.7777777777777778),
        float2(-0.5, -0.5555555555555556),
        float2(0.5, -0.33333333333333337),
        float2(-0.75, -0.11111111111111116),
        float2(0.25, 0.11111111111111116),
        float2(-0.25, 0.33333333333333326),
        float2(0.75, 0.5555555555555556),
        float2(-0.875, 0.7777777777777777)
    };

    const float u0 = Hash01(uint2(pixel), 7u);
    const uint offset = uint(Hash01(uint2(pixel), 9u) * 8.0f) & 7u;
    const float theta = u0 * 6.2831853f;
    const float sinTheta = sin(theta);
    const float cosTheta = cos(theta);
    const float radius = gConstants.SpatialRadius;

    FPairwiseMIS pairwiseMIS = InitPairwiseMIS(2.0f, r);

    float3 samplePosW[2];
    int2 samplePosSS[2];
    float3 sampleNormal[2];
    float3 sampleAlbedo[2];
    int k = 0;

    for (int i = 0; i < 4 && k < 2; i++)
    {
        float2 sampleUV = k_hammersley[(offset + i) & 7];
        float2 rotated;
        rotated.x = dot(sampleUV, float2(cosTheta, -sinTheta));
        rotated.y = dot(sampleUV, float2(sinTheta, cosTheta));
        rotated *= radius;

        int2 posSS_i = int2(round(float2(pixel) + rotated));
        if (posSS_i.x < 0 || posSS_i.y < 0 ||
            posSS_i.x >= (int)outputSize.x || posSS_i.y >= (int)outputSize.y)
            continue;

        int2 gb_i = GB(posSS_i);
        float depth_i = gDepth.Load(int3(gb_i, 0));
        if (depth_i <= 0.0f)
            continue;

        float3 posW_i = gWorldPos.Load(int3(gb_i, 0)).rgb;
        if (!PlaneHeuristic(posW_i, normal, worldPos, viewZ))
            continue;

        float3 normal_i = normalize(gNormals.Load(int3(gb_i, 0)).rgb * 2.0f - 1.0f);
        float normalSimilarity = dot(normal_i, normal);
        if (normalSimilarity <= 0.0f)
            continue;

        samplePosW[k] = posW_i;
        samplePosSS[k] = posSS_i;
        sampleNormal[k] = normal_i;
        sampleAlbedo[k] = max(gMaterial.Load(int3(gb_i, 0)).rgb, 0.0f);
        k++;
    }

    pairwiseMIS.k = float(k);
    uint rngBase = uint(gConstants.OutputSize.x) * 31u + uint(gConstants.OutputSize.y);

    [unroll]
    for (int j = 0; j < 2; j++)
    {
        if (j >= (int)k)
            break;
        FReservoir neighbor = LoadReservoir(samplePosSS[j]);
        if (neighbor.ID == 0xFFFFFFFFu)
            continue;
        // ZetaRay advances the RNG per reservoir update.
        float rng = Hash01(uint2(pixel), rngBase + uint(j) * 7u);
        Stream(pairwiseMIS, r, worldPos, normal, albedo, neighbor,
               samplePosW[j], sampleNormal[j], sampleAlbedo[j], rng);
    }

    float endRng = Hash01(uint2(pixel), rngBase + 13u);
    End(pairwiseMIS, r, worldPos, endRng);
    r = pairwiseMIS.r_s;
    r.M = min(r.M, gConstants.MaxM);

    gOutput[pixel] = float4(r.targetZ * r.W, 1.0f);
}
