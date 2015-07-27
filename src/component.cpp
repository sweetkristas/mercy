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

#include <boost/bimap.hpp>

#include "Texture.hpp"

#include "asserts.hpp"
#include "component.hpp"
#include "variant_utils.hpp"

namespace component
{
	namespace
	{
		typedef boost::bimap<Component, std::string> component_string_bimap;
		typedef component_string_bimap::value_type mapped_component_string;

		component_string_bimap& get_tile_map()
		{
			static component_string_bimap res;
			// XXX load from file? hard-coded for present.
			if(res.empty()) {
				res.insert(mapped_component_string(Component::AI, "ai"));
				res.insert(mapped_component_string(Component::COLLISION, "collision"));
				res.insert(mapped_component_string(Component::ENEMY, "enemy"));
				res.insert(mapped_component_string(Component::INPUT, "input"));
				res.insert(mapped_component_string(Component::LIGHTS, "lights"));
				res.insert(mapped_component_string(Component::PLAYER, "player"));
				res.insert(mapped_component_string(Component::POSITION, "position"));
				res.insert(mapped_component_string(Component::SPRITE, "sprite"));
				res.insert(mapped_component_string(Component::STATS, "stats"));
			}
			return res;
		}

	}

	const std::string& get_string_from_component(Component t)
	{
		auto it = get_tile_map().left.find(t);
		ASSERT_LOG(it != get_tile_map().left.end(), "Unable to find a mapping for component of type " << static_cast<int>(t) << " to string.");
		return it->get_right();
	}

	Component get_component_from_string(const std::string& s)
	{
		auto it = get_tile_map().right.find(s);
		ASSERT_LOG(it != get_tile_map().right.end(), "Unable to find a mapping for string " << s << " to Component.");
		return it->get_left();
	}

	lights::lights() 
		: component(Component::LIGHTS)
	{
	}

	lights::~lights()
	{
	}

	variant write_component_set(component_set_ptr c)
	{
		variant_builder res;
		for(int n = 0; n != static_cast<int>(Component::MAX_COMPONENTS); ++n) {
			if((genmask(static_cast<Component>(n)) & c->mask) != 0) {
				res.add("components", get_string_from_component(static_cast<Component>(n)));
			}
		}
		res.add("zorder", c->zorder);
		if(c->pos != nullptr) {
			res.add("position", c->pos->pos.x);
			res.add("position", c->pos->pos.y);
		}
		if(c->spr != nullptr) {
			// nothing need be done, we only have this for completeness.
		}
		if(c->stat != nullptr) {
			variant_builder stats;
			stats.add("health", c->stat->health);
			stats.add("attack", c->stat->attack);
			stats.add("armour", c->stat->armour);
			stats.add("visible_radius", c->stat->visible_radius);
			stats.add("name", c->stat->name);
			stats.add("id", c->stat->id);
			// XXX add more stats here as needed.
			res.add("stats", stats.build());
		}
		if(c->aip != nullptr) {
			res.add("ai", c->aip->type);
		}
		if(c->inp != nullptr) {
			// nothing need be done, we only have this for completeness.
		}
		return res.build();
	}
}