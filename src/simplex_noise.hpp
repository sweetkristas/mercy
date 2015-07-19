/* coherent noise function over 1, 2 or 3 dimensions */
/* (copyright Ken Perlin) */

#pragma once

#include <cstdint>

#include <glm/glm.hpp>

namespace noise
{
	namespace simplex
	{
		float noise1(float arg);
		float noise2(const glm::vec2& vec);
		float noise3(const glm::vec3& vec);
	}
}
