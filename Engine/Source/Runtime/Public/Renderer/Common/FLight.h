// FLight.h - 80-byte std430-compatible light struct shared by C++ and HLSL.
//
// Used by FGIPass (Sprint 1 of ReSTIR/GI separation plan) and later by
// FDeferredLightingPass area-light integration. The struct layout MUST match
// the HLSL mirror in Engine/Source/Runtime/Private/Renderer/Shader/Common/FLight.hlsl.
//
// Field semantics:
//   position        - world-space position (Point, Spot, Area)
//   range           - maximum falloff distance (Point, Spot)
//   direction       - normalized world-space direction (Directional, Spot, Area forward)
//   intensity       - normalized light strength (drives lux/lumens later)
//   color           - linear-space RGB tint
//   type            - ELightType discriminator
//   innerConeAngle  - full-angle inner cone in radians (Spot)
//   outerConeAngle  - full-angle outer cone in radians (Spot)
//   areaWidth       - X extent in world units (Area)
//   areaHeight      - Y extent in world units (Area)
//   flags           - bit 0 = castShadow, bit 1 = visible, bit 2..31 reserved
//   shadowMapIndex  - index into shadow-map atlas (or UINT32_MAX if none)

#pragma once

#include <cstdint>

namespace Renderer
{
    enum class ELightType : uint32_t
    {
        Point       = 0,
        Spot        = 1,
        Directional = 2,
        Area        = 3,
    };

    struct FLight
    {
        // Block 1: 16 bytes - position + range
        float position[3];
        float range;

        // Block 2: 16 bytes - direction + intensity
        float direction[3];
        float intensity;

        // Block 3: 16 bytes - color + type
        float color[3];
        uint32_t type;   // ELightType

        // Block 4: 16 bytes - cone / area geometry
        float innerConeAngle;
        float outerConeAngle;
        float areaWidth;
        float areaHeight;

        // Block 5: 16 bytes - flags + shadow info
        uint32_t flags;
        uint32_t shadowMapIndex;
        float    padding[2];
    };

    static_assert(sizeof(FLight) == 80, "FLight must be exactly 80 bytes (5 x 16-byte blocks) for std430 alignment");

    // Sentinel value for "no shadow map"
    constexpr uint32_t kNoShadowMap = 0xFFFFFFFFu;

    // Flag bits
    constexpr uint32_t kLightFlag_CastShadow = 1u << 0;
    constexpr uint32_t kLightFlag_Visible    = 1u << 1;
} // namespace Renderer