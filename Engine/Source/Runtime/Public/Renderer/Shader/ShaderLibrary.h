// Copyright 2026 HLVM Engine
//
// MIT License

#pragma once

#include "Core/String.h"
#include "Core/Parallel/Lock.h"
#include "Platform/FileSystem/Path.h"

#include <nvrhi/nvrhi.h>

#include <ctime>

/**
 * @brief Centralized shader blob cache and loader
 *
 * FShaderLibrary provides:
 * - Blob deduplication: same .sblob file read once, cached by absolute path
 * - Shader creation: extracts SPIR-V via ShaderMake, creates NVRHI shader
 * - File modification tracking: poll-based hot-reload support
 *
 * Thread-safe for concurrent reads. All shader loads happen on main thread
 * in practice; lock protects against future async usage.
 */
class FShaderLibrary : private FAtomicFlagNC
{
public:
    static FShaderLibrary& Get();

    /**
     * @brief Load a shader from a .sblob file
     *
     * Reads blob from disk (or cache), extracts SPIR-V, creates NVRHI shader.
     * Does NOT cache the NVRHI handle (device-lifetime sensitive).
     * Caches the blob bytes for deduplication.
     *
     * @param Device NVRHI device
     * @param BlobPath Absolute path to .sblob file
     * @param Type Shader type (Vertex, Pixel, Compute, etc.)
     * @return Valid shader handle on success, null on failure
     */
    nvrhi::ShaderHandle LoadShader(nvrhi::IDevice* Device, const FPath& BlobPath, nvrhi::ShaderType Type);

    /**
     * @brief Convenience overload combining directory and filename
     */
    nvrhi::ShaderHandle LoadShader(nvrhi::IDevice* Device, const FString& ShaderDataDir, const FString& FileName, nvrhi::ShaderType Type);

    /**
     * @brief Poll a single tracked file for modification
     * @return true if file changed since last load/poll
     */
    bool PollFile(const FPath& BlobPath);

    /**
     * @brief Poll all cached files for modifications
     * @return List of paths that have changed
     */
    TVector<FPath> PollAllCachedFiles();

    /**
     * @brief Force invalidate cached blob for a path
     */
    void Invalidate(const FPath& BlobPath);

    /**
     * @brief Clear all cached blobs
     */
    void Clear();

    /**
     * @brief Get number of cached entries
     */
    uint32_t GetCacheSize() const;

private:
    struct FEntry
    {
        TVector<uint8_t> BlobData;
        std::time_t LastWriteTime = 0;
    };

    bool ReadBlobFromDisk(const FPath& BlobPath, FEntry& OutEntry);

    mutable TMap<FPath, FEntry> Cache;
};
