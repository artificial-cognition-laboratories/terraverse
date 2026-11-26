// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/client/views/game/far/tiles/mask.h"
#include "src/shared/base/constants.h"
#include "src/shared/base/exceptions.h"
#include "src/shared/platform/system/log.h"
#include "library/world/planet/atlas.h"
#include <IVideoDriver.h>
#include <algorithm>
#include <limits>
#include <optional>
#include <string>
#include <vector>

namespace far {

namespace {

// A region the near world reaches, wider than this many columns a side, is
// cut down to it round its middle (a near range is a few dozen).
constexpr u32 MOST = 256;

struct Column
{
	int64_t i, j;              // the grid cell its block starts at
	const std::vector<s16> *ys; // the blocks of it the near world has
};

// The 32 blocks from `base` a texel holds.
constexpr s16 BITS = 32;

// The cell a map block column starts at, if it's one of the body's.
std::optional<planet::Atlas::Cell> columnCell(const planet::Atlas &atlas, int body, v2s16 column, s16 block)
{
	const int32_t x = column.X * MAP_BLOCKSIZE, z = column.Y * MAP_BLOCKSIZE;
	for (int32_t y : { (int32_t)block * MAP_BLOCKSIZE, 0 })
		if (const std::optional<planet::Atlas::Cell> c = atlas.cell({ x, y, z }))
			return c->body == body ? c : std::nullopt;
	return std::nullopt;
}

int64_t floorDiv(int64_t a, int64_t b)
{
	return a / b - (a % b != 0 && (a < 0) != (b < 0));
}

// A column's blocks as a texel's bits (far/mask.h).
video::SColor bitsOf(const std::vector<s16> &ys, s16 base)
{
	uint32_t bits = 0;
	for (s16 y : ys)
		if (y >= base && y < base + BITS)
			bits |= 1u << (y - base);
	return video::SColor(bits >> 24 & 0xff, bits & 0xff, bits >> 8 & 0xff, bits >> 16 & 0xff);
}

} // namespace

Mask::Mask(video::IVideoDriver *driver, const planet::Atlas &atlas, int body) :
	m_driver(driver), m_atlas(atlas), m_body(body)
{}

Mask::~Mask()
{
	drop();
}

void Mask::drop()
{
	for (auto &[_, f] : m_faces)
		m_driver->removeTexture(f.texture);
	m_faces.clear();
}

const Mask::Face *Mask::face(int face) const
{
	auto it = m_faces.find(face);
	return it == m_faces.end() ? nullptr : &it->second;
}

void Mask::update(const Coverage &coverage, s16 eye_block)
{
	if (coverage.generation() == m_generation && eye_block == m_eye_block)
		return;
	m_generation = coverage.generation();
	m_eye_block = eye_block;
	const s16 base = eye_block - BITS / 2;
	drop();

	// each column by its region's grid
	const std::vector<terragen::Atlas::Grid> &grids = m_atlas.body(m_body).grids();
	std::map<int, std::vector<Column>> by;
	for (const auto &[column, ys] : coverage.columns()) {
		const std::optional<planet::Atlas::Cell> c = columnCell(m_atlas, m_body, column, ys.front());
		if (!c)
			continue; // seam
		int face = -1;
		for (size_t g = 0; g < grids.size(); g++)
			if (grids[g].region == c->region && grids[g].index == c->index)
				face = (int)g;
		if (face < 0)
			throw BaseException("far view: a near block's region has no grid");
		by[face].push_back({ c->i, c->j, &ys });
	}

	for (const auto &[face, columns] : by) {
		// a map block is 16 cells of its grid, from the same phase throughout (tiles
		// sit on the grid's multiples of planet::Atlas::TILE, slots on whole blocks)
		const int64_t phase_i = ((columns[0].i % MAP_BLOCKSIZE) + MAP_BLOCKSIZE) % MAP_BLOCKSIZE;
		const int64_t phase_j = ((columns[0].j % MAP_BLOCKSIZE) + MAP_BLOCKSIZE) % MAP_BLOCKSIZE;
		int64_t ci0 = std::numeric_limits<int64_t>::max(), cj0 = ci0, ci1 = std::numeric_limits<int64_t>::min(), cj1 = ci1;
		for (const Column &c : columns) {
			if (((c.i - phase_i) % MAP_BLOCKSIZE) != 0 || ((c.j - phase_j) % MAP_BLOCKSIZE) != 0)
				throw BaseException("far view: a map block doesn't start on its grid's blocks");
			const int64_t ci = floorDiv(c.i, MAP_BLOCKSIZE), cj = floorDiv(c.j, MAP_BLOCKSIZE);
			ci0 = std::min(ci0, ci), ci1 = std::max(ci1, ci);
			cj0 = std::min(cj0, cj), cj1 = std::max(cj1, cj);
		}
		// round the middle, if it's more than a texture holds
		const auto fit = [](int64_t &lo, int64_t &hi) {
			if (hi - lo + 1 > MOST) {
				const int64_t mid = (lo + hi) / 2;
				lo = mid - MOST / 2;
				hi = lo + MOST - 1;
			}
		};
		fit(ci0, ci1);
		fit(cj0, cj1);
		const u32 width = (u32)(ci1 - ci0 + 1), height = (u32)(cj1 - cj0 + 1);

		video::IImage *image = m_driver->createImage(video::ECF_A8R8G8B8, core::dimension2du(width, height));
		image->fill(video::SColor(0, 0, 0, 0));
		for (const Column &c : columns) {
			const int64_t ti = floorDiv(c.i, MAP_BLOCKSIZE) - ci0, tj = floorDiv(c.j, MAP_BLOCKSIZE) - cj0;
			if (ti < 0 || tj < 0 || ti >= width || tj >= height)
				continue;
			image->setPixel((u32)ti, (u32)tj, bitsOf(*c.ys, base));
		}
		// a texel is a column, never blended with its neighbour's
		const bool mipmaps = m_driver->getTextureCreationFlag(video::ETCF_CREATE_MIP_MAPS);
		m_driver->setTextureCreationFlag(video::ETCF_CREATE_MIP_MAPS, false);
		video::ITexture *texture = m_driver->addTexture(
				"far:mask:" + std::to_string((uintptr_t)this) + ":" + std::to_string(face), image);
		m_driver->setTextureCreationFlag(video::ETCF_CREATE_MIP_MAPS, mipmaps);
		image->drop();
		if (!texture)
			throw BaseException("far view: the near world's mask couldn't be made");
		m_faces.emplace(face, Face{ texture, ci0 * MAP_BLOCKSIZE + phase_i, cj0 * MAP_BLOCKSIZE + phase_j, width, height, base });
	}
}

} // namespace far
