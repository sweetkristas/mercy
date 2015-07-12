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

#include <set>
#include <vector>
#include "geometry.hpp"

template<typename T, typename R=int, unsigned MAX_OBJECTS=10, unsigned MAX_LEVELS=5>
class quadtree
{
public:
	explicit quadtree(int level, const geometry::Rect<R>& bounds) 
		: level_(level), 
		  bounding_rect_(bounds) 
	{
		nodes_.reserve(4);
	}
	void clear() {
		objects_.clear();
		for(auto& n : nodes_) {
			n.clear();
		}
		nodes_.clear();
	}
	void split() {
		const R x = bounding_rect_.x();
		const R mx = bounding_rect_.mid_x();
		const R y = bounding_rect_.y();
		const R my = bounding_rect_.mid_y();
		const R sw = bounding_rect_.w()/2;
		const R sh = bounding_rect_.h()/2;

		nodes_.emplace_back(level_+1, geometry::Rect<R>(mx,  y, sw, sh));
		nodes_.emplace_back(level_+1, geometry::Rect<R>(x,   y, sw, sh));
		nodes_.emplace_back(level_+1, geometry::Rect<R>(x,  my, sw, sh));
		nodes_.emplace_back(level_+1, geometry::Rect<R>(mx, my, sw, sh));
	}
	void insert(const T& obj, const geometry::Rect<R>& r) {
		if(!nodes_.empty()) {
			int index = get_index(r);
			if(index != -1) {
				nodes_[index].insert(obj, r);
				return;
			}
		}
		objects_.insert(std::make_pair(obj, r));
		if(objects_.size() > MAX_OBJECTS && level_ < MAX_LEVELS) {
			if(nodes_.empty()) {
				split();
			}
			auto it = objects_.begin();
			while(it != objects_.end()) {
				int index = get_index(it->second);
				if(index != -1) {
					nodes_[index].insert(it->first, it->second);
					objects_.erase(it++);
				} else {
					++it;
				}
			}
		}
	}
	void remove(const T& obj) {
		auto it = objects_.begin();
		while(it != objects_.end()) {
			if(it->first == obj) {
				objects_.erase(it++);
			} else {
				++it;
			}
		}
	}
	void get_collidable(std::vector<T>& res, const geometry::Rect<R>& r) {
		int index = get_index(r);
		if(index == -1 && !nodes_.empty()) {
			nodes_[index].get_collidable(res, r);
		}
		for(auto& obj : objects_) {
			res.emplace_back(obj.first);
		}
	}
private:
	int level_;
	geometry::Rect<R> bounding_rect_;
	std::set<std::pair<T,geometry::Rect<R>>> objects_;
	std::vector<quadtree<T, R, MAX_OBJECTS, MAX_LEVELS>> nodes_;

	int get_index(const geometry::Rect<R> r) {
		int index = -1;
		bool tq = r.y() < bounding_rect_.mid_y() && r.y2() < bounding_rect_.mid_y();
		bool bq = r.y() > bounding_rect_.mid_y();
		if(r.x() < bounding_rect_.mid_x() && r.x2() < bounding_rect_.mid_x()) {
			if(tq) {
				index = 1;
			} else if(bq) {
				index = 2;
			}
		} else if(r.x() > bounding_rect_.mid_x()) {
			if(tq) {
				index = 0;
			} else if(bq) {
				index = 3;
			}
		}
		return index;
	}
};
