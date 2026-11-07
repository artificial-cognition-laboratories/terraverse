// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "irrlichttypes_bloated.h"
#include "irr_ptr.h"
#include "planetarium/lit.h"
#include "planetarium/rim.h"
#include "terragen/system.h"
#include <CMeshBuffer.h>
#include <functional>
#include <memory>
#include <optional>
#include <map>
#include <vector>

class IShaderSource;
namespace video { class IVideoDriver; class ITexture; }

/*
	The rest of a body's system, round it (planning/sprint-35-system.md): the
	star, the other planets and moons, their orbits. In the focused body's
	units (blocks, its centre the origin; the system's km × blocksPerKm),
	each drawn relative to the camera in doubles. Every body is drawn at the
	detail its distance calls for: a lit sphere coloured by its kind, or a
	dot where it would be under a couple of pixels; the star a bright sphere
	and a glow, the light all the spheres are lit by. Behind it all, a field
	of stars; each orbit in its body's colour, bright where the body has just
	been and fading round to where it's going (the hovered one brightest). Far to near, each body
	with its own depth range, so the far side of the system and the ground
	under the camera share one frame. The focused body is drawn by whoever
	owns it (the Planetarium: its tiles), in its turn.
*/
namespace planetarium {

class Galaxy;

class SystemLayer
{
public:
	struct Opts
	{
		std::shared_ptr<const terragen::System> system;
		size_t focus;       // the body at the origin, drawn by the caller
		double blocksPerKm; // the focused body's scale
		bool orbits = true; // their paths drawn (a viewer's aid: not in a sky seen from the ground)
		bool glare = true;  // the star dazzles, over all that's nearer (a camera's: not in a sky seen from the ground)
	};
	// (The scene is the focused body's frame: its centre the origin, its y
	// its spin axis, tilted from its orbit's by its tilt.)
	explicit SystemLayer(Opts opts);
	~SystemLayer();

	struct Camera
	{
		terragen::System::Vec at; // from the focused body's centre, blocks
		core::matrix4 view;       // camera-relative (at the origin)
		float fov, aspect;        // radians (vertical), width / height
		v2f shift;                // where the view's middle is drawn (OrbitCamera::perspective)
		u32 height;               // pixels
	};
	// The system `day` days after its day 0, the scene begun. `focused`
	// draws the focused body, given the depth range it may use (BS units).
	// `surface` may draw another body's land in its place (its index, where
	// it is from the camera and how wide it is drawn, pixels, in the depth
	// range given), saying whether it did: if not, it's a sphere in its colour.
	// What bodies are lit and edged with. Before the first draw; a window's needed.
	void shaders(IShaderSource *ssrc);
	// The material for a body's land: `centre` where it's put (BS), `turn` how far the frame
	// it's drawn in has turned from the scene's (the tiled body's spin; else 0). Its star
	// and second star, and what of its neighbours could eclipse it, in that frame.
	const video::SMaterial &lit(size_t body, double day, const v3f &centre, double turn, bool on, float contour = 0);
	using Surface = std::function<bool(size_t body, const terragen::System::Vec &at, double pixels, float near, float far)>;
	void draw(video::IVideoDriver *driver, const Camera &camera, double day,
			const std::function<void(float near, float far)> &focused, const Surface &surface = nullptr);

	// The farthest any body is from the focused one (blocks): how far out a
	// camera may want to go to see it all.
	double extent() const { return m_extent; }
	// Where a body is in the scene `day` days on (blocks), and how big (blocks).
	terragen::System::Vec centre(size_t body, double day) const;
	terragen::System::Vec starCentre(double day) const;
	// How far the focused body has turned (radians, its ground to space)
	// `day` days on at a time of day (0..1, noon at ½ on longitude 0, as
	// the game's sun has it).
	double spin(double day, double timeOfDay) const;
	// Any body's, at its own time of day (its System::clock's).
	double spinOf(size_t body, double day, double timeOfDay) const;
	// The way to the second star from the focused body, unit, in the scene.
	terragen::System::Vec toSecond(double day) const;
	// The way to the star from a body, unit, in the scene.
	terragen::System::Vec toStar(size_t body, double day) const;
	double radius(size_t body) const { return m_opts.system->radius(body) * m_opts.blocksPerKm; }
	double starRadius() const { return m_opts.system->starRadius() * m_opts.blocksPerKm; }
	const terragen::System &system() const { return *m_opts.system; }
	size_t focus() const { return m_opts.focus; }
	// A body drawn cut open (none: none): its shells from its card (core,
	// mantle, crust, ocean, …) as spheres, a window cut in them toward the
	// camera. The focused body too: then it's drawn here, not by its owner.
	void cutaway(std::optional<size_t> body) { m_cutaway = body; }
	// A body whose orbit is drawn bright (the one under the mouse), or none.
	void highlight(std::optional<size_t> body) { m_highlight = body; }

private:
	struct Body
	{
		terragen::System::Vec at; // from the camera, blocks
		double radius;            // blocks
		double distance;          // from the camera, blocks
		int index;                // the system's body; −1 the star
	};
	void sphere(video::IVideoDriver *driver, const Body &b, video::SColor colour, double day);
	void star(video::IVideoDriver *driver, const Body &b, video::SColor colour);
	void rim(video::IVideoDriver *driver, const Camera &camera, const Body &b, double day, float near, float far);
	void glow(video::IVideoDriver *driver, const Camera &camera, const Body &star);
	void shells(video::IVideoDriver *driver, const Body &b, const terragen::System::Vec &sun);
	void orbits(video::IVideoDriver *driver, const Camera &camera, double day);
	void glare(video::IVideoDriver *driver, const Camera &camera, const std::vector<Body> &bodies);
	video::ITexture *glareOf(video::IVideoDriver *driver, video::SColor colour);
	std::map<u32, video::ITexture *> m_glares; // by the star's colour
	float m_skyLevel = 1;
	bool m_orbits; // Opts::orbits, or as orbits(bool) has it now
public:
	void orbits(bool shown) { m_orbits = shown; }
	void skyLevel(float level) { m_skyLevel = level; }
private:

	// a vector of the system (km, the ecliptic's axes) in the scene's (blocks)
	terragen::System::Vec scene(const terragen::System::Vec &km) const;

	const Opts m_opts;
	std::unique_ptr<Lit> m_lit;
	std::unique_ptr<Rim> m_rim;
	double m_tilt; // radians: the focused body's
	double m_extent = 0;
	std::optional<size_t> m_cutaway;
	std::optional<size_t> m_highlight;
	// the sky: soft dots round the unit sphere and the galaxy's band behind
	// them, made for a screen's height
	std::unique_ptr<Galaxy> m_galaxy; // the stars behind it all, made with the shaders
	std::vector<u16> m_window; // a cut sphere's triangles, this frame
	irr_ptr<scene::SMeshBuffer> m_sphere; // unit, its normals its points
	video::ITexture *m_glow = nullptr;
	video::IVideoDriver *m_driver = nullptr; // the glow's, to take it off
};

} // namespace planetarium
