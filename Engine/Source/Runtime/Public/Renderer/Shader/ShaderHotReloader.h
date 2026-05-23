// Copyright 2026 HLVM Engine
//
// MIT License

#pragma once

#include "Core/String.h"
#include "Utility/Timer.h"

#include <set>

/**
 * @brief Interface for objects that can reload their shaders
 */
class IShaderReloadable
{
public:
    virtual ~IShaderReloadable() = default;

    /**
     * @brief Reload all shader-dependent resources
     *
     * Implementation should call Shutdown() + Initialize() or equivalent
     * to recreate shaders, binding layouts, and pipelines from disk.
     */
    virtual void ReloadShaders() = 0;
};

/**
 * @brief Shader hot-reload orchestrator
 *
 * Polls FShaderLibrary cached files for modifications and triggers
 * ReloadShaders() on all registered reloadable objects.
 *
 * Usage:
 *   FShaderHotReloader::Get().Register(this);
 *   // In main loop:
 *   FShaderHotReloader::Get().Update();
 */
class FShaderHotReloader
{
public:
    static FShaderHotReloader& Get();

    void Register(IShaderReloadable* Reloadable);
    void Unregister(IShaderReloadable* Reloadable);

    /**
     * @brief Poll for shader file changes and trigger reloads
     *
     * Checks cached shader files every second (configurable via timer).
     * If any changes detected, calls ReloadShaders() on all registered objects.
     */
    void Update();

    /**
     * @brief Manually trigger reload on all registered objects
     */
    void ReloadAll();

    /**
     * @brief Check if a reload is pending (change detected but not yet processed)
     */
    bool IsReloadPending() const { return bPendingReload; }

private:
    FShaderHotReloader() = default;

    std::set<IShaderReloadable*> Reloadables;
    FTimer PollTimer{ std::chrono::seconds{ 1 }, true };
    bool bPendingReload = false;
};
