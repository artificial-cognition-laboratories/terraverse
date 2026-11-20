// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/server/world/run/machines/piece/machine.h"
#include "src/server/world/run/sculpt/sculpt.h"
#include <string>
#include <vector>

/*
	The tools in hand (sprints 45–46): out or away, which, how big, and
	whether strokes are mirrored. Held by the game; the page shows it and
	picks a tool by its id (sculpt::Kind::id).

	The page's rail asks for what the keys do (sprint 48): each ask waits
	here till the game takes it, where it knows what's pointed at.

	A tool is a brush (add, cut, paint, or a new part) and a shape (sprint
	53). Sprint 70: one build mode (Tab); parts are picked and worked on
	with the tools away, and their joints set from their menu.
*/
class Hand
{
public:
	enum class Verb : u8 {
		Focus, Mirror, Undo, Start, Save, Place, Hollow, Level, Reset,
		Detach, Delete, Relink, // the part selected's: taken off, gone, a joint of it set
		Socket, Unsocket, // a socket made on the face its menu's on, one taken away
		// sprint 74: the part pointed at copied, another pasted; the part
		// pointed at to be saved (the page asks its name: Save); one from
		// the library taken into the hands, one forgotten
		Copy, Paste, Name, Take, Forget,
		Round, // the part's edges rounded to the tool's size (sprint 75)
		Count,
	};
	// What each verb is, by Verb: the page's name for it, and whether it
	// takes a name (a blueprint's).
	struct Action
	{
		const char *id;
		bool named;
	};
	static const Action ACTIONS[size_t(Verb::Count)];
	struct Ask
	{
		Verb verb;
		std::string name; // a blueprint's, for the verbs that take one
		// relinking: the part at the joint's other end, and what it's to be
		u64 other = 0;
		piece::Joint joint = piece::Joint::Fixed;
		bool unlink = false; // the joint taken away
	};
	// The selected part's joint to `other` set to `joint` (none: taken away).
	void relink(u64 other, std::optional<piece::Joint> joint);
	// A socket of `joint` made on the face the menu's on; socket `id` taken away.
	void socket(piece::Joint joint);
	void unsocket(u64 id);

	// What the brush does with the shape drawn (sprint 53): adds it to the
	// part, cuts it out, paints what's there, makes it a part of its own;
	// or pulls the flat face pressed on out or in (sprint 55). Joints are
	// set from a part's menu (sprint 71).
	enum class Brush : u8 { Add, Cut, Paint, Part, Pull, Count };
	struct BrushKind
	{
		const char *id; // the page's name for it
		sculpt::Act act; // pulling: as it's pulled, out (add) or in (cut)
		bool apart; // grown as a part of its own
		bool pulls; // a face pulled, not a shape drawn
		bool picked; // on the rail, and R steps to it
	};
	static const BrushKind BRUSHES[size_t(Brush::Count)];
	Brush brush() const { return m_brush; }
	// A new part's joint to what it's drawn on (sprint 54): welded, or a
	// hinge about the face's normal (a wheel).
	piece::Joint partJoint() const { return m_part_joint; }
	void setPartJoint(piece::Joint joint);
	// Throws on an id that isn't a brush the rail picks: the page is wrong.
	void pickBrush(const std::string &id);
	// The next brush on the rail (R).
	void nextBrush();

	bool out() const { return m_out; }
	// One build mode (sprint 70): what's added is an object (a part on the
	// ground, or on the workpiece) unless the world's asked for (Alt held),
	// when it's added to the world. Cut and paint work on whatever's hit.
	bool creating() const { return m_out && !m_world; }
	void setWorld(bool world) { m_world = world; }
	// The brush as it acts: creating, adding makes parts.
	const BrushKind &acting() const
	{
		return BRUSHES[size_t(creating() && m_brush == Brush::Add ? Brush::Part : m_brush)];
	}
	sculpt::Tool tool() const { return m_tool; }
	int size() const { return m_size; }
	bool mirrored() const { return m_mirror; }

	void toggle() { m_out = !m_out; }
	void takeOut() { m_out = true; }
	// Throws on an id that isn't a tool: the page is wrong.
	void pick(const std::string &id);
	// Steps through the sizes: up for steps > 0.
	void resize(int steps);
	void toggleMirror() { m_mirror = !m_mirror; }

	// Throws on an id that isn't an action, or a name missing or given
	// where it isn't taken: the page is wrong.
	void ask(const std::string &id, const std::string &name);
	// What's been asked since, oldest first; taken, it's gone.
	std::vector<Ask> take();

	// The status line's words for it: "Sculpt: ball · 6/16".
	std::string describe() const;

private:
	bool m_out = false;
	bool m_world = false;
	sculpt::Tool m_tool = sculpt::Tool::Box;
	int m_size = 1;
	bool m_mirror = false;
	std::vector<Ask> m_asked;
	Brush m_brush = Brush::Add;
	piece::Joint m_part_joint = piece::Joint::Fixed;
};
