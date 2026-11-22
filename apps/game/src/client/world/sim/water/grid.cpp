// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/client/world/sim/water/grid.h"
#include "src/client/world/sim/water/parts.h"
#include "src/shared/map/block/mapblock.h"
#include <algorithm>
#include <cmath>

namespace liquids {

namespace {
const int ALIGN = 4; // nodes: the grid's corner sits on this
} // namespace

Grid::Grid(Opts o) :
	res(o.res), nodes(ALIGN * int(std::lround(float(o.nodes) / ALIGN))), follow(o.follow), scans(o.scans),
	cols(nodes * res), tiles(cols / 16)
{
	for (int t = 0; t < tiles * tiles; t++)
		nearest.push_back(t);
	auto away = [&](int t) {
		const float x = t % tiles - tiles / 2 + 0.5f, z = t / tiles - tiles / 2 + 0.5f;
		return x * x + z * z;
	};
	std::sort(nearest.begin(), nearest.end(), [&](int a, int b) { return away(a) < away(b); });
}

Grid::Placed Grid::place(v3f camera)
{
	Placed placing;
	std::vector<v3s16> &let_go = placing.let_go;
	const v3s16 at(std::lround(camera.X), std::lround(camera.Y), std::lround(camera.Z));
	auto aligned = [](int v) { return int(std::floor(v / float(ALIGN))) * ALIGN; };
	// its height follows only a climb or fall past `follow` (its reach is by height)
	const s16 y = placed && std::abs(at.Y - origin.Y) <= follow ? origin.Y : at.Y;
	const v3s16 want_at(aligned(at.X - nodes / 2), y, aligned(at.Z - nodes / 2));
	if (placed && want_at == origin)
		return placing;
	const size_t columns = size_t(cols) * cols;
	std::vector<s32> n_floor(columns, NONE), n_shown(columns, NONE), n_sea(columns, NONE), n_want(columns, NONE);
	std::vector<float> n_depth(columns, 0);
	std::vector<std::array<float, 4>> n_out(columns, std::array<float, 4>{});
	std::vector<std::optional<v3s16>> n_tops(size_t(nodes) * nodes);
	std::vector<float> n_runs(size_t(nodes) * nodes, 0);
	std::vector<s32> n_levels(size_t(nodes) * nodes, NONE);
	std::vector<std::array<float, 2>> n_ways(size_t(nodes) * nodes, std::array<float, 2>{});
	std::vector<Tile> n_tile(size_t(tiles) * tiles);
	// it keeps all it has where it overlaps (thrown away and started again from the
	// server's amounts, a spreading puddle jumped back to the server's squares);
	// a climb or fall looks at it all again, keeping its water where it stands
	const bool keep = placed;
	const bool climbed = placed && want_at.Y != origin.Y;
	const int p = per();
	for (int tz = 0; tz < tiles && placed; tz++)
		for (int tx = 0; tx < tiles; tx++) {
			Tile &old = tile[tz * tiles + tx];
			const int mx = tx + (origin.X - want_at.X) / p, mz = tz + (origin.Z - want_at.Z) / p;
			if (!keep || mx < 0 || mx >= tiles || mz < 0 || mz >= tiles || !old.known) {
				// left behind: what it drew is drawn flat again, at once
				for (int nz = 0; nz < p; nz++)
					for (int nx = 0; nx < p; nx++)
						if (const auto &top = tops[(tz * p + nz) * nodes + tx * p + nx])
							let_go.push_back(getNodeBlockPos(*top));
				continue;
			}
			Tile &t = n_tile[mz * tiles + mx];
			t = old;
			t.drawn = false; // its texels are written again where it now is
			for (int cz = 0; cz < 16; cz++)
				for (int cx = 0; cx < 16; cx++) {
					const int from = (tz * 16 + cz) * cols + tx * 16 + cx, to = (mz * 16 + cz) * cols + mx * 16 + cx;
					n_floor[to] = floor[from];
					n_depth[to] = depth[from];
					n_out[to] = out[from];
					n_shown[to] = shown[from];
					n_sea[to] = sea[from];
					n_want[to] = want[from];
				}
			for (int nz = 0; nz < p; nz++)
				for (int nx = 0; nx < p; nx++) {
					const int from = (tz * p + nz) * nodes + tx * p + nx, to = (mz * p + nz) * nodes + mx * p + nx;
					n_tops[to] = tops[from];
					n_runs[to] = runs[from];
					n_levels[to] = levels[from];
					n_ways[to] = ways[from];
				}
		}
	const bool was = placed;
	const v3s16 old = origin;
	floor.swap(n_floor);
	shown.swap(n_shown);
	sea.swap(n_sea);
	want.swap(n_want);
	depth.swap(n_depth);
	out.swap(n_out);
	current.assign(columns, std::array<float, 2>{}); // found again with the pools
	tops.swap(n_tops);
	runs.swap(n_runs);
	levels.swap(n_levels);
	ways.swap(n_ways);
	tile.swap(n_tile);
	origin = want_at;
	placed = true;
	if (climbed)
		for (Tile &t : tile)
			t.scanned = false;
	placing.moved = true; // (its tiles' texels are written again as they're drawn)
	// at its new edges, nodes it had beside its tiles are gone: their levels found again
	for (int t = 0; t < tiles * tiles; t++) {
		const int tx = t % tiles, tz = t / tiles;
		if (tx == 0 || tz == 0 || tx == tiles - 1 || tz == tiles - 1)
			tile[t].relevel = true;
	}
	// what was the old grid's edge walled its water: run that again where it isn't level
	if (keep && was)
		for (int tz = 0; tz < tiles; tz++)
			for (int tx = 0; tx < tiles; tx++) {
				const int ox = tx + (want_at.X - old.X) / p, oz = tz + (want_at.Z - old.Z) / p;
				const bool edge = ox <= 0 || ox >= tiles - 1 || oz <= 0 || oz >= tiles - 1;
				const int t = tz * tiles + tx;
				if (edge && tile[t].known && uneven(t))
					wake(t);
			}
	return placing;
}

int Grid::columnAt(float x, float z) const
{
	if (!placed)
		return -1;
	const int cx = int(std::floor((x - (origin.X - 0.5f)) * res)), cz = int(std::floor((z - (origin.Z - 0.5f)) * res));
	if (cx < 0 || cx >= cols || cz < 0 || cz >= cols)
		return -1;
	return cz * cols + cx;
}

void Grid::wake(int t)
{
	Tile &k = tile[t];
	k.awake = true;
	k.quiet = 0;
	if (k.stale) {
		k.stale = false;
		k.scanned = false;
	}
}

bool Grid::owns(v3s16 p) const
{
	if (!placed)
		return false;
	const int nx = p.X - origin.X, nz = p.Z - origin.Z;
	if (nx < 0 || nx >= nodes || nz < 0 || nz >= nodes)
		return false;
	const Tile &t = tile[(nz / per()) * tiles + nx / per()];
	const std::optional<v3s16> &top = tops[nz * nodes + nx];
	return t.known && top && *top == p;
}

bool Grid::uneven(int t) const
{
	const int tx = t % tiles, tz = t / tiles;
	for (int s = 0; s < 4; s++) {
		const int nx = tx + SIDES[s].X, nz = tz + SIDES[s].Z;
		if (nx < 0 || nx >= tiles || nz < 0 || nz >= tiles || !tile[nz * tiles + nx].known)
			continue;
		for (int i = 0; i < 16; i++) {
			// this tile's edge column on side s, and the one across
			const int cx = SIDES[s].X > 0 ? 15 : SIDES[s].X < 0 ? 0 : i, cz = SIDES[s].Z > 0 ? 15 : SIDES[s].Z < 0 ? 0 : i;
			const int c = (tz * 16 + cz) * cols + tx * 16 + cx, n = c + stepOf(s);
			if (floor[c] == NONE || floor[n] == NONE || (depth[c] <= 0 && depth[n] <= 0))
				continue;
			// water runs only downhill from what's wet
			const float by = higher(c, n);
			if ((depth[c] > 0 && by > 1 / 32.0f) || (depth[n] > 0 && -by > 1 / 32.0f))
				return true;
		}
	}
	return false;
}

bool Grid::relevel(int t)
{
	Tile &k = tile[t];
	k.relevel = false;
	if (!k.known)
		return false;
	const int tx = t % tiles, tz = t / tiles, p = per();
	const auto levelOf = [&](int nx, int nz) {
		return nx < 0 || nx >= nodes || nz < 0 || nz >= nodes ? NONE : levels[nz * nodes + nx];
	};
	bool changed = false;
	for (int nz = 0; nz < p; nz++)
		for (int nx = 0; nx < p; nx++) {
			const int gx = tx * p + nx, gz = tz * p + nz;
			const s32 own = levelOf(gx, gz);
			std::array<float, 4> corners{};
			if (own != NONE) {
				held::Around around;
				for (int dz = -1; dz <= 1; dz++)
					for (int dx = -1; dx <= 1; dx++)
						around[dz + 1][dx + 1] = levelOf(gx + dx, gz + dz);
				corners = held::corners(around);
			}
			for (int iz = 0; iz < res; iz++)
				for (int ix = 0; ix < res; ix++) {
					const int c = (tz * 16 + nz * res + iz) * cols + tx * 16 + nx * res + ix;
					const s32 was = sea[c];
					const float off_was = held(c) ? off(c) : 0;
					sea[c] = own == NONE || floor[c] == NONE ? NONE
							: s32(std::lround(held::at(corners, (ix + 0.5f) / res, (iz + 0.5f) / res)));
					if (sea[c] == was)
						continue;
					changed = true;
					// its water moves with its level, keeping what's disturbed of it (first
					// found, none): a level found again as its neighbours come in, or a river
					// risen, is never a spill seen settling
					if (held(c))
						depth[c] = std::max(0.0f, std::max(sea[c], floor[c]) / 16.0f + (was == NONE ? 0 : off_was) - floor[c] / 16.0f);
				}
		}
	return changed;
}

} // namespace liquids
