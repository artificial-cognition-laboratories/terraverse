// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "far/tiling.h"
#include "far/terrain.h"
#include "far/mesh.h"
#include "constants.h"
#include "exceptions.h"
#include <algorithm>
#include <cmath>

namespace far {

using Vec = Tiling::Vec;
using Region = terragen::Atlas::Region;

Tiling::Tiling(const terragen::Atlas &atlas) : m_atlas(atlas)
{
	const std::vector<terragen::Atlas::Grid> &grids = m_atlas.grids();
	for (size_t g = 0; g < grids.size(); g++) {
		if (grids[g].region == Region::Core)
			continue; // no surface
		const int64_t width = grids[g].high[0] - grids[g].low[0] + 1, depth = grids[g].high[1] - grids[g].low[1] + 1;
		s32 level = 0;
		while ((int64_t)TILE << level < std::min(width, depth))
			level++;
		const int64_t span = (int64_t)TILE << level;
		for (int64_t z = 0; z * span < depth; z++)
			for (int64_t x = 0; x * span < width; x++)
				m_roots.push_back({ level, (s32)x, (s32)z, (s32)g });
	}
	if (m_roots.empty())
		throw BaseException("far tiling: the atlas has no surface");
}

terragen::Atlas::Cell Tiling::cell(const TileKey &key, s32 i, s32 j) const
{
	const terragen::Atlas::Grid &g = m_atlas.grids().at(key.face);
	const s64 c = key.cell();
	return { g.region, g.index, g.low[0] + (s64)key.x * key.span() + i * c + c / 2,
		g.low[1] + (s64)key.z * key.span() + j * c + c / 2, 0 };
}

bool Tiling::inGrid(const TileKey &key, const terragen::Atlas::Cell &cell) const
{
	const terragen::Atlas::Grid &g = m_atlas.grids().at(key.face);
	return cell.i >= g.low[0] && cell.i <= g.high[0] && cell.j >= g.low[1] && cell.j <= g.high[1];
}

bool Tiling::within(const TileKey &key) const
{
	const terragen::Atlas::Grid &g = m_atlas.grids().at(key.face);
	const s64 span = key.span();
	return key.x >= 0 && key.z >= 0 && g.low[0] + (s64)key.x * span <= g.high[0] && g.low[1] + (s64)key.z * span <= g.high[1];
}

Vec Tiling::point(const TileKey &key, double x, double y, double z) const
{
	// a cell centred on whole numbers: its corner at −½; a layer's top at y + ½
	const terragen::Atlas::Grid &g = m_atlas.grids().at(key.face);
	const double span = key.span();
	const terragen::Atlas::Cell origin{ g.region, g.index, 0, 0, 0 };
	return m_atlas.at(origin, g.low[0] + key.x * span + x, g.low[1] + key.z * span + z, y);
}

Tiling::Extent Tiling::extent(const TileKey &key) const
{
	const terragen::Atlas::Grid &g = m_atlas.grids().at(key.face);
	const double span = key.span();
	// the grid's cells, from the tile's corner
	const double gx = (double)(g.high[0] - g.low[0] + 1) - key.x * span, gz = (double)(g.high[1] - g.low[1] + 1) - key.z * span;
	return { -0.5, std::min(span, gx) - 0.5, -0.5, std::min(span, gz) - 0.5 };
}

Vec Tiling::middle(const TileKey &key) const
{
	const Extent e = extent(key);
	return point(key, (e.x0 + e.x1) / 2, -0.5, (e.z0 + e.z1) / 2);
}

double Tiling::diagonal(const TileKey &key) const
{
	const Extent e = extent(key);
	const Vec a = point(key, e.x0, -0.5, e.z0), b = point(key, e.x1, -0.5, e.z1);
	return std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y) + (a.z - b.z) * (a.z - b.z));
}

void Tiling::place(Mesh &mesh, const TileKey &key) const
{
	const Vec mid = middle(key);
	const float half = key.span() / 2.0f - 0.5f;
	// the tile's own axes at its middle, for the normals
	const auto axis = [&](double dx, double dy, double dz) {
		const Vec a = point(key, half + dx, -0.5 + dy, half + dz), b = point(key, half - dx, -0.5 - dy, half - dz);
		return v3f(a.x - b.x, a.y - b.y, a.z - b.z).normalize();
	};
	const v3f ex = axis(0.5, 0, 0), ez = axis(0, 0, 0.5);
	for (Mesh::Part &part : mesh.parts)
		for (video::S3DVertex &v : part.vertices) {
			const v3f t = v.Pos / BS;
			const Vec p = point(key, t.X, t.Y, t.Z);
			v.Pos = v3f(p.x - mid.x, p.y - mid.y, p.z - mid.z) * BS;
			// up is the planet's at the vertex, not the tile's middle: else a
			// flat top leans more toward the tile's edge, and shading by the
			// normal draws every tile's outline (east and north kept square to it)
			const v3f up = v3f(p.x, p.y, p.z).normalize();
			const v3f east = (ex - up * ex.dotProduct(up)).normalize();
			const v3f north = up.crossProduct(east) * (up.crossProduct(east).dotProduct(ez) < 0 ? -1.f : 1.f);
			v.Normal = (east * v.Normal.X + up * v.Normal.Y + north * v.Normal.Z).normalize();
		}
}

} // namespace far
