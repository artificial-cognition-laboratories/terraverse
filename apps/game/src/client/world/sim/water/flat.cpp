// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/client/world/sim/water/flat.h"
#include "src/client/world/sim/water/grid.h"
#include "src/client/world/sim/water/faces.h"
#include "src/client/world/sim/water/parts.h"
#include "src/client/views/game/water/liquidlook.h"
#include "src/client/render/map/clientmap.h"
#include "library/matter/fill/fill.h"
#include "library/geometry/shape/shape.h"
#include "src/shared/game/content/nodedef.h"
#include "src/shared/game/items/itemgroup.h"
#include "src/shared/map/block/mapblock.h"
#include <CMeshBuffer.h>
#include <IVideoDriver.h>
#include <algorithm>
#include <cmath>

namespace liquids {

namespace {
const float GLIDE = 4;         // per s: how fast a shown level closes on its fill (a quarter-second's ease)
const float SETTLED = 0.002;   // of its room: close enough to stop gliding
const float RELIGHT = 2;       // s between drawing again by the day's light
const int PER_FRAME = 6;       // blocks drawn again by the day's light in a frame (no hitch)
const float BLOCK_REACH = MAP_BLOCKSIZE * 0.8660254f; // a block's half diagonal (nodes)
const u8 ALPHA = u8(look::ALPHA * 255 + 0.5f);
// So much a frame, the rest waiting for the next (at speed a new block's flat
// mesh could take 20 ms a frame; a river rising rebuilt 98 blocks in one, 65 ms)
const s64 HURRIED_US = 1000, BUDGET_US = 1500; // (3 ms of it a frame took the frame's slack)

} // namespace

Flat::Flat(Opts o) :
	m_ndef(o.ndef), m_grid(o.grid),
	m_faces({ /*ndef*/ o.ndef, /*grid*/ o.grid })
{}

Flat::~Flat()
{
	for (auto &[pos, b] : m_blocks)
		if (b.mesh)
			b.mesh->drop();
}

bool Flat::begin(const std::unordered_set<v3s16> &changed, const std::unordered_set<v3s16> &ground, ClientMap &map,
		const Rivers &rivers, v3f camera, u32 daynight, float dtime)
{
	m_camera = camera;
	m_rivers = &rivers;
	m_range = map.getControl().range_all ? INFINITY : map.getControl().wanted_range;
	m_rebuild.clear();
	auto taking = [&](v3s16 bp, bool g) {
		take(bp, map, g);
		m_rebuild.insert(bp);
		// a neighbour's sides face this one's liquid
		for (v3s16 d : SIDES)
			if (m_blocks.count(bp + d))
				m_rebuild.insert(bp + d);
	};
	for (v3s16 bp : ground)
		taking(bp, true);
	for (v3s16 bp : changed)
		if (!ground.count(bp))
			taking(bp, false);
	// drawn again by the day's light only when it's changed (each 2 s it cost ms for nothing)
	m_relight -= dtime;
	const bool relight = m_relight <= 0 && daynight != m_daynight;
	if (m_relight <= 0) {
		m_relight = RELIGHT;
		m_daynight = daynight;
	}
	const float step = std::min(1.0f, GLIDE * dtime);
	for (auto it = m_blocks.begin(); it != m_blocks.end();) {
		Block &b = it->second;
		// unloaded: let it go
		if (!map.getBlockNoCreateNoEx(it->first)) {
			if (b.mesh)
				b.mesh->drop();
			m_relit.erase(it->first);
			m_reflat.erase(it->first);
			it = m_blocks.erase(it);
			continue;
		}
		if (b.gliding) {
			b.gliding = false;
			for (auto n = b.nodes.begin(); n != b.nodes.end();) {
				Shown &s = n->second;
				s.level += (s.target - s.level) * step;
				if (std::fabs(s.target - s.level) <= SETTLED)
					s.level = s.target;
				else
					b.gliding = true;
				if (s.level <= 0 && s.target <= 0)
					n = b.nodes.erase(n);
				else
					++n;
			}
			m_rebuild.insert(it->first);
		}
		if (relight && !b.nodes.empty())
			m_relit.insert(it->first);
		// its river risen or fallen (sprint 115): its held water drawn there
		auto r = rivers.find(it->first);
		const float rise = r == rivers.end() ? 0 : r->second.rise / 16.0f;
		if (rise != b.rise) {
			b.rise = rise;
			// (its rise is blended into the blocks round it: liquids::riseAt)
			for (s16 dz = -1; dz <= 1; dz++)
				for (s16 dx = -1; dx <= 1; dx++)
					for (s16 dy = -1; dy <= 1; dy++)
						m_rebuild.insert(it->first + v3s16(dx, dy, dz));
		}
		++it;
	}
	// by the day's light, and as the grid takes or lets go of liquid, a few at a time
	for (int i = 0; i < PER_FRAME && !m_relit.empty(); i++) {
		m_rebuild.insert(*m_relit.begin());
		m_relit.erase(m_relit.begin());
	}
	m_rebuild.insert(m_unowned.begin(), m_unowned.end());
	m_unowned.clear();
	m_rebuild.insert(m_reflat.begin(), m_reflat.end());
	m_reflat.clear();
	return relight;
}

void Flat::letGo(const std::vector<v3s16> &blocks)
{
	m_unowned.insert(blocks.begin(), blocks.end());
}

void Flat::taken(const std::vector<v3s16> &blocks)
{
	m_reflat.insert(blocks.begin(), blocks.end());
}

size_t Flat::rebuild(ClientMap &map, u32 daynight, bool hurrying)
{
	const s64 from = micros(), budget = hurrying ? HURRIED_US : BUDGET_US;
	for (v3s16 bp : m_rebuild) {
		if (micros() - from > budget) {
			m_reflat.insert(bp);
			continue;
		}
		auto it = m_blocks.find(bp);
		if (it != m_blocks.end())
			build(bp, it->second, map, daynight);
	}
	return m_rebuild.size();
}

void Flat::render(video::IVideoDriver *driver, v3s16 offset)
{
	for (const auto &[bp, b] : m_blocks) {
		if (!b.mesh || b.mesh->getIndexCount() == 0)
			continue;
		// as ClientMap culls its blocks: by the block's middle and reach
		const v3f middle = intToFloat(bp * MAP_BLOCKSIZE, 1) + v3f(MAP_BLOCKSIZE * 0.5f - 0.5f);
		if (middle.getDistanceFrom(m_camera) > m_range + BLOCK_REACH)
			continue;
		core::matrix4 world;
		world.setTranslation(intToFloat(bp * MAP_BLOCKSIZE - offset, BS));
		driver->setTransform(video::ETS_WORLD, world);
		driver->drawMeshBuffer(b.mesh);
	}
}

// A sea node's faces, looked at once when its ground changes: its top if
// nothing's over it, and its sides where what's beside stands lower (not
// against ground: the sea beside a smoothed shore meets the shore's fill)
void Flat::seaFaces(v3s16 p, Shown &s, ClientMap &map)
{
	const float above = besideLevel(map, m_ndef, p + v3s16(0, 1, 0));
	s.top = !(above >= 0 && above <= 1);
	for (int i = 0; i < 4; i++) {
		const v3s16 n = p + SIDES[i];
		const float there = besideLevel(map, m_ndef, n);
		s.from[i] = there >= 1 || (there < 0 && map.solidShape(n)) ? -1 : s8(std::lround(std::max(0.0f, there) * 16));
	}
}

// A block's fills, as they are now: new ones shown at once (a block just
// arrived), changed ones gliding from where they were shown; and the sea's
// blocks that can be seen (its surface, its edges)
void Flat::take(v3s16 blockpos, ClientMap &map, bool ground)
{
	MapBlock *block = map.getBlockNoCreateNoEx(blockpos);
	Block &b = m_blocks[blockpos];
	std::unordered_map<u16, Shown> was;
	was.swap(b.nodes);
	if (!block)
		return;
	// the sea's kept as it was unless its ground changed (a fill's change is often; looking is dear)
	if (!ground)
		for (const auto &[k, s] : was)
			if (s.sea)
				b.nodes[k] = s;
	const v3s16 base = blockpos * MAP_BLOCKSIZE;
	block->m_fills.each([&](v3s16 rel, const fill::Fill &f) {
		const u16 k = keys::of(rel);
		const float target = f.amount / 255.0f;
		auto old = was.find(k);
		Shown s{ old != was.end() ? old->second.level : target, target, f.r, f.g, f.b };
		b.nodes[k] = s;
		if (std::fabs(s.level - s.target) > SETTLED)
			b.gliding = true;
	});
	// what's gone drains away
	for (const auto &[k, s] : was)
		if (!b.nodes.count(k) && !s.sea && s.level > SETTLED) {
			b.nodes[k] = Shown{ s.level, 0, s.r, s.g, s.b };
			b.gliding = true;
		}
	// the sea, where it can be seen
	auto open = [&](v3s16 p) {
		const MapNode n = map.getNode(p);
		return n.getContent() != CONTENT_IGNORE && m_ndef.get(n).drawtype != NDT_LIQUID && !map.getFill(p);
	};
	for (s16 z = 0; z < MAP_BLOCKSIZE && ground; z++)
		for (s16 y = 0; y < MAP_BLOCKSIZE; y++)
			for (s16 x = 0; x < MAP_BLOCKSIZE; x++) {
				const ContentFeatures &f = m_ndef.get(block->getNodeNoCheck(x, y, z));
				if (f.drawtype != NDT_LIQUID || !itemgroup_get(f.groups, "liquid_layer"))
					continue;
				const v3s16 p = base + v3s16(x, y, z);
				bool seen = open(p + v3s16(0, 1, 0));
				for (int i = 0; i < 4 && !seen; i++)
					seen = open(p + SIDES[i]);
				if (!seen)
					continue;
				const MapNode n = block->getNodeNoCheck(x, y, z);
				const bool held = n.getParam2() & fill::HELD;
				const float top = held ? fill::partOf(n.getParam2()) / 16.0f : 1;
				Shown s{ top, top, (u8)f.color.getRed(), (u8)f.color.getGreen(), (u8)f.color.getBlue(), true, held };
				seaFaces(p, s, map);
				b.nodes[keys::of(v3s16(x, y, z))] = s;
			}
}

// The flat mesh: the sea, rivers and fills the grid doesn't run (Faces)
void Flat::build(v3s16 blockpos, Block &b, ClientMap &map, u32 daynight)
{
	scene::SMeshBuffer *mesh = quads::emptied(b.mesh);
	m_faces.build({ /*blockpos*/ blockpos, /*nodes*/ b.nodes, /*mesh*/ mesh, /*rivers*/ *m_rivers, /*daynight*/ daynight }, map);
	mesh->recalculateBoundingBox();
	mesh->setDirty();
}


} // namespace liquids
