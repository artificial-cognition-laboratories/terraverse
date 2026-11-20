// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/shared/base/irrlichttypes.h"
#include "src/shared/map/block/mapnode.h"
#include "src/server/world/generate/luanti/treegen.h"
#include "library/world/terragen/src/world.h"
#include <string>
#include <vector>

class NodeDefManager;

/*
	Tree species: which tree grows where. terragen says where a tree stands
	(World::cover); a species' niche says which one. The game registers
	species as data (core.terra.register_species); the generator's planting
	and the far view both choose with choose(), so the horizon grows the trees
	the world does.
*/
namespace terra {

// The climate a species takes, in core.terra.cover's units.
struct Niche
{
	double heat[2], wet[2];
	bool seasonal;     // only where the rain's season falls in `season`
	double season[2];
	bool bank;         // only beside rivers and lakes (and chosen twice as often there)
	bool shore;        // only at the coast or an oasis: low ground, or a bank
};

// An L-system shape, as core.spawn_tree takes it.
struct Shape
{
	std::string axiom, rules_a, rules_b, rules_c, rules_d, trunk_type;
	s32 angle, iterations, random_level;
	bool thin_branches;
};

// A species as the game registers it: blocks by name.
struct SpeciesDef
{
	std::string name;
	Niche niche;
	std::vector<std::string> on; // the ground blocks it grows on
	std::string trunk, leaves;
	Shape shape;
};

// Resolved: blocks by content id.
struct Species
{
	std::string name;
	Niche niche;
	std::vector<content_t> on;
	content_t trunk, leaves;
	Shape shape;
};

// What a column offers a tree.
struct Spot
{
	double heat, wet, season, elevation;
	bool bank;
	u32 roll;
};

// The coast: barely above the sea (terragen elevation; 0.03 ≈ 5 blocks).
constexpr double SHORE_ELEVATION = 0.03;

Spot spotAt(const terragen::Ground &ground, const terragen::Cover &cover);

// Of the species that fit, the one the spot's roll picks (banks count twice
// for bank species); nullptr if none fits.
const Species *choose(const std::vector<Species> &species, const Spot &spot, content_t ground);

// The tree a species grows, ready for treegen.
treegen::TreeDef treeDef(const Species &s);

// Throws BaseException naming any block the game doesn't have.
std::vector<Species> resolve(const std::vector<SpeciesDef> &defs, const NodeDefManager *ndef);

} // namespace terra
