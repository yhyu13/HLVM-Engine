/**
 * Copyright 2026 HLVM Engine
 *
 * MIT License
 *
 * TestTextureCache - Unit tests for FTextureCache
 */

#include "Test.h"
#include "Renderer/Texture/TextureCache.h"
#include "Platform/FileSystem/Path.h"

DECLARE_LOG_CATEGORY(LogTest)

RECORD(TestTextureCache_EmptyCache, true)
{
    FTextureCache::Get().Clear();
    HLVM_ENSURE(FTextureCache::Get().GetNumEntries() == 0);
    HLVM_ENSURE(FTextureCache::Get().GetTotalMemoryBytes() == 0);
    HLVM_LOG(LogTest, info, TXT("TestTextureCache_EmptyCache passed"));
}

RECORD(TestTextureCache_NullHandle, true)
{
    FTextureCache::Get().Clear();

    // Inserting null handle should be a no-op
    FTextureCache::Get().Insert(FPath(TXT("/fake/path.png")), nullptr);
    HLVM_ENSURE(FTextureCache::Get().GetNumEntries() == 0);

    HLVM_LOG(LogTest, info, TXT("TestTextureCache_NullHandle passed"));
}

RECORD(TestTextureCache_InvalidPath, true)
{
    FTextureCache::Get().Clear();

    // GetTexture on non-cached path returns null
    nvrhi::TextureHandle Handle = FTextureCache::Get().GetTexture(FPath(TXT("/nonexistent/texture.png")));
    HLVM_ENSURE(Handle == nullptr);

    HLVM_LOG(LogTest, info, TXT("TestTextureCache_InvalidPath passed"));
}

RECORD(TestTextureCache_Invalidate, true)
{
    FTextureCache::Get().Clear();

    // Invalidate non-cached path should not crash
    FTextureCache::Get().Invalidate(FPath(TXT("/fake/path.png")));
    HLVM_ENSURE(FTextureCache::Get().GetNumEntries() == 0);

    HLVM_LOG(LogTest, info, TXT("TestTextureCache_Invalidate passed"));
}

RECORD(TestTextureCache_MemoryStatsEmpty, true)
{
    FTextureCache::Get().Clear();

    // Empty cache has zero memory
    size_t Mem = FTextureCache::Get().GetTotalMemoryBytes();
    HLVM_ENSURE(Mem == 0);

    HLVM_LOG(LogTest, info, TXT("TestTextureCache_MemoryStatsEmpty passed"));
}

RECORD(TestTextureCache_ClearNoCrash, true)
{
    FTextureCache::Get().Clear();
    FTextureCache::Get().Clear(); // Double clear should be safe
    HLVM_ENSURE(FTextureCache::Get().GetNumEntries() == 0);

    HLVM_LOG(LogTest, info, TXT("TestTextureCache_ClearNoCrash passed"));
}
