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

#include <algorithm>

#include <boost/functional/hash.hpp>
#include <noise/noise.h>
#include "noiseutils.h"

#include "asserts.hpp"
#include "component.hpp"
#include "engine.hpp"
#include "random.hpp"
#include "terrain.hpp"
#include "variant_utils.hpp"

#include "FontDriver.hpp"

extern KRE::ColoredFontRenderablePtr text_block_renderer(const std::vector<std::string>& strs, const std::vector<KRE::Color>& colors, float* ts_x, float* ts_y);

namespace mercy
{
	namespace
	{
		const double terrain_scale_factor = 8.0;

		class terrain_data
		{
		public:
			terrain_data() {}
			void sort_tiles() {
				std::stable_sort(tiles_.begin(), tiles_.end());
			}
			const pointf& get_tile_size() const { return tile_size_; }
			void set_tile_size(const pointf& p) { tile_size_ = p; }
			terrain_tile_ptr getTileAtHeight(float value) const 
			{
				if(value >= tiles_.back()->get_threshold()) {
					return tiles_.back();
				}
				for(auto& t : tiles_) {
					if(value < t->get_threshold()) {
						return t;
					}
				}
				ASSERT_LOG(false, "No valid terrain value found for value: " << value);
				return nullptr;
			}
			bool is_higher_priority(TerrainType first, TerrainType second) const 
			{
				// is priority(first) > priority(second)
				for(const auto& priority : tile_priority_list_) {
					if(priority == first) {
						return true;
					} else if(priority == second) {
						return false;
					}
				}
				ASSERT_LOG(false, "Did not match terrain type " << first << " or " << second << " in priority list.");
				return false;
			}
			void load(const variant& n) {
				ASSERT_LOG(n.has_key("tiles") && n["tiles"].is_map(),
					"terrain data must have 'tiles' attribute that is a map.");
				auto& tiles = n["tiles"].as_map();

				tiles_.resize(tiles.size());

				TerrainType counter = 0;
				for(const auto& t : tiles) {
					const auto& name = t.first.as_string();
					ASSERT_LOG(t.second.has_key("symbol"), "tiles must have 'symbol' attribute");
					const auto& symbol = t.second["symbol"].as_string();
					ASSERT_LOG(t.second.has_key("gradient"), "tiles must have 'gradient' attribute");
					const auto& gradient = t.second["gradient"].as_float();
					ASSERT_LOG(t.second.has_key("color"), "tiles must have 'color' attribute");
					auto ntile = std::make_shared<terrain_tile>(gradient, counter, name, symbol, KRE::Color(t.second["color"]));
					if(t.second.has_key("walkable")) {
						ntile->setWalkable(t.second["walkable"].as_bool());
					}
					tiles_[counter] = ntile;
					tile_name_map_[ntile->get_name()] = ntile->get_terrain_type();
					++counter;
				}

				sort_tiles();

				// Load tile priorities
				// 'tile_priority' is a list of tile types of what looks better when two
				// tiles are placed next to each other.
				// e.g. jungle would look better when next to grass if the jungle encroached
				// the grass tile. i.e. provided the transitions.
				ASSERT_LOG(n.has_key("tile_priority"), "No 'tile_priority' attribute found in terrain data.");
				for(auto& s : n["tile_priority"].as_list_string()) {
					auto it = tile_name_map_.find(s);
					ASSERT_LOG(it != tile_name_map_.end(), "Couldn't find tile named '" << s << "' in the list of terrain tiles.");
					tile_priority_list_.emplace_back(it->second);
				}
			}
			const std::vector<TerrainType>& get_priority_list() const { return tile_priority_list_; }
			terrain_tile_ptr get_tile_from_terrain(TerrainType tt) {
				ASSERT_LOG(tt >= 0 && static_cast<unsigned>(tt) < tiles_.size(), "TerrainType exceeds internal terrain data. " << tt);
				return tiles_[tt];
			}
		private:
			pointf tile_size_;
			std::vector<terrain_tile_ptr> tiles_;
			// Map from name to terrain type for the tile.
			std::map<std::string, TerrainType> tile_name_map_;
			// List of terrain priorities. 
			// i.e. if we two tiles next to each other which one overlaps
			// the other.
			std::vector<TerrainType> tile_priority_list_;
			};

		terrain_data& get_terrain_data()
		{
			static terrain_data res;
			return res;
		}
	}

	terrain_tile::terrain_tile(float threshold, TerrainType tt, const std::string& name, const std::string& symbol, const KRE::Color& color)
		: threshold_(threshold), 
		  terrain_type_(tt), 
		  name_(name),
		  symbol_(symbol),
		  color_(color),
		  is_walkable_(false)
		{
		}


	chunk::chunk(const point&pos, int width, int height)
		: pos_(pos),
		  width_(width),
		  height_(height)
	{
		terrain_.resize(height);
		for(auto& row : terrain_) {
			row.resize(width);
		}
	}

	terrain_tile_ptr chunk::get_at(int x, int y)
	{
		ASSERT_LOG(x < width_, "x exceeds width of chunk: " << x << " >= " << width_);
		ASSERT_LOG(y < height_, "y exceeds height of chunk: " << y << " >= " << height_);
		return terrain_[y][x];
	}

	TerrainType chunk::get_terrain_at(int x, int y)
	{
		ASSERT_LOG(x < width_, "x exceeds width of chunk: " << x << " >= " << width_);
		ASSERT_LOG(y < height_, "y exceeds height of chunk: " << y << " >= " << height_);
		return terrain_[y][x]->get_terrain_type();
	}

	void chunk::set_at(int x, int y, float height_value)
	{
		ASSERT_LOG(x < width_, "x exceeds width of chunk: " << x << " >= " << width_);
		ASSERT_LOG(y < height_, "y exceeds height of chunk: " << y << " >= " << height_);
		terrain_[y][x] = get_terrain_data().getTileAtHeight(height_value);
	}

	KRE::SceneObjectPtr chunk::make_renderable_from_chunk(chunk_ptr chk)
	{
		auto& cache = get_terrain_data();
		auto& tile_size = cache.get_tile_size();
		int w = tile_size.x * chk->width();
		int h = tile_size.y * chk->height();

		float ts_x, ts_y;
		std::vector<std::string> op(chk->height(), std::string());
		std::vector<KRE::Color> colors;
		for(int y = 0; y != chk->height(); ++y) {			
			for(int x = 0; x != chk->width(); ++x) {
				op[y] += chk->get_at(x, y)->get_symbol();
				colors.emplace_back(chk->get_at(x, y)->get_color());
			}
		}
		
		auto r = text_block_renderer(op, colors, &ts_x, &ts_y);
		get_terrain_data().set_tile_size(pointf(ts_x, ts_y));
		return r;
	}

	terrain_tile_ptr Terrain::getTileAt(const point& p)
	{
		int cw2 = chunk_size_w_/2;
		int ch2 = chunk_size_h_/2;
		// position of chunk
		point cpos(p.x > 0 ? (p.x+cw2)/cw2-1 : (p.x+cw2)/cw2, p.y > 0 ? (p.y+ch2)/ch2-1 : (p.y+ch2)/ch2);
		// position in chunk
		point pos(p.x % cw2 + cw2, p.y % ch2 + ch2);
		ASSERT_LOG(pos.x >= 0 && pos.x < chunk_size_w_, "x coordinate outside of chunk bounds. 0 <= " << pos.x << " < " << chunk_size_w_);
		ASSERT_LOG(pos.y >= 0 && pos.y < chunk_size_h_, "y coordinate outside of chunk bounds. 0 <= " << pos.y << " < " << chunk_size_h_);
		auto it = chunks_.find(cpos);
		if(it == chunks_.end()) {
			// is no an existing chunk, create a new one
			auto nchunk = generate_terrain_chunk(cpos);
			return nchunk->get_at(pos.x, pos.y);
		}
		return it->second->get_at(pos.x, pos.y);
	}

	namespace {
		std::vector<point>& get_edge_offsets()
		{
			static std::vector<point> res;
			if(res.empty()) {
				res.emplace_back(point(-1,0));	// west
				res.emplace_back(point(0,-1));	// north
				res.emplace_back(point(1,0));	// east
				res.emplace_back(point(0,1));	// south
			}
			return res;
		}
		std::vector<point>& get_corner_offsets()
		{
			static std::vector<point> res;
			if(res.empty()) {
				res.emplace_back(point(-1,-1));	// north-west
				res.emplace_back(point(1,-1));	// north-east
				res.emplace_back(point(1,1));	// south-east
				res.emplace_back(point(-1,1));	// south-west
			}
			return res;
		}

		const std::vector<point>& get_direction_offsets()
		{
			static std::vector<point> res;
			if(res.empty()) {
				res.emplace_back(point(-1,0));	// west
				res.emplace_back(point(0,-1));	// north
				res.emplace_back(point(1,0));	// east
				res.emplace_back(point(0,1));	// south
				res.emplace_back(point(-1,-1));	// north-west
				res.emplace_back(point(1,-1));	// north-east
				res.emplace_back(point(1,1));	// south-east
				res.emplace_back(point(-1,1));	// south-west
			}
			return res;
		}
	}

	Terrain::Terrain(const variant& features)
		: BaseMap(-1, -1),
		  chunk_size_w_(16),
		  chunk_size_h_(16),
		  chunks_(),
		  terrain_seed_(generator::get_uniform_int(0, std::numeric_limits<int>::max())),
		  start_location_()
	{
	}

	Terrain::Terrain(const variant& node, const variant& features)
		: BaseMap(node),
		  chunk_size_w_(16),
		  chunk_size_h_(16),
		  chunks_(),
		  terrain_seed_(0),
		  start_location_()
	{
		ASSERT_LOG(node.has_key("seed"), "No seed value for terrain was found.");
		terrain_seed_ = node["seed"].as_int32();
		ASSERT_LOG(node.has_key("start_location"), "No starting location found.");
		start_location_ = variant_to_point(node["start_location"]);
		ASSERT_LOG(node.has_key("chunk_size"), "No terrain chunk_size attribute found.");
		point cs = variant_to_point(node["chunk_size"]);
		chunk_size_w_ = cs.x;
		chunk_size_h_ = cs.y;
		// XXX load chunks
	}

	void Terrain::load_terrain_data(const variant& n)
	{
		get_terrain_data().load(n);
	}

	std::vector<chunk_ptr> Terrain::get_chunks_in_area(const rect& r)
	{
		// We assume that chunks are placed on a grid. So that 0,0 is the co-ordinates of the 
		// center of the first chunk, the chunk one up/one right would be at chunk_size_w_,chunk_size_h_
	
		std::vector<chunk_ptr> res;
		
		// So first step, we create a new rectangle based on our grid that is normalised to cover the rectangle r.
		rect r_norm = rect::from_coordinates((r.x() - chunk_size_w_/2) / chunk_size_w_,
			(r.y() - chunk_size_h_/2) / chunk_size_h_,
			(r.x2() + chunk_size_w_/2) / chunk_size_w_,
			(r.y2() + chunk_size_h_/2) / chunk_size_h_);
		// Next iterate over the grid positions and get all the relevant chunks
		for(int y = r_norm.y(); y < r_norm.y2(); ++y) {
			for(int x = r_norm.x(); x < r_norm.x2(); ++x) {
				point pos(x * chunk_size_w_, y * chunk_size_h_);
				auto it = chunks_.find(pos);
				if(it == chunks_.end()) {
					auto nchunk = generate_terrain_chunk(pos);
					res.emplace_back(nchunk);
				} else {
					res.emplace_back(it->second);
				}				
			}
		}
		// XXX Need to add some chunk unloading logic.
		return res;
	}

	chunk_ptr Terrain::generate_terrain_chunk(const point& pos)
	{
		using namespace noise;
		module::Perlin pnoise;
		pnoise.SetSeed(terrain_seed_);
		chunk_ptr nchunk = std::make_shared<chunk>(pos, chunk_size_w_, chunk_size_h_);
		for(int y = 0; y < chunk_size_h_; ++y) {
			for(int x = 0; x < chunk_size_w_; ++x) {
				auto ns = pnoise.GetValue(
					(x + pos.x - chunk_size_w_ / 2.0) / (chunk_size_w_ * terrain_scale_factor),
					(y + pos.y - chunk_size_h_ / 2.0) / (chunk_size_h_ * terrain_scale_factor),
					0.0);
				nchunk->set_at(x, y, static_cast<float>(ns));
			}
		}
		nchunk->set_renderable(chunk::make_renderable_from_chunk(nchunk));
		auto& ts = get_terrain_data().get_tile_size();
		nchunk->get_renderable()->setPosition(pos.x * ts.x, pos.y * ts.y);
		chunks_[pos] = nchunk;
		return nchunk;
	}

	terrain_tile_ptr Terrain::getTileAt(const point& p) const
	{
		int cw2 = chunk_size_w_/2;
		int ch2 = chunk_size_h_/2;
		// position of chunk
		point cpos(p.x > 0 ? (p.x+cw2)/cw2-1 : (p.x+cw2)/cw2, p.y > 0 ? (p.y+ch2)/ch2-1 : (p.y+ch2)/ch2);
		// position in chunk
		point pos(p.x % cw2 + cw2, p.y % ch2 + ch2);
		ASSERT_LOG(pos.x >= 0 && pos.x < chunk_size_w_, "x coordinate outside of chunk bounds. 0 <= " << pos.x << " < " << chunk_size_w_);
		ASSERT_LOG(pos.y >= 0 && pos.y < chunk_size_h_, "y coordinate outside of chunk bounds. 0 <= " << pos.y << " < " << chunk_size_h_);
		auto it = chunks_.find(cpos);
		if(it == chunks_.end()) {
			// is no an existing chunk, return null
			return nullptr;
		}
		return it->second->get_at(pos.x, pos.y);
	}

	pointf Terrain::get_terrain_size()
	{
		return get_terrain_data().get_tile_size();
	}

	const std::vector<KRE::SceneObjectPtr>& Terrain::getRenderable(const rect& r) const
	{
		return renderable_;
	}

	void Terrain::generate(engine& eng)
	{
		generate_terrain_chunk(start_location_);
		auto& ts = get_terrain_data().get_tile_size();
		setTileSize(ts.x, ts.y);
	}

	void Terrain::clearVisible()
	{
		// XXX
	}

	bool Terrain::blocksLight(int x, int y) const
	{
		return false;
	}

	int Terrain::getDistance(int x, int y) const
	{
		return x + y; // Manhattan distance
	}

	bool Terrain::isWalkable(int x, int y) const
	{
		//const terrain_tile_ptr tt = getTileAt(point(x, y));
		//return tt == nullptr ? false : tt->isWalkable();
		return true;
	}

	const point& Terrain::getStartLocation() const
	{
		return start_location_;
	}

	void Terrain::handleSetVisible(int x, int y)
	{
		// XXX
	}

	variant Terrain::handleWrite()
	{
		// XXX
		return variant();
	}

	void Terrain::update(engine& eng)
	{
		const auto& p = eng.getPlayer();
		ASSERT_LOG(p != nullptr, "No player in engine!");

		const pointf& ts = getTileSize();
		pointf map_offset;
		// draw map
		const int screen_width_in_tiles = (eng.getGameArea().w() + ts.x - 1) / ts.x;
		const int screen_height_in_tiles = (eng.getGameArea().h() + ts.y - 1) / ts.y;
		rect area = rect::from_coordinates(-screen_width_in_tiles / 2 + p->pos->pos.x / ts.x, 
			-screen_height_in_tiles / 2 + p->pos->pos.y / ts.y,
			screen_width_in_tiles / 2 + p->pos->pos.x / ts.x,
			screen_height_in_tiles / 2 + p->pos->pos.y / ts.y);

		renderable_.clear();
		std::vector<KRE::SceneObjectPtr> renderables;
		for(auto& chunk : get_chunks_in_area(area)) {
			renderable_.emplace_back(chunk->get_renderable());
		}
	}
}
