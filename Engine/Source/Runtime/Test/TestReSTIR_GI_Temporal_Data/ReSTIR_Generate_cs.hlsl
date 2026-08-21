// ReSTIR GI — Candidate Packaging (RIS) [ZetaRay ground truth, Phase 1]
//
// The FGIPass ray-gen produced this pixel's ReSTIR candidate:
//   - second path vertex x2 (position u2, normal + pdf u3, ID u2.w)
//   - Lo = incident radiance at x2 towards the primary surface (u0.rgb)
//   - primary BSDF sample direction = cosine-weighted, pdf = cos/pi
// This pass packages it into the 3-texture reservoir (ZetaRay layout):
//   R0 = float4(x2Pos, asfloat(x2ID))
//   R1 = float4(Lo, M)                       M = 1 (one candidate)
//   R2 = float4(w_sum, W, OctEncode(x2Normal))
// with (Lambert BRDF, f includes |cos| per ZetaRay BSDF::Unified):
//   f      = albedo * max(NdotL, 0) / PI
//   target = Lo * f
//   w_sum  = targetLum / pdf
//   W      = 1 / pdf                         (single-candidate RIS)
// Primary-ray misses have no x2: the reservoir is INVALID (ID = 0xFFFFFFFF,
// M = 0, W = 0); the visible sky/direct lighting lives in DirectTexture (u4)
// and is recombined at display time.

struct FReSTIRConstants
{
    float2 OutputSize;
    float2 RcpOutputSize;
    float FrameIndex;
    float NumCandidates;
    float DepthThreshold;
    float NormalThreshold;
    float DebugVis;
    float GBufferScale;  // v210: full-res GBuffer width / half-res dispatch width
    float Pad0;
    float Pad1;
};

cbuffer Constants : register(b0)
{
    FReSTIRConstants gConstants;
}

Texture2D<float4> gRadiance   : register(t0);  // Lo + firstHitT (FGIPass u0)
Texture2D<float4> gWorldPos   : register(t1);  // primary surface (full-res)
Texture2D<float4> gNormals    : register(t2);  // primary normal (full-res)
Texture2D<float>  gDepth      : register(t3);  // primary depth (full-res)
Texture2D<float4> gSample     : register(t4);  // x2Pos.xyz + asfloat(x2ID) (FGIPass u2)
Texture2D<float4> gSampleInfo : register(t5);  // x2Normal.xyz + samplePdf (FGIPass u3)
Texture2D<float4> gMaterial   : register(t6);  // albedo.rgb + roughness.a (full-res)

RWTexture2D<float4> gReservoir0 : register(u0, space1);
RWTexture2D<float4> gReservoir1 : register(u1, space1);
RWTexture2D<float4> gReservoir2 : register(u2, space1);

float Luminance(float3 c)
{
    return dot(c, float3(0.2126, 0.7152, 0.0722));
}

float2 OctEncode(float3 n)
{
    n /= (abs(n.x) + abs(n.y) + abs(n.z));
    if (n.z < 0.0)
        n.xy = (1.0 - abs(n.yx)) * (n.xy >= 0.0 ? 1.0 : -1.0);
    return n.xy * 0.5 + 0.5;
}

// Half-res dispatch pixel -> full-res GBuffer texel (see ReSTIR_Temporal_cs.hlsl).
int2 GB(int2 p)
{
    int s = max(int(gConstants.GBufferScale), 1);
    return p * s + (s >> 1);
}

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    float2 outputSize = gConstants.OutputSize;
    if (dispatchThreadID.x >= (uint)outputSize.x || dispatchThreadID.y >= (uint)outputSize.y)
        return;

    int2 pixel = int2(dispatchThreadID.xy);
    int2 gb = GB(pixel);

    float4 rad  = gRadiance.Load(int3(pixel, 0));
    float4 smp  = gSample.Load(int3(pixel, 0));
    float4 info = gSampleInfo.Load(int3(pixel, 0));

    float3 x2Pos = smp.xyz;
    uint   x2ID  = asuint(smp.w);
    float3 x2Normal = info.xyz;
    float  pdf   = info.w;
    float3 lo    = rad.rgb;
    float  hitT  = rad.a;

    bool valid = (x2ID != 0xFFFFFFFFu) && (hitT > 0.0f);

    float4 out0 = float4(0.0f, 0.0f, 0.0f, asfloat(0xFFFFFFFFu));
    float4 out1 = float4(0.0f, 0.0f, 0.0f, 0.0f);
    float4 out2 = float4(0.0f, 0.0f, 0.0f, 0.0f);

    if (valid)
    {
        float3 worldPos = gWorldPos.Load(int3(gb, 0)).rgb;
        float3 normal   = normalize(gNormals.Load(int3(gb, 0)).rgb * 2.0f - 1.0f);
        float3 albedo   = max(gMaterial.Load(int3(gb, 0)).rgb, 0.0f);

        float3 wi = normalize(x2Pos - worldPos);
        float NdotL = max(dot(normal, wi), 0.0f);
        float3 f = albedo * (NdotL * (1.0f / 3.14159265f));
        float3 target = lo * f;
        float targetLum = max(Luminance(target), 0.0f);

        float w = targetLum / max(pdf, 1e-6f);
        float W = targetLum > 0.0f ? 1.0f / max(pdf, 1e-6f) : 0.0f;

        out0 = float4(x2Pos, asfloat(x2ID));
        out1 = float4(lo, 1.0f);
        out2 = float4(w, W, OctEncode(x2Normal));
    }

    gReservoir0[pixel] = out0;
    gReservoir1[pixel] = out1;
    gReservoir2[pixel] = out2;
}
