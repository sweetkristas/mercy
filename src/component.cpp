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

#include "Texture.hpp"

#include "asserts.hpp"
#include "component.hpp"

namespace component
{
	Component get_component_from_string(const std::string& s)
	{
		if(s == "position") {
			return Component::POSITION;
		} else if(s == "sprite") {
			return Component::SPRITE;
		} else if(s == "stats") {
			return Component::STATS;
		} else if(s == "ai") {
			return Component::AI;
		} else if(s == "input") {
			return Component::INPUT;
		} else if(s == "lights") {
			return Component::LIGHTS;
		} else if(s == "map") {
			return Component::MAP;
		} else if(s == "player") {
			return Component::PLAYER;
		} else if(s == "enemy") {
			return Component::ENEMY;
		} else if(s == "gui") {
			return Component::GUI;
		} else if(s == "collision") {
			return Component::COLLISION;
		}
		ASSERT_LOG(false, "Unrecognised component string '" << s << "'");
		return static_cast<Component>(0);
	}

	lights::lights() 
		: component(Component::LIGHTS)
	{
	}

	lights::~lights()
	{
	}
}