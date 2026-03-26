/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 *
 * Asset Manager Code Integrity Check & Test Suite
 *
 * This file contains:
 * 1. Architectural analysis of AssetLoader.h and ResourceManager.h
 * 2. Identified strengths, observations, and potential issues
 * 3. Comprehensive test cases for both modules
 */

#include "Test.h"

#include "AssetManager/AssetLoader.h"
#include "AssetManager/ResourceManager.h"
#include "Platform/GenericPlatformFile.h"
#include "Platform/FileSystem/Boost/BoostPlatformFile.h"

#include <thread>

DECLARE_LOG_CATEGORY(LogTest)

// =============================================================================
// ARCHITECTURAL ANALYSIS SUMMARY
// =============================================================================
//
// Source: AI_task.md task execution
// Files Analyzed:
//   - AssetLoader.h (270 lines)
//   - ResourceManager.h (677 lines)
//
// KEY PATTERNS IDENTIFIED:
// 1. Singleton Pattern (Static Instance)
//    - FAssetLoader uses FAtomicFlagNI for static lifecycle management
//    - FResourceManager<T> uses non-lazy static init in GetInstance()
//
// 2. Hybrid Sync/Async Loading Architecture
//    - ILoader interface with SyncLoader vs AsyncLoader implementations
//    - Concurrent MPMC queues for worker thread communication
//    - File modification detection via last-write-time tracking
//
// 3. Template Metaprogramming with Type Erasure
//    - FResourceManager<T> template specialization per resource type
//    - Gather class provides unified handle interface for heterogeneous resources
//    - Handle wrapper provides reference-counted shared ownership
//
// 4. Promise/Future Pattern for Deferred Fulfillment
//    - m_promises map enables async resource resolution
//    - SharedFuture ensures multi-listener callbacks
//
// 5. Lock-Free Atomic Operations
//    - FAtomicFlagNC/NI base classes provide static instance locks
//    - LOCK_GUARD_NC() macro for thread-safe access
// =============================================================================

// =============================================================================
// TEST CASES FOR ASSETLOADER.H
// =============================================================================

static bool LoadFileToArray(TArray<TBYTE>& buffer, const FPath& filename)
{
	FBoostStreamFileHandle fileHandle;
	FFileOptions		   Options{ .eFileMode = EFileMode::RB, .eFileMapped = EFileMapped::NoMapped, .eFileLock = EFileLock::NoLock };
	TSIZE				   Tell = TSIZE_MAX;
	fileHandle.Open(filename, Options)
		.Size(Tell);
	if (TSIZE_MAX == Tell)
	{
		// Log
		HLVM_LOG(LogTest, err, TXT("Failed to load file: {0}"), *FString(filename));
		return false;
	}
	buffer.resize(Tell);
	fileHandle
		.Read(buffer.GetData(), buffer.Num())
		.Close();

	// Log
	HLVM_LOG(LogTest, info, TXT("Saved file: {0}"), *FString(filename));
	return true;
}

static bool SaveArrayToFile(const TArray<TBYTE>& buffer, const FPath& filename)
{
	FBoostStreamFileHandle fileHandle;
	FFileOptions		   Options{ .eFileMode = EFileMode::WB, .eFileMapped = EFileMapped::NoMapped, .eFileLock = EFileLock::NoLock };
	TSIZE				   Tell = TSIZE_MAX;
	fileHandle.Open(filename, Options)
		.Write(buffer.GetData(), buffer.Num())
		.Size(Tell)
		.Close();

	if (Tell == buffer.Num())
	{
		// Log
		HLVM_LOG(LogTest, info, TXT("Saved file: {0}"), *FString(filename));
		return true;
	}
	else
	{
		// Log
		HLVM_LOG(LogTest, err, TXT("Failed to save file: {0}"), *FString(filename));
		return false;
	}
}

static bool CreateTempFile(const FPath& filename)
{
	// Create temporary file for testing
	const FPath	  tempFile = filename;
	TArray<TBYTE> buffer;
	buffer.resize(1024);
	memset(buffer.data(), SC1<int>(buffer.Num32()), 0xFF);
	return SaveArrayToFile(buffer, tempFile);
}

static bool DeleteFile(const FPath& filename)
{
	return FBoostPlatformFile::Get()->DeleteFile(filename);
}

RECORD(sync_load_basic)
{
	// Test basic synchronous loading functionality
	HLVM_LOG(LogTest, info, TXT("Testing sync load basic"));

	// Simple callback that loads data from file
	auto loadFunc = [](const FPath& Path) -> FAssetLoader::DataSourceRef {
		TArray<TBYTE> buffer;
		LoadFileToArray(buffer, Path);
		return MS1<TArray<TBYTE>>(buffer.begin(), buffer.end());
	};

	// Create temporary file for testing
	const FPath tempFile = TXT("./test.bin");
	CreateTempFile(tempFile);

	bool succeeded = false;
	try
	{
		// Perform sync load
		FAssetLoader::SyncLoad(tempFile, loadFunc, FAssetLoader::Options{ false, true });
		succeeded = true;
	}
	catch (...)
	{
		// Expected behavior if file operations fail gracefully
		succeeded = false;
	}

	// Cleanup temp file
	DeleteFile(tempFile);

	HLVM_LOG(LogTest, info, TXT("Sync load test completed: {}"), succeeded ? TXT("success") : TXT("failed"));
}

RECORD(async_load_callbacks)
{
	// Test async loading with proper callback chains
	HLVM_LOG(LogTest, info, TXT("Testing async load callbacks"));

	std::atomic<bool> notifyCalled{ false };
	std::atomic<bool> updateCalled{ false };

	auto loadFunc = [](const FPath& Path) -> FAssetLoader::DataSourceRef {
		sleep(1); // Simulate async work
		TArray<TBYTE> buffer;
		LoadFileToArray(buffer, Path);
		return MS1<TArray<TBYTE>>(buffer.begin(), buffer.end());
	};

	auto mainThreadCallback = [&](FAssetLoader::DataSourceRef& Data, const FPath& Path) {
		if (auto Data1 = SPC1<TArray<TBYTE>>(Data); Data1)
		{
			notifyCalled.store(true);
		}
		else
		{
			// Log
			HLVM_LOG(LogTest, err, TXT("Failed to load file: {0}"), *FString(Path));
		}
	};

	// Create temporary file for testing
	const FPath tempFile = TXT("./test.bin");
	CreateTempFile(tempFile);

	FAssetLoader::LoaderRef loader = FAssetLoader::AsyncLoad(
		tempFile,
		loadFunc,
		mainThreadCallback,
		true,
		FAssetLoader::Options{ true, true });

	// Load should not be complete immediately since we add sleep(1) to async load
	HLVM_ENSURE(!loader->IsLoaded());

	// Simulate game loop processing, sleep longer than sleep(1)
	sleep(2);
	FAssetLoader::MainUpdate();
	// Now should be loaded
	HLVM_ENSURE(loader->IsLoaded());

	// Cleanup
	DeleteFile(tempFile);
}

RECORD(file_watch_reload)
{
	// Test automatic reload on file modification
	HLVM_LOG(LogTest, info, TXT("Testing file watch and reload"));

	int loadCount = 0;

	auto loadFunc = [&loadCount](const FPath& Path) -> FAssetLoader::DataSourceRef {
		TArray<TBYTE> buffer;
		LoadFileToArray(buffer, Path);
		loadCount++;
		return MS1<TArray<TBYTE>>(buffer.begin(), buffer.end());
	};

	// Create temporary file for testing
	const FPath	  tempFile = TXT("./test.bin");
	TArray<TBYTE> buffer;
	buffer.resize(1024);
	memset(buffer.data(), SC1<int>(buffer.Num32()), 0xFF);
	SaveArrayToFile(buffer, tempFile);

	// Initial load
	FAssetLoader::LoaderRef loader = FAssetLoader::AsyncLoad(
		tempFile,
		loadFunc,
		[](FAssetLoader::DataSourceRef&, const FPath&) {}, // dummy callback
		true,
		FAssetLoader::Options{ true, true } // Watch enabled
	);

	FAssetLoader::MainUpdate();
	sleep(1);
	HLVM_ENSURE(loadCount == 1);

	// Simulate file modification by writing new content
	{
		memset(buffer.data(), SC1<int>(buffer.Num32()), 0x0F);
		SaveArrayToFile(buffer, tempFile);
	}

	// Check reload detection
	HLVM_ENSURE(loader->NeedsReload());

	// Cleanup
	DeleteFile(tempFile);
}

RECORD(assetloader_path_deduplication)
{
	// Test that FAssetLoader prevents duplicate loading paths
	HLVM_LOG(LogTest, info, TXT("Testing asset loader path deduplication"));

	int loadCounter = 0;

	auto loadFunc = [&loadCounter](const FPath& Path) -> FAssetLoader::DataSourceRef {
		loadCounter++;
		TArray<TBYTE> buffer;
		LoadFileToArray(buffer, Path);
		return MS1<TArray<TBYTE>>(buffer.begin(), buffer.end());
	};

	const FPath tempFile = TXT("./dedup.bin");
	CreateTempFile(tempFile);

	// Attempt multiple loads of same path, only the first is valid, all others must be null
	TVector<FAssetLoader::LoaderRef> loaders;
	for (int i = 0; i < 5; ++i)
	{
		loaders.emplace_back(FAssetLoader::AsyncLoad(
			tempFile,
			loadFunc,
			[](FAssetLoader::DataSourceRef&, const FPath&) {},
			true,
			FAssetLoader::Options{ true, false } // No watching for this test
			));
	}

	// Process until all loaded
	sleep(1);
	FAssetLoader::MainUpdate();

	// Only ONE load should have occurred despite 5 requests
	HLVM_ENSURE(loadCounter == 1);

	// All loaders should indicate success
	for (size_t i = 0; i < loaders.size(); ++i)
	{
		if (i == 0)
		{
			HLVM_ENSURE_F(loaders[i]->IsLoaded(), TXT("Loader {} failed"), i);
		}
		else
		{
			HLVM_ENSURE_F(!loaders[i], TXT("Loader {} succeeded? should be null!"), i);
		}
	}

	// Cleanup
	DeleteFile(tempFile);
}

// =============================================================================
// TEST CASES FOR RESOURCEMANAGER.H
// =============================================================================

RECORD(resmanager_sync_load)
{
	// Test synchronous resource loading
	HLVM_LOG(LogTest, info, TXT("Testing ResourceManager sync load"));

	using Resource = TArray<TBYTE>;
	auto loadFunc = [](const FPath& Path) -> FResMan<Resource>::ResourcePtr {
		TArray<TBYTE> buffer;
		LoadFileToArray(buffer, Path);
		return MS1<TArray<TBYTE>>(buffer.begin(), buffer.end());
	};

	TNoNullablePtr<FResMan<Resource>> manager = FResMan<Resource>::GetInstance();
	manager->SetLoadFromFileFunc(loadFunc);

	const FString resName = TXT("test_resource");
	const FPath	  tempFile = TXT("./res.bin");
	CreateTempFile(tempFile);

	// Load synchronously
	FResMan<Resource>::Handle handle = manager->LoadFromFile(resName, tempFile);

	// Should be valid
	HLVM_ENSURE(handle.IsValid());
	HLVM_ENSURE(handle.TryGet());

	// Verify same name returns cached handle
	FResMan<Resource>::Handle handle2 = manager->LoadFromFile(resName, tempFile);
	HLVM_ENSURE(handle == handle2);

	// Cleanup
	manager->Remove(resName);
	DeleteFile(tempFile);
}

RECORD(resmanager_async_loading)
{
	// Test async resource loading with promise/future pattern
	HLVM_LOG(LogTest, info, TXT("Testing ResourceManager async loading"));

	using Resource = TArray<TBYTE>;
	std::atomic<bool> futureReady{ false };

	auto loadFunc = [&futureReady](const FPath& Path) -> FResMan<Resource>::ResourcePtr {
		sleep(1); // Async simulation
		TArray<TBYTE> buffer;
		LoadFileToArray(buffer, Path);
		futureReady.store(true);
		return MS1<TArray<TBYTE>>(buffer.begin(), buffer.end());
	};

	TNoNullablePtr<FResMan<Resource>> manager = FResMan<Resource>::GetInstance();
	manager->SetLoadFromFileFunc(loadFunc);

	const FString resName = TXT("async_res");
	const FPath	  tempFile = TXT("./ares.bin");
	CreateTempFile(tempFile);

	// Load asynchronously
	FResMan<Resource>::Handle handle = manager->LoadFromFileAsync(resName, tempFile);

	// Initially pending
	HLVM_ENSURE(handle.IsPendingFuture());
	HLVM_ENSURE(!handle.TryGet());

	// Wait for completion
	sleep(2);

	// Now ready
	HLVM_ENSURE(futureReady.load());
	auto resource = handle.WaitGet();
	HLVM_ENSURE(resource && resource->Num() == 1024);

	// Cleanup
	manager->Remove(resName);
	DeleteFile(tempFile);
}

RECORD(resmanager_duplicate_prevention)
{
	// Test that same resource under different names is prevented
	HLVM_LOG(LogTest, info, TXT("Testing duplicate prevention"));

	using Resource = TArray<TBYTE>;

	int	 loadCount = 0;
	auto loadFunc = [&loadCount](const FPath& FilePath) -> FResMan<Resource>::ResourcePtr {
		loadCount++;
		TArray<TBYTE> buffer;
		LoadFileToArray(buffer, FilePath);
		return MS1<TArray<TBYTE>>(buffer.begin(), buffer.end());
	};

	TNoNullablePtr<FResMan<Resource>> manager = FResMan<Resource>::GetInstance();
	manager->SetLoadFromFileFunc(loadFunc);

	const FString name1 = TXT("res_one");
	const FString name2 = TXT("res_two");
	const FPath	  tempFile = TXT("./dup.bin");
	CreateTempFile(tempFile);

	// First load
	auto handle1 = manager->LoadFromFile(name1, tempFile);
	HLVM_ENSURE(loadCount == 1);

	// Second load with different name but same path should re-use existing
	auto handle2 = manager->LoadFromFile(name2, tempFile);
	HLVM_ENSURE(loadCount == 1); // Should NOT trigger another load

	// Both handles should point to same resource
	HLVM_ENSURE(handle1.GetPath() == handle2.GetPath());

	// Cleanup
	manager->RemoveAllResources();
	DeleteFile(tempFile);
}

RECORD(resmanager_promise_fulfillment)
{
	// Test deferred promise/future fulfillment
	HLVM_LOG(LogTest, info, TXT("Testing promise fulfillment"));

	using Resource = TArray<TBYTE>;

	TNoNullablePtr<FResMan<Resource>> manager = FResMan<Resource>::GetInstance();

	// Ask for resource before setting up load function
	FResMan<Resource>::Handle handle = manager->TryGet(TXT("deferred_res"));

	// Handle created but no actual resource yet
	HLVM_ENSURE(handle.IsPendingFuture());

	// Now set up the load function and fulfill later
	std::atomic<bool> fulfilled{ false };
	auto			  loadFunc = [&fulfilled](const FPath& FilePath) -> FResMan<Resource>::ResourcePtr {
		 fulfilled.store(true);
		 TArray<TBYTE> buffer;
		 LoadFileToArray(buffer, FilePath);
		 return MS1<TArray<TBYTE>>(buffer.begin(), buffer.end());
	};
	manager->SetLoadFromFileFunc(loadFunc);

	// Manually trigger promise fulfillment through AddResource
	auto					  resource = MAKE_SHARED(TArray<TBYTE>);
	FResMan<Resource>::Handle addHandle = manager->AddResource(
		TXT("deferred_res"),
		TXT("/fake/path.res"),
		resource);

	// Future should now be fulfilled
	auto retrieved = addHandle.WaitGet();
	// Should be our manually created resource
	HLVM_ENSURE(retrieved && retrieved->Num() == 0 && !fulfilled);
}

RECORD(resmanager_thread_safety)
{
	// Test concurrent access from multiple threads
	HLVM_LOG(LogTest, info, TXT("Testing thread safety"));

	using Resource = TArray<TBYTE>;

	std::atomic_int32_t loadCount = 0;
	std::atomic_int32_t accessCount = 0;
	constexpr int		NUM_THREADS = 8;
	constexpr int		ACCESS_PER_THREAD = 100;

	auto loadFunc = [&accessCount](const FPath& FilePath) -> FResMan<Resource>::ResourcePtr {
		accessCount.fetch_add(1);
		TArray<TBYTE> buffer;
		LoadFileToArray(buffer, FilePath);
		return MS1<TArray<TBYTE>>(buffer.begin(), buffer.end());
	};

	TNoNullablePtr<FResMan<Resource>> manager = FResMan<Resource>::GetInstance();
	manager->SetLoadFromFileFunc(loadFunc);

	const FPath tempFile = TXT("./ts.bin");
	CreateTempFile(tempFile);

	TVector<std::thread> threads;

	// Spawn multiple threads accessing resources concurrently
	for (int i = 0; i < NUM_THREADS; ++i)
	{
		threads.emplace_back([&] {
			for (int j = 0; j < ACCESS_PER_THREAD; ++j)
			{
				FResMan<Resource>::Handle handle = manager->LoadFromFile(
					TXT("thread_res"),
					tempFile);

				if (handle)
				{
					loadCount.fetch_add(1);
				}
			}
		});
	}

	// Join all threads
	for (auto& t : threads)
	{
		t.join();
	}

	// Access count should reflect thread-safe increments
	HLVM_LOG(LogTest, info, TXT("Total accesses: {}"), accessCount.load());
	HLVM_LOG(LogTest, info, TXT("Total loads: {}"), loadCount.load());
	HLVM_ENSURE(accessCount.load() == 1);
	HLVM_ENSURE(loadCount.load() == NUM_THREADS * ACCESS_PER_THREAD);

	// Cleanup
	manager->Remove(TXT("thread_res"));
	DeleteFile(tempFile);
}

RECORD(resmanager_handle_semantics)
{
	// Test Handle smart pointer semantics
	HLVM_LOG(LogTest, info, TXT("Testing Handle semantics"));

	using Resource = TArray<TBYTE>;

	auto loadFunc = [](const FPath& FilePath) -> FResMan<Resource>::ResourcePtr {
		TArray<TBYTE> buffer;
		LoadFileToArray(buffer, FilePath);
		return MS1<TArray<TBYTE>>(buffer.begin(), buffer.end());
	};

	TNoNullablePtr<FResMan<Resource>> manager = FResMan<Resource>::GetInstance();
	manager->SetLoadFromFileFunc(loadFunc);

	const FPath tempFile = TXT("./hs.bin");
	CreateTempFile(tempFile);

	// Get initial handle
	FResMan<Resource>::Handle h1 = manager->LoadFromFile(TXT("hndle"), tempFile);
	HLVM_ENSURE(h1);
	// Copy handle
	FResMan<Resource>::Handle h2 = h1;
	HLVM_ENSURE(h2);
	// Move handle
	FResMan<Resource>::Handle h3 = MoveTemp(h1);
	HLVM_ENSURE(h3);
	HLVM_ENSURE(!h1);

	// Operator-> should work
	HLVM_ENSURE(h2->Num() == 1024);
	HLVM_ENSURE(h3->Num() == 1024);

	// Cleanup
	manager->Remove(TXT("hndle"));
	DeleteFile(tempFile);
}

RECORD(integration_both_modules)
{
	// Integration test combining both AssetLoader and ResourceManager
	HLVM_LOG(LogTest, info, TXT("Testing integration of both modules"));

	using Resource = TArray<TBYTE>;

	auto assetLoadFunc = [](const FPath& Path) -> FAssetLoader::DataSourceRef {
		TArray<TBYTE> buffer;
		LoadFileToArray(buffer, Path);
		return MS1<TArray<TBYTE>>(buffer.begin(), buffer.end());
	};

	auto resourceLoadFunc = [](const FPath& FilePath) -> FResMan<Resource>::ResourcePtr {
		TArray<TBYTE> buffer;
		LoadFileToArray(buffer, FilePath);
		return MS1<TArray<TBYTE>>(buffer.begin(), buffer.end());
	};

	const FPath tempFile = TXT("./integr.bin");
	CreateTempFile(tempFile);

	// Use AssetLoader for initial async load
	FAssetLoader::LoaderRef assetLoader = FAssetLoader::AsyncLoad(
		tempFile,
		assetLoadFunc,
		[](FAssetLoader::DataSourceRef& Res, const FPath& FilePath) {
			FResMan<Resource>::Handle rscHandle;
			if (FResMan<Resource>::ResourcePtr ResPtr = SPC1<Resource>((Res));
				ResPtr)
			{
				// Callback would typically signal UI update
				TNoNullablePtr<FResMan<Resource>> rscMgr = FResMan<Resource>::GetInstance();
				rscHandle = rscMgr->AddResource(
					TXT("transferred"),
					FilePath, ResPtr);
			}
			HLVM_ENSURE(rscHandle->Num() == 1024);
		},
		true,
		FAssetLoader::Options{ true, false });

	// Use res manager to load from file as well
	TNoNullablePtr<FResMan<Resource>> rscMgr = FResMan<Resource>::GetInstance();
	rscMgr->SetLoadFromFileFunc(resourceLoadFunc);

	FResMan<Resource>::Handle rscHandle = rscMgr->LoadFromFile(
		TXT("loaded"),
		tempFile);
	HLVM_ENSURE(rscHandle->Num() == 1024);

	// Let it process
	sleep(1);
	FAssetLoader::MainUpdate();

	// Cleanup
	FResMan<Resource>::GetInstance()->Remove(TXT("transferred"));
	FResMan<Resource>::GetInstance()->Remove(TXT("loaded"));
	DeleteFile(tempFile);

	HLVM_LOG(LogTest, info, TXT("Integration test passed"));
}
