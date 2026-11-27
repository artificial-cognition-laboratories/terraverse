// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/client/render/map/near_meshes.h"
#include "src/client/render/map/mesh/mapblock_mesh.h"
#include "src/client/world/session/client.h"
#include "src/shared/map/block/mapblock.h"
#include "src/shared/map/block/mapsector.h"
#include "library/trace/owned.h"
#include "src/shared/platform/system/log.h"
#include <vector>

namespace {

// the driver lets a buffer go after 25 s unused (CNullDriver::expireHardwareBuffers)
constexpr std::chrono::seconds EVERY{ 5 };

} // namespace

NearMeshes::NearMeshes(Opts o) : m_client(o.client)
{
}

void NearMeshes::keep(const std::unordered_map<v2s16, MapSector *> &sectors, video::IVideoDriver *driver)
{
	const auto now = std::chrono::steady_clock::now();
	if (now - m_kept < EVERY)
		return;
	m_kept = now;
	MapBlockMesh::Held all;
	u64 meshes = 0;
	std::vector<v3s16> lost;
	for (const auto &[pos, sector] : sectors)
		for (const auto &[y, block] : static_cast<const MapSector &>(*sector).getBlocks()) {
			const MapBlockMesh *mesh = block->mesh;
			if (!mesh)
				continue;
			meshes++;
			const MapBlockMesh::Held h = mesh->held();
			all.ram += h.ram;
			all.card += h.card;
			all.unpacked += h.unpacked;
			all.transparent += h.transparent;
			if (!mesh->touch(driver))
				lost.push_back(block->getPos());
		}
	trace::owned("Map: meshes, opaque in RAM", all.ram, meshes);
	trace::owned("Map: meshes, transparent", all.transparent, meshes);
	// (two figures of one sum: "unpacked" is the share that's kept as built)
	trace::owned("Map: meshes on the card, packed", all.card - all.unpacked, meshes, trace::Where::Gpu);
	trace::owned("Map: meshes on the card, unpacked", all.unpacked, meshes, trace::Where::Gpu);
	if (!lost.empty()) {
		// (not silent: a mesh made again is cheap, but one lost says the touching fell behind)
		warningstream << "map: " << lost.size() << " block meshes' buffers let go by the driver; made again" << std::endl;
		for (const v3s16 &p : lost)
			m_client.addUpdateMeshTask(p);
	}
}
