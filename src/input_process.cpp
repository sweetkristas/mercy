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

#include "SDL.h"

#include "component.hpp"
#include "input_process.hpp"

namespace process
{
	input::input()
		: process(ProcessPriority::input)
	{
	}

	bool input::handle_event(const SDL_Event& evt)
	{
		if(evt.type == SDL_KEYDOWN) {
			keys_pressed_.push(evt.key.keysym.scancode);
			return true;
		}
		return false;
	}

	void input::update(engine& eng, float t, const entity_list& elist)
	{
		static component_id input_mask 
			= component::genmask(component::Component::POSITION) 
			| component::genmask(component::Component::INPUT)
			| component::genmask(component::Component::PLAYER);
		for(auto& e : elist) {
			if((e->mask & input_mask) == input_mask) {
				auto& inp = e->inp;
				auto& pos = e->pos;
				inp->action = component::input::Action::none;
				if(!keys_pressed_.empty()) {
					auto key = keys_pressed_.front();
					keys_pressed_.pop();
					if(key == SDL_SCANCODE_LEFT) {
						pos->mov.x -= 1;
						inp->action = component::input::Action::moved;
					} else if(key == SDL_SCANCODE_RIGHT) {
						pos->mov.x += 1;
						inp->action = component::input::Action::moved;
					} else if(key == SDL_SCANCODE_UP) {
						pos->mov.y -= 1;
						inp->action = component::input::Action::moved;
					} else if(key == SDL_SCANCODE_DOWN) {
						pos->mov.y += 1;
						inp->action = component::input::Action::moved;
					} else if(key == SDL_SCANCODE_PERIOD) {
						inp->action = component::input::Action::pass;
					}
				}
			}
		}
	}
}
