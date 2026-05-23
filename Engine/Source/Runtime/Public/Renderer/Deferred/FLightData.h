#pragma once

#include <glm/glm.hpp>
#include <cstdint>

/**
 * @brief Light data for deferred shading
 *
 * Supports directional, point, and spot lights.
 * For Week 4, only directional lights are fully wired through the pipeline.
 */
struct FLightData
{
    enum class EType : uint32_t
    {
        Directional = 0,
        Point,
        Spot
    };

    EType Type = EType::Directional;
    glm::vec3 Direction = glm::vec3(0.577f, 0.577f, 0.577f);
    float Intensity = 0.8f;
    glm::vec3 Color = glm::vec3(1.0f, 1.0f, 1.0f);
    float Pad0 = 0.0f;
    glm::vec3 Position = glm::vec3(0.0f);
    float Radius = 100.0f;
    float InnerConeAngle = 0.0f;
    float OuterConeAngle = 0.0f;
    float Pad1[2] = {};
};
