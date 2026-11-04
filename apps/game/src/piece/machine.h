// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "piece/object.h"
#include "shape/shape.h"
#include "irr_aabb3d.h"
#include <optional>
#include <string>
#include <vector>

class IGameDef;

/*
	A machine (sprint 52): parts, each an object (a design and what's built
	of it) set in the machine's grid, and the joints between them. A cart's
	chassis, its axles, its wheels. One entity carries it, and it moves and
	falls as one; a hinge's part is free to turn about its axle (when
	there's something to turn it: sprint 53).

	A part sits at an offset turned one of the 24 square turns: every
	part's sixteenths are the machine's, so the tools, and parts joined
	face to face, are exact. Points are in sixteenths from a grid's corner
	(node (0, 0, 0)'s least): a part's sixteenth (n, c) spans 16n + c to
	16n + c + 1; the first part's are the machine's.

	Its text is "TFM6" (sprint 130: its scale; TFM5 had none, sprint 75: each joint's spring, how far it travels and how firm; TFM4 had none: sprint 73's
	joint kinds are new numbers in it), its id, its parts, its joints, its sockets, its
	front (TFM3 had no front: its grid's +z; TFM2 had no sockets; TFM1, sprint 52, had what each part was made as: read
	past). An object's text
	(TFO1) or a piece's from before reads as a machine of one part.
*/
namespace piece {

struct Pose
{
	v3s32 offset; // where the part's corner is, in the machine's sixteenths
	u8 turn = 0;  // piece::turn

	// A point of the part's (sixteenths) in the machine's.
	v3f apply(v3f local) const;
	// A point of the machine's in the part's.
	v3f unapply(v3f machine) const;
	// The part's frame (BS) in the machine's.
	core::matrix4 matrix() const;
};

// What a joint is, by kind (sprint 63). A rotary joint spins freely (a
// wheel's axle); a hinge turns between stops (a door). A wheel's axle can
// also steer (turned by its driver's left and right) and be driven (turned
// by the machine's engines): sprint 73. Saved by number: before sprint 63
// the second was "hinge" and spun freely, so it stays the free one.
enum class Joint : u8 {
	Fixed,  // welded
	Rotary, // turning about its axis, as far as it's turned
	Hinge,  // turning about its axis, LIMIT either way
	Steered,      // a rotary that steers
	Driven,       // a rotary the engines turn
	SteerDriven,  // both
	Sprung,       // a wheel's suspension (sprint 75): the part on it is a hub, riding its wheel's spring (sprint 76)
	Count,
};
struct JointKind
{
	const char *id;
	const char *label; // for the player
	bool turns;  // about its axis
	f32 limit;   // degrees either way it turns; 0: no stops
	bool steers, driven; // a wheel on it (sprint 73)
	bool sprung = false; // a wheel's spring (sprint 75)
};
extern const JointKind JOINTS[size_t(Joint::Count)];

// A sprung joint's spring (sprint 75), picked on its menu: how far it
// travels and how firm it is, each by its table's index. A wheel's
// clearance is where it was built: there it sits, the spring sagged.
struct Spring
{
	u8 travel = 1, firmness = 1;
	bool operator==(const Spring &o) const { return travel == o.travel && firmness == o.firmness; }
};
struct SpringStep
{
	const char *id, *label;
	f32 value; // travel: nodes it moves, all told; firmness: how often it bounces, Hz
};
constexpr size_t SPRING_STEPS = 3;
extern const SpringStep TRAVELS[SPRING_STEPS], FIRMNESSES[SPRING_STEPS];

class Machine
{
public:
	struct Part
	{
		Object object;
		Pose pose;
	};
	struct Link
	{
		u64 a, b; // the parts' ids
		Joint joint;
		v3f anchor; // sixteenths, the machine's
		v3s16 axis; // a unit axis, the machine's: a hinge's
		Spring spring = {}; // a sprung joint's
	};
	// A joint waiting for a part (sprint 56): on a part's face, its axis
	// out of it. Something carried onto it is joined there, so, and it's
	// taken up.
	struct Socket
	{
		u64 id;
		u64 part;
		v3f point;  // sixteenths, the machine's
		v3s16 axis; // a unit axis, the machine's: out of the face
		Joint joint;
	};

	// A new machine: no parts yet, an id of its own.
	explicit Machine(IGameDef *gamedef);

	// Throws SerializationError on text that isn't a machine, an object
	// or a piece.
	void read(const std::string &text);
	std::string write() const;

	u64 id() const { return m_id; }
	// A copy (a blueprint placed): ids of its own, its parts' too.
	void renew();

	std::vector<Part> &parts() { return m_parts; }
	const std::vector<Part> &parts() const { return m_parts; }
	std::vector<Link> &links() { return m_links; }
	const std::vector<Link> &links() const { return m_links; }
	const std::vector<Socket> &sockets() const { return m_sockets; }
	// Its front (sprint 70): a level unit axis of its own, set as it was
	// begun (the way its maker faced). Mirroring is left to right of it.
	v3s16 front() const { return m_front; }
	// Throws on one that isn't a level unit axis.
	void setFront(v3s16 front);
	// How big it is against the world's grid (sprint 130: a tool's grid is
	// finer than a building's). Only what it's drawn, hit and weighed as:
	// its parts' sixteenths are its own all the same. 0 < scale <= 1.
	f32 scale() const { return m_scale; }
	void setScale(f32 scale);
	// Its frame (BS) in the entity's: what's put before a part's pose.
	core::matrix4 scaling() const;
	Part *part(u64 id);
	const Part *part(u64 id) const;
	Part &add(Part part);
	// A part taken out, with its joints; the rest stay as they are.
	Part take(u64 id);
	// A copy of a part and what hangs from it (sprint 74: copied to be
	// pasted): the parts joined to the first part only through it, with
	// their joints and sockets, that part first and its corner at the
	// copy's, and how it hung (its plug). The first part's copy is the
	// whole machine. Throws on no such part.
	Machine extract(u64 id) const;
	// How a copy hung from what it was copied off (sprint 76: a corner on
	// its spring): joined to something face to face as welded, it's joined
	// so instead. Kept in its text; none for what wasn't copied off anything.
	struct Plug
	{
		Joint joint;
		Spring spring;
		// The face it hung by (sprint 77): the middle of it (sixteenths,
		// the copy's) and its normal out of it, toward what it hung on. It
		// goes back on by that face, so a corner's hub meets the chassis
		// and its wheel stands outboard, with no turning it about by hand.
		struct By
		{
			v3f at;
			v3s16 out;
		};
		std::optional<By> by;
	};
	const std::optional<Plug> &plug() const { return m_plug; }
	void setPlug(std::optional<Plug> plug) { m_plug = plug; }
	// A part taken off with what hangs from it (sprint 77: pulled off the
	// workpiece by hand): `extract`'s copy, those parts gone from here.
	// Throws on the first part (the rest hang from it) or no such part.
	Machine pull(u64 id);

	// A face of a part: the sixteenth pointed at and its face's normal
	// (the part's own).
	struct Face
	{
		u64 part;
		shape::Sixteenth at;
		v3s16 normal;
	};
	// A socket made on a face (throws on a face of no part); one taken away.
	const Socket &socket(const Face &face, Joint joint);
	void unsocket(u64 id);
	// `other` carried onto socket `id`: its face `theirs` set flat against
	// the socket's face, middles met, turned `spin` quarters about the
	// socket's axis (sprint 64), joined there as the socket says (a hinge
	// about its axis), the socket taken up. Throws on no such socket.
	void plug(u64 id, Machine other, const Face &theirs, u8 spin);
	// Where on it (the machine's sixteenths: the face's middle) and which
	// way it faces.
	v3f point(const Face &face) const;
	v3s16 normal(const Face &face) const;
	// The face of its first part a plug's `by` names (nothing without one).
	// Throws on one that isn't on its first part's grid.
	std::optional<Face> plugFace() const;
	// The face of a part whose point is `at` (the machine's sixteenths: a
	// sixteenth's middle across, on its side along) facing `out` (the
	// machine's): how a face is named from where it is (sprint 77).
	Face faceAt(u64 part, v3f at, v3s16 out) const;
	// A part's filled extent (sprint 77), to the sixteenth, in its own
	// nodes as boxOf's are; nothing if it's empty. Throws on no such part.
	std::optional<aabb3f> filled(u64 id) const;
	// Room made on a part's grid for nodes `lo` to `hi` (its own, as they'd
	// be): what's on it moved along it, and the part the other way in the
	// machine, so nothing moves in the world. How far it moved (none: there
	// was room). Throws if they and what's there won't fit on a grid.
	v3s16 makeRoom(u64 part, v3s32 lo, v3s32 hi);
	// A part of its own grown out of a face: `cells` (the face's part's
	// sixteenths: a stroke's) of `material`, on its grid where they are,
	// joined to the face's part by `joint` at the face's point (a hinge
	// about its normal). Throws on cells further apart than a part holds.
	Part &grow(const Face &face, const std::vector<v3s32> &cells, content_t material, Joint joint);
	// The joint between two parts set to `joint`; none there, made at
	// `at` (a face of a's), or at a's corner if there's none given.
	void relink(u64 a, u64 b, Joint joint, const std::optional<Face> &at);
	// The joint between them's spring set (sprint 75). Throws on no joint there.
	void tune(u64 a, u64 b, Spring spring);
	// The joint between them taken away (none there: nothing's done).
	void unlink(u64 a, u64 b);
	// Its face furthest along `d` (a unit axis, the machine's): of the
	// node that reaches furthest that way (of those, the nearest its
	// middle across), the middle of that node's face. What's carried onto
	// another is joined by it. Nothing if it has no parts.
	std::optional<Face> faceToward(v3s16 d) const;
	// The face across the machine's plane of axis `axis` at half `twice`
	// (its sixteenths), on the same part (a pair of wheels' second).
	Face mirrored(const Face &face, int axis, s32 twice) const;
	// `other`'s parts and joints made this one's: its face set flat
	// against this one's (turned square, as little as it can be, then
	// `spin` quarters about the normal), joined there by `joint` about
	// this face's normal.
	void join(const Face &mine, Machine other, const Face &theirs, Joint joint, u8 spin);

	// Its parts in rigid groups: welded together, they move as one; a
	// hinge is between two groups. The first part's group is first; parts
	// no joint reaches are in it. Each group's parts in the machine's order.
	std::vector<std::vector<u64>> rigid() const;

	// Every part built as designed (creative).
	void build();
	bool empty() const;
	// Its designed and built extents, in the machine's nodes (BS = 1).
	std::optional<aabb3f> designed() const;
	std::optional<aabb3f> built() const;

private:
	IGameDef *m_gamedef;
	u64 m_id;
	std::vector<Part> m_parts;
	std::vector<Link> m_links;
	std::vector<Socket> m_sockets;
	v3s16 m_front{ 0, 0, 1 };
	f32 m_scale = 1;
	std::optional<Plug> m_plug;

	// `other`'s parts, joints and sockets made this one's, its face
	// `theirs` set flat at `p` facing against `n`, `spin` quarters about it.
	void adopt(Machine other, const Face &theirs, v3f p, v3s16 n, u8 spin);
};

// How what's carried goes onto a face (sprint 77): it's set on as it's
// held. Of the square turns setting a face of it flat on a face of normal
// `n` (theirs), the nearest `held` (how it's turned in the hands, in their
// frame: a rotation), the face meeting theirs given (`side`, its own
// normal, if it meets by a face it must: a plug's) or not. Its side that
// meets (its own axis) and the quarters about `n` join turns it by.
struct Fit
{
	v3s16 side;
	u8 spin;
};
Fit nearestFit(v3s16 n, std::optional<v3s16> side, const core::matrix4 &held);

} // namespace piece
