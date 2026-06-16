// GIPathTracing.hlsl - Few-bounce GI ray-tracing shader library.
//
// Task 1.3 of ReSTIR/GI separation sprint-1: stub phase. Returns black for all
// paths so the pipeline can dispatch without crashing. Real multi-bounce
// path tracing lands in Task 1.5 (shader bodies migrated from
// TestFewBounceGI_Data/FewBounceGI.hlsl) and Task 1.6 (loop + RR + EvalBRDF).
//
// Entry points (all compiled into one RT library):
//   - RayGen       [shader("raygeneration")]
//   - ClosestHit   [shader("closesthit")]
//   - Miss         [shader("miss")]
//
// Resources (bound via FGIPass binding layout, see Task 1.4):
//   b0  FViewConstants
//   t0  SceneBVH
//   t1  GBufferWorldPos
//   t2  GBufferNormal
//   t3  GBufferMaterial
//   u0  OutputTexture (radiance)
//   u1  DebugStatsTexture (optional)
//   s2  LinearSampler

struct GIPayload
{
    float3 throughput;
    float3 radiance;
    float3 origin;
    float3 direction;
    float  hitDistance;
    uint   bounceCount;
    uint   flags;
};

struct FViewConstants
{
    float4x4 ModelMatrix;
    float4x4 ViewMatrix;
    float4x4 ProjMatrix;
    float2   RenderTargetSize;
    float2   Padding;
};

struct Attributes
{
    float2 barycentrics;
};

[shader("raygeneration")]
void RayGen()
{
    // Real implementation lands in Task 1.5 + 1.6. For now, output is black
    // and the binding layout can be exercised without crashes.
    // We intentionally do NOT write to Output[u] here -- that requires u0
    // being bound. Task 1.4 wires u0; until then DispatchRays is a no-op.
}

[shader("closesthit")]
void ClosestHit(inout GIPayload /*payload*/, in Attributes /*attr*/)
{
    // Real implementation lands in Task 1.5 (migrated from FewBounceGI.hlsl).
}

[shader("miss")]
void Miss(inout GIPayload /*payload*/)
{
    // Real implementation lands in Task 1.5 (migrated from FewBounceGI.hlsl).
}