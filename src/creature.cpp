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

#include <map>

#include "component.hpp"
#include "creature.hpp"
#include "random.hpp"

namespace creature
{
	namespace
	{
		class creature
		{
		public:
			creature(const variant& n) : component_mask_(0) {
				using namespace component;
				ASSERT_LOG(n.is_map(), "Creature definitions must be maps");
				ASSERT_LOG(n.has_key("name"), "Must supply a 'name' attribute for the creature.");
				name_ = n["name"].as_string();
				
				// The 'components' is the single most important piece of information.
				ASSERT_LOG(n.has_key("components"), "No 'components' attribute found for creature " << name_);
				ASSERT_LOG(n["components"].is_list(), "'components' attribute must be a list of strings (" << name_ << ")");
				for(auto& s : n["components"].as_list_string()) {
					component_mask_ |= genmask(get_component_from_string(s));
				}

				if((component_mask_ & genmask(Component::STATS)) == genmask(Component::STATS)) {
					ASSERT_LOG(n.has_key("stats"), "'stats component found must supply a 'stats' attribute for the creature " << name_);
					const variant& stats = n["stats"];
					ASSERT_LOG(stats.has_key("health"), "Must supply a 'health' attribute for the creature " << name_);
					if(stats["health"].is_int()) {
						health_min_ = health_max_ = static_cast<int>(stats["health"].as_int());
					} else if(stats["health"].is_list()) {
						ASSERT_LOG(stats["health"].num_elements() == 2, "'health' attribute for creature wasn't a list of two integers " << name_);
						health_min_ = static_cast<int>(stats["health"][0].as_int());
						health_max_ = static_cast<int>(stats["health"][1].as_int());
						if(health_min_ > health_max_) {
							std::swap(health_min_, health_max_);
						}
					}
					ASSERT_LOG(health_min_ > 0 && health_max_ > 0, "Health min/max must be greater than zero: " << health_min_ << " : " << health_max_ << " for creature " << name_);

					ASSERT_LOG(stats.has_key("attack"), "Must supply a 'attack' attribute for the creature " << name_);
					if(stats["attack"].is_int()) {
						attack_min_ = attack_max_ = static_cast<int>(stats["attack"].as_int());
					} else if(stats["attack"].is_list()) {
						ASSERT_LOG(stats["attack"].num_elements() == 2, "'attack' attribute for creature wasn't a list of two integers. " << name_);
						attack_min_ = static_cast<int>(stats["attack"][0].as_int());
						attack_max_ = static_cast<int>(stats["attack"][1].as_int());
						if(attack_min_ > attack_max_) {
							std::swap(attack_min_, attack_max_);
						}
					}
					ASSERT_LOG(attack_min_ > 0 && attack_max_ > 0, "Attack min/max must be greater than zero: " << attack_min_ << " : " << attack_max_ << " for creature " << name_);
					
					armour_ = stats.has_key("armour") ? static_cast<int>(stats["armour"].as_int()) : 0;
				}

				if((component_mask_ & genmask(Component::AI)) == genmask(Component::AI)) {
					if(n.has_key("ai")) {
						ai_name_ = n["ai"].as_string();
					}
				}

				if((component_mask_ & genmask(Component::SPRITE)) == genmask(Component::SPRITE)) {
					ASSERT_LOG(n.has_key("sprite"), "No 'sprite' attribute found for creature " << name_);
					sprite_name_ = n["sprite"].as_string();
					if(n.has_key("area")) {
						ASSERT_LOG(n["area"].is_list() && n["area"].num_elements() == 4, "'area' attribute for creature " << name_ << " must be list of four integers (x1,y1,x2,y2). " << n["area"].type_as_string());
						sprite_area_ = rect::from_coordinates(static_cast<int>(n["area"][0].as_int()),
							static_cast<int>(n["area"][1].as_int()),
							static_cast<int>(n["area"][2].as_int()),
							static_cast<int>(n["area"][3].as_int()));
					}
				}
			}

			component_set_ptr create_instance(const point& pos) {
				component_set_ptr res = std::make_shared<component::component_set>(80);
				using namespace component;
				// XXX fix zorder here.
				if((component_mask_ & genmask(Component::STATS)) == genmask(Component::STATS)) {
					res->stat = std::make_shared<stats>();
					res->stat->health = generator::get_uniform_int(health_min_, health_max_);
					res->stat->attack = generator::get_uniform_int(attack_min_, attack_max_);
					res->stat->armour = armour_;
					res->stat->name = name_;
				}
				if((component_mask_ & genmask(Component::POSITION)) == genmask(Component::POSITION)) {
					res->pos = std::make_shared<position>(pos);
				}
				if((component_mask_ & genmask(Component::SPRITE)) == genmask(Component::SPRITE)) {
					// The whole sprite mess needs fixed, since it doesn't take an area.
					// XXX fixme
					///res->spr = std::make_shared<sprite>(sprite_name_, sprite_area_);
				}
				if((component_mask_ & genmask(Component::AI)) == genmask(Component::AI)) {
					res->aip = std::make_shared<ai>();
					res->aip->type = ai_name_;
				}
				if((component_mask_ & genmask(Component::LIGHTS)) == genmask(Component::LIGHTS)) {
					// XXX?
				}
				if((component_mask_ & genmask(Component::INPUT)) == genmask(Component::INPUT)) {
					res->inp = std::make_shared<input>();
					res->inp->action = input::Action::none;
				}
				res->mask = component_mask_;
				return res;
			}
		private:
			// Displayable name
			std::string name_;
			int health_min_;
			int health_max_;
			int attack_min_;
			int attack_max_;
			int armour_;
			// attack type (magic, physical, type of magic, type of physical, etc)
			// has ranged attack
			// what items might be carried.
			// lights
			std::string ai_name_;
			std::string sprite_name_;
			rect sprite_area_;
			component_id component_mask_;
			creature();
		};
		typedef std::shared_ptr<creature> creature_ptr;

		typedef std::map<std::string, creature_ptr> creature_cache;
		creature_cache& get_creature_cache()
		{
			static creature_cache res;
			return res;
		}
	}

	void loader(const variant& n)
	{
		for(auto& cr : n.as_map()) {
			get_creature_cache()[cr.first.as_string()] = std::make_shared<creature>(cr.second);
		}
	}

	component_set_ptr spawn(const std::string& type, const point& pos)
	{
		auto it = get_creature_cache().find(type);
		ASSERT_LOG(it != get_creature_cache().end(), "Couldn't find a definition for creature of type '" << type << "' in the cache.");
		return it->second->create_instance(pos);
	}
}
