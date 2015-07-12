/*
	Copyright (C) 2014-2015 by Kristina Simpson <sweet.kristas@gmail.com>
	
	This software is provided 'as-is', without any express or implied
	warranty. In no event will the authors be held liable for any damages
	arising from the use of this software.

	Permission is granted to anyone to use this software for any purpose,
	including commercial applications, and to alter it and redistribute it
	freely, subject to the following restrictions:

	   1. The origin of this software must not be misrepresented; you must not
	   claim that you wrote the original software. If you use this software
	   in a product, an acknowledgement in the product documentation would be
	   appreciated but is not required.

	   2. Altered source versions must be plainly marked as such, and must not be
	   misrepresented as being the original software.

	   3. This notice may not be removed or altered from any source
	   distribution.
*/

#pragma once

#include <random>

namespace generator
{
	std::size_t get_seed();
	void set_seed(std::size_t seed);
	std::size_t generate_seed();

	std::mt19937& get_random_engine();

	template<typename T>
	T get_uniform_int(T mn, T mx)
	{
		auto& re = get_random_engine();
		std::uniform_int_distribution<T> uniform_dist(mn, mx);
		return uniform_dist(re);
	}

	template<typename T>
	T get_uniform_real(T mn, T mx)
	{
		auto& re = get_random_engine();
		std::uniform_real_distribution<T> uniform_dist(mn, mx);
		return uniform_dist(re);
	}
}
