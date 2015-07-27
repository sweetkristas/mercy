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

#include <map>
#include <memory>

#include "color.hpp"
#include "variant.hpp"
#include "geometry.hpp"
#include "map.hpp"

#include "SceneFwd.hpp"

class engine;

namespace mercy
{
	typedef unsigned TerrainType;

	class chunk;
	typedef std::shared_ptr<chunk> chunk_ptr;

	typedef std::map<point, chunk_ptr> terrain_map_type;

	class terrain_tile
	{
	public:
		terrain_tile(float threshold, TerrainType tt, const std::string& name, const std::string& symbol, const KRE::Color& color);
		TerrainType get_terrain_type() const { return terrain_type_; }
		float get_threshold() const { return threshold_; }
		const std::string& get_name() const { return name_; }
		const std::string& get_symbol() const { return symbol_; }
		const KRE::Color& get_color() const { return color_; }
		void setWalkable(bool walkable=true) { is_walkable_ = walkable; }
		bool isWalkable() const { return is_walkable_; }
	private:
		float threshold_;
		TerrainType terrain_type_;
		std::string name_;
		std::string symbol_;
		KRE::Color color_;
		bool is_walkable_;
	};
	typedef std::shared_ptr<terrain_tile> terrain_tile_ptr;
	inline bool operator<(const terrain_tile& lhs, const terrain_tile& rhs) { return lhs.get_threshold() < rhs.get_threshold(); }
	inline bool operator<(const terrain_tile_ptr& lhs, const terrain_tile_ptr& rhs) { return *lhs < *rhs; }

	class chunk
	{
	public:
		chunk(const point& pos, int width, int height);
		void set_at(int x, int y, float tt);
		terrain_tile_ptr get_at(int x, int y);
		TerrainType get_terrain_at(int x, int y);
		int width() const { return width_; }
		int height() const { return height_; }
		const point& get_position() const { return pos_; }
		static KRE::SceneObjectPtr make_renderable_from_chunk(chunk_ptr chk);
		void set_renderable(const KRE::SceneObjectPtr& r) { renderable_ = r; }
		KRE::SceneObjectPtr get_renderable() const { return renderable_; }
	private:
		point pos_;
		int width_;
		int height_;
		std::vector<std::vector<terrain_tile_ptr>> terrain_;
		KRE::SceneObjectPtr renderable_;
	};
	typedef std::shared_ptr<chunk> chunk_ptr;

	class Terrain : public BaseMap
	{
	public:
		// default new world
		explicit Terrain(const variant& features);
		// when loading from existing data.
		explicit Terrain(const variant& node, const variant& features);

		// Find all the chunks which are in the given area, including partials.
		// Will generate chunks as needed for complete coverage.
		std::vector<chunk_ptr> get_chunks_in_area(const rect& r);

		// Pos is the worldspace position.
		chunk_ptr generate_terrain_chunk(const point& pos);
		terrain_tile_ptr getTileAt(const point& p);
		// const version of function to get a tile, if pos isn't available in
		// current chunks it will not spawn a new chunk.
		terrain_tile_ptr getTileAt(const point& pos) const;

		static void load_terrain_data(const variant& n);
		static pointf get_terrain_size();
		//variant write();

		void update(engine& eng) override;

		const std::vector<KRE::SceneObjectPtr>& getRenderable(const rect& r) const override;
		void generate(engine& eng) override;
		void clearVisible() override;
		bool blocksLight(int x, int y) const override;
		int getDistance(int x, int y) const override;
		
		bool isWalkable(int x, int y) const override;

		bool isFixedSize() const override { return false; }
		const point& getStartLocation() const override;

	private:
		void handleSetVisible(int x, int y) override;
		variant handleWrite() override;
		int chunk_size_w_;
		int chunk_size_h_;
		terrain_map_type chunks_;
		int terrain_seed_;
		point start_location_;
		std::vector<KRE::SceneObjectPtr> renderable_;
	};
}