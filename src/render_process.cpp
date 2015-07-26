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

#include "component.hpp"
#include "engine.hpp"
#include "map.hpp"
#include "render_process.hpp"

#include "ModelMatrixScope.hpp"
#include "SceneObject.hpp"
#include "WindowManager.hpp"

namespace process
{
	render::render()
		: process(ProcessPriority::render)
	{
	}

	void render::update(engine& eng, float t, const entity_list& elist)
	{
		using namespace component;
		static component_id render_mask = genmask(Component::SPRITE) | genmask(Component::POSITION);

		pointf cam = eng.get_camera();
		const KRE::WindowPtr wnd = eng.getWindow();
		const point screen_centre(eng.getGameArea().mid_x(), eng.getGameArea().mid_y());
		const mercy::BaseMapPtr& rmap = eng.getMap();
		const pointf& ts = rmap->getTileSize();
		pointf map_offset;
		// draw map
		auto mapr = rmap->getRenderable();
		if(rmap->isFixedSize()) {
			const float map_pixel_width = rmap->getWidth() * ts.x;
			const float map_pixel_height = rmap->getHeight() * ts.y;
			if(map_pixel_width > eng.getGameArea().w()) {
				if(cam.x < eng.getGameArea().mid_x()) {
					map_offset.x = 0;
				} else if(cam.x > (map_pixel_width - eng.getGameArea().mid_x())) {
					map_offset.x = eng.getGameArea().x2() - map_pixel_width;
				} else {
					map_offset.x = screen_centre.x - cam.x;
				}
			} else {
				map_offset.x = screen_centre.x - map_pixel_width / 2.0f;
			}
			if(map_pixel_height > eng.getGameArea().h()) {
				if(cam.y < eng.getGameArea().mid_y()) {
					map_offset.y = 0;
				} else if(cam.y > (map_pixel_height - eng.getGameArea().mid_y())) {
					map_offset.y = eng.getGameArea().y2() - map_pixel_height;
				} else {
					map_offset.y = screen_centre.y - cam.y;
				}
			} else {
				map_offset.y = screen_centre.y - map_pixel_height/ 2.0f;
			}
			mapr->setPosition(map_offset.x, map_offset.y);
		}
		mapr->preRender(wnd);
		wnd->render(mapr.get());

		for(auto& e : elist) {
			if((e->mask & render_mask) == render_mask) {
				auto& spr = e->spr;
				auto& pos = e->pos;
				const auto& visible = rmap->getPlayerVisibleTiles();

				if(visible.find(pos->pos) != visible.end()) {
					ASSERT_LOG(spr->obj != nullptr, "No renderable object attached to sprite.");
					spr->obj->setPosition(pos->pos.x * ts.x + map_offset.x, pos->pos.y * ts.y + map_offset.y);
					spr->obj->preRender(wnd);
					wnd->render(spr->obj.get());
				}
			}
		}
	}

}
