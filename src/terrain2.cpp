/*
	Copyright (C) 2003-2013 by Kristina Simpson <sweet.kristas@gmail.com>
	
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

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include <noise/noise.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include "asserts.hpp"
#include "profile_timer.hpp"
#include "terrain2.hpp"

#include "Color.hpp"

namespace
{
	const float fault_scale = 3.5f;
	const int fault_octaves = 5;
	const float fault_threshold = 0.95f;
	const float fault_erosion_scale = 10.0f;
	const float fault_erosion_octaves = 8.0f;

	const float land_mass_scale = 1.5f;
	const float equitorial_multiplier = 2.5f;
	const int coast_complexity = 12;
	const float water_level = 0.025f;
	const float coast_threshold = 0.02f;
	const float mountain_fault_threshold = 0.08f;
	const float mountain_hill_threshold = 0.4f;

	const float hill_scale = 5.0f;
	const int hill_octaves = 8;
	const float hill_threshold = 0.19f;

	const float moisture_reach = 0.1f;
	const float rainfall_influence = 0.03f;
	const float rainfall_hill_factor = 0.03f;
	const float rainfall_mountain_factor = 0.1f;
	const float rainfall_kernel_radius = 3.0f;

	const float ice_alt = 1.0f;
	
	enum class TerrainType {
		OCEAN,
		COAST,
		PLAIN,
		HILL,
		MOUNTAIN,
		ICE,
		TUNDRA,
	};

	typedef std::vector<KRE::Color> TerrainColor;
	const KRE::Color& get_terrain_color(TerrainType tt)
	{
		static TerrainColor res;
		if(res.empty()) { 
			res.emplace_back(0, 0, 150);
			res.emplace_back(64, 64, 255);
			res.emplace_back(32, 150, 64);
			res.emplace_back(100, 100, 0);
			res.emplace_back(150, 150, 180);
			res.emplace_back(220, 220, 255);
			res.emplace_back(150, 120, 130);
		}
		int ndx = static_cast<int>(tt);
		ASSERT_LOG(ndx < static_cast<int>(res.size()), "Unable to map " << ndx << " to color.");
		return res[ndx];
	}

	float& get_max_rainfall()
	{
		static float res = 0;
		return res;
	}

	std::vector<std::pair<int,int>> line(float dx, float dy)
	{
		std::vector<std::pair<int,int>> res;

		int idx = std::round(dx);
		int idy = std::round(dy);
		int x = 0;
		int end_x = x + idx;
		int y = 0;
		int end_y = y + idy;
		int sx = idx < 0 ? -1 : 1;
		int sy = idy < 0 ? -1 : 1;
		int adx = std::abs(idx);
		int ady = std::abs(idy);
		int err = adx > ady ? adx : -ady;

		while(x != end_x || y != end_y) {
			int err2 = err;
			if(err2 > -adx) {
				err -= ady;
				x += sx;
			}
			if(err2 < ady) {
				err += adx;
				y += sy;
			}
			res.emplace_back(x, y);
		}
		res.emplace_back(x, y);
		return res;
	}
}

namespace mercy
{
	class Tile
	{
	public:
		Tile() 
			: type_(TerrainType::PLAIN),
			  equator_distance_(0),
			  base_height_(0),
			  fault_(0),
			  elevation_(0),
			  ruggedness_(0),
			  is_land_(true),
			  air_moisture_(0),
			  rainfall_(0)
		{
		}
		explicit Tile(float ed, float bh, float flt, float el, float r, bool is_land, float am, float rain)
			: type_(TerrainType::PLAIN),
			  equator_distance_(ed),
			  base_height_(bh),
			  fault_(flt),
			  elevation_(el),
			  ruggedness_(r),
			  is_land_(is_land),
			  air_moisture_(am),
			  rainfall_(rain)
		{
			float alt = base_height_ + fault_ * 0.5f;
			if(alt + alt * ruggedness_ > (1.0f - equator_distance_) * ice_alt) {
				type_ = TerrainType::ICE;
			} else {
				if(base_height_ < water_level && alt < water_level + coast_threshold) {
					if(water_level - base_height_ > coast_threshold) {
						type_ = TerrainType::OCEAN;
					} else {
						type_ = TerrainType::COAST;
					}
				} else {
					if(ruggedness_ > mountain_hill_threshold) {
						type_ = TerrainType::MOUNTAIN;
					} else if(ruggedness_ + fault_ > hill_threshold && ruggedness_ > fault_) {
						type_ = TerrainType::HILL;
					} else if(fault_ > mountain_fault_threshold) {
						type_ = TerrainType::MOUNTAIN;
					} else {
						type_ = TerrainType::PLAIN;
					}
				}
			}
		}
		KRE::Color getTileColor()
		{
			int rain = static_cast<int>(rainfall_);
			KRE::Color color = get_terrain_color(type_);
			return KRE::Color(color.ri() - rain, color.gi() - rain, color.bi() - rain);
		}
		TerrainType getTerrainType() const { return type_; }
		bool isLand() const { return is_land_; }
		float getRainfall() const { return rainfall_; }
		void setRainfall(float rain) { rainfall_ = rain; }
		float getEquatorDistance() const { return equator_distance_; }
		float getRuggedness() const { return ruggedness_; }
	private:
		TerrainType type_;
		float equator_distance_;
		float base_height_;
		float fault_;
		float elevation_;		
		float ruggedness_;
		bool is_land_;
		float air_moisture_;
		float rainfall_;
	};

	typedef std::unique_ptr<Tile> TilePtr;

	class TerrainMap
	{
	public:
		explicit TerrainMap(int map_size, int seed)
			: width_(map_size),
			  height_(map_size),
			  seed_(seed),
			  moisture_reach_tiles_(std::round(moisture_reach / map_size)),
			  rainfall_influence_tiles_(std::round(rainfall_influence * map_size)),
			  fault_scale_f_(fault_scale / map_size),
			  erode_scale_f_(fault_erosion_scale / map_size),
			  land_scale_f_(land_mass_scale / map_size),
			  hill_scale_f_(hill_scale / map_size)
		{			
			tiles_.resize(height_);
			for(auto& row : tiles_) {
				row.resize(width_);
			}
			generate();
		}

		void generate()
		{
			// create tiles.
			for(int y = 0; y != tiles_.size(); ++y) {
				const float ed = equatorDistance(y);
				for(int x = 0; x != tiles_[y].size(); ++x) {
					const float bh = baseHeight(x, y);
					const float f = faultLevel(x, y);
					const float r = hilliness(x, y);
					const float el = bh + f * 0.5f;
					const bool is_land = bh >= water_level || el >= water_level + coast_threshold;

					tiles_[y][x].reset(new Tile(ed, bh, f, el, r, is_land, 0.0f, 0.0f));
				}
			}

			// set rainfall
			for(int y = 0; y != tiles_.size(); ++y) {
				for(int x = 0; x != tiles_[y].size(); ++x) {
					set_rainfall(x, y);
				}
			}
		}

		void set_rainfall(int x, int y)
		{
			TilePtr& tile = tiles_[y][x];
			if(!tile->isLand()) {
				return;
			}
			bool clear_line = true;
			float moisture = 0.0f;
			float rain_factor = 0.5f;
			int rainfall_reach = rainfall_influence_tiles_;

			for(auto& w : prevailingWindLine(y)) {
				const int wx = w.first;
				const int wy = w.second;
				if(y + wy < 0 || y + wy >= height_ || x + wx < 0 || x + wx >= width_) {
					continue;
				}
				TilePtr& nearby_tile = tiles_[y + wy][x + wx];
				TerrainType type = nearby_tile->getTerrainType();

				if(clear_line) {
					if(type == TerrainType::COAST || type == TerrainType::OCEAN) {
						moisture += 1.0f;
					} else if(type == TerrainType::MOUNTAIN) {
						clear_line = false;
					} else if(type == TerrainType::ICE) {
						moisture += (1.0f - nearby_tile->getEquatorDistance()) * (1.0f - nearby_tile->getRuggedness());
					} else {
						moisture *= 0.25;
					}
				}
				if(rainfall_reach != 0) {
					if(type == TerrainType::HILL) {
						rain_factor += rainfall_hill_factor;
					} else if(type == TerrainType::MOUNTAIN) {
						rain_factor += rainfall_mountain_factor;
					}
					rainfall_reach -= 1;
				} else if(!clear_line) {
					break;
				}
			}
			const float rainfall = rain_factor * moisture;
			if(rainfall > get_max_rainfall()) {
				get_max_rainfall() = rainfall;
			}
			for(int ty = y - rainfall_kernel_radius; ty != y + rainfall_kernel_radius; ++ty) {
				for(int tx = x - rainfall_kernel_radius; tx != x + rainfall_kernel_radius; ++tx) {
					if(ty < 0 || ty >= height_ || tx < 0 || tx >= width_) {
						continue;
					}
					tiles_[ty][tx]->setRainfall(rainfall);
				}
			}
		}

		std::pair<float, float> prevailingWind(int y)
		{
			const float angle = -equatorDistance(y) * 2.0f * static_cast<float>(M_PI);
			return std::make_pair(std::cos(angle), std::sin(angle));
		}

		std::vector<std::pair<int,int>> prevailingWindLine(int y)
		{
			auto delta = prevailingWind(y);
			return line(delta.first * -moisture_reach_tiles_, delta.second * -moisture_reach_tiles_);
		}

		float baseHeight(float x, float y) 
		{
			noise::module::Perlin perlin;
			perlin.SetOctaveCount(coast_complexity);
			perlin.SetSeed(seed_);
			const float height = perlin.GetValue(x * land_scale_f_, y * land_scale_f_, 0.5);
			return (height + height * std::log10(10.0f * (1.01f - equatorDistance(y))) * equitorial_multiplier) / (equitorial_multiplier + 1.0f);
		}

		float faultLevel(float x, float y) 
		{
			noise::module::Perlin perlin;
			perlin.SetOctaveCount(fault_octaves);
			perlin.SetSeed(seed_ + 10);

			float fl = 1.0f - std::abs(perlin.GetValue(x * fault_scale_f_, y * fault_scale_f_, 0.5));
			const float thold = std::max(0.0f, (fl - fault_threshold) / (1.0f - fault_threshold));
			perlin.SetSeed(seed_);
			perlin.SetOctaveCount(fault_erosion_octaves);
			perlin.SetPersistence(0.85);
			fl *= std::abs(perlin.GetValue(x * erode_scale_f_, y * erode_scale_f_, 0.5));
			fl *= std::log10(thold * 9.0f + 1.0f);
			return fl;
		}

		float hilliness(float x, float y) 
		{
			noise::module::Perlin perlin;
			perlin.SetOctaveCount(hill_octaves);
			perlin.SetSeed(seed_ + 10);
			perlin.SetPersistence(0.9);
			return std::abs(perlin.GetValue(x * hill_scale_f_, y * hill_scale_f_, 0.5));
		}

		float equatorDistance(float y)
		{
			return std::abs(height_ - y * 2.0f) / height_;
		}

		void writePng(const std::string& filename) const
		{
			std::vector<glm::u8vec4> data;
			data.resize(width_ * height_);
			for(int y = 0; y != height_; ++y) {
				for(int x = 0; x != width_; ++x) {
					data[x + y * width_] = tiles_[y][x]->getTileColor().as_u8vec4();
				}
			}
			stbi_write_png(filename.c_str(), width_, height_, 4, data.data(), height_ * 4);
		}
	private:
		int width_;
		int height_;
		int seed_;
		int moisture_reach_tiles_;
		int rainfall_influence_tiles_;
		float fault_scale_f_;
		float erode_scale_f_;
		float land_scale_f_;
		float hill_scale_f_;
		std::vector<std::vector<TilePtr>> tiles_;
		TerrainMap() = delete;
	};

	void write_terrain_image(const std::string& filename, int mapsize, int seed)
	{
		profile::manager pman("write_terrain_image");
		TerrainMap map(mapsize, seed);
		map.writePng(filename);
	}
}
