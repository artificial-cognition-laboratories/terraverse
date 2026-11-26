// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/client/world/sim/water/types.h"
#include "src/client/world/sim/water/grid.h"
#include "src/client/world/sim/water/flat.h"
#include "src/client/world/sim/water/scan.h"
#include "src/client/world/sim/water/base.h"
#include "src/client/world/sim/water/flow.h"
#include "src/client/world/sim/water/drops.h"
#include "src/client/world/sim/water/draw.h"
#include "src/client/world/sim/water/inspect.h"
#include "src/shared/base/irr_aabb3d.h"
#include <ISceneNode.h>
#include <SMaterial.h>
#include <memory>
#include <optional>
#include <unordered_set>
#include <vector>

class ClientMap;
class IShaderSource;
class NodeDefManager;

/*
	The liquid layer (sprint 81): liquid lying in the world's open space,
	drawn on its own, never by remeshing map blocks.

	Near the camera (sprint 82) water is a grid of columns a sixteenth wide,
	drawn on the GPU from a texture of its heights, each top on a sixteenth.
	Each column's water is a base plus a disturbance (sprint 122): the base
	is the server's (eased, never snapped), and only the disturbance is run,
	as shallow water by virtual pipes (wakes, splashes) carried by the
	current; tiles at rest aren't run at all. Past the grid, liquid's drawn
	flat. Poured liquid falls as sixteenth-cube drops that land in it. Only
	a picture: nothing goes back to the server.

	The door: it builds its leaves and runs them in a frame's order.
*/
namespace liquids {

class Layer : public scene::ISceneNode
{
public:
	struct Opts
	{
		scene::ISceneManager *smgr;
		const NodeDefManager *ndef;
		IShaderSource *shaders;
	};
	explicit Layer(Opts o);

	// A frame: the blocks whose fills changed, those whose nodes did (or
	// that arrived), what was poured and what moves, the map, where the
	// scene's origin is (the camera offset, in nodes), where the camera is
	// (nodes), the sea's level (the highest node of it; none in a world without
	// one) and the time since the last frame.
	void update(const std::unordered_set<v3s16> &changed, const std::unordered_set<v3s16> &ground,
			const std::vector<Stream> &streams, const std::vector<Mover> &movers, const Rivers &rivers, ClientMap &map,
			v3s16 offset, v3f camera, std::optional<s16> sea, u32 daynight, float dtime, bool hurrying);

	// What the debugger's World tab shows of it, and how it's asked to show it
	Inspect &inspect() { return m_inspect; }

	void OnRegisterSceneNode() override;
	void render() override;
	const aabb3f &getBoundingBox() const override { return m_box; }

private:
	video::SMaterial m_material; // the flat layer's and the drops'
	const bool m_gpu;
	Look m_look; // (the debugger's)            // the grid's run and drawn on the GPU; without it (no GL 3), water's drawn flat only

	Grid m_grid;
	Flat m_flat;
	Scan m_scan;
	Base m_base;
	Flow m_flow;
	Drops m_drops;
	std::unique_ptr<Draw> m_draw; // with the GPU only
	Inspect m_inspect;

	float m_owed = 0; // s of the grid's steps not run yet
	v3s16 m_offset;
	aabb3f m_box{ v3f(-1e7f), v3f(1e7f) }; // it is wherever liquid is: never culled as a whole
};

} // namespace liquids
