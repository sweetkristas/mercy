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
#include "collision_process.hpp"

namespace process
{
	ee_collision::ee_collision()
		: process(ProcessPriority::collision)
	{
	
	}
	
	// XXX Should we split this into a map/entity collision and entity/entity collision
	// classes?
	void ee_collision::update(engine& eng, float t, const entity_list& elist)
	{
		using namespace component;
		static component_id collision_mask = genmask(Component::POSITION) | genmask(Component::COLLISION);
		static component_id collision_map_mask = collision_mask | genmask(Component::MAP);
		// O(n^2) collision testing is for the pro's :-/
		// XXX Please make quad-tree or kd-tree for O(nlogn)
		for(auto& e1 : elist) {
			if((e1->mask & collision_map_mask) == collision_mask) {
				auto& e1pos = e1->pos;
				// XXX replace elist_ below with a call to eng.entities_in_area(r)
				// where r = active area for entities on map.
				for(auto& e2 : elist) {
					if(e1 == e2) {
						continue;
					}
					if((e2->mask & collision_mask) == collision_mask) {
						// entity - entity collision
						auto& e2pos = e2->pos;
						if(e1pos->pos + e1pos->mov == e2pos->pos) {
							e1pos->mov.clear();
						}						
					}
				}
			}
		}
	}

	em_collision::em_collision()
		: process(ProcessPriority::collision)
	{
	}
	
	void em_collision::update(engine& eng, float t, const entity_list& elist)
	{
		using namespace component;
		static component_id collision_mask = genmask(Component::COLLISION) | genmask(Component::POSITION);
		static component_id  map_mask      = genmask(Component::COLLISION) | genmask(Component::MAP);
		// O(n^2) collision testing is for the pro's :-/
		// XXX Please make quad-tree or kd-tree for O(nlogn)
		for(auto& e1 : elist) {
			if((e1->mask & map_mask) == map_mask) {
				//auto& e1map = e1->get()->map;
				for(auto& e2 : elist) {
					if(e1 == e2) {
						continue;
					}
					if((e2->mask & (collision_mask | genmask(Component::MAP))) == collision_mask) {
						//auto& e2pos = e2->pos;
						// entity - map collision
						//if(!is_passable(e1map->map[e2pos->p.y][e2pos->p.x])) {
						//	e2pos->p = e2pos->last_p;
						//}
						//for(auto& exitp : e1map->exits) {
						//	if(e2pos->p == exitp) {
						//		if(e2->is_player()) {
						//			// XXX do some exit logic, quitting the game for now
						//			eng.set_state(EngineState::QUIT);
						//		} else {
						//			// don't let the non-player controlled entities leave via exit
						//			e2pos->p = e2pos->last_p;
						//		}
						//	}
						//}
					} 
				}
			}
		}
	}
}
