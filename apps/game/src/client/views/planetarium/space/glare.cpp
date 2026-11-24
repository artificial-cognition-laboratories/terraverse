// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/client/views/planetarium/space/glare.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <mutex>

namespace planetarium::glare {

namespace {
std::shared_ptr<const std::vector<u32>> paint(video::SColor colour);
}

std::shared_ptr<const std::vector<u32>> pixels(video::SColor colour)
{
	// (a few colours a run, a megabyte each: the star classes a system has)
	static std::mutex lock;
	static std::map<u32, std::shared_ptr<const std::vector<u32>>> painted;
	std::lock_guard guard(lock);
	auto found = painted.find(colour.color);
	if (found == painted.end())
		found = painted.emplace(colour.color, paint(colour)).first;
	return found->second;
}

namespace {

std::shared_ptr<const std::vector<u32>> paint(video::SColor colour)
{
	// light as a lens takes it, added to what's behind: white where it's blown
	// out, the star's own colour as it thins
	std::vector<u32> pixels(SIZE * SIZE);
	const double tint[3] = { colour.getRed() / 255.0, colour.getGreen() / 255.0, colour.getBlue() / 255.0 };
	const auto spike = [](double along, double across, double length, double width) {
		// a thin line through the middle, thinning to nothing at its ends
		const double a = std::abs(along) / length;
		if (a >= 1)
			return 0.0;
		const double w = width * (1 - a * 0.7);
		return std::pow(1 - a, 2.5) * std::exp(-across * across / (w * w));
	};
	uint32_t seed = 0x2f6e2b1;
	for (u32 y = 0; y < SIZE; y++)
		for (u32 x = 0; x < SIZE; x++) {
			const double dx = (x + 0.5) / SIZE * 2 - 1, dy = (y + 0.5) / SIZE * 2 - 1;
			const double r = std::max(std::hypot(dx, dy), 1e-4);
			const double edge = r >= 1 ? 0 : std::pow(1 - r * r, 2);
			const double core = std::clamp((CORE * 1.25 - r) / (CORE * 0.5), 0.0, 1.0);
			const double corona = std::pow(CORE / r, 1.5) * 0.9;
			const double halo = 0.26 * std::exp(-r * 5) + 0.05 * std::exp(-r * 2);
			// the long pair across the star, tilted as a lens's would be; a shorter pair square to it
			constexpr double TILT = 0.3;
			const double c = std::cos(TILT), s = std::sin(TILT), u = dx * c + dy * s, v = -dx * s + dy * c;
			const double spikes = 0.9 * spike(u, v, 0.98, 0.007) + 0.6 * spike(v, u, 0.6, 0.006);
			const double white = std::min(1.0, core + corona * 0.8 + spikes * 0.7) * edge;
			const double thin = (corona * 0.4 + halo + spikes * 0.3) * edge;
			// (a shade of noise: long fades on black band without it)
			seed = seed * 1664525u + 1013904223u;
			const double dither = ((seed >> 8) / 16777216.0 - 0.5) / 255;
			u32 out[3];
			for (int i = 0; i < 3; i++)
				out[i] = (u32)(std::clamp(white + thin * tint[i] + (white + thin > 0.002 ? dither : 0), 0.0, 1.0) * 255);
			pixels[y * SIZE + x] = video::SColor(255, out[0], out[1], out[2]).color;
		}
	return std::make_shared<const std::vector<u32>>(std::move(pixels));
}

} // namespace

} // namespace planetarium::glare
