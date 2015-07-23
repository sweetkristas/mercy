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

#pragma once

#include <memory>

#include "SceneFwd.hpp"
#include "variant.hpp"
#include "visibility_fwd.hpp"

namespace mercy
{
	class BaseMap;
	typedef std::shared_ptr<BaseMap> BaseMapPtr;

	class BaseMap
	{
	public:
		explicit BaseMap(int width, int height);
		virtual ~BaseMap();
		int getWidth() const { return width_; }
		int getHeight() const { return height_; }
		static BaseMapPtr create(const std::string& type, int width, int height, const variant& features);
		virtual KRE::SceneObjectPtr getRenderable() = 0;
		virtual void generate() = 0;
		const pointf& getTileSize() const { return tile_size_; }

		virtual void clearVisible() = 0;
		virtual bool blocksLight(int x, int y) const = 0;
		virtual void setVisible(int x, int y) = 0;
		virtual int getDistance(int x, int y) const = 0;
		
		virtual bool isWalkable(int x, int y) const = 0;

		virtual bool isFixedSize() const = 0;

		void updatePlayerVisibility(const point& pos, int visible_radius);

		virtual const point& getStartLocation() const = 0;
	protected:
		void setTileSize(float x, float y) { tile_size_.x = x; tile_size_.y = y; }
	private:
		int width_;
		int height_;
		pointf tile_size_;
		VisibilityPtr visibility_;
	};
}
