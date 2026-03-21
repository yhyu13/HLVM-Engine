/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 */

#pragma once

#include "CommonMinimal.h"
#include "Core/Parallel/ConcurrentQueue.h"
#include "Core/Parallel/Lock.h"
#include "Utility/Timer.h"
#include "Platform/FileSystem/FileSystem.h"
#include "Platform/FileSystem/Path.h"
#include "Template/ReferenceTemplate.tpp"

#include <atomic>
#include <functional>
#include <memory>
#include <thread>

DECLARE_LOG_CATEGORY(LogAssetLoader)

/**
 * @brief Custom AssetManager that features async loading support
 *
 * FAssetLoader provides a unified interface for loading assets both synchronously and asynchronously.
 * It supports:
 * - Synchronous blocking loads for immediate asset availability
 * - Asynchronous non-blocking loads with completion callbacks
 * - File watching with automatic reload on modification (hot-reload support)
 * - Duplicate load prevention (same path won't be loaded multiple times)
 * - Thread-safe operations using atomic flags and concurrent queues
 *
 * Thread Safety:
 * - Static methods are thread-safe
 * - AsyncLoad() can be called from any thread
 * - MainUpdate() should be called from main thread to process completion callbacks
 * - s_pathInLoadingSet is protected by s_loadingSetLock to prevent race conditions
 *
 * Usage Example:
 * @code
 * // Sync load
 * FAssetLoader::SyncLoad(path, [](const FPath& p) { return LoadAsset(p); });
 *
 * // Async load with callback
 * auto loader = FAssetLoader::AsyncLoad(path, loadCallback, completionCallback);
 * // In game loop:
 * FAssetLoader::MainUpdate();
 * @endcode
 *
 * @note This class cannot be instantiated (NOINSTANTIATE). Use static methods only.
 */
class FAssetLoader : private FAtomicFlagNI<FAssetLoader>
{
public:
	NOINSTANTIATE(FAssetLoader);
	//! Data source reference type
	using DataSourceRef = TSharePtr<void>;

	//! Async callback: loads data from path
	using AsyncCallback = std::function<DataSourceRef(const FPath&)>;

	//! Main thread callback: notified when async load completes
	using MainThreadCallback = std::function<void(DataSourceRef&, const FPath&)>;

	//! Options for asset loading configuration
	/**
	 * @struct Options
	 * @brief Configuration flags controlling asset loading behavior
	 *
	 * @details Asynchronous If true, load in background thread. If false, block until complete.
	 * Watch If true, monitor file for changes and trigger reload on modification.
	 */
	struct Options
	{
		Options()
			: Asynchronous(true), Watch(true)
		{
		}
		Options(bool InAsynchronous, bool InWatch)
			: Asynchronous(InAsynchronous), Watch(InWatch)
		{
		}

		bool Asynchronous; //!< If true, load asynchronously in background thread
		bool Watch;		   //!< If true, auto-reload when file modification detected
	};

	/**
	 * @brief Base loader interface for all asset loading operations
	 *
	 * ILoader defines the common interface for both synchronous and asynchronous loading.
	 * It manages:
	 * - Load state tracking via atomic flag (m_loaded)
	 * - File path and callback storage
	 * - Watch timer for hot-reload functionality
	 *
	 * Lifecycle:
	 * 1. Created by FAssetLoader with appropriate callback
	 * 2. Notify() called to start loading
	 * 3. IsLoaded() checked to determine completion
	 * 4. Update() called to invoke completion callback (async only)
	 *
	 * @note Do not copy or move loaders - they manage active load operations
	 */
	class ILoader
	{
	public:
		NOCOPYMOVE(ILoader);
		ILoader() = default;
		virtual ~ILoader() = default;

		HLVM_NO_DISCARD const FPath& GetPath() const noexcept { return m_path; }

		//! Returns true if asset needs reload (only for watched assets)
		HLVM_NO_DISCARD bool NeedsReload()
		{
			if (!m_options.Watch)
			{
				// Log
				HLVM_LOG(LogAssetLoader, debug, TXT("NeedsReload: File not watched for, thus no reload: {}"), *m_path);
				return false;
			}

			if (!FPath::Exists(m_path))
			{
				// Log
				HLVM_LOG(LogAssetLoader, warn, TXT("NeedsReload: File not found: {}"), *m_path);
				return false;
			}

			auto currentWriteTime = GetLastWriteTime(m_path);
			if (m_lastWriteTime != currentWriteTime)
			{
				m_lastWriteTime = currentWriteTime;
				return true;
			}
			return false;
		}

		//! Loads the asset synchronously
		virtual void Notify() = 0;

		//! Async-specific update (called from main thread)
		virtual void Update() = 0;

		/**
		 * Checks if the asset has finished loading successfully.
		 *
		 * Two conditions must be met:
		 * 1. m_loaded atomic flag is set (load operation completed)
		 * 2. m_data is not null (load succeeded, not failed)
		 *
		 * @return true if asset is ready to use, false if still loading or failed
		 * @note Logs error if data is null after load completes
		 */
		HLVM_NO_DISCARD bool IsLoaded() const
		{
			if (!m_loaded.load(std::memory_order_acquire))
			{
				return false;
			}
			if (m_data == nullptr)
			{
				HLVM_LOG(LogAssetLoader, err, TXT("Loader failed : {}"), *m_path);
				return false;
			}
			return true;
		}

	protected:
		friend class FAssetLoader;

		std::atomic_bool m_loaded{ false };
		Options			 m_options;
		FPath			 m_path;
		std::time_t		 m_lastWriteTime{ 0 };
		AsyncCallback	 m_callback;
		DataSourceRef	 m_data{ nullptr };
	};
	using LoaderRef = TSharePtr<ILoader>;

	/**
	 * @brief Synchronous loader - blocks until asset is fully loaded
	 *
	 * SyncLoader performs immediate blocking load on the calling thread.
	 * Use for:
	 * - Critical assets that must be available immediately
	 * - Small assets where blocking is acceptable
	 * - Initialization code where async complexity is unwanted
	 *
	 * Not recommended for:
	 * - Large assets (causes frame hitch)
	 * - Runtime loading during gameplay
	 * - Network or slow I/O sources
	 */
	class SyncLoader final : public ILoader
	{
	public:
		NOCOPYMOVE(SyncLoader);

		explicit SyncLoader(const FPath& RelativePath, const AsyncCallback& Callback,
			const Options& InOptions)
		{
			m_options = InOptions;
			m_path = FPath::Absolute(RelativePath);
			m_callback = Callback;

			HLVM_ENSURE(FPath::Exists(m_path));
			if (m_options.Watch)
			{
				m_lastWriteTime = GetLastWriteTime(m_path);
			}
		}

		virtual void Notify() override
		{
			m_data = m_callback(m_path);
			m_loaded.store(true, std::memory_order_release);
		}

		virtual void Update() override
		{
		}
	};

	/**
	 * @brief Async loader using detached background thread
	 *
	 * AsyncLoader spawns a detached std::thread to load assets without blocking.
	 * Upon completion, the loader is queued for main thread callback invocation.
	 *
	 * Thread Behavior:
	 * - Background thread: Executes callback, sets m_loaded flag
	 * - Main thread: Calls Update() to invoke m_mainThreadCallback
	 *
	 * Exception Handling:
	 * - Exceptions in callback are caught and logged
	 * - Failed loads result in null m_data
	 * - No exception propagation to caller
	 *
	 * @warning Uses std::thread::detach() - thread cannot be joined
	 * @warning MainUpdate() must be called regularly to process completion callbacks
	 */
	class AsyncLoader final : public ILoader
	{
	public:
		NOCOPYMOVE(AsyncLoader);
		AsyncLoader() = default;

		explicit AsyncLoader(const FPath& RelativePath,
			const AsyncCallback&		  AsyncCallback,
			const MainThreadCallback&	  MainThreadCallback,
			const Options&				  InOptions)
		{
			m_options = InOptions;
			m_path = FPath::Absolute(RelativePath);
			m_callback = AsyncCallback;

			HLVM_ENSURE(FPath::Exists(m_path));
			if (m_options.Watch)
			{
				m_lastWriteTime = GetLastWriteTime(m_path);
			}
			m_mainThreadCallback = MainThreadCallback;
		}

		//! Loads asset in separated thread
		void Notify() override
		{
			m_loaded.store(false, std::memory_order_release);
			std::thread([this]() {
				try
				{
					m_data = m_callback(GetPath());
					m_loaded.store(true, std::memory_order_release);
				}
				catch (std::exception& e)
				{
					// Log
					HLVM_LOG(LogAssetLoader, err, TXT("Loader failed : {}"), TO_TCHAR_CSTR(e.what()));
				}
			}).detach();
		}

		//! Must be called from main thread after load completes
		void Update() override
		{
			if (IsLoaded())
			{
				if (m_mainThreadCallback)
				{
					m_mainThreadCallback(m_data, m_path);
				}
			}
		}

	private:
		MainThreadCallback m_mainThreadCallback;
	};

	/**
	 * @brief Synchronous load - blocks calling thread until asset is loaded
	 *
	 * @param RelativePath Path to asset file (relative to game directory)
	 * @param AsyncCallback Callback function that performs actual loading from file
	 * @param Options Loading configuration (default: sync, watch enabled)
	 *
	 * @note This function blocks until loading completes
	 * @note If Options.Watch=true, loader is added to watch queue for hot-reload
	 * @note If Options.Asynchronous must be false (will log error otherwise)
	 */
	static void SyncLoad(const FPath& RelativePath, const AsyncCallback& AsyncCallback,
		const Options& Options = { false, true });

	/**
	 * @brief Async load - spawns background thread, returns immediately
	 *
	 * @param RelativePath Path to asset file (relative to game directory)
	 * @param AsyncCallback Callback function that performs actual loading from file
	 * @param MainThreadCallback Invoked on main thread when load completes (may be null)
	 * @param ManagedAsync If true, auto-manage via MainUpdate(). If false, manual management required
	 * @param Options Loading configuration (default: async, watch enabled)
	 *
	 * @return LoaderRef Shared pointer to loader, or nullptr if path is already loading
	 *
	 * @note Returns immediately without blocking
	 * @note Call MainUpdate() regularly if ManagedAsync=true to process completion callbacks
	 * @note Duplicate path detection prevents loading same file twice simultaneously
	 */
	static LoaderRef AsyncLoad(const FPath& RelativePath, const AsyncCallback& AsyncCallback,
		const MainThreadCallback& MainThreadCallback,
		bool ManagedAsync = true, const Options& Options = { true, true });

	/**
	 * @brief Process queued async loaders - call from main game loop
	 *
	 * MainUpdate() performs two critical tasks:
	 * 1. Check watched assets for file modifications (every 2 seconds)
	 *    - Calls NeedsReload() on each watched loader
	 *    - Triggers reload if file changed
	 * 2. Process completed async loaders
	 *    - Checks IsLoaded() on each queued loader
	 *    - Invokes MainThreadCallback for completed loads
	 *    - Removes completed loaders from queue
	 *
	 * @note Thread-safe: Can be called from any thread, but typically main thread
	 * @note Performance: O(n) where n = number of watched/queued loaders
	 * @note Must be called regularly for async loading to complete
	 */
	static void MainUpdate();

	/**
	 * @brief Block until all loaders in list complete loading
	 *
	 * @param WaitList Vector of loaders to wait for (modified - completed loaders removed)
	 *
	 * @note This is a busy-wait - spins checking loader status
	 * @note Useful for shutdown/synchronization points
	 * @note Updates loaders as they complete (calls Update())
	 * @note WaitList will be empty when function returns
	 */
	static void WaitOnLoaders(TVector<LoaderRef>& WaitList);

private:
	/**
	 * @brief Register a loader as "currently loading" to prevent duplicates
	 *
	 * @param Loader Loader to register
	 * @return true if registration succeeded, false if path already loading
	 *
	 * @note Thread-safe: Protected by s_loadingSetLock (implicit via LOCK_GUARD_NI)
	 * @note Prevents same path from being loaded multiple times concurrently
	 * @note Logs debug message if duplicate detected
	 */
	static bool RecordLoading(const LoaderRef& Loader);

	/**
	 * @brief Unregister loader after completion
	 *
	 * @param Loader Loader to unregister
	 *
	 * @note Thread-safe: Protected by s_loadingSetLock (implicit via LOCK_GUARD_NI)
	 * @note Removes path from s_pathInLoadingSet
	 * @note Allows same path to be loaded again in future
	 */
	static void UnRecordLoading(const LoaderRef& Loader);

	/**
	 * @brief Get last write time of file for hot-reload detection
	 *
	 * @param Path File path to check
	 * @return std::time_t Last modification timestamp
	 *
	 * @note Used by NeedsReload() to detect file changes
	 * @note Implemented using boost::filesystem::last_write_time
	 */
	static std::time_t GetLastWriteTime(const FPath& Path);

private:
	HLVM_STATIC_VAR FTimer s_watcherTimer;

	//! Concurrent queue for watched assets (Mpmc)
	HLVM_STATIC_VAR TConcurrentQueue<LoaderRef, EConcurrentQueueMode::Mpmc> s_watcherQueue;

	//! Concurrent queue for async loading assets (Mpmc)
	HLVM_STATIC_VAR TConcurrentQueue<LoaderRef, EConcurrentQueueMode::Mpmc> s_asyncLoaderQueue;

	//! Set of paths currently loading
	HLVM_STATIC_VAR TSetSmall<FPath> s_pathInLoadingSet;
};

/*
 * Asset loader alias
 */
using FAssLoad = FAssetLoader;
