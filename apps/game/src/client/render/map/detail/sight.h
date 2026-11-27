// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <algorithm>
#include <cmath>

namespace detail {

/*
	How fine the world is drawn, anywhere on screen, by one rule: a voxel is never
	smaller than `pixels` on screen. Voxels nest in powers of two, a sixteenth of a
	block to the far view's kilometres, so each ring out doubles them and no ring
	is a bigger jump than that. The near meshes (Rings) and the far tiles ask the
	same rule, so the two are one ladder.
*/
struct Sight
{
	// How many pixels a block a block away covers: the screen's height over
	// the view's height there (2 tan(fov_y / 2)). Changes with the window and the zoom.
	float scale;
	// The smallest a voxel may be on screen, pixels (the setting voxel_pixels).
	float pixels;

	// From the screen's height (px) and the view's vertical angle (radians). The scale's
	// rounded to steps of a twelfth of an octave: a zoom easing in moved every
	// ring a little every frame, and every mesh on one was made again.
	static Sight of(float screen_h, float fov_y, float pixels)
	{
		const float exact = screen_h / (2.f * std::tan(fov_y / 2.f));
		const float steps = std::round(std::log2(std::max(exact, 1.f)) * 12.f);
		return { std::exp2(steps / 12.f), pixels };
	}

	// The finest voxel that's still `pixels` across at `distance` blocks: its edge as
	// a power of two of a block (-4: a sixteenth, 0: a block, 3: eight blocks).
	int exponent(float distance) const
	{
		const float edge = pixels * std::max(distance, 1e-3f) / scale; // blocks, at least
		return (int)std::ceil(std::log2(edge));
	}

	// The furthest a voxel of edge 2^exponent blocks is still `pixels` across: where
	// the next coarser takes over. reach(0): where whole blocks end, and the near
	// world with them (the far view's tiles go on from there, by the same rule).
	float reach(int exponent) const { return std::exp2((float)exponent) * scale / pixels; }

	bool operator==(const Sight &o) const { return scale == o.scale && pixels == o.pixels; }
	bool operator!=(const Sight &o) const { return !(*this == o); }
};

} // namespace detail
