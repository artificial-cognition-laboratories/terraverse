// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "flora.h"
#include <map>
#include <memory>
#include <mutex>

/*
	A world's flora as the generator plants it (sprint 113): its lineages,
	which of them stands at a spot, and the plants they grow there.

	Which: of the lineages that live at a site, each scored by how well it
	does there (how near its full height the site lets it grow) times how
	strongly its stands hold that ground: a field of patches, its own, a few
	dozen blocks across. So the fittest win, and where several fit, each
	keeps its groves (a beech wood, then oaks) instead of every tree being
	a different one.

	Grown plants are kept: the same lineage at much the same site grows one
	of a few individuals, each placed turned and mirrored as its spot rolls.
	Thread-safe (the generator's threads share it).
*/
namespace flora {

class Forest
{
public:
	explicit Forest(uint32_t world_seed);

	const std::vector<Lineage> &all() const { return m_lineages; }
	// What stands where the generator stands trees, or grows under them: tall lineages, and the rest.
	enum class Layer { Trees, Ground };
	// The lineage of that layer at a spot (block x, z on its land; its roll), or −1: none lives there.
	int choose(Layer layer, const Site &site, int32_t x, int32_t z, uint32_t roll) const;
	/*
		A plant's id: its lineage, its site rounded (plants for sites that
		round the same are the same plants, grown from the rounded site, so
		they never depend on which came first) and which of a few
		individuals it is, by the roll. 31 bits: what the world stores of a
		small plant, and the client grows it from (flora::Sprig).
	*/
	static uint32_t idOf(int lineage, const Site &site, uint32_t roll);
	static int lineageOf(uint32_t id) { return (int)(id >> LINEAGE_SHIFT); }
	static Site siteOf(uint32_t id);
	std::shared_ptr<const Plant> plant(uint32_t id);
	Traits traitsOf(uint32_t id) const;
	static constexpr int PER_PRESET = 2; // lineages a world has of each preset
	static constexpr uint32_t INDIVIDUALS = 6;
	static constexpr int LINEAGE_SHIFT = 24; // (the site's 21 bits and the individual's 3 under it)

private:
	uint32_t m_seed;
	std::vector<Lineage> m_lineages;
	std::vector<int> m_trees, m_ground; // by Layer
	std::mutex m_lock;
	std::map<uint32_t, std::shared_ptr<const Plant>> m_grown; // by id
};

// The forest of the world with that seed: one for every generator thread and the far view.
std::shared_ptr<Forest> forestFor(uint32_t world_seed);

} // namespace flora
