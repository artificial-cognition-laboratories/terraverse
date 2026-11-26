// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "library/matter/sandbox/world.h"
#include <algorithm>
#include <cmath>
#include <unordered_map>

/*
	Gas with momentum (sprint 117; planning/gas-and-fire.md, phase 1).

	Each face of an awake block carries a wind, kept from step to step, as
	liquids' pipes are. It speeds up with the difference in pressure across
	the face and with buoyancy (hot gas under cool rises, heavy gas sinks),
	and drag slows it. Gas moves with it, carrying its heat.

	The atmosphere isn't simulated. Asleep air is a reservoir at the
	weather's temperature and the body's pressure: a face onto it pushes
	against that air, gas leaving into it is gone, and what's drawn in is
	the room's air. Gas leaving that differs from the room (smoke, a hot
	plume) wakes the air it flows into, so a plume is as tall as it stays
	warm and no taller.

	A solid's pores and a vacuum keep the older exchange (balance): no wind.
*/
namespace sandbox {

using matter::State;

static const double GAS_STEP = 0.04;  // s: the longest step a wind takes (pressure waves cross a block in a tenth of a second)
static const double MOST = 0.2;       // the most of a block's gas one face carries off in a step
static const double CARRIED = 0.3;    // m/s: a wind slower than this wakes nothing (a block's pressure easing into the room just goes)
static const double WARMER = 15;      // K: air this far from the room's, blown out, wakes where it goes
static const double SMOKY = 0.03;     // share of its gas not the room's that makes gas worth following

static const double ENTRAINING = 0.1; // of its speed: room air a moving column draws in through each open side (a plume's α)

static const Pos FACES[6] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };

bool WorldMatter::windy(const Cell &c) const
{
	return !sky(c).vacuum && !c.dense;
}

void WorldMatter::winds(const std::vector<int64_t> &ids, double dt)
{
	const int steps = std::max(1, (int)std::ceil(dt / GAS_STEP - 1e-9));
	const double h = dt / steps;

	// What each face leads to is worked out once a step; the substeps are
	// arithmetic. Blocks awake but not stepped this time (past the budget)
	// still trade gas with those that are.
	std::vector<Cell *> cells;
	std::unordered_map<const Cell *, uint32_t> index;
	auto place = [&](Cell &c) {
		auto [it, added] = index.emplace(&c, (uint32_t)cells.size());
		if (added)
			cells.push_back(&c);
		return it->second;
	};
	struct Face
	{
		uint32_t a;     // the block it's stepped from
		int d;          // which of its sides
		int32_t b;      // the block beyond, −1: the room
		Pos room;       // where the room is
		double ambient, room_air; // the room's temperature and its air's density
		bool along;     // a room face across a column's way (it entrains)
	};
	std::vector<Face> faces;
	for (int64_t id : ids) {
		auto it = m_cells.find(id);
		if (it == m_cells.end())
			continue;
		Cell &c = it->second;
		if (!windy(c)) {
			std::fill(std::begin(c.wind), std::end(c.wind), 0.0);
			continue;
		}
		const uint32_t a = place(c);
		const Sky &air = sky(c);
		for (int d = 0; d < 6; d++) {
			const Pos n{ c.pos.x + FACES[d].x, c.pos.y + FACES[d].y, c.pos.z + FACES[d].z };
			auto other = m_cells.find(key(n));
			if (other != m_cells.end()) {
				Cell &o = other->second;
				if (!windy(o) || o.sky != c.sky) {
					c.wind[d] = 0;
					continue;
				}
				// a face two windy blocks share is stepped once: from the lower one,
				// unless that one isn't being stepped
				const bool theirs = (d & 1) && std::binary_search(ids.begin(), ids.end(), other->first);
				if (!theirs)
					faces.push_back({ a, d, (int32_t)place(o), n, 0, 0, false });
				continue;
			}
			const int32_t kind = m_ground.kind(n);
			if (kind < 0 || !m_kinds[kind].air || skyAt(n) != c.sky) {
				c.wind[d] = 0; // a wall
				continue;
			}
			const double ambient = m_ground.ambient(n);
			faces.push_back({ a, d, -1, n, ambient, air.physics.airDensity(air.air, ambient), true });
		}
	}
	if (faces.empty())
		return;

	struct Flow
	{
		uint32_t from;   // the giver: a block, or (from the room) the receiver
		int32_t to;      // a block, −1: the room
		size_t face;
		double share;    // of the giver's gas at the start of the substep (the room's: of a block of it)
		bool in;         // the room's air drawn into `from`
	};
	// what's solid and liquid doesn't move here: each block's free space, once
	std::vector<double> free;
	auto freeOf = [&](size_t i) { return sky(*cells[i]).physics.volume() - sky(*cells[i]).physics.condensed(cells[i]->mix); };
	for (size_t i = 0; i < cells.size(); i++)
		free.push_back(freeOf(i));
	std::vector<double> excess(cells.size()), density(cells.size());
	// heat capacities, kept as gas comes and goes (J/K)
	std::vector<double> capacity;
	for (Cell *c : cells)
		capacity.push_back(c->mix.heatCapacity());
	std::vector<std::vector<double>> gas(cells.size());
	std::vector<double> giving(cells.size());
	std::vector<Flow> flows;
	std::vector<char> woke(faces.size(), 0);
	for (int step = 0; step < steps; step++) {
		// each block as it is at the start of the substep: what moves is worked
		// out from that, so the order blocks are visited in doesn't matter
		for (size_t i = 0; i < cells.size(); i++) {
			const Cell &c = *cells[i];
			const Physics &physics = sky(c).physics;
			gas[i].assign(c.mix.amounts.size(), 0);
			double moles = 0, kg = 0;
			for (size_t k = 0; k < c.mix.amounts.size(); k++)
				if (c.mix.amounts[k] > 0) {
					gas[i][k] = c.mix.in(k, State::Gas);
					moles += gas[i][k];
					kg += gas[i][k] * m_matter.substances()[k].molar_mass / 1000.0;
				}
			// as Physics::excess and gasDensity, from the free space worked out once
			const double holds = free[i] > 0 ? physics.pressure() * free[i] / (matter::GAS_CONSTANT * c.mix.temperature) : 0;
			excess[i] = holds > 0 ? moles / holds - 1 : 0;
			density[i] = free[i] > 0 ? kg / free[i] : 0;
		}
		flows.clear();
		for (size_t f = 0; f < faces.size(); f++) {
			const Face &face = faces[f];
			Cell &c = *cells[face.a];
			const Sky &air = sky(c);
			const int up = face.d == 2 ? 1 : face.d == 3 ? -1 : 0;
			const double eb = face.b >= 0 ? excess[face.b] : 0, rb = face.b >= 0 ? density[face.b] : face.room_air;
			const double u = air.physics.drive(c.wind[face.d], excess[face.a], eb, density[face.a], rb, up, air.gravity, h);
			c.wind[face.d] = u;
			if (face.b >= 0)
				cells[face.b]->wind[face.d ^ 1] = -u;
			const double share = std::min(MOST, std::fabs(u) * h / air.physics.block());
			if (share > 0) {
				if (face.b >= 0)
					flows.push_back(u > 0 ? Flow{ face.a, face.b, f, share, false } : Flow{ (uint32_t)face.b, (int32_t)face.a, f, share, false });
				else
					flows.push_back(u > 0 ? Flow{ face.a, -1, f, share, false } : Flow{ face.a, -1, f, share, true });
			}
			// a column moving past open air draws it in and loses as much: why a
			// plume widens, cools and fades a few metres up instead of rising forever
			if (face.b < 0) {
				const double along = up ? std::fabs(c.wind[0]) + std::fabs(c.wind[1]) + std::fabs(c.wind[4]) + std::fabs(c.wind[5])
							: std::fabs(c.wind[2]) + std::fabs(c.wind[3]);
				const double entrained = std::min(MOST, ENTRAINING * along / 2 * h / air.physics.block());
				if (entrained > 0) {
					flows.push_back(Flow{ face.a, -1, f, entrained, false });
					flows.push_back(Flow{ face.a, -1, f, entrained, true });
				}
			}
		}

		// no block gives more than half its gas in a substep, whatever its faces ask
		std::fill(giving.begin(), giving.end(), 0.0);
		for (const Flow &f : flows)
			if (!f.in)
				giving[f.from] += f.share;
		for (Flow &f : flows)
			if (!f.in && giving[f.from] > 0.5)
				f.share *= 0.5 / giving[f.from];

		for (const Flow &f : flows) {
			Cell &from = *cells[f.from];
			const Face &face = faces[f.face];
			const Physics &physics = sky(from).physics;
			if (f.in) {
				// the room's air drawn in: a block of it, at the weather's temperature
				const double block = physics.pressure() * physics.volume() / (matter::GAS_CONSTANT * face.ambient);
				physics.admit(from.mix, sky(from).air, block * f.share, face.ambient, &capacity[f.from]);
				continue;
			}
			Cell *into = f.to >= 0 ? cells[f.to] : nullptr;
			if (!into && f.share > 0 && from.wind[face.d] > CARRIED && !woke[f.face]) {
				// out into the room, followed along its buoyancy (light gas up, heavy
				// gas down) where it's smoke or warm air: it wakes the air it flows
				// into, so a plume is as tall as it stays warm. A block's pressure
				// easing into the open just disperses there. (Fuel gas pushed out
				// sideways goes unburnt: a flame carried sideways is a flame front,
				// planning/gas-and-fire.md phase 2)
				const int up = face.d == 2 ? 1 : face.d == 3 ? -1 : 0;
				const bool buoyant = (up > 0 && density[f.from] < face.room_air) || (up < 0 && density[f.from] > face.room_air);
				if (buoyant && (unlike(from.mix, sky(from)) > SMOKY || std::fabs(from.mix.temperature - face.ambient) > WARMER)) {
					into = wake(face.room, face.ambient);
					woke[f.face] = into != nullptr;
					if (into) {
						place(*into); // (a block, not stepped: its gas is taken next step)
						while (capacity.size() < cells.size())
							capacity.push_back(cells[capacity.size()]->mix.heatCapacity());
					}
				}
			} else if (!into && woke[f.face]) {
				into = &m_cells.at(key(face.room));
			}
			const auto there = into ? index.find(into) : index.end();
			physics.carry(from.mix, into ? &into->mix : nullptr, gas[f.from], f.share, &capacity[f.from],
					there != index.end() ? &capacity[there->second] : nullptr);
		}
		// blocks woken this substep join the arrays (their state is read next substep)
		while (free.size() < cells.size())
			free.push_back(freeOf(free.size()));
		excess.resize(cells.size());
		density.resize(cells.size());
		gas.resize(cells.size());
		giving.resize(cells.size());
	}
}

} // namespace sandbox
