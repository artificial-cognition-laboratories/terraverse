// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "forest.h"
#include "grow.h"
#include <stdexcept>

namespace flora {

namespace {

const fix TREE_FROM = toFix(3);  // a lineage this tall where it does best stands at tree spots
const int32_t STAND = 40, DRIFT = 14; // blocks across a stand's patches; a ground plant's drifts
const fix STAND_FLOOR = R(25);   // where its stands are weakest it still scores this share
const fix JITTER = R(8);         // (ties break by the spot's roll)
const size_t KEPT = 4000;        // grown plants kept before they're all let go

uint32_t hash2(int32_t x, int32_t z, uint32_t seed) { return hash3(x, 0, z, seed); }

// Smooth 0..1 over blocks, `cell` across.
fix patches(int32_t x, int32_t z, uint32_t seed, int32_t cell)
{
	const auto floorDiv = [](int32_t a, int32_t b) { return a >= 0 ? a / b : -((-a + b - 1) / b); };
	const int32_t ix = floorDiv(x, cell), iz = floorDiv(z, cell);
	const fix tx = smooth(ratio(x - ix * cell, cell)), tz = smooth(ratio(z - iz * cell, cell));
	const auto at = [&](int32_t cx, int32_t cz) { return (fix)(hash2(cx, cz, seed) >> 16); };
	return lerp(lerp(at(ix, iz), at(ix + 1, iz), tx), lerp(at(ix, iz + 1), at(ix + 1, iz + 1), tx), tz);
}

/*
	A site, coarsely: each of what the id keeps, in so many steps over its
	range. Rounding back gives each step's middle; what's not kept is the
	usual (Site's defaults).
*/
struct Step
{
	fix Site::*field;
	fix lo, hi;
	uint32_t steps; // a power of two
};
const Step STEPS[] = {
	{ &Site::heat, -ONE, ONE, 16 },
	{ &Site::wet, -ONE, ONE, 16 },
	{ &Site::season, -ONE, ONE, 4 },
	{ &Site::fertility, 0, ONE, 4 },
	{ &Site::wind, 0, ONE, 4 },
	{ &Site::light, 0, ONE, 8 },
};
const uint32_t DEPTHS = 8; // blocks of soil, 0..7

uint32_t bitsOf(uint32_t steps) { return steps <= 1 ? 0 : 1 + bitsOf(steps / 2); }

} // namespace

Forest::Forest(uint32_t world_seed) : m_seed(world_seed)
{
	m_lineages = lineages(world_seed, (int)presets().size() * PER_PRESET);
	if (m_lineages.size() > 1u << (31 - LINEAGE_SHIFT))
		throw std::logic_error("flora: more lineages than a plant's id holds");
	for (size_t i = 0; i < m_lineages.size(); i++) {
		const Lineage &l = m_lineages[i];
		if (l.cover.on)
			continue; // (covers are the ground's, not a plant's)
		(l.genes[(int)Gene::Height] >= TREE_FROM ? m_trees : m_ground).push_back((int)i);
	}
}

int Forest::choose(Layer layer, const Site &site, int32_t x, int32_t z, uint32_t roll) const
{
	int best = -1;
	fix score = 0;
	const int32_t cell = layer == Layer::Trees ? STAND : DRIFT;
	for (const int i : layer == Layer::Trees ? m_trees : m_ground) {
		const Lineage &l = m_lineages[i];
		const Traits t = traits(l, site);
		if (!t.lives)
			continue;
		// how near its full height it grows here, and how strongly its stands hold the spot
		const fix fit = div(t.gene(Gene::Height), l.genes[(int)Gene::Height]);
		const fix stand = lerp(STAND_FLOOR, ONE, patches(x, z, l.seed, cell));
		const fix s = mul(fit, stand) + mul(JITTER, (fix)(hash2(i, 0, roll) >> 16));
		if (s > score) {
			score = s;
			best = i;
		}
	}
	return best;
}

uint32_t Forest::idOf(int lineage, const Site &site, uint32_t roll)
{
	uint32_t id = 0;
	for (const Step &st : STEPS) {
		const fix v = clamp(site.*st.field, st.lo, st.hi);
		const uint32_t step = std::min<uint32_t>(st.steps - 1, (uint32_t)((int64_t)(v - st.lo) * st.steps / (st.hi - st.lo)));
		id = (id << bitsOf(st.steps)) | step;
	}
	id = (id << bitsOf(DEPTHS)) | (uint32_t)std::clamp<int32_t>(site.depth, 0, DEPTHS - 1);
	id = (id << 1) | site.bank;
	return ((uint32_t)lineage << LINEAGE_SHIFT) | (id << 3) | (roll % INDIVIDUALS);
}

Site Forest::siteOf(uint32_t id)
{
	Site site;
	uint32_t at = (id & ((1u << LINEAGE_SHIFT) - 1)) >> 3;
	site.bank = at & 1;
	at >>= 1;
	site.depth = (int32_t)(at & (DEPTHS - 1));
	at >>= bitsOf(DEPTHS);
	for (int i = (int)(sizeof(STEPS) / sizeof(STEPS[0])) - 1; i >= 0; i--) {
		const Step &st = STEPS[i];
		const uint32_t step = at & (st.steps - 1);
		at >>= bitsOf(st.steps);
		site.*st.field = st.lo + (fix)((int64_t)(st.hi - st.lo) * (2 * step + 1) / (2 * st.steps));
	}
	return site;
}

Traits Forest::traitsOf(uint32_t id) const
{
	return traits(m_lineages.at(lineageOf(id)), siteOf(id));
}

std::shared_ptr<const Plant> Forest::plant(uint32_t id)
{
	{
		std::lock_guard<std::mutex> lock(m_lock);
		const auto found = m_grown.find(id);
		if (found != m_grown.end())
			return found->second;
	}
	// (grown outside the lock: two threads may grow the same one, and keep the same)
	const uint32_t individual = id & 7;
	auto grown = std::make_shared<const Plant>(grow(traitsOf(id), m_lineages.at(lineageOf(id)).seed + individual * 7919u));
	std::lock_guard<std::mutex> lock(m_lock);
	if (m_grown.size() >= KEPT)
		m_grown.clear();
	return m_grown.emplace(id, grown).first->second;
}

std::shared_ptr<Forest> forestFor(uint32_t world_seed)
{
	static std::mutex lock;
	static std::map<uint32_t, std::weak_ptr<Forest>> made;
	std::lock_guard<std::mutex> hold(lock);
	if (std::shared_ptr<Forest> kept = made[world_seed].lock())
		return kept;
	auto fresh = std::make_shared<Forest>(world_seed);
	made[world_seed] = fresh;
	return fresh;
}

} // namespace flora
