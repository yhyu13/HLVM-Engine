// FGIPass.cpp - Few-bounce / path-tracing GI ray-tracing pass implementation.
//
// Task 1.2 of the ReSTIR/GI separation sprint-1 plan: stub implementation that
// registers CVars and provides a no-op Dispatch. Real pipeline wiring lands in
// Task 1.4 (after the shader stubs in Task 1.3 land).

#include "Renderer/GI/FGIPass.h"

#include "Core/String.h"
#include "Utility/CVar/CVarMacros.h"

#include <nvrhi/nvrhi.h>

namespace GI
{
    // CVars - read at DispatchRays time so runtime tuning works without re-init.
    AUTO_CVAR_INT  (r_GI_MaxBounces,      4,    "Maximum number of GI bounces (0 = direct only)", EConsoleVariableFlag::Saved)
    AUTO_CVAR_INT  (r_GI_SamplesPerPixel, 8,    "Samples per pixel for indirect lighting",      EConsoleVariableFlag::Saved)
    AUTO_CVAR_FLOAT(r_GI_MinRayLength,    0.001f, "GI bounce ray TMin (avoid self-intersection)", EConsoleVariableFlag::Saved)
    AUTO_CVAR_BOOL (r_GI_EnableRR,        true, "Enable Russian Roulette path termination",     EConsoleVariableFlag::Saved)
    AUTO_CVAR_FLOAT(r_GI_RussianRoulette, 0.95f, "Russian Roulette survival threshold",          EConsoleVariableFlag::Saved)
    AUTO_CVAR_BOOL (r_GI_DebugBounceStats, false, "Write per-frame bounce stats to u1 UAV (gates DebugStatsTexture binding)", EConsoleVariableFlag::Console)

    bool FGIPass::Initialize(nvrhi::IDevice* InDevice,
                             const FString& InShaderDataDir,
                             const FScene* InScene)
    {
        if (bIsInitialized)
            return true;

        if (!InDevice)
            return false;

        Device        = InDevice;
        ShaderDataDir = InShaderDataDir;
        Scene         = InScene;

        // Real pipeline construction is Task 1.4 (depends on shader stubs from Task 1.3).
        bIsInitialized = true;
        return true;
    }

    void FGIPass::DispatchRays(nvrhi::ICommandList* /*CmdList*/, const FGIPassDesc& /*Desc*/)
    {
        // No-op until Task 1.4 lands. OutputTexture stays null; test should not call
        // DispatchRays until the pipeline is wired.
    }

    void FGIPass::Shutdown()
    {
        ShaderLibrary  = nullptr;
        RTPipeline     = nullptr;
        ShaderTable    = nullptr;
        BindingLayout  = nullptr;
        ConstantBuffer = nullptr;
        OutputTexture  = nullptr;
        Device         = nullptr;
        Scene          = nullptr;
        bIsInitialized = false;
    }

    // Private helpers - stubbed in Task 1.2, real implementations land in Task 1.4.
    bool FGIPass::LoadShaders()         { return false; }
    bool FGIPass::CreatePipeline()      { return false; }
    bool FGIPass::CreateBindingLayout() { return false; }
    bool FGIPass::CreateConstantBuffer(){ return false; }
    void FGIPass::WriteConstants(nvrhi::ICommandList* /*CmdList*/, const FGIPassDesc& /*Desc*/) {}
} // namespace GI
