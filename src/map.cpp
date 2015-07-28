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
#include <boost/bimap.hpp>

#include "FontDriver.hpp"
#include "asserts.hpp"
#include "geometry.hpp"
#include "map.hpp"
#include "profile_timer.hpp"
#include "random.hpp"
#include "terrain.hpp"
#include "utf8_to_codepoint.hpp"
#include "variant_utils.hpp"
#include "visibility.hpp"

extern KRE::ColoredFontRenderablePtr text_block_renderer(const std::vector<std::string>& strs, const std::vector<KRE::Color>& colors, float* ts_x, float* ts_y);

namespace mercy
{
	namespace
	{
		// XX move these and symbols to external file.
		enum class DungeonTile {
			ceiling,
			floor,
			wall,
			door,
			pit,
			lava,
			water,
			perimeter,
		};

		typedef boost::bimap<DungeonTile, char> tile_string_bimap;
		typedef tile_string_bimap::value_type mapped_dungeon_tile;

		tile_string_bimap& get_tile_map()
		{
			static tile_string_bimap res;
			// XXX load from file? hard-coded for present.
			if(res.empty()) {
				res.insert(mapped_dungeon_tile(DungeonTile::ceiling, ' '));
				res.insert(mapped_dungeon_tile(DungeonTile::floor, '.'));
				res.insert(mapped_dungeon_tile(DungeonTile::wall, '#'));
				res.insert(mapped_dungeon_tile(DungeonTile::door, 'D'));
				res.insert(mapped_dungeon_tile(DungeonTile::pit, 'X'));
				res.insert(mapped_dungeon_tile(DungeonTile::lava, '-'));
				res.insert(mapped_dungeon_tile(DungeonTile::water, '~'));
				res.insert(mapped_dungeon_tile(DungeonTile::perimeter, '+'));
			}
			return res;
		}

		char get_symbol_for_tile(DungeonTile t)
		{
			auto it = get_tile_map().left.find(t);
			ASSERT_LOG(it != get_tile_map().left.end(), "Unable to find a mapping for tile of type " << static_cast<int>(t) << " to symbol.");
			return it->get_right();
		}

		DungeonTile get_tile_for_symbol(char c)
		{
			auto it = get_tile_map().right.find(c);
			ASSERT_LOG(it != get_tile_map().right.end(), "Unable to find a mapping for tile symbol " << c << " to type.");
			return it->get_left();
		}

		class DungeonMap : public BaseMap
		{
		public:
			DungeonMap(int width, int height, const variant& features)
				: BaseMap(width, height),
				  tiles_(),
				  dpi_x_(96),
				  dpi_y_(96),
				  start_location_(),
				  renderable_(nullptr),
				  renderable_list_()
			{
				if(features.has_key("dpi_x")) {
					dpi_x_ = features["dpi_x"].as_int32();
				}
				if(features.has_key("dpi_y")) {
					dpi_y_ = features["dpi_y"].as_int32();
				}
			}
			DungeonMap(const variant& node, const variant& features) 
				: BaseMap(node),
				  tiles_(),
				  dpi_x_(96),
				  dpi_y_(96),
				  start_location_(),
				  renderable_(nullptr),
				  renderable_list_()				  
			{
				if(features.has_key("dpi_x")) {
					dpi_x_ = features["dpi_x"].as_int32();
				}
				if(features.has_key("dpi_y")) {
					dpi_y_ = features["dpi_y"].as_int32();
				}
				ASSERT_LOG(node.has_key("tiles") && node["tiles"].is_list(), "No 'tiles' attribute found in dungeon map while loading.");
				ASSERT_LOG(node.has_key("start_location") && node["start_location"].is_list(), "No 'start_location' attribute found in dungeon map while loading.");

				auto tiles = node["tiles"].as_list_string();
				
				tiles_.resize(tiles.size());
				int n = 0;
				for(auto& row : tiles) {
					tiles_[n].resize(row.size(), TileInfo(DungeonTile::ceiling));
					int m = 0;
					for(auto& col : row) {
						tiles_[n][m] = get_tile_for_symbol(col);
						++m;
					}
					++n;
				}
			}
			variant handleWrite() override
			{
				variant_builder res;
				res.add("start_location", start_location_.x);
				res.add("start_location", start_location_.y);
				for(auto& row : tiles_) {
					std::string s;
					for(auto& col : row) {
						s += get_symbol_for_tile(col.type);
					}
					res.add("tiles", s);
				}
				return res.build();
			}
			void update(engine& eng) override
			{
				if(renderable_list_.empty()) {
					recreate_renderable_ = false;
					renderable_ = createRenderable();
					renderable_list_.emplace_back(renderable_);
				} else if(renderable_list_[0] == nullptr) {
					renderable_ = createRenderable();
					renderable_list_[0] = renderable_;
				} else if(recreate_renderable_) {
					recreate_renderable_ = false;
					updateColors();
				}
			}
			const std::vector<KRE::SceneObjectPtr>& getRenderable(const rect&) const override
			{
				return renderable_list_;
			}
			void updateColors()
			{
				profile::manager pman("DungeonMap::updateColors");
				std::vector<KRE::Color> colors;
				for(auto& row : tiles_) {
					KRE::Color color;
					for(auto& col : row) {
						switch(col.type) {
							case DungeonTile::floor:
								color = KRE::Color::colorSaddlebrown();
								break;
							case DungeonTile::wall:
								color = KRE::Color::colorDarkslategrey();
								break;
							case DungeonTile::door:
								color = KRE::Color::colorBrown();
								break;
							case DungeonTile::pit:
								color = KRE::Color::colorBlack();
								break;
							case DungeonTile::lava:
								color = KRE::Color::colorOrange();
								break;
							case DungeonTile::water:
								color = KRE::Color::colorBlue();
								break;
							case DungeonTile::perimeter:
								color = KRE::Color::colorRed();
								break;
							case DungeonTile::ceiling:
							default:  
								break;
						}
						if(col.visibility & 1) {
							color.setAlpha(255);
						} else if(col.visibility & 2) {
							color.setAlpha(128);
						} else {
							color.setAlpha(0);
						}
						colors.emplace_back(color);
					}
				}
				ASSERT_LOG(!renderable_list_.empty() && renderable_list_[0] != nullptr, "Nothing in renderable list.");
				renderable_->updateColors(colors);
			}
			KRE::ColoredFontRenderablePtr createRenderable()
			{
				profile::manager pman("DungeonMap::createRenderable");
				std::vector<KRE::Color> colors;

				std::vector<std::string> transformed_output;
				for(auto& row : tiles_) {
					std::string txf_row;
					KRE::Color color;
					for(auto& col : row) {
						switch(col.type) {
							case DungeonTile::ceiling:
								txf_row += ' ';
								break;
							case DungeonTile::floor:
								txf_row += utils::codepoint_to_utf8(0xb7);
								color = KRE::Color::colorSaddlebrown();
								break;
							case DungeonTile::wall:
								txf_row += '#';
								color = KRE::Color::colorDarkslategrey();
								break;
							case DungeonTile::door:
								txf_row += 'D';
								color = KRE::Color::colorBrown();
								break;
							case DungeonTile::pit:
								txf_row += 'X';
								color = KRE::Color::colorBlack();
								break;
							case DungeonTile::lava:
								txf_row += '~';
								color = KRE::Color::colorOrange();
								break;
							case DungeonTile::water:
								txf_row += '~';
								color = KRE::Color::colorBlue();
								break;
							case DungeonTile::perimeter:
								txf_row += '+';
								color = KRE::Color::colorRed();
								break;
							default: 
								txf_row += '?';
								break;
						}
						if(col.visibility & 1) {
							color.setAlpha(255);
						} else if(col.visibility & 2) {
							color.setAlpha(128);
						} else {
							color.setAlpha(0);
						}
						colors.emplace_back(color);
					}
					transformed_output.emplace_back(txf_row);
				}

				float ts_x, ts_y;
				auto r = text_block_renderer(transformed_output, colors, &ts_x, &ts_y);
				setTileSize(ts_x, ts_y);
				return r;
			}
			void generate(engine& eng) override
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

				tiles_.clear();
				tiles_.resize(map_height);
				for(auto& t : tiles_) {
					t.resize(map_width, TileInfo(DungeonTile::ceiling));
				}
				for(int x = 0; x != map_width; ++x) { 
					tiles_[0][x].type = DungeonTile::perimeter;
					tiles_[map_height-1][x].type = DungeonTile::perimeter;
				}
				for(int y = 0;y != map_height; ++y) { 
					tiles_[y][0] = DungeonTile::perimeter;
					tiles_[y][map_width-1] = DungeonTile::perimeter;
				}
				for(auto& room : rooms) {
					for(int x = room.x1(); x <= room.x2()-1; ++x) {
						tiles_[room.y1()][x].type = DungeonTile::wall;
						tiles_[room.y2()-1][x].type = DungeonTile::wall;
					}
					for(int y = room.y1()+1; y <= room.y2()-1; ++y) {
						tiles_[y][room.x1()].type = DungeonTile::wall;
						tiles_[y][room.x2()-1].type = DungeonTile::wall;
					}

					for(int y = room.y1()+1; y < room.y2()-1; ++y) {
						for(int x = room.x1()+1; x < room.x2()-1; ++x) {
							tiles_[y][x].type = DungeonTile::floor;
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
										tiles_[y][r1.x2()-1].type = DungeonTile::floor;
										tiles_[y][r2.x1()].type = DungeonTile::floor;
										is_connected = true;
									}
								} else if(r1.y2() >= r2.y1() && r1.y1() <= r2.y2()) {
									const int start_y = std::max(r1.y1(), r2.y1())+1;
									const int end_y = std::min(r1.y2(),r2.y2())-1;
									for(int y = start_y; y < end_y; ++y) {
										tiles_[y][r1.x2()-1].type = DungeonTile::floor;
										tiles_[y][r2.x1()].type = DungeonTile::floor;
										is_connected = true;
									}
								}
							} else if(r1.y2() == r2.y1()) {
								if(r1.x1() >= r2.x1() && r1.x1() <= r2.x2()) {
									const int start_x = r1.x1()+1;
									const int end_x = std::min(r2.x2(), r1.x2())-1;
									for(int x = start_x; x < end_x; ++x) {
										tiles_[r1.y2()-1][x].type = DungeonTile::floor;
										tiles_[r2.y1()][x].type = DungeonTile::floor;
										is_connected = true;
									}
								} else if(r1.x2() >= r2.x1() && r1.x1() <= r2.x2()) {
									const int start_x = std::max(r1.x1(), r2.x1())+1;
									const int end_x = std::min(r1.x2(),r2.x2())-1;
									for(int x = start_x; x < end_x; ++x) {
										tiles_[r1.y2()-1][x].type = DungeonTile::floor;
										tiles_[r2.y1()][x].type = DungeonTile::floor;
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

				const int num_nodes = rooms.size();
				Graph g(edges.begin(), edges.end(), weights.data(), num_nodes);
				property_map<Graph, edge_weight_t>::type weightmap = get(edge_weight, g);
				std::vector<graph_traits <Graph>::vertex_descriptor> p(num_vertices(g));
				prim_minimum_spanning_tree(g, &p[0]);
				for (std::size_t i = 0; i != p.size(); ++i) {
					if (p[i] != i) {
						const point p1 = rooms[i].mid();
						const point p2 = rooms[p[i]].mid();
						const int start_x = p1.x < p2.x ? p1.x : p2.x;
						const int end_x   = p1.x < p2.x ? p2.x : p1.x;
						const int start_y = p1.x < p2.x ? p1.y : p2.y;
						const int end_y   = p1.x < p2.x ? p2.y : p1.y;
						for(int x = start_x; x != end_x; ++x) {
							if(tiles_[start_y][x].type == DungeonTile::ceiling) {
								tiles_[start_y][x].type = DungeonTile::floor;
							} else if(tiles_[start_y][x].type == DungeonTile::wall) {
								tiles_[start_y][x].type = DungeonTile::floor;
							}

							if(tiles_[start_y-1][x].type == DungeonTile::ceiling) {
								tiles_[start_y-1][x].type = DungeonTile::wall;
							} 
							if(tiles_[start_y+1][x].type == DungeonTile::ceiling) {
								tiles_[start_y+1][x].type = DungeonTile::wall;
							}
						}
						if(tiles_[start_y-1][end_x].type == DungeonTile::ceiling) {
							tiles_[start_y-1][end_x].type = DungeonTile::wall;
						} 
						if(tiles_[start_y+1][end_x].type == DungeonTile::ceiling) {
							tiles_[start_y+1][end_x].type = DungeonTile::wall;
						}
						if(end_x+1 < map_width) {
							if(tiles_[start_y-1][end_x+1].type == DungeonTile::ceiling) {
								tiles_[start_y-1][end_x+1].type = DungeonTile::wall;
							} 
							if(tiles_[start_y+1][end_x+1].type == DungeonTile::ceiling) {
								tiles_[start_y+1][end_x+1].type = DungeonTile::wall;
							}
						}

						const int y_incr  = start_y < end_y ? 1 : -1;
						for(int y = start_y; y != end_y; y += y_incr) {
							if(tiles_[y][end_x].type == DungeonTile::ceiling) {
								tiles_[y][end_x].type = DungeonTile::floor;
							} else if(tiles_[y][end_x].type == DungeonTile::wall) {
								tiles_[y][end_x].type = DungeonTile::floor;
							}
							if(tiles_[y][end_x-1].type == DungeonTile::ceiling) {
								tiles_[y][end_x-1].type = DungeonTile::wall;
							} 
							if(tiles_[y][end_x+1].type == DungeonTile::ceiling) {
								tiles_[y][end_x+1].type = DungeonTile::wall;
							}
						}
						if(tiles_[end_y][end_x-1].type == DungeonTile::ceiling) {
							tiles_[end_y][end_x-1].type = DungeonTile::wall;
						} 
						if(tiles_[end_y][end_x+1].type == DungeonTile::ceiling) {
							tiles_[end_y][end_x+1].type = DungeonTile::wall;
						}
					}
				}

				chooseStartLocation(rooms);

				LOG_DEBUG("map size: " << map_width << "x" << map_height);
				LOG_DEBUG("rooms built: " << rooms.size());
				LOG_DEBUG("Number of rooms we tried to construct: " << num_rooms);
			}
			void chooseStartLocation(const std::vector<rect>& rooms)
			{
				//XXX there could be lots of ways to choose this really.
				// choose a room/point on a particular side;
				// choose the middle then search around for the nearest valid location.
				// choose the room closet to the middle, etc.
				// For now, we are going to take the centre of the first room on the list.
				start_location_ = rooms.front().mid();
			}
			bool blocksLight(int x, int y) const override
			{
				if(x < 0 || y < 0 || y >= static_cast<int>(tiles_.size())) {
					return true;
				}
				if(x >= static_cast<int>(tiles_[y].size())) {
					return true;
				}
				auto& ti = tiles_[y][x];
				if(ti.type == DungeonTile::floor || ti.type == DungeonTile::pit || ti.type == DungeonTile::lava) {
					return false;
				}
				return true;
			}
			void clearVisible() override 
			{
				recreate_renderable_ = true;
				for(auto& row : tiles_) {
					for(auto& col : row) {
						col.visibility &= ~(1 << 0);
					}
				}
			}
			void handleSetVisible(int x, int y) override
			{
				if(x < 0 || y < 0 || y >= static_cast<int>(tiles_.size()) || x >= static_cast<int>(tiles_[y].size())) {
					return;
				}
				auto& ti = tiles_[y][x];
				ti.visibility |= (1 << 0) | (1 << 1);
			}
			int getDistance(int x, int y) const override
			{
				//return x + y;	// Manhattan distance
				return static_cast<int>(std::sqrt(static_cast<float>(x * x + y * y)));
			}
			bool isWalkable(int x, int y) const
			{
				if(x < 0 || y < 0 || y >= static_cast<int>(tiles_.size()) || x >= static_cast<int>(tiles_[y].size())) {
					return false;
				}
				auto& ti = tiles_[y][x];
				if(ti.type == DungeonTile::floor || ti.type == DungeonTile::pit || ti.type == DungeonTile::lava) {
					return true;
				}
				return false;
			}
			bool isFixedSize() const override 
			{
				return true;
			}
			const point& getStartLocation() const
			{
				return start_location_;
			}
		private:
			struct TileInfo {
				// XXX: visibility should default to 0. Is 2 for testing.
				TileInfo(DungeonTile t) : type(t), visibility(0) {}
				DungeonTile type;
				// XXX codify these with some constants.
				// Visibility information.
				//	bit 0 -- 1 if currently visible, 0 if can't be seen
				//  bit 1 -- 1 if has been seen in the past, 0 if never been seen.
				int visibility;
			};
			std::vector<std::vector<TileInfo>> tiles_;
			int dpi_x_;
			int dpi_y_;
			bool recreate_renderable_ = false;
			point start_location_;
			KRE::ColoredFontRenderablePtr renderable_;
			std::vector<KRE::SceneObjectPtr> renderable_list_;
		};
	}

	BaseMap::BaseMap(int width, int height)
		: width_(width),
		  height_(height),
		  tile_size_(0, 0),
		  visibility_(nullptr),
		  player_visible_tiles_()
	{
		visibility_ = std::make_shared<ShadowCastVisibility>(std::bind(&BaseMap::blocksLight, this, std::placeholders::_1, std::placeholders::_2),
			std::bind(&BaseMap::getDistance, this, std::placeholders::_1, std::placeholders::_2));
	}

	BaseMap::BaseMap(const variant& node)
		: width_(node["width"].as_int32()),
		  height_(node["height"].as_int32()),
		  tile_size_(0, 0),
		  visibility_(nullptr),
		  player_visible_tiles_()
	{
		visibility_ = std::make_shared<ShadowCastVisibility>(std::bind(&BaseMap::blocksLight, this, std::placeholders::_1, std::placeholders::_2),
			std::bind(&BaseMap::getDistance, this, std::placeholders::_1, std::placeholders::_2));
	}

	BaseMap::~BaseMap()
	{
	}

	BaseMapPtr BaseMap::create(const std::string& type, int width, int height, const variant& features)
	{
		if(type == "dungeon") {
			return std::make_shared<DungeonMap>(width, height, features);
		} else if(type == "terrain") {
			return std::make_shared<Terrain>(features);
		} else {
			ASSERT_LOG(false, "unrecognised map type to create.");
		}
		return nullptr;
	}

	void BaseMap::updatePlayerVisibility(const point& pos, int visible_radius)
	{
		std::set<point> visible_tiles;
		visibility_->Compute(pos, visible_radius, [&visible_tiles, this](int x, int y) { 
			visible_tiles.emplace(x, y);
			handleSetVisible(x, y);
		});
		player_visible_tiles_ = visible_tiles;
	}

	std::set<point> BaseMap::getVisibleTilesAt(const point& pos, int visible_radius)
	{
		std::set<point> visible_tiles;
		visibility_->Compute(pos, visible_radius, [&visible_tiles](int x, int y) { 
			visible_tiles.emplace(x, y);
		});
		return visible_tiles;
	}

	std::set<point> BaseMap::getVisibleTilesAt(int x, int y, int visible_radius)
	{
		return getVisibleTilesAt(point(x, y), visible_radius);
	}

	variant BaseMap::write()
	{
		variant v = handleWrite();
		v.as_mutable_map()[variant("width")] = variant(width_);
		v.as_mutable_map()[variant("height")] = variant(height_);
		return v;
	}

	BaseMapPtr BaseMap::load(const variant& node, const variant& features)
	{
		ASSERT_LOG(node.is_map() && node.has_key("type"), "No 'type' attribute found in loading map. " << node.to_debug_string());
		std::string type = node["type"].as_string();
		if(type == "dungeon") {			
			return std::make_shared<DungeonMap>(node, features);
		} else if(type == "terrain") {
			return std::make_shared<Terrain>(node, features);
		} else {
			ASSERT_LOG(false, "unrecognised map type to create.");
		}
		return nullptr;
	}
}
