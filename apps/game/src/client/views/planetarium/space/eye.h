// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/shared/base/irrlichttypes_bloated.h"
#include "library/world/terragen/src/system/system.h"
#include <matrix4.h>
#include <chrono>
#include <optional>
#include <vector>

/*
	How open the eye is, as an eye (or a camera) adjusts to what it's looking
	at: with the star or a sunlit planet in view it closes down and the sky
	goes nearly black; looking out at empty sky it opens up, slowly, and the
	galaxy comes out. 1 is wide open (dark-adapted), down to about half with the
	star itself in view. It closes fast and opens slowly, as eyes do. Visual
	only: not part of any world's determinism.
*/
namespace planetarium {

class Eye
{
public:
	using Vec = terragen::System::Vec;

	// Something bright in the scene, from the camera (blocks)
	struct Thing
	{
		Vec at;
		double radius;
		bool star; // a star; else a body, lit by the star
	};

	// What's in view this frame
	struct View
	{
		core::matrix4 view;          // camera-relative (at the origin)
		float fov, aspect;           // radians (vertical), width / height
		std::vector<Thing> things;
		Vec star;                    // the star that lights them, from the camera
	};

	Eye() = default;

	// How open the eye is now, having seen `view` since the last frame.
	float look(const View &view);

private:
	std::optional<float> m_open; // none: nothing seen yet, so it starts adjusted
	std::chrono::steady_clock::time_point m_last;
};

} // namespace planetarium
