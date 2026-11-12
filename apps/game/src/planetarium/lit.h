// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "irr_ptr.h"
#include "irrlichttypes_bloated.h"
#include <SMaterial.h>

class IShaderSource;

/*
	A body's land lit by its star: the side toward it day, the far side night,
	by the same curve as the spheres far bodies are drawn as (SystemLayer::sphere),
	so one becoming the other doesn't jump. Each pixel's way up is from the body's
	centre, so it's the ball that's lit. The land's relief is shaded on top, by
	its tiles' slopes (far/mesh.cpp), from the star and from a fixed light in
	the north-west, as a map's hillshade, so it reads from straight above.
*/
namespace planetarium {

class Lit
{
public:
	explicit Lit(IShaderSource *ssrc);
	~Lit();

	static constexpr int OCCLUDERS = 4;
	struct Light
	{
		v3f centre;     // the body's centre, as its tiles' world transform puts it
		v3f sun;        // unit: the way to the star, in the same frame
		v3f second;     // the second star's (with share 0, not read)
		float share;    // the second's part of the light, 0..1
		bool on;        // false: every side as lit (an overlay is being read)
		// What can stand between it and the star: where (as `centre`) and how big,
		// radius 0 for none. Eclipses, worked out as spheres per pixel.
		float occluders[OCCLUDERS][4];
		float penumbra; // the star's angular radius, radians
		float contour;  // blocks between contour lines on the land; 0 for none
		bool textured = false; // its colours its texture's, not its vertices' (planetarium/globe.h)
	};
	// The material to draw land with, for this body. Set again per body.
	const video::SMaterial &material(const Light &light);

private:
	struct Uniforms;
	irr_ptr<Uniforms> m_uniforms;
	video::SMaterial m_material;
};

} // namespace planetarium
