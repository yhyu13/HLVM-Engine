/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

#include "Core/Parallel/ParallelDefinition.h"
#include "Core/Log.h"

HLVM_ENUM(EThreadPriority, TUINT8,
	Background, // Background priority thread
	Normal,		// Normal priority thread
	Prioritized // Prioritized thread
);

// https://man7.org/linux/man-pages/man3/pthread_setaffinity_np.3.html
// https://eli.thegreenplace.net/2016/c11-threads-affinity-and-hyperthreading/
HLVM_ENUM(ECoreCapability, TUINT8,
	P1, // Performance 1
	P2, // Performance 2
	M1, // Midcore 1
	M2, // Midcore 2
	E1, // Efficient 1
	E2	// Efficient 2
);

HLVM_ENUM(ECoreType, TUINT8,
	Physical, // Physical core
	Logical	  // Logical core
);

struct FCoreDescription
{
	TUINT32			ID;
	ECoreType		Type{ ECoreType::Logical };
	ECoreCapability Capability{ ECoreCapability::P1 };

	FString ToString() const
	{
		return FString::Format(TXT("Core {}, Type: {}, Capability: {}"),
			ID,
			HLVM_ENUM_TCHAR_STR(Type),
			HLVM_ENUM_TCHAR_STR(Capability));
	}
	HLVM_STATIC_FUNC TVector<FCoreDescription> NLogicalCores(TUINT32 N, ECoreCapability Capability = ECoreCapability::P1)
	{
		TVector<FCoreDescription> Targets{ N };
		for (TUINT32 i = 0; i < N; ++i)
		{
			Targets[i].ID = (i);
			Targets[i].Type = ECoreType::Logical;
			Targets[i].Capability = Capability;
		}
		return Targets;
	}
	HLVM_STATIC_FUNC TVector<FCoreDescription> NPhysicalCores(TUINT32 N, ECoreCapability Capability = ECoreCapability::P1)
	{
		TVector<FCoreDescription> Targets{ N };
		for (TUINT32 i = 0; i < N; ++i)
		{
			Targets[i].ID = (i);
			Targets[i].Type = ECoreType::Physical;
			Targets[i].Capability = Capability;
		}
		return Targets;
	}
};

/**
 * One to One thread affinity mask
 * i.e. each thread is assigned to a corresponding targeted core
 * e.g. Number of threads is 4, and 4 cores are targeted, each thread is assigned to a corresponding targeted core
 */
struct FThreadAffinityMode1
{
	EThreadPriority			  Priority;
	TUINT32					  NumThreads;
	TVector<FCoreDescription> TargetedCores;

	bool IsValid() const
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
 * e.g. Number of threads is 4, and 4 cores are targeted, each thread can be assigned to among all of the 4 cores on OS's command
 */
struct FThreadAffinityMode2
{
	EThreadPriority			  Priority;
	TUINT32					  NumThreads;
	TVector<FCoreDescription> TargetedCores;

	bool IsValid() const
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
 * i.e. each thread is assigned to a specific group of targeted cores
 * e.g. Number of threads is 4, and 4 cores are targeted, thread 0, 1 to 0th, 1th p1 cores, thread 2,3 thread to 2th, 3th m1 cores, etc
 */
struct FThreadAffinityMode3
{
	EThreadPriority													Priority;
	TUINT32															NumThreads;
	TVector<std::pair<TVector<TUINT32>, TVector<FCoreDescription>>> TargetedCores;

	bool IsValid() const
	{
		bool bIsValid = NumThreads > 0 && TargetedCores.size() > 0;
		for (TUINT32 i = 0; i < TargetedCores.size(); ++i)
		{
			bIsValid &= std::all_of(TargetedCores[i].first.begin(), TargetedCores[i].first.end(),
				[this](TUINT32 id) { return id < NumThreads; });
			bIsValid &= TargetedCores[i].second.size() > 0;
		}
		return bIsValid;
	}
	FString ToString() const
	{
		return FString::Format(TXT("Thread affinity mask2, {} Priority {} threads, affinity masks\n{}"),
			HLVM_ENUM_TCHAR_STR(Priority),
			NumThreads,
			FString::Join(TargetedCores, [](const std::pair<TVector<TUINT32>, TVector<FCoreDescription>>& pair) {
				return FString::Format(TXT("threads {}, affinity {}"),
					FString::Join(pair.first, [](const TUINT32& core) { return FString::Format(TXT("{}"), core); }),
					FString::Join(pair.second, [](const FCoreDescription& core) { return core.ToString(); }));
			}));
	}
};

/**
 * Thread affinity mask
 */
class FThreadAffinityMode : public std::variant<FThreadAffinityMode1, FThreadAffinityMode2, FThreadAffinityMode3>
{
public:
	FThreadAffinityMode() = default;
	explicit FThreadAffinityMode(const FThreadAffinityMode1& Mask1)
		: std::variant<FThreadAffinityMode1,
			FThreadAffinityMode2,
			FThreadAffinityMode3>(Mask1)
	{
	}
	explicit FThreadAffinityMode(const FThreadAffinityMode2& Mask2)
		: std::variant<FThreadAffinityMode1,
			FThreadAffinityMode2, FThreadAffinityMode3>(Mask2)
	{
	}
	explicit FThreadAffinityMode(const FThreadAffinityMode3& Mask3)
		: std::variant<FThreadAffinityMode1,
			FThreadAffinityMode2,
			FThreadAffinityMode3>(Mask3)
	{
	}

	/**
	 * Convert to each alternative implicitly
	 */
	operator FThreadAffinityMode1*()
	{
		return std::get_if<0>(this);
	}
	operator FThreadAffinityMode2*()
	{
		return std::get_if<1>(this);
	}
	operator FThreadAffinityMode3*()
	{
		return std::get_if<2>(this);
	}
	operator const FThreadAffinityMode1*() const
	{
		return std::get_if<0>(this);
	}
	operator const FThreadAffinityMode2*() const
	{
		return std::get_if<1>(this);
	}
	operator const FThreadAffinityMode3*() const
	{
		return std::get_if<2>(this);
	}

	bool Valid() const
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

namespace hlvm_private
{
	HLVM_INLINE_VAR FThreadAffinityMode2 AllPhysicalCores{
		.Priority = EThreadPriority::Normal,
		.NumThreads = S_C(TUINT32, std::thread::hardware_concurrency() / HLVM_PLATFORM_SIMT),
		.TargetedCores = FCoreDescription::NPhysicalCores(std::thread::hardware_concurrency() / HLVM_PLATFORM_SIMT)
	};
	HLVM_INLINE_VAR FThreadAffinityMode2 BgTwoPhysicalCores{
		.Priority = EThreadPriority::Background,
		.NumThreads = 2u,
		.TargetedCores = FCoreDescription::NPhysicalCores(2u)
	};
	HLVM_INLINE_VAR FThreadAffinityMode2 AllLogicalCores{
		.Priority = EThreadPriority::Normal,
		.NumThreads = S_C(TUINT32, std::thread::hardware_concurrency()),
		.TargetedCores = FCoreDescription::NLogicalCores(std::thread::hardware_concurrency())
	};
} // namespace hlvm_private

HLVM_INLINE_VAR FThreadAffinityMode AllPhysicalCores{
	hlvm_private::AllPhysicalCores
};

HLVM_INLINE_VAR FThreadAffinityMode BgTwoPhysicalCores{
	hlvm_private::BgTwoPhysicalCores
};

HLVM_INLINE_VAR FThreadAffinityMode AllLogicalCores{
	hlvm_private::AllLogicalCores
};
