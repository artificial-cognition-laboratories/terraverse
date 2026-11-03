// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "air/tables.h"
#include "irr_ptr.h"
#include "irrlichttypes_bloated.h"
#include "terragen/system.h"
#include <SMaterial.h>
#include <future>
#include <memory>
#include <optional>

class IShaderSource;
namespace video { class ITexture; class IVideoDriver; }

/*
	The air over a body, drawn over space in the sky pass (planning/arc-atmosphere.md).
	Each star's sky is a small picture (air::skyImage), made on a worker whenever
	that star has moved enough, so the frame only looks it up. A body with no air
	draws nothing, and a star too faint to matter has no picture.
*/
namespace planetarium {

class Air
{
public:
	Air(IShaderSource *ssrc, const terragen::System &system, size_t body);
	~Air();

	bool any() const { return m_tables != nullptr || m_building.valid(); }

	struct Star
	{
		v3f way;           // unit: in the frame the view is given in, y straight up
		double share;      // of the light here, 0..1
		float cos_radius;  // of its disc's angular radius
	};
	// The sight: its axes in that same frame, how wide (tan of half the height, and
	// the aspect). `height`: the eye, as a share of the air's depth (0..1).
	struct View
	{
		v3f ahead, right, up;
		float tan_half, aspect;
	};
	// `drawn_dip`: how far below level the drawn planet's horizon is, radians.
	void update(double height, double drawn_dip, const Star &first, const std::optional<Star> &second);
	void draw(video::IVideoDriver *driver, const View &view);
	// The fog's colour: the sky at the horizon. None till there's a picture, or no air.
	std::optional<video::SColor> horizon() const;
	// What daylight's coloured by here. White with no air.
	const air::Rgb &tint() const { return m_tint; }

private:
	struct Part
	{
		std::future<air::SkyImage> making;
		video::ITexture *texture = nullptr;
		double elevation = 10, height = -1, light = -1; // what its picture was asked for
		air::Rgb horizon{}, sun{};
		bool shown = false;
		Star star{};
	};
	void step(Part &part, video::IVideoDriver *driver);
	void ask(Part &part, double height, double drawn_dip, double real_dip, const Star &star, const air::Rgb &tint);

	std::future<std::shared_ptr<const air::Tables>> m_building;
	std::shared_ptr<const air::Tables> m_tables;
	Part m_parts[2];
	air::Rgb m_tints[2]{};
	double m_sunlight = 1;
	air::Rgb m_tint{ 1, 1, 1 };
	float m_dips[2]{}; // the drawn horizon's and the real air's, radians
	video::SMaterial m_material;
	struct Uniforms;
	irr_ptr<Uniforms> m_uniforms;
	video::IVideoDriver *m_driver = nullptr; // what the textures are on, once drawn
};

} // namespace planetarium
