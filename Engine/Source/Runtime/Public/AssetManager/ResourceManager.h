/**
* Copyright (c) 2026. MIT License. All rights reserved.
*/

#pragma once
#include "CommonMinimal.h"
#include "Platform/FileSystem/Path.h"
#include "Platform/FileSystem/FileSystem.h"
#include "Core/Parallel/Lock.h"
#include "Template/ReferenceTemplate.tpp"

#include <map>
#include <memory>
#include <future>
#include <functional>
#include <vector>

DECLARE_LOG_CATEGORY(LogResManager)

/**
 * @brief Template-based resource management system with async loading support
 *
 * FResourceManager<T> provides a unified interface for managing resources of type T:
 * - Named resource lookup (string-based)
 * - Path-based deduplication (same file won't be loaded twice)
 * - Synchronous and asynchronous loading
 * - Promise/future pattern for deferred fulfillment
 * - Automatic callback invocation when resources become available
 * - Thread-safe operations with atomic flags and locks
 *
 * Key Features:
 * 1. Resource caching - loaded resources are cached by name and path
 * 2. Duplicate prevention - same path loaded once, shared across requests
 * 3. Async completion - callbacks fire automatically when async loads finish
 * 4. Handle semantics - safe access via Handle wrapper with null checks
 *
 * Thread Safety:
 * - All public methods are thread-safe
 * - Uses FAtomicFlagNC for mutual exclusion
 * - Gather objects use individual locks for fine-grained concurrency
 *
 * Usage Example:
 * @code
 * using MyResource = FTexture;
 * auto* manager = FResourceManager<MyResource>::GetInstance();
 *
 * // Set loader callback
 * manager->SetLoadFromFileFunc([](const FPath& path) {
 *     return MakeShared<MyResource>(path);
 * });
 *
 * // Sync load
 * auto handle = manager->LoadFromFile("name", path);
 * auto* resource = handle.TryGet();
 *
 * // Async load
 * auto handle = manager->LoadFromFileAsync("name", path);
 * if (handle.IsPendingFuture()) {
 *     // Will complete later
 * }
 * auto* resource = handle.WaitGet(); // Blocks until ready
 * @endcode
 *
 * @tparam T Resource type (must be compatible with TSharePtr<T>)
 */
template <typename T>
class FResourceManager : private FAtomicFlagNC
{
public:
	//! Resource type alias
	using ResourcePtr = TSharePtr<T>;
	using ResPtr = ResourcePtr;
	//! Load from file function signature
	using LoadFromFileFunc = std::function<ResPtr(const FPath&)>;

private:
	FResourceManager() = default;
	NOCOPYMOVE(FResourceManager);

	//! Future for async loading result
	using ResourceFuture = std::shared_future<ResourcePtr>;

	//! Callback invoked when resource becomes available
	using ResourceCallback = std::function<void(ResourcePtr&)>;

	//! Promise for fulfilling futures
	using ResourcePromise = std::promise<ResourcePtr>;

	/**
	 * @brief Task handle for managing a single resource
	 *
	 * Wraps either an immediate resource or a future awaiting completion.
	 * Thread-safe with locking guard on all operations.
	 */
	class Gather final : public FAtomicFlag, public FRefCountable
	{
	public:
		Gather() = default;

		//! Convert from ResourcePtr (immediate availability)
		explicit Gather(const ResourcePtr& Ptr, const FPath& Path)
			: ptr(Ptr), path(Path)
		{
		}

		//! Convert from ResourceFuture (async pending)
		explicit Gather(const ResourceFuture& Future, const FPath& Path)
			: future(Future), path(Path)
		{
		}

		// Copy constructor and assignment only
		Gather(const Gather&) = default;
		Gather& operator=(const Gather&) = default;

		//! Set resource directly
		void SetResource(const ResourcePtr& Ptr)
		{
			LOCK_GUARD();
			ptr = Ptr;
			future = std::shared_future<ResourcePtr>(); // Invalidate future
			if (callback && ptr)
			{
				callback(ptr);
			}
		}

		//! Set callback that fires once when resource is available or resource ia already available
		//! Callback persists across copies
		void SetCallback(const ResourceCallback& Callback)
		{
			LOCK_GUARD();
			callback = Callback;
			if (ptr && callback)
			{
				callback(ptr);
			}
		}

		HLVM_NO_DISCARD bool IsResourceValid() const noexcept
		{
			LOCK_GUARD();
			return ptr != nullptr;
		}

		//! Set future from promise
		void SetFuture(const ResourceFuture& Future) noexcept
		{
			LOCK_GUARD();
			ptr.reset();
			future = Future;
		}

		//! Check if future is still valid (not completed/not null)
		HLVM_NO_DISCARD bool IsFutureValid() const noexcept
		{
			LOCK_GUARD();
			return future.valid();
		}

		//! Reset pointer but not future or path
		void Reset() noexcept
		{
			LOCK_GUARD();
			ptr.reset();
		}

		//! Set path directly
		void SetPath(const FPath& Path)
		{
			path = Path;
		}

		//! Get path
		HLVM_NO_DISCARD const FPath& GetPath() const
		{
			return path;
		}

		//! Try to get resource non-blocking
		//! Returns nullptr if not ready yet
		HLVM_NO_DISCARD ResourcePtr TryGet()
		{
			LOCK_GUARD();

			if (ptr != nullptr)
			{
				return ptr;
			}

			if (future.valid())
			{
				switch (auto status = future.wait_for(std::chrono::seconds(0)); status)
				{
					case std::future_status::ready:
						if (ptr = future.get(); ptr)
						{
							if (callback)
							{
								callback(ptr);
							}
							// Reset future
							future = std::shared_future<ResourcePtr>();
							return ptr;
						}
						else
						{
							// Log
							HLVM_LOG(LogResManager, err, TXT("Gather Get<{}> failed - future returned nullptr"), TO_TCHAR_CSTR(typeid(T).name()));
							return nullptr;
						}
					case std::future_status::timeout:
					case std::future_status::deferred:
					default:
						return nullptr; // Not ready
				}
			}
			return nullptr;
		}

		//! Get resource blocking until ready
		//! Throws exception if future is invalid or returns nullptr
		HLVM_NO_DISCARD ResourcePtr WaitGet()
		{
			LOCK_GUARD();

			if (ptr != nullptr)
			{
				return ptr;
			}

			if (future.valid())
			{
				if (ptr = future.get(); ptr)
				{
					if (callback)
					{
						callback(ptr);
					}
					// Reset future
					future = std::shared_future<ResourcePtr>();
					return ptr;
				}
				else
				{
					// Log
					HLVM_LOG(LogResManager, critical, TXT("Gather Get<{}> failed - future returned nullptr"), TO_TCHAR_CSTR(typeid(T).name()));
					return nullptr;
				}
			}
			else
			{
				// Log
				HLVM_LOG(LogResManager, critical, TXT("Gather Get<{}> failed - no valid future"), TO_TCHAR_CSTR(typeid(T).name()));
				return nullptr;
			}
		}

		//! Pointer redirector
		T* operator->()
		{
			if (ptr)
			{
				return ptr.get();
			}
			else if (auto _ptr = TryGet(); _ptr)
			{
				return _ptr.get();
			}
			else
			{
				// Log
				HLVM_LOG(LogResManager, debug, TXT("Gather->wait,{}"), *GetPath());
				return WaitGet().get();
			}
		}

	private:
		friend FResourceManager;

		ResourcePtr		 ptr{ nullptr };
		ResourceFuture	 future;
		ResourceCallback callback;
		FPath			 path;
	};

public:
	/**
	 * @brief Handle wrapper for Gather
	 *
	 * Provides shared ownership semantics with automatic cleanup.
	 * Returns empty/safe object when handle is invalid.
	 */
	class Handle
	{
	public:
		Handle() = default;
		//! Implicit construction from shared gather pointer
		Handle(const TRefCountPtr<Gather>& Task)
			: gather(Task)
		{
		}
		//! Explicit nullptr constructor
		Handle(std::nullptr_t)
			: gather(nullptr)
		{
		}
		Handle(const Handle&) = default;
		Handle& operator=(const Handle&) = default;
		Handle(Handle&&) = default;
		Handle& operator=(Handle&&) = default;

		ResourcePtr TryGet() const
		{
			if (gather)
			{
				return gather->TryGet();
			}
			// Log
			HLVM_SEGFAULT_INLINE();
			return nullptr;
		}

		ResourcePtr WaitGet() const
		{
			if (gather)
			{
				return gather->WaitGet();
			}
			// Log
			HLVM_SEGFAULT_INLINE();
			return nullptr;
		}

		const FPath& GetPath() const
		{
			if (gather)
			{
				return gather->GetPath();
			}
			// Log
			HLVM_SEGFAULT_INLINE();
			return FPath::Empty;
		}

		bool IsPendingFuture() const
		{
			return gather && gather->IsFutureValid();
		}

		T* operator->() const
		{
			if (gather)
			{
				return gather->operator->();
			}
			// Log
			HLVM_SEGFAULT_INLINE();
			return nullptr;
		}

		HLVM_NO_DISCARD bool IsValid() const noexcept
		{
			return gather != nullptr;
		}

		//! Boolean check
		HLVM_NO_DISCARD operator bool() const noexcept
		{
			return IsValid();
		}

	private:
		mutable TRefCountPtr<Gather> gather;
	};

	/**
	 * @brief Get singleton instance
	 */
	static TNoNullablePtr<FResourceManager> GetInstance() noexcept
	{
		static FResourceManager inst = FResourceManager();
		return &inst;
	}

	/**
	 * @brief Add resource to manager (replaces existing)
	 */
	Handle AddResource(const FString& Name, const FPath& FilePath, const ResourcePtr& Resource) noexcept
	{
		LOCK_GUARD_NC();

		//! Fulfill any pending promises if there are
		auto it = m_promises.find(Name);
		if (it != m_promises.end())
		{
			auto& promise = it->second;
			promise.set_value(Resource);
			m_promises.erase(it);
		}

		// Override or add
		TRefCountPtr<Gather>* gather = &(m_resources[Name]);
		*gather = TRefCountPtr<Gather>(new Gather(Resource, FilePath));
		m_resources_path[FilePath] = gather;
		return *gather;
	}

	/**
	 * @brief Set custom load-from-file callback (must call before first load)
	 */
	void SetLoadFromFileFunc(const LoadFromFileFunc& Func) noexcept
	{
		LOCK_GUARD_NC();
		m_loadFromFileFunc = Func;
	}

	/**
	 * @brief Check if LoadFromFileFunc has been registered
	 */
	HLVM_NO_DISCARD bool IsLoadFromFileFuncValid() const noexcept
	{
		LOCK_GUARD_NC();
		return static_cast<bool>(m_loadFromFileFunc);
	}

	/**
	 * @brief Load resource synchronously from file path
	 *
	 * @param Name Resource name (string identifier for lookup)
	 * @param FilePath Path to file to load
	 * @return Handle to resource (valid if loaded, null on failure)
	 *
	 * Behavior:
	 * 1. Checks if resource already loaded by name → returns cached handle
	 * 2. Checks if same path loaded under different name → returns existing handle
	 * 3. Calls m_loadFromFileFunc to load from disk
	 * 4. Fulfills any pending promises for this name
	 * 5. Caches resource by name and path
	 *
	 * @note Blocking - does not return until load completes
	 * @note Returns same handle for duplicate requests (caching)
	 * @note Logs error if SetLoadFromFileFunc not called first
	 * @note Logs error if load callback returns nullptr
	 */
	HLVM_NO_DISCARD Handle LoadFromFile(const FString& Name, const FPath& FilePath)
	{
		LOCK_GUARD_NC();

		ResourcePtr Resource;
		//! Check if already loaded by name
		if (auto nameIt = m_resources.find(Name); nameIt != m_resources.end())
		{
			Gather& resRef = *(nameIt->second);
			if (resRef.IsResourceValid() || resRef.IsFutureValid())
			{
				return nameIt->second;
			}
		}

		//! Check if same path already managed under different name
		if (auto pathIt = m_resources_path.find(FilePath); pathIt != m_resources_path.end())
		{
			Gather& resRef = *(*pathIt->second);
			if (resRef.IsResourceValid() || resRef.IsFutureValid())
			{
				return *pathIt->second;
			}
		}

		// Log
		HLVM_LOG(LogResManager, info, TXT("Loading resource {} from {}"), Name.ToTCharCStr(), FilePath.ToTCharCStr());
		if (!m_loadFromFileFunc)
		{
			// Log
			HLVM_LOG(LogResManager, critical, TXT("Resource type {} does not have SetLoadFromFileFunc called!"), TCHARSTR(typeid(T).name()));
			return Handle(nullptr);
		}

		Resource = m_loadFromFileFunc(FilePath);
		if (!Resource)
		{
			// Log
			HLVM_LOG(LogResManager, critical, TXT("Failed to load resource at {}"), FilePath.ToTCharCStr());
			return Handle(nullptr);
		}

		//! Fulfill any pending promises if there are
		auto it = m_promises.find(Name);
		if (it != m_promises.end())
		{
			auto& promise = it->second;
			promise.set_value(Resource);
			m_promises.erase(it);
		}

		// Override or add
		TRefCountPtr<Gather>* gather = &(m_resources[Name]);
		*gather = TRefCountPtr<Gather>(new Gather(Resource, FilePath));
		m_resources_path[FilePath] = gather;
		return *gather;
	}

	/**
	 * @brief Load resource asynchronously from file path
	 *
	 * @param Name Resource name (string identifier for lookup)
	 * @param FilePath Path to file to load
	 * @return Handle to resource (may be pending if async)
	 *
	 * Behavior:
	 * 1. Checks if resource already loaded → returns cached handle
	 * 2. Checks if same path loaded under different name → returns existing handle
	 * 3. Spawns std::async task to load from disk
	 * 4. Returns immediately with handle to pending future
	 * 5. When async completes, resource cached by name and path
	 *
	 * @return Handle Handle to resource (check IsPendingFuture() to see if still loading)
	 *
	 * @note Non-blocking - returns immediately
	 * @note Uses std::async with std::launch::async
	 * @note Handle can be polled via TryGet() or blocked via WaitGet()
	 * @note Logs error if SetLoadFromFileFunc not called first
	 */
	HLVM_NO_DISCARD Handle LoadFromFileAsync(const FString& Name, const FPath& FilePath)
	{
		LOCK_GUARD_NC();

		//! Check if already loaded
		if (auto nameIt = m_resources.find(Name); nameIt != m_resources.end())
		{
			Gather& resRef = *(nameIt->second);
			if (resRef.IsResourceValid() || resRef.IsFutureValid())
			{
				return nameIt->second;
			}
		}

		//! Check for path collision
		if (auto pathIt = m_resources_path.find(FilePath); pathIt != m_resources_path.end())
		{
			Gather& resRef = *(*pathIt->second);
			if (resRef.IsResourceValid() || resRef.IsFutureValid())
			{
				return *pathIt->second;
			}
		}

		// Log
		HLVM_LOG(LogResManager, info, TXT("Loading resource {} from {}"), Name.ToTCharCStr(), FilePath.ToTCharCStr());
		if (!m_loadFromFileFunc)
		{
			// Log
			HLVM_LOG(LogResManager, critical, TXT("Resource type {} does not have SetLoadFromFileFunc called!"), TCHARSTR(typeid(T).name()));
			return nullptr;
		}

		//! Launch async loading
		Gather* gather = new Gather(std::async(std::launch::async, [this, FilePath]() -> ResourcePtr {
			const auto Resource = m_loadFromFileFunc(FilePath);
			if (!Resource)
			{
				// Log
				HLVM_LOG(LogResManager, critical, TXT("Failed to load resource at {}"), FilePath.ToTCharCStr());
				return nullptr;
			}
			return Resource;
		}).share(),
			FilePath);

		TRefCountPtr<Gather>* _gather = &(m_resources[Name]);
		*_gather = TRefCountPtr<Gather>(gather);
		return *_gather;
	}

	/**
	 * @brief Try to get resource by name
	 *
	 * If not found but path exists, reloads automatically.
	 * If neither found nor path, creates promise/future combo that can be fulfilled later.
	 */
	HLVM_NO_DISCARD Handle TryGet(const FString& Name)
	{
		LOCK_GUARD_NC();

		if (auto it = m_resources.find(Name); it != m_resources.end())
		{
			Gather& resRef = *(it->second);
			if (resRef.IsResourceValid() || resRef.IsFutureValid())
			{
				return it->second;
			}

			//! Path exists - trigger reload
			if (!resRef.GetPath().empty())
			{
				HLVM_LOG(LogResManager, info, TXT("Reloading resource {} from {}"), TO_TCHAR_CSTR(Name.c_str()), resRef.GetPath().ToTCharCStr());
				if (!m_loadFromFileFunc)
				{
					// Log
					HLVM_LOG(LogResManager, critical, TXT("Resource type {} does not have SetLoadFromFileFunc called!"), TO_TCHAR_CSTR(typeid(T).name()));
					return Handle();
				}

				resRef.SetFuture(std::async(std::launch::async, [this, filePath = resRef.GetPath(), Name]() -> ResourcePtr {
					const auto Resource = m_loadFromFileFunc(filePath);
					if (!Resource)
					{
						// Log
						HLVM_LOG(LogResManager, critical, TXT("Failed to reload resource at {}"), filePath.ToTCharCStr());
						return nullptr;
					}
					return Resource;
				}).share());
				return Handle(it->second);
			}

			//! No path - use promise/future pattern
			{
				if (auto promiseIt = m_promises.find(Name); promiseIt != m_promises.end())
				{
					HLVM_LOG(LogResManager, critical, TXT("Resource type {} already has a promise for {}"), TO_TCHAR_CSTR(typeid(T).name()), TO_TCHAR_CSTR(Name.c_str()));
				}
				auto& promise = m_promises[Name];
				resRef.SetFuture(promise.get_future().share());
			}
			return it->second;
		}

		TRefCountPtr<Gather>* gather = &(m_resources[Name]);
		//! New entry - create promise/future
		{
			if (auto promiseIt = m_promises.find(Name); promiseIt != m_promises.end())
			{
				HLVM_LOG(LogResManager, critical, TXT("Resource type {} already has a promise for {}"), TO_TCHAR_CSTR(typeid(T).name()), TO_TCHAR_CSTR(Name.c_str()));
			}
			auto& promise = m_promises[Name];
			*gather = TRefCountPtr<Gather>(new Gather(promise.get_future().share(), FPath()));
		}
		return *gather;
	}

	// Yuhang : Why do we need remove all resource?
	/**
	 * @brief Remove all resources (keeps pending futures intact)
	 */
	void RemoveAllResources() noexcept
	{
		LOCK_GUARD_NC();
		m_resources.clear();
		m_resources_path.clear();
		m_promises.clear();
	}

	/**
	 * @brief Remove by name or path
	 */
	void Remove(const FString& NameOrPath) noexcept
	{
		LOCK_GUARD_NC();

		if (auto it = m_resources.find(NameOrPath); it != m_resources.end())
		{
			// Log
			HLVM_LOG(LogResManager, info, TXT("Removing resource {}"), NameOrPath.ToTCharCStr());
			it->second->Reset();
			m_resources.erase(it);
		}

		//! Search by path
		if (auto pathIt = m_resources_path.find(NameOrPath); pathIt != m_resources_path.end())
		{
			// Log
			HLVM_LOG(LogResManager, info, TXT("Removing resource at {}"), NameOrPath.ToTCharCStr());
			(*pathIt->second)->Reset();
			m_resources_path.erase(pathIt);
		}

		// Remove promise
		m_promises.erase(NameOrPath);
	}

	/**
	 * @brief Check if resource exists
	 */
	HLVM_NO_DISCARD bool Contains(const FString& NameOrPath) const noexcept
	{
		LOCK_GUARD_NC();

		if (auto it = m_resources.find(NameOrPath); it != m_resources.end())
		{
			return true;
		}

		//! Search by path
		if (auto pathIt = m_resources_path.find(NameOrPath); pathIt != m_resources_path.end())
		{
			return true;
		}
		return false;
	}

	/**
	 * @brief Get all resource names (sorted alphabetically)
	 */
	HLVM_NO_DISCARD std::vector<FString> GetAllNames() const noexcept
	{
		LOCK_GUARD_NC();
		std::vector<FString> Names;
		// TODO
		//		LongMarch_MapKeyToVec(m_resources, Names);
		//		std::sort(Names.begin(), Names.end());
		return Names;
	}

	/**
	 * @brief Get all resource paths
	 */
	HLVM_NO_DISCARD TSet<FPath> GetAllPaths() const noexcept
	{
		LOCK_GUARD_NC();
		TSet<FPath> Paths;
		Paths.reserve(m_resources.Num());
		for (const auto& item : m_resources)
		{
			Paths.emplace(item.second->GetPath());
		}
		return MoveTemp(Paths);
	}

	/**
	 * @brief Get all name-path pairs
	 */
	HLVM_NO_DISCARD TMap<FString, FPath> GetAllNameAndPaths() const noexcept
	{
		LOCK_GUARD_NC();
		TMap<FString, FPath> Pairs;
		for (const auto& item : m_resources)
		{
			Pairs.Add(item.first, item.second->GetPath());
		}
		return MoveTemp(Pairs);
	}

	/**
	 * @brief Get all resources as vector of pointers
	 */
	HLVM_NO_DISCARD TVector<ResourcePtr> GetAllResources() const noexcept
	{
		LOCK_GUARD_NC();
		TVector<ResourcePtr> Resources;
		Resources.Reserve(m_resources.Num());
		for (const auto& item : m_resources)
		{
			if (auto resource = item.second->TryGet(); resource)
			{
				Resources.Add(resource);
			}
		}
		return MoveTemp(Resources);
	}

private:
	LoadFromFileFunc m_loadFromFileFunc;

	//! Map: resource name -> resource gather
	mutable TMap<FString, TRefCountPtr<Gather>> m_resources;
	//! Map: resource path -> resource gather
	mutable TMap<FPath, TNoNullablePtr<TRefCountPtr<Gather>>> m_resources_path;

	//! Map: name -> list of pending promises (for deferred fulfillment)
	mutable TMap<FString, ResourcePromise> m_promises;
};

template <typename T>
using FResMan = FResourceManager<T>;
