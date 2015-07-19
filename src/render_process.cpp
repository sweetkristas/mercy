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
#include "poly_map.hpp"
#include "render_process.hpp"

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

		const point& cam = eng.get_camera();
		const KRE::WindowPtr wnd = eng.getWindow();
		const point screen_centre(wnd->width() / 2, wnd->height() / 2);
		const mercy::BaseMapPtr& rmap = eng.getMap();
		const pointf& ts = rmap->getTileSize();

		// draw map
		// XXX fix this to rmap->getRenderable();
		static auto mapr = rmap->createRenderable();
		// XXX need to set map position offset by camera.
		mapr->preRender(wnd);
		wnd->render(mapr.get());

		for(auto& e : elist) {
			if((e->mask & render_mask) == render_mask) {
				auto& spr = e->spr;
				auto& pos = e->pos;

				spr->obj->setPosition(pos->pos.x * ts.x, pos->pos.y * ts.y);
				spr->obj->preRender(wnd);
				wnd->render(spr->obj.get());
			}
		}

		static geometry::PolyMap pm(10, 10, 512, 512);
		static auto polyr = pm.createRenderable();
		polyr->preRender(wnd);
		wnd->render(polyr.get());
	}

}
