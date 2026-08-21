// ReSTIR GI — Temporal Resampling [ZetaRay ground truth, Phases 1-2]
//
// Port of ZetaRay's RGI_Util::FindTemporalCandidate / TemporalResample1 /
// TemporalResample2 / JacobianReconnectionShift / PlaneHeuristic
// (Source/ZetaRenderPass/IndirectLighting/ReSTIR_GI/Resampling.hlsli).
//
// Convention: every position is in CURRENT-frame world coordinates. History
// reservoir positions (stored in the previous frame's world space) are rotated
// into the current frame with R_y(SceneYaw - PrevSceneYaw) — the turntable is
// a rigid Y rotation shared by every instance. Candidate search reads the
// CURRENT GBuffer at the reprojected pixel (same surface under exact
// reprojection; a true prev-GBuffer chain is a follow-up, and the segment
// visibility test is Phase 4).
//
// Reservoir layout (ZetaRay):
//   R0 = float4(x2Pos, asfloat(x2ID))
//   R1 = float4(Lo, M)
//   R2 = float4(w_sum, W, OctEncode(x2Normal))
// Final W = w_sum / targetLum(selected); M = M_curr + sum(M_prev).

struct FReSTIRTemporalConstants
{
    float4x4 InverseCurrViewProj;
    float4x4 PrevViewProj;
    float2 OutputSize;
    float2 RcpOutputSize;
    float FrameIndex;
    float MaxM;
    float DepthThreshold;
    float NormalThreshold;
    float DebugVis;
    float SceneYaw;      // scene Y-rotation this frame (deg)
    float PrevSceneYaw;  // scene Y-rotation previous frame (deg)
    float NearPlane;
    float FarPlane;
    float GBufferScale;  // full-res GBuffer width / half-res dispatch width
};

cbuffer Constants : register(b0)
{
    FReSTIRTemporalConstants gConstants;
}

Texture2D<float4> gCurrReservoir0 : register(t0);
Texture2D<float4> gCurrReservoir1 : register(t1);
Texture2D<float4> gHistReservoir0 : register(t2);
Texture2D<float4> gHistReservoir1 : register(t3);
Texture2D<float>  gDepth          : register(t4);
Texture2D<float4> gNormals        : register(t5);
Texture2D<float>  gPrevDepth      : register(t6);
Texture2D<float4> gPrevNormals    : register(t7);
Texture2D<float4> gCurrRadiance   : register(t8);
Texture2D<float4> gHistRadiance   : register(t9);
Texture2D<float4> gCurrReservoir2 : register(t10);
Texture2D<float4> gHistReservoir2 : register(t11);
Texture2D<float4> gWorldPos       : register(t12);  // primary surface (full-res)
Texture2D<float4> gMaterial       : register(t13);  // albedo.rgb + roughness.a (full-res)
Texture2D<float4> gPrevWorldPos   : register(t14);  // v210: true prev-frame surface
Texture2D<float4> gPrevMaterial   : register(t15);
RaytracingAccelerationStructure g_bvh : register(t16);  // v211: segment visibility

RWTexture2D<float4> gOutReservoir0 : register(u0, space1);
RWTexture2D<float4> gOutReservoir1 : register(u1, space1);
RWTexture2D<float4> gOutReservoir2 : register(u2, space1);
RWTexture2D<float4> gOutRadiance   : register(u3, space1);

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

// PCG-style hash -> [0,1)
float Hash01(uint2 p, uint f)
{
    uint state = p.x * 747796405u + p.y * 2891336453u + f * 277803737u;
    state = (state >> 13u) ^ state;
    state *= 0x85ebca6bu;
    state ^= state >> 16u;
    return float(state & 0xFFFFFFu) / float(0x1000000u);
}

float2 Hash2(uint2 p, uint f)
{
    return float2(Hash01(p, f), Hash01(p, f + 0x9E3779B9u));
}

// ---- Reservoir -----------------------------------------------------------------

struct FReservoir
{
    float3 pos;      // x2 world position (current frame coords)
    float3 normal;   // x2 shading normal
    float3 Lo;       // incident radiance at x2 towards the primary
    float3 targetZ;  // Lo * f evaluated at the owning pixel
    uint   ID;
    float  M;
    float  w_sum;
    float  W;
};

FReservoir LoadReservoir(int2 pixel, Texture2D<float4> r0, Texture2D<float4> r1, Texture2D<float4> r2)
{
    FReservoir r;
    float4 a = r0.Load(int3(pixel, 0));
    float4 b = r1.Load(int3(pixel, 0));
    float4 c = r2.Load(int3(pixel, 0));
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

void StoreReservoir(int2 pixel, FReservoir r)
{
    gOutReservoir0[pixel] = float4(r.pos, asfloat(r.ID));
    gOutReservoir1[pixel] = float4(r.Lo, r.M);
    gOutReservoir2[pixel] = float4(r.w_sum, r.W, OctEncode(r.normal));
}

// Lambert f (rendering-equation convention, includes |cos|)
float3 EvalF(float3 albedo, float3 normal, float3 wi)
{
    return albedo * (max(dot(normal, wi), 0.0f) * (1.0f / k_PI));
}

// ---- ZetaRay: JacobianReconnectionShift ------------------------------------------
// Jacobian of path reconnection in solid-angle measure.
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

    float j = (abs(cosPhi_r) * t_q2) / max(abs(cosPhi_q) * t_r2, 1e-6f);
    return j;
}

// ---- v211 (Phase 4): ZetaRay Visibility_Segment port ----------------------
// Returns true when the segment (origin, origin + wi*rayT) is unoccluded.
// A hit on the sample's OWN triangle (packed ID) is not an occluder. Opaque
// surfaces only (Sponza has no transmission in the GI path).
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

// ---- ZetaRay: PlaneHeuristic -----------------------------------------------------
static const float k_MaxPlaneDistReuse = 0.005f;

bool PlaneHeuristic(float3 samplePos, float3 currNormal, float3 currPos, float linearDepth)
{
    float planeDist = dot(currNormal, samplePos - currPos);
    return abs(planeDist) <= k_MaxPlaneDistReuse * linearDepth;
}

// ---- Candidate search (ZetaRay FindTemporalCandidate<2>, adapted) ----------------

struct FCandidate
{
    int2  posSS;
    float3 posW;
    float3 normal;
    float  roughness;
    bool   valid;
};

// Rotate a previous-frame world position into current-frame coordinates
// (turntable: rigid Y rotation, delta = SceneYaw - PrevSceneYaw).
float3 RotatePrevToCurr(float3 p)
{
    float yawDelta = radians(gConstants.SceneYaw - gConstants.PrevSceneYaw);
    float c = cos(yawDelta);
    float s = sin(yawDelta);
    return float3(c * p.x + s * p.z, p.y, -s * p.x + c * p.z);
}

void FindTemporalCandidates(int2 pixel, float3 posW, float3 normal, float viewZ,
                            float roughness, float2 prevUV, out FCandidate outC[2])
{
    outC[0].valid = false;
    outC[1].valid = false;
    outC[0].posSS = int2(0, 0);
    outC[1].posSS = int2(0, 0);

    if (any(prevUV < 0.0f) || any(prevUV > 1.0f))
        return;

    float2 renderDim = gConstants.OutputSize;
    int2 prevPixel = int2(prevUV * renderDim);
    int curr = 0;
    const uint frame = uint(gConstants.FrameIndex);

    [unroll]
    for (int i = 0; i < 3; i++)
    {
        float2 dir = float2(0.0f, 0.0f);
        if (i > 0)
        {
            float2 u = Hash2(uint2(pixel), frame * 3u + uint(i));
            float theta = u.x * 6.2831853f;
            dir = 16.0f * float2(cos(theta), sin(theta));
        }
        int2 samplePosSS = prevPixel + (i > 0) * int2(dir);

        if (samplePosSS.x < 0 || samplePosSS.y < 0 ||
            samplePosSS.x >= (int)renderDim.x || samplePosSS.y >= (int)renderDim.y)
            continue;
        if (i > 0 && samplePosSS.x == pixel.x && samplePosSS.y == pixel.y)
            continue;

        int2 gb = GB(samplePosSS);
        // v210: read the PREVIOUS frame's GBuffer at the reprojected pixel —
        // that is the surface that was actually there last frame. Under the
        // turntable the current frame's GBuffer at the same pixel is a
        // different surface.
        float depth = gPrevDepth.Load(int3(gb, 0));
        if (depth <= 0.0f)
            continue;

        float3 samplePosPrev = gPrevWorldPos.Load(int3(gb, 0)).rgb;
        float3 samplePos = RotatePrevToCurr(samplePosPrev);  // -> current coords
        if (!PlaneHeuristic(samplePos, normal, posW, viewZ))
            continue;

        float3 sampleNormal = normalize(gPrevNormals.Load(int3(gb, 0)).rgb * 2.0f - 1.0f);
        bool valid = dot(sampleNormal, normal) > 0.1f;
        if (roughness < 0.5f)
        {
            float sampleRoughness = gPrevMaterial.Load(int3(gb, 0)).a;
            valid = valid && (abs(sampleRoughness - roughness) < 0.15f);
        }

        if (valid)
        {
            outC[curr].valid = true;
            outC[curr].posSS = samplePosSS;
            outC[curr].posW = samplePos;
            outC[curr].normal = sampleNormal;
            outC[curr].roughness = gPrevMaterial.Load(int3(gb, 0)).a;
            curr++;
            if (curr == 2)
                break;
        }
    }
}

// ---- ZetaRay: TargetLumAtTemporalPixel --------------------------------------
float TargetLumAtTemporalPixel(FReservoir r, FCandidate candidate, float3 albedo, bool testVisibility)
{
    float3 wi = r.pos - candidate.posW;
    if (dot(wi, wi) == 0)
        return 0;
    wi = normalize(wi);
    float3 target = r.Lo * EvalF(albedo, candidate.normal, wi);
    float targetLum = Luminance(target);
    // v211: test whether the sample vertex is visible from the temporal pixel.
    if (testVisibility && targetLum > 1e-5f)
    {
        float t = length(r.pos - candidate.posW);
        if (!VisibilitySegment(candidate.posW, wi, t, candidate.normal, r.ID, g_bvh))
            return 0;
    }
    return targetLum;
}

// ---- ZetaRay: TemporalResample2 (2 candidates, pairwise MIS) ----------------------
void TemporalResample2(FReservoir r, float3 posW, float3 normal, float3 albedo,
                       FCandidate candidate[2], inout FReservoir r_prev[2],
                       inout float rng, inout float M_new)
{
    // Target at temporal pixel with current pixel's sample
    {
        float p_curr = max(Luminance(r.targetZ), 0.0f);
        float denom = p_curr;

        if (Luminance(r.Lo) > 1e-5f)
        {
            [unroll]
            for (int p = 0; p < 2; p++)
            {
                if (r_prev[p].M == 0)
                    continue;
                float targetLum_prev = TargetLumAtTemporalPixel(r, candidate[p], albedo, p != 0);
                float J_curr_to_temporal = JacobianReconnectionShift(r.normal, candidate[p].posW, posW, r.pos);
                denom += r_prev[p].M * J_curr_to_temporal * targetLum_prev;
            }
        }

        float m_curr = denom == 0 ? 0 : p_curr / denom;
        r.w_sum *= m_curr;
    }

    // Target at current pixel with temporal reservoir's samples
    [unroll]
    for (int i = 0; i < 2; i++)
    {
        float3 wi = r_prev[i].pos - posW;
        float t = all(wi == 0) ? 0 : length(wi);
        wi = t > 1e-6f ? wi / t : float3(0.0f, 1.0f, 0.0f);

        float3 target_curr = r_prev[i].Lo * EvalF(albedo, normal, wi);
        float targetLum_curr = Luminance(target_curr);
        if (targetLum_curr < 1e-5f)
            continue;

        if (VisibilitySegment(posW, wi, t, normal, r_prev[i].ID, g_bvh))
        {
            float targetLum_prev = r_prev[i].W > 0 ? r_prev[i].w_sum / r_prev[i].W : 0;
            float J_temporal_to_curr = JacobianReconnectionShift(r_prev[i].normal, posW,
                candidate[i].posW, r_prev[i].pos);
            float numerator = r_prev[i].M * targetLum_prev;
            float denom = (numerator / max(J_temporal_to_curr, 1e-6f)) + targetLum_curr;
            if (r_prev[1 - i].M > 0 && targetLum_prev > 0)
            {
                float J_temporal_to_temporal = JacobianReconnectionShift(r_prev[i].normal,
                    candidate[1 - i].posW, candidate[i].posW, r_prev[i].pos);
                float targetLum_other = TargetLumAtTemporalPixel(r_prev[i], candidate[1 - i], albedo, true);
                denom += r_prev[1 - i].M * targetLum_other / max(J_temporal_to_temporal, 1e-6f);
            }

            denom = J_temporal_to_curr == 0 ? 0 : denom;
            float m_prev = denom == 0 ? 0 : numerator / max(denom, 1e-6f);
            float w_prev = m_prev * targetLum_curr * r_prev[i].W;

            // Reservoir stream (ZetaRay Reservoir::Update)
            r.w_sum += w_prev;
            r.M += 1.0f;
            if (rng < w_prev / max(1e-6f, r.w_sum))
            {
                r.pos = r_prev[i].pos;
                r.normal = r_prev[i].normal;
                r.ID = r_prev[i].ID;
                r.Lo = r_prev[i].Lo;
                r.targetZ = target_curr;
            }
        }
    }

    float targetLum = max(Luminance(r.targetZ), 0.0f);
    r.W = targetLum > 0.0f ? r.w_sum / targetLum : 0.0f;
    r.M = M_new;
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
    float  roughness = gMaterial.Load(int3(gb, 0)).a;

    FReservoir r = LoadReservoir(pixel, gCurrReservoir0, gCurrReservoir1, gCurrReservoir2);
    if (r.ID == 0xFFFFFFFFu || r.M == 0)
    {
        // No valid current sample — still write an invalid reservoir (the
        // spatial pass handles it; display adds DirectTexture separately).
        StoreReservoir(pixel, r);
        gOutRadiance[pixel] = float4(0.0f, 0.0f, 0.0f, 0.0f);
        return;
    }

    // Current sample's target at this pixel: Lo * f(pos -> x2)
    {
        float3 wi = normalize(r.pos - worldPos);
        r.targetZ = r.Lo * EvalF(albedo, normal, wi);
    }

    // ---- Reproject to the previous frame (matrix + turntable yaw) ----
    float2 uv = (float2(pixel) + 0.5f) * gConstants.RcpOutputSize;
    float2 ndc = float2(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f);

    float nearP = gConstants.NearPlane;
    float farP = gConstants.FarPlane;
    float ndcZ = 0.0f;
    if (viewZ > 1e-6f)
        ndcZ = (farP + nearP) / (farP - nearP) - (2.0f * farP * nearP) / ((farP - nearP) * viewZ);

    float4 worldClip = mul(gConstants.InverseCurrViewProj, float4(ndc.x, ndc.y, ndcZ, 1.0f));
    worldClip.xyz /= worldClip.w;

    float yawDelta = radians(gConstants.PrevSceneYaw - gConstants.SceneYaw);
    float yawCos = cos(yawDelta);
    float yawSin = sin(yawDelta);
    float3 objPrevPos = float3(
        yawCos * worldClip.x + yawSin * worldClip.z,
        worldClip.y,
        -yawSin * worldClip.x + yawCos * worldClip.z);

    float4 prevClip = mul(gConstants.PrevViewProj, float4(objPrevPos, 1.0f));
    prevClip.xyz /= prevClip.w;
    float2 prevUV = float2(prevClip.x * 0.5f + 0.5f, -prevClip.y * 0.5f + 0.5f);

    FCandidate candidate[2];
    FindTemporalCandidates(pixel, worldPos, normal, viewZ, roughness, prevUV, candidate);

    FReservoir r_prev[2];
    r_prev[0] = LoadReservoir(candidate[0].posSS, gHistReservoir0, gHistReservoir1, gHistReservoir2);
    r_prev[1] = LoadReservoir(candidate[1].posSS, gHistReservoir0, gHistReservoir1, gHistReservoir2);
    // History positions were stored in the PREVIOUS frame's world space; the
    // turntable rotates the scene, so bring them into current-frame coords.
    r_prev[0].pos = RotatePrevToCurr(r_prev[0].pos);
    r_prev[1].pos = RotatePrevToCurr(r_prev[1].pos);

    float M_new = r.M;
    [unroll]
    for (int k = 0; k < 2; k++)
    {
        if (candidate[k].valid)
            M_new += r_prev[k].M;
    }

    float rng = Hash01(uint2(pixel), uint(gConstants.FrameIndex));

    if (candidate[1].valid && roughness > 0.05f)
    {
        TemporalResample2(r, worldPos, normal, albedo, candidate, r_prev, rng, M_new);
    }
    else if (candidate[0].valid)
    {
        // TemporalResample1 path: single candidate. Port of ZetaRay's
        // TemporalResample1 (m_curr scaling + one m_prev stream).
        float p_curr = max(Luminance(r.targetZ), 0.0f);
        if (r.w_sum != 0)
        {
            float targetLum_prev = 0.0f;
            if (r_prev[0].M > 0 && Luminance(r.Lo) > 1e-6f)
                targetLum_prev = TargetLumAtTemporalPixel(r, candidate[0], albedo, true);

            float J_curr_to_temporal = JacobianReconnectionShift(r.normal, candidate[0].posW, worldPos, r.pos);
            float m_curr = p_curr / max(p_curr + r_prev[0].M * targetLum_prev * J_curr_to_temporal, 1e-6f);
            r.w_sum *= m_curr;
        }

        if (r_prev[0].ID == 0xFFFFFFFFu || dot(r_prev[0].Lo, 1.0f) == 0)
        {
            float targetLum = max(Luminance(r.targetZ), 0.0f);
            r.W = targetLum > 0.0f ? r.w_sum / targetLum : 0.0f;
            r.M = M_new;
        }
        else
        {
            float3 wi = r_prev[0].pos - worldPos;
            float t = length(wi);
            wi = t > 1e-6f ? wi / t : float3(0.0f, 1.0f, 0.0f);
            float3 target_curr = r_prev[0].Lo * EvalF(albedo, normal, wi);
            float targetLum_curr = Luminance(target_curr);
            if (targetLum_curr > 1e-6f && VisibilitySegment(worldPos, wi, t, normal, r_prev[0].ID, g_bvh))
            {
                float targetLum_prev = r_prev[0].W > 0 ? r_prev[0].w_sum / r_prev[0].W : 0;
                float J_temporal_to_curr = JacobianReconnectionShift(r_prev[0].normal, worldPos,
                    candidate[0].posW, r_prev[0].pos);
                float numerator = r_prev[0].M * targetLum_prev;
                float denom = numerator / max(J_temporal_to_curr, 1e-6f) + targetLum_curr;
                float m_prev = numerator / max(denom, 1e-6f);
                float w_prev = m_prev * targetLum_curr * r_prev[0].W;

                r.w_sum += w_prev;
                r.M += 1.0f;
                if (rng < w_prev / max(1e-6f, r.w_sum))
                {
                    r.pos = r_prev[0].pos;
                    r.normal = r_prev[0].normal;
                    r.ID = r_prev[0].ID;
                    r.Lo = r_prev[0].Lo;
                    r.targetZ = target_curr;
                }
            }
            float targetLum = max(Luminance(r.targetZ), 0.0f);
            r.W = targetLum > 0.0f ? r.w_sum / targetLum : 0.0f;
            r.M = M_new;
        }
    }
    else
    {
        // No temporal candidate: keep the current sample, W = w_sum / targetLum.
        float targetLum = max(Luminance(r.targetZ), 0.0f);
        r.W = targetLum > 0.0f ? r.w_sum / targetLum : 0.0f;
        r.M = M_new;
    }

    r.M = min(r.M, gConstants.MaxM);
    // v211 (Phase 4): ZetaRay SuppressOutlierReservoirs — a reservoir whose
    // w_sum dwarfs its wave's average is likely a firefly; cap its M.
    {
        float waveSum = WaveActiveSum(r.w_sum);
        float waveAvg = (waveSum - r.w_sum) / max(float(WaveGetLaneCount()) - 1.0f, 1e-6f);
        if (r.w_sum > 25.0f * waveAvg)
            r.M = 1.0f;
    }
    StoreReservoir(pixel, r);
    // Final indirect estimate at this pixel: target_z * W.
    gOutRadiance[pixel] = float4(r.targetZ * r.W, 1.0f);
}
