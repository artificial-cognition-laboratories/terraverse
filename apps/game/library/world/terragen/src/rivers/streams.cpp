// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "library/world/terragen/src/rivers/streams.h"
#include "library/world/terragen/src/rivers/streams_detail.h"
#include "library/world/terragen/src/column/amplify.h"
#include "library/world/terragen/src/grid/climate.h"
#include "library/world/terragen/src/grid/drainage.h"
#include "library/world/terragen/src/grid/grid.h"
#include "library/world/terragen/src/world.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <cstdio>
#include <cstdlib>

namespace terragen {

using namespace streams;

namespace {

// Each Streams' own mark on what threads keep of it (never 0): an address is
// reused once one's freed, and worlds sharing a seed would share each other's
uint64_t nextOwner()
{
	static std::atomic<uint64_t> next{ 1 };
	return next++;
}

} // namespace

Streams::Streams(Opts o) :
	m_grid(o.grid), m_world(o.world), m_amplifier(o.amplifier), m_radius(o.radius), m_cell_blocks(o.cell_blocks),
	m_height(o.height), m_seed(o.seed), m_owner(nextOwner()), m_kept(std::make_unique<Kept>())
{
	m_spacing.push_back(m_cell_blocks >> FRAC);
	m_scale.push_back(pow(toFix(2), FALL));
	const int64_t finest = std::max<int64_t>(FINEST, (FINEST_M * o.metre) >> FRAC);
	for (int64_t l = m_spacing[0] / 2; l >= finest && (int)m_spacing.size() <= MOST_LEVELS; l /= 2) {
		m_spacing.push_back(l);
		m_scale.push_back(pow(ratio(l, m_spacing[0] / 2), FALL));
	}
	m_main.assign((size_t)m_grid.size(), -1);
	for (int32_t c = 0; c < m_grid.size(); c++) {
		const int32_t r = m_grid.receiver[c];
		if (r >= 0 && m_grid.base[c] >= 0 && (m_main[r] < 0 || m_grid.drained[c] > m_grid.drained[m_main[r]]))
			m_main[r] = c;
	}
	// the floors: the grid's water, a cell's valley depth under it (none at a
	// lake or the sea, never under the sea's level), never under what it drains
	// to, from the outlets up
	{
		const Grid &g = m_grid;
		const int64_t cell = m_cell_blocks;
		m_floor.assign((size_t)g.size(), 0);
		const auto height = [&](int32_t i) -> int64_t {
			return g.base[i] < 0 ? 0 : (int64_t)std::max<fix>(0, g.lake[i] ? std::min(g.eroded[i], g.filled[i]) : g.filled[i]) * m_height >> FRAC;
		};
		for (int32_t c : upstreamOrder(g.receiver)) {
			const int64_t top = height(c);
			const int64_t ks = std::max<fix>(0, m_amplifier.steepnesses()[(size_t)c]);
			const int64_t deep = g.base[c] < 0 || g.lake[c] ? 0 : mulShift(mulShift(mulShift(RELIEF, ks, FRAC), cell, FRAC), m_scale[0], FRAC);
			int64_t f = std::max<int64_t>(std::min<int64_t>(top, ONE), top - deep);
			const int32_t r = g.receiver[c];
			if (r >= 0)
				f = std::max(f, m_floor[(size_t)r]);
			m_floor[(size_t)c] = std::min(f, top);
		}
	}
	m_kept->runs.resize((size_t)m_grid.size());
	m_kept->ran = std::make_unique<std::once_flag[]>((size_t)m_grid.size());
	m_kept->rings.resize((size_t)m_grid.size());
	m_kept->ringed = std::make_unique<std::once_flag[]>((size_t)m_grid.size());
}

Streams::~Streams() = default;

Streams::Read Streams::read(const Place &p, int64_t above, int64_t least) const
{
	const V v = toV(p);
	int upto = 0;
	// (a valley narrower than a few of a far tile's cells only aliases on it)
	while (upto < (int)m_spacing.size() && (m_spacing[(size_t)upto] << FRAC) >= least * FAR_TILES)
		upto++;
	Read out;
	const int64_t top = above + mulShift(depth(here(v), 0), UP, FRAC);
	out.land = carve(v, top, upto, &out, least * 4 / 10);
	// the channel, cut into its valley's floor: water a freeboard under the
	// floor (and never over the land: where it crosses a valley cut deeper
	// than its own, a tributary over its trunk's floodplain, it runs down that
	// one's), its bed a depth under the water across the channel, and banks
	// sloping from the water's edge up to the land; the land's never raised
	if (out.level >= 0) {
		const int64_t w = out.width;
		const int64_t water = std::min(out.bed, out.land) - std::clamp<int64_t>(mulShift(w, FREEBOARD, FRAC), ONE / 2, 2 * ONE);
		const int64_t half = w / 2, bank = bankOf(w);
		if (out.from < half) {
			const int64_t deep = std::max<int64_t>(mulShift(w, DEEP, FRAC), ONE);
			const fix across = (fix)(((__int128)out.from << FRAC) / std::max<int64_t>(half, 1));
			out.land = std::min(out.land, water - mulShift(deep, ONE - mul(across, across), FRAC));
			out.bed = water;
		} else if (out.from < least * 4 / 10) {
			// (a far tile's: water drawn a cell wide, its land as it is)
			out.bed = std::min(water, out.land);
		} else {
			// (a bank: smooth, steepest just over the water)
			const fix t = (fix)std::clamp<int64_t>(((__int128)(out.from - half) << FRAC) / bank, 0, ONE);
			if (out.land > water)
				out.land = std::min(out.land, water + mulShift(out.land - water, mul(t, toFix(2) - t), FRAC));
			out.level = -1;
		}
	}
	return out;
}

int64_t Streams::carve(const V &p, int64_t above, int upto, Read *wet, int64_t within) const
{
	const Here h = here(p);
	// (smooth: read straight across a triangle the slope's the same all over
	// it, and a walk ran dead straight)
	if (above == FROM_GRID)
		above = ((int64_t)m_amplifier.smooth(m_grid.eroded, toPlace(p)) * m_height >> FRAC) + mulShift(depth(h, 0), UP, FRAC);
	int64_t land = above, lowest = INT64_MAX, widest = -1;
	for (int m = 0; m < upto; m++) {
		const int64_t S = m_spacing[(size_t)m] << FRAC;
		const int64_t was = land;
		near(p, h, m, [&](const Branch &b) {
			// (a valley reaches a lattice at most past its channel's edge)
			if (!b.within(p, S + b.widest / 2 + within))
				return;
			const Near n = nearest(b, p);
			// (in a channel's water before on any's bank; then the widest)
			const int64_t rank = n.width + (n.d < std::max(n.width / 2, within) ? INT64_MAX / 2 : 0);
			if (wet && n.width > 0 && !b.drawn && n.d < std::max(n.width / 2 + bankOf(n.width), within) && rank > widest) {
				widest = rank;
				wet->level = m;
				wet->bed = n.bed;
				wet->width = n.width;
				wet->from = n.d;
			}
			if (n.bed >= was)
				return;
			// its valley cut into the land the coarser left: a floor the channel
			// wide, sides t(2 − t) rising to that land, never past a hillside's
			// gradient (but within the lattice)
			const int64_t cut = was - n.bed;
			const int64_t R = std::clamp<int64_t>((int64_t)((__int128)cut * 2 * ONE / CRITICAL), mulShift(S, VALLEY, FRAC), S);
			const fix t = (fix)std::clamp<int64_t>(((int64_t)((__int128)std::max<int64_t>(0, n.d - n.width / 2) << FRAC) / R), 0, ONE);
			const int64_t at = n.bed + mulShift(cut, mul(t, toFix(2) - t), FRAC);
			if (at < land) {
				land = at;
				lowest = std::min(lowest, n.bed);
			}
		});
	}
	if (wet)
		wet->hillside = lowest == INT64_MAX || above <= lowest ? ONE
				: (fix)std::clamp<int64_t>((int64_t)((__int128)(land - lowest) * 2 * ONE / (above - lowest)) - ONE, -ONE, ONE);
	return land;
}

Streams::Here Streams::here(const V &p) const
{
	const Sphere::Spot s = gridSphere().spot(toPlace(p).dir());
	return { s.cells, s.weights };
}

fix Streams::Here::of(const std::vector<fix> &field) const
{
	int64_t sum = 0;
	for (int k = 0; k < 3; k++)
		sum += (int64_t)weights[k] * field[(size_t)cells[k]];
	return (fix)(sum >> FRAC);
}

int64_t Streams::depth(const Here &h, int level) const
{
	const int64_t ks = std::max<fix>(0, h.of(m_amplifier.steepnesses()));
	return mulShift(mulShift(mulShift(RELIEF, ks, FRAC), m_cell_blocks, FRAC), m_scale[(size_t)level], FRAC);
}

// what runs off here, of a cell's average rain: its rain, less what the land
// gives back to the air (the share of what passes through it that's still
// water, Grid::discharge of Grid::drained)
bool Streams::frozen(const Here &h) const
{
	return h.of(m_grid.heat) - mul(std::max<fix>(0, h.of(m_grid.eroded)), LAPSE) < ICE;
}

bool Streams::wet(int level, int64_t i, int64_t j, int64_t l, int64_t cells) const
{
	if (cells < WET)
		return false;
	int64_t keep = ONE;
	for (int k = THIN_FROM; k < level; k++)
		keep = mulShift(keep, KEEP, FRAC);
	const fix more = (fix)std::min<int64_t>(64 * ONE, (int64_t)(((__int128)cells << FRAC) / WET));
	keep = std::min<int64_t>(ONE, mulShift(keep, pow(more, WETTER), FRAC));
	const uint64_t h = mix(keyOf(i, j, l) ^ ((uint64_t)m_seed << 32) ^ (uint64_t)level * 0x9e3779b97f4a7c15ull);
	return (int64_t)(h & (ONE - 1)) < keep;
}

fix Streams::water(const Here &h) const
{
	int64_t sum = 0;
	for (int k = 0; k < 3; k++) {
		const int32_t c = h.cells[k];
		const int64_t drained = m_grid.drained[(size_t)c];
		const fix kept = drained > 0 ? (fix)std::min<int64_t>(ONE, (m_grid.discharge[(size_t)c] << FRAC) / drained) : 0;
		sum += (int64_t)h.weights[k] * mul(m_grid.rain(c), kept);
	}
	return (fix)(sum >> FRAC);
}

void Streams::near(const V &p, const Here &h, int level, const std::function<void(const Branch &)> &f) const
{
	if (level == 0) {
		// (the runs of the cells round the place's nearest, kept per cell: a
		// place is within a spacing of it, a run's valley a spacing of its line)
		const int32_t at = h.cells[0];
		std::call_once(m_kept->ringed[(size_t)at], [&] {
			const Sphere &sphere = gridSphere();
			const Place centre(sphere.centre(at));
			const int64_t reach = (int64_t)sphere.spacing() * 5 / 2 << (PLACE_FRAC - FRAC);
			std::vector<const Branch *> ring;
			for (int32_t c : sphere.around(at, [&](int32_t k) { return chord(Place(sphere.centre(k)), centre) <= reach; }))
				if (const std::shared_ptr<const Branch> &b = run(c))
					ring.push_back(b.get());
			m_kept->rings[(size_t)at] = std::move(ring);
		});
		for (const Branch *b : m_kept->rings[(size_t)at])
			f(*b);
		return;
	}
	// the branches through the place's cube and the cubes round it
	const int64_t S = m_spacing[(size_t)level] << SUB;
	// (a cube the ground doesn't pass has none: its middle within S of the
	// ground, by squares)
	const __int128 R = (__int128)m_radius << SUB, inner = (R - S) * (R - S), outer = (R + S) * (R + S);
	const auto onGround = [&](int64_t a, int64_t b, int64_t c) {
		const __int128 x = a * S + S / 2, y = b * S + S / 2, z = c * S + S / 2, r2 = x * x + y * y + z * z;
		return r2 >= inner && r2 <= outer;
	};
	const int64_t ci = floorDiv(p.x, S), cj = floorDiv(p.y, S), cl = floorDiv(p.z, S);
	std::vector<const Branch *> seen;
	std::shared_ptr<const Kept::Shard::Block> block;
	int64_t bi = INT64_MIN, bj = 0, bl = 0;
	for (int64_t i = ci - 1; i <= ci + 1; i++)
		for (int64_t j = cj - 1; j <= cj + 1; j++)
			for (int64_t l = cl - 1; l <= cl + 1; l++) {
				if (!onGround(i, j, l))
					continue;
				const int64_t ni = floorDiv(i, BLOCK), nj = floorDiv(j, BLOCK), nl = floorDiv(l, BLOCK);
				if (!block || ni != bi || nj != bj || nl != bl) {
					block = indexed(level, ni, nj, nl);
					bi = ni; bj = nj; bl = nl;
				}
				const auto it = block->find(keyOf(i, j, l));
				if (it == block->end())
					continue;
				for (const std::shared_ptr<const Branch> &b : it->second)
					if (std::find(seen.begin(), seen.end(), b.get()) == seen.end()) {
						seen.push_back(b.get());
						f(*b);
					}
			}
}

// A block of cubes' index: every edge that could reach the block,
// from the heads round it, filed under each cube of the block it passes
std::shared_ptr<const Streams::Kept::Shard::Block> Streams::indexed(int level, int64_t bi, int64_t bj, int64_t bl) const
{
	Kept::Shard &shard = m_kept->levels[(size_t)level][mix(keyOf(bi, bj, bl) ^ 0x5bd1e995u) % SHARDS];
	{
		std::lock_guard<std::mutex> hold(shard.lock);
		const auto it = shard.blocks.find(keyOf(bi, bj, bl));
		if (it != shard.blocks.end())
			return it->second;
	}
	const int64_t S = m_spacing[(size_t)level] << SUB;
	const __int128 R = (__int128)m_radius << SUB, inner = (R - S) * (R - S), outer = (R + S) * (R + S);
	const auto onGround = [&](int64_t a, int64_t b, int64_t c) {
		const __int128 x = a * S + S / 2, y = b * S + S / 2, z = c * S + S / 2, r2 = x * x + y * y + z * z;
		return r2 >= inner && r2 <= outer;
	};
	auto made = std::make_shared<Kept::Shard::Block>();
	const int64_t H = 2; // (an edge reaches its neighbour, and bends a little past)
	const int64_t i0 = bi * BLOCK, j0 = bj * BLOCK, l0 = bl * BLOCK;
	for (int64_t a = i0 - H; a < i0 + BLOCK + H; a++)
		for (int64_t b = j0 - H; b < j0 + BLOCK + H; b++)
			for (int64_t c = l0 - H; c < l0 + BLOCK + H; c++) {
				if (!onGround(a, b, c))
					continue;
				const std::shared_ptr<const Branch> br = grown(level, a, b, c);
				if (!br)
					continue;
				uint64_t last = 0;
				for (const V &q : br->pts) {
					const int64_t i = floorDiv(q.x, S), j = floorDiv(q.y, S), l = floorDiv(q.z, S);
					if (i < i0 || i >= i0 + BLOCK || j < j0 || j >= j0 + BLOCK || l < l0 || l >= l0 + BLOCK)
						continue;
					const uint64_t k = keyOf(i, j, l);
					if (k == last)
						continue;
					last = k;
					std::vector<std::shared_ptr<const Branch>> &in = (*made)[k];
					if (in.empty() || in.back() != br)
						in.push_back(br);
				}
			}
	std::lock_guard<std::mutex> hold(shard.lock);
	if (shard.blocks.size() >= SHARD_MOST / 8)
		shard.blocks.clear();
	shard.blocks.emplace(keyOf(bi, bj, bl), made);
	return made;
}

std::shared_ptr<const Streams::Branch> Streams::grown(int level, int64_t i, int64_t j, int64_t l) const
{
	Kept::Shard &shard = m_kept->levels[(size_t)level][mix(keyOf(i, j, l)) % SHARDS];
	{
		std::lock_guard<std::mutex> hold(shard.lock);
		const auto it = shard.heads.find(keyOf(i, j, l));
		if (it != shard.heads.end())
			return it->second;
	}
	// (built outside the lock: building asks for coarser branches; two
	// threads building the same build the same)
	std::shared_ptr<const Branch> made = edge(level, i, j, l);
	std::lock_guard<std::mutex> hold(shard.lock);
	if (shard.heads.size() >= SHARD_MOST)
		shard.heads.clear();
	shard.heads.emplace(keyOf(i, j, l), made);
	return made;
}

} // namespace terragen
