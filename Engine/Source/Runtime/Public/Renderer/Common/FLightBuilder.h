// Copyright 2026 HLVM Engine
//
// MIT License

#pragma once

#include "Renderer/Common/FLight.h"

#include <nvrhi/nvrhi.h>

#include <vector>

namespace Renderer
{
    // Factory helpers for constructing Renderer::FLight values.
    // These fill the 80-byte std430 struct used by FGIPass and FDeferredLightingPass.

    FLight MakeDirectionalLight(const float* Direction,
                                const float* Color,
                                float        Intensity);

    FLight MakePointLight(const float* Position,
                          float        Range,
                          const float* Color,
                          float        Intensity);

    FLight MakeSpotLight(const float* Position,
                         const float* Direction,
                         float        Range,
                         float        InnerConeAngle,
                         float        OuterConeAngle,
                         const float* Color,
                         float        Intensity);

    FLight MakeAreaLight(const float* Position,
                         const float* Direction,
                         float        Width,
                         float        Height,
                         const float* Color,
                         float        Intensity);

    // Upload a contiguous array of FLight values to a GPU structured buffer.
    // Returns a null handle if the input is empty or buffer creation fails.
    nvrhi::BufferHandle UploadLightBuffer(nvrhi::IDevice* Device,
                                          const FLight*   Lights,
                                          size_t          Count);

    inline nvrhi::BufferHandle UploadLightBuffer(nvrhi::IDevice*       Device,
                                                 const std::vector<FLight>& Lights)
    {
        return UploadLightBuffer(Device, Lights.data(), Lights.size());
    }
} // namespace Renderer
