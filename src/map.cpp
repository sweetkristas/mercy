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

#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/prim_minimum_spanning_tree.hpp>

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
				: BaseMap(width, height),
				  dpi_x_(96),
				  dpi_y_(96)
			{
				if(features.has_key("dpi_x")) {
					dpi_x_ = features["dpi_x"].as_int32();
				}
				if(features.has_key("dpi_y")) {
					dpi_y_ = features["dpi_y"].as_int32();
				}
				generate();
			}
			KRE::SceneObjectPtr createRenderable() override
			{
				profile::manager pman("DungeonMap::createRenderable");
				std::vector<std::string> ff;
				ff.emplace_back("SourceCodePro-Regular");
				ff.emplace_back("square");
				ff.emplace_back("whitrabt");
				ff.emplace_back("monospace");

				const int font_size = 10;
				const float fs = static_cast<float>(font_size * dpi_y_) / 72.0f;
				auto fh = KRE::FontDriver::getFontHandle(ff, fs);
				std::vector<point> final_path;
				int y = static_cast<int>(fh->getScaleFactor() * fs);
				std::vector<KRE::Color> colors;
				
				const float ts_x = static_cast<float>(fh->calculateCharAdvance('x') / 65536.0f);
				const float ts_y = fs;
				setTileSize(ts_x, ts_y);

				std::vector<std::string> transformed_output;
				for(auto& row : output_) {
					std::string txf_row;
					for(auto& col : row) {
						if(col == '#') {
							txf_row += '#';
							colors.emplace_back(KRE::Color::colorDarkslategrey());
						} else if(col == ' ') {
							txf_row += ' ';
							colors.emplace_back(KRE::Color::colorWhite());
						} else if(col == '.') {
							txf_row += utils::codepoint_to_utf8(0xb7);
							colors.emplace_back(KRE::Color::colorSaddlebrown());
						} else if(col == '+') {
							txf_row += '+';
							colors.emplace_back(KRE::Color::colorRed());
						} else {
							txf_row += col;
							colors.emplace_back(KRE::Color::colorWhite());
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
				return fh->createColoredRenderableFromPath(nullptr, concat_op, final_path, colors);
			}
			void generate() override
			{
				profile::manager pman("DungeonMap::generate");
				const int map_width = getWidth();
				const int map_height = getHeight();

				const int min_room_size = 5;
				const int max_room_size = 12;
	
				// quite the emperical value
				const int num_rooms = (map_width * map_height) / (min_room_size * max_room_size * 2);

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
					// the more iterations we use the better chance of fitting more rooms in, but comes at a time-cost.
					if(++iterations >= 500) {
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

				// XXX need to construct a graph of connected rooms here so we can make sure
				// all roooms are reachable.
				using namespace boost;
				typedef adjacency_list <vecS, vecS, undirectedS, property<vertex_distance_t, int>, property <edge_weight_t, int>> Graph;
				typedef std::pair<int, int> E;
				std::vector<E> edges;
				std::vector<int> weights;

				for(auto it1 = rooms.begin(); it1 != rooms.end(); ++it1) {
					auto& r1 = *it1;
					for(auto it2 = rooms.begin(); it2 != rooms.end(); ++it2) {
						auto& r2 = *it2;
						if(it1 != it2) {
							bool is_connected = false;
							if(r1.x2() == r2.x1()) {
								if(r1.y1() >= r2.y1() && r1.y1() <= r2.y2()) {
									const int start_y = r1.y1()+1;
									const int end_y = std::min(r2.y2(), r1.y2())-1;
									for(int y = start_y; y < end_y; ++y) {
										output_[y][r1.x2()-1] = '.';
										output_[y][r2.x1()] = '.';
										is_connected = true;
									}
								} else if(r1.y2() >= r2.y1() && r1.y1() <= r2.y2()) {
									const int start_y = std::max(r1.y1(), r2.y1())+1;
									const int end_y = std::min(r1.y2(),r2.y2())-1;
									for(int y = start_y; y < end_y; ++y) {
										output_[y][r1.x2()-1] = '.';
										output_[y][r2.x1()] = '.';
										is_connected = true;
									}
								}
							} else if(r1.y2() == r2.y1()) {
								if(r1.x1() >= r2.x1() && r1.x1() <= r2.x2()) {
									const int start_x = r1.x1()+1;
									const int end_x = std::min(r2.x2(), r1.x2())-1;
									for(int x = start_x; x < end_x; ++x) {
										output_[r1.y2()-1][x] = '.';
										output_[r2.y1()][x] = '.';
										is_connected = true;
									}
								} else if(r1.x2() >= r2.x1() && r1.x1() <= r2.x2()) {
									const int start_x = std::max(r1.x1(), r2.x1())+1;
									const int end_x = std::min(r1.x2(),r2.x2())-1;
									for(int x = start_x; x < end_x; ++x) {
										output_[r1.y2()-1][x] = '.';
										output_[r2.y1()][x] = '.';
										is_connected = true;
									}
								}
							}


							if(!is_connected && it2 < it1) {
								// only add things not already connected.
								const int n = std::distance(rooms.begin(), it1);
								const int m = std::distance(rooms.begin(), it2);
								edges.emplace_back(E(n, m));

								const point p1 = rooms[m].mid();
								const point p2 = rooms[n].mid();
								const int dx = std::abs(p1.x - p2.x);
								const int dy = std::abs(p1.y - p2.y);
								const int len = static_cast<int>(std::sqrt(dx * dx + dy * dy) * 1000.0f);
								weights.emplace_back(len);
							}
						}
					}
				}

				//for(int n = 0; n != edges.size(); ++n) {
				//	std::cout << "edge(" << edges[n].first << "," << edges[n].second << ") -- " << weights[n] << "\n";
				//}

				/*
				for(int n = 0; n != rooms.size(); ++n) {
					for(int m = 0; m != rooms.size(); ++m) {
						if(n != m) {
							edges.emplace_back(E(n, m));

							const point p1 = rooms[m].mid();
							const point p2 = rooms[n].mid();
							const int dx = std::abs(p1.x - p2.x);
							const int dy = std::abs(p1.y - p2.y);
							const int len = static_cast<int>(std::sqrt(dx * dx + dy * dy) * 1000.0f);
							weights.emplace_back(len);
						}
					}					
				}
				*/
				const int num_nodes = rooms.size();
				Graph g(edges.begin(), edges.end(), weights.data(), num_nodes);
				property_map<Graph, edge_weight_t>::type weightmap = get(edge_weight, g);
				std::vector<graph_traits <Graph>::vertex_descriptor> p(num_vertices(g));
				prim_minimum_spanning_tree(g, &p[0]);
				for (std::size_t i = 0; i != p.size(); ++i) {
					if (p[i] != i) {
						//std::cout << "parent[" << i << "] = " << p[i] << std::endl;
						// XXX test draw line from i to [i]
						const point p1 = rooms[i].mid();
						const point p2 = rooms[p[i]].mid();
						const int start_x = p1.x < p2.x ? p1.x : p2.x;
						const int end_x   = p1.x < p2.x ? p2.x : p1.x;
						const int start_y = p1.x < p2.x ? p1.y : p2.y;
						const int end_y   = p1.x < p2.x ? p2.y : p1.y;
						for(int x = start_x; x != end_x; ++x) {
							if(output_[start_y][x] == ' ') {
								output_[start_y][x] = '.';
							} else if(output_[start_y][x] == '#') {
								output_[start_y][x] = '.';
							}

							if(output_[start_y-1][x] == ' ') {
								output_[start_y-1][x] = '#';
							} 
							if(output_[start_y+1][x] == ' ') {
								output_[start_y+1][x] = '#';
							}
						}
						if(output_[start_y-1][end_x] == ' ') {
							output_[start_y-1][end_x] = '#';
						} 
						if(output_[start_y+1][end_x] == ' ') {
							output_[start_y+1][end_x] = '#';
						}
						if(end_x+1 < map_width) {
							if(output_[start_y-1][end_x+1] == ' ') {
								output_[start_y-1][end_x+1] = '#';
							} 
							if(output_[start_y+1][end_x+1] == ' ') {
								output_[start_y+1][end_x+1] = '#';
							}
						}

						const int y_incr  = start_y < end_y ? 1 : -1;
						for(int y = start_y; y != end_y; y += y_incr) {
							if(output_[y][end_x] == ' ') {
								output_[y][end_x] = '.';
							} else if(output_[y][end_x] == '#') {
								output_[y][end_x] = '.';
							}
							if(output_[y][end_x-1] == ' ') {
								output_[y][end_x-1] = '#';
							} 
							if(output_[y][end_x+1] == ' ') {
								output_[y][end_x+1] = '#';
							}
						}
						if(output_[end_y][end_x-1] == ' ') {
							output_[end_y][end_x-1] = '#';
						} 
						if(output_[end_y][end_x+1] == ' ') {
							output_[end_y][end_x+1] = '#';
						}
					} else {
						//std::cout << "parent[" << i << "] = no parent" << std::endl;
					}
				}

				/*int n = 0;
				for(auto& room : rooms) {
					const point p1 = room.mid();
					output_[p1.y][p1.x] = n + '0';
					++n;
				}*/

				//for(auto& op : output_) {
				//	std::cout << op << "\n";
				//}
				LOG_DEBUG("map size: " << map_width << "x" << map_height);
				LOG_DEBUG("rooms built: " << rooms.size());
				LOG_DEBUG("Number of rooms we tried to construct: " << num_rooms);
			}
		private:
			std::vector<std::string> output_;
			int dpi_x_;
			int dpi_y_;
		};
	}

	BaseMap::BaseMap(int width, int height)
		: width_(width),
		  height_(height),
		  tile_size_(0, 0)
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
