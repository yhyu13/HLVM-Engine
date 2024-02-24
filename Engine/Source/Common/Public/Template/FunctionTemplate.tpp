/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

#include "Core/Assert.h"

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

template <typename FuncConstruct, typename FuncDestruct, typename... Args>
class TConstructorTrick
{
public:
	TConstructorTrick() = delete;
	explicit TConstructorTrick(const FuncConstruct& _Func1, const FuncDestruct& _Func2, Args... _Args)
		: Func2(_Func2)
	{
		if constexpr (std::is_convertible_v<decltype(TCallFunc(_Func1, _Args...)), bool>)
		{
			HLVM_ENSURE(TCallFunc(_Func1, _Args...), TXT("TConstructorTrick constructor failed"));
		}
		else
		{
			TCallFunc(_Func1, _Args...);
		}
	}

	~TConstructorTrick()
	{
		if constexpr (std::is_convertible_v<decltype(TCallFunc(Func2)), bool>)
		{
			HLVM_ENSURE(TCallFunc(Func2), TXT("TConstructorTrick constructor failed"));
		}
		else
		{
			TCallFunc(Func2);
		}
	}

private:
	FuncDestruct Func2;
};

template <typename FuncConstruct, typename FuncDestruct, typename... Args>
class TConstructorTrick2
{
public:
	TConstructorTrick2() = delete;
	explicit TConstructorTrick2(const FuncConstruct& _Func1, const FuncDestruct& _Func2, Args... _Args)
		: Values(_Args...), Func2(_Func2)
	{
		if constexpr (std::is_convertible_v<decltype(TCallFunc(_Func1, _Args...)), bool>)
		{
			HLVM_ENSURE(TCallFunc(_Func1, _Args...), TXT("TConstructorTrick constructor failed"));
		}
		else
		{
			TCallFunc(_Func1, _Args...);
		}
	}

	~TConstructorTrick2()
	{
		if constexpr (std::is_convertible_v<decltype(TApplyFunc(Func2, Values)), bool>)
		{
			HLVM_ENSURE(TApplyFunc(Func2, Values), TXT("TConstructorTrick constructor failed"));
		}
		else
		{
			TApplyFunc(Func2, Values);
		}
	}

private:
	FuncDestruct		Func2;
	std::tuple<Args...> Values;
};
