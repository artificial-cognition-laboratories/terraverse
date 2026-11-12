// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "irr_v3d.h"
#include "irrlichttypes.h"
#include <iostream>
#include <unordered_map>

/*
	Sprig: a small plant as the world keeps it (sprint 113). Not its blocks:
	which plant it is (flora::Forest's id, in the forest of its world's
	seed) and how it stands. Everyone grows the same plant from that, so the
	server stores and sends a few bytes and the client draws it (a grass's
	thousands of sixteenths never cross the network). It belongs to the node
	at its foot, a tf_world:growth; whoever changes that node's content
	drops it (Map::setNode, MapBlock::copyFrom), as a cut node's shape.
*/
namespace sprig {

struct Sprig
{
	u32 forest = 0; // its world's seed (flora::forestFor)
	u32 id = 0;     // flora::Forest::idOf
	u8 pose = 0;    // quarter turns (bits 0–1), mirrored (bit 2)
	int turn() const { return pose & 3; }
	bool mirror() const { return pose & 4; }
	bool operator==(const Sprig &o) const { return forest == o.forest && id == o.id && pose == o.pose; }
};

class Sprigs
{
public:
	const Sprig *get(v3s16 rel) const;
	void set(v3s16 rel, const Sprig &s) { m_sprigs[key(rel)] = s; }
	void erase(v3s16 rel) { m_sprigs.erase(key(rel)); }
	bool empty() const { return m_sprigs.empty(); }

	template <class F>
	void each(const F &f) const
	{
		for (const auto &[k, s] : m_sprigs)
			f(unkey(k), s);
	}

	void serialize(std::ostream &os) const;
	void deSerialize(std::istream &is);

private:
	static u16 key(v3s16 rel);
	static v3s16 unkey(u16 k);

	std::unordered_map<u16, Sprig> m_sprigs;
};

} // namespace sprig
