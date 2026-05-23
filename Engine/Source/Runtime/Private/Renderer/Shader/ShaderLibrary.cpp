// Copyright 2026 HLVM Engine
//
// MIT License

#include "Renderer/Shader/ShaderLibrary.h"
#include "Renderer/ShaderMake/ShaderBlob.h"
#include "Core/Log.h"
#include "Platform/FileSystem/FileSystem.h"

#include <boost/filesystem.hpp>
#include <fstream>

DECLARE_LOG_CATEGORY(LogShaderLibrary)

FShaderLibrary& FShaderLibrary::Get()
{
    static FShaderLibrary Instance;
    return Instance;
}

bool FShaderLibrary::ReadBlobFromDisk(const FPath& BlobPath, FEntry& OutEntry)
{
    std::ifstream File(BlobPath.string(), std::ios::ate | std::ios::binary);
    if (!File.is_open())
    {
        HLVM_LOG(LogShaderLibrary, err, TXT("FShaderLibrary: Failed to open blob: {}"), BlobPath.ToTCharCStr());
        return false;
    }

    size_t FileSize = static_cast<size_t>(File.tellg());
    if (FileSize == 0)
    {
        HLVM_LOG(LogShaderLibrary, err, TXT("FShaderLibrary: Empty blob file: {}"), BlobPath.ToTCharCStr());
        return false;
    }

    OutEntry.BlobData.resize(FileSize);
    File.seekg(0);
    File.read(reinterpret_cast<char*>(OutEntry.BlobData.data()), static_cast<std::streamsize>(FileSize));
    File.close();

    try
    {
        OutEntry.LastWriteTime = boost::filesystem::last_write_time(BlobPath.string());
    }
    catch (const boost::filesystem::filesystem_error&)
    {
        OutEntry.LastWriteTime = 0;
    }

    return true;
}

nvrhi::ShaderHandle FShaderLibrary::LoadShader(nvrhi::IDevice* Device, const FPath& BlobPath, nvrhi::ShaderType Type)
{
    if (!Device)
    {
        HLVM_LOG(LogShaderLibrary, err, TXT("FShaderLibrary: Null device"));
        return nullptr;
    }

    FPath AbsolutePath = FPath::Absolute(BlobPath);

    FEntry Entry;
    {
        LOCK_GUARD_NC();
        auto It = Cache.find(AbsolutePath);
        if (It != Cache.end())
        {
            Entry = It->second;
        }
    }

    if (Entry.BlobData.empty())
    {
        if (!ReadBlobFromDisk(AbsolutePath, Entry))
        {
            return nullptr;
        }

        {
            LOCK_GUARD_NC();
            Cache[AbsolutePath] = Entry;
        }
    }

    const void* Binary = nullptr;
    size_t BinarySize = 0;
    if (!ShaderMake::FindPermutationInBlob(Entry.BlobData.data(), Entry.BlobData.size(), nullptr, 0, &Binary, &BinarySize))
    {
        HLVM_LOG(LogShaderLibrary, err, TXT("FShaderLibrary: Failed to extract SPIR-V from blob: {}"), AbsolutePath.ToTCharCStr());
        return nullptr;
    }

    nvrhi::ShaderDesc Desc;
    Desc.setShaderType(Type);
    nvrhi::ShaderHandle Shader = Device->createShader(Desc, Binary, BinarySize);
    if (!Shader)
    {
        HLVM_LOG(LogShaderLibrary, err, TXT("FShaderLibrary: Failed to create shader: {}"), AbsolutePath.ToTCharCStr());
        return nullptr;
    }

    return Shader;
}

nvrhi::ShaderHandle FShaderLibrary::LoadShader(nvrhi::IDevice* Device, const FString& ShaderDataDir, const FString& FileName, nvrhi::ShaderType Type)
{
    FPath BlobPath = FPath::Combine(ShaderDataDir, FileName);
    return LoadShader(Device, BlobPath, Type);
}

bool FShaderLibrary::PollFile(const FPath& BlobPath)
{
    FPath AbsolutePath = FPath::Absolute(BlobPath);

    LOCK_GUARD_NC();
    auto It = Cache.find(AbsolutePath);
    if (It == Cache.end())
    {
        return false;
    }

    try
    {
        std::time_t CurrentTime = boost::filesystem::last_write_time(AbsolutePath.string());
        if (CurrentTime != It->second.LastWriteTime)
        {
            It->second.LastWriteTime = CurrentTime;
            It->second.BlobData.clear();
            return true;
        }
    }
    catch (const boost::filesystem::filesystem_error&)
    {
        // File may have been deleted; leave cache as-is
    }

    return false;
}

TVector<FPath> FShaderLibrary::PollAllCachedFiles()
{
    TVector<FPath> ChangedPaths;

    LOCK_GUARD_NC();
    for (auto& Pair : Cache)
    {
        try
        {
            std::time_t CurrentTime = boost::filesystem::last_write_time(Pair.first.string());
            if (CurrentTime != Pair.second.LastWriteTime)
            {
                Pair.second.LastWriteTime = CurrentTime;
                Pair.second.BlobData.clear();
                ChangedPaths.push_back(Pair.first);
            }
        }
        catch (const boost::filesystem::filesystem_error&)
        {
            // Skip files that can't be accessed
        }
    }

    return ChangedPaths;
}

void FShaderLibrary::Invalidate(const FPath& BlobPath)
{
    FPath AbsolutePath = FPath::Absolute(BlobPath);

    LOCK_GUARD_NC();
    auto It = Cache.find(AbsolutePath);
    if (It != Cache.end())
    {
        It->second.BlobData.clear();
        It->second.LastWriteTime = 0;
    }
}

void FShaderLibrary::Clear()
{
    LOCK_GUARD_NC();
    Cache.clear();
}

uint32_t FShaderLibrary::GetCacheSize() const
{
    LOCK_GUARD_NC();
    return static_cast<uint32_t>(Cache.size());
}
