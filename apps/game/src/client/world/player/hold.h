// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/shared/base/irr_v2d.h"
#include "src/shared/base/irr_v3d.h"
#include <line3d.h>
#include <matrix4.h>
#include <optional>

/*
	An object on the workbench (sprint 56): F lifts it a little and it
	floats there, anchored in the world, while the player walks round it
	with the tools. It's turned about one axis at a time, in steps, about
	its middle: the world's up (yaw: the disc under it), the player's right
	(pitch), the line to the player (roll: the ring facing them). Where it
	is (its pose) is the client's to say, the server's to check. In node
	coordinates.
*/
class Hold
{
public:
	// `object` (its object id), its grid's middle `middle` (local nodes),
	// `radius` nodes round it, turned as `turned` (its frame's rotation:
	// piece::frame), its middle anchored at `anchor`.
	Hold(u16 object, v3f middle, f32 radius, const core::matrix4 &turned, v3f anchor);

	u16 object() const { return m_object; }
	// Its middle in its own grid: mirroring mirrors across it.
	v3f middle() const { return m_middle; }
	f32 radius() const { return m_radius; }
	// Where its middle's anchored.
	v3f held() const { return m_anchor; }
	// Where the player's eye is and which way they look, as they move: what
	// the ring faces and pitch turns about.
	void watch(v3f eye, v3f look);

	enum class Spin : u8 { Yaw, Pitch, Roll };
	v3f axis(Spin spin) const;
	// A turn about one axis, from how it's turned when it's begun; turned
	// to `angle` (radians) since, in whole `step`s (none: free).
	void begin(Spin spin);
	void spin(f32 angle, f32 step);
	void end() { m_from.reset(); }
	bool spinning() const { return m_from.has_value(); }
	// Turned (a step at once) about one axis.
	void turn(Spin spin, f32 angle);
	// Its design changed: its middle and how far it reaches from it.
	void reshape(v3f middle, f32 radius);
	// Upright: what's most up of it made up, its heading kept.
	void level();
	// Square to the player, as it's made.
	void reset();

	struct Pose
	{
		v3f position; // BS: the entity's
		v3f rotation; // degrees, as the server sets it (piece::frame)
	};
	Pose pose() const;

	// The turning handle: a ring, its middle and axis (nodes, the world's).
	struct Ring
	{
		v3f centre, axis;
		f32 radius;
	};
	Ring disc() const; // on the ground under it: yaw
	Ring ring() const; // round it, facing the player: roll
	// What a ray (BS) is on: the disc's face, or the ring's rim; and where
	// round it, an angle (radians) to turn by the change of.
	struct Grip
	{
		Spin spin;
		f32 angle;
	};
	std::optional<Grip> grip(const core::line3d<f32> &ray) const;
	// Where round `spin`'s ring a ray (BS) meets its plane (radians);
	// nothing if it runs along it.
	std::optional<f32> angle(Spin spin, const core::line3d<f32> &ray) const;
	// The axis of its grid the player's left and right run most along:
	// what mirroring mirrors across.
	int across() const;

private:
	v3f right() const;

	u16 m_object;
	v3f m_middle;
	f32 m_radius;
	core::matrix4 m_turned; // rotation only
	struct From
	{
		Spin spin;
		core::matrix4 turned;
	};
	std::optional<From> m_from;
	v3f m_anchor;
	v3f m_eye, m_look; // the look level (no pitch), unit
};
