// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/client/render/map/detail/sight.h"
#include "src/shared/base/irrlichttypes_bloated.h"
#include <mutex>
#include <unordered_map>

namespace detail {

/*
	The detail a mesh's ground is made at (sixteenths a drawn step: 1, 2, 4, 8 or 16, a
	whole node), by the Sight's rule at its distance from the camera, held where it was
	made: a mesh changes detail only once the camera's a MARGIN (a share of its distance)
	past the ring it crossed. Without the margin, a player standing on a ring swapped its
	meshes back and forth every step they took.
	The mesh workers and the client both ask, so it keeps its own lock.
*/
class Rings
{
public:
	// How far past a ring the camera goes before a mesh changes detail, a share of its distance.
	static constexpr float MARGIN = 0.15f;

	explicit Rings(Sight sight);

	// The window or the zoom changed: the rings with it.
	void see(Sight sight);

	// The detail mesh p is made at now, d nodes off; kept as what it was made at.
	u8 make(v3s16 p, float d);
	// The detail mesh p would be made at now: differs from its made one when it's to be made again.
	u8 wanted(v3s16 p, float d) const;
	// Mesh p is gone: what it was made at is let go of.
	void forget(v3s16 p);

private:
	u8 at(float d) const;
	u8 held(float d, u8 was) const;

	mutable std::mutex m_mutex;
	Sight m_sight;
	std::unordered_map<v3s16, u8> m_made;
};

} // namespace detail
