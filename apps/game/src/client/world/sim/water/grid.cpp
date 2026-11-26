// Terraverse
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
	const v3s16 at(std::lround(camera.X), std::lround(camera.Y), std::lround(camera.Z));
	auto aligned = [](int v) { return int(std::floor(v / float(ALIGN))) * ALIGN; };
	// its height follows only a climb or fall past `follow` (its reach is by height)
	const s16 y = placed && std::abs(at.Y - origin.Y) <= follow ? origin.Y : at.Y;
	const v3s16 want_at(aligned(at.X - nodes / 2), y, aligned(at.Z - nodes / 2));
	if (placed && want_at == origin)
		return placing;
	const size_t columns = size_t(cols) * cols, cells = size_t(nodes) * nodes;
	Grid n(Opts{ /*res*/ res, /*nodes*/ nodes, /*follow*/ follow, /*scans*/ scans });
	n.floor.assign(columns, NONE);
	n.target.assign(columns, NONE);
	n.level.assign(columns, 0);
	n.eta.assign(columns, 0);
	n.out.assign(columns, {});
	n.vel.assign(columns, {});
	n.kind.assign(columns, Kind::None);
	n.shown.assign(columns, NONE);
	n.tops.assign(cells, std::nullopt);
	n.targets.assign(cells, NONE);
	n.kinds.assign(cells, Kind::None);
	n.flows.assign(cells, {});
	n.tile.assign(size_t(tiles) * tiles, Tile{});
	// it keeps all it has where it overlaps; a climb or fall looks at it all
	// again, keeping its water where it stands
	const bool climbed = placed && want_at.Y != origin.Y;
	const int p = per();
	for (int tz = 0; tz < tiles && placed; tz++)
		for (int tx = 0; tx < tiles; tx++) {
			const Tile &old = tile[tz * tiles + tx];
			const int mx = tx + (origin.X - want_at.X) / p, mz = tz + (origin.Z - want_at.Z) / p;
			if (mx < 0 || mx >= tiles || mz < 0 || mz >= tiles || !old.known) {
				// left behind: what it drew is drawn flat again, at once
				for (int nz = 0; nz < p; nz++)
					for (int nx = 0; nx < p; nx++)
						if (const auto &top = tops[(tz * p + nz) * nodes + tx * p + nx])
							placing.let_go.push_back(getNodeBlockPos(*top));
				continue;
			}
			Tile &t = n.tile[mz * tiles + mx];
			t = old;
			t.drawn = false; // its texels are written again where it now is
			for (int cz = 0; cz < 16; cz++)
				for (int cx = 0; cx < 16; cx++) {
					const int from = (tz * 16 + cz) * cols + tx * 16 + cx, to = (mz * 16 + cz) * cols + mx * 16 + cx;
					n.floor[to] = floor[from];
					n.target[to] = target[from];
					n.level[to] = level[from];
					n.eta[to] = eta[from];
					n.out[to] = out[from];
					n.vel[to] = vel[from];
					n.kind[to] = kind[from];
					n.shown[to] = shown[from];
				}
			for (int nz = 0; nz < p; nz++)
				for (int nx = 0; nx < p; nx++) {
					const int from = (tz * p + nz) * nodes + tx * p + nx, to = (mz * p + nz) * nodes + mx * p + nx;
					n.tops[to] = tops[from];
					n.targets[to] = targets[from];
					n.kinds[to] = kinds[from];
					n.flows[to] = flows[from];
				}
		}
	floor.swap(n.floor);
	target.swap(n.target);
	level.swap(n.level);
	eta.swap(n.eta);
	out.swap(n.out);
	vel.swap(n.vel);
	kind.swap(n.kind);
	shown.swap(n.shown);
	tops.swap(n.tops);
	targets.swap(n.targets);
	kinds.swap(n.kinds);
	flows.swap(n.flows);
	tile.swap(n.tile);
	origin = want_at;
	placed = true;
	if (climbed)
		for (Tile &t : tile)
			t.scanned = false;
	placing.moved = true; // (its tiles' texels are written again as they're drawn)
	// at its new edges, nodes it had beside its tiles are gone: their targets found again
	for (int t = 0; t < tiles * tiles; t++) {
		const int tx = t % tiles, tz = t / tiles;
		if (tx == 0 || tz == 0 || tx == tiles - 1 || tz == tiles - 1)
			tile[t].retarget = true;
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

} // namespace liquids
