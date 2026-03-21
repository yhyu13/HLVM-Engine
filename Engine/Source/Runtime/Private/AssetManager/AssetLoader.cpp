/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 *
 * @file AssetLoader.cpp
 * @brief Implementation of FAssetLoader - asynchronous and synchronous asset loading system
 *
 * This file implements the core asset loading functionality:
 * - Static member initializations
 * - SyncLoad() - blocking synchronous loading
 * - AsyncLoad() - non-blocking background loading
 * - MainUpdate() - processes completion callbacks and file watching
 * - WaitOnLoaders() - busy-wait for loaders to complete
 * - RecordLoading/UnRecordLoading - duplicate load prevention
 */

#include "AssetManager/AssetLoader.h"

// =============================================================================
// Static Member Initializations
// =============================================================================
// All static state shared across FAssetLoader instances
// These are defined here to avoid ODR (One Definition Rule) violations

//! Watcher timer - checks for file modifications every 2 seconds
FTimer FAssetLoader::s_watcherTimer{ std::chrono::seconds{ 2 } };
//! Queue of loaders being watched for hot-reload (MPMC - thread-safe)
TConcurrentQueue<FAssetLoader::LoaderRef, EConcurrentQueueMode::Mpmc> FAssetLoader::s_watcherQueue;
//! Queue of async loaders awaiting completion processing (MPMC - thread-safe)
TConcurrentQueue<FAssetLoader::LoaderRef, EConcurrentQueueMode::Mpmc> FAssetLoader::s_asyncLoaderQueue;
//! Set of paths currently loading - prevents duplicate loads (protected by lock in RecordLoading)
TSetSmall<FPath>													  FAssetLoader::s_pathInLoadingSet;

// =============================================================================
// Public API Implementation
// =============================================================================

/**
 * @brief Synchronous asset loading - blocks until complete
 *
 * Creates a SyncLoader, executes it immediately, and optionally adds to watch queue.
 *
 * @param RelativePath Path to asset file (relative to game directory)
 * @param AsyncCallback Callback that performs actual loading from file
 * @param Options Configuration - must have Asynchronous=false
 *
 * @note Will log error if Options.Asynchronous=true (sync load requires false)
 * @note If Options.Watch=true, loader is added to watch queue for hot-reload
 * @note Loader failure is logged but does not throw exception
 */
void FAssetLoader::SyncLoad(const FPath& RelativePath, const FAssetLoader::AsyncCallback& AsyncCallback, const FAssetLoader::Options& Options)
{
	LoaderRef loader;
	if (Options.Asynchronous)
	{
		HLVM_LOG(LogAssetLoader, err, TXT("Async load requires Asynchronous=true!"));
	}
	else
	{
		loader = MAKE_SHARED(SyncLoader, RelativePath, AsyncCallback, Options);
		loader->Notify();
		if (!loader->IsLoaded())
		{
			HLVM_LOG(LogAssetLoader, err, TXT("Loader failed : {}"), *loader->GetPath());
		}
	}

	if (Options.Watch)
	{
		s_watcherQueue.Push(loader);
	}
}

FAssetLoader::LoaderRef FAssetLoader::AsyncLoad(const FPath& RelativePath, const FAssetLoader::AsyncCallback& AsyncCallback, const FAssetLoader::MainThreadCallback& MainThreadCallback, bool ManagedAsync, const FAssetLoader::Options& Options)
{
	LoaderRef loader;
	if (Options.Asynchronous)
	{
		loader = MAKE_SHARED(AsyncLoader, RelativePath, AsyncCallback, MainThreadCallback, Options);
		if (RecordLoading(loader))
		{
			loader->Notify();
		}
		else
		{
			return nullptr;
		}
	}
	else
	{
		// Log
		HLVM_LOG(LogAssetLoader, err, TXT("Loader failed : {}"), *loader->GetPath());
	}

	if (ManagedAsync)
	{
		s_asyncLoaderQueue.Push(loader);
	}

	if (Options.Watch)
	{
		s_watcherQueue.Push(loader);
	}

	return loader;
}

void FAssetLoader::MainUpdate()
{
	// Check watch list periodically every 2 seconds
	if (!s_watcherQueue.IsEmpty() && s_watcherTimer.Check(false))
	{
		TVector<LoaderRef> loaders;
		while (!s_watcherQueue.IsEmpty())
		{
			LoaderRef loader;
			s_watcherQueue.PopFront(loader);

			if (loader && loader->NeedsReload())
			{
				if (loader->m_options.Asynchronous)
				{
					if (RecordLoading(loader))
					{
						loader->Notify();
						s_asyncLoaderQueue.Push(loader);
					}
				}
				else
				{
					loader->Notify();
					if (!loader->IsLoaded())
					{
						// Log
						HLVM_LOG(LogAssetLoader, err, TXT("Watcher reload failed : {}"), *loader->GetPath());
					}
				}
			}
			loaders.push_back(loader);
		}

		for (auto& loader : loaders)
		{
			s_watcherQueue.Push(loader); // Re-push
		}
	}

	if (!s_asyncLoaderQueue.IsEmpty())
	{
		// Process async loading queue
		TVector<LoaderRef> stillLoading;
		while (!s_asyncLoaderQueue.IsEmpty())
		{
			LoaderRef loader;
			s_asyncLoaderQueue.PopFront(loader);

			if (loader && loader->IsLoaded())
			{
				loader->Update();
				UnRecordLoading(loader);
			}
			else
			{
				stillLoading.push_back(loader);
			}
		}

		for (auto& ref : stillLoading)
		{
			s_asyncLoaderQueue.Push(MoveTemp(ref));
		}
	}
}

// =============================================================================
// Internal Helper Functions
// =============================================================================

/**
 * @brief Block until all loaders in WaitList complete
 *
 * Busy-waits by repeatedly checking IsLoaded() on each loader.
 * Completed loaders are updated and removed from the list.
 *
 * @param WaitList Vector of loaders to wait for (modified in-place)
 *
 * @note This is a blocking call - will not return until all loaders complete
 * @note Calls Update() on each loader as it completes
 * @note Removes completed loaders from WaitList (list will be empty on return)
 */
void FAssetLoader::WaitOnLoaders(TVector<LoaderRef>& WaitList)
{
	while (!WaitList.empty())
	{
		for (auto it = WaitList.begin(); it != WaitList.end();)
		{
			if (auto& loader = *it; loader)
			{
				if (loader->IsLoaded())
				{
					loader->Update();
					UnRecordLoading(loader);
					it = WaitList.erase(it);
				}
				else
				{
					++it;
				}
			}
			else
			{
				it = WaitList.erase(it);
			}
		}
	}
}

/**
 * @brief Register loader to prevent duplicate path loading
 *
 * Checks if path is already in s_pathInLoadingSet.
 * If not, adds it and returns true.
 * If yes, returns false (duplicate detected).
 *
 * @param Loader Loader to register
 * @return true if registered successfully, false if duplicate
 *
 * @note Thread-safe: Uses LOCK_GUARD_NI() for mutual exclusion
 * @note Logs debug message on duplicate, info message on success
 */
bool FAssetLoader::RecordLoading(const FAssetLoader::LoaderRef& Loader)
{
	LOCK_GUARD_NI();
	if (s_pathInLoadingSet.contains(Loader->GetPath()))
	{
		// Log - duplicate detected, ignoring
		HLVM_LOG(LogAssetLoader, debug, TXT("Loader dups ignore: {}"), *Loader->GetPath());
		return false;
	}
	// Log - starting new load
	HLVM_LOG(LogAssetLoader, info, TXT("Loader started : {}"), *Loader->GetPath());
	s_pathInLoadingSet.insert(Loader->GetPath());
	return true;
}

/**
 * @brief Unregister loader after completion
 *
 * Removes loader's path from s_pathInLoadingSet,
 * allowing the same path to be loaded again in future.
 *
 * @param Loader Loader to unregister
 *
 * @note Thread-safe: Uses LOCK_GUARD_NI() for mutual exclusion
 * @note Should be called after loader completes (successfully or not)
 */
void FAssetLoader::UnRecordLoading(const FAssetLoader::LoaderRef& Loader)
{
	LOCK_GUARD_NI();
	// Log
	HLVM_LOG(LogAssetLoader, info, TXT("Loader finished : {}"), *Loader->GetPath());
	s_pathInLoadingSet.erase(Loader->GetPath());
}

/**
 * @brief Get file's last modification time
 *
 * @param Path File path to query
 * @return std::time_t Last write time
 *
 * @note Used by NeedsReload() for hot-reload detection
 * @note Implemented using boost::filesystem
 */
std::time_t FAssetLoader::GetLastWriteTime(const FPath& Path)
{
	namespace fs = boost::filesystem;
	auto time = fs::last_write_time(Path);
	return time;
}
