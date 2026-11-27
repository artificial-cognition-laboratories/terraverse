// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/shared/base/irr_v3d.h"
#include <iosfwd>
#include <optional>
#include <utility>
#include <vector>

/*
	Rises: where in its top block a block's natural ground stands (sprint
	128). The world model decides the land to a sixteenth; a loose top block
	keeps how far its ground stands over (or under) its face, −8..7 sixteenths,
	and ground::Ground averages those at its corners. A node with a rise is
	natural ground: the world model's field runs through it and the ground
	round it (sprint 129). One with none (dug, built, or never generated so)
	stands at its face. Kept by map blocks beside their shapes, saved and sent
	with them; a node that becomes something else loses its rise.
*/
namespace ground {

class Rises
{
public:
	// sixteenths over the face of the node at rel; none: not natural ground
	std::optional<int> get(v3s16 rel) const;
	// keeps `rise` (−8..7) for the node at rel
	void set(v3s16 rel, int rise);
	void erase(v3s16 rel);
	bool empty() const { return m_rises.empty(); }

	template <class F>
	void each(const F &f) const
	{
		for (const auto &[k, r] : m_rises)
			f(unkey(k), int(r));
	}

	void serialize(std::ostream &os) const;
	void deSerialize(std::istream &is);

private:
	static u16 key(v3s16 rel);
	static v3s16 unkey(u16 k);

	// by key, sorted: a block's surface is a few hundred, a few bytes each
	std::vector<std::pair<u16, s8>> m_rises;
};

} // namespace ground
