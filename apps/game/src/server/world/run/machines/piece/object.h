// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/server/world/run/machines/piece/piece.h"
#include <string>

class IGameDef;

/*
	An object (sprint 49): a thing made off the world's grid, a cart or a
	pot. It's two pieces on the one grid: its design (what the tools edit,
	what a blueprint keeps) and what's built of it. What's designed and
	not built is drawn a ghost: in survival, what's left to make; in
	creative there's none, what's built follows the design.

	Its id is its own and lasts (undo names it by it); its entity's object
	id is only for now. Its text is "TFO1", the id, the design's and the
	built piece's text; a piece's text from before (TFP1, TFP2) reads as
	one built as designed.
*/
namespace piece {

// An id of an object's or a machine's own: never 0.
u64 freshId();

// An object's text's id and its two pieces' texts, unread (its boxes
// without its names); a piece's from before is both, of no id.
struct ObjectText
{
	std::optional<u64> id;
	std::string design, built;
};
ObjectText split(const std::string &text);

class Object
{
public:
	// A new object: nothing designed or built, an id of its own.
	explicit Object(IGameDef *gamedef);

	// Throws SerializationError on text that isn't an object or a piece.
	void read(const std::string &text);
	std::string write() const;

	u64 id() const { return m_id; }
	// A copy (a blueprint placed): an id of its own.
	void renew();

	Piece &design() { return m_design; }
	const Piece &design() const { return m_design; }
	Piece &built() { return m_built; }
	const Piece &built() const { return m_built; }
	// What's built made the design: in creative, nothing's left to make.
	void build();
	// What's designed and not built, as a piece (each node, its sixteenths
	// short of the design's).
	Piece unbuilt() const;
	bool empty() const { return m_design.empty() && m_built.empty(); }
	// Its design and what's built moved on its grid by `by`.
	void shift(v3s16 by)
	{
		m_design.shift(by);
		m_built.shift(by);
	}

private:
	IGameDef *m_gamedef;
	u64 m_id;
	Piece m_design, m_built;
};

} // namespace piece
