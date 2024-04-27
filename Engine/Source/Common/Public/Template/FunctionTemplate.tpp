/**
 * Copyright (c) 2024. MIT License. All rights reserved.
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

// template <typename FuncConstruct, typename FuncDestruct, typename... Args>
// class TScopedVariable2
//{
// public:
//	TScopedVariable2() = delete;
//	explicit TScopedVariable2(FuncConstruct&& _Func1, FuncDestruct&& _Func2, Args... _Args)
//		: Values(_Args...), Func2(_Func2)
//	{
//		if constexpr (std::is_convertible_v<decltype(TCallFunc(_Func1, _Args...)), bool>)
//		{
//			HLVM_ENSURE(TCallFunc(_Func1, _Args...), TXT("TScopedVariable2 constructor failed"));
//		}
//		else
//		{
//			TCallFunc(_Func1, _Args...);
//		}
//		HLVM_ATOMIC_THREAD_FENCE();
//	}
//
//	~TScopedVariable2()
//	{
//		HLVM_ATOMIC_THREAD_FENCE();
//		if constexpr (std::is_convertible_v<decltype(TApplyFunc(Func2, Values)), bool>)
//		{
//			HLVM_ENSURE(TApplyFunc(Func2, Values), TXT("TScopedVariable2 constructor failed"));
//		}
//		else
//		{
//			TApplyFunc(Func2, Values);
//		}
//	}
//
// private:
//	FuncDestruct		Func2;
//	std::tuple<Args...> Values;
// };
