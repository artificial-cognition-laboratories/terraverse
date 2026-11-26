// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/server/world/run/rules/river_surface.h"
#include "library/matter/fill/fill.h"
#include "src/server/app/server.h"
#include "src/server/world/state/environment/serverenvironment.h"
#include "src/server/world/terra.h"
#include "src/shared/map/map.h"

std::optional<int> terraHeldSurface(Server *server, v3s16 node)
{
	Map &map = server->getEnv().getMap();
	const MapNode n = map.getNode(node);
	if (n.getContent() == CONTENT_IGNORE || !(n.getParam2() & fill::HELD))
		return std::nullopt;
	const MapNode over = map.getNode(node + v3s16(0, 1, 0));
	if (over.getContent() == n.getContent() && (over.getParam2() & fill::HELD))
		return 16; // under its top: full
	// the rises the clients are sent (terraSendRivers: a block with a river's held water, once read)
	const ServerTerra &t = server->terra();
	const float rise = fill::surface::riseAt([&](v3s16 block) -> std::optional<float> {
		auto rb = t.river_blocks.find(block);
		return rb == t.river_blocks.end() || !rb->second.held || !rb->second.read_at ? std::nullopt
				: std::optional<float>(rb->second.rise);
	}, node);
	return fill::surface::of(n.getParam2(), rise);
}
