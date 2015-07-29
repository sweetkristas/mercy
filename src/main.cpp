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
#include "FontDriver.hpp"
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
#include "cave.hpp"
#include "collision_process.hpp"
#include "component.hpp"
#include "creature.hpp"
#include "input_process.hpp"
#include "json.hpp"
#include "map.hpp"
#include "random.hpp"
#include "render_process.hpp"
#include "terrain.hpp"
#include "visibility.hpp"

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

// dpi abstraction
class DeviceMetrics
{
public:
	DeviceMetrics()
		: dpi_x_(96),
		  dpi_y_(96)
	{
#if defined(_MSC_VER)
		HDC hdc = GetDC(nullptr);
		if (hdc) {
			dpi_x_ = GetDeviceCaps(hdc, LOGPIXELSX);
			dpi_y_ = GetDeviceCaps(hdc, LOGPIXELSY);
			ReleaseDC(nullptr, hdc);
		}
#elif defined(linux) || defined(__linux__)
		int screen_number = 0;
		Display* disp = XOpenDisplay(nullptr);
		const int dw = DisplayWidth(disp, screen_number);
		const int dh = DisplayHeight(disp, screen_number)
		const int dw_mm = DisplayWidthMM(disp, screen_number);
		const int dh_mm = DisplayHeightMM(disp, screen_number);
		if(dw_mm != 0) {
			dpi_x_ = (dw * 2540) / (dw_mm * 100);
		}
		if(dh_mm != 0) {
			dpi_y_ = (dh * 2540) / (dh_mm * 100);
		}
#elif defined(__APPLE__) 
	#include "TargetConditionals.h"
	#if TARGET_OS_IPHONE && TARGET_IPHONE_SIMULATOR
		// simulator
	#elif TARGET_OS_IPHONE
		// iphone
	#else
		// osx
		/*
		NSScreen *screen = [NSScreen mainScreen];
		NSDictionary *description = [screen deviceDescription];
		NSSize displayPixelSize = [[description objectForKey:NSDeviceSize] sizeValue];
		CGSize displayPhysicalSize = CGDisplayScreenSize(
					[[description objectForKey:@"NSScreenNumber"] unsignedIntValue]);

		NSLog(@"DPI is %0.2f", 
				 (displayPixelSize.width / displayPhysicalSize.width) * 25.4f); 
				 // there being 25.4 mm in an inch
		*/
	#endif
#else
#endif
		LOG_INFO("Device DPI in use: " << dpi_x_ << ", " << dpi_y_);
	}
	int getDpiX() const { return dpi_x_; }
	int getDpiY() const { return dpi_y_; }
private:
	int dpi_x_;
	int dpi_y_;
};

// XXX convert this to a helper class
KRE::ColoredFontRenderablePtr text_block_renderer(const std::vector<std::string>& strs, const std::vector<KRE::Color>& colors, float* ts_x, float* ts_y)
{
	static std::vector<std::string> ff;
	if(ff.empty()) {
		ff.emplace_back("SourceCodePro-Regular");
		ff.emplace_back("square");
		ff.emplace_back("whitrabt");
		ff.emplace_back("monospace");
	}

	static DeviceMetrics dm;
	
	static const int font_size = 16;
	static const float fs = static_cast<float>(font_size * dm.getDpiY()) / 72.0f;
	static auto fh = KRE::FontDriver::getFontHandle(ff, fs);
	int y = static_cast<int>(fh->getScaleFactor() * fs);

	if(ts_x != nullptr) {
		*ts_x = static_cast<float>(fh->calculateCharAdvance('M') / 65536.0f);
	}
	if(ts_y != nullptr) {
		*ts_y = fs;
	}

	std::vector<point> final_path;
	std::string concat_op;
	for(auto& op : strs) {
		auto glyph_path = fh->getGlyphPath(op);
		for(auto it = glyph_path.begin(); it != glyph_path.end()-1; ++it) {
			auto& gp = *it;
			final_path.emplace_back(gp.x, gp.y + y);
		}
		concat_op += op;
		y += static_cast<int>(fh->getScaleFactor() * fs);
	}
	return fh->createColoredRenderableFromPath(nullptr, concat_op, final_path, colors);
}

void create_player(engine& e, int dpi_x, int dpi_y)
{
	ASSERT_LOG(e.getMap() != nullptr, "Please add map before creating player.");
	auto& map = e.getMap();

	component_set_ptr player = std::make_shared<component::component_set>(100);
	// Player component simply acts as a tag for the entity
	//font::font_ptr fnt = font::get_font("SourceCodePro-Regular.ttf", 20);
	player->mask |= component::genmask(component::Component::PLAYER);
	player->mask |= component::genmask(component::Component::POSITION);
	player->mask |= component::genmask(component::Component::STATS);
	player->mask |= component::genmask(component::Component::INPUT);
	player->mask |= component::genmask(component::Component::SPRITE);
	player->mask |= component::genmask(component::Component::COLLISION);
	player->pos = std::make_shared<component::position>(map->getStartLocation());
	e.set_camera(player->pos->pos);
	player->stat = std::make_shared<component::stats>();
	player->stat->health = 10;
	player->stat->visible_radius = 5;
	player->stat->armour = 0;
	player->stat->attack = 1;
	player->inp = std::make_shared<component::input>();
	//auto surf = std::make_shared<graphics::surface>(font::render_shaded("@", fnt, graphics::color(255,255,255), graphics::color(255,0,0)));
	//auto surf = std::make_shared<graphics::surface>("images/spritely_fellow.png");
	// XX codify this better.
	std::vector<std::string> ff;
	ff.emplace_back("SourceCodePro-Regular");
	const int font_size = 16;
	const float fs = static_cast<float>(font_size * dpi_y) / 72.0f;
	auto fh = KRE::FontDriver::getFontHandle(ff, fs);
	auto glyph_path = fh->getGlyphPath("@");
	for(auto& pt : glyph_path) {
		pt.y += static_cast<int>(fh->getScaleFactor() * fs);
	}
	auto spr = fh->createRenderableFromPath(nullptr, "@", glyph_path);
	spr->setColor(KRE::Color::colorWhite());

	//std::vector<std::string> strs;
	//strs.emplace_back("@");
	//std::vector<KRE::Color> colors(1, KRE::Color::colorWhite());
	//auto spr = text_block_renderer(strs, colors, nullptr, nullptr);

	player->spr = std::make_shared<component::sprite>(spr);
	e.add_entity(player);
	
	map->updatePlayerVisibility(player->pos->pos, player->stat->visible_radius);
}

class TestRenderable : public KRE::SceneObject
{
public:
	TestRenderable()
		: KRE::SceneObject("TestRenderable"),
		  attribs_(nullptr)
	{
		using namespace KRE;
		setShader(ShaderProgram::getProgram("attr_color_shader"));

		auto as = DisplayDevice::createAttributeSet(false);
		attribs_.reset(new KRE::Attribute<vertex_color>(AccessFreqHint::DYNAMIC, AccessTypeHint::DRAW));
		attribs_->addAttributeDesc(AttributeDesc(AttrType::POSITION, 2, AttrFormat::FLOAT, false, sizeof(vertex_color), offsetof(vertex_color, vertex)));
		attribs_->addAttributeDesc(AttributeDesc(AttrType::COLOR,  4, AttrFormat::UNSIGNED_BYTE, true, sizeof(vertex_color), offsetof(vertex_color, color)));
		as->addAttribute(attribs_);
		as->setDrawMode(DrawMode::TRIANGLE_STRIP);
		as->enableMultiDraw();

		addAttributeSet(as);

		init();
	}
	void clear()
	{
		attribs_->clear();
	}
	void addRect(const rect& r, const KRE::Color& color)
	{
		glm::u8vec4 c = color.as_u8vec4();

		const float x1 = static_cast<float>(r.x1());
		const float y1 = static_cast<float>(r.y1());
		const float x2 = static_cast<float>(r.x2());
		const float y2 = static_cast<float>(r.y2());

		std::vector<KRE::vertex_color> vcarray;
		vcarray.emplace_back(glm::vec2(x1, y1), c);
		vcarray.emplace_back(glm::vec2(x1, y2), c);
		vcarray.emplace_back(glm::vec2(x2, y1), c);
		vcarray.emplace_back(glm::vec2(x2, y2), c);
		attribs_->addMultiDraw(&vcarray);
	}
	void init() 
	{
		addRect(rect(100, 100, 50, 50), KRE::Color::colorPalevioletred());
		addRect(rect(150, 100, 50, 50), KRE::Color::colorRed());
		addRect(rect(100, 150, 50, 50), KRE::Color::colorGreen());
		addRect(rect(150, 150, 50, 50), KRE::Color::colorBlue());
	}
private:
	std::shared_ptr<KRE::Attribute<KRE::vertex_color>> attribs_;
};

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
	FontDriver::setAvailableFonts(font_files);
	FontDriver::setFontProvider("stb");

	mercy::Terrain::load_terrain_data(json::parse_from_file("../data/terrain.cfg"));
	creature::loader(json::parse_from_file("../data/creatures.cfg"));

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
	// Render process needs to be handled slightly differently.
	eng->setRenderProcess(std::make_shared<process::render>());

	// XX move device metrics into KRE DisplayDevice.
	DeviceMetrics dm;

	const int map_width = 100;
	const int map_height = 40;
	variant_builder features;
	features.add("dpi_x", dm.getDpiX());
	features.add("dpi_y", dm.getDpiY());
	eng->setMap(mercy::BaseMap::create("dungeon", map_width, map_height, features.build()));
	//eng->setMap(mercy::BaseMap::create("terrain", map_width, map_height, features.build()));
	eng->getMap()->generate(*eng);

	create_player(*eng, dm.getDpiX(), dm.getDpiY());
	
	eng->add_entity(creature::spawn("gnarled_goblin", eng->getMap()->getStartLocation()-point(1, 1)));

	//auto cave_test = mercy::cave_fixed_param(80, 50);
	//for(auto& line : cave_test) {
	//	std::cout << "    " << line << "\n";
	//}

	/*terrain::terrain terra_firma;
	auto chks = terra_firma.get_chunks_in_area(rect(0, 0, 80, 50));
	for(auto& chk : chks) {
		root->attachObject(chk->get_renderable());
	}*/

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

		scene->renderScene(rman);
		rman->render(main_wnd);

		if(scene_tree != nullptr) {
			scene_tree->preRender(main_wnd);
			scene_tree->render(main_wnd);
		}

		main_wnd->swap();
	}
	return 0;
}
