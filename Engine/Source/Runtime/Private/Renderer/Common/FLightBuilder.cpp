// Copyright 2026 HLVM Engine
//
// MIT License

#include "Renderer/Common/FLightBuilder.h"

#include <cmath>
#include <cstring>

namespace Renderer
{
    namespace
    {
        void CopyFloat3(float* Dest, const float* Src)
        {
            Dest[0] = Src[0];
            Dest[1] = Src[1];
            Dest[2] = Src[2];
        }

        FLight MakeBaseLight(const float* Color, float Intensity)
        {
            FLight Light{};
            CopyFloat3(Light.color, Color);
            Light.intensity = Intensity;
            Light.innerConeAngle = 0.0f;
            Light.outerConeAngle = 0.0f;
            Light.areaWidth = 0.0f;
            Light.areaHeight = 0.0f;
            Light.flags = kLightFlag_Visible;
            Light.shadowMapIndex = kNoShadowMap;
            return Light;
        }
    } // namespace

    FLight MakeDirectionalLight(const float* Direction,
                                const float* Color,
                                float        Intensity)
    {
        FLight Light = MakeBaseLight(Color, Intensity);
        CopyFloat3(Light.direction, Direction);
        Light.position[0] = 0.0f;
        Light.position[1] = 0.0f;
        Light.position[2] = 0.0f;
        Light.range = 1e20f;
        Light.type = static_cast<uint32_t>(ELightType::Directional);
        return Light;
    }

    FLight MakePointLight(const float* Position,
                          float        Range,
                          const float* Color,
                          float        Intensity)
    {
        FLight Light = MakeBaseLight(Color, Intensity);
        CopyFloat3(Light.position, Position);
        Light.range = Range;
        Light.direction[0] = 0.0f;
        Light.direction[1] = -1.0f;
        Light.direction[2] = 0.0f;
        Light.type = static_cast<uint32_t>(ELightType::Point);
        return Light;
    }

    FLight MakeSpotLight(const float* Position,
                         const float* Direction,
                         float        Range,
                         float        InnerConeAngle,
                         float        OuterConeAngle,
                         const float* Color,
                         float        Intensity)
    {
        FLight Light = MakeBaseLight(Color, Intensity);
        CopyFloat3(Light.position, Position);
        CopyFloat3(Light.direction, Direction);
        Light.range = Range;
        Light.innerConeAngle = InnerConeAngle;
        Light.outerConeAngle = OuterConeAngle;
        Light.type = static_cast<uint32_t>(ELightType::Spot);
        return Light;
    }

    FLight MakeAreaLight(const float* Position,
                         const float* Direction,
                         float        Width,
                         float        Height,
                         const float* Color,
                         float        Intensity)
    {
        FLight Light = MakeBaseLight(Color, Intensity);
        CopyFloat3(Light.position, Position);
        CopyFloat3(Light.direction, Direction);
        Light.range = 1e20f;
        Light.areaWidth = Width;
        Light.areaHeight = Height;
        Light.type = static_cast<uint32_t>(ELightType::Area);
        return Light;
    }

    nvrhi::BufferHandle UploadLightBuffer(nvrhi::IDevice* Device,
                                          const FLight*   Lights,
                                          size_t          Count)
    {
        if (!Device || Count == 0)
        {
            return nullptr;
        }

        const size_t ByteSize = Count * sizeof(FLight);
        nvrhi::BufferDesc Desc;
        Desc.byteSize = ByteSize;
        Desc.structStride = sizeof(FLight);
        Desc.initialState = nvrhi::ResourceStates::ShaderResource;
        Desc.keepInitialState = true;
        Desc.debugName = "FLightBuilder.Lights";

        nvrhi::BufferHandle Buffer = Device->createBuffer(Desc);
        if (!Buffer)
        {
            return nullptr;
        }

        nvrhi::CommandListHandle Cmd = Device->createCommandList();
        Cmd->open();
        Cmd->writeBuffer(Buffer, Lights, ByteSize);
        Cmd->close();
        Device->executeCommandList(Cmd);
        Device->waitForIdle();

        return Buffer;
    }
} // namespace Renderer
