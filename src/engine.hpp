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

#include "engine_fwd.hpp"
#include "geometry.hpp"
#include "process.hpp"
#include "profile_timer.hpp"
#include "quadtree.hpp"

#include "WindowManagerFwd.hpp"

enum class EngineState {
	PLAY,
	PAUSE,
	QUIT,
};

enum class EngineUserEvents {
	NEW_TURN = 1,
};

class engine
{
public:
	engine(const KRE::WindowPtr& wm);
	~engine();
	
	void add_entity(component_set_ptr e);
	void remove_entity(component_set_ptr e);

	void add_process(process::process_ptr s);
	void remove_process(process::process_ptr s);

	KRE::WindowPtr& get_window() { return wm_; }
	const KRE::WindowPtr& get_window() const { return wm_; }

	void set_state(EngineState state) { state_ = state; }
	EngineState get_state() const { return state_; }

	bool update(double time);

	int get_turns() const { return turns_; }
	void inc_turns(int cnt = 1);

	void set_camera(const point& cam) { camera_ = cam; }
	const point& get_camera() { return camera_; }

	entity_list entities_in_area(const rect& r);

	const point& get_tile_size() const { return tile_size_; }
	void set_tile_size(const point& p) { tile_size_ = p; }

private:
	void translate_mouse_coords(SDL_Event* evt);
	void process_events();
	void populate_quadtree();
	EngineState state_;
	int turns_;
	point camera_;
	KRE::WindowPtr wm_;
	entity_list entity_list_;
	quadtree<component_set_ptr> entity_quads_;
	std::vector<process::process_ptr> process_list_;
	point tile_size_;
};
