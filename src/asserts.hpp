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

#include <iostream>
#include <sstream>

#ifndef SERVER_BUILD
#include "SDL.h"
#endif // SERVER_BUILD

#if defined(_MSC_VER)
#include <intrin.h>
#define DebuggerBreak()		do{ __debugbreak(); } while(0)
#define __SHORT_FORM_OF_FILE__		\
	(strrchr(__FILE__, '\\')		\
	? strrchr(__FILE__, '\\') + 1	\
	: __FILE__						\
	)
#else
#include <signal.h>
#define DebuggerBreak()		do{ raise(SIGINT); }while(0)
#define __SHORT_FORM_OF_FILE__		\
	(strrchr(__FILE__, '/')			\
	? strrchr(__FILE__, '/') + 1	\
	: __FILE__						\
	)
#endif

#ifdef SERVER_BUILD
#define ASSERT_LOG(_a,_b)															\
	do {																			\
		if(!(_a)) {																	\
			std::cerr << "CRITICAL: " << __SHORT_FORM_OF_FILE__ << ":" << __LINE__	\
				<< " : " << _b << "\n";												\
			DebuggerBreak();														\
			exit(1);																\
		}																			\
	} while(0)

#define LOG_INFO(_a)																\
	do {																			\
		std::ostringstream _s;														\
		std::cerr << "INFO: " << __SHORT_FORM_OF_FILE__ << ":" << __LINE__ << " : " \
			<< _a << "\n";															\
	} while(0)

#define LOG_DEBUG(_a)																\
	do {																			\
		std::ostringstream _s;														\
		std::cerr << "DEBUG: " << __SHORT_FORM_OF_FILE__ << ":" << __LINE__ << " : "\
			<< _a << "\n";															\
	} while(0)

#define LOG_WARN(_a)																\
	do {																			\
		std::ostringstream _s;														\
		std::cerr << "WARN: " << __SHORT_FORM_OF_FILE__ << ":" << __LINE__ << " : " \
			<< _a << "\n";															\
	} while(0)

#define LOG_ERROR(_a)																\
	do {																			\
		std::ostringstream _s;														\
		std::cerr << "ERROR: " << __SHORT_FORM_OF_FILE__ << ":" << __LINE__ << " : "\
			 << _a << "\n";															\
	} while(0)

#else

#define ASSERT_LOG(_a,_b)															\
	do {																			\
		if(!(_a)) {																	\
			std::ostringstream _s;													\
			_s << __SHORT_FORM_OF_FILE__ << ":" << __LINE__ << " : " << _b << "\n";	\
			SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, _s.str().c_str(), "");	\
			DebuggerBreak();														\
			exit(1);																\
		}																			\
	} while(0)

#define LOG_INFO(_a)																\
	do {																			\
		std::ostringstream _s;														\
		_s << __SHORT_FORM_OF_FILE__ << ":" << __LINE__ << " : " << _a << "\n";		\
		SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, _s.str().c_str(), "");			\
	} while(0)

#define LOG_DEBUG(_a)																\
	do {																			\
		std::ostringstream _s;														\
		_s << __SHORT_FORM_OF_FILE__ << ":" << __LINE__ << " : " << _a << "\n";		\
		SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, _s.str().c_str(), "");			\
	} while(0)

#define LOG_WARN(_a)																\
	do {																			\
		std::ostringstream _s;														\
		_s << __SHORT_FORM_OF_FILE__ << ":" << __LINE__ << " : " << _a << "\n";		\
		SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, _s.str().c_str(), "");			\
	} while(0)

#define LOG_ERROR(_a)																\
	do {																			\
		std::ostringstream _s;														\
		_s << __SHORT_FORM_OF_FILE__ << ":" << __LINE__ << " : " << _a << "\n";		\
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, _s.str().c_str(), "");			\
	} while(0)

#endif
