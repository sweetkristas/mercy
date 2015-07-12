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

#pragma once

#include <bitset>
#include <memory>
#include <string>
#include <vector>

#include "Color.hpp"
#include "Texture.hpp"
#include "geometry.hpp"
#include "variant.hpp"

typedef std::bitset<64> component_id;

namespace component
{
	// XXX Todo thing of a cleaner way of doing this with bitsets.
	enum class Component
	{
		POSITION,
		SPRITE,
		STATS,
		AI,
		INPUT,
		LIGHTS,
		MAP,
		GUI,
		// tag only values must go at end.
		PLAYER,
		ENEMY,
		COLLISION,
		MAX_COMPONENTS,
	};
	static_assert(static_cast<int>(Component::MAX_COMPONENTS) <= 64, "Maximum number of components must be less than 64. If you need more consider a vector<bool> solution.");

	Component get_component_from_string(const std::string& s);

	inline component_id operator<<(int value, const Component& rhs) 
	{
		return component_id(value << static_cast<unsigned long long>(rhs));
	}
	inline component_id genmask(const Component& lhs)
	{
		return 1 << lhs;
	}

	class component
	{
	public:
		explicit component(Component id) : id_(component_id(static_cast<unsigned long long>(id))) {}
		virtual ~component() {}
		component_id id() const { return id_; }
	private:
		const component_id id_;
	};

	typedef std::shared_ptr<component> component_ptr;

	struct position : public component
	{
		position() : component(Component::POSITION) {}
		position(const point& p) : component(Component::POSITION), pos(p) {}
		point pos;
		point mov;
	};

	struct sprite : public component
	{
		sprite() : component(Component::SPRITE) {}
		sprite(KRE::TexturePtr t, const rect& area=rect());
		sprite(const std::string& filename, const rect& area=rect());
		~sprite();
		void update_texture(KRE::TexturePtr t);
		KRE::TexturePtr tex;
	};

	struct stats : public component
	{
		stats() : component(Component::STATS), health(1), attack(0), armour(0) {}
		int health;
		int attack;
		int armour;
		std::string name;
	};

	struct ai : public component
	{
		ai() : component(Component::AI) {}
		// XXX Need to add some data
		std::string type;
	};

	struct input : public component
	{
		input() : component(Component::INPUT) {}
		enum class Action {
			none,
			moved,
			use,
			attack,
			spell,
			pass,
		} action;
	};

	struct point_light
	{
		point_light(int xx, int yy, const KRE::Color& cc, float att)
			: x(xx), y(yy), color(cc), attenuation(att) 
		{}
		int x;
		int y;
		KRE::Color color;
		float attenuation;
	};

	struct lights : public component
	{
		lights();
		~lights();
		// XXX These should in some sort of quadtree like structure.
		std::vector<point_light> light_list;
		KRE::TexturePtr tex;
	};

	struct component_set
	{
		component_set(int z=0) : mask(component_id(0)), zorder(z) {}
		component_id mask;
		int zorder;
		std::shared_ptr<position> pos;
		std::shared_ptr<sprite> spr;
		std::shared_ptr<stats> stat;
		std::shared_ptr<ai> aip;
		std::shared_ptr<input> inp;
		bool is_player() { return (mask & genmask(Component::PLAYER)) == genmask(Component::PLAYER); }
	};
	typedef std::shared_ptr<component_set> component_set_ptr;
	
	inline bool operator<(const component_set_ptr& lhs, const component_set_ptr& rhs)
	{
		return lhs->zorder == rhs->zorder ? lhs.get() < rhs.get() : lhs->zorder < rhs->zorder;
	}
}
