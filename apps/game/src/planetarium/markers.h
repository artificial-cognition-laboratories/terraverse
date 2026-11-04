// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "far/tiling.h"
#include <S3DVertex.h>
#include <SColor.h>
#include <vector>

namespace video { class IVideoDriver; }

namespace planetarium {

/*
	Places marked on the body (the player, later anything the page asks
	for): a mast up from the ground with a ring round its foot, sized to be
	seen from wherever the camera is. A marker in the air has its ring up
	there, on a line from the ground under it.
*/
class MarkerLayer
{
public:
	using Vec = far::Tiling::Vec;

	struct Marker
	{
		double latitude, longitude; // radians
		video::SColor colour;
		double altitude = 0; // blocks over the sea: what's marked may be in the air
	};

	explicit MarkerLayer(double sea) : m_sea(sea) {}

	void set(std::vector<Marker> markers) { m_markers = std::move(markers); }

	// From `eye` (blocks from the centre), `altitude` over the sea; view and
	// projection already set.
	void draw(video::IVideoDriver *driver, const Vec &eye, double altitude);

private:
	const double m_sea;
	std::vector<Marker> m_markers;
	std::vector<video::S3DVertex> m_vertices;
	std::vector<u16> m_indices;
};

} // namespace planetarium
