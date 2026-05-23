// Copyright 2026 HLVM Engine
//
// MIT License

#include "Renderer/Shader/ShaderHotReloader.h"
#include "Renderer/Shader/ShaderLibrary.h"
#include "Core/Log.h"

DECLARE_LOG_CATEGORY(LogShaderHotReload)

FShaderHotReloader& FShaderHotReloader::Get()
{
    static FShaderHotReloader Instance;
    return Instance;
}

void FShaderHotReloader::Register(IShaderReloadable* Reloadable)
{
    if (Reloadable)
    {
        Reloadables.insert(Reloadable);
    }
}

void FShaderHotReloader::Unregister(IShaderReloadable* Reloadable)
{
    if (Reloadable)
    {
        Reloadables.erase(Reloadable);
    }
}

void FShaderHotReloader::Update()
{
    if (!PollTimer.Check(true))
    {
        return;
    }

    TVector<FPath> ChangedPaths = FShaderLibrary::Get().PollAllCachedFiles();
    if (!ChangedPaths.empty())
    {
        HLVM_LOG(LogShaderHotReload, info, TXT("ShaderHotReloader: Detected {} changed shader file(s)"), ChangedPaths.size());
        for (const auto& Path : ChangedPaths)
        {
            HLVM_LOG(LogShaderHotReload, info, TXT("  - {}"), Path.ToTCharCStr());
        }
        bPendingReload = true;
    }

    if (bPendingReload)
    {
        ReloadAll();
        bPendingReload = false;
    }
}

void FShaderHotReloader::ReloadAll()
{
    HLVM_LOG(LogShaderHotReload, info, TXT("ShaderHotReloader: Reloading shaders for {} object(s)"), Reloadables.size());

    for (IShaderReloadable* Reloadable : Reloadables)
    {
        if (Reloadable)
        {
            try
            {
                Reloadable->ReloadShaders();
            }
            catch (const std::exception& e)
            {
                HLVM_LOG(LogShaderHotReload, err, TXT("ShaderHotReloader: Reload failed: {}"), TO_TCHAR_CSTR(e.what()));
            }
        }
    }
}
