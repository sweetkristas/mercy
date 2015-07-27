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

#include <codecvt>
#include <iostream>
#include <locale>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <boost/filesystem.hpp>

#include "asserts.hpp"
#include "filesystem.hpp"

namespace sys
{
	using namespace boost::filesystem;

	bool file_exists(const std::string& name)
	{
		path p(name);
		return exists(p) && is_regular_file(p);
	}

	std::string read_file(const std::string& name)
	{
		path p(name);
		ASSERT_LOG(exists(p), "Couldn't read file: " << name);
		std::ifstream file(p.generic_wstring(), std::ios_base::binary);
		std::stringstream ss;
		ss << file.rdbuf();
		return ss.str();
	}

	void write_file(const std::string& name, const std::string& data)
	{
		path p(name);
		ASSERT_LOG(p.is_absolute() == false, "Won't write absolute paths: " << name);
		ASSERT_LOG(p.has_filename(), "No filename found in write_file path: " << name);

		// Create any needed directories
		create_directories(p.parent_path());

		// Write the file.
		std::ofstream file(name, std::ios_base::binary);
		file << data;
	}

	std::string wstring_to_string(const std::wstring& ws)
	{
		typedef std::codecvt_utf8<wchar_t> convert_type;
		std::wstring_convert<convert_type, wchar_t> converter;
		return converter.to_bytes(ws);
	}

	void get_unique_files(const std::string& name, file_path_map& fpm)
	{
		path p(name);
		if(exists(p)) {
			ASSERT_LOG(is_directory(p) || is_other(p), "get_unique_files() not directory: " << name);
			for(auto it = recursive_directory_iterator(p); it != recursive_directory_iterator(); ++it) {
				if(is_regular_file(it->path())) {
					std::string fn = wstring_to_string(it->path().filename().generic_wstring());
					std::string fp = wstring_to_string(it->path().generic_wstring());
					fpm[fn] = fp;
				}
			}
		} else {
			std::cerr << "WARNING: path " << p.generic_string() << " doesn't exit" << std::endl;
		}
	}
}