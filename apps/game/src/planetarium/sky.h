// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "irrlichttypes_bloated.h"
#include "far/terrain.h"
#include "far/tiles.h"
#include "planet/atlas.h"
#include "planetarium/air.h"
#include "planetarium/system.h"
#include <ISceneNode.h>
#include <memory>
#include <optional>

class Camera;
class IShaderSource;
class ClientMap;
namespace far { class Palette; }

/*
	The sky (planning/sprint-60-sky.md): the Planetarium's system seen from
	where the player is. The stock sky is off on a planet; behind the far
	view (which draws the planet itself) is space: the field of stars, the
	star and the other bodies where they are on the world's date, turned as
	the planet has turned by its time of day. No air yet: the sky is black
	by day too, till the atmosphere is drawn.

	Drawn first in the sky pass (made before the far view), its own depth
	ranges, then the depth cleared and the camera's own put back for what
	follows.
*/
namespace planetarium {

class Sky : public scene::ISceneNode
{
public:
	Sky(scene::ISceneManager *smgr, IShaderSource *ssrc, const planet::Atlas &atlas, const far::Horizon &horizon, int body);

	~Sky();

	// The land's colours, for a body near enough to draw from its own tiles.
	// (None till given: spheres.) Outlives the sky.
	void palette(const far::Palette &palette) { m_palette = &palette; }

	// Once a frame, before drawing: the eye, and the world's date (days
	// since its day 0, with the time of day as the fraction).
	void update(const Camera &camera, ClientMap &map, double worldDays);

	// What fog should be, the air's horizon. None: leave it (no air, or none worked out yet).
	std::optional<video::SColor> fog() const { return m_air.horizon(); }

	// What casts the world's shadows: the brighter star, the way to it in the scene's
	// axes, and how dark they are (0: it's down). None: the eye has no grid.
	struct Light
	{
		v3f way;
		float strength;
	};
	const std::optional<Light> &light() const { return m_light; }

	void OnRegisterSceneNode() override;
	void render() override;
	const aabb3f &getBoundingBox() const override { return m_box; }

private:
	SystemLayer m_system;
	const int32_t m_year; // the world's year, in its days
	const int m_body;     // the atlas's body it's over
	aabb3f m_box{ -1e7f, -1e7f, -1e7f, 1e7f, 1e7f, 1e7f };

	// this frame's, in space (the system's frame round the body)
	struct Eye
	{
		terragen::System::Vec at; // from the body's centre, blocks
		core::matrix4 view;       // camera-relative
		float fov;                // radians, vertical
		double day;               // the system's
		double worldDays;         // the world's, as update was given
	};
	std::optional<Eye> m_eye; // none: the eye has no grid (seam)
	Air m_air;
	std::optional<Light> m_light;
	std::string m_said; // what the log last said of the sky's state
	void say(const std::string &state);
	double m_airHeight = 0; // the eye, as a share of the air's depth; 1: above it
	Air::View m_sight{};

	// A body that fills enough of the sky (a double planet's other half) is its
	// own land, seen as the Planetarium sees it, by its own clock, with a night side.
	struct Baked
	{
		int body;     // the atlas's
		size_t index; // the system's
		double sea;   // its sea's radius (its own blocks)
		std::unique_ptr<far::Tiles> tiles; // made when first big enough
	};
	std::vector<Baked> m_baked;
	const planet::Atlas m_atlas;
	const far::Horizon m_horizon;
	const far::Palette *m_palette = nullptr;
	bool drawBaked(video::IVideoDriver *driver, size_t index, const terragen::System::Vec &at, double pixels,
			float near, float far, float aspect, u32 height);
};

} // namespace planetarium
