// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/client/render/map/detail/remaking.h"
#include "src/client/render/map/mesh_generator_thread.h"
#include "src/shared/map/map.h"
#include <algorithm>

namespace detail {

// made again a few a frame (not urgent, alone: their neighbours' detail is their own)
static constexpr int EACH_FRAME = 6;

void Remaking::follow(v3s16 camera, Sight sight, MeshUpdateManager &meshes, Map &map)
{
	if (camera != m_camera || !m_sight || sight != *m_sight) {
		m_camera = camera;
		m_sight = sight;
		meshes.setCamera(camera);
		meshes.see(sight);
		weigh(camera, meshes, map);
	}
	for (int n = 0; n < EACH_FRAME && !m_pending.empty(); n++) {
		const v3s16 p = m_pending.back();
		m_pending.pop_back();
		auto it = m_made.find(p);
		if (it == m_made.end() || !map.getBlockNoCreateNoEx(p))
			continue;
		it->second = meshes.detailAt(p); // asked: not again till it changes again
		meshes.updateBlock(&map, p, false, false);
	}
}

// The meshes whose ring the camera crossed.
void Remaking::weigh(v3s16 camera, MeshUpdateManager &meshes, Map &map)
{
	m_pending.clear();
	for (auto it = m_made.begin(); it != m_made.end();) {
		if (!map.getBlockNoCreateNoEx(it->first)) {
			meshes.forgetDetail(it->first);
			it = m_made.erase(it);
			continue;
		}
		if (meshes.detailAt(it->first) != it->second)
			m_pending.push_back(it->first);
		++it;
	}
	const v3s16 at = getNodeBlockPos(camera);
	std::sort(m_pending.begin(), m_pending.end(), [&](v3s16 a, v3s16 b) {
		return (a - at).getLengthSQ() > (b - at).getLengthSQ();
	});
}

} // namespace detail
