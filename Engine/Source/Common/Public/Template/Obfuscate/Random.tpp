/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

#include <random>

namespace andrivet
{
	namespace ADVobfuscator
	{

		namespace
		{
			// TODO : Change this to something else in production environment in case source code is leaked
			constexpr int seed = 19640324;
		} // namespace

		// 1988, Stephen Park and Keith Miller
		// "Random Number Generators: Good Ones Are Hard To Find", considered as "minimal standard"
		// Park-Miller 31 bit pseudo-random number generator, implemented with G. Carta's optimisation:
		// with 32-bit math and without division

		template <int N>
		struct MetaRandomGenerator
		{
		private:
			static constexpr unsigned a = 16807;	  // 7^5
			static constexpr unsigned m = 2147483647; // 2^31 - 1

			static constexpr unsigned s = MetaRandomGenerator<N - 1>::value;
			static constexpr unsigned lo = a * (s & 0xFFFF);			// Multiply lower 16 bits by 16807
			static constexpr unsigned hi = a * (s >> 16);				// Multiply higher 16 bits by 16807
			static constexpr unsigned lo2 = lo + ((hi & 0x7FFF) << 16); // Combine lower 15 bits of hi with lo's upper bits
			static constexpr unsigned hi2 = hi >> 15;					// Discard lower 15 bits of hi
			static constexpr unsigned lo3 = lo2 + hi;

		public:
			static constexpr unsigned max = m;
			static constexpr unsigned value = lo3 > m ? lo3 - m : lo3;
		};

		template <>
		struct MetaRandomGenerator<0>
		{
			static constexpr unsigned value = seed;
		};

		// Note: A bias is introduced by the modulo operation.
		// However, I do belive it is neglictable in this case (M is far lower than 2^31 - 1)

		template <int N, int M>
		struct MetaRandom
		{
			static const int value = MetaRandomGenerator<N + 1>::value % M;
		};

	} // namespace ADVobfuscator
} // namespace andrivet
