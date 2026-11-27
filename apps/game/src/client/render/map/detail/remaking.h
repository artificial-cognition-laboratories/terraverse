// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/client/render/map/detail/sight.h"
#include "src/shared/base/irrlichttypes_bloated.h"
#include <optional>
#include <unordered_map>
#include <vector>

class Map;
class MeshUpdateManager;

namespace detail {

/*
	The meshes made again as the camera moves across their rings (sprint 85): what
	each was made at, and, once the camera's moved a node or the sight's changed
	(the window, the zoom), the ones whose detail it wants is another, made again a
	few a frame, nearest first. All at once, a ring crossed was a burst of meshes.
*/
class Remaking
{
public:
	// Mesh p was made at `detail` (MeshUpdateResult::detail).
	void made(v3s16 p, u8 detail) { m_made[p] = detail; }
	// Each frame: from where the camera is (nodes) and how it sees.
	void follow(v3s16 camera, Sight sight, MeshUpdateManager &meshes, Map &map);

private:
	void weigh(v3s16 camera, MeshUpdateManager &meshes, Map &map);

	std::unordered_map<v3s16, u8> m_made;
	v3s16 m_camera{ S16_MIN, S16_MIN, S16_MIN };
	std::optional<Sight> m_sight;
	std::vector<v3s16> m_pending; // nearest last: taken from the back
};

} // namespace detail
