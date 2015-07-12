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

#include <cstdint>
#include <string>
#include <iomanip>
#include <sstream>
#include <vector>

struct formatter
{
	template<typename T>
	formatter& operator<<(const T& o) {
		stream << o;
		return *this;
	}

	const std::string str() {
		return stream.str();
	}

	const char* c_str() {
		return str().c_str();
	}

	operator std::string() {
		return stream.str();
	}
	std::ostringstream stream;
};

template<> inline formatter& formatter::operator<<(const std::vector<uint8_t>& o) {
	for(auto c : o) {
		if(c < 32 || c > 127) {
			stream << "[" << std::setw(2) << std::setfill('0') << std::hex << int(c) << std::dec << "]";
		} else {
			stream << char(c);
		}
	}
	return *this;
}
