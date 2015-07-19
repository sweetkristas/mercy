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

#include "SDL.h"

#include "engine_fwd.hpp"

namespace process
{
	enum class ProcessPriority {
		input			= 100,
		ai				= 200,
		collision		= 600,
		action			= 700,
		world			= 800,
		gui			    = 850,
		render			= 900
	};

	// abstract system interface
	class process
	{
	public:
		explicit process(ProcessPriority priority);
		virtual ~process();
		virtual void start() {}
		virtual void end() {}
		virtual void update(engine& eng, float t, const entity_list& elist) = 0;

		bool process_event(const SDL_Event& evt);
		ProcessPriority get_priority() const { return priority_; }
	private:
		process();
		virtual bool handle_event(const SDL_Event& evt) { return false; }
		ProcessPriority priority_;
	};

	typedef std::shared_ptr<process> process_ptr;
}
