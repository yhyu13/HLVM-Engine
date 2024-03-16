/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

HLVM_ENUM(EAsyncMode, uint8_t,
	ThreadPool, // Add to default work steal thread pool
	Thread		// Startup a new thread
);

HLVM_ENUM(EThreadPriority, uint8_t,
	Background, // Background priority thread
	Normal,		// Normal priority thread
	Prioritized // Prioritized thread
);

// https://man7.org/linux/man-pages/man3/pthread_setaffinity_np.3.html
// https://eli.thegreenplace.net/2016/c11-threads-affinity-and-hyperthreading/
HLVM_ENUM(ECoreCapability, uint8_t,
	P1, // Performance 1
	P2, // Performance 2
	M1, // Midcore 1
	M2, // Midcore 2
	E1, // Efficient 1
	E2	// Efficient 2
);

HLVM_ENUM(ECoreType, uint8_t,
	Physical, // Physical core
	Logical	  // Logical core
);

struct FCoreDescription
{
	uint8_t			ID;
	ECoreType		Type{ ECoreType::Logical };
	ECoreCapability Capability{ ECoreCapability::P1 };

	FString ToString() const
	{
		return FString::Format(TXT("Core {}, Type: {}, Capability: {}"),
			ID,
			HLVM_ENUM_TCHAR_STR(Type),
			HLVM_ENUM_TCHAR_STR(Capability));
	}
};

/**
 * One to One thread affinity mask
 * e.g. each thread is assigned to a corresponding targeted core
 */
struct FThreadAffinityMask1
{
	EThreadPriority			  Priority;
	uint8_t					  NumThreads;
	TVector<FCoreDescription> TargetedCores;
	bool					  IsValid() const
	{
		return NumThreads > 0 && NumThreads == TargetedCores.size();
	}
	FString ToString() const
	{
		return FString::Format(TXT("Thread affinity mask1, {} Priority {} threads, affinity masks\n{}"),
			HLVM_ENUM_TCHAR_STR(Priority),
			NumThreads,
			FString::Join(TargetedCores, [](const FCoreDescription& core) { return core.ToString(); }));
	}
};

/**
 * Many to Many thread affinity mask
 * i.e. each thread can be assigned to any targeted core
 */
struct FThreadAffinityMask2
{
	EThreadPriority			  Priority;
	uint8_t					  NumThreads;
	TVector<FCoreDescription> TargetedCores;
	bool					  IsValid() const
	{
		return NumThreads > 0 && TargetedCores.size() > 0;
	}
	FString ToString() const
	{
		return FString::Format(TXT("Thread affinity mask2, {} Priority {} threads, affinity masks\n{}"),
			HLVM_ENUM_TCHAR_STR(Priority),
			NumThreads,
			FString::Join(TargetedCores, [](const FCoreDescription& core) { return core.ToString(); }));
	}
};

/**
 * Specific thread ids to Specific thread affinity mask
 * i.e. thread 0, 1 to 0th, 1th p1 cores, 2,3 thread to 2th, 3th m1 cores, etc
 */
struct FThreadAffinityMask3
{
	EThreadPriority													Priority;
	uint8_t															NumThreads;
	TVector<std::pair<TVector<uint8_t>, TVector<FCoreDescription>>> TargetedCores;
	bool															IsValid() const
	{
		bool bIsValid = NumThreads > 0 && TargetedCores.size() > 0;
		for (uint8_t i = 0; i < TargetedCores.size(); ++i)
		{
			bIsValid &= std::all_of(TargetedCores[i].first.begin(), TargetedCores[i].first.end(),
				[this](uint8_t id) { return id < NumThreads; });
			bIsValid &= TargetedCores[i].second.size() > 0;
		}
		return bIsValid;
	}
	FString ToString() const
	{
		return FString::Format(TXT("Thread affinity mask2, {} Priority {} threads, affinity masks\n{}"),
			HLVM_ENUM_TCHAR_STR(Priority),
			NumThreads,
			FString::Join(TargetedCores, [](const std::pair<TVector<uint8_t>, TVector<FCoreDescription>>& pair) {
				return FString::Format(TXT("threads {}, affinity {}"),
					FString::Join(pair.first, [](const uint8_t& core) { return FString::Format(TXT("{}"), core); }),
					FString::Join(pair.second, [](const FCoreDescription& core) { return core.ToString(); }));
			}));
	}
};

/**
 * Thread affinity mask
 */
class FThreadAffinityMask : public std::variant<FThreadAffinityMask1, FThreadAffinityMask2, FThreadAffinityMask3>
{
public:
	FThreadAffinityMask() = delete;
	explicit FThreadAffinityMask(const FThreadAffinityMask1& Mask1)
		: std::variant<FThreadAffinityMask1,
			FThreadAffinityMask2,
			FThreadAffinityMask3>(Mask1)
	{
	}
	explicit FThreadAffinityMask(const FThreadAffinityMask2& Mask2)
		: std::variant<FThreadAffinityMask1,
			FThreadAffinityMask2, FThreadAffinityMask3>(Mask2)
	{
	}
	explicit FThreadAffinityMask(const FThreadAffinityMask3& Mask3)
		: std::variant<FThreadAffinityMask1,
			FThreadAffinityMask2,
			FThreadAffinityMask3>(Mask3)
	{
	}

	bool IsValid() const
	{
		if (const auto* val = std::get_if<0>(this))
		{
			return val->IsValid();
		}
		if (const auto* val = std::get_if<1>(this))
		{
			return val->IsValid();
		}
		if (const auto* val = std::get_if<2>(this))
		{
			return val->IsValid();
		}
		return false;
	}
	FString ToString() const
	{
		if (const auto* val = std::get_if<0>(this))
		{
			return val->ToString();
		}
		if (const auto* val = std::get_if<1>(this))
		{
			return val->ToString();
		}
		if (const auto* val = std::get_if<1>(this))
		{
			return val->ToString();
		}
		return FString{};
	}
};
