/*
	Copyright (C) 2015 by Kristina Simpson <sweet.kristas@gmail.com>
	
	This software is provided 'as-is', without any express or implied
	warranty. In no event will the authors be held liable for any damages
	arising from the use of this software.

	Permission is granted to anyone to use this software for any purpose,
	including commercial applications, and to alter it and redistribute it
	freely, subject to the following restrictions:

	   1. The origin of this software must not be misrepresented; you must not
	   claim that you wrote the original software. If you use this software
	   in a product, an acknowledgment in the product documentation would be
	   appreciated but is not required.

	   2. Altered source versions must be plainly marked as such, and must not be
	   misrepresented as being the original software.

	   3. This notice may not be removed or altered from any source
	   distribution.
*/

#include <functional>

#include "asserts.hpp"
#include "cave.hpp"
#include "profile_timer.hpp"
#include "random.hpp"

namespace mercy
{
	namespace
	{
		enum class CaveSymbols
		{
			WALL,
			FLOOR,
		};

		typedef std::map<CaveSymbols, std::string> symbol_map;

		void init_symbols(symbol_map& symbols)
		{
			symbols[CaveSymbols::WALL] = "#";
			symbols[CaveSymbols::FLOOR] = ".";
		}

		symbol_map& get_symbol_map()
		{
			static symbol_map res;
			if(res.empty()) {
				init_symbols(res);
			}
			return res;
		}
	}

	unsigned process_neighbor(int x, int y, const std::vector<std::vector<CaveSymbols>>& cave, CaveSymbols sym)
	{
		const int w = static_cast<int>(cave[0].size());
		const int h = static_cast<int>(cave.size());
		if(y < 0 || y >= h || x < 0 || x >= w) {
			if(sym == CaveSymbols::WALL) {
				return 1;
			}
			return 0;
		}
		if(cave[y][x] == sym) {
			return 1;
		}
		return 0;
	}

	unsigned count_neighbours(const std::vector<std::vector<CaveSymbols>>& cave, int x, int y, CaveSymbols sym)
	{
		unsigned neighbors = 0;
		neighbors += process_neighbor(x - 1, y - 1, cave, sym);
		neighbors += process_neighbor(x - 1, y + 0, cave, sym);
		neighbors += process_neighbor(x - 1, y + 1, cave, sym);

		neighbors += process_neighbor(x + 0, y - 1, cave, sym);
		neighbors += process_neighbor(x + 0, y + 0, cave, sym);
		neighbors += process_neighbor(x + 0, y + 1, cave, sym);

		neighbors += process_neighbor(x + 1, y - 1, cave, sym);
		neighbors += process_neighbor(x + 1, y + 0, cave, sym);
		neighbors += process_neighbor(x + 1, y + 1, cave, sym);
		return neighbors;
	}

	/*std::vector<std::vector<CaveSymbols>> automata(const std::vector<std::vector<CaveSymbols>>& old_map, const variant& params)
	{
		ASSERT_LOG(params.is_list() || params.is_function(),
			"'thresholds' attribute should be list of functions or singular function. " << params.type_as_string());
		std::vector<std::vector<CaveSymbols>> res(old_map);

		const unsigned w = old_map[0].size();
		const unsigned h = old_map.size();

		std::vector<node_fn> fn_list;
		if(params.is_function()) {
			fn_list.emplace_back(params.as_function());
		} else {
			for(unsigned n = 0; n != params.num_elements(); ++n) {
				fn_list.emplace_back(params[n].as_function());
			}
		}
		
		for(unsigned y = 0; y != h; ++y) {
			for(unsigned x = 0; x != w; ++x) {
				int neighbours = count_neighbours(old_map, x, y, CaveSymbols::WALL);
				bool ord_p = false;
				for(auto& fn : fn_list) {
					ord_p |= fn(variant(neighbours)).as_bool();
				}
				res[y][x] = ord_p ? CaveSymbols::WALL : CaveSymbols::FLOOR;
			}
		}

		return res;
	}

	std::vector<std::string> cave(unsigned width, unsigned height, const variant& params)
	{
		profile::manager cave_gen("cave_gen");
		int threshold = static_cast<int>((params.has_key("threshold") ? params["threshold"].as_float() : 0.40f) * 100.0f);
		ASSERT_LOG(params.has_key("passes"), "params must have a 'passes' attribute.");
		std::vector<std::vector<CaveSymbols>> res;
		res.resize(height);
		
		for(auto& row : res) {
			row.reserve(width);
			for(unsigned x = 0; x != width; ++x) {
				row.emplace_back(generator::get_uniform_int(1, 100) < threshold ? CaveSymbols::WALL : CaveSymbols::FLOOR);
			}
		}
		if(params["passes"].is_list()) {
			for(unsigned n = 0; n != params["passes"].num_elements(); ++n) {
				const auto& pass = params["passes"][n];
				ASSERT_LOG(pass.is_map() && pass.has_key("iterations") && pass.has_key("thresholds"),
					"'passes' must be a map with 'iterations' and 'thresholds' attributes. " << pass.type_as_string());
				for(int it = 0; it != pass["iterations"].as_int(); ++it) {
					res = automata(res, pass["thresholds"]);
				}
			}
		} else {
			const auto& pass = params["passes"];
			ASSERT_LOG(pass.is_map() && pass.has_key("iterations") && pass.has_key("thresholds"),
				"'passes' must be a map with 'iterations' and 'thresholds' attributes. " << pass.type_as_string());
			for(int it = 0; it != pass["iterations"].as_int(); ++it) {				
				res = automata(res, pass["thresholds"]);
			}
		}
		
		std::vector<std::string> output;
		output.reserve(res.size());
		for(auto& row : res) {
			std::string s;
			for(auto& col : row) {
				s += get_symbol_map()[col];
			}
			output.emplace_back(s);
		}
		return output;
	}*/

	std::vector<std::vector<CaveSymbols>> automata_fixed(const std::vector<std::vector<CaveSymbols>>& old_map, std::function<bool(int)> fn)
	{
		std::vector<std::vector<CaveSymbols>> res(old_map);

		const unsigned w = old_map[0].size();
		const unsigned h = old_map.size();
		for(unsigned y = 0; y != h; ++y) {
			for(unsigned x = 0; x != w; ++x) {
				unsigned neighbours = count_neighbours(old_map, x, y, CaveSymbols::WALL);
				res[y][x] = fn(neighbours) ? CaveSymbols::WALL : CaveSymbols::FLOOR;
			}
		}
		return res;
	}

	std::vector<std::string> cave_fixed_param(unsigned width, unsigned height)
	{
		profile::manager cave_gen("cave_gen_fixed_param");
		int threshold = 40;
		std::vector<std::vector<CaveSymbols>> res;
		res.resize(height);

		// Generate a random set of walls and floor
		for(auto& row : res) {
			row.reserve(width);
			for(unsigned x = 0; x != width; ++x) {
				row.emplace_back(generator::get_uniform_int(1, 100) < threshold ? CaveSymbols::WALL : CaveSymbols::FLOOR);
			}
		}

		for(int n = 0; n != 4; ++n) {
			res = automata_fixed(res, [](int count) { return count >= 5 || count < 1; });
		}
		for(int n = 0; n != 2; ++n) {
			res = automata_fixed(res, [](int count) { return count >= 5; });
		}

		// Generate a set of strings mapped as appropriate
		std::vector<std::string> output;
		output.reserve(res.size());
		for(auto& row : res) {
			std::string s;
			for(auto& col : row) {
				s += get_symbol_map()[col];
			}
			output.emplace_back(s);
		}
		return output;
	}
}
