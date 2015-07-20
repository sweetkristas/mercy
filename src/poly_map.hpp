/*
	Copyright (C) 2012-2015 by Kristina Simpson <sweet.kristas@gmail.com>
	
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

#include <glm/glm.hpp>

#include "SceneFwd.hpp"
#include "geometry.hpp"

namespace geometry
{
	typedef std::vector<glm::vec2> fpoint_list;	

	struct edge
	{
		glm::vec2 p1;
		glm::vec2 p2;
		edge(const glm::vec2& a, const glm::vec2& b) : p1(a), p2(b) {}
	};

	class Polygon
	{
	public:
		Polygon(int id) : id_(id), height_(0) {}
		
		void addPoint(float x, float y) {
			pts_.emplace_back(x, y);
		}
		void setHeight(int height) {
			height_ = height;
		}
		void init();
		void normalise() {
			pts_.erase(std::unique(pts_.begin(), pts_.end()), pts_.end());
		}
		void calculateCentroid(glm::vec2* centroid) {
			centroid->x = 0;
			centroid->y = 0;
			for(auto& p : pts_) {
				centroid->x += p.x;
				centroid->y += p.y;
			}
			centroid->x /= static_cast<float>(pts_.size());
			centroid->y /= static_cast<float>(pts_.size());
		}

		void setCentroid(const glm::vec2& ct) { centroid_ = ct; }
		void setColor(const KRE::Color& c) { color_ = c; }

		int getId() const { return id_; }
		int height() const { return height_; }
		const glm::vec2& getCentroid() const { return centroid_; }
		const std::vector<glm::vec2>& getPoints() const { return pts_; }
		const KRE::Color& getColor() const { return color_; }
		const std::vector<glm::vec2>& getTriangleFan() const { return varray_; }
	private:
		std::vector<glm::vec2> pts_;
		int id_;

		// A somewhat nebulous parameter
		int height_;

		// Stuff for drawing.
		// Constructed triangle fan
		std::vector<glm::vec2> varray_;
		// Color of polygon
		KRE::Color color_;
		// edges for drawing black border.
		std::vector<glm::vec2> vedges_;

		glm::vec2 centroid_;

		Polygon() = delete;
		Polygon(const Polygon&) = delete;
	};
	typedef std::shared_ptr<Polygon> PolygonPtr;

	std::ostream& operator<<(std::ostream& os, const Polygon& poly);

	namespace voronoi
	{
		class Wrapper
		{
		public:
			Wrapper(const fpoint_list& pts, int relaxations=1, float left=0, float top=0, float right=0, float bottom=0);
			~Wrapper();

			float left() const { return left_; }
			float right() const { return right_; }
			float top() const { return top_; }
			float bottom() const { return bottom_; }

			const std::vector<edge>& getEdges() const { return output_; }
			const std::vector<PolygonPtr>& getPolys() const { return polygons_; }
			const fpoint_list& getSites() const { return sites_; }
		private:
			// bounding box
			float left_;
			float top_; 
			float right_; 
			float bottom_;

			fpoint_list sites_;

			void calculateBoundingBox(const fpoint_list& pts);
			void generate(fpoint_list& pts);

			std::vector<edge> output_;
			std::vector<PolygonPtr> polygons_;

			Wrapper();
			Wrapper(const Wrapper&);
		};

		std::ostream& operator<<(std::ostream& os, const Wrapper& obj);
	}

	class PolyMap
	{
	public:
		explicit PolyMap(int n_pts, int relaxations_, int width, int height);
		explicit PolyMap(const variant& v, int width, int height);
		void init();
		KRE::SceneObjectPtr createRenderable();
	private:
		int npts_;
		int relaxations_;
		std::vector<glm::vec2> pts_;
		std::vector<glm::vec2> edges_;
		int width_;
		int height_;

		// Controls the island-ness of the terrain.
		float noise_multiplier_;
		int height_adjust_;

		std::vector<PolygonPtr> polygons_;

		PolyMap() = delete;
	};
}
