/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#pragma once

#include "Core/Container/ContainerDefinition.h"

/**
 * @brief Delegate
 * @tparam Args Function arguments
 */
template <typename... Args>
class FDelegate
{
public:
	using FuncType = std::function<void(Args...)>;

	FDelegate() = default;

	void Add(std::function<void(Args...)>&& func)
	{
		functions.push_back(func);
	}

	void Add(const FuncType& func)
	{
		functions.push_back(func);
	}

	bool IsBound() const
	{
		return !functions.empty();
	}

	void Invoke(Args... args)
	{
		for (auto& func : functions)
		{
			func(args...);
		}
	}

private:
	TVector<FuncType, TPMRLowLvl<FuncType>> functions;
};

/**
 * @brief CoreDelegates
 */
class CoreDelegates
{
public:
	/**
	 * @brief OnMallocatorShutdown
	 */
	HLVM_INLINE_VAR HLVM_STATIC_VAR FDelegate<void*> OnMallocatorShutdown;
};
