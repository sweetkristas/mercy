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

#include "action_process.hpp"
#include "component.hpp"
#include "engine.hpp"

namespace process
{
	action::action()
		: process(ProcessPriority::action)
	{
	}

	void action::update(engine& eng, float t, const entity_list& elist)
	{
		using namespace component;
		static component_id mask = genmask(Component::INPUT) | genmask(Component::POSITION);
		for(auto& e : elist) {
			if((e->mask & mask) == mask) {
				auto& inp = e->inp;
				auto& pos = e->pos;

				switch(inp->action)
				{
				case input::Action::none:	break;
				case input::Action::moved:	
					// Test to see if we moved but the collision detection failed it.
					// XX there must be a better way.
					if(pos->mov.x == 0 && pos->mov.y == 0) {
						inp->action = input::Action::none;
					} else {
						e->pos->pos += e->pos->mov;
						e->pos->mov.clear();
						if(e->is_player()) {
							eng.set_camera(e->pos->pos);
						}
					}
					break;
				case input::Action::pass:	break;
				case input::Action::attack:	break;
				case input::Action::spell:	break;
				case input::Action::use:	break;
				default: 
					ASSERT_LOG(false, "No action defined for " << static_cast<int>(inp->action));
					break;
				}
				// increment turns on successful update.
				if(inp->action != input::Action::none) {
					eng.inc_turns();
				}
			} else if((e->mask & genmask(Component::AI)) == genmask(Component::AI)) {
				//auto& aip = e->aip;
			}
		}
	}
}
