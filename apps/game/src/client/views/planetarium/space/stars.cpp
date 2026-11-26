// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/client/views/planetarium/space/stars.h"
#include <SColor.h>
#include <algorithm>
#include <cmath>

namespace planetarium::stars {

namespace {

constexpr int STARS = 110000; // a wide lens shows about a fifth: nearly all faint, like the band's own
constexpr int PER_BUFFER = 16000; // (a mesh buffer's indices are 16 bits: 4 vertices a star)

} // namespace

std::vector<irr_ptr<scene::SMeshBuffer>> of(const milkyway::Picture &sky)
{
	std::vector<irr_ptr<scene::SMeshBuffer>> stars;
	// the same sky every time; most faint, a few bright (as magnitudes go).
	// Where the picture's bright there are more: the band's glow comes apart
	// into them, and dust that hides its light hides its stars. Coloured by how hot.
	uint32_t seed = 0x9e3779b9;
	const auto next = [&seed]() { seed = seed * 1664525u + 1013904223u; return (seed >> 8) / 16777216.0; };
	// (pale: a star's colour shows only a little against the dark, as the band's do)
	static const video::SColor HUES[] = { { 255, 205, 220, 255 }, { 255, 232, 238, 255 }, { 255, 255, 250, 242 },
		{ 255, 255, 238, 214 }, { 255, 255, 222, 196 } };
	for (int n = 0; n < STARS;) {
		const double y = next() * 2 - 1, a = next() * 2 * M_PI, r = std::sqrt(1 - y * y);
		const u32 px = std::min(sky.width - 1, (u32)(a / (2 * M_PI) * sky.width));
		const u32 py = std::min(sky.height - 1, (u32)((0.5 - std::asin(y) / M_PI) * sky.height));
		const u8 *at = &sky.rgb[((size_t)py * sky.width + px) * 3];
		const double light = (at[0] + at[1] + at[2]) / (3.0 * 255);
		if (next() > 0.06 + light * light * 3)
			continue;
		if (n % PER_BUFFER == 0) {
			stars.push_back(make_irr<scene::SMeshBuffer>());
			stars.back()->setHardwareMappingHint(scene::EHM_STATIC);
		}
		scene::SMeshBuffer &buffer = *stars.back();
		// Most just over the band's own specks, a few standing out: a point of
		// light and a faint glow round it, never a disc (the quad is the glow's
		// reach; the shader draws a pixel-sized core in it)
		const double bright = std::pow(next(), 12);
		const f32 radius = (f32)(1.1 + 2.6 * bright);
		video::SColor c = HUES[std::min(4, (int)(next() * 5))];
		c.setAlpha((u32)(255 * (0.06 + 0.64 * std::pow(bright, 0.6))));
		const v3f way((f32)(r * std::cos(a)), (f32)y, (f32)(r * std::sin(a)));
		auto &v = buffer.Vertices->Data;
		auto &ix = buffer.Indices->Data;
		const u16 first = (u16)v.size();
		for (const v2f &corner : { v2f(-1, -1), v2f(1, -1), v2f(1, 1), v2f(-1, 1) })
			v.emplace_back(way, v3f(radius, 0, 0), c, corner);
		for (u16 k : { 0, 1, 2, 0, 2, 3 })
			ix.push_back(first + k);
		n++;
	}
	for (const irr_ptr<scene::SMeshBuffer> &buffer : stars)
		buffer->recalculateBoundingBox();
	return stars;
}


std::vector<u32> grain()
{
	// the stars too many to draw: most specks faint, a few bright; stored at a
	// third, so 1 (its mean) is about 85. Its mips average out to smooth glow.
	std::vector<u32> pixels((size_t)GRAIN * GRAIN);
	uint32_t seed = 0x6d2b79f5;
	for (u32 &pixel : pixels) {
		seed = seed * 1664525u + 1013904223u;
		const double u = std::max((seed >> 8) / 16777216.0, 0.002);
		const u32 v = (u32)(std::min(std::pow(u, -0.32) * 0.72, 3.0) / 3 * 255);
		pixel = video::SColor(255, v, v, v).color;
	}
	return pixels;
}


} // namespace planetarium::stars
