// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <algorithm>
#include <array>
#include <cmath>

// How a liquid's surface looks, for all that draw one (the game's near water
// and the wiki's: liquid_shader's tops use `light` and `foam` as given here).
// Colour shows what a column's height can't: it only moves in sixteenths, but
// its water's depth is a smooth number, so ripples far smaller than that are seen.
namespace liquids::look {

constexpr float RIPPLE = 1.2f;      // light per unit of slope
constexpr float MOST_RIPPLE = 0.2f; // and at most
constexpr float SHADES = 0.05f;     // a column's light is on a step of this: pixel art, not a gradient
constexpr float FOAM_FROM = 1.2f;   // nodes/s: running faster than this starts to go pale
constexpr float FOAM_FULL = 4.0f;
constexpr float FOAM_MOST = 0.35f;
constexpr float CLIFF = 0.25f;      // nodes: a step this big beside it's a ledge or a wall, not a ripple

struct Surface
{
	float light = 1, foam = 0;
};

// A column's top (nodes), what's beside it (+x −x +z −z: a top, or below −1e8 for nothing),
// its width (nodes), flow out of it (node²/s) and depth (nodes).
inline Surface surface(float here, const std::array<float, 4> &beside, float width, const std::array<float, 4> &out, float depth)
{
	const auto near = [&](float other) { return other < -1e8f || std::fabs(other - here) > CLIFF ? here : other; };
	const float gx = (near(beside[0]) - near(beside[1])) / (2 * width), gz = (near(beside[2]) - near(beside[3])) / (2 * width);
	// lit from one side, as blocks are: a slope facing it is lighter
	const float tilt = std::clamp(-(gx + gz) * 0.7f * RIPPLE, -MOST_RIPPLE, MOST_RIPPLE);
	Surface s;
	s.light = 1 + std::round(tilt / SHADES) * SHADES;
	const float speed = (out[0] + out[1] + out[2] + out[3]) / std::max(depth, 0.02f);
	s.foam = std::round(std::clamp((speed - FOAM_FROM) / (FOAM_FULL - FOAM_FROM), 0.f, 1.f) * 3) / 3 * FOAM_MOST;
	return s;
}

// What something this hot gives off, as black body radiation: its colour at
// the eye's three wavelengths (Planck's law, then to the screen's gamma), and
// how much of what's seen is that and not its own colour. Nothing to see below Draper's point.
struct Glow
{
	float r = 0, g = 0, b = 0, share = 0;
};

inline Glow glow(double kelvin)
{
	constexpr double DRAPER = 798, BRIGHT = 1400; // K: first seen; outshining its own colour
	if (kelvin <= DRAPER)
		return {};
	const auto planck = [&](double micrometres) {
		return 1 / (std::pow(micrometres, 5) * (std::exp(14388 / (micrometres * kelvin)) - 1));
	};
	const double r = planck(0.61), g = planck(0.55), b = planck(0.465), most = std::max({ r, g, b });
	const auto screen = [&](double v) { return (float)std::pow(v / most, 0.5); };
	const double up = std::min(1.0, (kelvin - DRAPER) / (BRIGHT - DRAPER));
	return { screen(r), screen(g), screen(b), (float)(up * up * (3 - 2 * up)) * 0.92f };
}

} // namespace liquids::look
