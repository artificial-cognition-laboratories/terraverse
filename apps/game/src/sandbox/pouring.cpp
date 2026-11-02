// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "world.h"
#include <algorithm>
#include <array>
#include <cmath>

/*
	Liquids and powders in the world's blocks (sprint 80). A block holds an
	amount, and the world draws its level in sixty-fourths; nothing here is
	a voxel.

	- Liquids fall into space below, then run sideways by "virtual pipes"
	  (Mei, Decaudin & Hu 2007): each side has a flow that speeds up with
	  the difference in surface height and is kept from step to step, so
	  water has momentum, sloshes a little and settles. A liquid spreads no
	  thinner than a sixteenth of a block (a puddle).
	- Powders (anything poured, or a loose kind) fall too, and slide only
	  where their heap is steeper than its angle of repose; denser than the
	  liquid under them, they sink through it.
	- A liquid's open surface gives off vapour to the air over it, by how far
	  its vapour pressure is above what's there: the block loses it (and the
	  heat it took), and the air carries it off, or burns it.

	A still pool is asleep: it costs nothing until something changes beside
	it, and a difference under a sixty-fourth (less than is drawn) doesn't
	wake its neighbours, so a cup into the sea stays where it's poured.
*/
namespace sandbox {

using matter::State;

static const double SUBSTEP = 0.1;          // s: the longest step liquids take
static const double THINNEST = 1.0 / 16;    // of a block: a liquid spreads no thinner
static const double FALLING = 8;            // per s: how fast what's over space falls into it
static const double DAMPING = 2;            // per s: a pipe's flow lost to viscosity (an arcade's: it settles)
static const double LEVEL = 1.0 / 128;      // of a block: a difference smaller than drawn wakes nothing asleep
static const double REPOSE = 0.7;           // a heap's steepest, rise over run (about 35°)
static const double SLIDING = 4;            // per s: how fast a heap too steep slides
static const double EVAPORATION = 0.003;    // m/s: vapour carried off a surface (still air's mass transfer)
static const double TRACE = 1e-7;           // m³ (a tenth of a millilitre): less doesn't run, it evaporates
static const double ROOM_AGAIN = 5;          // s between looking again at an awake block's open space
static const double MIST = 1e-4;            // m³ (a tenth of a litre): less, in the air, hangs as mist
static const double VAPOUR = 0.02;          // mol/s of vapour that wakes the air it rises into (a warm spirit's; a pond's goes to the room)

static const Pos *sides()
{
	static const Pos s[4] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };
	return s;
}

static Pos at(Pos p, int dx, int dy, int dz)
{
	return { p.x + dx, p.y + dy, p.z + dz };
}

void WorldMatter::volumes(Cell &c) const
{
	c.liquid = c.solid = 0;
	const double p = sky(c).physics.pressure();
	for (size_t s = 0; s < c.mix.amounts.size(); s++) {
		if (c.mix.amounts[s] <= 0)
			continue;
		const matter::Substance &sub = m_matter.substances()[s];
		const double kg = sub.molar_mass / 1000.0;
		c.liquid += c.mix.in(s, State::Liquid) * kg / sub.density(State::Liquid, c.mix.temperature, p);
		c.solid += c.mix.in(s, State::Solid) * kg / sub.density(State::Solid, c.mix.temperature, p);
	}
	// its open space (sprint 81): a sculpted block's empty sixteenths, looked
	// up now and then (a block's sculpted rarely; disturb() asks again)
	if (c.room_at < 0 || m_clock - c.room_at > ROOM_AGAIN) {
		c.room = m_ground.room(c.pos);
		c.open = sky(c).physics.volume() * c.room.total() / 4096.0;
		c.room_at = m_clock;
	}
}

// What of a block's open space is taken: its liquid, and solids that lie
// in it (a heap, ash in the air); a block's own (sand cut to a pit) aren't.
double WorldMatter::taken(const Cell &c) const
{
	const Kind &k = m_kinds[c.kind];
	return c.liquid + (c.loose || k.air ? c.solid : 0);
}

// Where a liquid's surface stands over a block, in blocks (its floor is
// its y): what fills its open space, from the bottom of its empty
// sixteenths, and, full, the surface of what's on top of it. NAN where
// liquid can't go (rock, a wall, a vessel's side).
double WorldMatter::surface(Pos p, int depth) const
{
	Room room;
	double share;
	auto it = m_cells.find(key(p));
	if (it != m_cells.end()) {
		const Cell &c = it->second;
		const Kind &k = m_kinds[c.kind];
		if (k.flowing || k.vessel || c.open <= 0)
			return NAN;
		room = c.room;
		share = taken(c) / c.open;
	} else {
		const int32_t kind = m_ground.kind(p);
		if (kind < 0)
			return NAN;
		const Kind &k = m_kinds[kind];
		if (k.vessel || k.flowing)
			return NAN;
		room = m_ground.room(p);
		if (room.total() == 0)
			return NAN;
		if (k.leveled)
			share = k.fill * m_ground.level(p);
		else if (!k.air && k.state == State::Liquid)
			share = 1; // a block of liquid (the sea's)
		else
			share = m_ground.fill(p).share;
	}
	const double top = p.y + room.surface(share);
	if (share < FULL || depth >= 32)
		return top;
	// full: the surface is the top of the column of liquid standing on it
	const double above = surface(at(p, 0, 1, 0), depth + 1);
	return std::isnan(above) || above <= p.y + 1 ? top : above;
}

void WorldMatter::flow(const std::vector<int64_t> &ids, double dt)
{
	const double v = base().volume();
	std::vector<int64_t> live = ids;
	for (int64_t id : live) {
		Cell &c = m_cells.at(id);
		c.moved = 0;
		// a still solid's (its chemistry not run: nothing in it changed) are as they were
		if (!(c.dense && c.mix.still() && c.room_at >= 0))
			volumes(c);
	}
	// what runs: liquid anywhere but inside a solid block (water in wet clay
	// stays in the clay), and a powder's solids
	auto runs = [&](const Cell &c) {
		const Kind &k = m_kinds[c.kind];
		return !k.flowing && c.open > 0 && (c.loose || k.air || k.state == State::Liquid || c.room.total() < 4096);
	};
	// a block matter can run into from a side or from above, woken if asleep:
	// nullptr where it can't (a wall, too much awake) or, asleep, where the
	// difference is less than drawn (it'd wake the sea for a cup)
	auto into = [&](Pos p, bool from_above, double difference) -> Cell * {
		auto it = m_cells.find(key(p));
		if (it != m_cells.end()) {
			const Kind &k = m_kinds[it->second.kind];
			if (k.flowing || (k.vessel && !from_above) || (it->second.open <= 0 && !k.vessel))
				return nullptr;
			return &it->second;
		}
		const int32_t kind = m_ground.kind(p);
		if (kind < 0)
			return nullptr;
		const Kind &k = m_kinds[kind];
		if (k.flowing || (k.vessel && !from_above))
			return nullptr;
		if (!k.vessel && m_ground.room(p).total() == 0)
			return nullptr; // solid
		// asleep with liquid in it already (the sea, a pool): not for less than is drawn
		const bool wet = (!k.air && k.state == State::Liquid) || k.leveled || m_ground.fill(p).substance >= 0;
		if (wet && difference < LEVEL)
			return nullptr;
		const size_t before = m_cells.size();
		Cell *n = wake(p, m_ground.ambient(p));
		if (n && m_cells.size() > before) {
			volumes(*n);
			live.push_back(key(p));
		}
		return n;
	};
	// whether a block holds liquid already (a dry one is run into no thinner than a puddle)
	auto wet = [&](Pos p) {
		auto it = m_cells.find(key(p));
		if (it != m_cells.end())
			return it->second.liquid >= TRACE;
		const int32_t kind = m_ground.kind(p);
		if (kind < 0)
			return false;
		const Kind &k = m_kinds[kind];
		return (!k.air && k.state == State::Liquid) || k.leveled || m_ground.fill(p).substance >= 0;
	};
	// a share of a block's liquid (or its solids) moved to another, carrying its heat
	auto shift = [&](Cell &from, Cell &to, State state, double volume) {
		double &have = state == State::Liquid ? from.liquid : from.solid;
		if (volume <= 0 || have <= 0)
			return;
		const double share = std::min(1.0, volume / have);
		for (size_t s = 0; s < from.mix.amounts.size(); s++)
			if (from.mix.amounts[s] > 0) {
				const double n = from.mix.in(s, state) * share;
				if (n > 0)
					base().move(from.mix, to.mix, s, state, n);
			}
		const double moved = have * share;
		have -= moved;
		(state == State::Liquid ? to.liquid : to.solid) += moved;
		from.moved += moved;
		to.moved += moved;
		if (state == State::Solid)
			to.loose = true; // poured onto it: a heap
		if (moved > TRACE)
			to.quiet = 0; // (rounding's trickle between level neighbours doesn't keep them up)
	};

	for (double left = dt; left > 1e-9; left -= SUBSTEP) {
		const double h = std::min(left, SUBSTEP);
		// falling first: into what's below while it has room
		for (size_t i = 0; i < live.size(); i++) {
			auto found = m_cells.find(live[i]);
			if (found == m_cells.end())
				continue;
			Cell &c = found->second;
			// liquid falls into space below, but not mist (vapour condensing in the air: it hangs, and evaporates)
			// (sprint 81: from open space only; a solid's pores keep their water, and a block
			// that melts becomes the open space its liquid lies in first)
			if (m_kinds[c.kind].flowing || c.open <= 0 || (c.liquid < MIST && (!c.loose || c.solid < TRACE)))
				continue;
			Cell *below = into(at(c.pos, 0, -1, 0), true, 1);
			// through its floor, into the top of what's below (a sculpted block open at the top)
			if (!below || (c.open > 0 && !c.room.layers[0]) || (below->open > 0 && !below->room.layers[15]))
				continue;
			const double room = below->open - taken(*below);
			const double rate = std::min(1.0, FALLING * h);
			if (c.loose && c.solid > 0) {
				const double fall = std::min(c.solid, std::max(0.0, room)) * rate;
				shift(c, *below, State::Solid, fall);
				// a powder denser than the liquid under it sinks, the liquid rising past it
				if (below->liquid > 0 && c.solid > 0 && below->solid < below->open * FULL) {
					const double sink = std::min({ c.solid, below->liquid, below->open - below->solid }) * rate * 0.5;
					if (density(c, State::Solid) > density(*below, State::Liquid)) {
						shift(c, *below, State::Solid, sink);
						shift(*below, c, State::Liquid, sink);
					}
				}
			}
			const double space = below->open - taken(*below);
			if (c.liquid > 0 && space > 0)
				shift(c, *below, State::Liquid, std::min(c.liquid, space) * rate);
		}
		// what's more than a block holds rises into what's over it (sprint 82: a
		// pit poured past full overflows, and runs off from there)
		for (size_t i = 0; i < live.size(); i++) {
			auto found = m_cells.find(live[i]);
			if (found == m_cells.end())
				continue;
			Cell &c = found->second;
			// (a heap in the air takes room; a sand block's own grains, cut to a pit, don't)
			const double over = std::min(c.liquid, c.liquid + (m_kinds[c.kind].air ? c.solid : 0) - c.open);
			if (m_kinds[c.kind].flowing || c.open <= 0 || over <= TRACE || !c.room.layers[15])
				continue;
			Cell *above = into(at(c.pos, 0, 1, 0), true, 1);
			if (above && above != &c && above->open > 0 && above->room.layers[0])
				shift(c, *above, State::Liquid, over);
		}
		// then sideways: each side's pipe speeds up with the difference in
		// height and slows by viscosity; what can't be paid for is scaled down
		std::vector<std::pair<int64_t, std::array<Cell *, 4>>> ways;
		for (size_t i = 0; i < live.size(); i++) {
			auto found = m_cells.find(live[i]);
			if (found == m_cells.end())
				continue;
			Cell &c = found->second;
			std::array<Cell *, 4> to{};
			// only liquid resting on something runs sideways: in the air, it falls
			auto resting = [&] {
				const double floor = surface(at(c.pos, 0, -1, 0));
				return std::isnan(floor) || floor >= c.pos.y - 1e-9 || !c.room.layers[0];
			};
			if (!runs(c) || c.liquid < TRACE || !resting()) {
				std::fill(std::begin(c.flux), std::end(c.flux), 0.0);
			} else {
				const double mine = surface(c.pos);
				double out = 0;
				for (int d = 0; d < 4; d++) {
					const Pos n = at(c.pos, sides()[d].x, 0, sides()[d].z);
					double theirs = surface(n);
					// through the open part of the face they share (sprint 81): from its lowest opening up
					const uint16_t face = m_ground.face(c.pos, d);
					if (std::isnan(theirs) || !face) {
						c.flux[d] = 0;
						continue;
					}
					int lowest = 0;
					while (!(face & (1 << lowest)))
						lowest++;
					const double sill = c.pos.y + lowest / 16.0;
					theirs = std::max(theirs, sill);
					// a puddle spreads no thinner than a sixteenth onto dry ground, but runs
					// freely to where it can fall (over an edge, into a pit beneath)
					// (or on, past it, to somewhere lower than here: water finds the pit)
					bool drains = false;
					if (!wet(n)) {
						const double under = surface(at(n, 0, -1, 0));
						drains = !std::isnan(under) && under < n.y - 1e-9 && m_ground.room(n).layers[0] > 0;
						for (int e = 0; e < 4 && !drains; e++) {
							const Pos past = at(n, sides()[e].x, 0, sides()[e].z);
							if (past.x == c.pos.x && past.z == c.pos.z)
								continue;
							const double there = surface(past);
							drains = !std::isnan(there) && there < mine - THINNEST && m_ground.face(n, e);
						}
					}
					double difference = mine <= sill ? 0 : mine - theirs - (wet(n) || drains ? 0 : THINNEST);
					if (difference <= 0 && c.flux[d] <= 0) {
						c.flux[d] = 0;
						continue;
					}
					to[d] = into(n, false, difference);
					if (!to[d]) {
						c.flux[d] = 0;
						continue;
					}
					c.flux[d] = std::max(0.0, c.flux[d] * std::max(0.0, 1 - DAMPING * h) + h * sky(c).gravity * difference * base().block());
					out += c.flux[d] * h;
				}
				if (out > c.liquid)
					for (double &f : c.flux)
						f *= c.liquid / out;
			}
			ways.push_back({ live[i], to });
		}
		for (auto &[id, to] : ways) {
			Cell &c = m_cells.at(id);
			for (int d = 0; d < 4; d++)
				if (to[d] && c.flux[d] > 0)
					shift(c, *to[d], State::Liquid, c.flux[d] * h);
		}
		// heaps too steep slide down their sides
		for (size_t i = 0; i < live.size(); i++) {
			auto found = m_cells.find(live[i]);
			if (found == m_cells.end())
				continue;
			Cell &c = found->second;
			if (!c.loose || c.solid <= 0)
				continue;
			const double top = c.pos.y + c.solid / v;
			for (int d = 0; d < 4 && c.solid > 0; d++) {
				const Pos n = at(c.pos, sides()[d].x, 0, sides()[d].z);
				auto it = m_cells.find(key(n));
				double theirs = it != m_cells.end() ? n.y + it->second.solid / v : surface(n);
				if (std::isnan(theirs))
					continue;
				const double steep = top - theirs - REPOSE;
				if (steep <= 0)
					continue;
				Cell *side = into(n, false, steep);
				if (side && taken(*side) < side->open)
					shift(c, *side, State::Solid, std::min(steep / 2 * v, side->open - taken(*side)) * std::min(1.0, SLIDING * h));
			}
		}
	}
}

double WorldMatter::density(const Cell &c, State state) const
{
	double kg = 0, m3 = 0;
	for (size_t s = 0; s < c.mix.amounts.size(); s++) {
		const double n = c.mix.amounts[s] > 0 ? c.mix.in(s, state) : 0;
		if (n <= 0)
			continue;
		const matter::Substance &sub = m_matter.substances()[s];
		kg += n * sub.molar_mass / 1000.0;
		m3 += n * sub.molar_mass / 1000.0 / sub.density(state, c.mix.temperature, sky(c).physics.pressure());
	}
	return m3 > 0 ? kg / m3 : 0;
}

// A liquid's open surface: into the block's own air where it isn't full
// (then out with its gas), else into the air over it.
void WorldMatter::evaporate(Cell &c, double dt)
{
	matter::Mixture *air = nullptr;
	const std::vector<double> *room = nullptr;
	const Pos up = at(c.pos, 0, 1, 0);
	// at the weather's temperature, under air that's asleep, it's a pond: it
	// keeps (drying by the day is the Settled tier's, not stepped here)
	if (std::fabs(c.mix.temperature - m_ground.ambient(c.pos)) < 1 && !m_cells.count(key(up)))
		return;
	auto over = m_cells.find(key(up));
	const bool open = over != m_cells.end() && (over->second.liquid + over->second.solid) / sky(c).physics.volume() < 0.5 &&
			sky(c).physics.gasMoles(over->second.mix) > 0;
	if (open) {
		air = &over->second.mix; // awake air over it (a flame, a cloud): the vapour goes straight there
	} else if ((c.liquid + c.solid) / sky(c).physics.volume() < FULL && sky(c).physics.gasMoles(c.mix) > 0) {
		air = &c.mix;
	} else if (over != m_cells.end()) {
		return; // covered
	} else {
		const int32_t kind = m_ground.kind(up);
		if (kind < 0 || !m_kinds[kind].air)
			return; // covered
		room = &sky(c).air;
	}
	double liquid = 0;
	for (size_t s = 0; s < c.mix.amounts.size(); s++)
		if (c.mix.amounts[s] > 0)
			liquid += c.mix.in(s, State::Liquid);
	if (liquid <= 0)
		return;
	const double t = c.mix.temperature, p = sky(c).physics.pressure();
	const double gas = air ? sky(c).physics.gasMoles(*air) : 0;
	for (size_t s = 0; s < c.mix.amounts.size(); s++) {
		const double here = c.mix.amounts[s] > 0 ? c.mix.in(s, State::Liquid) : 0;
		if (here <= 0)
			continue;
		const matter::Substance &sub = m_matter.substances()[s];
		if (t <= sub.triple.t || t >= sub.critical.t)
			continue;
		// Raoult: its share of the liquid's vapour pressure, against what the air holds of it
		const double there = air ? (gas > 0 ? air->in(s, State::Gas) / gas : 0) : (*room)[s];
		const double drive = here / liquid * sub.vapourPressure(t) - there * p;
		if (drive <= 0)
			continue;
		const double n = std::min(here * 0.25, EVAPORATION * sky(c).physics.area() * drive / (matter::GAS_CONSTANT * t) * dt);
		// it takes its heat of vaporisation from the liquid it leaves
		c.mix.temperature = std::max(1.0, t - n * sub.vaporization * 1000 / c.mix.heatCapacity());
		c.mix.take(s, State::Liquid, n);
		if (air == &c.mix) {
			c.mix.add(s, State::Gas, n);
			continue;
		}
		Cell *over = air ? &m_cells.at(key(up)) : nullptr;
		if (!over && n / dt > VAPOUR)
			over = wake(up, m_ground.ambient(up)); // enough to matter: it rises as vapour (and can burn)
		if (!over)
			continue; // a little: the room takes it
		const double per_kelvin = n * sub.molar_mass / 1000.0 * sub.heat_capacity;
		const double capacity = over->mix.heatCapacity();
		over->mix.add(s, State::Gas, n);
		over->mix.temperature = (capacity * over->mix.temperature + per_kelvin * t) / (capacity + per_kelvin);
		over->quiet = 0;
	}
}

} // namespace sandbox
