/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#pragma once

#include "Core/Assert.h"
#include "Core/Parallel/ParallelDefinition.h"

template <typename Func, typename... Args>
auto TCallFunc(Func func, Args&&... args)
{
	return std::invoke(func, std::forward<Args>(args)...);
}

template <typename Func, typename... Args>
auto TApplyFunc(Func func, std::tuple<Args...>&& values)
{
	return std::apply(func, std::forward<Args...>(values));
}

template <typename T, typename Count, typename Func>
auto TLowerBound(T&& value, Count count, Func&& func)
{
	T*	  first = &value;
	Count step;
	while (count > 0)
	{
		auto it = first;
		step = count / 2;
		std::advance(*it, step);

		if (func(*it))
		{
			first = std::advance(*it, 1);
			count -= step + 1;
		}
		else
		{
			count = step;
		}
	}
	return first;
}

template <typename FuncConstruct, typename FuncDestruct>
class TScopedVariable
{
public:
	TScopedVariable() = delete;
	explicit TScopedVariable(FuncConstruct&& _Func1, FuncDestruct&& _Func2)
		: Func2(_Func2)
	{
		if constexpr (std::is_convertible_v<decltype(TCallFunc(_Func1)), bool>)
		{
			HLVM_ENSURE(TCallFunc(_Func1), TXT("TScopedVariable constructor failed"));
		}
		else
		{
			TCallFunc(_Func1);
		}
		HLVM_ATOMIC_THREAD_FENCE();
	}

	~TScopedVariable()
	{
		HLVM_ATOMIC_THREAD_FENCE();
		if constexpr (std::is_convertible_v<decltype(TCallFunc(Func2)), bool>)
		{
			HLVM_ENSURE(TCallFunc(Func2), TXT("TScopedVariable constructor failed"));
		}
		else
		{
			TCallFunc(Func2);
		}
	}

private:
	FuncDestruct Func2;
};

#define HLVM_SCOPED_VARIABLE(var, FuncConstruct, FuncDestruct)                                                         \
	TScopedVariable<std::function<void()>, std::function<void()>> TOKENPASTE2LINE(var){ FuncConstruct, FuncDestruct }; \
	HLVM_ATOMIC_THREAD_FENCE()

#define HLVM_SCOPED_VARIABLE1(var, FuncType, FuncConstruct, FuncDestruct)                                                  \
	TScopedVariable<std::function<FuncType>, std::function<FuncType>> TOKENPASTE2LINE(var){ FuncConstruct, FuncDestruct }; \
	HLVM_ATOMIC_THREAD_FENCE()

#define HLVM_SCOPED_VARIABLE2(var, FuncType1, FuncConstruct, FuncType2, FuncDestruct)                                        \
	TScopedVariable<std::function<FuncType1>, std::function<FuncType2>> TOKENPASTE2LINE(var){ FuncConstruct, FuncDestruct }; \
	HLVM_ATOMIC_THREAD_FENCE()
