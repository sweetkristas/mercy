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

#include "FontDriver.hpp"
#include "asserts.hpp"
#include "geometry.hpp"
#include "map.hpp"
#include "profile_timer.hpp"
#include "random.hpp"
#include "utf8_to_codepoint.hpp"

namespace mercy
{
	namespace
	{
		class DungeonMap : public BaseMap
		{
		public:
			DungeonMap(int width, int height, const variant& features)
				: BaseMap(width, height)
			{
				generate();
			}
			KRE::SceneObjectPtr createRenderable() override
			{
				std::vector<std::string> ff;
				//ff.emplace_back("SourceCodePro-Regular");
				ff.emplace_back("square");
				ff.emplace_back("whitrabt");
				ff.emplace_back("monospace");
				const float fs = 10.0f;
				auto fh = KRE::FontDriver::getFontHandle(ff, fs);
				std::vector<point> final_path;
				int y = static_cast<int>(fh->getScaleFactor() * fs);

				std::vector<std::string> transformed_output;
				for(auto& row : output_) {
					std::string txf_row;
					for(auto& col : row) {
						if(col == '#') {
							txf_row += '#';
						} else if(col == ' ') {
							txf_row += ' ';
						} else if(col == '.') {
							txf_row += utils::codepoint_to_utf8(0xb7);
						} else if(col == '+') {
							txf_row += '+';
						} else {
							txf_row += col;
						}
					}
					transformed_output.emplace_back(txf_row);
				}

				std::string concat_op;
				for(auto& op : transformed_output) {
					auto glyph_path = fh->getGlyphPath(op);
					for(auto it = glyph_path.begin(); it != glyph_path.end()-1; ++it) {
						auto& gp = *it;
						final_path.emplace_back(gp.x, gp.y + y);
					}
					concat_op += op;
					y += static_cast<int>(fh->getScaleFactor() * fs);
				}
				return fh->createRenderableFromPath(nullptr, concat_op, final_path);
			}
			void generate() override
			{
				profile::manager pman("DungeonMap::generate");
				const int map_width = getWidth();
				const int map_height = getHeight();

				const int min_room_size = 4;
				const int max_room_size = 12;
	
				// quite the emperical value
				const int num_rooms = (map_width * map_height) / 60;

				std::vector<rect> rooms;
	
				int iterations = 0;
				bool done = false;
				while(!done) { 
					int w = generator::get_uniform_int<int>(min_room_size, max_room_size);
					int h = generator::get_uniform_int<int>(min_room_size, max_room_size);
					int x = generator::get_uniform_int<int>(0, map_width-w);
					int y = generator::get_uniform_int<int>(0, map_height-h);

					rect r(x, y, w, h);
					bool intersects = false;
					for(auto& room : rooms) {
						if(geometry::rects_intersect(r, room)) {
							intersects = true;
							break;
						}
					}
					if(!intersects) {
						rooms.emplace_back(r);
						if(static_cast<int>(rooms.size()) >= num_rooms) {
							done = true;
						}
					}
					if(++iterations >= 1000) {
						done = true;
					}
				}

				output_.clear();
				output_.resize(map_height);
				for(auto& op : output_) {
					op.resize(map_width, ' ');
				}
				for(int x = 0; x != map_width; ++x) { 
					output_[0][x] = '+';
					output_[map_height-1][x] = '+';
				}
				for(int y = 0;y != map_height; ++y) { 
					output_[y][0] = '+';
					output_[y][map_width-1] = '+';
				}
				for(auto& room : rooms) {
					for(int x = room.x1(); x <= room.x2()-1; ++x) {
						output_[room.y1()][x] = '#';
						output_[room.y2()-1][x] = '#';
					}
					for(int y = room.y1()+1; y <= room.y2()-1; ++y) {
						output_[y][room.x1()] = '#';
						output_[y][room.x2()-1] = '#';
					}

					for(int y = room.y1()+1; y < room.y2()-1; ++y) {
						for(int x = room.x1()+1; x < room.x2()-1; ++x) {
							output_[y][x] = '.';
						}
					}
				}

				for(auto it1 = rooms.begin(); it1 != rooms.end(); ++it1) {
					auto& r1 = *it1;
					for(auto it2 = rooms.begin(); it2 != rooms.end(); ++it2) {
						auto& r2 = *it2;
						if(it1 != it2) {
							if(r1.x2() == r2.x1()) {
								if(r1.y1() >= r2.y1() && r1.y1() <= r2.y2()) {
									const int start_y = r1.y1()+1;
									const int end_y = std::min(r2.y2(), r1.y2())-1;
									for(int y = start_y; y < end_y; ++y) {
										output_[y][r1.x2()-1] = '.';
										output_[y][r2.x1()] = '.';
									}
								} else if(r1.y2() >= r2.y1() && r1.y1() <= r2.y2()) {
									const int start_y = std::max(r1.y1(), r2.y1())+1;
									const int end_y = std::min(r1.y2(),r2.y2())-1;
									for(int y = start_y; y < end_y; ++y) {
										output_[y][r1.x2()-1] = '.';
										output_[y][r2.x1()] = '.';
									}
								}
							} else if(r1.y2() == r2.y1()) {
								if(r1.x1() >= r2.x1() && r1.x1() <= r2.x2()) {
									const int start_x = r1.x1()+1;
									const int end_x = std::min(r2.x2(), r1.x2())-1;
									for(int x = start_x; x < end_x; ++x) {
										output_[r1.y2()-1][x] = '.';
										output_[r2.y1()][x] = '.';
									}
								} else if(r1.x2() >= r2.x1() && r1.x1() <= r2.x2()) {
									const int start_x = std::max(r1.x1(), r2.x1())+1;
									const int end_x = std::min(r1.x2(),r2.x2())-1;
									for(int x = start_x; x < end_x; ++x) {
										output_[r1.y2()-1][x] = '.';
										output_[r2.y1()][x] = '.';
									}
								}
							}
						}
					}
				}

				// XXX need to construct a graph of connected rooms here so we can make sure
				// all roooms are reachable.

				//for(auto& op : output_) {
				//	std::cout << op << "\n";
				//}
			}
		private:
			std::vector<std::string> output_;
		};
	}

	BaseMap::BaseMap(int width, int height)
		: width_(width),
		  height_(height)
	{
	}

	BaseMap::~BaseMap()
	{
	}

	BaseMapPtr BaseMap::create(const std::string& type, int width, int height, const variant& features)
	{
		if(type == "dungeon") {
			return std::make_shared<DungeonMap>(width, height, features);
		} else {
			ASSERT_LOG(false, "unrecognised map type to create.");
		}
		return nullptr;
	}
}
