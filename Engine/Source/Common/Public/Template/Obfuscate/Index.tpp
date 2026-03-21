/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 */

#pragma once

namespace andrivet
{
	namespace ADVobfuscator
	{
		template <int... I>
		struct Indexes
		{
			using type = Indexes<I..., sizeof...(I)>;
		};

		template <int N>
		struct Make_Indexes
		{
			using type = typename Make_Indexes<N - 1>::type::type;
		};

		template <>
		struct Make_Indexes<0>
		{
			using type = Indexes<>;
		};
	} // namespace ADVobfuscator
} // namespace andrivet
