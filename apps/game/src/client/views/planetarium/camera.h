// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/client/views/game/far/tiles/tiling.h"
#include "src/client/views/game/far/tiles/tiles.h"
#include <matrix4.h>
#include <cmath>
#include <optional>

namespace planetarium {

/*
	A camera in orbit round a body, looking down at its centre: the way from
	the centre to it, the screen's up across that, and how high above the sea
	it is. In the body's own frame (its ground's), so the ground stays still
	under it however the body spins. Dragging rolls it round the body like a
	ball under the hand, slower the lower it is (the ground under it moves at
	about the same speed on screen), over the poles as anywhere: nothing
	keeps north up but focus. Zooming moves it up and down, from a few body
	radii out to 100 blocks over the sea. In doubles; what it gives the GPU
	is relative to itself, in floats.
*/
class OrbitCamera
{
public:
	using Vec = far::Tiling::Vec;

	// the body's sea: blocks from its centre; the farthest it goes over it
	OrbitCamera(double sea, double farthest);
	// The lens everything's seen through, the sky included (one lens: the
	// stars and the bodies turn together): this wide across the screen,
	// radians, whatever its shape, so a wider window sees more sky, not a
	// bigger one. Wide when the window fills the eye, to feel vast; narrower
	// when it's small, where a wide lens's stretch toward the edges shows (a
	// sphere off the axis is an egg). Between, by the window's width in pixels
	static constexpr float WIDEST = 1.396f, NARROWEST = 1.0f;       // 80°, 57°
	static constexpr float WIDE_FROM = 1800, NARROW_BELOW = 1000;  // pixels across
	// and never taller than this (a window on end)
	static constexpr float TALLEST = 1.4f;
	// The lens's vertical angle (radians) on a screen this many pixels
	static float fovFor(float width, float height);
	// Pixels a unit of tan spans from the middle of a screen `height` high
	// through a lens `fov` high: what sizes things on screen (a ball of
	// radius r at distance d is r / d · focal pixels across, from the middle)
	static float focal(float height, float fov) { return height / (2 * std::tan(fov / 2)); }
	// pixels a tile's cell may span before it splits: about one block a pixel
	// or two, as sharp at every height as the land zoomed into (sprint 114;
	// 6 left a coarse planet till it was zoomed in on). Lowered further only
	// for a reference render (the ladder's)
	static inline float cell = 2;

	// pixels the mouse moved while held; wheel steps (up: closer)
	void drag(double dx, double dy);
	void zoom(double steps);
	// over a place (radians), at an altitude (blocks over the sea; kept
	// within the camera's range), or at its own
	void focus(double latitude, double longitude, std::optional<double> altitude = std::nullopt);
	// at an altitude (kept within range), over where it is
	void altitude(double altitude);
	// from the centre to it (unit), the screen's up (unit, square to it), and how high
	void place(const Vec &up, const Vec &north, double altitude);
	// round a body of another size (its sea, its farthest): as high in its radii
	void resize(double sea, double farthest);

	double altitude() const { return m_altitude; }
	// from the centre to it (unit); the screen's up (unit, square to it)
	const Vec &up() const { return m_up; }
	const Vec &north() const { return m_north; }
	double sea() const { return m_sea; }
	double farthest() const { return m_farthest; }
	// over where (radians)
	double latitude() const;
	double longitude() const;
	// where it is, from the body's centre (blocks)
	Vec position() const;
	// What far::Tiles chooses for from a point `p` (blocks from a body's
	// centre, its sea `sea`): the point under it at the sea; out to what it
	// can see of the body (its horizon); tiles fine enough that a cell is
	// about `cell` pixels across through a lens of this `focal` (focal()).
	static far::Eye eyeAt(const Vec &p, double sea, float focal);

	// view and projection, the camera at the origin (camera-relative: a
	// point p is drawn at (p − position()) · BS). Every projection is a plain,
	// symmetric perspective: what's off centre on screen is there because the
	// camera's turned, never because the lens is slid (that skews the sky).
	core::matrix4 view() const;
	core::matrix4 projection(float aspect, float fov) const;
	static core::matrix4 perspective(float aspect, float near, float far, float fov);
	// The turn that draws what's straight ahead of a view at `at` on screen
	// (−1..1, +x right, +y up) instead of its middle: aimed(at) * view. The
	// least turn that does it, so the horizon tilts no more than it must.
	static core::matrix4 aimed(v2f at, float aspect, float fov);
	// The way through a screen point (−1..1, +y up) in a view's camera space
	// (left-handed: x right, y up, z ahead), unit.
	static core::vector3df through(v2f at, float aspect, float fov);

	// A body that spins: a point of its own frame (its ground's) where it
	// is in space, the body turned by `spin` radians about its axis (y),
	// eastward (longitude grows by it); and back.
	static Vec turn(const Vec &p, double spin);
	// A view that draws a spun body's own frame where the body is: what's
	// given in its frame turned (turn), then seen by `view`.
	static core::matrix4 turned(const core::matrix4 &view, double spin);

	// The way from a camera seen by `view` through a pixel of a screen
	// `width` × `height` (unit, in the frame `view` looks at).
	static Vec ray(const core::matrix4 &view, double x, double y, double width, double height, float fov);

private:
	double m_sea, m_farthest; // (not const: a camera is replaced when it goes to another body)
	Vec m_up, m_north; // unit, orthogonal: from the centre to the camera; the screen's up
	double m_altitude;
};

} // namespace planetarium
