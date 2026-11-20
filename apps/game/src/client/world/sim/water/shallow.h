// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

/*
	Shallow water by virtual pipes (Mei, Decaudin & Hu 2007), a column at a time,
	and where its top is drawn: the rules liquids::Layer runs near the camera,
	with nothing of a map in them, so the wiki's scenes run the same water.
*/
namespace liquids::shallow {

constexpr float G = 9.81f;       // nodes/s² (a node is a metre)
constexpr float DAMP = 1.2f;     // per s: how fast a flow dies away (sloshing settles in a few seconds)
constexpr float FRICTION = 1.5f; // per s, over a sixteenth's depth: the ground drags thin water still

// A column's flows out of each side next step. `open`: there's a column there; `higher`: how far this
// surface stands over that one (nodes). `L`: a column's width (nodes). No column gives more than it has.
inline void flowOut(std::array<float, 4> &out, float d, float dt, float L, const bool open[4], const float higher[4])
{
	const float slowed = std::max(0.0f, 1 - DAMP * dt - dt * FRICTION / std::max(d * 16, 0.25f));
	const float speed = std::min(d, 0.5f); // (past this, deep water's waves outrun the grid and it stands in spikes)
	float sum = 0;
	for (int s = 0; s < 4; s++) {
		out[s] = open[s] ? std::max(0.0f, out[s] * slowed + dt * G * speed * higher[s] / L) : 0;
		sum += out[s];
	}
	if (sum * dt > d * L) {
		const float k = d * L / (sum * dt);
		for (float &o : out)
			o *= k;
	}
}

// Where a column's top is drawn, on a sixteenth, moving only when its water's well past it (still water
// stays still on screen); any water at all is a sixteenth deep at least. `none`: nothing shown.
inline int32_t topShown(int32_t shown, int32_t floor, float depth, int32_t none)
{
	const float d = depth * 16, raw = floor + d;
	const int32_t least = floor + 1;
	if (shown == none ? d >= 0.2f : d < 0.08f)
		return d < 0.08f ? none : std::max(least, (int32_t)std::lround(raw));
	if (shown != none && std::fabs(raw - shown) > 0.7f)
		return std::max(least, (int32_t)std::lround(raw));
	return shown;
}

} // namespace liquids::shallow
