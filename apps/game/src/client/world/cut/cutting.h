// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/shared/base/irr_aabb3d.h"
#include "src/shared/base/irr_v3d.h"
#include "src/shared/game/cut/peel.h"
#include "src/shared/game/cut/record.h"
#include <array>
#include <json/json.h>
#include <line3d.h>
#include <optional>

class Client;
struct PointedThing;
namespace shell { class Ui; }

/*
	Cutting, as the client sees it (sprint 123; the server's is server/game/cut):
	whether F opens the cut menu; what a dig would take (a member peeled off,
	or one placed taken back); and, holding a member, where it would go:
	snapped to what's built near the crosshair, turned as they've turned it.
	Read from the UI value "cut" the server tells and the blocks round them,
	by the rules the server cuts and places by (shared/game/cut).
*/
namespace cut {

class Cutting
{
public:
	struct Opts
	{
		Client *client;
		const shell::Ui &ui;
	};
	explicit Cutting(const Opts &opts) : m_opts(opts) {}

	// How a member held is placed: snapped to what's near, freely (Ctrl), or onto a stack (Shift).
	enum class Mode { Snap, Free, Stack };
	// Each frame, before anything's asked: the line they look along (BS, the world's), and how they place.
	void look(const core::line3d<f32> &ray, Mode mode);

	// A cutting tool held, pointed at a block it cuts: F is the cut menu's.
	bool menu(const PointedThing &pointed) const;
	// What a dig would take where they aim (BS, about the pointed node's
	// centre): the member it would peel off, with a cut chosen; else the
	// placed member it would take back. None where it's neither.
	std::optional<aabb3f> preview(const PointedThing &pointed) const;
	// A dig done with a cut chosen, or of timber: the cut (or the placed member
	// back) asked of the server where they aim, in place of the dig (nothing's
	// predicted). False if it's neither.
	bool take(const PointedThing &pointed) const;

	// Where the member held would go: a node it's drawn from, its box about
	// that node's centre (BS), and whether it fits. None if none's held.
	struct Ghost
	{
		v3s16 node;
		aabb3f box;
		bool fits;
	};
	std::optional<Ghost> ghost(const PointedThing &pointed) const;
	// A place done holding a member: asked of the server where the ghost is,
	// in place of placing. False if they don't hold one.
	bool place(const PointedThing &pointed) const;
	// The member held turned a quarter round the up (R; stood up, that rolls it), or
	// stood up and laid back down end over end (Shift+R): together, any way it can
	// lie. False if they don't hold one.
	bool turn();
	bool tip();

private:
	struct Aim
	{
		v3s16 node, face, cell;
		// placed between two members: the middle of them (world sixteenths, ·2), to line up with along the face's axis
		std::optional<v3s32> between;
	};
	struct Laying
	{
		Aim aim;
		std::array<int, 3> axes; // the world axis its along, across and in lie on
		peel::Box box; // world sixteenths
		bool fits;
	};
	// Where they aim, if a cut's chosen and the tool held cuts what's pointed at.
	std::optional<Aim> aim(const PointedThing &pointed) const;
	// The member held being placed: where, which way, whether it fits.
	std::optional<Laying> laying(const PointedThing &pointed) const;
	// Stacking it (stack.h): the next slot of the stack of its kind they aim at, else
	// a new stack on what they aim at. `along`: the world axis it lies along unless the stack says.
	std::optional<Laying> stacking(const PointedThing &pointed, const peel::Size &size, int along) const;
	// Whether `box` (world sixteenths) fits as the server will judge it: into open blocks or
	// timber with room, resting against something.
	bool fits(const peel::Box &box) const;
	// What placing aims at: in the air between members the line passes (two posts),
	// else the block pointed at within RANGE, else the ground under where the line
	// ends at RANGE (or the air there).
	std::optional<Aim> placingAim(const PointedThing &pointed) const;
	// How far along the line, short of `before`, it first passes between two members. None if it doesn't.
	// The middle of the two (world sixteenths, ·2) comes with it.
	struct Between
	{
		f32 t;
		v3s32 middle;
	};
	std::optional<Between> between(f32 before) const;
	// Placing in the air at `at` (BS), looked at along `dir`: as if on a face turned to them.
	static Aim airAt(v3f at, v3f dir);
	// The members placed in or near `box` (world sixteenths).
	std::vector<record::Member> near(const peel::Box &box) const;
	// The placed member where they aim on timber (record.h): its box, world sixteenths.
	std::optional<peel::Box> placed(const PointedThing &pointed) const;
	// Where they aim on the block pointed at, whatever's held.
	static std::optional<Aim> aimed(const PointedThing &pointed);
	// Whether the tool held cuts the block pointed at.
	bool cuts(const PointedThing &pointed) const;
	// The member item held's entry in the UI value (null if none).
	const Json::Value &held() const;

	Opts m_opts;
	core::line3d<f32> m_ray;
	Mode m_mode = Mode::Snap;
	// which of the member's along, across and in lies on each of the view's side, up, forward
	enum { SIDE, UP, FORWARD };
	std::array<int, 3> m_view = { 1, 2, 0 }; // lying along the way they look, flat
	int m_laid = FORWARD; // the way it lay before it was stood up, to lay it back down
};

} // namespace cut
