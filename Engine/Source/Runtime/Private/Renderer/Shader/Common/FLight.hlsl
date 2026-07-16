// FLight.hlsl - HLSL mirror of Renderer::FLight (80 bytes, std430).
//
// Layout MUST match Engine/Source/Runtime/Public/Renderer/Common/FLight.h.
// Five 16-byte blocks; uniform HLSL packing gives std430-equivalent layout
// for this struct (each block is 4 floats).

#ifndef FLIGHT_HLSL
#define FLIGHT_HLSL

enum ELightTypeHLSL : uint
{
    Point       = 0,
    Spot        = 1,
    Directional = 2,
    Area        = 3,
};

// 5 x float4 = 80 bytes. std430-compatible.
struct FLight
{
    // Block 1
    float3 position;
    float  range;

    // Block 2
    float3 direction;
    float  intensity;

    // Block 3
    float3 color;
    uint   type;   // ELightTypeHLSL

    // Block 4
    float innerConeAngle;
    float outerConeAngle;
    float areaWidth;
    float areaHeight;

    // Block 5
    uint  flags;
    uint  shadowMapIndex;
    float padding0;
    float padding1;
};

// Sentinel for "no shadow map"
static const uint kNoShadowMap = 0xFFFFFFFFu;

// Flag bits
static const uint kLightFlag_CastShadow = 1u << 0;
static const uint kLightFlag_Visible    = 1u << 1;

#endif // FLIGHT_HLSL
