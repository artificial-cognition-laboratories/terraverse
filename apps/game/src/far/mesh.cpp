// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "mesh.h"
#include "constants.h"
#include "exceptions.h"
#include "light.h"
#include <algorithm>
#include <cmath>

namespace far {

// The near mesh's shade per face (client/mesh.cpp applyFacesShading).
static const float SHADE[6] = { 1.0f, 0.447213f, 0.670820f, 0.670820f, 0.836660f, 0.836660f };
static const v3f NORMAL[6] = { { 0, 1, 0 }, { 0, -1, 0 }, { 1, 0, 0 }, { -1, 0, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };


namespace {

// Collects quads into parts of at most 65536 vertices, one quarter's each.
class Builder
{
public:
	Mesh mesh;
	u8 quarter = 0; // the quarter the cell being built lies in

	// a quad from four corners (in blocks), wound to face `face`. A liquid's
	// faces carry alpha 0: the shader drops them where the near world draws
	// its own liquid there.
	void quad(const v3f (&corner)[4], Face face, video::SColorf colour)
	{
		quad(corner, face, { 1, 1, 1, 1 }, colour);
	}

	// with each corner lit to its own level (0..1), as smooth lighting does
	void quad(const v3f (&corner)[4], Face face, const float (&lit)[4], video::SColorf colour, v3f normal = {})
	{
		if (normal == v3f())
			normal = NORMAL[face];
		int &at = m_last[quarter];
		if (at < 0 || mesh.parts[at].vertices.size() + 4 > 65536) {
			at = (int)mesh.parts.size();
			mesh.parts.emplace_back().quarter = quarter;
		}
		Mesh::Part &p = mesh.parts[at];
		u16 base = p.vertices.size();
		for (int k = 0; k < 4; k++) {
			float s = SHADE[face] * lit[k];
			video::SColor c = video::SColorf(colour.r * s, colour.g * s, colour.b * s, 1).toSColor();
			p.vertices.emplace_back(corner[k] * BS, normal, c, v2f(0, 0));
		}
		p.indices.insert(p.indices.end(), { base, (u16)(base + 1), (u16)(base + 2), base, (u16)(base + 2), (u16)(base + 3) });
	}

	// the horizontal face at height y over [x0, x1] × [z0, z1]
	// `normal`: the land's way up where it slopes (none: the face's own)
	void flat(float x0, float x1, float z0, float z1, float y, Face face, video::SColorf colour, v3f normal = {})
	{
		static const float LIT[4] = { 1, 1, 1, 1 };
		if (face == UP)
			quad({ { x0, y, z0 }, { x0, y, z1 }, { x1, y, z1 }, { x1, y, z0 } }, face, LIT, colour, normal);
		else
			quad({ { x0, y, z0 }, { x1, y, z0 }, { x1, y, z1 }, { x0, y, z1 } }, face, colour);
	}

	// tops at height y over [xs[0], xs[n]] × [z0, z1], alike (colour, normal),
	// as one strip: a vertex pair at every cell's edge, shared, so the strip
	// bends onto the planet as its neighbours do (one long quad, bent only at
	// its ends, left cracks against the next row's)
	void row(const std::vector<float> &xs, float z0, float z1, float y, video::SColorf colour, v3f normal)
	{
		if (normal == v3f())
			normal = NORMAL[UP];
		const size_t n = xs.size();
		int &at = m_last[quarter];
		if (at < 0 || mesh.parts[at].vertices.size() + 2 * n > 65536) {
			at = (int)mesh.parts.size();
			mesh.parts.emplace_back().quarter = quarter;
		}
		Mesh::Part &p = mesh.parts[at];
		const u16 base = p.vertices.size();
		const video::SColor c = video::SColorf(colour.r * SHADE[UP], colour.g * SHADE[UP], colour.b * SHADE[UP], 1).toSColor();
		for (float x : xs) {
			p.vertices.emplace_back(v3f(x, y, z0) * BS, normal, c, v2f(0, 0));
			p.vertices.emplace_back(v3f(x, y, z1) * BS, normal, c, v2f(0, 0));
		}
		// as flat()'s UP quad winds: (x0, z0), (x0, z1), (x1, z1), (x1, z0)
		for (size_t k = 0; k + 1 < n; k++) {
			const u16 a = (u16)(base + 2 * k), b = (u16)(a + 1), d = (u16)(a + 2), e = (u16)(a + 3);
			p.indices.insert(p.indices.end(), { a, b, e, a, e, d });
		}
	}

	// the vertical face on the `face` side of [x0, x1] × [z0, z1], from y0 up to y1
	// `low` and `high`: how lit its bottom and top edges are
	void side(float x0, float x1, float z0, float z1, float y0, float y1, Face face, video::SColorf colour,
			float low = 1, float high = 1)
	{
		if (y1 <= y0)
			return;
		const float lit[4] = { low, high, high, low }; // each case below runs bottom, top, top, bottom
		switch (face) {
		case EAST:  quad({ { x1, y0, z0 }, { x1, y1, z0 }, { x1, y1, z1 }, { x1, y0, z1 } }, face, lit, colour); break;
		case WEST:  quad({ { x0, y0, z1 }, { x0, y1, z1 }, { x0, y1, z0 }, { x0, y0, z0 } }, face, lit, colour); break;
		case NORTH: quad({ { x1, y0, z1 }, { x1, y1, z1 }, { x0, y1, z1 }, { x0, y0, z1 } }, face, lit, colour); break;
		case SOUTH: quad({ { x0, y0, z0 }, { x0, y1, z0 }, { x1, y1, z0 }, { x1, y0, z0 } }, face, lit, colour); break;
		default: break;
		}
	}

private:
	int m_last[4] = { -1, -1, -1, -1 }; // each quarter's part being filled
};

// How far a tile's skirt hangs below its edge cells' ground, in its cells.
static constexpr s32 SKIRT = 2;
// How far, in its cells, a cell reaches into a seam beside it.
static constexpr float REACH = 0.5f;

struct Span
{
	float low, high;
};

// [low, high] less what `cover` hides: up to two pieces
static int subtract(Span s, Span cover, Span out[2])
{
	if (cover.high <= s.low || cover.low >= s.high) {
		out[0] = s;
		return 1;
	}
	int n = 0;
	if (cover.low > s.low)
		out[n++] = { s.low, cover.low };
	if (cover.high < s.high)
		out[n++] = { cover.high, s.high };
	return n;
}

// faces are at block edges: a block at y fills y − ½ .. y + ½
static float top(s32 y)
{
	return (float)y + 0.5f;
}

// Full sunlight on the engine's light curve. (The curve is the client's:
// set_light_curve. Unset, every level is 0.)
static float sunLight()
{
	const float sun = (float)decode_light(LIGHT_SUN);
	if (sun == 0)
		throw BaseException("far mesh: the light curve isn't set (set_light_curve)");
	return sun;
}
// Under water, light falls a level per block of it (water passes light,
// not sunlight). At height y (a face's edge) under a surface at `water`.
static float lightAt(float y, s32 water)
{
	const float sun = sunLight();
	s32 depth = (s32)std::ceil((float)water + 0.5f - y);
	return (float)decode_light((u8)std::clamp(LIGHT_SUN - depth, 0, (int)LIGHT_SUN)) / sun;
}

} // namespace

video::SColorf surface(const Cell &cell, const Palette &palette)
{
	if (cell.water <= cell.ground)
		return palette.face(cell.top, UP);
	// the liquid, seen through as the near world's is
	return under(palette, cell.liquid, palette.face(cell.top, UP), cell.water - cell.ground);
}

video::SColorf under(const Palette &palette, MapNode liquid, video::SColorf floor, s32 depth)
{
	return palette.through(liquid, floor, lightAt(top(0), depth));
}

Mesh mesh(const Tile &tile, const Palette &palette)
{
	Builder b;
	const s32 cell = tile.key.cell();
	static const s32 DI[4] = { 1, -1, 0, 0 }, DJ[4] = { 0, 0, 1, -1 };
	static const Face SIDE[4] = { EAST, WEST, NORTH, SOUTH };
	sunLight(); // tiles are lit by it: unset, say so whatever the tile
	// A row's tops alike side by side (as high, as coloured, as sloped: a
	// sea's, a plain's) as one face: cell by cell, a planet's worth of them
	// was most of what the Planetarium sent the GPU each frame (sprint 114)
	struct Tops
	{
		bool open = false;
		std::vector<float> xs; // its cells' edges
		float z0 = 0, z1 = 0, y = 0;
		video::SColorf colour;
		v3f normal;
		u8 quarter = 0;
	} run;
	const auto flush = [&] {
		if (!run.open)
			return;
		const u8 was = b.quarter;
		b.quarter = run.quarter;
		b.row(run.xs, run.z0, run.z1, run.y, run.colour, run.normal);
		b.quarter = was;
		run.open = false;
	};
	for (s32 j = 0; j < TILE; j++) {
		const float cz0 = (float)(j * cell) - 0.5f, cz1 = cz0 + (float)cell;
		flush();
		// this cell's top: onto the run if it carries it on, else a run of its own
		const auto topFace = [&](float x0, float x1, float z0, float z1, float y, video::SColorf colour, v3f normal) {
			const video::SColorf &r = run.colour;
			if (run.open && run.quarter == b.quarter && run.xs.back() == x0 && run.z0 == z0 && run.z1 == z1 && run.y == y &&
					r.r == colour.r && r.g == colour.g && r.b == colour.b && run.normal == normal) {
				run.xs.push_back(x1);
				return;
			}
			flush();
			run.open = true;
			run.xs.assign({ x0, x1 });
			run.z0 = z0;
			run.z1 = z1;
			run.y = y;
			run.colour = colour;
			run.normal = normal;
			run.quarter = b.quarter;
		};
		for (s32 i = 0; i < TILE; i++) {
			const Cell &c = tile.at(i, j);
			if (c.space)
				continue;
			b.quarter = (u8)((i >= TILE / 2) | (j >= TILE / 2) << 1);
			const float cx0 = (float)(i * cell) - 0.5f, cx1 = cx0 + (float)cell;
			const bool wet = c.water > c.ground;
			// A cell beside one the atlas has none of reaches half a cell into
			// it: a coarse cell is no cell when its middle isn't, though up to
			// half of it is, and the region across the seam stops the same way;
			// between them the background showed (TestFar testNoHolesFromAbove).
			const float reach = REACH * cell;
			const float x0 = tile.at(i - 1, j).space ? cx0 - reach : cx0, x1 = tile.at(i + 1, j).space ? cx1 + reach : cx1;
			const float z0 = tile.at(i, j - 1).space ? cz0 - reach : cz0, z1 = tile.at(i, j + 1).space ? cz1 + reach : cz1;
			if (!wet) {
				// its top faces the way the land slopes across it, so a view that
				// shades relief (the Planetarium's) can; the far view's shader
				// doesn't read normals
				const auto height = [&](s32 di, s32 dj) {
					const Cell &n = tile.at(i + di, j + dj);
					return (float)(n.space ? c.ground : n.ground);
				};
				const float gx = (height(1, 0) - height(-1, 0)) / (2.0f * cell), gz = (height(0, 1) - height(0, -1)) / (2.0f * cell);
				topFace(x0, x1, z0, z1, top(c.ground), surface(c, palette), v3f(-gx, 1, -gz).normalize());
			} else {
				topFace(x0, x1, z0, z1, top(c.water), surface(c, palette), v3f());
			}

			for (int k = 0; k < 4; k++) {
				const s32 ni = i + DI[k], nj = j + DJ[k];
				Cell n = tile.at(ni, nj);
				const bool within = !n.space && ni >= 0 && ni < TILE && nj >= 0 && nj < TILE;
				// at the tile's edge, a skirt: the next tile may be coarser or finer,
				// its edge not meeting ours vertex for vertex; a wall hung below the
				// ground fills what would show through. Toward a seam too, from
				// where the cell reaches: the region across it may stand lower.
				if (n.space) {
					n.ground = c.ground - SKIRT * cell;
					n.water = std::max(n.ground, c.water) - SKIRT * cell;
				} else if (ni < 0 || ni >= TILE || nj < 0 || nj >= TILE) {
					n.ground = std::min(n.ground, c.ground - SKIRT * cell);
					n.water = std::min(n.water, std::max(n.ground, c.water) - SKIRT * cell);
				}
				Face face = SIDE[k];
				// a cliff: the soil's bands, then rock. Not one all under the
				// water beside it: its top is drawn opaque over it (the far
				// view's and the Planetarium's), and a sea floor's every step
				// went to the GPU unseen (sprint 114)
				if (n.ground < c.ground && !(within && n.water > n.ground && n.water >= c.ground)) {
					float low = top(n.ground), high = top(c.ground);
					float bands[3] = { top(c.top_end), top(c.filler_end), low };
					MapNode nodes[3] = { c.top, c.filler, c.rock };
					float upper = high;
					// a step under water is lit as deep as it is: the neighbour's
					// water is what lies against it
					bool under = n.water > n.ground;
					for (int band = 0; band < 3 && upper > low; band++) {
						float lower = std::max(bands[band], low);
						b.side(x0, x1, z0, z1, lower, upper, face, palette.face(nodes[band], face),
								under ? lightAt(lower, n.water) : 1.0f, under ? lightAt(upper, n.water) : 1.0f);
						upper = std::min(upper, lower);
					}
				}
				// a liquid's edge, where the land beyond is lower than its surface
				if (wet) {
					float beyond = top(std::max(n.ground, n.water));
					b.side(x0, x1, z0, z1, std::max(beyond, top(c.ground)), top(c.water), face,
							palette.face(c.liquid, face));
				}
			}

			// the trees: boxes of their blocks, open where a neighbour covers them
			for (const Run *run : { &c.trunk, &c.canopy }) {
				if (run->empty())
					continue;
				Span s{ (float)run->low - 0.5f, top(run->high) };
				b.flat(x0, x1, z0, z1, s.high, UP, palette.face(run->node, UP));
				if (run->low > c.ground + 1)
					b.flat(x0, x1, z0, z1, s.low, DOWN, palette.face(run->node, DOWN));
				for (int k = 0; k < 4; k++) {
					const Cell &n = tile.at(i + DI[k], j + DJ[k]);
					Span pieces[4] = { s };
					int count = 1;
					Span covers[3] = { { -1e9f, top(n.ground) },
						{ (float)n.trunk.low - 0.5f, n.trunk.empty() ? -1e9f : top(n.trunk.high) },
						{ (float)n.canopy.low - 0.5f, n.canopy.empty() ? -1e9f : top(n.canopy.high) } };
					for (const Span &cover : covers) {
						Span next[4];
						int m = 0;
						for (int p = 0; p < count; p++)
							m += subtract(pieces[p], cover, next + m);
						std::copy(next, next + m, pieces);
						count = m;
					}
					for (int p = 0; p < count; p++)
						b.side(x0, x1, z0, z1, pieces[p].low, pieces[p].high, SIDE[k], palette.face(run->node, SIDE[k]));
				}
			}

		}
	}
	flush(); // (the last row's)
	return std::move(b.mesh);
}

} // namespace far
