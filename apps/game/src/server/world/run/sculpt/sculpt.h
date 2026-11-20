// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/shared/core/irr_v3d.h"
#include "src/shared/map/mapnode.h"
#include "library/geometry/shape/shape.h"
#include "src/shared/map/voxel.h"
#include <line3d.h>
#include <deque>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

class Map;

/*
	Sculpt: the tools in hand (sprint 45), working the world where it stands
	at any size from a sixteenth to a few blocks. A stroke is a tool, a
	size, where it's pointed and dig or place: the world's sixteenths it
	takes or fills. The server applies it (Sculptor); the client outlines
	it before the click.

	Sprint 75: structure. A frame or truss is drawn as a box is, its
	members the size thick (a flat one, clicked or not pulled out, is a
	chassis); a beam from one point to another across the face.

	A shape is drawn by its footprint (sprint 50): dragged out on the face
	it starts on, from the sixteenth pointed at to `to` (a box corner to
	corner; a round one from its middle out, sprint 65); the size is how far
	it goes, out of the face (adding) or into it (cutting, painting). Clicked, the
	footprint's a square of the size. Each tool fills that box its own way:
	the box whole, the ellipse across it (a cylinder), tapering to a point
	(a cone), the ellipsoid in it (a sphere).
*/
namespace sculpt {

enum class Tool : u8 {
	Box,
	Cylinder, // along the face's normal: a wheel, an axle, a pipe
	Cone,     // from the face to a point
	Sphere,
	Cut,  // a plane: dig shaves what stands past the face, place fills what's short of it
	Bore, // a round passage into the face (dig only)
	// sprint 75: structure, its members the size thick
	Frame, // the box's edges: a chassis, a cage
	Truss, // the edges and a cross on each side big enough
	Beam,  // a straight member from where it's begun to where it's dragged, at any angle across the face
	Count,
};

// What a stroke does with its shape (sprint 53): takes it, adds it, or
// paints what's there in it the material held (each node it touches: a
// node is of one material). Cut and paint go into the face; add out of it.
enum class Act : u8 { Cut, Add, Paint, Count };

// What each tool is, by Tool.
struct Kind
{
	// how it fills its box: within the ellipse (or ellipsoid) the box
	// holds, measured from its middle
	enum class Round : u8 {
		None,   // the whole box
		Ball,   // the ellipsoid
		Across, // the ellipse across the normal, all along it
		Taper,  // the ellipse across the normal, shrinking to a point away from the face
		Frame,  // within the size of two of the box's sides: its edges
		Truss,  // the frame, and the diagonals across each side
		Line,   // within half the size of the line across the face, start to end
	};
	const char *id;   // the page's name for it
	const char *name;
	bool places;    // it adds (and makes a part): else it only cuts and paints
	bool outward;   // dig works past the face, place short of it (the cut); else into and onto it
	int depth;      // how deep it goes, in sizes
	Round round;
};
extern const Kind KINDS[size_t(Tool::Count)];

constexpr int MAX_SIZE = 4 * shape::STEPS; // four blocks
constexpr int MAX_DEEP = 8 * shape::STEPS; // pulled out: eight blocks
// How far a player's strokes reach (nodes, from the eye to what's pointed
// at), past the stroke's own size: the server's check, and where the
// client stops aiming.
constexpr float REACH = 12;

// A sixteenth of the world, counted from the world's origin.
v3s32 world(const shape::Sixteenth &s);
shape::Sixteenth local(v3s32 w);

struct Stroke
{
	Tool tool;
	int size; // in sixteenths, 1..MAX_SIZE
	shape::Sixteenth at; // the sixteenth pointed at
	v3s16 normal;        // the face pointed at's outward normal: one axis, ±1
	Act act;
	// Mirrored across a plane (sprint 46): its axis (0..2), and twice its
	// coordinate in world sixteenths (a plane between cells is odd)
	struct Mirror
	{
		int axis;
		s32 twice;
		bool operator==(const Mirror &o) const { return axis == o.axis && twice == o.twice; }
	};
	std::optional<Mirror> mirror;
	// the footprint's other corner, dragged to on the face's plane; none
	// if it's clicked
	std::optional<shape::Sixteenth> to;
	// how far it goes from the face (sixteenths), pulled out (sprint 55);
	// none: as deep as its tool takes its size
	std::optional<s32> deep;

	// Valid: a tool, a size in range, a unit normal, an act, adding only
	// with a tool that adds, and a footprint on the face's plane no wider than
	// the largest size.
	bool valid() const;
	bool operator==(const Stroke &o) const
	{
		return tool == o.tool && size == o.size && at.node == o.at.node && at.cell == o.at.cell
				&& normal == o.normal && act == o.act && mirror == o.mirror
				&& to.has_value() == o.to.has_value() && (!to || (to->node == o.to->node && to->cell == o.to->cell))
				&& deep == o.deep;
	}
	// Its size in words, for the player as they drag it: "2 blocks × 12/16,
	// 4/16 deep" (the footprint across, then how far it goes).
	std::string measure() const;
	// The world sixteenths it takes, fills or paints, and their mirror
	// if it's mirrored. A size of whole blocks snaps to the node grid.
	std::vector<v3s32> cells() const;
};

// How far along the line from `from` along `axis` (a unit vector) a ray
// comes nearest it, signed: how far a shape's pulled out of its face as
// the cursor goes. None when the ray runs along it.
std::optional<f32> nearestAlong(v3f from, v3f axis, const core::line3d<f32> &ray);

// The edges of a volume of world sixteenths: where its surface bends
// (not the seams between its cells), each a straight run, its ends in
// world sixteenths.
std::vector<std::pair<v3s32, v3s32>> outline(const std::vector<v3s32> &cells);

/*
	What a stroke changed: each node it touched as it was before (the
	material, and its shape if it was cut). Restored, the map is as it was
	(smoothed ground smoothed again: it had no shape).
*/
struct Edit
{
	struct Was
	{
		v3s16 p;
		MapNode n;
		std::optional<shape::Shape> shape;
	};
	std::vector<Was> was;

	void restore(Map &map) const;
};

/*
	Each player's strokes, newest last, to undo: the last DEPTH, each with
	what it was made on (the world, 0, or a machine's part). Undoing puts
	back what the stroke found, whatever's happened there since.
*/
class History
{
public:
	static constexpr size_t DEPTH = 64;

	struct Entry
	{
		u64 machine; // 0: the world; else its piece::Machine id
		u64 part;    // which of its parts
		Edit edit;
	};

	void record(const std::string &player, Entry entry);
	// A part's grid moved (Machine::makeRoom): what's kept of it, moved
	// with it, everyone's.
	void shift(u64 machine, u64 part, v3s16 by);
	// The player's last stroke, taken off; nothing if there's none.
	std::optional<Entry> pop(const std::string &player);

private:
	std::unordered_map<std::string, std::deque<Entry>> m_edits;
};

/*
	Applies strokes to a map. Smoothed ground it touches is settled first
	(Map::settleGround), so it's cut from the shape it had. A node holds one
	material: digging takes from a block's sixteenths; placing fills open
	nodes with `material`, and a cut node of that material, and nothing
	else. Emptied, a node is air; filled whole, whole.
*/
class Sculptor
{
public:
	explicit Sculptor(Map &map) : m_map(map) {}

	// What it changed, to undo it. `within`: the only nodes it may change
	// (a piece's grid ends: what's past it isn't cut or placed).
	Edit apply(const Stroke &s, content_t material, std::optional<VoxelArea> within = std::nullopt);
	// What's in `area` made a shell `thickness` sixteenths thick: every
	// sixteenth further than that from open space is taken (sprint 50: a
	// vessel, a chamber, a hull). Open space round the area counts.
	Edit hollow(const VoxelArea &area, int thickness);
	// What's in `area` with its edges and corners rounded to `radius`
	// sixteenths (sprint 75): what a ball that size, rolled round inside
	// it, can't reach is taken.
	Edit round(const VoxelArea &area, int radius);

private:
	struct Solid;
	std::optional<Solid> solidIn(const VoxelArea &area, int reach) const;
	Edit takeWhere(const Solid &solid, const std::function<bool(v3s32 cell)> &taken);

	Map &m_map;
};

} // namespace sculpt
