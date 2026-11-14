// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "far/tiling.h"
#include "far/tiles.h"
#include <matrix4.h>
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
	// radians, vertical: narrow enough that the sky, shifted to frame a body
	// off centre (perspective), isn't stretched toward the far side or warped
	// as the view turns (1 rad was: a wide screen then saw ~90° across)
	static constexpr float FOV = 0.7f;
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
	// what far::Tiles chooses for, on a screen `height` pixels high (eyeAt,
	// from where it is)
	far::Eye eye(float height) const { return eyeAt(position(), m_sea, height); }
	// What far::Tiles chooses for from a point `p` (blocks from a body's
	// centre, its sea `sea`): the point under it at the sea; out to what it
	// can see of the body (its horizon); tiles fine enough that a cell is
	// about two pixels across on a screen `height` pixels high.
	static far::Eye eyeAt(const Vec &p, double sea, float height);

	// view and projection, the camera at the origin (camera-relative: a
	// point p is drawn at (p − position()) · BS)
	core::matrix4 view() const;
	core::matrix4 projection(float aspect, v2f shift = v2f(), float fov = FOV) const;
	// A perspective, its middle moved `shift` (the screen's half widths and
	// heights, +x right, +y up): what's ahead is drawn there, not at the
	// screen's middle (the free part of a screen half under a panel).
	static core::matrix4 perspective(float aspect, float near, float far, v2f shift, float fov = FOV);

	// A body that spins: a point of its own frame (its ground's) where it
	// is in space, the body turned by `spin` radians about its axis (y),
	// eastward (longitude grows by it); and back.
	static Vec turn(const Vec &p, double spin);
	// A view that draws a spun body's own frame where the body is: what's
	// given in its frame turned (turn), then seen by `view`.
	static core::matrix4 turned(const core::matrix4 &view, double spin);

	// The way from the camera through a pixel (unit, planet frame), on a
	// screen `width` × `height`, the lens shifted by `shift` (perspective).
	Vec ray(double x, double y, double width, double height, v2f shift = v2f()) const;

private:
	double m_sea, m_farthest; // (not const: a camera is replaced when it goes to another body)
	Vec m_up, m_north; // unit, orthogonal: from the centre to the camera; the screen's up
	double m_altitude;
};

} // namespace planetarium
