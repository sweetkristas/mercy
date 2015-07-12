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

#include "asserts.hpp"
#include "random.hpp"

namespace generator
{
	namespace
	{
		bool seed_set = false;
		std::size_t seed_internal = 0;
	}

	std::mt19937& get_random_engine()
	{
		static std::mt19937 random_engine(seed_internal);
		ASSERT_LOG(seed_set, "No seed set");
		return random_engine;
	}

	std::size_t get_seed()
	{
		return seed_internal;
	}

	void set_seed(std::size_t seed)
	{
		seed_internal = seed;
		seed_set = true;
	}

	std::size_t generate_seed()
	{
		seed_internal = std::default_random_engine()();
		seed_set = true;
		return seed_internal;
	}
}
