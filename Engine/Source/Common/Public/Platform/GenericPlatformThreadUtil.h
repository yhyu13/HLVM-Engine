/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 */

#pragma once

#include "GenericPlatform.h"
#include "Core/Parallel/Async/AsyncConfig.h"

#include <boost/thread/thread.hpp>

class FGenericPlatformThreadUtil
{
public:
	virtual ~FGenericPlatformThreadUtil() noexcept = default;

	HLVM_INLINE_FUNC HLVM_STATIC_FUNC bool SetThreadsWithAffinity(const TVector<boost::thread*>& Threads, const FThreadAffinityMode& AffinityMode)
	{
		return sInstance->InternalSetThreadsWithAffinity(Threads, AffinityMode);
	}

protected:
	virtual bool InternalSetThreadsWithAffinity(const TVector<boost::thread*>& Threads, const FThreadAffinityMode& AffinityMode) = 0;

	HLVM_STATIC_VAR FGenericPlatformThreadUtil* sInstance;
};
