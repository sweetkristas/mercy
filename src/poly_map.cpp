/*
	Copyright (C) 2012-2014 by Kristina Simpson <sweet.kristas@gmail.com>
	
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

#include <iostream>
#include <memory>

#include "AttributeSet.hpp"
#include "DisplayDevice.hpp"
#include "SceneObject.hpp"
#include "SceneUtil.hpp"
#include "Shaders.hpp"

#include "asserts.hpp"
#include "poly_map.hpp"
#include "simplex_noise.hpp"
#include "VoronoiDiagramGenerator.h"

namespace geometry
{
	namespace
	{
		static bool draw_borders = true;

		// XXX: centralise the hsv->rgb, rgb->hsv conversion functions 
		// somewhere (maybe add to graphics::color as well)
		struct rgb
		{
			uint8_t r, g, b;
		};

		struct hsv
		{
			uint8_t h, s, v;
		};

		hsv rgb_to_hsv(uint8_t r, uint8_t g, uint8_t b)
		{
			hsv out;
			uint8_t min_color, max_color, delta;

			min_color = std::min(r, std::min(g, b));
			max_color = std::max(r, std::max(g, b));

			delta = max_color - min_color;
			out.v = max_color;
			if(out.v == 0) {
				out.s = 0;
				out.h = 0;
				return out;
			}

			out.s = uint8_t(255.0 * delta / out.v);
			if(out.s == 0) {
				out.h = 0;
				return out;
			}

			if(r == max_color) {
				out.h = uint8_t(43.0 * (g-b)/delta);
			} else if(g == max_color) {
				out.h = 85 + uint8_t(43.0 * (b-r)/delta);
			} else {
				out.h = 171 + uint8_t(43.0 * (r-g)/delta);
			}
			return out;
		}

		rgb hsv_to_rgb(uint8_t h, uint8_t s, uint8_t v)
		{
			rgb out;
			uint8_t region, remainder, p, q, t;

			if(s == 0) {
				out.r = out.g = out.b = v;
			} else {
				region = h / 43;
				remainder = (h - (region * 43)) * 6; 

				p = (v * (255 - s)) >> 8;
				q = (v * (255 - ((s * remainder) >> 8))) >> 8;
				t = (v * (255 - ((s * (255 - remainder)) >> 8))) >> 8;

				switch(region)
				{
					case 0:  out.r = v; out.g = t; out.b = p; break;
					case 1:  out.r = q; out.g = v; out.b = p; break;
					case 2:  out.r = p; out.g = v; out.b = t; break;
					case 3:  out.r = p; out.g = q; out.b = v; break;
					case 4:  out.r = t; out.g = p; out.b = v; break;
					default: out.r = v; out.g = p; out.b = q; break;
				}
			}
			return out;
		}

		class PolyMapRenderable : public KRE::SceneObject
		{
		public:
			PolyMapRenderable()
				: KRE::SceneObject("PolyMapRenderable"),
				  attribs_(nullptr)
			{
				using namespace KRE;
				setShader(ShaderProgram::getProgram("attr_color_shader"));

				auto as = DisplayDevice::createAttributeSet(false);
				attribs_.reset(new KRE::Attribute<KRE::vertex_color>(AccessFreqHint::DYNAMIC, AccessTypeHint::DRAW));
				attribs_->addAttributeDesc(AttributeDesc(AttrType::POSITION, 2, AttrFormat::FLOAT, false, sizeof(vertex_color), offsetof(vertex_color, vertex)));
				attribs_->addAttributeDesc(AttributeDesc(AttrType::COLOR,  4, AttrFormat::UNSIGNED_BYTE, true, sizeof(vertex_color), offsetof(vertex_color, color)));
				as->addAttribute(attribs_);
				as->setDrawMode(DrawMode::TRIANGLE_FAN);
				as->enableMultiDraw();

				addAttributeSet(as);
			}
			void clear()
			{
				attribs_->clear();
			}
			void update(std::vector<KRE::vertex_color>* vertices) 
			{
				attribs_->addMultiDraw(vertices);
			}
		private:
			std::shared_ptr<KRE::Attribute<KRE::vertex_color>> attribs_;
		};
	}

	namespace voronoi
	{
		Wrapper::Wrapper(const fpoint_list& pts, int relaxations, float left, float top, float right, float bottom)
			: left_(left), 
			  right_(right), 
			  top_(top), 
			  bottom_(bottom)
		{
			if(left == 0 && right == 0 && top == 0 && bottom == 0) {
				left_ = std::numeric_limits<float>::max();
				top_ = std::numeric_limits<float>::max();
				right_ = std::numeric_limits<float>::min();
				bottom_ = std::numeric_limits<float>::min();
				calculateBoundingBox(pts);
			}

			ASSERT_LOG(relaxations > 0, "Number of relaxation cycles must be at least 1: " << relaxations);

			sites_.assign(pts.begin(), pts.end());
			for(int n = 0; n != relaxations; ++n) {
				generate(sites_);
			}
		}

		Wrapper::~Wrapper()
		{
		}

		void Wrapper::generate(fpoint_list& pts)
		{
			polygons_.clear();
			std::unique_ptr<SourcePoint[]> srcpts(new SourcePoint[pts.size()]);

			for(int n = 0; n != pts.size(); ++n) {
				srcpts[n].x = pts[n].x;
				srcpts[n].y = pts[n].y;
				srcpts[n].id = n;
				srcpts[n].weight = 0.0;
			}

			VoronoiDiagramGenerator v;
			v.generateVoronoi(srcpts.get(), static_cast<int>(pts.size()), left_, right_, top_, bottom_);
			for(int n = 0; n != pts.size(); ++n) {
				int npoints = 0;
				PolygonPoint* pp = nullptr;
				v.getSitePoints(n, &npoints, &pp);
				PolygonPtr poly = std::make_shared<Polygon>(n);
				for(int m = 0; m != npoints; ++m) {
					poly->addPoint(pp[m].coord.x, pp[m].coord.y);
				}
				poly->normalise();
				poly->calculateCentroid(&pts[n]);
				poly->setCentroid(pts[n]);
				polygons_.emplace_back(poly);
			}
		}

		void Wrapper::calculateBoundingBox(const fpoint_list& pts)
		{
			for(auto& pt : pts) {
				if(pt.x < left_) {
					left_ = pt.x;
				}
				if(pt.x > right_) {
					right_ = pt.x;
				}
				if(pt.y < top_) {
					top_ = pt.y;
				}
				if(pt.y > bottom_) {
					bottom_ = pt.y;
				}
			}
			// enlarge the bounding box a little.
			float dx = (right_-left_+1.0f)/5.0f;
			float dy = (bottom_-top_+1.0f)/5.0f;
			left_ -= dx;
			right_ += dx;
			top_ -= dy;
			bottom_ += dy;
		}

		std::ostream& operator<<(std::ostream& os, const Wrapper& obj)
		{
			os << "Bounding box: " << obj.left() << "," << obj.top() << "," << obj.right() << "," << obj.bottom() << std::endl;
			auto& segs = obj.getEdges();
			for(auto& s : segs) {
				os << s.p1.x << "," << s.p1.y << " " << s.p2.x << "," << s.p2.y << std::endl;
			}
			return os;
		}
	}

	std::ostream& operator<<(std::ostream& os, const Polygon& poly)
	{
		os << "POLYGON(" << poly.getId() << "," << poly.getPoints().size() << "," << poly.height() << ") :" << std::endl;
		for(auto& p : poly.getPoints()) {
			os << "  " << p.x << "," << p.y << std::endl;
		}
		return os;
	}

	PolyMap::PolyMap(int npts, int relaxations, int width, int height) 
		: npts_(npts), 
		  relaxations_(relaxations), 
		  pts_(),
		  edges_(),
		  width_(width),
		  height_(height)
	{
		init();
	}

	PolyMap::PolyMap(const variant& v, int width, int height) 
		: npts_(v["points"].as_int32(10)), 
		  relaxations_(v["relaxations"].as_int32(2)),
		  noise_multiplier_(1.5f)
	{
		if(v.has_key("island_multiplier")) {
			noise_multiplier_ = float(v["island_multiplier"].as_float());
		}

		init();
	}

	void PolyMap::init()
	{
		// Generate an intial random series of points
		pts_.clear();
		for(int n = 0; n != npts_; ++n) {
			pts_.emplace_back(std::rand() % (width_-4)+2, std::rand() % (height_-4)+2);
		}

		// Calculate voronoi polygons, running multiple Lloyd relaxation cycles.
		geometry::voronoi::Wrapper v(pts_, relaxations_, 0.0f, 0.0f, static_cast<float>(width_), static_cast<float>(height_));
		
		hsv base_color = rgb_to_hsv(112, 144, 95);

		// Set heights via simplex noise
		for(auto& p : v.getPolys()) {
			glm::vec2 vec;
			vec[0] = static_cast<float>(p->getCentroid().x/width_*noise_multiplier_);
			vec[1] = static_cast<float>(p->getCentroid().y/height_*noise_multiplier_);
			p->setHeight(static_cast<int>(noise::simplex::noise2(vec)*256.0f));
			
			if(p->height() < 0) {
				p->setColor(KRE::Color(52, 58, 94));
			} else {
				rgb col = hsv_to_rgb(base_color.h, base_color.s, static_cast<uint8_t>(base_color.v * p->height()/200.0f+128.0f));
				p->setColor(KRE::Color(col.r, col.g, col.b));
			}
		}

		for(auto& p : v.getPolys()) {
			auto& points = p->getPoints();
			for(int n = 1; n != points.size(); ++n) {
				edges_.emplace_back(points[n-1].x, points[n-1].y);
				edges_.emplace_back(points[n].x, points[n].y);
			}
		}
		pts_.assign(v.getSites().begin(), v.getSites().end());

		auto& polys = v.getPolys();
		polygons_.assign(polys.begin(), polys.end());

		for(auto& p : polygons_) {
			p->init();
		}
	}

	KRE::SceneObjectPtr PolyMap::createRenderable()
	{
		auto polyr = std::make_shared<PolyMapRenderable>();

		for(auto& p : polygons_) {
			std::vector<KRE::vertex_color> vertices;
			vertices.reserve(p->getTriangleFan().size());
			for(auto& pt : p->getTriangleFan()) {
				vertices.emplace_back(pt, p->getColor().as_u8vec4());
				LOG_DEBUG("   XXX: pt: " << pt.x << "," << pt.y);
			}
			polyr->update(&vertices);
		}
		return polyr;
	}

	void Polygon::init() 
	{
		if(pts_.size() > 0) {
			varray_.emplace_back(getCentroid().x, getCentroid().y); 
			for(auto& p : pts_) {
				varray_.emplace_back(p.x, p.y);
			}
			// close the loop
			varray_.emplace_back(varray_[1]);

			for(int n = 1; n != pts_.size(); ++n) {
				vedges_.emplace_back(pts_[n-1].x, pts_[n-1].y);
				vedges_.emplace_back(pts_[n].x, pts_[n].y);
			}
		}
	}

}
