// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/server/world/generate/place/terra_species.h"
#include "src/shared/base/exceptions.h"
#include "src/shared/game/content/nodedef.h"
#include <algorithm>

using namespace terragen;

namespace terra {

static double real(fix v)
{
	return (double)v / (double)ONE; // as core.terra.cover reports it
}

Spot spotAt(const Ground &g, const Cover &c)
{
	return { real(g.column.heat), real(g.column.wet), real(g.column.season), real(g.column.elevation),
		c.bank, c.roll };
}

static bool within(double v, const double range[2])
{
	return v >= range[0] && v <= range[1];
}

static bool fits(const Species &s, const Spot &spot, content_t ground)
{
	const Niche &n = s.niche;
	return within(spot.heat, n.heat) && within(spot.wet, n.wet) &&
			(!n.seasonal || within(spot.season, n.season)) &&
			(!n.bank || spot.bank) &&
			(!n.shore || spot.elevation < SHORE_ELEVATION || spot.bank) &&
			std::find(s.on.begin(), s.on.end(), ground) != s.on.end();
}

const Species *choose(const std::vector<Species> &species, const Spot &spot, content_t ground)
{
	const Species *fit[64];
	size_t count = 0;
	for (const Species &s : species) {
		if (!fits(s, spot, ground))
			continue;
		for (int k = 0; k < (s.niche.bank ? 2 : 1) && count < std::size(fit); k++)
			fit[count++] = &s;
	}
	return count ? fit[spot.roll % count] : nullptr;
}

treegen::TreeDef treeDef(const Species &s)
{
	treegen::TreeDef def;
	def.initial_axiom = s.shape.axiom;
	def.rules_a = s.shape.rules_a;
	def.rules_b = s.shape.rules_b;
	def.rules_c = s.shape.rules_c;
	def.rules_d = s.shape.rules_d;
	def.trunknode = MapNode(s.trunk);
	def.leavesnode = MapNode(s.leaves);
	def.leaves2node = MapNode(CONTENT_IGNORE);
	def.leaves2_chance = 0;
	def.angle = s.shape.angle;
	def.iterations = s.shape.iterations;
	def.iterations_random_level = s.shape.random_level;
	def.trunk_type = s.shape.trunk_type;
	def.thin_branches = s.shape.thin_branches;
	def.fruitnode = MapNode(CONTENT_IGNORE);
	def.fruit_chance = 0;
	def.seed = 0;
	def.explicit_seed = false; // seeded by position, as the world's trees are
	return def;
}

std::vector<Species> resolve(const std::vector<SpeciesDef> &defs, const NodeDefManager *ndef)
{
	auto id = [&](const std::string &species, const std::string &name) {
		content_t c = ndef->getId(name);
		if (c == CONTENT_IGNORE)
			throw BaseException("tree species '" + species + "': the game has no block '" + name + "'");
		return c;
	};
	std::vector<Species> out;
	for (const SpeciesDef &d : defs) {
		Species s{ d.name, d.niche, {}, id(d.name, d.trunk), id(d.name, d.leaves), d.shape };
		for (const std::string &ground : d.on)
			s.on.push_back(id(d.name, ground));
		out.push_back(std::move(s));
	}
	return out;
}

} // namespace terra
