// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/client/world/sim/water/drops.h"
#include "src/client/world/sim/water/grid.h"
#include "src/client/world/sim/water/parts.h"
#include "src/client/world/sim/water/shallow.h"
#include "src/client/render/map/clientmap.h"
#include "library/matter/fill/fill.h"
#include "library/geometry/shape/shape.h"
#include "src/shared/game/content/nodedef.h"
#include <CMeshBuffer.h>
#include <IVideoDriver.h>
#include <algorithm>
#include <cmath>

namespace liquids {

namespace {
const size_t MOST_DROPS = 3000;
const float DROP_LIFE = 3;      // s: a drop that's landed nowhere by then is gone
const u8 DROP_ALPHA = 220;
const float SPLASH = 0.03;      // node²/s of flow outward per node/s a drop lands at
} // namespace

Drops::Drops(Opts o) : m_ndef(o.ndef), m_grid(o.grid), m_grid_runs(o.grid_runs) {}

Drops::~Drops()
{
	if (m_mesh)
		m_mesh->drop();
}

void Drops::pour(const std::vector<Stream> &streams)
{
	std::uniform_real_distribution<float> jitter(-1, 1), early(0, 0.08f);
	for (const Stream &s : streams) {
		if (s.cups == 0)
			continue;
		const int n = std::min<int>(s.cups, 48);
		const float cups = float(s.cups) / n;
		const v3f way = s.to - s.from;
		const float t = std::clamp(way.getLength() / 7, 0.15f, 0.5f);
		v3f v = way / t;
		v.Y += 0.5f * shallow::G * t;
		for (int i = 0; i < n && m_drops.size() < MOST_DROPS; i++) {
			Drop d;
			d.vel = v + v3f(jitter(m_random), jitter(m_random), jitter(m_random)) * 0.25f;
			d.pos = s.from + v3f(jitter(m_random), jitter(m_random), jitter(m_random)) * 0.03f +
					d.vel * early(m_random); // poured through the frame, not at its start
			d.cups = cups;
			d.age = 0;
			d.r = s.r, d.g = s.g, d.b = s.b;
			d.splash = false;
			m_drops.push_back(d);
		}
	}
}

void Drops::fall(ClientMap &map, float dtime)
{
	std::vector<Drop> thrown;
	const int steps = std::max(1, int(std::ceil(std::min(dtime, 0.1f) * 60)));
	const float dt = std::min(dtime, 0.1f) / steps;
	for (int i = 0; i < steps; i++)
		m_drops.erase(std::remove_if(m_drops.begin(), m_drops.end(), [&](Drop &d) {
			d.vel.Y -= shallow::G * dt;
			d.pos += d.vel * dt;
			d.age += dt;
			if (d.age > DROP_LIFE)
				return true;
			if (m_grid_runs) {
				const int landed = land(d, thrown);
				if (landed >= 0)
					return landed == 1;
			}
			return landsOutside(map, d);
		}), m_drops.end());
	m_drops.insert(m_drops.end(), thrown.begin(), thrown.end());
}

// A drop over the grid lands on its water or the ground under it, adding its
// cup to the column it hits, throwing water out round it (and, landing
// fast, a splash of smaller drops); its water's the tile's till the
// server's amount takes over.
int Drops::land(Drop &d, std::vector<Drop> &thrown)
{
	Grid &g = m_grid;
	const int c = g.columnAt(d.pos.X, d.pos.Z);
	if (c < 0 || g.floor[c] == NONE)
		return -1;
	const float surface = g.floor[c] / 16.0f - 0.5f + g.depth[c];
	if (d.pos.Y > surface)
		return 0;
	std::uniform_real_distribution<float> jitter(-1, 1);
	const float speed = d.vel.getLength();
	float kept = d.cups;
	if (!d.splash && g.depth[c] > 0 && speed > 3.5f && thrown.size() + m_drops.size() < MOST_DROPS) {
		for (int k = 0; k < 2; k++) {
			Drop s = d;
			s.splash = true;
			s.cups = d.cups * 0.25f;
			s.age = 0;
			s.vel = v3f(jitter(m_random), 0, jitter(m_random)) * speed * 0.2f +
					v3f(0, speed * (0.2f + 0.15f * jitter(m_random)), 0);
			s.pos.Y = surface + 0.02f;
			thrown.push_back(s);
		}
		kept = d.cups * 0.5f;
	}
	g.depth[c] += kept / 4096 * g.res * g.res; // a cup is a sixteenth-cube
	const int t = g.tileOf(c);
	Tile &tile = g.tile[t];
	tile.credit += kept / 4096;
	if (!tile.coloured) {
		tile.r = d.r, tile.g = d.g, tile.b = d.b;
		tile.coloured = true;
	}
	for (int s = 0; s < 4; s++)
		g.out[c][s] += SPLASH * speed;
	g.wake(t);
	return 1;
}

// Past the grid: a drop's gone where it meets liquid or ground
bool Drops::landsOutside(ClientMap &map, const Drop &d) const
{
	const v3s16 p(std::lround(d.pos.X), std::lround(d.pos.Y), std::lround(d.pos.Z));
	const MapNode n = map.getNode(p);
	if (n.getContent() == CONTENT_IGNORE)
		return true;
	const ContentFeatures &cf = m_ndef.get(n);
	if (cf.drawtype == NDT_LIQUID)
		return true; // into the sea
	if (const fill::Fill *there = map.getFill(p)) {
		const std::optional<shape::Shape> solid = map.solidShape(p);
		if (d.pos.Y < p.Y - 0.5f + fill::Room::of(solid ? &*solid : nullptr).surface(there->amount / 255.0f))
			return true;
	}
	const int x = std::clamp(int((d.pos.X - (p.X - 0.5f)) * 16), 0, 15);
	const int y = std::clamp(int((d.pos.Y - (p.Y - 0.5f)) * 16), 0, 15);
	const int z = std::clamp(int((d.pos.Z - (p.Z - 0.5f)) * 16), 0, 15);
	if (const std::optional<shape::Shape> solid = map.solidShape(p))
		return solid->has(v3s16(x, y, z));
	return cf.walkable;
}

void Drops::build(ClientMap &map, v3s16 offset, u32 daynight)
{
	scene::SMeshBuffer *mesh = quads::emptied(m_mesh);
	auto &vertices = mesh->Vertices->Data;
	auto &indices = mesh->Indices->Data;
	const v3f origin = intToFloat(offset, 1);
	static const float H = 1 / 32.0f;
	// five faces (it's seen from above and the sides), each corners and a normal
	static const v3f FACES[5][5] = {
		{ { -H, H, -H }, { -H, H, H }, { H, H, H }, { H, H, -H }, { 0, 1, 0 } },
		{ { H, -H, -H }, { H, H, -H }, { H, H, H }, { H, -H, H }, { 1, 0, 0 } },
		{ { -H, -H, H }, { -H, H, H }, { -H, H, -H }, { -H, -H, -H }, { -1, 0, 0 } },
		{ { H, -H, H }, { H, H, H }, { -H, H, H }, { -H, -H, H }, { 0, 0, 1 } },
		{ { -H, -H, -H }, { -H, H, -H }, { H, H, -H }, { H, -H, -H }, { 0, 0, -1 } },
	};
	for (const Drop &d : m_drops) {
		if (vertices.size() + 20 > 65535)
			break;
		const v3s16 p(std::lround(d.pos.X), std::lround(d.pos.Y), std::lround(d.pos.Z));
		const float shade = light::shade(map, m_ndef, p, daynight);
		const v3f at = d.pos - origin;
		for (int f = 0; f < 5; f++) {
			const float by = shade * (f == 0 ? 1.1f : 0.85f);
			const video::SColor colour(DROP_ALPHA, u32(std::min(255.0f, d.r * by)), u32(std::min(255.0f, d.g * by)),
					u32(std::min(255.0f, d.b * by)));
			const u16 first = u16(vertices.size());
			for (int c = 0; c < 4; c++)
				vertices.emplace_back((at + FACES[f][c]) * BS, FACES[f][4], colour, v2f(0, 0));
			for (u16 i : { 0, 1, 2, 2, 3, 0 })
				indices.push_back(first + i);
		}
	}
	mesh->recalculateBoundingBox();
	mesh->setDirty();
}

void Drops::render(video::IVideoDriver *driver)
{
	if (!m_mesh || m_mesh->getIndexCount() == 0)
		return;
	driver->setTransform(video::ETS_WORLD, core::matrix4());
	driver->drawMeshBuffer(m_mesh);
}

} // namespace liquids
