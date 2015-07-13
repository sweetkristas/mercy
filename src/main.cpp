/*
	Copyright (C) 2015 by Kristina Simpson <sweet.kristas@gmail.com>
	
	This software is provided 'as-is', without any express or implied
	warranty. In no event will the authors be held liable for any damages
	arising from the use of this software.

	Permission is granted to anyone to use this software for any purpose,
	including commercial applications, and to alter it and redistribute it
	freely, subject to the following restrictions:

	   1. The origin of this software must not be misrepresented; you must not
	   claim that you wrote the original software. If you use this software
	   in a product, an acknowledgment in the product documentation would be
	   appreciated but is not required.

	   2. Altered source versions must be plainly marked as such, and must not be
	   misrepresented as being the original software.

	   3. This notice may not be removed or altered from any source
	   distribution.
*/

#include <clocale>
#include <locale>

#include "asserts.hpp"
#include "filesystem.hpp"
#include "Blittable.hpp"
#include "CameraObject.hpp"
#include "Canvas.hpp"
#include "Font.hpp"
#include "RenderManager.hpp"
#include "RenderTarget.hpp"
#include "SceneGraph.hpp"
#include "SceneNode.hpp"
#include "SceneTree.hpp"
#include "SDLWrapper.hpp"
#include "SurfaceBlur.hpp"
#include "WindowManager.hpp"
#include "profile_timer.hpp"
#include "variant_utils.hpp"
#include "unit_test.hpp"

#include "css_parser.hpp"
#include "FontDriver.hpp"
#include "xhtml.hpp"
#include "xhtml_layout_engine.hpp"
#include "xhtml_root_box.hpp"
#include "xhtml_style_tree.hpp"
#include "xhtml_node.hpp"
#include "xhtml_render_ctx.hpp"

#include "engine.hpp"
#include "action_process.hpp"
#include "ai_process.hpp"
#include "collision_process.hpp"
#include "input_process.hpp"
#include "random.hpp"

#if defined(_MSC_VER)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#pragma comment(lib, "libnoise")
#endif

void check_layout(int width, int height, xhtml::StyleNodePtr& style_tree, xhtml::DocumentPtr doc, KRE::SceneTreePtr& scene_tree, KRE::SceneGraphPtr graph)
{
	xhtml::RenderContextManager rcm;
	// layout has to happen after initialisation of graphics
	if(doc->needsLayout()) {
		LOG_DEBUG("Triggered layout!");

		//display_list->clear();

		// XXX should we should have a re-process styles flag here.

		{
		profile::manager pman("apply styles");
		doc->processStyleRules();
		}

		{
			profile::manager pman("create style tree");
			if(style_tree == nullptr) {
				style_tree = xhtml::StyleNode::createStyleTree(doc);
				scene_tree = style_tree->getSceneTree();
			} else {
				style_tree->updateStyles();
			}
		}

		xhtml::RootBoxPtr layout = nullptr;
		{
		profile::manager pman("layout");
		layout = xhtml::Box::createLayout(style_tree, width, height);
		}
		{
		profile::manager pman_render("render");
		scene_tree->clear();
		layout->render(point());
		}
	}
}

xhtml::DocumentPtr load_xhtml(const std::string& ua_ss, const std::string& test_doc)
{
	auto user_agent_style_sheet = std::make_shared<css::StyleSheet>();
	css::Parser::parse(user_agent_style_sheet, sys::read_file(ua_ss));

	auto doc = xhtml::Document::create(user_agent_style_sheet);
	auto doc_frag = xhtml::parse_from_file(test_doc, doc);
	doc->addChild(doc_frag, doc);
	//doc->normalize();
	doc->processStyles();
	// whitespace can only be processed after applying styles.
	doc->processWhitespace();

	/*doc->preOrderTraversal([](xhtml::NodePtr n) {
		LOG_DEBUG(n->toString());
		return true;
	});*/

	// XXX - open question. Should we generate another tree for handling mouse events.

	return doc;
}


std::string wide_string_to_utf8(const std::wstring& ws)
{
	std::string res;

	int ret = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), ws.size(), nullptr, 0, nullptr, nullptr);
	if(ret > 0) {
		std::vector<char> str;
		str.resize(ret);
		ret = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), ws.size(), str.data(), str.size(), nullptr, nullptr);
		res = std::string(str.begin(), str.end());
	}

	return res;
}

void read_system_fonts(sys::file_path_map* res)
{
#if defined(_MSC_VER)
	// HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Explorer\Shell Folders\Fonts

	// should enum the key for the data type here, then run a query with a null buffer for the size

	HKEY font_key;
	LONG err = RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders", 0, KEY_READ, &font_key);
	if(err == ERROR_SUCCESS) {
		std::vector<wchar_t> data;
		DWORD data_size = 0;

		err = RegQueryValueExW(font_key, L"Fonts", 0, 0, nullptr, &data_size);
		if(err != ERROR_SUCCESS) {
			return;
		}
		data.resize(data_size);

		err = RegQueryValueExW(font_key, L"Fonts", 0, 0, reinterpret_cast<LPBYTE>(data.data()), &data_size);
		if(err == ERROR_SUCCESS) {
			if(data[data_size-2] == 0 && data[data_size-1] == 0) {
				// was stored with null terminator
				data_size -= 2;
			}
			data_size >>= 1;
			std::wstring base_font_dir(data.begin(), data.begin()+data_size);
			std::wstring wstr = base_font_dir + L"\\*.?tf";
			WIN32_FIND_DATA find_data;
			HANDLE hFind = FindFirstFileW(wstr.data(), &find_data);
			(*res)[wide_string_to_utf8(std::wstring(find_data.cFileName))] = wide_string_to_utf8(base_font_dir + L"\\" + std::wstring(find_data.cFileName));
			if(hFind != INVALID_HANDLE_VALUE) {			
				while(FindNextFileW(hFind, &find_data)) {
					(*res)[wide_string_to_utf8(std::wstring(find_data.cFileName))] = wide_string_to_utf8(base_font_dir + L"\\" + std::wstring(find_data.cFileName));
				}
			}
			FindClose(hFind);
		} else {
			LOG_WARN("Unable to read \"Fonts\" sub-key");
		}
		RegCloseKey(font_key);
	} else {
		LOG_WARN("Unable to read the shell folders registry key");
		// could try %windir%\fonts as a backup
	}
#elif defined(linux) || defined(__linux__)
#else
#endif
}

void generate_map(engine& eng)
{
	const int map_width = 100;
	const int map_height = 50;

	const int min_room_size = 3;
	const int max_room_size = 20;
	
	const int num_rooms = 150;

	std::vector<rect> rooms;
	for(int n = 0; n != num_rooms; ++n) {
		int x = generator::get_uniform_int<int>(max_room_size, map_width-max_room_size);
		int y = generator::get_uniform_int<int>(max_room_size, map_height-max_room_size);
		int w = generator::get_uniform_int<int>(min_room_size, max_room_size);
		int h = generator::get_uniform_int<int>(min_room_size, max_room_size);

		rooms.emplace_back(rect(x, y, w, h));
	}

	std::vector<std::string> output;
	output.resize(map_height);
	for(auto& op : output) {
		op.resize(map_width, ' ');
	}
	for(auto& room : rooms) {
		for(int x = room.x1(); x != room.x2(); ++x) {
			output[room.y1()][x] = '#';
			output[room.y2()][x] = '#';
		}
		for(int y = room.y1()+1; y != room.y2()-1; ++y) {
			output[y][room.x1()] = '#';
			output[y][room.x2()] = '#';
		}
	}

	for(auto& line : output) {
		std::cout << line << "\n";
	}
}

int main(int argc, char* argv[])
{
	std::vector<std::string> args;
	for(int i = 1; i < argc; ++i) {
		args.emplace_back(argv[i]);
	}

	int width = 1600;
	int height = 900;

	using namespace KRE;
	SDL::SDL_ptr manager(new SDL::SDL());
	SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_DEBUG);

	if(!test::run_tests()) {
		// Just exit if some tests failed.
		exit(1);
	}

	const std::string data_path = "data/";
	const std::string ua_ss = data_path + "user_agent.css";

	sys::file_path_map font_files;
	sys::get_unique_files(data_path + "fonts/", font_files);
	read_system_fonts(&font_files);
	KRE::FontDriver::setAvailableFonts(font_files);
	KRE::FontDriver::setFontProvider("stb");

	WindowManager wm("SDL");

	variant_builder hints;
	hints.add("renderer", "opengl");
	hints.add("dpi_aware", true);
	hints.add("use_vsync", true);
	hints.add("resizeable", true);

	LOG_DEBUG("Creating window of size: " << width << "x" << height);
	auto main_wnd = wm.createWindow(width, height, hints.build());
	main_wnd->enableVsync(true);
	const float aspect_ratio = static_cast<float>(width) / height;

	LOG_DEBUG("setting image file filter to 'images/'");
	Surface::setFileFilter(FileFilterType::LOAD, [](const std::string& fname) { return "images/" + fname; });
	Surface::setFileFilter(FileFilterType::SAVE, [](const std::string& fname) { return "images/" + fname; });
	Font::setAvailableFonts(font_files);

	SceneGraphPtr scene = SceneGraph::create("main");
	SceneNodePtr root = scene->getRootNode();
	root->setNodeName("root_node");

	DisplayDevice::getCurrent()->setDefaultCamera(std::make_shared<Camera>("ortho1", 0, width, 0, height));

	auto rman = std::make_shared<RenderManager>();
	auto rq = rman->addQueue(0, "opaques");

	xhtml::DocumentPtr doc = nullptr; //load_xhtml(ua_ss, test_doc);
	xhtml::StyleNodePtr style_tree = nullptr;
	KRE::SceneTreePtr scene_tree = nullptr;
	if(doc != nullptr) {
		check_layout(width, height, style_tree, doc, scene_tree, scene);
	}

	auto canvas = Canvas::getInstance();

	std::unique_ptr<engine> eng(new engine(main_wnd));

	eng->add_process(std::make_shared<process::input>());
	eng->add_process(std::make_shared<process::ai>());
	eng->add_process(std::make_shared<process::action>());
	// N.B. entity/map collision needs to come before entity/entity collision
	eng->add_process(std::make_shared<process::em_collision>());
	eng->add_process(std::make_shared<process::ee_collision>());

	generate_map(*eng);

	SDL_Event e;
	bool running = true;
	Uint32 last_tick_time = SDL_GetTicks();
	while(running) {
		/*while(SDL_PollEvent(&e)) {
			// XXX we need to add some keyboard/mouse callback handling here for "doc".
			if(e.type == SDL_KEYUP && e.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
				done = true;
			} else if(e.type == SDL_KEYDOWN) {
				LOG_DEBUG("KEY PRESSED: " << SDL_GetKeyName(e.key.keysym.sym) << " : " << e.key.keysym.sym << " : " << e.key.keysym.scancode);
			} else if(e.type == SDL_QUIT) {
				done = true;
			} else if(e.type == SDL_MOUSEMOTION) {
				if(doc != nullptr) {
					doc->handleMouseMotion(false, e.motion.x, e.motion.y);
				}
			} else if(e.type == SDL_MOUSEBUTTONDOWN) {
				if(doc != nullptr) {
					doc->handleMouseButtonDown(false, e.button.x, e.button.y, e.button.button);
				}
			} else if(e.type == SDL_MOUSEBUTTONUP) {
				if(doc != nullptr) {
					doc->handleMouseButtonUp(false, e.button.x, e.button.y, e.button.button);
				}
			} else if(e.type == SDL_WINDOWEVENT) {
				const SDL_WindowEvent& wnd = e.window;
				if(wnd.event == SDL_WINDOWEVENT_RESIZED) {
					if(doc != nullptr) {
						doc->triggerLayout();
					}
					width = wnd.data1;
					height = wnd.data2;
					main_wnd->notifyNewWindowSize(width, height);
					DisplayDevice::getCurrent()->setDefaultCamera(std::make_shared<Camera>("ortho1", 0, width, 0, height));
				}
			}
		}*/

		//main_wnd->setClearColor(KRE::Color::colorWhite());
		main_wnd->clear(ClearFlags::ALL);

		if(doc) {
			check_layout(width, height, style_tree, doc, scene_tree, scene);
		}

		// Called once a cycle before rendering.
		Uint32 current_tick_time = SDL_GetTicks();
		float dt = (current_tick_time - last_tick_time) / 1000.0f;
		if(style_tree != nullptr) {
			style_tree->process(dt);
		}
		scene->process(dt);
		running = eng->update(dt);
		last_tick_time = current_tick_time;

		//scene->renderScene(rman);
		//rman->render(main_wnd);

		if(scene_tree != nullptr) {
			scene_tree->preRender(main_wnd);
			scene_tree->render(main_wnd);
		}

		main_wnd->swap();
	}

	return 0;
}
