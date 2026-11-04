// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "physics/structure.h"
#include "physics/world.h"
#include "piece/machine.h"
#include <optional>
#include <vector>

class IGameDef;

namespace workshop {

class Materials;

/*
	A machine as physics::Structure sees it: a quarter of a node to a cell,
	each knowing which of its sixteenths are filled, so a thin beam's joints have
	a thin beam's cross-section. Only a thing of one part with nothing fitted is
	judged for now: a car's frame takes its load through its wheels and springs,
	which this doesn't model yet.
*/
struct StructureOf
{
	physics::Structure structure;
	std::vector<v3s32> cells;    // each cell's place, in quarters of a node
	std::vector<aabb3f> boxes;   // each cell's box (nodes, the machine's frame)
};
// Nothing if it's not to be judged: more than one part or any fitting, nothing
// that can fail, or more than MOST_CELLS (it stays as it's built: too big to ask).
constexpr size_t MOST_CELLS = 20000;
// A blow under this (m/s, of velocity or rad/s of spin) isn't asked about.
constexpr float LEAST_BLOW = 1.5f;
// How long a blow takes to stop something (s): the load is what it lost over this.
// Softer ground would take longer; one for everything for now, tuned by eye.
constexpr float BLOW_TIME = 0.15f;
std::optional<StructureOf> structureOf(const piece::Machine &m, const Materials &materials, IGameDef *gamedef);

// A machine's one part cut as a result says: one machine per group, each on the
// same grid at the same pose, so nothing moves. In the result's order.
// Each's `by` is where its grid's corner now is from the old one (nodes, the machine's
// frame): a piece's grid starts at the piece, as a body far from its entity's position
// swings wide between what the client's told and is seen to shake.
struct Cut
{
	piece::Machine machine;
	v3f by;
};
std::vector<Cut> cut(const piece::Machine &m, const StructureOf &s, const physics::StructureResult &r, IGameDef *gamedef);

// A question put to the solver: pure, so it can go off the server's thread.
struct StructureQuestion
{
	StructureOf of;
	std::vector<bool> resting;
	physics::StructureLoad load;
};
// What a blast (`centre`: nodes, world; `power`: Pa·s a node off) does to it where it
// lies: the question, and the impulse it's flung by (N·s, world).
struct Blasted
{
	StructureQuestion question;
	v3f impulse;
};
constexpr float BLAST_REACH = 16; // nodes past which nothing's worth asking
std::optional<Blasted> blasted(const piece::Machine &m, const Materials &materials, IGameDef *gamedef,
		const physics::World &world, physics::World::Id id, v3f centre, float power);
// Whether it's a question at all, where it lies in `world` (`id`): at rest, or
// (`blow`) as it's just been hit.
std::optional<StructureQuestion> asked(const piece::Machine &m, const Materials &materials, IGameDef *gamedef,
		const physics::World &world, physics::World::Id id, const std::optional<physics::World::Blow> &blow = std::nullopt);

} // namespace workshop
