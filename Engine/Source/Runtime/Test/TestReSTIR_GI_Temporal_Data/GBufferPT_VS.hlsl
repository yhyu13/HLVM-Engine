/*
 * GBufferPT_VS.hlsl — Path-trace GBuffer vertex shader for Sponza.
 *
 * Companion to GBufferPT_PS.hlsl. Together they implement the GBuffer pass
 * that populates GBufferWorldPos / GBufferNormal / GBufferMaterial as a
 * 3-MRT render-target output readable by FGIPass (GIPathTracing.hlsl).
 *
 * Why a separate VS/PS pair (not the existing GBufferSponzaVS/PS):
 *   The existing GBufferSponzaVS expects POSITION/NORMAL/TEXCOORD0/TANGENT
 *   inputs (44-byte FVertex layout from IMesh.h) and the existing PS writes
 *   5 MRTs with a DiffuseTexture t0 sample. The path-trace GBuffer needs:
 *     - 3 MRTs only (worldPos, normal, material)
 *     - Material color from per-instance FInstanceInfo.AlbedoColor
 *       (no texture bind — this test uses a fallback material color
 *        because Sponza's .ktx texture load is a separate card)
 *
 * Vertex stream layout (matches TestReSTIR_GI_Temporal.cpp FVertex, but the
 * shader only consumes POSITION/NORMAL; the input layout still strides the
 * full sizeof(FVertex)=64 so the offsets 0/16 stay correct):
 *   POSITION  : float3 (offset 0)
 *   NORMAL    : float3 (offset 16)
 *
 * Bindings (per-draw CB approach; FInstanceInfo is bound as a single
 * constant buffer b1 by the C++ driver so each mesh-draw sees only its
 * own FInstanceInfo):
 *   b0  ViewConstants (Model/View/Proj + CameraPos)
 *   b1  PerInstanceInfo (one FInstanceInfo, 48 bytes)
 */

// =============================================================================
// Constant Buffer — ViewConstants
// =============================================================================

cbuffer ViewConstants : register(b0) {
    float4x4 ModelMatrix;
    float4x4 ViewMatrix;
    float4x4 ProjMatrix;
    float4   CameraPos;       // .xyz = position, .w unused
};

// =============================================================================
// Constant Buffer — PerInstanceInfo (one FInstanceInfo per draw)
// Must match C++ FInstanceInfo (48 bytes) in TestReSTIR_GI_Temporal.cpp.
// =============================================================================

cbuffer PerInstanceInfo : register(b1) {
    uint   VertexOffset;
    uint   IndexOffset;
    uint   VertexCount;
    uint   IndexCount;
    float3 AlbedoColor;
    uint   AlbedoTextureIndex;
    uint   MaterialFlags;
    float  Roughness;   // gltf roughnessFactor (2026-08-10 Phase 2)
    float  Metallic;
    uint   Pad;
};

// =============================================================================
// Vertex Input
// =============================================================================

struct VSInput {
    float3 Position : POSITION;
    float3 Normal   : NORMAL;
    float2 UV       : TEXCOORD0;
};

// =============================================================================
// Vertex Output / Pixel Input
// =============================================================================

struct PSInput {
    float4 Position  : SV_POSITION;
    float3 WorldPos  : TEXCOORD0;
    float3 Normal    : TEXCOORD1;
    float3 Albedo    : TEXCOORD2;
    float  ViewDepth : TEXCOORD3;   // positive view-space depth (-viewPos.z)
    float2 UV        : TEXCOORD4;   // material albedo UV (2026-08-10 Phase 1)
};

PSInput main(VSInput input) {
    PSInput output;

    // World-space position — note Model is identity in this test (the
    // per-instance transform is baked into TLAS at 0.01 scale).
    float4 worldPos = mul(ModelMatrix, float4(input.Position, 1.0));
    output.WorldPos = worldPos.xyz;

    float4 viewPos = mul(ViewMatrix, worldPos);
    output.Position = mul(ProjMatrix, viewPos);

    // World-space normal — Model is identity so just normalize.
    output.Normal = normalize(mul((float3x3)ModelMatrix, input.Normal));

    // Positive view-space depth for the ReSTIR temporal/spatial passes.
    // Camera looks down -Z, so -viewPos.z is positive in front of the camera.
    output.ViewDepth = -viewPos.z;

    // Per-instance material color — used by PS to populate MRT2 (material).
    output.Albedo = AlbedoColor;
    output.UV     = input.UV;

    return output;
}
