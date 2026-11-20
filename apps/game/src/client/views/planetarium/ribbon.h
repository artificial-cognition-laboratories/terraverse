// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/shared/core/irrlichttypes_bloated.h"
#include <S3DVertex.h>
#include <SColor.h>
#include <vector>

namespace video { class IVideoDriver; }

namespace planetarium {

/*
	Lines a few pixels wide (orbits, the player's path): a line's points
	drawn as a strip facing the camera, as wide on screen wherever it is,
	solid down its middle and soft at its edges. A driver's lines are a
	pixel wide whatever's asked, and look ragged at that.
*/
class Ribbons
{
public:
	// `focal`: pixels per unit of depth at a unit away (the screen's height / 2·tan(fov / 2)).
	Ribbons(float width, double focal) : m_half(width / 2), m_focal(focal) {}

	// A line through points (camera-relative, the camera at the origin) in
	// their colours; closed: its last point joins its first.
	void add(const std::vector<v3f> &points, const std::vector<video::SColor> &colours, bool closed);

	// Drawn in the transforms set (the view's turn only, at the origin);
	// depth tested or not.
	void draw(video::IVideoDriver *driver, bool depth) const;

private:
	float m_half;
	double m_focal;
	std::vector<video::S3DVertex> m_vertices;
	std::vector<u32> m_indices;
};

} // namespace planetarium
